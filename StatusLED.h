#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

void initStatusLED();
void setRGB(uint8_t r, uint8_t g, uint8_t b);
void setStatusBoot();
void setStatusConfig();
void setStatusWiFiOK();
void setStatusWiFiFail();
void setStatus4GOK();
void setStatus4GFail();
void setStatusSleep();

void setErrorCode(uint8_t code);
void updateErrorLED();

#endif
