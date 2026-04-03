#ifndef HX711_SENSOR_H
#define HX711_SENSOR_H

#include <Arduino.h>

class HX711_Sensor {
  public: 
    HX711_Sensor(int doutPin, int sckPin);
    void begin();
    void set_scale(float scale);
    float get_weight(int samples = 10);
    void tare(int samples = 10);
    void calibrate(int samples = 10);

  private: 
    int _doutPin;
    int _sckPin;
    float _scale;
    long _offset;
};

#endif
