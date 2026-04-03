#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

class PowerManager {
public:
    PowerManager();
    ~PowerManager();
    void turnOn();
    void turnOff();
    bool isPowerOn();
};

#endif // POWER_MANAGER_H
