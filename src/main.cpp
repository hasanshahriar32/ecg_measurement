#include <Arduino.h>
#include <WiFi.h>
#include "telegram_notify.h"
#include "mqtt_publish.h"

// ====== Pin configuration (adjust to your wiring) ======
// ECG analog output from AD8232 -> an ADC-capable pin on ESP32 (GPIO34..39 are input-only ADC1 pins)
const int PIN_ECG = 34;        // GPIO34 (ADC1_6)
// Lead-off indicator pins from AD8232 (LO+ and LO-)
const int PIN_LO_PLUS = 32;    // GPIO32
const int PIN_LO_MINUS = 33;   // GPIO33

// ====== Sampling and detection parameters ======
const int SAMPLE_HZ = 250;                     // ECG sample rate (Hz)
const uint32_t SAMPLE_US = 1000000UL / SAMPLE_HZ;
const uint32_t CONSOLE_INTERVAL_MS = 100;      // Console output every 100ms (10Hz) - adjust this to make faster/slower
const int BASELINE_WINDOW = 64;                // Moving-average window for baseline removal
const float ENVELOPE_ALPHA = 0.05f;            // Smoothing for rectified envelope (0..1)
const float THRESH_SCALE = 0.6f;               // Threshold as a fraction of envelope
const float MIN_THRESHOLD = 8.0f;              // Minimum threshold in ADC counts after HP filter
const uint32_t REFRACTORY_MS = 250;            // Min time between beats (ms) ~240 BPM max

// ====== Networking (fill these) ======
const char* WIFI_SSID = "realme 9i";
const char* WIFI_PASS = "gragra12345";

// ====== State ======
volatile uint32_t lastSampleMicros = 0;
volatile uint32_t lastConsoleMs = 0;  // For timing console output

int rawBuf[BASELINE_WINDOW];
long rawSum = 0;
int rawIndex = 0;
int baseline = 0;

float envelope = 0.0f;
bool prevAbove = false;
uint16_t bpm = 0;                              // Smoothed BPM

// HRV and panic detection variables
uint32_t rrIntervals[20];                      // Store last 20 R-R intervals
int rrIndex = 0;
float rmssd = 0.0f;                           // HRV metric (panic shows <20ms)
uint16_t baselineHR = 70;                     // Baseline heart rate
int hrTrend = 0;                              // HR change rate (BPM/min)
uint32_t lastHrUpdate = 0;
bool panicAlertSent = false;

// Expose values for MQTT module
int heartRate = 0;
int signalValue = 0;
int thresholdValue = 0; // dynamic threshold for publishing

static void initBaseline(int seed) {
  rawSum = 0;
  for (int i = 0; i < BASELINE_WINDOW; i++) {
    rawBuf[i] = seed;
    rawSum += seed;
  }
  rawIndex = 0;
  baseline = seed;
}

static void resetDetection() {
  envelope = 0.0f;
  prevAbove = false;
  bpm = 0;
  heartRate = 0;
  rmssd = 0.0f;
  rrIndex = 0;
  panicAlertSent = false;
  int r = analogRead(PIN_ECG);
  initBaseline(r);
}

static bool leadsOff() {
  // AD8232 LO+ / LO- go HIGH when leads are off
  return digitalRead(PIN_LO_PLUS) == HIGH || digitalRead(PIN_LO_MINUS) == HIGH;
}

static void detectPanicSignatures(uint32_t nowMs) {
  static uint32_t lastPanicCheck = 0;
  
  if (nowMs - lastPanicCheck < 10000) return; // Check every 10 seconds
  lastPanicCheck = nowMs;
  
  bool panicDetected = false;
  String alertMsg = "";
  
  // Signature 1: Sudden HR increase (>20 BPM above baseline)
  if (bpm > baselineHR + 20 && bpm > 90) {
    panicDetected = true;
    alertMsg += "Sudden HR spike: " + String(bpm) + " BPM. ";
  }
  
  // Signature 2: Sustained tachycardia (>100 BPM)
  static uint32_t tachyStartTime = 0;
  if (bpm > 100) {
    if (tachyStartTime == 0) tachyStartTime = nowMs;
    else if (nowMs - tachyStartTime > 300000) { // 5 minutes
      panicDetected = true;
      alertMsg += "Sustained tachycardia: " + String(bpm) + " BPM. ";
    }
  } else {
    tachyStartTime = 0;
  }
  
  // Signature 3: Low HRV (stress indicator)
  if (rmssd > 0 && rmssd < 20 && bpm > 80) {
    panicDetected = true;
    alertMsg += "Low HRV: " + String(rmssd, 1) + "ms. ";
  }
  
  // Signature 4: Rapid HR acceleration
  if (hrTrend > 30) { // >30 BPM increase in last minute
    panicDetected = true;
    alertMsg += "Rapid HR acceleration: +" + String(hrTrend) + " BPM/min. ";
  }
  
  // Send alert (once per episode)
  if (panicDetected && !panicAlertSent) {
    Serial.println("PANIC_ALERT: " + alertMsg);
    if (WiFi.status() == WL_CONNECTED) {
      String telegramMsg = "🚨 PANIC ALERT: " + alertMsg + "Time: " + String(millis()/1000) + "s\n\n";
      telegramMsg += "📊 View live ECG data: https://ecg-measurement.onrender.com/";
      sendTelegramNotification(telegramMsg);
    }
    panicAlertSent = true;
  }
  
  // Reset alert flag when HR normalizes
  if (bpm < baselineHR + 10 && rmssd > 25) {
    panicAlertSent = false;
  }
}

static void sampleAndProcess() {
  if (leadsOff()) {
    static uint32_t lastMsg = 0;
    if (millis() - lastMsg > 1000) {
      Serial.println("LEADS_OFF");
      lastMsg = millis();
    }
    resetDetection();
    return;
  }

  int raw = analogRead(PIN_ECG);               // 12-bit on ESP32 (0..4095)

  // Update baseline (moving average)
  rawSum -= rawBuf[rawIndex];
  rawBuf[rawIndex] = raw;
  rawSum += raw;
  rawIndex = (rawIndex + 1) % BASELINE_WINDOW;
  baseline = (int)(rawSum / BASELINE_WINDOW);

  // High-pass via baseline removal
  int hp = raw - baseline;

  // Rectified envelope (EMA)
  float absHp = fabsf((float)hp);
  envelope += ENVELOPE_ALPHA * (absHp - envelope);

  float thr = fmaxf(MIN_THRESHOLD, envelope * THRESH_SCALE);
  thresholdValue = (int)thr; // update threshold for MQTT

  // Beat detection with refractory period and rising edge
  static uint32_t lastBeatMs = 0;
  uint32_t nowMs = millis();
  bool above = (hp > (int)thr);

  if (above && !prevAbove) {
    if (lastBeatMs == 0 || (nowMs - lastBeatMs) > REFRACTORY_MS) {
      if (lastBeatMs != 0) {
        uint32_t ibi = nowMs - lastBeatMs;     // R-R interval (ms)
        
        // Store R-R interval for HRV analysis
        rrIntervals[rrIndex % 20] = ibi;
        rrIndex++;
        
        uint16_t instBpm = (uint16_t)(60000UL / ibi);
        uint16_t prevBpm = bpm;
        bpm = (bpm == 0) ? instBpm : (uint16_t)(0.8f * bpm + 0.2f * instBpm);
        
        // Calculate HR trend (change per minute)
        if (nowMs - lastHrUpdate > 60000) { // Every minute
          hrTrend = (int)bpm - (int)baselineHR;
          
          // Update baseline during calm periods
          if (bpm < 90 && rmssd > 25) {
            baselineHR = (baselineHR * 3 + bpm) / 4; // Slow adaptation
          }
          
          lastHrUpdate = nowMs;
        }
        
        // Calculate HRV (RMSSD)
        if (rrIndex >= 5) {
          float sumSquaredDiffs = 0;
          int validCount = 0;
          int maxIdx = min(rrIndex, 20);
          
          for (int i = 1; i < maxIdx; i++) {
            uint32_t curr = rrIntervals[i % 20];
            uint32_t prev = rrIntervals[(i-1) % 20];
            if (curr > 300 && curr < 2000 && prev > 300 && prev < 2000) {
              float diff = (float)curr - (float)prev;
              sumSquaredDiffs += diff * diff;
              validCount++;
            }
          }
          
          if (validCount > 2) {
            rmssd = sqrt(sumSquaredDiffs / validCount);
          }
        }
        
        // Panic attack detection
        detectPanicSignatures(nowMs);
        
        heartRate = bpm;
      }
      lastBeatMs = nowMs;
    }
  }
  prevAbove = above;

  // Update values for MQTT/serial
  signalValue = hp;

  // Print hp and threshold for plotting (slowed down)
  static int printCounter = 0;
  printCounter++;
  if (printCounter >= 5) { // Print every 5th sample (50 Hz)
    Serial.print(hp);
    Serial.print(",");
    Serial.println((int)thr);
    printCounter = 0;
  }
}

static void connectWiFi() {
  if (WIFI_SSID == nullptr || strlen(WIFI_SSID) == 0) return;
  
  Serial.print("Connecting to WiFi network: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  uint32_t t0 = millis();
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    Serial.print('.');
    delay(500);
    attempts++;
    
    if (attempts % 10 == 0) {
      Serial.print(" [");
      Serial.print(attempts);
      Serial.print(" attempts] ");
    }
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected successfully! IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    String connectionMsg = String("✅ ESP32 ECG Monitor connected successfully!\n");
    connectionMsg += "📍 Device IP: " + WiFi.localIP().toString() + "\n";
    connectionMsg += "📶 Signal: " + String(WiFi.RSSI()) + " dBm\n\n";
    connectionMsg += "📊 View live ECG dashboard: https://ecg-measurement.onrender.com/";
    sendTelegramNotification(connectionMsg);
  } else {
    Serial.println("WiFi connection FAILED!");
    Serial.print("WiFi status: ");
    Serial.println(WiFi.status());
    Serial.println("Please check:");
    Serial.println("- WiFi name is correct (case-sensitive)");
    Serial.println("- Password is correct");
    Serial.println("- WiFi is 2.4GHz (not 5GHz)");
    Serial.println("- Device is in range");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

#ifdef ARDUINO_ARCH_ESP32
  analogReadResolution(12);                    // 12-bit ADC
  // Expand input range to ~3.6V (AD8232 centered near mid-supply)
  analogSetPinAttenuation(PIN_ECG, ADC_11db);
#endif

  pinMode(PIN_ECG, INPUT);
  pinMode(PIN_LO_PLUS, INPUT);
  pinMode(PIN_LO_MINUS, INPUT);

  int r = analogRead(PIN_ECG);
  initBaseline(r);

  lastSampleMicros = micros();
  Serial.println("AD8232 ECG ready");
  Serial.println("hp,threshold,bpm");

  // Network services
  connectWiFi();
  mqttSetup();
}

void loop() {
  uint32_t now = micros();
  if ((now - lastSampleMicros) >= SAMPLE_US) {
    lastSampleMicros += SAMPLE_US;
    sampleAndProcess();
  }
  // Publish over MQTT once a second in mqttLoopAndPublish
  mqttLoopAndPublish();
}