#include "SPIFFS_Manager.h"

void SPIFFS_Manager::begin() {
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
    } else {
        Serial.println("SPIFFS Mount Success");
    }
}

void SPIFFS_Manager::saveConfig(float scale, long offset) {
    StaticJsonDocument<256> doc;
    doc["scale"] = scale;
    doc["offset"] = offset;
    
    File file = SPIFFS.open(configFile, "w");
    if (!file) {
        Serial.println("Failed to open config file for writing");
        return;
    }
    
    serializeJson(doc, file);
    file.close();
    Serial.println("Config saved");
}

void SPIFFS_Manager::loadConfig(float& scale, long& offset) {
    File file = SPIFFS.open(configFile, "r");
    if (!file) {
        Serial.println("Config file not found");
        return;
    }
    
    StaticJsonDocument<256> doc;
    deserializeJson(doc, file);
    file.close();
    
    scale = doc["scale"] | 2280.0f;
    offset = doc["offset"] | 0;
}

void SPIFFS_Manager::saveSettings(String ssid, String password, String apiKey) {
    StaticJsonDocument<512> doc;
    doc["ssid"] = ssid;
    doc["password"] = password;
    doc["apiKey"] = apiKey;
    
    File file = SPIFFS.open(settingsFile, "w");
    if (!file) {
        Serial.println("Failed to open settings file");
        return;
    }
    
    serializeJson(doc, file);
    file.close();
}

void SPIFFS_Manager::loadSettings(String& ssid, String& password, String& apiKey) {
    File file = SPIFFS.open(settingsFile, "r");
    if (!file) {
        Serial.println("Settings file not found");
        return;
    }
    
    StaticJsonDocument<512> doc;
    deserializeJson(doc, file);
    file.close();
    
    ssid = doc["ssid"] | "";
    password = doc["password"] | "";
    apiKey = doc["apiKey"] | "";
}

void SPIFFS_Manager::saveSchedules(void* schedules, int count) {
    StaticJsonDocument<1024> doc;
    JsonArray array = doc.createNestedArray("schedules");
    
    // Add schedule data to array
    
    File file = SPIFFS.open(schedulesFile, "w");
    if (!file) return;
    
    serializeJson(doc, file);
    file.close();
}

void SPIFFS_Manager::loadSchedules(void* schedules, int& count) {
    File file = SPIFFS.open(schedulesFile, "r");
    if (!file) return;
    
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, file);
    file.close();
    
    // Load schedule data from JSON
}