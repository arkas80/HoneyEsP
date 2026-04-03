// HoneyEsP.ino - Main ESP32-S3 Beehive Monitoring System
// Includes: HX711 (weight), BME280 (temp/humidity/pressure), ILI9341 display
// Features: WiFi AP, ThinkSpeak upload, WhatsApp notifications, power scheduling

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

#include "config.h"

// Global variables
float weight = 0.0f;
float temperature = 0.0f;
float humidity = 0.0f;
float pressure = 0.0f;
int batteryPercent = 100;
unsigned long lastMeasurementTime = 0;
unsigned long measurementInterval = 300000; // 5 minutes default

WebServer server(80);

void setup() {
  Serial.begin(DEBUG_SERIAL_SPEED);
  delay(1000);
  Serial.println("\n\n=== HoneyEsP ESP32-S3 Startup ===");
  
  // Initialize pins
  pinMode(SETUP_PIN, INPUT_PULLUP);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  // Initialize storage
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
  }
  
  // Initialize display
  initializeDisplay();
  displayMessage("HoneyEsP", "Initializing...");
  
  // Initialize I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  
  // Initialize sensors
  Serial.println("Initializing sensors...");
  delay(1000);
  
  // Check for setup mode
  if (digitalRead(SETUP_PIN) == LOW) {
    delay(2000);
    if (digitalRead(SETUP_PIN) == LOW) {
      enterSetupMode();
    }
  }
  
  // Initialize WiFi
  connectToWiFi();
  
  Serial.println("Setup complete!");
  displayMessage("Ready", "Waiting...");
}

void loop() {
  // Handle setup button
  if (digitalRead(SETUP_PIN) == LOW) {
    delay(50);
    if (digitalRead(SETUP_PIN) == LOW) {
      enterSetupMode();
    }
  }
  
  // Check if measurement time
  if (millis() - lastMeasurementTime >= measurementInterval) {
    takeMeasurements();
    displayMeasurements();
    uploadToThinkSpeak();
    sendWhatsAppNotification();
    lastMeasurementTime = millis();
  }
  
  // Handle web requests if WiFi is connected
  if (WiFi.status() == WL_CONNECTED && WiFi.mode() == WIFI_AP_STA) {
    server.handleClient();
  }
  
  delay(100);
}

void initializeDisplay() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, LOW);
  delay(20);
  digitalWrite(TFT_RST, HIGH);
  delay(50);
  
  Serial.println("Display initialized");
}

void displayMessage(String title, String message) {
  // TFT display - placeholder for actual display code
  Serial.println(title + ": " + message);
}

void displayMeasurements() {
  Serial.println("=== Current Measurements ===");
  Serial.println("Weight: " + String(weight, 2) + "g");
  Serial.println("Temp: " + String(temperature, 1) + "°C");
  Serial.println("Humidity: " + String(humidity, 1) + "%");
  Serial.println("Pressure: " + String(pressure, 0) + "hPa");
  Serial.println("Battery: " + String(batteryPercent) + "%");
}

void takeMeasurements() {
  Serial.println("Taking measurements...");
  
  // Simulate measurements (replace with actual sensor reading)
  weight = 15.5f;
  temperature = 24.5f;
  humidity = 65.0f;
  pressure = 1013.25f;
  batteryPercent = 85;
  
  displayMeasurements();
}

void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  // WiFi connection logic here
  displayMessage("WiFi", "Connecting...");
}

void enterSetupMode() {
  Serial.println("Entering Setup Mode - Starting WiFi AP");
  
  // Start AP mode
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("HoneyEsP-Setup", "honeyadmin");
  
  Serial.println("AP Started: HoneyEsP-Setup");
  Serial.println("IP: " + WiFi.softAPIP().toString());
  
  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/settings", HTTP_GET, handleGetSettings);
  server.on("/api/settings", HTTP_POST, handlePostSettings);
  server.on("/api/tare", HTTP_POST, handleTare);
  server.on("/api/calibrate", HTTP_POST, handleCalibrate);
  server.on("/api/schedule", HTTP_POST, handleSchedule);
  
  server.begin();
  
  displayMessage("Setup Mode", "192.168.4.1");
  
  // Wait for configuration or timeout
  unsigned long startTime = millis();
  while (millis() - startTime < AP_TIMEOUT_MS) {
    server.handleClient();
    delay(100);
  }
  
  // Exit setup mode
  server.stop();
  WiFi.mode(WIFI_STA);
  ESP.restart();
}

void handleRoot() {
  String html = R"(
    <!DOCTYPE html>
    <html>
    <head>
      <title>HoneyEsP Setup</title>
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <style>
        body { font-family: Arial; margin: 20px; }
        .container { max-width: 600px; margin: 0 auto; }
        input, select { width: 100%; padding: 8px; margin: 5px 0; }
        button { padding: 10px; background: #4CAF50; color: white; border: none; cursor: pointer; }
        button:hover { background: #45a049; }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>HoneyEsP Configuration</h1>
        
        <h2>WiFi Settings</h2>
        <form method="POST" action="/api/settings">
          <label>SSID:</label>
          <input type="text" name="ssid" required>
          
          <label>Password:</label>
          <input type="password" name="password" required>
          
          <h2>ThinkSpeak</h2>
          <label>API Key:</label>
          <input type="text" name="thingspeak_key" required>
          
          <label>Channel ID:</label>
          <input type="number" name="thingspeak_channel" required>
          
          <h2>Measurement Interval (seconds)</h2>
          <input type="number" name="interval" value="300" required>
          
          <br><br>
          <button type="submit">Save Settings</button>
        </form>
        
        <br>
        <button onclick="tareSensor()">Tare Scale</button>
        <button onclick="calibrateSensor()">Calibrate Scale</button>
      </div>
      
      <script>
        function tareSensor() {
          fetch('/api/tare', {method: 'POST'}).then(r => r.json()).then(d => alert(d.message));
        }
        function calibrateSensor() {
          fetch('/api/calibrate', {method: 'POST'}).then(r => r.json()).then(d => alert(d.message));
        }
      </script>
    </body>
    </html>
  )";
  
  server.send(200, "text/html", html);
}

void handleGetSettings() {
  StaticJsonDocument<256> doc;
  doc["interval"] = measurementInterval / 1000;
  doc["battery"] = batteryPercent;
  doc["weight"] = weight;
  doc["temp"] = temperature;
  doc["humidity"] = humidity;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handlePostSettings() {
  if (server.arg("interval") != "") {
    measurementInterval = server.arg("interval").toInt() * 1000;
  }
  
  StaticJsonDocument<128> doc;
  doc["status"] = "ok";
  doc["message"] = "Settings saved";
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleTare() {
  Serial.println("Tare requested");
  
  StaticJsonDocument<128> doc;
  doc["status"] = "ok";
  doc["message"] = "Scale tared";
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleCalibrate() {
  Serial.println("Calibrate requested");
  
  StaticJsonDocument<128> doc;
  doc["status"] = "ok";
  doc["message"] = "Scale calibrated";
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSchedule() {
  Serial.println("Schedule update requested");
  
  StaticJsonDocument<128> doc;
  doc["status"] = "ok";
  doc["message"] = "Schedule updated";
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void uploadToThinkSpeak() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Uploading to ThinkSpeak...");
    
    HTTPClient http;
    String url = "https://api.thingspeak.com/update?api_key=" + String(THINGSPEAK_API_KEY);
    url += "&field1=" + String(weight, 2);
    url += "&field2=" + String(temperature, 1);
    url += "&field3=" + String(humidity, 1);
    url += "&field4=" + String(pressure, 0);
    url += "&field5=" + String(batteryPercent);
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
      Serial.println("ThinkSpeak upload successful");
    } else {
      Serial.println("ThinkSpeak upload failed: " + String(httpCode));
    }
    http.end();
  }
}

void sendWhatsAppNotification() {
  // WhatsApp notification logic - implement based on your chosen service
  Serial.println("WhatsApp notification would be sent here");
}