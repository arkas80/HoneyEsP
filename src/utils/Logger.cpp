#include "Logger.h"

void Logger::begin() {
    Serial.println("Logger initialized");
}

void Logger::log(String message) {
    printTimestamp();
    Serial.println("[INFO] " + message);
}

void Logger::logError(String message) {
    printTimestamp();
    Serial.println("[ERROR] " + message);
}

void Logger::logWarning(String message) {
    printTimestamp();
    Serial.println("[WARN] " + message);
}

void Logger::logDebug(String message) {
    #if DEBUG_MODE
    printTimestamp();
    Serial.println("[DEBUG] " + message);
    #endif
}

void Logger::printTimestamp() {
    unsigned long ms = millis();
    unsigned long sec = ms / 1000;
    unsigned long min = sec / 60;
    
    Serial.print("[");
    Serial.print(min);
    Serial.print(":");
    Serial.print(sec % 60);
    Serial.print(".");
    Serial.print(ms % 1000);
    Serial.print("] ");
}