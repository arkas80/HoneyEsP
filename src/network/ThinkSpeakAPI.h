// src/network/ThinkSpeakAPI.h

#ifndef THINGSPEAK_API_H
#define THINGSPEAK_API_H

#include <Arduino.h>
#include <HTTPClient.h>

class ThinkSpeakAPI {
private:
    String apiKey;
    String host;
    unsigned long channelID;

public:
    ThinkSpeakAPI(String key, unsigned long channel);
    
    bool uploadData(float field1, float field2, float field3, float field4, float field5);
    bool uploadValue(int fieldNumber, float value);
};

#endif