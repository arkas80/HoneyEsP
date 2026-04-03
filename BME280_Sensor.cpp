#include "BME280_Sensor.h"
#include <math.h>

BME280_Sensor::BME280_Sensor(uint8_t address) {
    _address = address;
    _wire = &Wire;
    t_fine = 0;
    seaLevelPressure = 1013.25;
}

bool BME280_Sensor::begin(TwoWire *wire) {
    _wire = wire;
    
    #ifdef ARDUINO_ARCH_ESP32
    _wire->setTimeOut(1000);
    #endif
    
    _wire->beginTransmission(_address);
    if (_wire->endTransmission() != 0) {
        Serial.println("BME280 not found at address 0x" + String(_address, HEX));
        return false;
    }
    
    uint8_t id = readRegister(0xD0);
    Serial.println("BME280 Chip ID: 0x" + String(id, HEX));
    
    if (id != 0x60 && id != 0x58) {
        Serial.println("Invalid BME280 ID");
        return false;
    }
    
    if (!readCalibrationData()) {
        return false;
    }
    
    writeRegister(0xE0, 0xB6);
    delay(100);
    
    writeRegister(0xF2, 0x01);
    writeRegister(0xF4, 0x27);
    writeRegister(0xF5, 0x00);
    
    delay(100);
    Serial.println("BME280 initialized");
    return true;
}

bool BME280_Sensor::isConnected() {
    _wire->beginTransmission(_address);
    return (_wire->endTransmission() == 0);
}

bool BME280_Sensor::readCalibrationData() {
    uint8_t cal1[26];
    if (!readRegisters(0x88, cal1, 26)) {
        return false;
    }
    
    dig_T1 = (uint16_t)((cal1[1] << 8) | cal1[0]);
    dig_T2 = (int16_t)((cal1[3] << 8) | cal1[2]);
    dig_T3 = (int16_t)((cal1[5] << 8) | cal1[4]);
    
    dig_P1 = (uint16_t)((cal1[7] << 8) | cal1[6]);
    dig_P2 = (int16_t)((cal1[9] << 8) | cal1[8]);
    dig_P3 = (int16_t)((cal1[11] << 8) | cal1[10]);
    dig_P4 = (int16_t)((cal1[13] << 8) | cal1[12]);
    dig_P5 = (int16_t)((cal1[15] << 8) | cal1[14]);
    dig_P6 = (int16_t)((cal1[17] << 8) | cal1[16]);
    dig_P7 = (int16_t)((cal1[19] << 8) | cal1[18]);
    dig_P8 = (int16_t)((cal1[21] << 8) | cal1[20]);
    dig_P9 = (int16_t)((cal1[23] << 8) | cal1[22]);
    
    dig_H1 = cal1[25];
    
    uint8_t cal2[8];
    if (!readRegisters(0xE1, cal2, 8)) {
        return false;
    }
    
    dig_H2 = (int16_t)((cal2[1] << 8) | cal2[0]);
    dig_H3 = cal2[2];
    dig_H4 = (int16_t)((cal2[3] << 4) | (cal2[4] & 0x0F));
    dig_H5 = (int16_t)((cal2[5] << 4) | (cal2[4] >> 4));
    dig_H6 = (int8_t)cal2[6];
    
    return true;
}

bool BME280_Sensor::readAll(float* temperature, float* pressure, float* humidity) {
    uint8_t status = readRegister(0xF3);
    
    if (status & 0x08) {
        delay(10);
    }
    
    uint8_t data[8];
    if (!readRegisters(0xF7, data, 8)) {
        *temperature = NAN;
        *pressure = NAN;
        *humidity = NAN;
        return false;
    }
    
    int32_t adc_pres = (int32_t)((data[0] << 12) | (data[1] << 4) | (data[2] >> 4));
    int32_t adc_temp = (int32_t)((data[3] << 12) | (data[4] << 4) | (data[5] >> 4));
    int32_t adc_hum = (int32_t)((data[6] << 8) | data[7]);
    
    if (adc_temp == 0x80000 || adc_temp == 0) {
        *temperature = NAN;
        *pressure = NAN;
        *humidity = NAN;
        return false;
    }
    
    *temperature = compensateTemperature(adc_temp);
    float raw_pressure = compensatePressure(adc_pres);
    
    if (raw_pressure < 30000.0f || raw_pressure > 120000.0f) {
        *pressure = NAN;
    } else {
        *pressure = raw_pressure / 100.0f;
    }
    
    if (adc_hum == 0x8000 || adc_hum == 0) {
        *humidity = NAN;
    } else {
        *humidity = compensateHumidity(adc_hum);
        if (*humidity > 100.0f) *humidity = 100.0f;
        if (*humidity < 0.0f) *humidity = 0.0f;
    }
    
    return true;
}

float BME280_Sensor::readTemperature() {
    float temp, pres, hum;
    if (readAll(&temp, &pres, &hum)) {
        return temp;
    }
    return NAN;
}

float BME280_Sensor::readPressure() {
    float temp, pres, hum;
    if (readAll(&temp, &pres, &hum)) {
        return pres;
    }
    return NAN;
}

float BME280_Sensor::readHumidity() {
    float temp, pres, hum;
    if (readAll(&temp, &pres, &hum)) {
        return hum;
    }
    return NAN;
}

float BME280_Sensor::readAltitude(float seaLevel) {
    if (seaLevel <= 0) {
        seaLevel = seaLevelPressure;
    }
    
    float pressure = readPressure();
    if (isnan(pressure) || pressure <= 300.0f || pressure >= 1200.0f) {
        return NAN;
    }
    
    if (seaLevel <= 300.0f || seaLevel >= 1200.0f || seaLevel <= pressure) {
        return NAN;
    }
    
    return 44330.0f * (1.0f - pow(pressure / seaLevel, 0.1903f));
}

void BME280_Sensor::setSeaLevelPressure(float pressure) {
    seaLevelPressure = pressure;
}

float BME280_Sensor::getSeaLevelPressure() {
    return seaLevelPressure;
}

float BME280_Sensor::compensateTemperature(int32_t adc_temp) {
    int32_t var1, var2, T;
    
    var1 = ((((adc_temp >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_temp >> 4) - ((int32_t)dig_T1)) * ((adc_temp >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
    
    t_fine = var1 + var2;
    T = (t_fine * 5 + 128) >> 8;
    
    return (float)T / 100.0f;
}

float BME280_Sensor::compensatePressure(int32_t adc_press) {
    int64_t var1, var2, p;
    int64_t pressure;
    
    if (t_fine == 0) {
        return 0;
    }
    
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + (((int64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;
    
    if (var1 == 0) {
        return 0;
    }
    
    p = 1048576 - adc_press;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dig_P8) * p) >> 19;
    
    pressure = (p + var1 + var2) >> 8;
    
    return (float)pressure / 256.0f;
}

float BME280_Sensor::compensateHumidity(int32_t adc_hum) {
    int32_t v_x1_u32r;
    
    v_x1_u32r = (t_fine - ((int32_t)76800));
    v_x1_u32r = (((((adc_hum << 14) - (((int32_t)dig_H4) << 20) - (((int32_t)dig_H5) * v_x1_u32r)) +
                  ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)dig_H6)) >> 10) * (((v_x1_u32r * ((int32_t)dig_H3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) * ((int32_t)dig_H2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)dig_H1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
    v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
    
    return (float)(v_x1_u32r >> 12) / 1024.0f;
}

uint8_t BME280_Sensor::readRegister(uint8_t reg) {
    _wire->beginTransmission(_address);
    _wire->write(reg);
    if (_wire->endTransmission() != 0) {
        return 0xFF;
    }
    
    if (_wire->requestFrom(_address, (uint8_t)1) != 1) {
        return 0xFF;
    }
    return _wire->read();
}

void BME280_Sensor::writeRegister(uint8_t reg, uint8_t value) {
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->write(value);
    _wire->endTransmission();
}

bool BME280_Sensor::readRegisters(uint8_t reg, uint8_t* buffer, uint8_t length) {
    _wire->beginTransmission(_address);
    _wire->write(reg);
    if (_wire->endTransmission() != 0) {
        return false;
    }
    
    if (_wire->requestFrom(_address, length) != length) {
        return false;
    }
    
    for (uint8_t i = 0; i < length; i++) {
        buffer[i] = _wire->read();
    }
    return true;
}