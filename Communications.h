#ifndef COMMUNICATIONS_H
#define COMMUNICATIONS_H

#include <Arduino.h>

// ============================================================================================
// THINGSPEAK FUNCTIONS
// ============================================================================================

// ThingSpeak - WiFi (με RSSI)
bool sendToThingSpeakWiFi(float weight, float temp, float hum, float voltage, float battPercent, float dsTemp, int rssi);

// ThingSpeak - 4G (με RSSI) - PRIMARY
bool sendToThingSpeak4G(float weight, float temp, float hum, float voltage, float battPercent, float dsTemp, int modemRssi);

// ThingSpeak - 4G with retry (χωρίς RSSI - auto detect)
bool sendToThingSpeak4GWithRetry(float weight, float temp, float hum, float voltage, float battPercent, float dsTemp, int maxRetries = 2);

// ============================================================================================
// WHATSAPP FUNCTIONS
// ============================================================================================

// WhatsApp - WiFi (με RSSI)
bool sendToWhatsAppWiFi(float weight, float temp, float hiveTemp, float humidity, float batteryPercent, float batteryVoltage, int rssi);

// WhatsApp - 4G (με RSSI) - PRIMARY
bool sendToWhatsApp4G(float weight, float temp, float hiveTemp, float humidity, float batteryPercent, float batteryVoltage, int modemRssi);

// Overload για WhatsApp 4G (χωρίς τάση)
bool sendToWhatsApp4G(float weight, float temp, float hiveTemp, float humidity, float batteryPercent, int modemRssi);

// ============================================================================================
// ALERT FUNCTIONS
// ============================================================================================

// Alert - WiFi
bool sendAlertWhatsAppWiFi(String message);

// Alert - 4G
bool sendAlertWhatsApp4G(String message);

// ============================================================================================
// MAIN SEND FUNCTION
// ============================================================================================

// Ενιαία συνάρτηση με auto-fallback (ΠΡΩΤΑ 4G, μετά WiFi)
bool sendDataWithFallback(float weight, float temp, float hum, float voltage, float battPercent, float dsTemp, bool alarm);

// ============================================================================================
// HELPER FUNCTIONS
// ============================================================================================

// URL Encode για WhatsApp messages
String urlEncode(String str);

// ============================================================================================
// EXTERNAL VARIABLES
// ============================================================================================

extern float lastStoredWeight;

String getCurrentNetworkOperator();
int getCurrentRSSI();


#endif
