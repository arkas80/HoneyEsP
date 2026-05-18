#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_SSD1306.h>
#include "Config.h"

// OLED διαστάσεις (128x64)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C  // Συνήθης διεύθυνση (ή 0x3D)

// TFT Display functions
void initDisplay();
void forceDisplayReset();
void drawBattery(float batteryPercent, bool charging);  // ΑΛΛΑΓΗ: voltage -> batteryPercent
void drawSignal(int rssi);
void drawLogo();
void drawBootLogo();
void drawNetworkOperator(String operatorName, int rssi);

// ΝΕΕΣ overload functions για showMeasurement
void showMeasurement(float weight, float tempAir, float tempHive, float humidity, 
                     float batteryPercent, int rssi, bool alarm, String networkOperator);
void showMeasurement(float weight, float tempAir, float tempHive, float humidity, 
                     float batteryPercent, int rssi, bool alarm);  // Auto-detect network

void drawConfigModeScreen();
void turnOffDisplay();

// OLED Display functions
void initOLED();
void drawOLEDLogo();

// ΝΕΕΣ overload functions για OLED
void showOLEDMeasurement(float weight_kg, float tempAir, float tempHive, float humidity, 
                         float batteryPercent, int rssi, String networkOperator);
void showOLEDMeasurement(float weight_kg, float tempAir, float tempHive, float humidity, 
                         float batteryPercent, int rssi);  // Auto-detect network

void oledSleep();
void oledWake();

extern Adafruit_ILI9341 tft;
extern Adafruit_SSD1306 oled;

#endif
