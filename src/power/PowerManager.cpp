#include "PowerManager.h"

PowerManager::PowerManager() {
    scheduleCount = 0;
    sleepDuration = 300;
    batteryPercentage = 100;
}

void PowerManager::begin() {
    pinMode(BATTERY_ADC_PIN, INPUT);
    checkBattery();
}

bool PowerManager::shouldTakeMeasurement() {
    // Check if current time is within any active schedule
    return true; // Placeholder
}

bool PowerManager::shouldSleep() {
    return true; // Placeholder
}

void PowerManager::addSchedule(int startH, int startM, int endH, int endM) {
    if (scheduleCount < MAX_SCHEDULES) {
        schedules[scheduleCount].startHour = startH;
        schedules[scheduleCount].startMinute = startM;
        schedules[scheduleCount].endHour = endH;
        schedules[scheduleCount].endMinute = endM;
        schedules[scheduleCount].enabled = true;
        scheduleCount++;
    }
}

void PowerManager::removeSchedule(int index) {
    if (index < scheduleCount) {
        for (int i = index; i < scheduleCount - 1; i++) {
            schedules[i] = schedules[i + 1];
        }
        scheduleCount--;
    }
}

void PowerManager::loadSchedules(Schedule* scheds, int count) {
    for (int i = 0; i < count && i < MAX_SCHEDULES; i++) {
        schedules[i] = scheds[i];
    }
    scheduleCount = count;
}

void PowerManager::saveSchedules() {
    // Save to SPIFFS
}

int PowerManager::getBatteryPercent() {
    return batteryPercentage;
}

uint32_t PowerManager::getSleepDuration() {
    return sleepDuration;
}

void PowerManager::setSleepDuration(uint32_t seconds) {
    sleepDuration = seconds;
}

void PowerManager::enterDeepSleep(uint32_t seconds) {
    esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
    esp_deep_sleep_start();
}

void PowerManager::checkBattery() {
    int rawValue = analogRead(BATTERY_ADC_PIN);
    // Convert ADC reading to percentage (depends on voltage divider)
    // Assuming max voltage = 4.2V (fully charged), min = 3.0V (empty)
    float voltage = (rawValue / 4095.0f) * 3.3f * 2.0f; // Adjust multiplier for divider
    
    if (voltage >= 4.2f) {
        batteryPercentage = 100;
    } else if (voltage <= 3.0f) {
        batteryPercentage = 0;
    } else {
        batteryPercentage = (voltage - 3.0f) / (4.2f - 3.0f) * 100;
    }
}