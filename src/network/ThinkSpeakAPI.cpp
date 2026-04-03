// ThinkSpeakAPI.cpp

#include <WiFi.h>
#include <HTTPClient.h>

class ThinkSpeakAPI {
private:
    const char* apiKey;
    const char* server = "api.thingspeak.com";

public:
    ThinkSpeakAPI(const char* key) {
        apiKey = key;
    }

    void sendData(float field1, float field2) {
        if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            String url = String("https://") + server + "/update?api_key=" + apiKey;
            url += "&field1=" + String(field1);
            url += "&field2=" + String(field2);

            http.begin(url);
            int httpResponseCode = http.GET();
            if (httpResponseCode > 0) {
                String response = http.getString();
                Serial.println(httpResponseCode);
                Serial.println(response);
            } else {
                Serial.print("Error on sending GET: ");
                Serial.println(httpResponseCode);
            }
            http.end();
        } else {
            Serial.println("WiFi not connected");
        }
    }
};

// Example usage:
// ThinkSpeakAPI ts("YOUR_API_KEY");
// ts.sendData(23.5, 45.0); // Send example data
