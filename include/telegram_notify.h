#ifndef TELEGRAM_NOTIFY_H
#define TELEGRAM_NOTIFY_H

// Make this header work on both ESP8266 and ESP32
#ifdef ARDUINO_ARCH_ESP32
  #include <WiFi.h>
  #include <HTTPClient.h>
  #include <WiFiClientSecure.h>
#else
  #include <ESP8266WiFi.h>
  #include <ESP8266HTTPClient.h>
  #include <WiFiClientSecure.h>
#endif
#include <ctype.h>

// === Fill in your Telegram Bot Token and User ID ===
#define TELEGRAM_BOT_TOKEN "5623049233:AAFX7zAZjHrsRYhAzcLiKLZ3dVWQiJHdnC8"
#define TELEGRAM_USER_ID "-1002769415296"

/**
 * URL-encode a string for safe HTTP transmission
 */
String urlencode(const String& str);

/**
 * Send a notification message to a Telegram user via bot
 * @param message The message to send
 * @return true if sent successfully, false otherwise
 */
bool sendTelegramNotification(const String& message) {
    if (WiFi.status() != WL_CONNECTED) return false;
    HTTPClient http;
    WiFiClientSecure wifiClient;
    wifiClient.setInsecure(); // Accept all certificates (testing only)
    String url = "https://api.telegram.org/bot";
    url += TELEGRAM_BOT_TOKEN;
    url += "/sendMessage?chat_id=";
    url += TELEGRAM_USER_ID;
    url += "&text=";
    url += urlencode(message);

    http.begin(wifiClient, url);
    int httpCode = http.GET();
    String payload = http.getString();
    Serial.print("[Telegram] HTTP code: ");
    Serial.println(httpCode);
    Serial.print("[Telegram] Response: ");
    Serial.println(payload);
    http.end();
    return (httpCode == 200);
}

// Simple URL encoder for message text
String urlencode(const String& str) {
    String encoded = "";
    char c;
    char code0, code1;
    for (size_t i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (isalnum((unsigned char)c)) {
            encoded += c;
        } else {
            code1 = (c & 0xF) + '0';
            if ((c & 0xF) > 9) code1 = (c & 0xF) - 10 + 'A';
            code0 = ((c >> 4) & 0xF) + '0';
            if (((c >> 4) & 0xF) > 9) code0 = ((c >> 4) & 0xF) - 10 + 'A';
            encoded += '%';
            encoded += code0;
            encoded += code1;
        }
    }
    return encoded;
}

#endif // TELEGRAM_NOTIFY_H
