// HX711 sensor implementation
#include <HX711.h>

class HX711_Sensor {
public:
    HX711_Sensor(int LOADCELL_DOUT_PIN, int LOADCELL_SCK_PIN) {
        hx711 = HX711(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
        hx711.set_scale();    // set the scale
        hx711.tare();         // reset the scale to 0
    }

    float getWeight() {
        return hx711.get_units(10); // get weight
    }

private:
    HX711 hx711;
};

