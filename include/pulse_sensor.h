#ifndef PULSE_SENSOR_H
#define PULSE_SENSOR_H

#include <Arduino.h>

// ========================= PULSE SENSOR CONFIGURATION =========================
const int PULSE_PIN = 35;              // Analog pin for pulse sensor (ESP32 compatible)
const int PULSE_THRESHOLD = 512;       // Base threshold for beat detection
const int PULSE_SAMPLE_INTERVAL = 20;  // Sample every 20ms (50Hz)
const int BEAT_WINDOW = 10;             // Number of beats to average

// ========================= PULSE SENSOR CLASS =========================
class PulseSensor {
private:
    // Heart rate calculation variables
    int heartRate = 0;
    unsigned long lastBeatTime = 0;
    unsigned long beatInterval = 0;
    int beatsPerMinute = 0;
    bool beatDetected = false;
    int signalValue = 0;
    int peakValue = 0;
    int troughValue = 4095;  // ESP32 12-bit ADC max
    bool pulseDetected = false;
    
    // Moving average for smoother readings
    int beatIntervals[BEAT_WINDOW];
    int beatIndex = 0;
    bool beatArrayFilled = false;
    
    // Sample timing
    unsigned long lastSampleTime = 0;
    int lastSignalValue = 0;
    bool rising = false;
    unsigned long lastResetTime = 0;

public:
    /**
     * Initialize the pulse sensor
     */
    void begin() {
        pinMode(PULSE_PIN, INPUT);
        
        // Initialize beat interval array
        for (int i = 0; i < BEAT_WINDOW; i++) {
            beatIntervals[i] = 0;
        }
        
        // Initialize peak/trough values
        peakValue = 2048;    // Mid-range for ESP32 12-bit ADC
        troughValue = 2048;
        lastSampleTime = 0;
        lastBeatTime = 0;
        
        Serial.println("Pulse sensor initialized on pin " + String(PULSE_PIN));
    }
    
    /**
     * Advanced heart rate detection using peak detection algorithm
     * Call this function regularly in your main loop
     * Returns: current BPM (0 if no valid reading)
     */
    int readHeartRate() {
        unsigned long currentTime = millis();
        
        // Sample at regular intervals
        if (currentTime - lastSampleTime >= PULSE_SAMPLE_INTERVAL) {
            lastSampleTime = currentTime;
            
            signalValue = analogRead(PULSE_PIN);
            
            // Adaptive threshold based on signal range
            if (signalValue > peakValue) peakValue = signalValue;
            if (signalValue < troughValue) troughValue = signalValue;
            
            // Calculate dynamic threshold
            int dynamicThreshold = troughValue + ((peakValue - troughValue) * 0.6);
            
            // Detect rising edge (beat detection)
            if (signalValue > dynamicThreshold && lastSignalValue <= dynamicThreshold && !rising) {
                rising = true;
                beatDetected = true;
                
                // Calculate time between beats
                if (lastBeatTime > 0) {
                    beatInterval = currentTime - lastBeatTime;
                    
                    // Valid beat interval (30-200 BPM range)
                    if (beatInterval > 300 && beatInterval < 2000) {
                        // Store in circular buffer for averaging
                        beatIntervals[beatIndex] = beatInterval;
                        beatIndex = (beatIndex + 1) % BEAT_WINDOW;
                        if (beatIndex == 0) beatArrayFilled = true;
                        
                        // Calculate average BPM
                        int sum = 0;
                        int count = beatArrayFilled ? BEAT_WINDOW : beatIndex;
                        for (int i = 0; i < count; i++) {
                            sum += beatIntervals[i];
                        }
                        
                        if (count > 0) {
                            int avgInterval = sum / count;
                            beatsPerMinute = 60000 / avgInterval; // Convert to BPM
                            heartRate = beatsPerMinute;
                            pulseDetected = true;
                        }
                    }
                }
                lastBeatTime = currentTime;
            }
            
            // Reset rising flag when signal falls
            if (signalValue <= dynamicThreshold) {
                rising = false;
            }
            
            lastSignalValue = signalValue;
            
            // Reset peaks periodically to adapt to changes
            if (currentTime - lastResetTime > 5000) { // Reset every 5 seconds
                peakValue = max(signalValue, 2048);
                troughValue = min(signalValue, 2048);
                lastResetTime = currentTime;
            }
        }
        
        // Return current BPM or 0 if no valid reading
        return pulseDetected ? heartRate : 0;
    }
    
    /**
     * Get the raw signal value from the pulse sensor
     */
    int getSignalValue() {
        return signalValue;
    }
    
    /**
     * Check if a beat was just detected
     */
    bool isBeatDetected() {
        if (beatDetected) {
            beatDetected = false; // Reset flag
            return true;
        }
        return false;
    }
    
    /**
     * Check if pulse is currently being detected
     */
    bool isPulseDetected() {
        return pulseDetected;
    }
    
    /**
     * Get current heart rate (same as readHeartRate but without processing)
     */
    int getBPM() {
        return heartRate;
    }
    
    /**
     * Get the dynamic threshold value
     */
    int getThreshold() {
        return troughValue + ((peakValue - troughValue) * 0.6);
    }
    
    /**
     * Reset the pulse detection (useful when sensor is disconnected/reconnected)
     */
    void reset() {
        heartRate = 0;
        lastBeatTime = 0;
        beatInterval = 0;
        beatsPerMinute = 0;
        beatDetected = false;
        pulseDetected = false;
        beatIndex = 0;
        beatArrayFilled = false;
        
        // Reset beat intervals
        for (int i = 0; i < BEAT_WINDOW; i++) {
            beatIntervals[i] = 0;
        }
        
        Serial.println("Pulse sensor reset");
    }
};

#endif // PULSE_SENSOR_H
