// src/sensors/BME280_Sensor.h

#ifndef BME280_SENSOR_H
#define BME280_SENSOR_H

#include <Arduino.h>
#include <Wire.h>

class BME280_Sensor {
private:
    uint8_t _address;
    TwoWire *_wire;
    
    uint16_t dig_T1;
    int16_t dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    uint8_t dig_H1, dig_H3;
    int16_t dig_H2, dig_H4, dig_H5, dig_H6;
    
    int32_t t_fine;
    float seaLevelPressure;
    
    bool readCalibrationData();
    uint8_t readRegister(uint8_t reg);
    void writeRegister(uint8_t reg, uint8_t value);
    bool readRegisters(uint8_t reg, uint8_t* buffer, uint8_t length);
    
    float compensateTemperature(int32_t adc_temp);
    float compensatePressure(int32_t adc_press);
    float compensateHumidity(int32_t adc_hum);

public:
    BME280_Sensor(uint8_t address = 0x76);
    
    bool begin(TwoWire *wire = &Wire);
    bool isConnected();
    
    bool readAll(float* temperature, float* pressure, float* humidity);
    float readTemperature();
    float readPressure();
    float readHumidity();
    float readAltitude(float seaLevel = 0);
    
    void setSeaLevelPressure(float pressure);
    float getSeaLevelPressure();
};

#endif