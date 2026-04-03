#include "HX711_Sensor.h"
#include <cmath>

HX711_Sensor::HX711_Sensor() {
    DEBUG_MODE = false;
    numDataFilteredOut = 0;
    OFFSET = 0;
    SCALE = 2280.0f;
    tempCompensationFactor = -0.04f;
    referenceTemp = 20.0f;
    currentTemperature = 0.0f;
}

void HX711_Sensor::begin(byte dout, byte pd_sck, byte gain) {
    PD_SCK_PIN = pd_sck;
    DOUT_PIN = dout;
    pinMode(PD_SCK_PIN, OUTPUT);
    pinMode(DOUT_PIN, INPUT);
    set_gain(gain);
}

bool HX711_Sensor::is_ready() {
    return digitalRead(DOUT_PIN) == LOW;
}

void HX711_Sensor::set_gain(byte gain) {
    switch (gain) {
        case 128: GAIN = 1; break;
        case 64: GAIN = 3; break;
        case 32: GAIN = 2; break;
        default: GAIN = 1; break;
    }
    digitalWrite(PD_SCK_PIN, LOW);
    read();
}

long HX711_Sensor::read() {
    while (!is_ready()) {
        yield();
    }
    
    unsigned long value = 0;
    noInterrupts();
    
    for (uint8_t i = 0; i < 24; i++) {
        digitalWrite(PD_SCK_PIN, HIGH);
        delayMicroseconds(1);
        digitalWrite(PD_SCK_PIN, LOW);
        delayMicroseconds(1);
        value = value << 1;
        if (digitalRead(DOUT_PIN)) { value++; }
    }
    
    for (uint8_t i = 0; i < GAIN; i++) {
        digitalWrite(PD_SCK_PIN, HIGH);
        delayMicroseconds(1);
        digitalWrite(PD_SCK_PIN, LOW);
        delayMicroseconds(1);
    }
    
    interrupts();
    
    if (value & 0x800000) {
        value |= 0xFF000000;
    }
    
    return (long)value;
}

long HX711_Sensor::read_average(byte times) {
    if (times == 0) times = 1;
    long sum = 0;
    for (byte i = 0; i < times; i++) {
        sum += read();
        delay(10);
    }
    return sum / times;
}

double HX711_Sensor::get_value(byte times) {
    return read_average(times) - OFFSET;
}

float HX711_Sensor::get_units(byte times) {
    return get_value(times) / SCALE;
}

float HX711_Sensor::get_robust_units(byte times) {
    if (times < 5) times = 5;
    
    float *readings = new float[times];
    float sum = 0;
    
    for (byte i = 0; i < times; i++) {
        readings[i] = get_units(1);
        sum += readings[i];
        delay(5);
    }
    
    float mean = sum / times;
    float stdDev = 0;
    
    for (byte i = 0; i < times; i++) {
        stdDev += pow(readings[i] - mean, 2);
    }
    stdDev = sqrt(stdDev / times);
    
    float filteredSum = 0;
    byte validCount = 0;
    numDataFilteredOut = 0;
    
    for (byte i = 0; i < times; i++) {
        if (abs(readings[i] - mean) <= (2.5f * stdDev)) {
            filteredSum += readings[i];
            validCount++;
        } else {
            numDataFilteredOut++;
        }
    }
    
    float result = (validCount > 0) ? (filteredSum / validCount) : mean;
    delete[] readings;
    return result;
}

float HX711_Sensor::get_stability_metric(byte times) {
    if (times < 3) times = 3;
    
    float *readings = new float[times];
    float sum = 0;
    
    for (byte i = 0; i < times; i++) {
        readings[i] = get_units(1);
        sum += readings[i];
        delay(10);
    }
    
    float mean = sum / times;
    float variance = 0;
    
    for (byte i = 0; i < times; i++) {
        variance += pow(readings[i] - mean, 2);
    }
    
    float stability = sqrt(variance / times);
    delete[] readings;
    return stability;
}

bool HX711_Sensor::is_stable(byte times, float threshold) {
    return get_stability_metric(times) <= threshold;
}

void HX711_Sensor::tare(byte times) {
    if (times == 0) times = 1;
    double sum = 0;
    for (byte i = 0; i < times; i++) {
        sum += read();
        delay(10);
    }
    set_offset((long)(sum / times));
}

void HX711_Sensor::set_scale(float scale) {
    SCALE = scale;
}

float HX711_Sensor::get_scale() {
    return SCALE;
}

void HX711_Sensor::set_offset(long offset) {
    OFFSET = offset;
}

long HX711_Sensor::get_offset() {
    return OFFSET;
}

void HX711_Sensor::power_down() {
    digitalWrite(PD_SCK_PIN, LOW);
    digitalWrite(PD_SCK_PIN, HIGH);
    delayMicroseconds(100);
}

void HX711_Sensor::power_up() {
    digitalWrite(PD_SCK_PIN, LOW);
}

bool HX711_Sensor::reset() {
    power_down();
    power_up();
    return true;
}

void HX711_Sensor::set_debug_mode(bool flag) {
    DEBUG_MODE = flag;
}

int HX711_Sensor::get_num_data_filtered_out() {
    return numDataFilteredOut;
}

void HX711_Sensor::set_temperature_compensation(float factor, float refTemp) {
    tempCompensationFactor = factor;
    referenceTemp = refTemp;
}

float HX711_Sensor::get_compensated_units(float temperature, byte times) {
    float rawWeight = get_robust_units(times);
    float tempDifference = temperature - referenceTemp;
    float compensation = 1.0f + (tempCompensationFactor * tempDifference / 100.0f);
    return rawWeight * compensation;
}

bool HX711_Sensor::auto_calibrate(float knownWeight, float temperature) {
    if (knownWeight <= 0 || knownWeight > 200000.0f) {
        Serial.println("Invalid known weight");
        return false;
    }
    
    long raw_reading = read_average(20);
    double raw_value = raw_reading - OFFSET;
    
    if (abs(raw_value) < 10.0) {
        Serial.println("Raw value too small");
        return false;
    }
    
    float newScale = raw_value / knownWeight;
    
    if (newScale < 100.0f || newScale > 100000.0f) {
        Serial.println("Scale factor out of range");
        return false;
    }
    
    set_scale(newScale);
    return true;
}