#include "StatusLED.h"
#include "Config.h"

Adafruit_NeoPixel rgb(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

uint8_t errorCode = 0;
unsigned long lastBlink = 0;
uint8_t blinkCount = 0;
bool ledState = false;

void initStatusLED() {
    pinMode(LED2_PIN, OUTPUT);
    digitalWrite(LED2_PIN, LOW);
    
    // Αρχικοποίηση CONFIG_LED (GPIO12)
    pinMode(CONFIG_LED, OUTPUT);
    digitalWrite(CONFIG_LED, LOW);

    rgb.begin();
    rgb.clear();
    rgb.show();
}

void setRGB(uint8_t r, uint8_t g, uint8_t b) {
    rgb.setPixelColor(0, rgb.Color(r, g, b));
    rgb.show();
}

// ================= STATUS =================

void setStatusBoot() { 
    setRGB(255, 150, 0);     // Πορτοκαλί
    digitalWrite(CONFIG_LED, LOW);
}

void setStatusConfig() { 
    setRGB(0, 0, 255);       // Μπλε RGB
    digitalWrite(CONFIG_LED, HIGH);  // Ανάβει το ξεχωριστό LED
}

void setStatusWiFiOK() { 
    setRGB(0, 255, 0);       // Πράσινο
    digitalWrite(CONFIG_LED, LOW);
}

void setStatusWiFiFail() { 
    setRGB(150, 0, 255);     // Μωβ
    digitalWrite(CONFIG_LED, LOW);
}

void setStatus4GOK() { 
    setRGB(0, 0, 255);       // Μπλε
    digitalWrite(CONFIG_LED, LOW);
}

void setStatus4GFail() { 
    setRGB(255, 0, 0);       // Κόκκινο
    digitalWrite(CONFIG_LED, LOW);
}

void setStatusSleep() { 
    setRGB(0, 0, 0);         // Σβηστό
    digitalWrite(CONFIG_LED, LOW);
}

// ================= ERROR SYSTEM =================

void setErrorCode(uint8_t code) {
    errorCode = code;
    blinkCount = 0;
}

void updateErrorLED() {
    if (errorCode == 0) return;

    unsigned long now = millis();

    if (now - lastBlink > 300) {
        lastBlink = now;

        ledState = !ledState;
        digitalWrite(LED2_PIN, ledState);

        if (!ledState) {
            blinkCount++;

            if (blinkCount >= errorCode) {
                delay(1500);
                blinkCount = 0;
            }
        }
    }
}
