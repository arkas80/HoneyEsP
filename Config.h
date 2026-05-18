#ifndef CONFIG_H
#define CONFIG_H

#define VERSION "0.5.0-FLEXIBLE-MODEM"

#include <Arduino.h>

// ==========================
// MODEM SELECTION
// ==========================
#ifndef MODEM_TYPE
    #define MODEM_TYPE 1  // Default: SIM7080 (NB-IoT/LTE-M)
#endif

// Modem APN default
#ifndef MODEM_APN_DEFAULT
    #define MODEM_APN_DEFAULT "internet"
#endif

// ==========================
// GLOBAL VARIABLES
// ==========================
extern bool debugMode;

// ==========================
// SENSOR POWER
// ==========================
#define SENSOR_POWER_PIN 45

// ==========================
// TFT DISPLAY
// ==========================
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   10
#define TFT_DC   11
#define TFT_RST  1
#define TFT_BL   2

// ==========================
// HX711 WEIGHT SENSOR
// ==========================
#define HX711_DOUT 9
#define HX711_SCK  8

// ==========================
// INTERNAL I2C (PMU AXP2101) - ΔΙΟΡΘΩΣΗ: 15 & 7 βάσει pinout
// ==========================
#define PMU_SDA 15
#define PMU_SCL 7

// ==========================
// EXTERNAL I2C (BME280, OLED, MAX17048)
// ==========================
#define SENSORS_SDA 48
#define SENSORS_SCL 47

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

// ==========================
// MAX17048
// ==========================
#define HAS_MAX17048

// ==========================
// ONE WIRE (DS18B20) - Σου δουλεύει στο 21, το κρατάμε
// ==========================
#define ONE_WIRE_PIN 21   // 16 

/*Σημείωση για το DS18B20 στο 21: Το άφησα ως έχει αφού αναφέρεις ότι δουλεύει. Αν στο μέλλον δεις "περίεργες" θερμοκρασίες όταν το modem στέλνει δεδομένα, είναι επειδή το pin 21 μοιράζεται τη γραμμή RI (Ring Indicator).*/

// ==========================
// BATTERY VOLTAGE (ADC fallback)
// ==========================
#define BAT_ADC_PIN 3

// ==========================
// MODEM PINS for LilyGo T-SIM7080G-S3 (ΑΠΟ ΤΟ GITHUB)
// ==========================
// Σύμφωνα με τον πίνακα:
// Modem | PWR  | RXD  | TXD  | RI   | DTR
// GPIO  | 41   | 4    | 5    | 3    | 42
#define MODEM_TX       5     //17   Εσωτερικό TX για το S3
#define MODEM_RX      4     //18    Εσωτερικό RX για το S3
#define MODEM_PWRKEY  41   // Power key από τη λίστα "SIM PIN"
#define MODEM_DTR     42   // DTR από τη λίστα "SIM PIN"
#define MODEM_RI      3    // RI από τη λίστα "SIM PIN"
#define MODEM_STATUS  -1   // Προαιρετικό

// ==========================
// CONFIG BUTTON & LEDS
// ==========================
#define CONFIG_PIN  0
#define CONFIG_LED  12
#define RGB_LED_PIN 46
#define LED2_PIN     4
// ==========================
// NETWORK & CLOUD DEFAULTS
// ==========================
#define WIFI_SSID_DEFAULT "Your_SSID"
#define WIFI_PASS_DEFAULT "Your_Password"

#define TS_SERVER "://thingspeak.com"
#define TS_API_KEY_DEFAULT "YOUR_CHANNEL_KEY"

#define WA_PHONE_DEFAULT "+3069XXXXXXXX"
#define WA_API_KEY_DEFAULT "123456"

// ==========================
// MODEM APN SETTINGS
// ==========================
#define MODEM_GPRS_USER_DEFAULT ""
#define MODEM_GPRS_PASS_DEFAULT ""

extern String modemApn;
extern String modemGprsUser;
extern String modemGprsPass;
extern bool modemUseNbIot;

// ==========================
// SLEEP TIMES
// ==========================
extern uint32_t sleepTimeSec;

#endif
