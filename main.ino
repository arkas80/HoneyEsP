#include <WiFi.h>
#include <Wire.h>
#include "XPowersLib.h"
#include "Config.h"
#include "Sensors.h"
#include "DisplayManager.h"
#include "Communications.h"
#include "ModemManager.h"
#include "WebConfig.h"
#include "StatusLED.h"

// ============================================================
// ΕΝΕΡΓΟΠΟΙΗΣΗ DEBUG ΓΙΑ ΜΠΑΤΑΡΙΑ (προαιρετικό)
// ============================================================
#define ENABLE_BATTERY_DEBUG 

// ============================================================================================
// EXTERNAL I2C BUS
// ============================================================================================
extern TwoWire I2C_Sensors;

// ============================================================================================
// PMU OBJECT - SINGLE INSTANCE
// ============================================================================================
XPowersAXP2101 PMU;

// ============================================================================================
// GLOBAL VARIABLES
// ============================================================================================
extern String wifiSSID;
extern String wifiPass;
extern String tsApiKey;
extern uint32_t sleepTimeSec;

bool debugMode = true;

RTC_DATA_ATTR float lastStoredWeight = 0;

// ========== GLOBAL VARIABLES FOR DISPLAY ==========
int lastKnownRSSI = -75;
String lastKnownNetwork = "---";
// ==================================================

// ============================================================================================
// DEBUG MACROS
// ============================================================================================
#define DEBUG_PRINT(x)    Serial.print(x)
#define DEBUG_PRINTLN(x)  Serial.println(x)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)

// ============================================================================================
// I2C SCANNER
// ============================================================================================
bool checkI2CDevice(uint8_t address) {
  Wire.beginTransmission(address);
  byte error = Wire.endTransmission();
  return (error == 0);
}

// ============================================================================================
// SETUP - COMPLETE FIXED VERSION
// ============================================================================================
void setup() {
  Serial.begin(115200);
  
  // ============================================================
  // STEP 0: Release all GPIO pins from deep sleep hold
  // ============================================================
  Serial.println("[BOOT] [0/8] Releasing GPIO pins from deep sleep hold...");
  
  gpio_hold_dis((gpio_num_t)TFT_BL);
  gpio_hold_dis((gpio_num_t)TFT_CS);
  gpio_hold_dis((gpio_num_t)TFT_DC);
  gpio_hold_dis((gpio_num_t)TFT_RST);
  gpio_hold_dis((gpio_num_t)TFT_MOSI);
  gpio_hold_dis((gpio_num_t)TFT_SCLK);
  gpio_hold_dis((gpio_num_t)PMU_SDA);
  gpio_hold_dis((gpio_num_t)PMU_SCL);
  gpio_hold_dis((gpio_num_t)SENSORS_SDA);
  gpio_hold_dis((gpio_num_t)SENSORS_SCL);
  
  // End all buses
  SPI.end();
  Wire.end();
  if (&I2C_Sensors != nullptr) I2C_Sensors.end();
  delay(100);
  
  Serial.println("      ✅ All pins released\n");

  // ============================================================
  // STEP 0.5: Reset WiFi state
  // ============================================================
  Serial.println("[BOOT] [0.5/8] Resetting WiFi state...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);

  // ============================================================
  // Check wake reason (important for modem power management)
  // ============================================================
  if (esp_reset_reason() == ESP_RST_DEEPSLEEP) {
    Serial.println("[BOOT] Woke from deep sleep - will power on modem");
    delay(2500);
  }

  unsigned long startAttempt = millis();
  while (!Serial && (millis() - startAttempt) < 5000) {
    delay(10);
  }

  delay(200);
  Serial.println("\n[USB CONNECTED]\n");

  // ============================================================
  // Check reset reason
  // ============================================================
  esp_reset_reason_t reason = esp_reset_reason();
  Serial.printf("[BOOT] Reset reason: %d - ", reason);
  switch(reason) {
    case 1: Serial.println("POWER_ON_RESET"); break;
    case 2: Serial.println("EXT_RESET"); break;
    case 3: Serial.println("SW_RESET"); break;
    case 4: Serial.println("DEEP_SLEEP_RESET"); break;
    case 5: Serial.println("SDIO_RESET"); break;
    default: Serial.println("OTHER"); break;
  }

  Serial.println("\n");
  Serial.println("╔═══════════════════════════════════════════════════════════════════════╗");
  Serial.print("║                    HoneyEsp  v");
  Serial.print(VERSION);
  Serial.println(" - Smart Hive Monitor                  ║");
  Serial.println("║                         ESP32-S3 with 4G + WiFi                      ║");
  Serial.println("╚═══════════════════════════════════════════════════════════════════════╝\n");

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW);

  // ============================================================
  // STEP 1: I2C Initialization
  // ============================================================
  Serial.println("[BOOT] [1/8] Initializing I2C buses...");
  
  Wire.begin(PMU_SDA, PMU_SCL);
  Serial.printf("      PMU I2C: SDA=%d, SCL=%d\n", PMU_SDA, PMU_SCL);

  I2C_Sensors.begin(SENSORS_SDA, SENSORS_SCL);
  I2C_Sensors.setClock(100000);
  Serial.printf("      Sensors I2C: SDA=%d, SCL=%d\n\n", SENSORS_SDA, SENSORS_SCL);

  // ============================================================
  // STEP 2: PMU Initialization - SINGLE INSTANCE
  // ============================================================
  Serial.println("[BOOT] [2/8] Initializing AXP2101 PMU...");
  
  if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL)) {
    Serial.println("      ⚠️ AXP2101 not found!");
  } else {
    Serial.println("      ✅ AXP2101 found!");
    
    // Enable power rails
    PMU.enableDC1();
    PMU.setDC3Voltage(3300);
    PMU.enableDC3();
    PMU.setALDO2Voltage(3300);
    PMU.enableALDO2();
    PMU.setBLDO1Voltage(3300);
    PMU.enableBLDO1();
    
    Serial.println("      ✅ PMU power rails enabled\n");

    Serial.println("🔋 Battery Status:");
    float batVolt = PMU.getBattVoltage();
    if (batVolt > 20) batVolt /= 1000.0;  // Convert mV to V if needed

    //Τάση συστήματος (VSYS)
    float sysVolt = PMU.getSystemVoltage();
    if (sysVolt > 20) sysVolt /= 1000.0;
    
    float vbusVolt = PMU.getVbusVoltage();
    if (vbusVolt > 20) vbusVolt /= 1000.0;
    
    Serial.printf("   Batt: %.2f V | Sys: %.2f V", batVolt, sysVolt);
    
    if (vbusVolt > 0.1) {
      Serial.printf(" | USB: %.2f V (Plugged in)\n", vbusVolt);
    } else {
      Serial.println(" | USB: 0.00 V (Battery only)");
    }
    Serial.println();
  }

  // ============================================================
  // STEP 2.5: Modem Power Sequence (if wake from sleep)
  // ============================================================
  if (reason == ESP_RST_DEEPSLEEP) {
    Serial.println("[BOOT] [2.5/8] Powering on modem...");
    
    // Modem is already powered off, we just need to pulse PWRKEY
    pinMode(MODEM_PWRKEY, OUTPUT);
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(500);      // 500ms HIGH
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(3000);     // Wait for modem to wake up
    
    Serial.println("      ✅ Modem power sequence complete\n");
  }

  // ============================================================
  // STEP 3: MAX17048 Detection
  // ============================================================
  Serial.println("[BOOT] [3/8] Checking for MAX17048...");
  if (checkI2CDevice(0x36)) {
    Serial.println("      ✅ MAX17048 found at 0x36\n");
  } else {
    Serial.println("      ℹ️ MAX17048 not found - using ADC\n");
  }

  // ============================================================
  // STEP 4: Status LED & Sensor Power
  // ============================================================
  Serial.println("[BOOT] [4/8] Initializing Status LED and Sensor Power...");
  initStatusLED();
  setStatusBoot();

  pinMode(SENSOR_POWER_PIN, OUTPUT);
  digitalWrite(SENSOR_POWER_PIN, HIGH);
  Serial.printf("      ✅ Sensor power ON (GPIO %d)\n\n", SENSOR_POWER_PIN);
  delay(100);

  // ============================================================
  // STEP 5: Load Settings from Flash
  // ============================================================
  Serial.println("[BOOT] [5/8] Loading settings from Flash...");
  loadSettings();
  Serial.printf("      ✅ WiFi SSID: %s\n", wifiSSID.c_str());
  Serial.printf("      ✅ Sleep time: %u seconds (%u min)\n\n", sleepTimeSec, sleepTimeSec/60);

  // ============================================================
  // STEP 6: Initialize Sensors
  // ============================================================
  Serial.println("[BOOT] [6/8] Initializing Sensors...");
  delay(500);
  initSensors();
  Serial.println("      ✅ Sensors ready\n");

  // ============================================================
  // STEP 7: Config Mode Check
  // ============================================================
  Serial.println("[BOOT] [7/8] Checking for Config Mode...");
  pinMode(CONFIG_PIN, INPUT_PULLUP);
  pinMode(CONFIG_LED, OUTPUT);
  digitalWrite(CONFIG_LED, LOW);

  initDisplay();
  
  if (reason == ESP_RST_DEEPSLEEP) {
    Serial.println("[DISPLAY] Forcing reset after deep sleep");
    forceDisplayReset();
  }
  
  drawBootLogo();

  tft.setTextSize(1);
  tft.setTextColor(ILI9341_CYAN);
  tft.setCursor(45, 210);
  tft.print("Hold GPIO0 for 3s to config");

  unsigned long start = millis();
  bool enterConfig = false;
  bool buttonPressed = false;

  Serial.println("      Press CONFIG button (GPIO0) within 5 seconds...");

  unsigned long lastBlink = millis();
  bool ledState = false;

  while (millis() - start < 5000) {
    if (millis() - lastBlink > 500) {
      ledState = !ledState;
      digitalWrite(CONFIG_LED, ledState ? HIGH : LOW);
      lastBlink = millis();
    }
    
    if (digitalRead(CONFIG_PIN) == LOW) {
      if (!buttonPressed) {
        Serial.println("      → Button pressed! Hold for 3 seconds...");
        buttonPressed = true;
        digitalWrite(CONFIG_LED, HIGH);
        
        tft.fillRect(45, 230, 230, 20, ILI9341_BLACK);
        tft.setCursor(50, 235);
        tft.setTextColor(ILI9341_ORANGE);
        tft.print("Hold for config...");
      }
      
      unsigned long pressStart = millis();
      while (digitalRead(CONFIG_PIN) == LOW) {
        if (millis() - lastBlink > 100) {
          ledState = !ledState;
          digitalWrite(CONFIG_LED, ledState ? HIGH : LOW);
          lastBlink = millis();
        }
        
        if (millis() - pressStart > 3000) {
          enterConfig = true;
          Serial.println("      → Entering CONFIG MODE...");
          break;
        }
        delay(10);
      }
      if (enterConfig) break;
    }
    delay(50);
  }

  if (!enterConfig) {
    Serial.println("      → Continuing to NORMAL MODE\n");
    tft.fillScreen(ILI9341_BLACK);
    digitalWrite(TFT_BL, LOW);
    
    for (int i = 0; i < 2; i++) {
      digitalWrite(CONFIG_LED, HIGH);
      delay(80);
      digitalWrite(CONFIG_LED, LOW);
      delay(80);
    }
    digitalWrite(CONFIG_LED, LOW);
  }

  // ========================================================================================
  // CONFIG MODE
  // ========================================================================================
  if (enterConfig) {
    Serial.println("\n╔═══════════════════════════════════════════════════════════════════════╗");
    Serial.println("║                         CONFIG MODE ACTIVE                           ║");
    Serial.println("╚═══════════════════════════════════════════════════════════════════════╝\n");

    setStatusConfig();

    digitalWrite(TFT_BL, HIGH);
    tft.fillScreen(ILI9341_BLACK);
    drawConfigModeScreen();

    for (int i = 0; i < 5; i++) {
      digitalWrite(CONFIG_LED, HIGH);
      delay(100);
      digitalWrite(CONFIG_LED, LOW);
      delay(100);
    }
    digitalWrite(CONFIG_LED, HIGH);

    Serial.println("   Access Point : HoneyEsp_Config");
    Serial.println("   IP Address   : 192.168.4.1");
    Serial.println("   URL          : http://192.168.4.1\n");

    setupAP();

    while (true) {
      handleClient();
      delay(10);
    }
  }

  // ========================================================================================
  // STEP 8: NORMAL MODE - READ SENSORS
  // ========================================================================================
  Serial.println("\n[BOOT] [8/8] Starting normal measurement cycle...\n");

  Serial.println("╔═══════════════════════════════════════════════════════════════════════╗");
  Serial.println("║                      READING SENSOR DATA                             ║");
  Serial.println("╚═══════════════════════════════════════════════════════════════════════╝\n");

  digitalWrite(TFT_BL, HIGH);

  DEBUG_PRINT(" 📊 Weight: ");
  float w = getWeight();
  DEBUG_PRINTF("%.2f g (%.2f kg)\n", w, w / 1000.0);

  DEBUG_PRINT(" 🌡️ BME280: ");
  float t, p, h;
  getBME(t, p, h);
  DEBUG_PRINTF("Temp=%.1f C, Press=%.1f hPa, Hum=%.1f %%\n", t, p, h);

  DEBUG_PRINT(" 🐝 DS18B20: ");
  float dsTemp = getDS();
  DEBUG_PRINTF("%.1f C\n", dsTemp);

    DEBUG_PRINT(" 🔋 Battery: ");
  float v = getBatteryVoltage();
  float batt = getBatteryPercent();
  
  // Επιπλέον debug για έλεγχο
  #ifdef ENABLE_BATTERY_DEBUG
    printBatteryDebug();
  #endif
  
  DEBUG_PRINTF("%.2f V (%.0f %%)\n\n", v, batt);

  if (batt < 15 && batt > 0) {
    Serial.printf("⚠️ LOW BATTERY! %.0f%% remaining\n", batt);
  }

  bool alarm = false;
  if (lastStoredWeight > 0) {
    float diff = abs(w - lastStoredWeight);
    if (diff > 2000) {
      alarm = true;
      DEBUG_PRINTF(" 🚨 ALARM! Weight changed by %.1f kg\n", diff / 1000.0);
    }
  }
  lastStoredWeight = w;

  // ========================================================================
  // SEND DATA - Primary 4G, Fallback WiFi
  // ========================================================================
  bool dataSent = sendDataWithFallback(w, t, h, v, batt, dsTemp, alarm);

  // ========================================================================
  // Display Results
  // ========================================================================
  if (!dataSent) {
    Serial.println("\n⚠️ WARNING: No data sent! Showing last values...");
  }

  Serial.printf("\n[DISPLAY] Network: %s | RSSI: %d dBm\n", lastKnownNetwork.c_str(), lastKnownRSSI);

  DEBUG_PRINTLN("\n 🖥️ Displaying measurements (10 seconds)...");
  showMeasurement(w, t, dsTemp, h, batt, lastKnownRSSI, alarm, lastKnownNetwork);
  delay(10000);

  turnOffDisplay();
  DEBUG_PRINTLN(" ✅ Display off\n");

  // Status blink before sleep
  DEBUG_PRINTLN("✨ System alive - White LED blink");
  for (int i = 0; i < 3; i++) {
    setRGB(255, 255, 255);
    delay(200);
    setRGB(0, 0, 0);
    delay(1800);
  }

  setStatusSleep();

  // ========================================================================================
  // DEEP SLEEP PREPARATION
  // ========================================================================================
  Serial.println("\n╔═══════════════════════════════════════════════════════════════════════╗");
  Serial.println("║                    PREPARING FOR DEEP SLEEP                          ║");
  Serial.println("╚═══════════════════════════════════════════════════════════════════════╝\n");

  // Hold GPIO pins for deep sleep
  gpio_hold_en((gpio_num_t)TFT_BL);
  gpio_hold_en((gpio_num_t)TFT_CS);
  gpio_hold_en((gpio_num_t)TFT_DC);
  gpio_hold_en((gpio_num_t)TFT_RST);

  // End SPI
  SPI.end();

  // Power down modem BEFORE deep sleep
  Serial.println("[MODEM] Powering down modem before sleep...");
  PMU.disableDC3();  // Modem power off
  delay(500);

  // Disable other rails
  PMU.disableALDO2();
  delay(100);

  Serial.printf("\n💤 Sleeping for %u seconds (%u min)\n", sleepTimeSec, sleepTimeSec / 60);
  Serial.println("════════════════════════════════════════════════════════════════════════\n");

  Serial.flush();

  esp_sleep_enable_timer_wakeup((uint64_t)sleepTimeSec * 1000000ULL);
  esp_deep_sleep_start();
}

void loop() {
    delay(1000);
}
