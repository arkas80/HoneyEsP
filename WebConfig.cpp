/*
 * ============================================================================================
 * WebConfig.cpp - HoneyEsp Web Configuration Server
 * Version: 0.4.9 - WITH APN PRESETS & CUSTOM ENTRY
 * ============================================================================================
 */

#include "WebConfig.h"
#include "Config.h"
#include "Sensors.h"
#include "ModemManager.h"
#include <WiFi.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// ============================================================================================// EXTERNAL VARIABLES (από main)
// ============================================================================================
extern long offset;
extern float smoothWeight;
extern bool debugMode;

// ============================================================================================
// GLOBAL OBJECTS
// ============================================================================================
WebServer server(80);
DNSServer dnsServer;
Preferences prefs;

// ============================================================================================
// CONFIG VARIABLES
// ============================================================================================
String wifiSSID;
String wifiPass;
String tsApiKey;
String waPhone;
String waApiKey;
String simApn;
uint32_t sleepTimeSec = 3600;
float scaleRatio = 1.0;
bool modemUseNbIot = false;

String lastScanResult = "";

// ============================================================================================
// APN PRESETS (Ελληνικοί πάροχοι και διεθνείς)
// ============================================================================================
struct ApnPreset {
    const char* name;
    const char* apn;
    const char* user;
    const char* pass;
};

const ApnPreset apnPresets[] = {
    {"🌐 Cosmote (Greece)", "internet", "", ""},
    {"🌐 Cosmote NB-IoT (Greece)", "nb-internet", "", ""},
    {"🌐 Vodafone (Greece)", "internet", "", ""},
    {"🌐 Vodafone NB-IoT (Greece)", "nb-internet", "", ""},
    {"🌐 Nova/Wind (Greece)", "internet", "", ""},
    {"🌐 Nova NB-IoT (Greece)", "nb-internet", "", ""},
    {"🇪🇺 European Roaming", "internet", "", ""},
    {"🌍 Global (Default)", "internet", "", ""},
    {"📱 Custom (Manual Entry)", "CUSTOM", "", ""}
};

const int apnPresetCount = sizeof(apnPresets) / sizeof(apnPresets[0]);

// ============================================================================================
// LOAD SETTINGS FROM FLASH
// ============================================================================================
void loadSettings() {
    prefs.begin("config", true);
    wifiSSID = prefs.getString("ssid", WIFI_SSID_DEFAULT);
    wifiPass = prefs.getString("pass", WIFI_PASS_DEFAULT);
    tsApiKey = prefs.getString("tskey", TS_API_KEY_DEFAULT);
    waPhone  = prefs.getString("phone", WA_PHONE_DEFAULT);
    waApiKey = prefs.getString("waapi", WA_API_KEY_DEFAULT);
    simApn   = prefs.getString("apn", MODEM_APN_DEFAULT);
    sleepTimeSec = prefs.getUInt("sleep", 3600);
    scaleRatio   = prefs.getFloat("scale", 1.0);
    modemUseNbIot = prefs.getBool("nbiot", false);
    prefs.end();
    
    Serial.println("[CONFIG] Settings loaded from flash");
}

// ============================================================================================
// SAVE SETTINGS TO FLASH
// ============================================================================================
void saveSettings() {
    prefs.begin("config", false);
    prefs.putString("ssid", wifiSSID);
    prefs.putString("pass", wifiPass);
    prefs.putString("tskey", tsApiKey);
    prefs.putString("phone", waPhone);
    prefs.putString("waapi", waApiKey);
    prefs.putString("apn", simApn);
    prefs.putUInt("sleep", sleepTimeSec);
    prefs.putFloat("scale", scaleRatio);
    prefs.putBool("nbiot", modemUseNbIot);
    prefs.end();
    
    Serial.println("[CONFIG] Settings saved to flash");
}

// ============================================================================================
// WIFI SCAN
// ============================================================================================
void performWiFiScan() {
    Serial.println("[WEB] 🔍 Scanning WiFi networks...");
    WiFi.scanDelete();
    int n = WiFi.scanNetworks();
    lastScanResult = "";
    
    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        int rssi = WiFi.RSSI(i);
        String encrypted = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? "🔒" : "🔓";
        
        if (ssid.length() > 0) {
            lastScanResult += "<option value=\"" + ssid + "\">" + encrypted + " " + ssid + " (" + String(rssi) + " dBm)</option>";
        }
    }
    
    if (lastScanResult == "") {
        lastScanResult = "<option value=\"\">❌ No networks found</option>";
    }
    
    WiFi.scanDelete();
    Serial.printf("[WEB] 📶 Found %d networks\n", n);
}

void handleScan() {
    performWiFiScan();
    server.send(200, "application/json", "{\"success\":true}");
}

// ============================================================================================
// LIVE MEASUREMENT (JSON)
// ============================================================================================
void handleLiveMeasurement() {
    float w = getWeight();
    float t, p, h;
    getBME(t, p, h);
    float dsTemp = getDS();
    float v = getBatteryVoltage();
    float batt = getBatteryPercent();
    
    // RSSI: Προτεραιότητα σε WiFi (αν είναι σε config mode, το WiFi είναι AP mode)
    int rssi = -999;
    String connectionType = "none";
    
    // Σε config mode, το WiFi είναι σε AP mode (όχi STA)
    // Οπότε δεν έχει σήμα από εξωτερικό δίκτυο
    if (WiFi.status() == WL_CONNECTED) {
        rssi = WiFi.RSSI();
        connectionType = "wifi";
    } else {
        // Δοκίμασε modem μόνο αν υπάρχει SIM
        if (initModemWithAPN()) {
            rssi = getModemRSSI();
            connectionType = "4g";
            powerOffModem();
        } else {
            // Default τιμή για config mode
            rssi = -75;
            connectionType = "config";
        }
    }

    // Αν το rssi είναι -999, βάλε default
    if (rssi == -999) rssi = -75;

    String response = "{";
    response += "\"success\":true,";
    response += "\"weight_grams\":" + String(isnan(w) ? 0 : w, 1) + ",";
    response += "\"weight_kg\":" + String(isnan(w) ? 0 : w / 1000.0, 2) + ",";
    response += "\"temperature_air\":" + String(isnan(t) ? 0 : t, 1) + ",";
    response += "\"temperature_hive\":" + (isnan(dsTemp) ? String("null") : String(dsTemp, 1)) + ",";
    response += "\"pressure\":" + String(isnan(p) ? 0 : p, 1) + ",";
    response += "\"humidity\":" + String(isnan(h) ? 0 : h, 1) + ",";
    response += "\"voltage\":" + String(isnan(v) ? 0 : v, 2) + ",";
    response += "\"battery_percent\":" + String(isnan(batt) ? 0 : batt, 0) + ",";
    response += "\"rssi\":" + String(rssi) + ",";
    response += "\"connection_type\":\"" + connectionType + "\"";
    response += "}";
    
    server.send(200, "application/json", response);
}


// ============================================================================================
// TARE SCALE
// ============================================================================================
void handleTare() {
    Serial.println("\n=========================================");
    Serial.println("[WEB] ⚖️ TARE REQUESTED FROM BROWSER");
    Serial.println("=========================================");
    
    // Έλεγχος αν η ζυγαριά είναι έτοιμη
    if (!scale.is_ready()) {
        Serial.println("❌ Scale not ready for tare!");
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Scale not ready\"}");
        return;
    }
    
    float before = getWeight();
    Serial.printf("📊 Before tare - Weight: %.1f g\n", before);
    
    tareScale();
    delay(500);  // Αύξησε το delay
    
    smoothWeight = 0;  // Reset smoothing
    delay(500);
    
    float after = getWeight();
    Serial.printf("📊 After tare - Weight: %.1f g\n", after);
    Serial.println("=========================================\n");
    
    String response = "{\"success\":true,\"message\":\"✅ Η ζυγαριά μηδενίστηκε επιτυχώς! Νέο βάρος: " + String(after, 1) + " g\"}";
    server.send(200, "application/json", response);
}
// ============================================================================================
// CALIBRATE SCALE
// ============================================================================================
void handleCalibrate() {
    Serial.println("\n╔══════════════════════════════════════════╗");
    Serial.println("║         📐 CALIBRATION START             ║");
    Serial.println("╚══════════════════════════════════════════╝");

    if (!server.hasArg("weight")) {
        Serial.println("❌ ERROR: Missing weight parameter - Λείπει η παράμετρος βάρους");
        server.send(400, "application/json", "{\"success\":false,\"error\":\"missing weight\"}");
        return;
    }

    float knownWeight = server.arg("weight").toFloat();

    if (knownWeight <= 0 || knownWeight > 200000) {
        Serial.printf("❌ ERROR: Invalid weight value: %.1f g\n", knownWeight);
        server.send(400, "application/json", "{\"success\":false,\"error\":\"invalid weight- Μη έγκυρη τιμή βάρους\"}");
        return;
    }

    Serial.printf("🎯 Known weight: %.1f g\n", knownWeight);
    
    Serial.println("\n⚠️ IMPORTANT: Make sure you have TARED with EMPTY scale first!");
    Serial.println("   If not, cancel and do TARE first!\n");
    delay(2000);

    if (!scale.is_ready()) {
        Serial.println("⏳ HX711 not ready, waiting...");
        unsigned long start = millis();
        while (!scale.is_ready() && millis() - start < 5000) {
            delay(100);
        }
        if (!scale.is_ready()) {
            Serial.println("❌ ERROR: HX711 still not ready!");
            server.send(500, "application/json", "{\"success\":false,\"error\":\"Η ζυγαριά δεν είναι έτοιμη!\"}");
      return;
    }
        Serial.println("✅ HX711 is ready!");
    }

    long currentOffset = scale.get_offset();

    Serial.println("\n📊 Reading WITH weight...");
    Serial.println("   DO NOT TOUCH THE SCALE!");
    delay(1500);

    long rawWithWeight = scale.read_average(30);
    long difference = rawWithWeight - currentOffset;

    Serial.println("\n--- 📊 CALCULATION DETAILS ---");
    Serial.printf("📊 OFFSET: %ld\n", currentOffset);
    Serial.printf("📊 RAW with weight: %ld\n", rawWithWeight);
    Serial.printf("📈 DIFFERENCE: %ld\n", abs(difference));
    Serial.printf("🎯 Known weight: %.1f g\n", knownWeight);

    if (abs(difference) < 10) {
        Serial.println("❌ ERROR: No weight detected!");
        server.send(500, "application/json", "{\"success\":false,\"error\":\"no weight detected - check wiring or tare - Δεν ανιχνεύθηκε βάρος - ελέγξτε τις συνδέσεις\"}");
        return;
    }

    float newFactor = (float)abs(difference) / knownWeight;

    Serial.printf("📐 New SCALE FACTOR: %.5f\n", newFactor);

    if (newFactor < 0.0001 || newFactor > 50000) {
        Serial.printf("❌ ERROR: Unrealistic scale factor: %.5f\n", newFactor);
        server.send(500, "application/json", "{\"success\":false,\"error\":\"bad calibration factor\"}");
        return;
    }

    Serial.println("\n💾 Saving calibration...");

    setCalibration(newFactor, false);

    // Αποθήκευση της παλιάς τιμής debugMode
    bool oldDebugMode = debugMode;
    debugMode = true;      // Απενεργοποίηση smoothing για ακριβή μέτρηση
    smoothWeight = 0;

    Serial.println("⏳ Waiting for weight to stabilize...");
    for (int i = 0; i < 10; i++) {
        float temp = getWeight();
        Serial.printf("   [%d] %.1f g\n", i+1, temp);
        delay(200);
    }

    float testWeight = getWeight();
    debugMode = oldDebugMode;  // Επαναφορά στην προηγούμενη τιμή

    float error = abs(testWeight - knownWeight);
    float errorPercent = (error / knownWeight) * 100.0;

    Serial.println("\n--- 🧪 VERIFICATION ---");
    Serial.printf("📊 Measured: %.1f g\n", testWeight);
    Serial.printf("🎯 Expected: %.1f g\n", knownWeight);
    Serial.printf("❌ Error: %.1f g (%.1f%%)\n", error, errorPercent);

    if (errorPercent > 10) {
        Serial.println("⚠️ WARNING: High error! Try:");
        Serial.println("   • Better tare");
        Serial.println("   • More stable surface");
        Serial.println("   • Heavier calibration weight");
    }

    Serial.println("╚══════════════════════════════════════════╝\n");

    String response = "{";
    response += "\"success\":true,";
  response += "\"message\":\"✅ Η βαθμονόμηση ολοκληρώθηκε με επιτυχία!\",";
    response += "\"known_weight\":" + String(knownWeight, 1) + ",";
    response += "\"offset\":" + String(currentOffset) + ",";
    response += "\"raw_weight\":" + String(rawWithWeight) + ",";
    response += "\"difference\":" + String(abs(difference)) + ",";
    response += "\"new_factor\":" + String(newFactor, 5) + ",";
    response += "\"measured\":" + String(testWeight, 1) + ",";
    response += "\"error_percent\":" + String(errorPercent, 1);
    response += "}";

    server.send(200, "application/json", response);
    
    Serial.println("[DEBUG] Calibration completed, debugMode restored");
}

// ============================================================================================
// SYSTEM STATUS
// ============================================================================================
void handleStatus() {
    String response = "{";
    response += "\"status\":\"ok\",";
    response += "\"uptime_seconds\":" + String(millis() / 1000) + ",";
    response += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    response += "\"free_psram\":" + String(ESP.getFreePsram()) + ",";
    response += "\"cpu_freq\":" + String(ESP.getCpuFreqMHz());
    response += "}";
    server.send(200, "application/json", response);
}

// ============================================================================================
// SLEEP SETTINGS
// ============================================================================================
void handleSleep() {
    if (server.hasArg("seconds")) {
        sleepTimeSec = server.arg("seconds").toInt();
        if (sleepTimeSec < 10) sleepTimeSec = 10;
        if (sleepTimeSec > 86400) sleepTimeSec = 86400;
        saveSettings();
        server.send(200, "application/json", "{\"success\":true,\"sleep_seconds\":" + String(sleepTimeSec) + "}");
    } else {
        server.send(200, "application/json", "{\"sleep_seconds\":" + String(sleepTimeSec) + "}");
    }
}

// ============================================================================================
// BATTERY INFO
// ============================================================================================
void handleBattery() {
    float voltage = getBatteryVoltage();
    float percent = getBatteryPercent();
    
    String response = "{";
    response += "\"voltage\":" + String(voltage, 2) + ",";
    response += "\"percent\":" + String(percent, 0);
    if (percent > 0 && sleepTimeSec > 0) {
        int cyclesPerHour = 3600 / sleepTimeSec;
        int hoursLeft = (percent / 5) * cyclesPerHour;
        if (hoursLeft > 0 && hoursLeft < 1000) {
            response += ",\"estimated_hours\":" + String(hoursLeft);
        }
    }
    response += "}";
    server.send(200, "application/json", response);
}

// ============================================================================================
// NETWORK INFO (Operator + RSSI)
// ============================================================================================
void handleNetworkInfo() {
    String operatorName = "none";
    int rssi = -999;
    
    // Σε config mode, το WiFi είναι σε AP mode
    if (WiFi.softAPgetStationNum() > 0) {
        operatorName = "AP Mode";
        rssi = -65;  // Default good signal for AP mode
    }
    // Έλεγχος για modem (αν υπάρχει SIM)
    else if (initModemWithAPN()) {
        if (modem.isNetworkConnected()) {
            operatorName = getNetworkOperator();
            if (operatorName == "") operatorName = "4G";
            rssi = getModemRSSI();
        }
        powerOffModem();
    }
    // Έλεγχος για WiFi client mode
    else if (WiFi.status() == WL_CONNECTED) {
        operatorName = "WiFi";
        rssi = WiFi.RSSI();
    }
    
    // Default αν δεν υπάρχει τίποτα
    if (operatorName == "none") {
        operatorName = "Config Mode";
        rssi = -75;
    }
    if (rssi == -999) rssi = -75;
    
    String response = "{";
    response += "\"operator\":\"" + operatorName + "\",";
    response += "\"rssi\":" + String(rssi);
    response += "}";
    
    server.send(200, "application/json", response);
}

// ============================================================================================
// REBOOT DEVICE
// ============================================================================================
void handleReboot() {
    String html = "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>HoneyEsp - Επανεκκίνηση</title>"
    "<style>"
    "* { margin: 0; padding: 0; box-sizing: border-box; }"
    "body {"
    "    font-family: 'Segoe UI', Arial, sans-serif;"
    "    background: linear-gradient(135deg, #0a0a0a 0%, #1a1a2e 100%);"
    "    min-height: 100vh;"
    "    display: flex;"
    "    justify-content: center;"
    "    align-items: center;"
    "    color: #e0e0e0;"
    "}"
    ".container {"
    "    text-align: center;"
    "    padding: 40px;"
    "    background: rgba(26,26,46,0.95);"
    "    border-radius: 20px;"
    "    backdrop-filter: blur(10px);"
    "    border: 1px solid #f44336;"
    "    box-shadow: 0 10px 40px rgba(0,0,0,0.5);"
    "}"
    ".icon { font-size: 64px; margin-bottom: 20px; }"
    "h2 { color: #f44336; margin-bottom: 15px; }"
    "p { margin: 15px 0; line-height: 1.6; }"
    ".spinner {"
    "    display: inline-block;"
    "    width: 40px;"
    "    height: 40px;"
    "    border: 4px solid #333;"
    "    border-top: 4px solid #f44336;"
    "    border-radius: 50%;"
    "    animation: spin 1s linear infinite;"
    "    margin: 20px auto;"
    "}"
    "@keyframes spin {"
    "    0% { transform: rotate(0deg); }"
    "    100% { transform: rotate(360deg); }"
    "}"
    "</style>"
    "</head>"
    "<body>"
    "<div class='container'>"
    "<div class='icon'>🔄</div>"
    "<h2>Επανεκκίνηση συσκευής...</h2>"
    "<p>Παρακαλώ περιμένετε, η συσκευή επανεκκινείται.</p>"
    "<div class='spinner'></div>"
    "<p style='font-size:12px; color:#888; margin-top:20px;'>Αυτό μπορεί να διαρκέσει λίγα δευτερόλεπτα.</p>"
    "</div>"
    "</body>"
    "</html>";
    
    server.send(200, "text/html", html);
    delay(1000);
    ESP.restart();
}

// ============================================================================================
// SAVE ALL SETTINGS
// ============================================================================================
void handleSave() {
    if (server.hasArg("ssid")) wifiSSID = server.arg("ssid");
    if (server.hasArg("pass")) wifiPass = server.arg("pass");
    if (server.hasArg("tskey")) tsApiKey = server.arg("tskey");
    if (server.hasArg("phone")) waPhone = server.arg("phone");
    if (server.hasArg("waapi")) waApiKey = server.arg("waapi");
    
    if (server.hasArg("nbiot")) {
        modemUseNbIot = (server.arg("nbiot") == "true" || server.arg("nbiot") == "1");
    }
    
    if (server.hasArg("apn_preset")) {
        String apnPreset = server.arg("apn_preset");
        if (apnPreset == "📱 Custom (Manual Entry)") {
            if (server.hasArg("apn_custom")) {
                simApn = server.arg("apn_custom");
                if (simApn.length() == 0) simApn = "internet";
            }
        } else {
            for (int i = 0; i < apnPresetCount; i++) {
                if (apnPreset == apnPresets[i].name) {
                    simApn = apnPresets[i].apn;
                    break;
                }
            }
        }
    }
    
    if (server.hasArg("sleep")) {
        sleepTimeSec = server.arg("sleep").toInt();
        if (sleepTimeSec < 300) sleepTimeSec = 300;
        if (sleepTimeSec > 86400) sleepTimeSec = 86400;
    }
    if (server.hasArg("scale")) {
        scaleRatio = server.arg("scale").toFloat();
    }
    
    saveSettings();
    
    // ΠΛΗΡΩΣ ΔΙΟΡΘΩΜΕΝΟ ΜΗΝΥΜΑ - ΧΩΡΙΣ ΠΑΡΑΞΕΝΟΥΣ ΧΑΡΑΚΤΗΡΕΣ
    String html = "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>HoneyEsp - Αποθήκευση</title>"
    "<style>"
    "* { margin: 0; padding: 0; box-sizing: border-box; }"
    "body {"
    "    font-family: 'Segoe UI', Arial, sans-serif;"
    "    background: linear-gradient(135deg, #0a0a0a 0%, #1a1a2e 100%);"
    "    min-height: 100vh;"
    "    display: flex;"
    "    justify-content: center;"
    "    align-items: center;"
    "    color: #e0e0e0;"
    "}"
    ".container {"
    "    text-align: center;"
    "    padding: 40px;"
    "    background: rgba(26,26,46,0.95);"
    "    border-radius: 20px;"
    "    backdrop-filter: blur(10px);"
    "    border: 1px solid #ff9800;"
    "    box-shadow: 0 10px 40px rgba(0,0,0,0.5);"
    "    max-width: 500px;"
    "    margin: 20px;"
    "}"
    ".icon { font-size: 64px; margin-bottom: 20px; }"
    "h2 { color: #ff9800; margin-bottom: 15px; }"
    "p { margin: 15px 0; line-height: 1.6; }"
    ".spinner {"
    "    display: inline-block;"
    "    width: 40px;"
    "    height: 40px;"
    "    border: 4px solid #333;"
    "    border-top: 4px solid #ff9800;"
    "    border-radius: 50%;"
    "    animation: spin 1s linear infinite;"
    "    margin: 20px auto;"
    "}"
    "@keyframes spin {"
    "    0% { transform: rotate(0deg); }"
    "    100% { transform: rotate(360deg); }"
    "}"
    ".info-box {"
    "    background: rgba(0,0,0,0.3);"
    "    padding: 15px;"
    "    border-radius: 10px;"
    "    margin-top: 20px;"
    "    font-size: 13px;"
    "    color: #aaa;"
    "    text-align: left;"
    "}"
    ".info-box span { color: #ff9800; }"
    "</style>"
    "</head>"
    "<body>"
    "<div class='container'>"
    "<div class='icon'>💾</div>"
    "<h2>Οι ρυθμίσεις αποθηκεύτηκαν!</h2>"
    "<p>Η συσκευή θα επανεκκινήσει σε λίγα δευτερόλεπτα...</p>"
    "<div class='spinner'></div>"
    "<div class='info-box'>"
    "📋 <span>Ρυθμίσεις που αποθηκεύτηκαν:</span><br>"
    "• 📡 WiFi SSID: " + wifiSSID + "<br>"
    "• ⏰ Διάστημα ύπνου: " + String(sleepTimeSec) + " δευτερόλεπτα (" + String(sleepTimeSec/60) + " λεπτά)<br>"
    "• ⚖️ Συντελεστής ζυγαριάς: " + String(scaleRatio) + "<br>"
    "📱 APN: " + simApn + "<br>"
    "</div>"
    "</div>"
    "</body>"
    "</html>";
    
    server.send(200, "text/html", html);
    delay(2500);
    ESP.restart();
}


// ============================================================================================
// HTML PAGE GENERATOR
// ============================================================================================
String getHTML() {
    if (lastScanResult == "") {
        performWiFiScan();
    }
    
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>🍯 HoneyEsp Config</title>
<style>
* { box-sizing: border-box; }
body {
    font-family: 'Segoe UI', Arial, sans-serif;
    background: linear-gradient(135deg, #0a0a0a 0%, #1a1a2e 100%);
    color: #e0e0e0;
    margin: 0;
    padding: 20px;
    min-height: 100vh;
}
.container {
    max-width: 750px;
    margin: 0 auto;
}
h1 {
    color: #ff9800;
    text-align: center;
    font-size: 32px;
    margin-bottom: 5px;
    text-shadow: 0 0 20px rgba(255,152,0,0.5);
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 10px;
}
.subtitle {
    text-align: center;
    color: #aaa;
    margin-bottom: 30px;
    font-size: 14px;
    border-bottom: 1px solid #333;
    padding-bottom: 15px;
}
h2 {
    color: #ff9800;
    font-size: 18px;
    margin: 0 0 15px 0;
    padding-bottom: 8px;
    border-bottom: 1px solid #333;
    display: flex;
    align-items: center;
    gap: 8px;
}
.card {
    background: rgba(26,26,26,0.9);
    border-radius: 16px;
    padding: 20px;
    margin-bottom: 20px;
    border: 1px solid #2a2a2a;
    backdrop-filter: blur(10px);
    box-shadow: 0 4px 15px rgba(0,0,0,0.3);
}
input, select {
    width: 100%;
    padding: 12px;
    margin: 8px 0;
    border-radius: 10px;
    border: 1px solid #444;
    background: #1e1e1e;
    color: #fff;
    font-size: 14px;
    transition: all 0.3s ease;
}
input:focus, select:focus {
    outline: none;
    border-color: #ff9800;
    box-shadow: 0 0 10px rgba(255,152,0,0.3);
}
button {
    padding: 12px 20px;
    margin: 8px 5px;
    border: none;
    border-radius: 10px;
    font-size: 14px;
    font-weight: bold;
    cursor: pointer;
    transition: all 0.3s ease;
    display: inline-flex;
    align-items: center;
    gap: 6px;
}
button:hover { 
    opacity: 0.9; 
    transform: translateY(-2px); 
    box-shadow: 0 5px 15px rgba(0,0,0,0.3);
}
.btn-primary { background: linear-gradient(135deg, #ff9800, #f57c00); color: #1a1a1a; }
.btn-success { background: linear-gradient(135deg, #00c853, #00a844); color: white; }
.btn-warning { background: linear-gradient(135deg, #ffc107, #ffa000); color: #1a1a1a; }
.btn-danger { background: linear-gradient(135deg, #f44336, #d32f2f); color: white; }
.btn-info { background: linear-gradient(135deg, #2196f3, #1976d2); color: white; }
.btn-group { display: flex; flex-wrap: wrap; gap: 10px; margin-top: 10px; }
.result-box {
    background: linear-gradient(135deg, #0d0d0d, #0a0a0a);
    padding: 15px;
    margin-top: 15px;
    border-radius: 10px;
    font-family: 'Courier New', monospace;
    font-size: 14px;
    border-left: 4px solid #ff9800;
    line-height: 1.8;
}
.flex-row { display: flex; gap: 10px; align-items: center; }
.flex-row select { flex: 1; }
.flex-row button { margin: 0; white-space: nowrap; }
.password-wrapper {
    display: flex;
    gap: 10px;
    align-items: center;
    width: 100%;
}
.password-wrapper input {
    flex: 1;
    margin: 0;
}
.password-wrapper button {
    margin: 0;
    padding: 12px 16px;
    white-space: nowrap;
}
.spinner {
    display: inline-block;
    width: 20px;
    height: 20px;
    border: 3px solid #333;
    border-top: 3px solid #ff9800;
    border-radius: 50%;
    animation: spin 1s linear infinite;
    margin-left: 10px;
}
@keyframes spin {
    0% { transform: rotate(0deg); }
    100% { transform: rotate(360deg); }
}
.footer {
    text-align: center;
    margin-top: 20px;
    color: #666;
    font-size: 12px;
}
.info-text {
    font-size: 12px;
    color: #888;
    margin-top: 5px;
}
.custom-apn-field {
    display: none;
    margin-top: 10px;
}
.custom-apn-field.show {
    display: block;
}
.setting-group {
    margin-bottom: 15px;
}
.setting-group label {
    display: block;
    margin-bottom: 5px;
    font-weight: bold;
    font-size: 14px;
}
</style>
</head>
<body>
<div class="container">
<h1>
    <span>🍯</span> HoneyEsp 
    <span style="font-size:16px; background:#ff9800; color:#1a1a1a; padding:2px 10px; border-radius:20px;">)rawliteral";
    
    html += VERSION;
    
    html += R"rawliteral(</span>
</h1>
<div class="subtitle">🐝 Smart Hive Monitor • Web Configuration</div>

<!-- 📡 WiFi Settings -->
<div class="card">
<h2><span>📡</span> WiFi Settings</h2>
<div class="flex-row">
    <select id="wifiSelect" onchange="document.getElementById('ssid').value=this.value">
        <option value="">-- 🔍 Select Network --</option>
)rawliteral";
    
    html += lastScanResult;
    
    html += R"rawliteral(
    </select>
    <button class="btn-info" onclick="refreshWiFiList()">🔄 Scan</button>
</div>
<input type="text" id="ssid" placeholder="📶 WiFi SSID" value=")rawliteral" + wifiSSID + R"rawliteral(">
<div class="password-wrapper">
    <input type="password" id="pass" placeholder="🔑 Password" value=")rawliteral" + wifiPass + R"rawliteral(">
    <button type="button" class="btn-info" onclick="togglePassword()" id="toggleBtn">👁️ Show</button>
</div>
</div>

<!-- 📊 ThingSpeak -->
<div class="card">
<h2><span>📊</span> ThingSpeak</h2>
<input type="text" id="tskey" placeholder="🔑 API Key" value=")rawliteral" + tsApiKey + R"rawliteral(">
</div>

<!-- 💬 WhatsApp -->
<div class="card">
<h2><span>💬</span> WhatsApp Alerts</h2>
<input type="text" id="phone" placeholder="📱 Phone (+3069...)" value=")rawliteral" + waPhone + R"rawliteral(">
<input type="text" id="waapi" placeholder="🔐 CallMeBot API Key" value=")rawliteral" + waApiKey + R"rawliteral(">
</div>

<!-- 📱 4G Settings -->
<div class="card">
<h2><span>📱</span> 4G Fallback</h2>
<select id="apnPreset" onchange="toggleApnCustom()">
)rawliteral";

    // Add APN presets
    for (int i = 0; i < apnPresetCount; i++) {
        html += "<option value=\"" + String(apnPresets[i].name) + "\"";
        
        // Check if current simApn matches any preset
        bool matched = false;
        for (int j = 0; j < apnPresetCount; j++) {
            if (simApn == apnPresets[j].apn && String(apnPresets[i].name) == String(apnPresets[j].name)) {
                html += " selected";
                matched = true;
                break;
            }
        }
        // If no match and it's custom, select custom option
        if (!matched && i == apnPresetCount - 1 && simApn != "internet" && simApn != "nb-internet") {
            html += " selected";
        }
        html += ">" + String(apnPresets[i].name) + "</option>";
    }

html += R"rawliteral(
</select>
<div id="customApnDiv" class="custom-apn-field">
    <input type="text" id="apnCustom" placeholder="✏️ Enter custom APN (e.g., myinternet.gr)" value=")rawliteral" + simApn + R"rawliteral(">
    <div class="info-text">💡 Enter your provider's custom APN (e.g., "internet", "web.gprs", "my-internet")</div>
</div>
<div id="apnInfo" class="info-text" style="margin-top: 8px;">
    ℹ️ Standard APN for Greece: Cosmote/Vodafone/Nova: "internet"<br>
    📡 NB-IoT APN: "nb-internet" (for SIM7080)
</div>

<!-- NB-IoT Mode Toggle -->
<div class="setting-group" style="margin-top: 15px;">
    <label>📡 NB-IoT Mode (for SIM7080)</label>
    <select id="nbiot">
        <option value="0" )rawliteral" + String(modemUseNbIot ? "" : "selected") + R"rawliteral(>❌ Disabled (4G/LTE)</option>
        <option value="1" )rawliteral" + String(modemUseNbIot ? "selected" : "") + R"rawliteral(>✅ Enabled (NB-IoT)</option>
    </select>
    <div class="info-text">💡 NB-IoT uses less power but slower data. Works with SIM7080 only.</div>
</div>
</div>

<!-- ⏰ Timer & Scale Settings -->
<div class="card">
    <h2><span>⏰⚖️</span> Operation Settings</h2>
    
    <div class="setting-group">
        <label>🕐 Measurement Interval</label>
        <input type="number" id="sleep" placeholder="seconds" value=")rawliteral" + String(sleepTimeSec) + R"rawliteral(">
        <div class="info-text" style="color: #ff9800;">
            💡 <strong>Important:</strong> Longer intervals save battery (3600 = 1 hour)
        </div>
    </div>
    
    <div class="setting-group">
        <label>⚖️ Scale Factor</label>
        <input type="text" id="scale" placeholder="e.g., 42000.0" value=")rawliteral" + String(scaleRatio) + R"rawliteral(">
        <div class="info-text" style="color: #ff9800;">
            💡 <strong>Warning:</strong> Only change if you have known weight for calibration!
        </div>
    </div>
</div>

<script>
document.getElementById('sleep').addEventListener('change', function() {
    let val = parseInt(this.value);
    if (val < 300) {
        alert('Interval cannot be less than 300 seconds (5 minutes) for battery protection!');
        this.value = 300;
    }
});
</script>

<!-- 💾 Save Button -->
<button class="btn-primary" onclick="saveSettings()" style="width:100%; padding:15px; font-size:16px;">
    💾 Save All Settings & Reboot
</button>

<!-- 🎮 Live Control -->
<div class="card">
<h2><span>🎮</span> Live Control</h2>
<div class="btn-group">
    <button class="btn-success" onclick="liveMeasurement()">📊 Live Measure</button>
    <button class="btn-warning" onclick="tareScale()" id="tareBtn">⚖️ Tare Scale</button>
</div>
<div id="liveResult" class="result-box">
    <span style="color:#888;">👆 Click "Live Measure" to see readings...</span>
</div>
</div>

<!-- 📐 Calibration -->
<div class="card">
<h2><span>📐</span> Scale Calibration</h2>
<p style="font-size:13px; color:#aaa; margin:0 0 10px 0;">🎯 Place a known weight and enter its value:</p>
<input type="number" id="calWeight" placeholder="⚖️ Weight in grams (e.g., 5000)">
<button class="btn-warning" onclick="calibrateScale()" style="width:100%">📏 Calibrate Now</button>
<div id="calResult" class="result-box"></div>
</div>


<!-- ℹ️ System Info -->
<div class="card">
<h2><span>ℹ️</span> System Info</h2>
<div class="btn-group">
    <button class="btn-info" onclick="getStatus()">📈 Status</button>
    <button class="btn-info" onclick="scanWiFi()">🔍 Scan WiFi</button>
    <button class="btn-info" onclick="getBattery()">🔋 Battery</button>
    <button class="btn-info" onclick="getNetworkInfo()">📡 Network</button>  <!-- ← ΝΕΟ -->
</div>
<div id="sysResult" class="result-box"></div>
<div id="networkResult" class="result-box" style="margin-top:10px;"></div>  <!-- ← ΝΕΟ -->
</div>

<!-- 🔄 Reboot -->
<button class="btn-danger" onclick="rebootDevice()" style="width:100%">
    🔄 Reboot Device Now
</button>

<div class="footer">
    🍯 HoneyEsp by Arkas Electronics • Smart Hive Monitor • v)rawliteral" + String(VERSION) + R"rawliteral(<br>
    🌐 Access: 192.168.4.1 • honeyesp.local
</div>
<br>
</div>

<script>
function togglePassword() {
    var passInput = document.getElementById('pass');
    var toggleBtn = document.getElementById('toggleBtn');
    
    if (passInput.type === 'password') {
        passInput.type = 'text';
        toggleBtn.innerHTML = '🙈 Hide';
        toggleBtn.style.background = '#ff9800';
    } else {
        passInput.type = 'password';
        toggleBtn.innerHTML = '👁️ Show';
        toggleBtn.style.background = '#2196f3';
    }
}

function toggleApnCustom() {
    var preset = document.getElementById('apnPreset').value;
    var customDiv = document.getElementById('customApnDiv');
    
    if (preset === '📱 Custom (Manual Entry)') {
        customDiv.classList.add('show');
    } else {
        customDiv.classList.remove('show');
    }
}

function saveSettings() {
    var ssid = document.getElementById('ssid').value;
    var pass = document.getElementById('pass').value;
    var tskey = document.getElementById('tskey').value;
    var phone = document.getElementById('phone').value;
    var waapi = document.getElementById('waapi').value;
    var apnPreset = document.getElementById('apnPreset').value;
    var apnCustom = document.getElementById('apnCustom').value;
    var sleep = document.getElementById('sleep').value;
    var scale = document.getElementById('scale').value;
    var nbiot = document.getElementById('nbiot').value;
    
    var url = '/save?ssid=' + encodeURIComponent(ssid) +
              '&pass=' + encodeURIComponent(pass) +
              '&tskey=' + encodeURIComponent(tskey) +
              '&phone=' + encodeURIComponent(phone) +
              '&waapi=' + encodeURIComponent(waapi) +
              '&apn_preset=' + encodeURIComponent(apnPreset) +
              '&apn_custom=' + encodeURIComponent(apnCustom) +
              '&sleep=' + encodeURIComponent(sleep) +
              '&scale=' + encodeURIComponent(scale) +
              '&nbiot=' + encodeURIComponent(nbiot);
    
    document.body.innerHTML = '<div style="text-align:center; margin-top:100px;"><h2>💾 Saving settings...</h2><p>🔄 Device will reboot...</p><div class="spinner"></div></div>';
    window.location.href = url;
}

function refreshWiFiList() {
    var select = document.getElementById('wifiSelect');
    select.innerHTML = '<option value="">🔍 Scanning...</option>';
    fetch('/scan').then(function() { location.reload(); });
}

function liveMeasurement() {
    var resultDiv = document.getElementById('liveResult');
    resultDiv.innerHTML = '📡 Reading sensors... <span class="spinner"></span>';
    fetch('/live')
        .then(function(r) { return r.json(); })
        .then(function(data) {
            var hiveTemp = (data.temperature_hive !== null) ? data.temperature_hive.toFixed(1) : 'N/A';
            var signalDisplay = data.rssi + ' dBm';
            if (data.connection_type === 'config') {
                signalDisplay = 'Config Mode';
            } else if (data.rssi === -999) {
                signalDisplay = 'No Signal';
            }
            resultDiv.innerHTML = 
                '⚖️ Weight: <strong>' + data.weight_kg.toFixed(2) + ' kg</strong> (' + data.weight_grams.toFixed(0) + ' g)<br>' +
                '🌡️ Air Temp: <strong>' + data.temperature_air.toFixed(1) + ' °C</strong><br>' +
                '🐝 Hive Temp: <strong>' + hiveTemp + ' °C</strong><br>' +
                '💧 Humidity: <strong>' + data.humidity.toFixed(1) + ' %</strong><br>' +
                '🔋 Battery: <strong>' + data.battery_percent.toFixed(0) + ' %</strong> (' + data.voltage.toFixed(2) + ' V)<br>' +
                '📶 Signal: <strong>' + signalDisplay + '</strong>';
        })
        .catch(function() { 
            resultDiv.innerHTML = '❌ Error reading sensors! Check connections.'; 
        });
}

function tareScale() {
    if (!confirm('⚠️ IMPORTANT!\n\nMake sure the scale is EMPTY (no hive)!\n\nPress OK to tare (zero) the scale.')) {
        return;
    }
    
    var resultDiv = document.getElementById('liveResult');
    var tareBtn = document.getElementById('tareBtn');
    
    tareBtn.disabled = true;
    tareBtn.style.opacity = '0.5';
    resultDiv.innerHTML = '⚖️ Performing tare... Please wait (3 sec) <span class="spinner"></span>';
    
    fetch('/tare')
        .then(function(r) { return r.json(); })
        .then(function(data) {
            if (data.success) {
                resultDiv.innerHTML = '✅ ' + data.message + '<br>';
                setTimeout(function() { liveMeasurement(); }, 1000);
            } else {
                resultDiv.innerHTML = '❌ Tare failed!';
            }
            tareBtn.disabled = false;
            tareBtn.style.opacity = '1';
        })
        .catch(function(error) {
            resultDiv.innerHTML = '❌ Tare failed!';
            tareBtn.disabled = false;
            tareBtn.style.opacity = '1';
        });
}

function calibrateScale() {
    var weight = document.getElementById('calWeight').value;
    if (!weight || weight <= 0) { 
        alert('❌ Enter valid weight!'); 
        return; 
    }
    
    var resultDiv = document.getElementById('calResult');
    resultDiv.innerHTML = '📐 Calibrating with ' + weight + 'g...<br>' +
                          '⚠️ DO NOT TOUCH THE SCALE!<br>' +
                          '<span class="spinner"></span>';
    
    fetch('/calibrate?weight=' + weight)
        .then(function(r) { return r.json(); })
        .then(function(data) {
            if (data.success) {
                resultDiv.innerHTML = 
                    '✅ Calibration successful!<br>' +
                    '📏 New factor: <strong>' + data.new_factor.toFixed(3) + '</strong><br>' +
                    '🎯 Known: ' + data.known_weight.toFixed(1) + ' g<br>' +
                    '⚖️ Measured: ' + data.measured.toFixed(1) + ' g (' + data.error_percent.toFixed(1) + '% error)';
                
                setTimeout(function() { 
                    liveMeasurement(); 
                }, 1000);
            } else { 
                resultDiv.innerHTML = '❌ Calibration failed: ' + (data.error || 'Unknown error'); 
            }
        })
        .catch(function(error) {
            resultDiv.innerHTML = '❌ Calibration error: ' + error.message;
        });
}

function getStatus() {
    var resultDiv = document.getElementById('sysResult');
    resultDiv.innerHTML = '📊 Fetching status... <span class="spinner"></span>';
    fetch('/status')
        .then(function(r) { return r.json(); })
        .then(function(data) {
            resultDiv.innerHTML = 
                '⏱️ Uptime: <strong>' + (data.uptime_seconds / 3600).toFixed(1) + ' hours</strong><br>' +
                '💾 Free Heap: <strong>' + (data.free_heap / 1024).toFixed(0) + ' KB</strong><br>' +
                '📦 Free PSRAM: <strong>' + (data.free_psram / 1024).toFixed(0) + ' KB</strong><br>' +
                '⚡ CPU Freq: <strong>' + data.cpu_freq + ' MHz</strong>';
        })
        .catch(function() { resultDiv.innerHTML = '❌ Error fetching status!'; });
}

function scanWiFi() {
    var resultDiv = document.getElementById('sysResult');
    resultDiv.innerHTML = '🔍 Scanning WiFi networks... <span class="spinner"></span>';
    fetch('/scan').then(function() { location.reload(); });
}

function getBattery() {
    var resultDiv = document.getElementById('sysResult');
    resultDiv.innerHTML = '🔋 Reading battery... <span class="spinner"></span>';
    fetch('/battery')
        .then(function(r) { return r.json(); })
        .then(function(data) {
            var hours = data.estimated_hours || 'N/A';
            if (hours !== 'N/A') hours = hours.toFixed(0) + ' hours';
            var battEmoji = data.percent > 50 ? '🟢' : (data.percent > 20 ? '🟡' : '🔴');
            resultDiv.innerHTML = 
                '🔋 Voltage: <strong>' + data.voltage.toFixed(2) + ' V</strong><br>' +
                '📊 Charge: <strong>' + data.percent.toFixed(0) + ' %</strong> ' + battEmoji + '<br>' +
                '⏳ Est. remaining: <strong>' + hours + '</strong>';
        })
        .catch(function() { resultDiv.innerHTML = '❌ Error reading battery!'; });
}

function getNetworkInfo() {
    var resultDiv = document.getElementById('networkResult');
    resultDiv.innerHTML = '📡 Fetching network info... <span class="spinner"></span>';
    fetch('/network')
        .then(function(r) { return r.json(); })
        .then(function(data) {
            var signalBars = '';
            if (data.rssi > -55) signalBars = '█████';
            else if (data.rssi > -65) signalBars = '████';
            else if (data.rssi > -75) signalBars = '███';
            else if (data.rssi > -85) signalBars = '██';
            else if (data.rssi > -95) signalBars = '█';
            else signalBars = '░';
            
            resultDiv.innerHTML = 
                '🌐 Network: <strong>' + data.operator + '</strong><br>' +
                '📶 Signal: <strong>' + data.rssi + ' dBm</strong><br>' +
                '📊 Signal strength: ' + signalBars;
        })
        .catch(function() { 
            resultDiv.innerHTML = '❌ Error fetching network info!'; 
        });
}

function rebootDevice() {
    if (confirm('🔄 Are you sure you want to reboot?')) {
        document.body.innerHTML = '<div style="text-align:center; margin-top:100px;"><h2>🔄 Rebooting...</h2><div class="spinner"></div></div>';
        fetch('/reboot');
    }
}

function rebootDevice() {
    if (confirm('🔄 Are you sure you want to reboot?')) {
        document.body.innerHTML = '<div style="text-align:center; margin-top:100px;"><h2>🔄 Rebooting...</h2><div class="spinner"></div></div>';
        fetch('/reboot');
    }
}

window.onload = function() {
    setTimeout(liveMeasurement, 500);
    toggleApnCustom();
};
</script>
</body>
</html>
)rawliteral";
    
    return html;
}

// ============================================================================================
// ROOT HANDLER
// ============================================================================================
void handleRoot() {
    server.send(200, "text/html", getHTML());
}

// ============================================================================================
// SETUP ACCESS POINT WITH mDNS & CAPTIVE PORTAL
// ============================================================================================
void setupAP() {
    // WiFi.softAP("HoneyEsp_Config");
    
    dnsServer.start(53, "*", WiFi.softAPIP());
    
    Serial.println("\n╔══════════════════════════════════════════╗");
    Serial.println("║        🌐 WEB SERVER STARTED             ║");
    Serial.println("╚══════════════════════════════════════════╝");
    Serial.println("[WEB] 📡 AP SSID: HoneyEsp_Config");
    Serial.printf("[WEB] 🌍 IP: %s\n", WiFi.softAPIP().toString().c_str());
    
    #ifdef ESP32
        if (MDNS.begin("honeyesp")) {
            MDNS.addService("http", "tcp", 80);
            Serial.println("[WEB] 🔗 mDNS: http://honeyesp.local");
        } else {
            Serial.println("[WEB] ⚠️ mDNS failed to start");
        }
    #endif
    
    Serial.println("[WEB] 🎯 Captive Portal: ANY domain works");
    Serial.println("╚══════════════════════════════════════════╝\n");
    
    server.on("/", handleRoot);
    server.on("/save", handleSave);
    server.on("/reboot", handleReboot);
    server.on("/live", handleLiveMeasurement);
    server.on("/tare", handleTare);
    server.on("/calibrate", handleCalibrate);
    server.on("/status", handleStatus);
    server.on("/scan", handleScan);
    server.on("/sleep", handleSleep);
    server.on("/battery", handleBattery);
    server.on("/network", handleNetworkInfo);
    
    server.begin();
    Serial.println("[WEB] ✅ HTTP server ready!\n");
}

// ============================================================================================
// HANDLE CLIENT - ΑΥΤΗ ΠΡΕΠΕΙ ΝΑ ΕΙΝΑΙ ΕΞΩ ΑΠΟ ΤΗΝ setupAP
// ============================================================================================
void handleClient() {
    dnsServer.processNextRequest();
    server.handleClient();
}
