#ifndef DS18B20_SENSOR_H
#define DS18B20_SENSOR_H

#include <OneWire.h>
#include <DallasTemperature.h>

class DS18B20_Sensor {
public:  
    DS18B20_Sensor(int pin);
    void begin();
    float readTemperature();

private:  
    OneWire* oneWire;
    DallasTemperature* sensors;
    int pin;
};

#endif
