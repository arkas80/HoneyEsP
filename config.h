// HoneyEsP Configuration for ESP32-S3

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 320

// Pin definitions for TFT display
#define TFT_CS 15
#define TFT_RST 4
#define TFT_DC 2

// Pin definitions for HX711 weight sensor
#define HX711_DOUT 3
#define HX711_CLK 5

// Pin definitions for BME280 environmental sensor
#define BME280_I2C_ADDRESS 0x76

// Schedule settings
#define SCHEDULE_INTERVAL 60000 // Interval in ms

// Thingspeak API settings
const char *thingspeak_api_key = "YOUR_API_KEY";

// WhatsApp settings
const char *whatsapp_number = "+1234567890";
const char *whatsapp_message = "Hello from HoneyEsP!";

// Feature flags
#define FEATURE_ENABLE_WEIGHT_SENSOR 1
#define FEATURE_ENABLE_ENVIRONMENTAL_SENSOR 1
#define FEATURE_ENABLE_NOTIFY_WHATSAPP 0
