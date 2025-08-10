#ifndef MQTT_PUBLISH_H
#define MQTT_PUBLISH_H

#include <PubSubClient.h>

#ifdef ARDUINO_ARCH_ESP32
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
#else
  #include <ESP8266WiFi.h>
  #include <WiFiClientSecure.h>
#endif

// HiveMQ Cloud broker details (update username/password below)
const char* mqtt_server = "d5e9ca698a2a4640b81af8b8e3e6e1e4.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_topic = "mrhasan/heart"; // Unique topic for your project
const char *mqtt_username = "Paradox";    // <-- Set your HiveMQ Cloud username
const char *mqtt_password = "Paradox1";    // <-- Set your HiveMQ Cloud password

extern int heartRate;
extern int signalValue;     // hp (high-pass filtered ECG)
extern int thresholdValue;  // dynamic threshold
extern float rmssd;
extern int hrTrend;
extern uint16_t baselineHR;

WiFiClientSecure espMqttClient;
PubSubClient mqttClient(espMqttClient);

void mqttReconnect() {
  // Only try to connect if WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  
  int attempts = 0;
  while (!mqttClient.connected() && attempts < 3) {
    attempts++;
    String clientId = String("ESP32Client-") + String((uint32_t)ESP.getEfuseMac(), HEX);
    Serial.print("[MQTT] Attempting connection... ");
    if (mqttClient.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Connected to HiveMQ Cloud!");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.print(" (attempt ");
      Serial.print(attempts);
      Serial.println("/3)");
      if (attempts < 3) delay(2000);
    }
  }
}

void mqttSetup() {
  espMqttClient.setInsecure(); // For testing only. For production, use a proper root CA cert.
  mqttClient.setServer(mqtt_server, mqtt_port);
}

void mqttLoopAndPublish() {
  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();
  static unsigned long lastMqtt = 0;
  if (millis() - lastMqtt > 1000) {
    lastMqtt = millis();

    // Prepare JSON payload containing hp and threshold (no BPM)
    char payload[256];
    char deviceId[20];
    snprintf(deviceId, sizeof(deviceId), "ESP32_%04X", (uint16_t)(ESP.getEfuseMac() & 0xFFFF));

    // Simple timestamp placeholder; replace with RTC/NTP if available
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%lu", (unsigned long)millis());

    snprintf(payload, sizeof(payload),
             "{\"userId\":\"BW8NUP21AWMkI0xrrI2nxBP6Xd92\",\"dataType\":\"ecg_analysis\",\"hp\":%d,\"threshold\":%d,\"bpm\":%d,\"baselineHR\":%d,\"rmssd\":%.1f,\"hrTrend\":%d,\"timestamp\":\"%s\",\"deviceId\":\"%s\"}",
             signalValue, thresholdValue, heartRate, baselineHR, rmssd, hrTrend, timestamp, deviceId);

    mqttClient.publish(mqtt_topic, payload);
    Serial.print("[MQTT] Published: ");
    Serial.println(payload);
  }
}

#endif // MQTT_PUBLISH_H
