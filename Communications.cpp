/*
 * ============================================================================================
 * Communications.cpp - HoneyEsp Data Sending
 * Version: 1.0.2 - CLEANED: PMU reset only in ModemManager
 * ============================================================================================
 */

#include "Communications.h"
#include "Config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <Wire.h>           
#include "XPowersLib.h"     

// Για 4G functions
#include "ModemManager.h"

// ========================================================================
// EXTERN VARIABLES
// ========================================================================
extern String tsApiKey;
extern String waPhone;
extern String waApiKey;
extern float lastStoredWeight;
extern String wifiSSID;
extern String wifiPass;
extern String simApn;

// ========== GLOBAL VARIABLES FOR DISPLAY (από main) ==========
extern int lastKnownRSSI;
extern String lastKnownNetwork;
// ==============================================================

// ============================================================================================
// URL ENCODE
// ============================================================================================
String urlEncode(String str) {
    String encoded = "";
    char c;
    char buf[4];
    for (int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else if (c == ' ') {
            encoded += '+';
        } else if (c == '\n') {
            encoded += "%0A";
        } else {
            sprintf(buf, "%%%02X", (unsigned char)c);
            encoded += buf;
        }
    }
    return encoded;
}

// ============================================================================================
// HELPER: GET TIMESTAMP
// ============================================================================================
String getTimestamp() {
    unsigned long seconds = millis() / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    
    hours = hours % 24;
    minutes = minutes % 60;
    seconds = seconds % 60;
    
    char buffer[30];
    sprintf(buffer, "Up: %lud %02lu:%02lu:%02lu", days, hours, minutes, seconds);
    return String(buffer);
}

// ============================================================================================
// SEND TO THINGSPEAK (4G - PRIMARY)
// ============================================================================================
bool sendToThingSpeak4G(float w, float t, float h, float v, float batt, float ds, int modemRssi) {
    Serial.println("\n[4G] Sending to ThingSpeak via 4G...");
    
    if (sendData4G(w, t, h, ds, v, batt, modemRssi)) {
        float weight_kg = w / 1000.0;
        Serial.printf("✅ ThingSpeak via 4G: %.2f kg sent (RSSI: %d dBm)\n", weight_kg, modemRssi);
        return true;
    } else {
        Serial.println("❌ ThingSpeak via 4G failed");
        return false;
    }
}

// ============================================================================================
// SEND TO THINGSPEAK (WiFi - FALLBACK)
// ============================================================================================
bool sendToThingSpeakWiFi(float w, float t, float h, float v, float batt, float ds, int rssi) {
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ WiFi not connected!");
        return false;
    }
    
    String cleanApiKey = tsApiKey;
    cleanApiKey.trim();
    
    if (cleanApiKey.length() != 16) {
        Serial.printf("⚠️ WARNING: API Key should be 16 chars, but is %d!\n", cleanApiKey.length());
    }
    
    if (isnan(t)) t = 0;
    if (isnan(h)) h = 0;
    if (isnan(batt)) batt = 0;
    if (isnan(v)) v = 0;
    if (isnan(ds)) ds = 0;
    if (isnan(w)) w = 0;
    
    WiFiClient client;
    
    if (!client.connect("api.thingspeak.com", 80)) {
        Serial.println("❌ ThingSpeak connection failed");
        return false;
    }
    
    float weight_kg = w / 1000.0;

    String url = "/update?api_key=" + cleanApiKey;
    url += "&field1=" + String(weight_kg, 2);
    url += "&field2=" + String(t, 1);
    url += "&field3=" + String(h, 1);
    url += "&field4=" + String(ds, 1);
    url += "&field5=" + String(v, 2);
    url += "&field6=" + String((int)batt);
    url += "&field7=" + String(rssi);

    Serial.println("\n📊 THINGSPEAK VIA WiFi:");
    Serial.printf("   Weight: %.2f kg | Air: %.1f°C | Humidity: %.1f%% | Hive: %.1f°C | Batt: %.2fV (%.0f%%) | RSSI: %d dBm\n", 
                  weight_kg, t, h, ds, v, batt, rssi);
    
    client.print("GET " + url + " HTTP/1.1\r\n");
    client.print("Host: api.thingspeak.com\r\n");
    client.print("Connection: close\r\n");
    client.print("\r\n");
    
    unsigned long timeout = millis();
    bool success = false;
    
    while (client.available() == 0) {
        if (millis() - timeout > 8000) {
            client.stop();
            Serial.println("❌ ThingSpeak timeout");
            return false;
        }
        delay(50);
    }
    
    while (client.available()) {
        String response = client.readStringUntil('\n');
        if (response.indexOf("200") >= 0 || (response.length() > 0 && response.length() < 10 && isDigit(response.charAt(0)))) {
            success = true;
        }
    }
    
    client.stop();
    
    if (success) {
        Serial.printf("✅ ThingSpeak via WiFi: %.2f kg (RSSI: %d dBm)\n", weight_kg, rssi);
        return true;
    } else {
        Serial.println("❌ ThingSpeak via WiFi failed");
        return false;
    }
}

// ============================================================================================
// SEND TO WHATSAPP (4G - PRIMARY)
// ============================================================================================
bool sendToWhatsApp4G(float weight, float temp, float hiveTemp, float humidity, float batteryPercent, float batteryVoltage, int modemRssi) {
    Serial.println("\n[4G] Sending WhatsApp via 4G...");
    
    if (waPhone == "" || waApiKey == "") {
        Serial.println("⚠️ WhatsApp not configured");
        return false;
    }
    
    String phoneNumber = waPhone;
    if (!phoneNumber.startsWith("+")) {
        phoneNumber = "+" + phoneNumber;
    }
    
    float weight_kg = weight / 1000.0;
    
    String batteryEmoji = "🔋";
    if (batteryPercent < 15) {
        batteryEmoji = "🪫⚠️";
    } else if (batteryPercent < 35) {
        batteryEmoji = "🪫";
    } else if (batteryPercent > 80) {
        batteryEmoji = "🔋✅";
    }
    
    String message = "🐝 *HoneyEsp Hive Report (4G)* 🐝\n";
    message += "━━━━━━━━━━━━━━━━━━━━\n";
    message += "⚖️ *Weight:* " + String(weight_kg, 2) + " kg\n";
    message += "🌡️ *Air Temp:* " + String(temp, 1) + "°C\n";
    message += "💧 *Humidity:* " + String(humidity, 1) + "%\n";
    message += "🏠 *Hive Temp:* " + String(hiveTemp, 1) + "°C\n";
    message += batteryEmoji + " *Battery:* " + String(batteryPercent, 0) + "%";
    
    if (batteryVoltage > 0) {
        message += " (" + String(batteryVoltage, 2) + "V)";
    }
    
    message += "\n📶 *4G Signal:* " + String(modemRssi) + " dBm\n";
    message += "━━━━━━━━━━━━━━━━━━━━";
    
    String url = "/whatsapp.php?phone=" + phoneNumber;
    url += "&text=" + urlEncode(message);
    url += "&apikey=" + waApiKey;
    
    Serial.printf("[4G] Sending WhatsApp to: %s (Modem RSSI: %d dBm)\n", phoneNumber.c_str(), modemRssi);
    Serial.printf("[4G] Using APN: %s\n", simApn.c_str());
    
    TinyGsmClient client4G(modem);
    
    if (client4G.connect("api.callmebot.com", 80)) {
        client4G.print(String("GET ") + url + " HTTP/1.1\r\n");
        client4G.print("Host: api.callmebot.com\r\n");
        client4G.print("Connection: close\r\n\r\n");
        
        unsigned long timeout = millis();
        String fullResponse = "";
        bool success = false;
        
        while (client4G.connected() && millis() - timeout < 10000) {
            if (client4G.available()) {
                String line = client4G.readStringUntil('\n');
                fullResponse += line + "\n";
                
                if (line.indexOf("200") >= 0 || line.indexOf("210") >= 0) {
                    success = true;
                }
                if (line.indexOf("Message sent") >= 0 || line.indexOf("success") >= 0) {
                    success = true;
                }
                timeout = millis();
            }
            delay(10);
        }
        
        client4G.stop();
        
        if (success) {
            Serial.println("[4G] WhatsApp Response:");
            Serial.println(fullResponse.substring(0, 300));
            Serial.println("✅ WhatsApp message sent via 4G!");
            return true;
        } else {
            Serial.println("[4G] WhatsApp failed - no success response");
            if (fullResponse.length() > 0) {
                Serial.println("[4G] Last response: " + fullResponse.substring(0, 200));
            }
        }
    }
    
    Serial.println("❌ WhatsApp via 4G failed");
    return false;
}

// ============================================================================================
// SEND TO WHATSAPP (WiFi - FALLBACK)
// ============================================================================================
bool sendToWhatsAppWiFi(float weight, float temp, float hiveTemp, float humidity, float batteryPercent, float batteryVoltage, int rssi) {
    
    if (waPhone == "" || waApiKey == "") {
        Serial.println("⚠️ WhatsApp not configured - skipping");
        return false;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ WiFi not connected for WhatsApp");
        return false;
    }
    
    HTTPClient http;
    
    String phoneNumber = waPhone;
    if (!phoneNumber.startsWith("+")) {
        phoneNumber = "+" + phoneNumber;
    }
    
    float weight_kg = weight / 1000.0;
    
    String batteryEmoji = "🔋";
    if (batteryPercent < 15) {
        batteryEmoji = "🪫⚠️";
    } else if (batteryPercent < 35) {
        batteryEmoji = "🪫";
    } else if (batteryPercent > 80) {
        batteryEmoji = "🔋✅";
    }
    
    String message = "🐝 *HoneyEsp Hive Report (WiFi)* 🐝\n";
    message += "━━━━━━━━━━━━━━━━━━━━\n";
    message += "⚖️ *Weight:* " + String(weight_kg, 2) + " kg\n";
    message += "🌡️ *Air Temp:* " + String(temp, 1) + "°C\n";
    message += "💧 *Humidity:* " + String(humidity, 1) + "%\n";
    message += "🏠 *Hive Temp:* " + String(hiveTemp, 1) + "°C\n";
    message += batteryEmoji + " *Battery:* " + String(batteryPercent, 0) + "%";
    
    if (batteryVoltage > 0) {
        message += " (" + String(batteryVoltage, 2) + "V)";
    }
    
    message += "\n📶 *WiFi RSSI:* " + String(rssi) + " dBm\n";
    message += "━━━━━━━━━━━━━━━━━━━━";
    
    String url = "http://api.callmebot.com/whatsapp.php?phone=" + phoneNumber;
    url += "&text=" + urlEncode(message);
    url += "&apikey=" + waApiKey;
    
    Serial.printf("📱 Sending WhatsApp via WiFi to: %s (RSSI: %d dBm)\n", phoneNumber.c_str(), rssi);
    
    http.begin(url);
    http.setTimeout(8000);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
        String response = http.getString();
        Serial.printf("[WiFi] WhatsApp Response HTTP %d: %s\n", httpCode, response.c_str());
        
        if (httpCode == 200 || httpCode == 210 || httpCode == 201) {
            if (response.indexOf("Message sent") >= 0 || response.indexOf("success") >= 0) {
                Serial.println("✅ WhatsApp message confirmed!");
                http.end();
                return true;
            } else {
                Serial.println("⚠️ WhatsApp returned success but unexpected response");
                http.end();
                return true;
            }
        } else {
            Serial.printf("❌ WhatsApp failed with HTTP %d\n", httpCode);
        }
    } else {
        Serial.printf("❌ WhatsApp request failed: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
    return false;
}

// ============================================================================================
// OVERLOAD FUNCTIONS FOR COMPATIBILITY
// ============================================================================================

bool sendToWhatsAppWiFi(float weight, float temp, float hiveTemp, float humidity, 
                        float batteryPercent, int rssi) {
    return sendToWhatsAppWiFi(weight, temp, hiveTemp, humidity, batteryPercent, -1, rssi);
}

bool sendToThingSpeakWiFi(float weight, float temp, float humidity, float voltage, 
                          float battery, float hiveTemp) {
    if (WiFi.status() != WL_CONNECTED) return false;
    int rssi = WiFi.RSSI();
    return sendToThingSpeakWiFi(weight, temp, humidity, voltage, battery, hiveTemp, rssi);
}

bool sendToWhatsApp4G(float weight, float temp, float hiveTemp, float humidity, 
                      float batteryPercent, int rssi) {
    return sendToWhatsApp4G(weight, temp, hiveTemp, humidity, batteryPercent, -1, rssi);
}

bool sendToThingSpeak4G(float weight, float temp, float humidity, float voltage, 
                        float battery, float hiveTemp) {
    int rssi = getModemRSSI();
    return sendToThingSpeak4G(weight, temp, humidity, voltage, battery, hiveTemp, rssi);
}

// ============================================================================================
// MAIN SEND FUNCTION - PRIMARY 4G, FALLBACK WiFi
// ============================================================================================
bool sendDataWithFallback(float w, float t, float h, float v, float batt, float ds, bool alarm) {
    
    if (isnan(t)) t = 20.0;
    if (isnan(h)) h = 50.0;
    if (isnan(batt)) batt = 0;
    if (isnan(v)) v = 0;
    if (isnan(ds)) ds = 20.0;
    if (isnan(w)) w = 0;
    
    Serial.println("\n╔═══════════════════════════════════════════════════════════════════════╗");
    Serial.println("║                    STARTING DATA TRANSMISSION                        ║");
    Serial.println("╚═══════════════════════════════════════════════════════════════════════╝");
    
    Serial.printf("[DEBUG] Final values - T:%.1f, H:%.1f, Batt:%.1f%%, Weight:%.2fkg, Hive:%.1f°C\n", 
                  t, h, batt, w/1000.0, ds);
    
    bool success = false;
    
    // ============================================================
    // 1. ΠΡΩΤΑ 4G (SIM) - PRIMARY CONNECTION
    // ============================================================
    Serial.println("\n[1/2] 📱 Attempting 4G connection (PRIMARY)...");
    
    // ============================================================
    // ΣΗΜΑΝΤΙΚΟ: ΔΕΝ κάνουμε PMU reset ή power key εδώ!
    // Το initModemWithAPN() τα κάνει ήδη μέσα του
    // ============================================================
    
    if (initModemWithAPN()) {
        int modemRssi = getModemRSSI();
        Serial.printf("      📶 Modem Signal Strength: %d dBm\n", modemRssi);
        
        lastKnownRSSI = modemRssi;
        lastKnownNetwork = getNetworkOperator();
        if (lastKnownNetwork == "") lastKnownNetwork = "4G";
        
        bool tsSuccess = sendToThingSpeak4G(w, t, h, v, batt, ds, modemRssi);
        bool waSuccess = sendToWhatsApp4G(w, t, ds, h, batt, v, modemRssi);
        
        if (alarm) {
            float diff_kg = abs(w - lastStoredWeight) / 1000.0;
            if (diff_kg > 0.5) {
                String alertMsg = "⚠️ Απότομη αλλαγή βάρους: " + String(diff_kg, 1) + " kg!\n";
                alertMsg += "Τρέχον βάρος: " + String(w/1000.0, 2) + " kg";
                sendAlertWhatsApp4G(alertMsg);
            }
        }
        
        success = (tsSuccess || waSuccess);
        
        powerOffModem();
        Serial.println("      📴 Modem powered off");
        
        if (success) {
            Serial.println("\n✅ DATA SENT SUCCESSFULLY VIA 4G!");
        } else {
            Serial.println("\n⚠️ 4G transmission had issues, checking WiFi fallback...");
        }
        
    } else {
        Serial.println("❌ Modem initialization FAILED!");
    }
    
    // ============================================================
    // 2. FALLBACK ΣΕ WiFi (ΑΝ ΑΠΟΤΥΧΕ ΤΟ 4G)
    // ============================================================
    if (!success) {
        Serial.println("\n[2/2] 🌐 Attempting WiFi connection (FALLBACK)...");
        
        if (wifiSSID.length() > 0 && wifiSSID != "") {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            delay(100);
            
            WiFi.mode(WIFI_STA);
            delay(100);
            
            WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
            
            unsigned long startWiFi = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - startWiFi < 30000) {
                delay(500);
                Serial.print(".");
            }
            Serial.println();
            
            if (WiFi.status() == WL_CONNECTED) {
                int wifiRssi = WiFi.RSSI();
                Serial.printf("✅ WiFi connected! RSSI: %d dBm\n", wifiRssi);
                
                lastKnownRSSI = wifiRssi;
                lastKnownNetwork = "WiFi";
                
                bool tsSuccess = sendToThingSpeakWiFi(w, t, h, v, batt, ds, wifiRssi);
                bool waSuccess = sendToWhatsAppWiFi(w, t, ds, h, batt, v, wifiRssi);
                
                if (alarm) {
                    float diff_kg = abs(w - lastStoredWeight) / 1000.0;
                    if (diff_kg > 0.5) {
                        String alertMsg = "Weight changed by " + String(diff_kg, 1) + " kg!\n";
                        alertMsg += "Current: " + String(w/1000.0, 2) + " kg";
                        sendAlertWhatsAppWiFi(alertMsg);
                    }
                }
                
                success = (tsSuccess || waSuccess);
                
                WiFi.disconnect(true);
                WiFi.mode(WIFI_OFF);
                Serial.println("      📡 WiFi disconnected");
                
                if (success) {
                    Serial.println("\n✅ DATA SENT SUCCESSFULLY VIA WiFi (FALLBACK)!");
                }
                
            } else {
                Serial.println("❌ WiFi connection FAILED!");
            }
        } else {
            Serial.println("⚠️ No WiFi credentials configured!");
        }
    }
    
    Serial.println("\n╔═══════════════════════════════════════════════════════════════════════╗");
    if (success) {
        Serial.println("║                    ✅ TRANSMISSION SUCCESSFUL ✅                    ║");
    } else {
        Serial.println("║                    ❌ TRANSMISSION FAILED ❌                       ║");
    }
    Serial.println("╚═══════════════════════════════════════════════════════════════════════╝\n");
    
    return success;
}

// ============================================================================================
// SEND ALERT (4G)
// ============================================================================================
bool sendAlertWhatsApp4G(String message) {
    if (waPhone == "" || waApiKey == "") return false;
    
    String phoneNumber = waPhone;
    if (!phoneNumber.startsWith("+")) {
        phoneNumber = "+" + phoneNumber;
    }
    
    String url = "/whatsapp.php?phone=" + phoneNumber;
    url += "&text=" + urlEncode("🚨 *ALERT!* 🚨\n" + message);
    url += "&apikey=" + waApiKey;
    
    TinyGsmClient client4G(modem);
    
    if (client4G.connect("api.callmebot.com", 80)) {
        client4G.print(String("GET ") + url + " HTTP/1.1\r\n");
        client4G.print("Host: api.callmebot.com\r\n");
        client4G.print("Connection: close\r\n\r\n");
        
        unsigned long timeout = millis();
        bool success = false;
        while (client4G.connected() && millis() - timeout < 10000) {
            if (client4G.available()) {
                String response = client4G.readStringUntil('\n');
                if (response.indexOf("200") >= 0 || response.indexOf("210") >= 0) {
                    success = true;
                }
                if (response.indexOf("Message sent") >= 0) {
                    success = true;
                }
                timeout = millis();
            }
            delay(10);
        }
        
        client4G.stop();
        
        if (success) {
            Serial.println("✅ Alert sent via 4G!");
        }
        return success;
    }
    
    return false;
}

// ============================================================================================
// SEND ALERT (WiFi)
// ============================================================================================
bool sendAlertWhatsAppWiFi(String message) {
    if (waPhone == "" || waApiKey == "") {
        Serial.println("⚠️ WhatsApp alert: not configured");
        return false;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ WiFi not connected for alert");
        return false;
    }
    
    HTTPClient http;
    
    String phoneNumber = waPhone;
    if (!phoneNumber.startsWith("+")) {
        phoneNumber = "+" + phoneNumber;
    }
    
    String encodedMessage = urlEncode("🚨 *ALERT!* 🚨\n" + message);
    
    String url = "http://api.callmebot.com/whatsapp.php?phone=" + phoneNumber;
    url += "&text=" + encodedMessage;
    url += "&apikey=" + waApiKey;
    
    Serial.println("[WiFi] Sending WhatsApp alert...");
    
    http.begin(url);
    http.setTimeout(8000);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
        String response = http.getString();
        Serial.printf("[WiFi] Alert Response HTTP %d: %s\n", httpCode, response.c_str());
        
        if (httpCode == 200 || httpCode == 210) {
            if (response.indexOf("Message sent") >= 0 || response.indexOf("success") >= 0) {
                Serial.println("✅ WhatsApp alert sent via WiFi!");
                http.end();
                return true;
            } else {
                Serial.println("⚠️ Alert returned HTTP 200 but unexpected response");
                http.end();
                return true;
            }
        } else {
            Serial.printf("❌ WhatsApp alert failed: HTTP %d\n", httpCode);
        }
    } else {
        Serial.printf("❌ Alert request failed: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
    return false;
}

// ============================================================================================
// GET CURRENT NETWORK OPERATOR (για display)
// ============================================================================================
String getCurrentNetworkOperator() {
    if (WiFi.status() == WL_CONNECTED) {
        return "WiFi";
    }
    return lastKnownNetwork;
}

int getCurrentRSSI() {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.RSSI();
    }
    return lastKnownRSSI;
}