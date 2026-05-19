#ifndef MODEM_MANAGER_H
#define MODEM_MANAGER_H

#include <Arduino.h>

// ========================================================================
// MODEM SELECTION - Flexible support for SIM7080 & SIM7600
// ========================================================================

// Default to SIM7080 if nothing is defined
#ifndef MODEM_TYPE
    #define MODEM_TYPE 1  // 1 = SIM7080, 2 = SIM7600
#endif

#if MODEM_TYPE == 1
    #define TINY_GSM_MODEM_SIM7080
#elif MODEM_TYPE == 2
    #define TINY_GSM_MODEM_SIM7600
#else
    #define TINY_GSM_MODEM_SIM7080  // Default fallback
#endif

#include <TinyGsmClient.h>

// ========================================================================
// EXTERNAL DECLARATIONS
// ========================================================================

extern HardwareSerial SerialAT;
extern TinyGsm modem;
extern TinyGsmClient client;

// ========================================================================
// FUNCTIONS
// ========================================================================

// Initialization
bool initModem();
bool initModemWithRetry(int maxRetries = 3);


bool initModemWithAPN();
bool configureModemWithAPN();

// Signal Quality
int getModemRSSI();     // Returns dBm
int getModemCSQ();      // Returns raw CSQ (0-31)

// Data sending (6 parameters - auto RSSI)
bool sendData4G(float weight, float temp, float humidity, float hiveTemp, float voltage, float batteryPercent);

// Data sending (7 parameters - WITH RSSI parameter) ⭐ ΝΕΟ
bool sendData4G(float weight, float temp, float humidity, float hiveTemp, float voltage, float batteryPercent, int rssi);

// Data sending (9 parameters - with GPS & alarm)
bool sendData4G(float weight, float temp, float humidity, float voltage, float batteryPercent,
                float hiveTemp, float latitude, float longitude, bool alarm);

// Data sending with custom APN ⭐ ΝΕΟ
bool sendData4G(String apn, String user, String pass, float weight, float temp, float humidity, 
                float hiveTemp, float voltage, float batteryPercent);

// Power management
void powerOffModem();
void modemDeepSleep();
void wakeModem();       // ⭐ ΝΕΟ - Ξύπνημα από deep sleep
void resetModem();      // ⭐ ΝΕΟ - Hard reset

// Status checks ⭐ ΝΕΕΣ
bool isModemAlive();
bool isSimCardReady();
String getNetworkOperator();

// Debug
void printModemInfo();

#endif
