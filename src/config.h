// config.h - HoneyEsP Configuration
// ESP32-S3 LILYGO T-SIM7080G-S3 pins and settings

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============= PIN DEFINITIONS =============
// Display (ILI9341 240x320)
#define TFT_CS      15
#define TFT_DC      2
#define TFT_RST     4
#define TFT_BL      18
#define TFT_MOSI    13
#define TFT_SCLK    12

// Sensors
#define HX711_DOUT      16  // Weight sensor data
#define HX711_SCK       17  // Weight sensor clock
#define I2C_SDA         21  // BME280
#define I2C_SCL         20
#define ONE_WIRE_PIN    5   // DS18B20 (optional)
#define BATTERY_ADC_PIN 3   // Battery monitoring

// System
#define SETUP_PIN       0   // Boot button
#define TEST_BUTTON_PIN 15  // Test button
#define WS2812_PIN      48  // LED (optional)

// ============= TIMING =============
#define SLEEP_SECONDS_DEFAULT 300
#define DISPLAY_ON_SECONDS 120
#define AP_TIMEOUT_MS (5 * 60 * 1000UL)

// ============= API KEYS & SETTINGS =============
#define THINGSPEAK_API_KEY "YOUR_API_KEY_HERE"
#define THINGSPEAK_CHANNEL_ID 0
#define THINGSPEAK_HOST "api.thingspeak.com"

#define WHATSAPP_ENABLED true
#define WHATSAPP_PHONE "+30XXXXXXXXXX"
#define WHATSAPP_API_URL "https://api.callmebot.com/whatsapp.php"

// ============= SENSOR CALIBRATION =============
#define HX711_DEFAULT_SCALE 2280.0f
#define HX711_GAIN 128
#define BME280_ADDRESS 0x76
#define BME280_SEA_LEVEL_PRESSURE 1013.25f

// ============= DEBUG =============
#define DEBUG_MODE true
#define DEBUG_SERIAL_SPEED 115200

// ============= FEATURES =============
#define ENABLE_WIFI true
#define ENABLE_THINGSPEAK true
#define ENABLE_WHATSAPP true
#define ENABLE_WEB_SERVER true
#define ENABLE_POWER_MANAGEMENT true
#define ENABLE_BATTERY_MONITORING true
#define ENABLE_DS18B20 false

#endif