/*
 * ============================================================================================
 * WebConfig.h - HoneyEsp Web Configuration Server
 * Version: 0.4.6 - WITH APN PRESETS & CUSTOM ENTRY
 * ============================================================================================
 */

#ifndef WEBCONFIG_H
#define WEBCONFIG_H

#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>

extern WebServer server;
extern DNSServer dnsServer;
extern Preferences prefs;

extern String wifiSSID;
extern String wifiPass;
extern String tsApiKey;
extern String waPhone;
extern String waApiKey;
extern String simApn;
extern uint32_t sleepTimeSec;
extern float scaleRatio;

void loadSettings();
void saveSettings();
void setupAP();
void handleClient();
void handleRoot();
String getHTML();
void handleLiveMeasurement();
void handleTare();
void handleCalibrate();
void handleStatus();
void handleScan();
void handleSleep();
void handleBattery();
void handleSave();
void handleReboot();
void performWiFiScan();

// Νέα function για APN presets
String getApnPresetName(String apnValue);

#endif
