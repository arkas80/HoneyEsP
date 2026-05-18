// HX711.cpp - Complete HoneyPi-style implementation for ESP32
// FIXED: get_units() returns GRAMS correctly
#include "HX711.h"
#include <cmath>

HX711::HX711() {
    DEBUG_MODE = false;
    numDataFilteredOut = 0;
    OFFSET = 0;
    SCALE = 2280.0f;
    tempCompensationFactor = -0.05f;
    referenceTemp = 20.0f;
    currentTemperature = 0.0f;
    filterOutlierThreshold = 2.5f;
    minSamplesForFilter = 5;
}

void HX711::begin(byte dout, byte pd_sck, byte gain) {
    PD_SCK_PIN = pd_sck;
    DOUT_PIN = dout;
    pinMode(PD_SCK_PIN, OUTPUT);
    pinMode(DOUT_PIN, INPUT);
    set_gain(gain);
}

bool HX711::is_ready() {
    return digitalRead(DOUT_PIN) == LOW;
}

void HX711::set_gain(byte gain) {
    switch (gain) {
        case 128: GAIN = 1; break;
        case 64: GAIN = 3; break;
        case 32: GAIN = 2; break;
        default: GAIN = 1; break;
    }
    digitalWrite(PD_SCK_PIN, LOW);
    read();
}

long HX711::read() {
   unsigned long start = millis();
const unsigned long TIMEOUT = 100; // ms


// αλλαγη για να μη κολλαει οταν δεν υπαρχει


while (!is_ready()) { 
    if (millis() - start > TIMEOUT) {
        return 0; // 👈 bailout αν δεν υπάρχει HX711
    }
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

long HX711::read_average(byte times) {
    if (times == 0) times = 1;
    
    long sum = 0;
    for (byte i = 0; i < times; i++) {
        sum += read();
        delay(10);
    }
    return sum / times;
}

double HX711::get_value(byte times) {
    return read_average(times) - OFFSET;
}

// ============================================================
// FIXED: get_units() returns GRAMS
// Το SCALE που υπολογίζεται είναι: raw_difference / weight_in_GRAMS
// Οπότε: weight_in_GRAMS = raw_difference / SCALE
// ============================================================
float HX711::get_units(byte times) {
    return get_value(times) / SCALE;  // SCALE είναι ήδη για GRAMS
}

// ============ HONEYPI HELPER FUNCTIONS ============

float HX711::average(std::vector<float>& arr) {
    if (arr.empty()) return 0;
    float sum = 0;
    for (float v : arr) {
        sum += v;
    }
    return sum / arr.size();
}

float HX711::find_max(std::vector<float>& arr) {
    if (arr.empty()) return 0;
    float maxVal = arr[0];
    for (float v : arr) {
        if (v > maxVal) maxVal = v;
    }
    return maxVal;
}

float HX711::find_min(std::vector<float>& arr) {
    if (arr.empty()) return 0;
    float minVal = arr[0];
    for (float v : arr) {
        if (v < minVal) minVal = v;
    }
    return minVal;
}

float HX711::take_closest(std::vector<float>& arr, float target) {
    if (arr.empty()) return 0;
    float closest = arr[0];
    float minDiff = fabs(arr[0] - target);
    
    for (float v : arr) {
        float diff = fabs(v - target);
        if (diff < minDiff) {
            minDiff = diff;
            closest = v;
        }
    }
    return closest;
}

// ============ HONEYPI ROBUST FILTERING ============

float HX711::get_robust_units(byte times) {
    if (times < minSamplesForFilter) times = minSamplesForFilter;
    
    std::vector<float> readings;
    numDataFilteredOut = 0;
    
    for (byte i = 0; i < times; i++) {
        readings.push_back(get_units(1));
        delay(5);
    }
    
    float mean = average(readings);
    
    float variance = 0;
    for (float v : readings) {
        variance += pow(v - mean, 2);
    }
    float stdDev = sqrt(variance / readings.size());
    
    std::vector<float> filtered;
    for (float v : readings) {
        if (fabs(v - mean) <= (filterOutlierThreshold * stdDev)) {
            filtered.push_back(v);
        } else {
            numDataFilteredOut++;
        }
    }
    
    float result = filtered.empty() ? mean : average(filtered);
    
    if (DEBUG_MODE) {
        Serial.printf("📊 Robust Filter: %.3fg (outliers: %d/%d)\n", 
                     result, numDataFilteredOut, times);
    }
    
    return result;
}

float HX711::get_weight_mean(int readings) {
    if (readings <= 0) readings = 30;
    return get_robust_units(readings);
}

int HX711::get_num_data_filtered_out() {
    return numDataFilteredOut;
}

float HX711::get_percentage_filtered_out(int total_samples) {
    if (total_samples == 0) return 0;
    return (numDataFilteredOut * 100.0f) / total_samples;
}

// ============ HONEYPI ADVANCED MEASUREMENT ============

float HX711::measure_weight_honeypi(int reference_unit, int offset, bool invert, bool filter_negative) {
    if (DEBUG_MODE) {
        Serial.println("🍯 HoneyPi Advanced Measurement started");
    }
    
    set_scale_ratio(reference_unit);
    set_offset(offset);
    
    int LOOP_TRYS = 6;
    float weight = 0;
    
    for (int count = 1; count <= LOOP_TRYS; count++) {
        std::vector<float> weightMeasures;
        int LOOP_AVG = 3;
        
        for (int count_avg = 1; count_avg <= LOOP_AVG && count_avg <= 6; count_avg++) {
            int num_measurements = 41;
            float reading = get_weight_mean(num_measurements);
            
            if (!isnan(reading) && reading != 0) {
                weightMeasures.push_back(reading);
                float percentFiltered = get_percentage_filtered_out(num_measurements);
                
                if (DEBUG_MODE) {
                    Serial.printf("   Measurement %d: %.1fg (filtered: %.1f%%)\n", 
                                 count_avg, reading, percentFiltered);
                }
                
                if (percentFiltered >= 40) {
                    Serial.printf("⚠️  WARNING: %.1f%% filtered out! Check power supply/cabling\n", 
                                 percentFiltered);
                }
            } else {
                LOOP_AVG++;
                Serial.println("   Failed measurement, retrying...");
            }
        }
        
        if (!weightMeasures.empty()) {
            float avgWeight = average(weightMeasures);
            float maxWeight = find_max(weightMeasures);
            float minWeight = find_min(weightMeasures);
            weight = take_closest(weightMeasures, avgWeight);
            
            if (DEBUG_MODE) {
                Serial.printf("   Max: %.1fg, Min: %.1fg, Avg: %.1fg, Chosen: %.1fg\n",
                             maxWeight, minWeight, avgWeight, weight);
            }
            
            float ALLOWED_DIVERGENCE = (500.0f / reference_unit);
            if (ALLOWED_DIVERGENCE < 20) ALLOWED_DIVERGENCE = 20;
            if (ALLOWED_DIVERGENCE > 300) ALLOWED_DIVERGENCE = 300;
            
            if (fabs(avgWeight - weight) <= ALLOWED_DIVERGENCE) {
                break;
            } else {
                Serial.printf("⚠️  Divergence too high (%.1f > %.1f), retry %d/%d\n",
                             fabs(avgWeight - weight), ALLOWED_DIVERGENCE, count, LOOP_TRYS);
                
                if (count == LOOP_TRYS) {
                    weight = easy_weight(PD_SCK_PIN, DOUT_PIN, 'A', reference_unit, offset);
                    Serial.printf("   Using fallback weight: %.1fg\n", weight);
                }
            }
        }
    }
    
    if (invert && !isnan(weight)) {
        weight = -weight;
    }
    
    if (filter_negative && weight < -1) {
        weight = 0;
    }
    
    if (DEBUG_MODE) {
        Serial.printf("🍯 Final weight: %.1fg\n", weight);
    }
    
    return weight;
}

// ============ HONEYPI EASY WEIGHT (FALLBACK) ============

float HX711::easy_weight(byte dout_pin, byte sck_pin, char channel, float reference_unit, long offset) {
    if (DEBUG_MODE) {
        Serial.println("🔄 Using easy_weight fallback method");
    }
    
    byte old_dout = DOUT_PIN;
    byte old_sck = PD_SCK_PIN;
    
    begin(dout_pin, sck_pin);
    set_channel(channel);
    set_scale_ratio(reference_unit);
    set_offset(offset);
    
    float weight = get_weight_mean(30);
    
    begin(old_dout, old_sck);
    
    return weight;
}

// ============ HONEYPI INIT WITH RETRY ============

bool HX711::init_hx711(byte dout, byte pd_sck, char channel, bool debug) {
    set_debug_mode(debug);
    
    if (DEBUG_MODE) {
        Serial.printf("🔧 Initializing HX711 on DT:%d, SCK:%d, Channel:%c\n", 
                     dout, pd_sck, channel);
    }
    
    int loops = 0;
    bool errorEncountered = true;
    
    while (errorEncountered && loops < 3) {
        loops++;
        try {
            begin(dout, pd_sck);
            set_channel(channel);
            errorEncountered = !reset();
            
            if (!errorEncountered) {
                if (DEBUG_MODE) {
                    Serial.printf("✅ HX711 initialized successfully (attempt %d/3)\n", loops);
                }
                return true;
            }
        } catch (...) {
            if (DEBUG_MODE) {
                Serial.printf("⚠️  Init failed (attempt %d/3), retrying...\n", loops);
            }
        }
        delay(1000);
    }
    
    Serial.printf("❌ HX711 initialization failed after %d attempts!\n", loops);
    return false;
}

// ============ STABILITY METRICS ============

float HX711::get_stability_metric(byte times) {
    if (times < 3) times = 3;
    
    std::vector<float> readings;
    for (byte i = 0; i < times; i++) {
        readings.push_back(get_units(1));
        delay(10);
    }
    
    float mean = average(readings);
    float variance = 0;
    for (float v : readings) {
        variance += pow(v - mean, 2);
    }
    
    return sqrt(variance / readings.size());
}

bool HX711::is_stable(byte times, float threshold) {
    return get_stability_metric(times) <= threshold;
}

// ============ TEMPERATURE COMPENSATION ============

float HX711::compensate_temperature(float weight, float temperature, float compensation_temp, float compensation_value) {
    if (compensation_temp == 0 || compensation_value == 0) {
        return weight;
    }
    
    float delta = temperature - compensation_temp;
    float compensated = weight - (compensation_value * delta);
    
    if (DEBUG_MODE) {
        Serial.printf("🌡️ Temp Comp: %.1f°C -> %.1fg (delta: %.1f, comp: %.3f)\n",
                     temperature, compensated, delta, compensation_value);
    }
    
    return compensated;
}

float HX711::get_compensated_weight(float rawWeight, float temperature) {
    if (temperature == 0.0f || rawWeight == 0.0f) {
        return rawWeight;
    }
    
    float tempDifference = temperature - referenceTemp;
    float compensatedWeight = rawWeight - (tempCompensationFactor * tempDifference * rawWeight / 100.0f);
    
    return compensatedWeight;
}

float HX711::get_compensated_units(float temperature, byte times) {
    float rawWeight = get_robust_units(times);
    return get_compensated_weight(rawWeight, temperature);
}

// ============ AUTO-CALIBRATION ============

bool HX711::auto_calibrate(float knownWeight, float temperature) {
    if (knownWeight <= 0 || knownWeight > 200000.0f) {
        Serial.println("❌ Invalid known weight");
        return false;
    }
    
    Serial.println("\n=== HONEYPI CALIBRATION START ===");
    Serial.printf("Target: %.1fg\n", knownWeight);
    
    float measuredWeight = get_robust_units(30);
    
    if (measuredWeight <= 0) {
        Serial.println("❌ Invalid measurement");
        return false;
    }
    
    float newScale = (measuredWeight / knownWeight) * SCALE;
    
    Serial.printf("Old scale: %.4f\n", SCALE);
    Serial.printf("New scale: %.4f\n", newScale);
    
    set_scale(newScale);
    
    float testWeight = get_robust_units(20);
    float errorPercent = fabs(testWeight - knownWeight) / knownWeight * 100.0f;
    
    Serial.printf("Verification: %.1fg\n", testWeight);
    Serial.printf("Error: %.1f%%\n", errorPercent);
    Serial.println("=== CALIBRATION COMPLETE ===");
    
    return (errorPercent < 10.0f);
}

// ============ TARE ============

void HX711::tare(byte times) {
    if (times == 0) times = 15;
    
    Serial.println("\n=== TARE START ===");
    
    float tareValue = get_robust_units(times);
    long currentReading = read_average(5);
    long newOffset = currentReading - (long)(tareValue * SCALE);
    
    set_offset(newOffset);
    
    Serial.printf("Tare complete. Offset: %ld\n", newOffset);
    Serial.println("=== TARE COMPLETE ===\n");
}

// ============ SCALE AND OFFSET MANAGEMENT ============

void HX711::set_scale(float scale) {
    SCALE = scale;
    if (DEBUG_MODE) Serial.printf("Scale: %.4f\n", SCALE);
}

float HX711::get_scale() {
    return SCALE;
}

void HX711::set_scale_ratio(float scale_ratio) {
    SCALE = scale_ratio;
}

float HX711::get_scale_ratio() {
    return SCALE;
}

void HX711::set_offset(long offset) {
    OFFSET = offset;
    if (DEBUG_MODE) Serial.printf("Offset: %ld\n", OFFSET);
}

long HX711::get_offset() {
    return OFFSET;
}

// ============ POWER MANAGEMENT ============

void HX711::power_down() {
    digitalWrite(PD_SCK_PIN, LOW);
    digitalWrite(PD_SCK_PIN, HIGH);
    delayMicroseconds(100);
}

void HX711::power_up() {
    digitalWrite(PD_SCK_PIN, LOW);
}

bool HX711::reset() {
    power_down();
    delay(10);
    power_up();
    set_gain(128);
    return true;
}

// ============ UTILITIES ============

void HX711::set_debug_mode(bool flag) {
    DEBUG_MODE = flag;
}

long HX711::get_raw_data_mean(int readings) {
    if (readings <= 0) readings = 1;
    return read_average(readings);
}

void HX711::set_channel(char channel) {
    if (channel == 'A') {
        set_gain(128);
    } else if (channel == 'B') {
        set_gain(32);
    }
}

void HX711::set_temperature_compensation(float factor, float refTemp) {
    tempCompensationFactor = factor;
    referenceTemp = refTemp;
}

void HX711::set_temperature(float temp) {
    currentTemperature = temp;
}

float HX711::get_current_temperature() {
    return currentTemperature;
}

float HX711::get_compensation_factor() {
    return tempCompensationFactor;
}

void HX711::set_filter_threshold(float threshold) {
    filterOutlierThreshold = threshold;
}

float HX711::get_filter_threshold() {
    return filterOutlierThreshold;
}
