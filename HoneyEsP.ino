// HoneyEsP.ino

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>

// Sensor Configuration
#define DHTPIN 4          // DHT sensor pin
#define DHTTYPE DHT11     // DHT 11
DHT dht(DHTPIN, DHTTYPE);

// WiFi Configuration
const char* ssid = "your_SSID";
const char* password = "your_PASSWORD";

// API Endpoint
const char* apiEndpoint = "https://api.yourservice.com/honeydata";

// Display Configuration
TFT_eSPI tft = TFT_eSPI(); // Invoke library

void setup() {
  Serial.begin(115200);
  dht.begin();

  // Initialize TFT
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi!");
}

void loop() {
  delay(2000);
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Display Sensor Data
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.printf("Temp: %.1f C\n", t);
  tft.printf("Humidity: %.1f %%\n", h);

  // Send data to server
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(apiEndpoint);
    http.addHeader("Content-Type", "application/json");
    String jsonData = "{"; 
    jsonData += \"temperature\": \"" + String(t) + "\", ";
    jsonData += \"humidity\": \"" + String(h) + "\"}";
    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.println(httpResponseCode);
      Serial.println(payload);
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}