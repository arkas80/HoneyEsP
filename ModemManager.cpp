/*
 * ============================================================================================
 * ModemManager.cpp - HoneyEsp 4G/NB-IoT Modem Control
 * Version: 0.7.0 - Enhanced initialization with power cycling
 * ============================================================================================
 */

#include "ModemManager.h"
#include "Config.h"
#include <HardwareSerial.h>

// ========================================================================
// GLOBAL OBJECTS
// ========================================================================
HardwareSerial SerialAT(1);  // UART1
TinyGsm modem(SerialAT);
TinyGsmClient client(modem);

// External variables (from WebConfig)
extern String simApn;
extern bool modemUseNbIot;
extern String tsApiKey;

// ========================================================================
// MODEM POWER CYCLE - Complete restart with timing
// ========================================================================
void modemPowerCycle() {
    Serial.println("[MODEM] 🔄 Starting power cycle...");
    
    // Ensure PWRKEY is defined
    pinMode(MODEM_PWRKEY, OUTPUT);
    
    // Power down sequence
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(1000);
    
    // Power up sequence
    Serial.println("[MODEM]   Sending PWRKEY pulse (HIGH 500ms)...");
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(500);
    digitalWrite(MODEM_PWRKEY, LOW);
    
    Serial.println("[MODEM]   Waiting for modem to boot (3 seconds)...");
    delay(3000);
    
    Serial.println("[MODEM] ✅ Power cycle complete");
}

// ========================================================================
// INIT MODEM WITH POWER CYCLE & RETRY
// ========================================================================
bool initModem() {
    Serial.println("\n[MODEM] 🚀 Initializing modem...");

    // Step 1: Power cycle the modem
    modemPowerCycle();

    // Step 2: Initialize UART
    Serial.println("[MODEM] 📡 Starting UART communication...");
    Serial.printf("[MODEM]    TX: GPIO%d, RX: GPIO%d, Baud: 115200\n", MODEM_TX, MODEM_RX);
    
    SerialAT.end();  // Ensure clean start
    delay(100);
    
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(1000);  // Give UART time to stabilize

    // Step 3: Sync baud rate - AGGRESSIVE approach
    Serial.println("[MODEM] 🔄 Syncing baud rate (5 attempts)...");
    
    bool atOK = false;
    for (int attempt = 0; attempt < 5; attempt++) {
        Serial.printf("[MODEM]    Attempt %d...", attempt + 1);
        
        // Send AT multiple times
        for (int i = 0; i < 3; i++) {
            SerialAT.println("AT");
            delay(100);
        }
        
        // Read response
        delay(500);
        String response = "";
        unsigned long timeout = millis();
        while (SerialAT.available() && millis() - timeout < 1000) {
            response += (char)SerialAT.read();
            delay(10);
        }
        
        if (response.indexOf("OK") >= 0) {
            Serial.println(" ✅ OK");
            atOK = true;
            break;
        } else {
            Serial.printf(" ❌ (got: %s)\n", response.c_str());
        }
        
        delay(500);
    }

    if (!atOK) {
        Serial.println("[MODEM] ❌ Could not sync baud rate!");
        return false;
    }

    // Step 4: Test with TinyGsm
    Serial.println("[MODEM] 🧪 Testing with AT command...");
    modem.sendAT("AT");
    int result = modem.waitResponse(2000);
    
    Serial.printf("[MODEM]    Result: %d (1=OK, 0=TIMEOUT, -1=ERROR)\n", result);
    
    if (result != 1) {
        Serial.println("[MODEM] ❌ TinyGsm AT test failed!");
        return false;
    }
    
    Serial.println("[MODEM] ✅ AT test passed!");

    // Step 5: Restart modem
    Serial.println("[MODEM] 🔄 Restarting modem...");
    
    if (!modem.restart()) {
        Serial.println("[MODEM] ⚠️ Restart command failed (may still work)");
    } else {
        Serial.println("[MODEM] ✅ Modem restarted");
    }
    
    delay(3000);

    // Step 6: Get modem model
    String model = modem.getModemName();
    Serial.println("[MODEM] 📱 Model: " + model);

    // Step 7: Configure based on modem type
    if (model.indexOf("7080") != -1) {
        Serial.println("[MODEM] ✅ Detected SIM7080");

        if (modemUseNbIot) {
            Serial.println("[MODEM]    Setting NB-IoT mode...");
            modem.setNetworkMode(28);  // NB-IoT only
            delay(500);
        } else {
            Serial.println("[MODEM]    Setting AUTO mode (4G/LTE)...");
            modem.setNetworkMode(1);   // Auto
            delay(500);
        }

        // GPS OFF for power saving
        modem.sendAT("+CGPS=0");
        delay(100);
        
    } else if (model.indexOf("7600") != -1) {
        Serial.println("[MODEM] ✅ Detected SIM7080");
        modem.setNetworkMode(1);  // Auto mode
        delay(500);
    } else {
        Serial.println("[MODEM] ⚠️ Unknown modem, continuing...");
    }

    // Step 8: Wait for network registration
    Serial.println("[MODEM] 🌐 Waiting for network registration...");
    Serial.println("[MODEM]    (this may take 30-60 seconds, be patient...)");
    
    if (!modem.waitForNetwork(60000, true)) {
        Serial.println("[MODEM] ❌ Network registration FAILED");
        Serial.println("[MODEM]    Check: SIM card, antenna, signal coverage");
        return false;
    }

    Serial.println("[MODEM] ✅ Network registered!");
    
    String operatorName = modem.getOperator();
    Serial.println("[MODEM]    Operator: " + operatorName);
    
    return true;
}

// ========================================================================
// INIT MODEM WITH RETRY
// ========================================================================
bool initModemWithRetry(int maxRetries) {
    int attempt = 0;
    bool success = false;
    
    while (attempt < maxRetries && !success) {
        Serial.printf("\n[MODEM] ═══ INITIALIZATION ATTEMPT %d/%d ═══\n", attempt + 1, maxRetries);
        success = initModem();
        
        if (!success) {
            attempt++;
            if (attempt < maxRetries) {
                Serial.printf("[MODEM] ⏳ Retrying in 10 seconds...\n");
                delay(10000);
            }
        }
    }
    
    Serial.println("\n[MODEM] ═══════════════════════════════════════════");
    if (success) {
        Serial.println("[MODEM] ✅ MODEM INITIALIZED SUCCESSFULLY!");
    } else {
        Serial.println("[MODEM] ❌ MODEM INITIALIZATION FAILED AFTER ALL RETRIES");
    }
    Serial.println("[MODEM] ═══════════════════════════════════════════\n");
    
    return success;
}

// ========================================================================
// GET MODEM RSSI (Signal Quality in dBm)
// ========================================================================
int getModemRSSI() {
    int rssi = modem.getSignalQuality();
    
    // Convert CSQ (0-31) to dBm
    int rssi_dBm = -113;
    if (rssi == 0) rssi_dBm = -115;
    else if (rssi == 1) rssi_dBm = -111;
    else if (rssi == 2) rssi_dBm = -107;
    else if (rssi == 3) rssi_dBm = -103;
    else if (rssi == 4) rssi_dBm = -99;
    else if (rssi == 5) rssi_dBm = -95;
    else if (rssi == 6) rssi_dBm = -91;
    else if (rssi == 7) rssi_dBm = -87;
    else if (rssi == 8) rssi_dBm = -83;
    else if (rssi == 9) rssi_dBm = -79;
    else if (rssi == 10) rssi_dBm = -75;
    else if (rssi == 11) rssi_dBm = -71;
    else if (rssi == 12) rssi_dBm = -67;
    else if (rssi == 13) rssi_dBm = -63;
    else if (rssi == 14) rssi_dBm = -59;
    else if (rssi == 15) rssi_dBm = -55;
    else if (rssi == 16) rssi_dBm = -51;
    else if (rssi >= 17 && rssi <= 30) rssi_dBm = -47 + (rssi - 17);
    else if (rssi == 31) rssi_dBm = -25;
    
    Serial.printf("[MODEM] Signal: CSQ=%d -> %d dBm\n", rssi, rssi_dBm);
    return rssi_dBm;
}

// ========================================================================
// GET MODEM CSQ (Raw 0-31)
// ========================================================================
int getModemCSQ() {
    int csq = modem.getSignalQuality();
    Serial.printf("[MODEM] Raw CSQ: %d (0-31)\n", csq);
    return csq;
}

// ========================================================================
// SEND DATA VIA 4G (with RSSI)
// ========================================================================
bool sendData4G(float weight, float temp, float humidity, float hiveTemp, 
                float voltage, float batteryPercent, int rssi) {
    
    Serial.println("[4G] 📡 Connecting GPRS...");
    Serial.printf("[4G]    APN: %s\n", simApn.c_str());
    
    if (!modem.gprsConnect(simApn.c_str(), "", "")) {
        Serial.println("[4G] ❌ GPRS connection failed");
        return false;
    }
    
    Serial.println("[4G] ✅ GPRS connected");

    float weight_kg = weight / 1000.0;

    String url = "/update?api_key=" + tsApiKey;
    url += "&field1=" + String(weight_kg, 2);
    url += "&field2=" + String(temp, 1);
    url += "&field3=" + String(humidity, 1);
    url += "&field4=" + String(hiveTemp, 1);
    url += "&field5=" + String(voltage, 2);
    url += "&field6=" + String(batteryPercent, 0);
    url += "&field7=" + String(rssi);

    Serial.println("\n[4G] 📊 Sending to ThingSpeak:");
    Serial.printf("     Field1: %.2f kg\n", weight_kg);
    Serial.printf("     Field2: %.1f °C (Air)\n", temp);
    Serial.printf("     Field3: %.1f %% (Humidity)\n", humidity);
    Serial.printf("     Field4: %.1f °C (Hive)\n", hiveTemp);
    Serial.printf("     Field5: %.2f V\n", voltage);
    Serial.printf("     Field6: %.0f %%\n", batteryPercent);
    Serial.printf("     Field7: %d dBm\n\n", rssi);
    
    if (client.connect("api.thingspeak.com", 80)) {
        client.print(String("GET ") + url + " HTTP/1.1\r\n");
        client.print("Host: api.thingspeak.com\r\n");
        client.print("Connection: close\r\n\r\n");
        
        unsigned long timeout = millis();
        bool success = false;
        
        while (client.connected() && millis() - timeout < 8000) {
            if (client.available()) {
                String response = client.readStringUntil('\n');
                Serial.printf("[4G]    Response: %s\n", response.c_str());
                
                if (response.indexOf("200") >= 0 || 
                    (response.length() > 0 && response.length() < 10 && isDigit(response.charAt(0)))) {
                    success = true;
                }
                timeout = millis();
            }
            delay(10);
        }

        client.stop();
        modem.gprsDisconnect();
        
        if (success) {
            Serial.printf("[4G] ✅ Data sent! (%.2f kg, RSSI: %d dBm)\n", weight_kg, rssi);
            return true;
        }
    }

    modem.gprsDisconnect();
    Serial.println("[4G] ❌ Failed to send data");
    return false;
}

// ========================================================================
// SEND DATA VIA 4G (auto RSSI)
// ========================================================================
bool sendData4G(float weight, float temp, float humidity, float hiveTemp, 
                float voltage, float batteryPercent) {
    int rssi = getModemRSSI();
    return sendData4G(weight, temp, humidity, hiveTemp, voltage, batteryPercent, rssi);
}

// ========================================================================
// SEND DATA VIA 4G (GPS variant)
// ========================================================================
bool sendData4G(float weight, float temp, float humidity, float voltage, float batteryPercent,
                float hiveTemp, float latitude, float longitude, bool alarm) {
    return sendData4G(weight, temp, humidity, hiveTemp, voltage, batteryPercent);
}

// ========================================================================
// SEND DATA VIA 4G (Custom APN)
// ========================================================================
bool sendData4G(String apn, String user, String pass, float weight, float temp, 
                float humidity, float hiveTemp, float voltage, float batteryPercent) {
    
    Serial.println("[4G] Connecting with custom APN: " + apn);
    
    if (!modem.gprsConnect(apn.c_str(), user.c_str(), pass.c_str())) {
        Serial.println("[4G] ❌ Custom GPRS failed");
        return false;
    }
    
    int rssi = getModemRSSI();
    return sendData4G(weight, temp, humidity, hiveTemp, voltage, batteryPercent, rssi);
}

// ========================================================================
// POWER OFF MODEM
// ========================================================================
void powerOffModem() {
    Serial.println("[MODEM] 💤 Powering off...");
    modem.poweroff();
    delay(500);
}

// ========================================================================
// MODEM DEEP SLEEP
// ========================================================================
void modemDeepSleep() {
    Serial.println("[MODEM] 💤 Deep sleep mode...");
    modem.sendAT("+CSCLK=2");
    delay(100);
}

// ========================================================================
// WAKE MODEM
// ========================================================================
void wakeModem() {
    Serial.println("[MODEM] ⏰ Waking up...");
    pinMode(MODEM_PWRKEY, OUTPUT);
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(100);
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(1000);
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(2000);
}

// ========================================================================
// CHECK MODEM STATUS
// ========================================================================
bool isModemAlive() {
    modem.sendAT("AT");
    int result = modem.waitResponse(1000);
    
    if (result == 1) {
        Serial.println("[MODEM] ✅ Alive");
        return true;
    } else {
        Serial.printf("[MODEM] ❌ No response (result=%d)\n", result);
        return false;
    }
}

// ========================================================================
// RESET MODEM (Hard reset)
// ========================================================================
void resetModem() {
    Serial.println("[MODEM] 🔄 Hard reset...");
    pinMode(MODEM_PWRKEY, OUTPUT);
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(2000);
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(1000);
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(1000);
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(2000);
}

// ========================================================================
// PRINT MODEM INFO
// ========================================================================
void printModemInfo() {
    Serial.println("\n╔═══════════════════════════════════════════════════════════════════════╗");
    Serial.println("║                         MODEM INFORMATION                             ║");
    Serial.println("╚═══════════════════════════════════════════════════════════════════════╝");
    Serial.print("  📱 Model: ");
    Serial.println(modem.getModemName());
    Serial.print("  🔢 IMEI: ");
    Serial.println(modem.getIMEI());
    Serial.print("  📡 Operator: ");
    Serial.println(modem.getOperator());
    Serial.print("  📶 Signal (CSQ): ");
    Serial.print(modem.getSignalQuality());
    Serial.println(" (0-31)");
    Serial.print("  📶 Signal (dBm): ");
    Serial.print(getModemRSSI());
    Serial.println(" dBm");
    Serial.print("  💳 SIM Card: ");
    Serial.println(isSimCardReady() ? "✅ Ready" : "❌ Not ready");
    Serial.print("  🌐 Network: ");
    Serial.println(modem.isNetworkConnected() ? "✅ Connected" : "❌ Disconnected");
    Serial.print("  🔗 GPRS: ");
    Serial.println(modem.isGprsConnected() ? "✅ Connected" : "❌ Disconnected");
    Serial.println("═══════════════════════════════════════════════════════════════════════\n");
}

// ========================================================================
// GET NETWORK OPERATOR
// ========================================================================
String getNetworkOperator() {
    return modem.getOperator();
}

// ========================================================================
// CHECK SIM CARD
// ========================================================================
bool isSimCardReady() {
    int simStatus = modem.getSimStatus();
    
    if (simStatus == 1) {
        Serial.println("[MODEM] ✅ SIM Ready");
        return true;
    } else {
        Serial.printf("[MODEM] ❌ SIM status: %d\n", simStatus);
        return false;
    }
}

// ========================================================================
// INIT MODEM WITH APN (for WebConfig)
// ========================================================================
bool initModemWithAPN() {
    return initModemWithRetry(2);
}
