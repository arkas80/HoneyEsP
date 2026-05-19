#include "Sensors.h"
#include "Config.h"
#include <Wire.h>
#include "HX711.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include <Adafruit_MAX1704X.h>
#include <Adafruit_BME280.h>
#include "XPowersLib.h"

// ============================================================
// SECOND I2C BUS (Για εξωτερικά modules: BME, OLED, MAX)
// ============================================================
TwoWire I2C_Sensors = TwoWire(1);

// ============================================================
// EXTERNAL PMU DECLARATION (ορίζεται στο main)
// ============================================================
extern XPowersAXP2101 PMU;

// ============================================================
// OBJECTS
// ============================================================

HX711 scale;
Adafruit_BME280 bme;
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature ds(&oneWire);
Adafruit_MAX17048 maxlipo;
Preferences calPrefs;

// ============================================================
// FLAGS (Ορατά σε όλο το project)
// ============================================================
bool bmeOK = false;
bool dsOK = false;
bool hx711OK = false;
bool hasMAX17048 = false;

// ============================================================
// LOCAL FLAGS
// ============================================================
bool batteryCheckDone = false;
bool batteryActuallyPresent = true;

// ============================================================
// VARIABLES
// ============================================================

long offset = 0;
float scaleFactor = 1.0;
float smoothWeight = 0;
bool invertSignal = true;
bool isWarming = false;

extern bool debugMode;

// ============================================================
// INIT
// ============================================================

void initSensors() {
  Serial.println("\n=== SENSORS INIT ===");

  // 1. Εσωτερικό Bus (PMU)
  Wire.begin(PMU_SDA, PMU_SCL);

  // 2. Εξωτερικό Bus (BME, OLED, MAX)
  I2C_Sensors.begin(SENSORS_SDA, SENSORS_SCL);
  I2C_Sensors.setClock(100000);

  // 3. DS18B20
  ds.begin();
  if (ds.getDeviceCount() > 0) {
    dsOK = true;
    Serial.println("✅ DS18B20 initialized");
  } else {
    dsOK = false;
    Serial.println("⚠️ DS18B20 not found");
  }

  // 4. BME280 (προαιρετικό)
  if (!bme.begin(0x76, &I2C_Sensors)) {
    if (!bme.begin(0x77, &I2C_Sensors)) {
      Serial.println("⚠️ BME280 not found");
      bmeOK = false;
    } else {
      Serial.println("✅ BME280 found at 0x77");
      bmeOK = true;
    }
  } else {
    Serial.println("✅ BME280 found at 0x76");
    bmeOK = true;
  }

  // 5. MAX17048 (ΕΛΕΓΧΟΣ - Θα υπάρχει σε κάποιες πλακέτες)
  if (!maxlipo.begin(&I2C_Sensors)) {
    Serial.println("⚠️ MAX17048 not found - using AXP2101 for battery");
    hasMAX17048 = false;
  } else {
    Serial.println("✅ MAX17048 found - using for battery monitoring");
    hasMAX17048 = true;
  }

  // 6. HX711
  scale.begin(HX711_DOUT, HX711_SCK);
  delay(300);
  
  if (scale.is_ready()) {
    hx711OK = true;
    Serial.println("✅ HX711 initialized");
  } else {
    hx711OK = false;
    Serial.println("⚠️ HX711 not found");
  }

  // Load calibration
  calPrefs.begin("cal", true);
  offset = calPrefs.getLong("offset", 0);
  scaleFactor = calPrefs.getFloat("scale", 1.0);
  invertSignal = calPrefs.getBool("invert", false);
  calPrefs.end();

  scale.set_offset(offset);
  scale.set_scale(scaleFactor);

  smoothWeight = 0;

  Serial.printf("Offset: %ld | Scale: %.6f | Invert: %s\n", offset, scaleFactor, invertSignal ? "YES" : "NO");

  // ✅ WARM-UP
  Serial.println("🔥 Warming up scale...");
  isWarming = true;
  for (int i = 0; i < 30; i++) {
    getWeight();
    delay(50);
  }
  isWarming = false;
  Serial.println("✅ Scale warmed up!");

  // Εκτύπωση κατάστασης μπαταρίας
  printBatteryDebug();

  Serial.println("=== READY ===\n");
}

// ============================================================
// GET WEIGHT (με invert applied correctly)
// ============================================================

float getWeight() {
  if (!scale.is_ready()) return smoothWeight;

  float rawWeight = scale.get_robust_units(10);
  float absWeight = fabs(rawWeight);
  float weight = absWeight;

  if (!debugMode) {
    if (isWarming) {
      smoothWeight = smoothWeight * 0.5 + weight * 0.5;
    } else {
      smoothWeight = smoothWeight * 0.8 + weight * 0.2;
    }
  } else {
    smoothWeight = weight;
  }

  if (fabs(smoothWeight) < 2) smoothWeight = 0;

  Serial.printf("🔍 DEBUG - RAW: %.2f | ABS: %.2f | SCALE: %.2f | FINAL: %.2f\n",
                rawWeight, absWeight, scaleFactor, smoothWeight);

  return smoothWeight;
}

// ============================================================
// TARE (RAW BASED)
// ============================================================

void tareScale() {
  Serial.println("\n=== ⚖️ TARE START ===");
  Serial.println("REMOVE ALL WEIGHT!");
  delay(3000);

  if (!scale.is_ready()) {
    Serial.println("❌ HX711 not ready");
    return;
  }

  long rawOffset = scale.read_average(40);

  scale.set_offset(rawOffset);
  offset = rawOffset;

  calPrefs.begin("cal", false);
  calPrefs.putLong("offset", offset);
  calPrefs.end();

  smoothWeight = 0;

  Serial.printf("✅ Tare OK | Offset: %ld\n", offset);
  Serial.println("=====================\n");
}

// ============================================================
// BME280 (επιστρέφει default αν δεν υπάρχει)
// ============================================================

void getBME(float &t, float &p, float &h) {
  if (!bmeOK) {
    // Mock values για testing όταν δεν υπάρχει BME
    t = 23.5;
    p = 1013.25;
    h = 55.0;
    return;
  }

  t = bme.readTemperature();
  h = bme.readHumidity();
  p = bme.readPressure() / 100.0;

  if (isnan(t)) t = 23.5;
  if (isnan(h)) h = 55.0;
  if (isnan(p)) p = 1013.25;
}

// ============================================================
// DS18B20
// ============================================================

float getDS() {
  if (!dsOK) return 0.0;
  
  ds.requestTemperatures();
  delay(750);
  float temp = ds.getTempCByIndex(0);
  return (temp == DEVICE_DISCONNECTED_C) ? 0.0 : temp;
}

// ============================================================
// CHECK IF BATTERY IS PRESENT (ΑΥΤΟΜΑΤΟΣ ΕΛΕΓΧΟΣ)
// ============================================================

bool isBatteryPresent() {
  if (batteryCheckDone) return batteryActuallyPresent;
  
  Serial.println("\n[BATTERY DETECTION] Checking...");
  
  // Αν έχουμε MAX17048, σίγουρα υπάρχει μπαταρία
  if (hasMAX17048) {
    Serial.println("   ✅ MAX17048 present - battery assumed present");
    batteryActuallyPresent = true;
    batteryCheckDone = true;
    return true;
  }
  
  // Έλεγχος USB
  float vbus = PMU.getVbusVoltage();
  if (vbus > 20) vbus /= 1000.0;
  bool usbConnected = (vbus > 4.0);
  
  Serial.printf("   USB: %.2fV %s\n", vbus, usbConnected ? "Connected" : "Not connected");
  
  // Χωρίς USB, πρέπει να υπάρχει μπαταρία
  if (!usbConnected) {
    Serial.println("   ✅ No USB - battery must be present");
    batteryActuallyPresent = true;
    batteryCheckDone = true;
    return true;
  }
  
  // Με USB, δοκιμάζουμε την τάση
  float battVoltage = getBatteryVoltage();
  Serial.printf("   Voltage: %.3fV\n", battVoltage);
  
  // Αν η τάση είναι 4.2V ακριβώς με USB, μάλλον no battery
  if (battVoltage >= 4.18 && battVoltage <= 4.22) {
    Serial.println("   ❌ No battery (stable 4.2V with USB)");
    batteryActuallyPresent = false;
    batteryCheckDone = true;
    return false;
  }
  
  // Αν η τάση είναι 3.7V ακριβώς με USB, μάλλον no battery
  if (battVoltage > 3.65 && battVoltage < 3.75) {
    Serial.println("   ❌ No battery (stable 3.7V with USB)");
    batteryActuallyPresent = false;
    batteryCheckDone = true;
    return false;
  }
  
  Serial.println("   ✅ Battery detected");
  batteryActuallyPresent = true;
  batteryCheckDone = true;
  return true;
}

// ============================================================
// BATTERY VOLTAGE (ΥΠΟΣΤΗΡΙΖΕΙ ΚΑΙ ΤΑ ΔΥΟ)
// ============================================================

float getBatteryVoltage() {
  float voltage = 0.0;
  
  // 1. MAX17048 αν υπάρχει (πιο ακριβές)
  if (hasMAX17048) {
    voltage = maxlipo.cellVoltage();
    if (!isnan(voltage) && voltage > 0.1 && voltage < 5.0) {
      Serial.printf("[MAX17048] Voltage: %.3f V\n", voltage);
      return voltage;
    }
  }
  
  // 2. AXP2101
  float pmu_raw = PMU.getBattVoltage();
  
  if (pmu_raw > 20.0) {
    voltage = pmu_raw / 1000.0;
  } else if (pmu_raw > 0.1 && pmu_raw < 5.0) {
    voltage = pmu_raw;
  } else {
    voltage = 0.0;
  }
  
  Serial.printf("[AXP2101] Raw: %.0f mV -> %.3f V\n", pmu_raw, voltage);
  
  // Αν δεν υπάρχει μπαταρία, επέστρεψε 4.2V
  if (!isBatteryPresent()) {
    Serial.println("   No battery - returning 4.2V");
    return 4.2;
  }
  
  return voltage;
}

// ============================================================
// BATTERY PERCENTAGE (ΥΠΟΣΤΗΡΙΖΕΙ ΚΑΙ ΤΑ ΔΥΟ)
// ============================================================

float getBatteryPercent() {
  // 1. MAX17048 αν υπάρχει (πιο ακριβές)
  if (hasMAX17048) {
    float percent = maxlipo.cellPercent();
    if (!isnan(percent) && percent >= 0 && percent <= 100) {
      Serial.printf("[MAX17048] Percent: %.1f%%\n", percent);
      return percent;
    }
  }
  
  // 2. Υπολογισμός από τάση AXP2101
  float voltage = getBatteryVoltage();
  
  // Αν δεν υπάρχει μπαταρία
  if (!isBatteryPresent()) {
    Serial.println("[BATTERY] No battery - returning 100%");
    return 100.0;
  }
  
  if (voltage <= 0) return 0;
  if (voltage >= 4.2) return 100;
  if (voltage <= 3.0) return 0;
  
  // Βελτιωμένη καμπύλη LiPo
  float percent;
  if (voltage >= 3.7) {
    percent = 85.0 + (voltage - 3.7) / (4.2 - 3.7) * 15.0;
  } else if (voltage >= 3.5) {
    percent = 50.0 + (voltage - 3.5) / (3.7 - 3.5) * 35.0;
  } else {
    percent = (voltage - 3.0) / (3.5 - 3.0) * 50.0;
  }
  
  if (percent > 100) percent = 100;
  if (percent < 0) percent = 0;
  
  Serial.printf("[AXP2101] Voltage: %.3fV -> Percent: %.1f%%\n", voltage, percent);
  return percent;
}

// ============================================================
// DEBUG: ΕΚΤΥΠΩΣΗ ΚΑΤΑΣΤΑΣΗΣ ΜΠΑΤΑΡΙΑΣ
// ============================================================

void printBatteryDebug() {
  Serial.println("\n🔋 BATTERY DEBUG INFO:");
  Serial.printf("   MAX17048 present: %s\n", hasMAX17048 ? "YES" : "NO");
  
  float vbus = PMU.getVbusVoltage();
  if (vbus > 20) vbus /= 1000.0;
  Serial.printf("   USB Voltage: %.2f V %s\n", vbus, (vbus > 4.0) ? "(Connected)" : "(Not connected)");
  
  float battVoltage = getBatteryVoltage();
  float battPercent = getBatteryPercent();
  bool battPresent = isBatteryPresent();
  
  Serial.printf("   Battery present: %s\n", battPresent ? "YES" : "NO");
  Serial.printf("   Final Voltage: %.3f V\n", battVoltage);
  Serial.printf("   Final Percent: %.1f %%\n", battPercent);
  
  if (hasMAX17048) {
    Serial.printf("   MAX17048 Raw: %.1f%% / %.3fV\n", 
                  maxlipo.cellPercent(), maxlipo.cellVoltage());
  }
  
  float pmu_raw = PMU.getBattVoltage();
  Serial.printf("   AXP2101 Raw: %.0f mV\n", pmu_raw);
}

// ============================================================
// DEBUG HELPERS
// ============================================================

long getScaleOffset() {
  return scale.get_offset();
}

float getScaleFactor() {
  return scale.get_scale();
}

// ============================================================
// SET CALIBRATION FACTOR
// ============================================================

void setCalibration(float factor, bool invert) {
  Serial.println("\n=== 🔧 SET CALIBRATION ===");
  Serial.printf("📐 New scale factor: %.5f\n", factor);
  Serial.printf("🔄 Invert signal: %s\n", invert ? "YES" : "NO");

  if (factor < 0.0001 || factor > 50000) {
    Serial.println("❌ ERROR: Factor out of range!");
    return;
  }

  scaleFactor = factor;
  invertSignal = invert;
  scale.set_scale(scaleFactor);

  calPrefs.begin("cal", false);
  calPrefs.putFloat("scale", scaleFactor);
  calPrefs.putBool("invert", invertSignal);
  calPrefs.end();

  Serial.printf("✅ Scale factor saved: %.5f\n", scaleFactor);
  Serial.printf("✅ Invert saved: %s\n", invertSignal ? "YES" : "NO");
  Serial.println("==========================\n");
}

void setCalibration(float factor) {
  setCalibration(factor, false);
}
