#include "Config.h"
#include "DisplayManager.h"
#include <SPI.h>
#include <Wire.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>

// Χρήση του εξωτερικού I2C bus από το main
extern TwoWire I2C_Sensors; 

// Ορισμός αντικειμένων οθονών
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C_Sensors, -1);

// Για min/max υγρασίας
Preferences humPrefs;

// ============================================================================================
// TFT DISPLAY - INITIALIZATION & POWER
// ============================================================================================
void initDisplay() {
    gpio_hold_dis((gpio_num_t)TFT_BL);
    gpio_hold_dis((gpio_num_t)TFT_CS);
    gpio_hold_dis((gpio_num_t)TFT_DC);
    gpio_hold_dis((gpio_num_t)TFT_RST);
    gpio_hold_dis((gpio_num_t)TFT_MOSI);
    gpio_hold_dis((gpio_num_t)TFT_SCLK);
    
    SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
    
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    
    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(ILI9341_BLACK);
}

// ============================================================================================
// FORCE DISPLAY RESET - ΓΙΑ DEEP SLEEP WAKE
// ============================================================================================
void forceDisplayReset() {
    Serial.println("[DISPLAY] Force resetting display after deep sleep...");
    
    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RST, LOW);
    delay(10);
    digitalWrite(TFT_RST, HIGH);
    delay(120);
    
    SPI.end();
    delay(10);
    SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
    
    tft.begin();
    tft.setRotation(1);
    
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    
    Serial.println("[DISPLAY] Force reset complete");
}

void turnOffDisplay() {
    tft.fillScreen(ILI9341_BLACK);
    tft.sendCommand(0x28); // Display OFF
    delay(10);
    tft.sendCommand(0x10); // Sleep Mode
    delay(120);
    digitalWrite(TFT_BL, LOW);
    SPI.end();
    gpio_hold_en((gpio_num_t)TFT_BL);
}

// ============================================================================================
// TFT DRAWING HELPERS (BATTERY & SIGNAL)
// ============================================================================================
void drawBattery(float batteryPercent, bool charging) {
    float percent = constrain(batteryPercent, 0, 100);

    int x = 270, y = 5, w = 40, h = 14;
    tft.drawRoundRect(x, y, w, h, 3, ILI9341_WHITE);
    tft.fillRect(x + w, y + 4, 3, h - 8, ILI9341_WHITE);

    int fill = map(percent, 0, 100, 0, w - 4);
    uint16_t color = (percent < 15) ? ILI9341_RED : (percent < 35) ? ILI9341_ORANGE : ILI9341_GREEN;
    tft.fillRoundRect(x + 2, y + 2, constrain(fill, 1, w - 4), h - 4, 2, color);

    if (charging) {
        static bool blink = false;
        blink = !blink;
        if (blink) {
            tft.setTextColor(ILI9341_YELLOW);
            tft.setTextSize(1);
            tft.setCursor(x + 12, y + 3);
            tft.print("+");
        }
    }
}

void drawSignal(int rssi) {
    int bars = 0;
    String displayText = "";
    
    if (rssi == -999) {
        bars = 0;
        displayText = "NO SIG";
    } else if (rssi < 0) {
        // WiFi mode - dBm to bars (σαν κινητό)
        if (rssi > -55) { 
            bars = 4; 
            displayText = "EXCELLENT";
        } else if (rssi > -65) { 
            bars = 3; 
            displayText = "GOOD";
        } else if (rssi > -75) { 
            bars = 2; 
            displayText = "FAIR";
        } else if (rssi > -85) { 
            bars = 1; 
            displayText = "POOR";
        } else { 
            bars = 0; 
            displayText = "VERY POOR";
        }
        displayText += " " + String(rssi) + "dBm";
    } else {
        // 4G mode - CSQ (0-31) to bars (σαν κινητό)
        if (rssi >= 20) { 
            bars = 4; 
            displayText = "EXCELLENT";
        } else if (rssi >= 10) { 
            bars = 3; 
            displayText = "GOOD";
        } else if (rssi >= 5) { 
            bars = 2; 
            displayText = "FAIR";
        } else if (rssi >= 2) { 
            bars = 1; 
            displayText = "POOR";
        } else { 
            bars = 0; 
            displayText = "VERY POOR";
        }
        displayText += " CSQ" + String(rssi);
    }
    
    // Σχεδίασε μπάρες σήματος (δεξιά από το όνομα παρόχου)
    int x = 95, y = 5;
    for (int i = 0; i < 4; i++) {
        int h = (i + 1) * 4;
        tft.fillRoundRect(x + i * 7, y + (16 - h), 5, h, 2, 
                         (i < bars) ? ILI9341_CYAN : ILI9341_DARKGREY);
    }
    
    // Κείμενο δίπλα στις μπάρες
    tft.setCursor(x + 35, y + 3);
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_CYAN);
    tft.print(displayText);
}

// ============================================================================================
// NETWORK OPERATOR DISPLAY με RSSI και μπάρες
// ============================================================================================
void drawNetworkOperator(String operatorName, int rssi) {
    int x = 10, y = 5;
    
    tft.setTextSize(1);
    
    if (operatorName == "" || operatorName == "NO NETWORK") {
        tft.setTextColor(ILI9341_RED);
        tft.setCursor(x, y);
        tft.print("NO NET");
        drawSignal(rssi);
        return;
    }
    
    // Έλεγχος για WiFi
    if (operatorName == "WiFi") {
        tft.setTextColor(ILI9341_CYAN);
        tft.setCursor(x, y);
        tft.print("WiFi");
        drawSignal(rssi);
        return;
    }
    
    // Έλεγχος για γνωστούς Ελληνικούς παρόχους
    String displayName = operatorName;
    tft.setTextColor(ILI9341_GREEN);
    
    if (operatorName == "COSMOTE") {
        displayName = "COS";
    }
    else if (operatorName == "VODAFONE") {
        displayName = "VOD";
        tft.setTextColor(ILI9341_YELLOW);
    }
    else if (operatorName == "WIND") {
        displayName = "WND";
        tft.setTextColor(ILI9341_ORANGE);
    }
    else {
        // Άγνωστος πάροχος - κόψε σε 6-8 χαρακτήρες
        if (operatorName.length() > 8) {
            displayName = operatorName.substring(0, 6) + "..";
        } else {
            displayName = operatorName;
        }
        tft.setTextColor(ILI9341_CYAN);
    }
    
    tft.setCursor(x, y);
    tft.print(displayName);
    
    // Σχεδίασε μπάρες και RSSI
    drawSignal(rssi);
}

// ============================================================================================
// BOOT LOGO
// ============================================================================================
void drawBootLogo() {
    tft.fillScreen(ILI9341_BLACK);
    
    // Εξωτερικό πλαίσιο
    tft.drawRect(5, 5, 310, 230, ILI9341_DARKGREY);
    
    // Λογότυπο HoneyEsp - ΚΕΝΤΡΑΡΙΣΜΕΝΟ
    tft.setTextColor(ILI9341_YELLOW);
    tft.setTextSize(3);
    String logoText = "HoneyEsp";
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(logoText, 0, 0, &x1, &y1, &w, &h);
    int logoX = 160 - (w / 2);  // Κεντράρισμα στην οθόνη (320/2=160)
    tft.setCursor(logoX, 55);
    tft.print(logoText);
    
    // Γραμμή κάτω από το λογότυπο
    tft.drawFastHLine(logoX - 10, 80, w + 20, ILI9341_YELLOW);
    
    // "By Arkas" - ΚΕΝΤΡΑΡΙΣΜΕΝΟ
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    String byText = "By Arkas";
    tft.getTextBounds(byText, 0, 0, &x1, &y1, &w, &h);
    int byX = 160 - (w / 2);
    tft.setCursor(byX, 105);
    tft.print(byText);
    
    // Έκδοση - ΚΕΝΤΡΑΡΙΣΜΕΝΗ
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_LIGHTGREY);
    String versionText = VERSION;
    tft.getTextBounds(versionText, 0, 0, &x1, &y1, &w, &h);
    int versionX = 160 - (w / 2);
    tft.setCursor(versionX, 125);
    tft.print(versionText);
    
    // Διαχωριστική γραμμή
    tft.drawFastHLine(40, 145, 240, ILI9341_DARKGREY);
    
    // Οδηγίες - ΚΕΝΤΡΑΡΙΣΜΕΝΕΣ
    tft.setTextColor(ILI9341_CYAN);
    String line1 = "Hold GPIO0 for 3s";
    tft.getTextBounds(line1, 0, 0, &x1, &y1, &w, &h);
    int line1X = 160 - (w / 2);
    tft.setCursor(line1X, 175);
    tft.print(line1);
    
    String line2 = "to config";
    tft.getTextBounds(line2, 0, 0, &x1, &y1, &w, &h);
    int line2X = 160 - (w / 2);
    tft.setCursor(line2X, 195);
    tft.print(line2);
    
    // Μικρό εικονίδιο μελισσούλας (προαιρετικό)
    tft.setTextColor(ILI9341_YELLOW);
    tft.setCursor(155, 215);
    tft.print("🐝");
}

// ============================================================================================
// MAIN MEASUREMENT SCREEN (TFT) - ΠΛΗΡΗΣ ΕΚΔΟΣΗ
// ============================================================================================
void showMeasurement(float weight, float tempAir, float tempHive, float humidity, 
                     float batteryPercent, int rssi, bool alarm, String networkOperator) {
    tft.fillScreen(ILI9341_BLACK);

    // --- ΠΑΝΩ ΜΠΑΡΑ (Αριστερή πλευρά) ---
    
    // 1. Network Operator (Αριστερά - ΠΑΝΩ ΣΕΙΡΑ)
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(8, 5);  // Μετακινήθηκε πιο πάνω (από 10 σε 5)
    String shortNet = networkOperator;
    if(shortNet.length() > 8) shortNet = shortNet.substring(0, 8);
    tft.print(shortNet);
    
    // 2. RSSI (Σήμα) - Δίπλα στο όνομα (ίδια γραμμή)
    drawNetworkOperator("", rssi);
    
    // --- ΠΑΝΩ ΜΠΑΡΑ (Δεξιά πλευρά) ---
    // 3. Μπαταρία (Δεξιά - ΠΑΝΩ ΣΕΙΡΑ)
    drawBattery(batteryPercent, false);
    
    // --- ΚΕΝΤΡΙΚΟΣ ΤΙΤΛΟΣ (ΜΕΤΑΦΕΡΘΗΚΕ ΠΙΟ ΚΑΤΩ) ---
    tft.setTextColor(ILI9341_YELLOW);
    tft.setTextSize(2);
    tft.setCursor(112, 35);  // Από 7 σε 35 - ΚΑΤΩ από το network operator
    tft.print("HoneyEsp");
    tft.drawFastHLine(60, 52, 200, ILI9341_YELLOW);  // Γραμμή κάτω από τον τίτλο (από 28 σε 52)

    // --- ΚΕΝΤΡΙΚΟ ΒΑΡΟΣ ---
    uint16_t weightColor = alarm ? ILI9341_RED : ILI9341_WHITE;
    tft.setTextColor(weightColor);
    tft.setTextSize(5);
    String weightStr = String(weight / 1000.0, 2);
    
    int16_t x1, y1;
    uint16_t w_str, h_str;
    tft.getTextBounds(weightStr, 0, 0, &x1, &y1, &w_str, &h_str);
    tft.setCursor(160 - w_str / 2, 90);  // Από 60 σε 90 (μετατοπίστηκε λόγω τίτλου)
    tft.print(weightStr);
    
    tft.setTextSize(2);
    tft.setCursor(150, 130);  // Από 105 σε 130
    tft.print("kg");

    // --- ΔΙΑΧΕΙΡΙΣΗ ΜΝΗΜΗΣ ΥΓΡΑΣΙΑΣ ---
    humPrefs.begin("hum", false);
    float minHum = humPrefs.getFloat("min", 100.0);
    float maxHum = humPrefs.getFloat("max", 0.0);

    if (humidity < minHum) { minHum = humidity; humPrefs.putFloat("min", minHum); }
    if (humidity > maxHum) { maxHum = humidity; humPrefs.putFloat("max", maxHum); }
    humPrefs.end();

    // --- ΚΑΤΩ ΜΕΡΟΣ: 3 ΚΑΡΤΕΣ ---
    
    // 1. Air Temp
    tft.fillRoundRect(8, 165, 96, 85, 8, ILI9341_DARKGREY);  // Από 140 σε 165
    tft.setTextColor(ILI9341_CYAN);
    tft.setTextSize(1);
    tft.setCursor(18, 175);  // Από 150 σε 175
    tft.print("AIR TEMP");
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(18, 195);  // Από 170 σε 195
    tft.printf("%.1f", tempAir);
    tft.setTextSize(1);
    tft.print(" C");

    // 2. Hive Temp
    tft.fillRoundRect(112, 165, 96, 85, 8, ILI9341_DARKGREY);  // Από 140 σε 165
    tft.setTextColor(ILI9341_ORANGE);
    tft.setTextSize(1);
    tft.setCursor(122, 175);  // Από 150 σε 175
    tft.print("HIVE TEMP");
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(122, 195);  // Από 170 σε 195
    tft.printf("%.1f", tempHive);
    tft.setTextSize(1);
    tft.print(" C");

    // 3. Humidity + Min/Max
    tft.fillRoundRect(216, 165, 96, 85, 8, ILI9341_DARKGREY);  // Από 140 σε 165
    tft.setTextColor(ILI9341_GREEN);
    tft.setTextSize(1);
    tft.setCursor(226, 175);  // Από 150 σε 175
    tft.print("HUMIDITY");
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(226, 195);  // Από 170 σε 195
    tft.printf("%.0f%%", humidity);
    
    // Min/Max στο κάτω μέρος της κάρτας
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_LIGHTGREY);
    tft.setCursor(224, 220);  // Από 195 σε 220
    tft.printf("min:%.0f%%", minHum);
    tft.setCursor(224, 233);  // Από 208 σε 233
    tft.printf("max:%.0f%%", maxHum);
}

void showMeasurement(float weight, float tempAir, float tempHive, float humidity, 
                     float batteryPercent, int rssi, bool alarm) {
    String networkOperator = "NO NET";
    
    #ifdef ENABLE_MODEM
    extern TinyGsm modem;
    if (modem.isNetworkConnected()) {
        networkOperator = modem.getOperator();
        if (networkOperator == "") networkOperator = "GSM";
    } else 
    #endif
    if (WiFi.status() == WL_CONNECTED) {
        networkOperator = "WiFi";
    }
    
    showMeasurement(weight, tempAir, tempHive, humidity, batteryPercent, rssi, alarm, networkOperator);
}


// ============================================================================================
// CONFIG MODE SCREEN (TFT)
// ============================================================================================
void drawConfigModeScreen() {
    tft.fillScreen(ILI9341_BLACK);
    
    // Πλαίσιο
    tft.drawRoundRect(5, 5, 310, 230, 10, ILI9341_YELLOW);
    
    // Τίτλος σε μία γραμμή (Κεντραρισμένος)
    tft.setTextColor(ILI9341_YELLOW);
    tft.setTextSize(2);
    tft.setCursor(90, 20); // Ανέβηκε στο 20 και έγινε ενιαίο
    tft.print("CONFIG MODE");
    
    // Οριζόντια γραμμή (Ανέβηκε στο 45)
    tft.drawFastHLine(30, 45, 260, ILI9341_CYAN);
    
    tft.setTextSize(1);
    // Βήμα 1 (Ανέβηκε στο 65)
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(25, 65);
    tft.print("1. Connect to Wi-Fi:");
    tft.setTextColor(ILI9341_GREEN);
    tft.setCursor(25, 78);
    tft.print("   HoneyEsp_Config");
    
    // Βήμα 2 (Ανέβηκε στο 100)
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(25, 100);
    tft.print("2. Open browser:");
    tft.setTextColor(ILI9341_CYAN);
    tft.setCursor(25, 113);
    tft.print("   192.168.4.1");
    
    // Βήμα 3 (Ανέβηκε στο 135)
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(25, 135);
    tft.print("3. Configure your");
    tft.setCursor(25, 148);
    tft.print("   WiFi, ThingSpeak, etc.");
    
    // Βήμα 4 (Ανέβηκε στο 175)
    tft.setTextColor(ILI9341_YELLOW);
    tft.setCursor(25, 175);
    tft.print("4. Save & Restart");
    
    // Έκδοση στο κάτω μέρος
    tft.setTextColor(ILI9341_LIGHTGREY);
    int versionLen = strlen(VERSION);
    int versionX = 160 - (versionLen * 3);
    tft.setCursor(versionX, 215);
    tft.print(VERSION);
}


// ============================================================================================
// Μικρό λογότυπο
// ============================================================================================
void drawLogo() {
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_YELLOW);
    tft.setCursor(5, 5);
    tft.print("H");
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(12, 5);
    tft.print("ESP");
}

// ============================================================================================
// OLED DISPLAY FUNCTIONS
// ============================================================================================
void initOLED() {
    I2C_Sensors.begin(SENSORS_SDA, SENSORS_SCL);
    I2C_Sensors.setClock(100000);
    
    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("⚠️ OLED not found on Pins 48/47!");
        return;
    }
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.display();
    Serial.println("[OLED] Initialized successfully");
}

void drawOLEDLogo() {
    oled.clearDisplay();
    oled.setTextSize(2);
    oled.setCursor(20, 10);
    oled.print("HoneyEsp");
    oled.setTextSize(1);
    oled.setCursor(30, 35);
    oled.print("By Arkas");
    oled.setCursor(25, 50);
    oled.print(VERSION);
    oled.display();
    delay(2000);
    oled.clearDisplay();
}

void showOLEDMeasurement(float weight_kg, float tempAir, float tempHive, float humidity, 
                         float batteryPercent, int rssi, String networkOperator) {
    oled.clearDisplay();
    
    // ΓΡΑΜΜΗ 1: ΒΑΡΟΣ
    oled.setTextSize(2);
    oled.setCursor(0, 0);
    oled.print("W:");
    oled.print(weight_kg, 1);
    oled.print("kg");
    
    // ΓΡΑΜΜΗ 2: ΘΕΡΜΟΚΡΑΣΙΕΣ
    oled.setTextSize(1);
    oled.setCursor(0, 20);
    oled.print("A:");
    oled.print(tempAir, 1);
    oled.print("C H:");
    oled.print(tempHive, 1);
    oled.print("C");
    
    // ΓΡΑΜΜΗ 3: ΥΓΡΑΣΙΑ + ΜΠΑΤΑΡΙΑ
    oled.setCursor(0, 35);
    oled.print("HUM:");
    oled.print(humidity, 0);
    oled.print("%");
    
    oled.setCursor(72, 35);
    oled.print("B:");
    oled.print(batteryPercent, 0);
    oled.print("%");
    
    // ΓΡΑΜΜΗ 4: ΠΑΡΟΧΟΣ + ΣΗΜΑ
    oled.setCursor(0, 50);
    
    // Συντομευμένο όνομα παρόχου
    String shortOp = networkOperator;
    if (shortOp == "COSMOTE") shortOp = "COS";
    else if (shortOp == "VODAFONE") shortOp = "VOD";
    else if (shortOp == "WIND") shortOp = "WND";
    else if (shortOp == "WiFi") shortOp = "WF";
    else if (shortOp == "NO NETWORK") shortOp = "NO";
    else if (shortOp.length() > 4) shortOp = shortOp.substring(0, 3);
    
    oled.print(shortOp);
    oled.print(":");
    
    // Εμφάνιση RSSI
    if (rssi == -999) {
        oled.print("NO");
    } else if (rssi < 0) {
        oled.print(rssi);
        oled.print("dB");
    } else {
        oled.print("C");
        oled.print(rssi);
    }
    
    oled.display();
}

// Overload για OLED χωρίς networkOperator (auto-detect)
void showOLEDMeasurement(float weight_kg, float tempAir, float tempHive, float humidity, 
                         float batteryPercent, int rssi) {
    String networkOperator = "---";
    
    #ifdef ENABLE_MODEM
    extern TinyGsm modem;
    if (modem.isNetworkConnected()) {
        networkOperator = modem.getOperator();
        if (networkOperator == "") networkOperator = "NO NET";
    } else 
    #endif
    if (WiFi.status() == WL_CONNECTED) {
        networkOperator = "WiFi";
    } else {
        networkOperator = "NO NET";
    }
    
    showOLEDMeasurement(weight_kg, tempAir, tempHive, humidity, batteryPercent, rssi, networkOperator);
}

void oledSleep() {
    oled.clearDisplay();
    oled.display();
    oled.ssd1306_command(SSD1306_DISPLAYOFF);
}

void oledWake() {
    oled.ssd1306_command(SSD1306_DISPLAYON);
    oled.display();
}
