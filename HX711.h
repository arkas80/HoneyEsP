#ifndef HX711_H
#define HX711_H

#include <Arduino.h>
#include <vector>

class HX711 {
private:
    byte PD_SCK_PIN;
    byte DOUT_PIN;
    byte GAIN;
    long OFFSET;
    float SCALE;
    bool DEBUG_MODE;
    int numDataFilteredOut;
    
    // Temperature compensation
    float tempCompensationFactor;
    float referenceTemp;
    float currentTemperature;
    
    // HoneyPi settings
    float filterOutlierThreshold;
    byte minSamplesForFilter;
    
    // Helper functions (HoneyPi style)
    float find_max(std::vector<float>& arr);
    float find_min(std::vector<float>& arr);
    float take_closest(std::vector<float>& arr, float target);
    float average(std::vector<float>& arr);
    
public:
    HX711();
    
    // Basic functions
    void begin(byte dout, byte pd_sck, byte gain = 128);
    bool is_ready();
    void set_gain(byte gain = 128);
    long read();
    long read_average(byte times = 10);
    double get_value(byte times = 10);
    float get_units(byte times = 10);
    
    // HoneyPi-style robust filtering
    float get_robust_units(byte times = 15);
    float get_weight_mean(int readings = 30);
    int get_num_data_filtered_out();
    
    // HoneyPi stability metrics
    float get_stability_metric(byte times = 10);
    bool is_stable(byte times = 10, float threshold = 0.1f);
    
    // HoneyPi advanced measurement (πλήρης μεταφορά)
    float measure_weight_honeypi(int reference_unit = 1, int offset = 0, bool invert = false, bool filter_negative = false);
    float easy_weight(byte dout_pin, byte sck_pin, char channel, float reference_unit, long offset);
    
    // HoneyPi initialization with retry
    bool init_hx711(byte dout, byte pd_sck, char channel, bool debug = false);
    
    // Tare and calibration (HoneyPi style)
    void tare(byte times = 15);
    bool auto_calibrate(float knownWeight, float temperature = 20.0f);
    
    // Scale and offset management
    void set_scale(float scale = 1.0f);
    float get_scale();
    void set_offset(long offset = 0);
    long get_offset();
    void set_scale_ratio(float scale_ratio);
    float get_scale_ratio();
    
    // Power management
    void power_down();
    void power_up();
    bool reset();
    
    // Debug and utilities
    void set_debug_mode(bool flag = false);
    long get_raw_data_mean(int readings = 30);
    void set_channel(char channel);
    
    // Temperature compensation (HoneyPi style)
    void set_temperature_compensation(float factor = -0.05f, float refTemp = 20.0f);
    float get_compensated_units(float temperature, byte times = 10);
    float get_compensated_weight(float rawWeight, float temperature);
    float compensate_temperature(float weight, float temperature, float compensation_temp, float compensation_value);
    void set_temperature(float temp);
    float get_current_temperature();
    float get_compensation_factor();
    
    // Filter configuration
    void set_filter_threshold(float threshold);
    float get_filter_threshold();
    
    // Statistics
    float get_percentage_filtered_out(int total_samples);
};

#endif
