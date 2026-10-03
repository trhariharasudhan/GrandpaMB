#pragma once
// ============================================================
//  GrandpaMB v0 — pin map & constants
//  Board: ESP32-S3-DevKitC-1  (same pins used in diagram.json)
//  Change a pin here AND in diagram.json — keep them in sync.
// ============================================================

#define GMB_DEVICE_NAME   "GrandpaMB"
#define GMB_FW_VERSION    "0.1.0"
#define GMB_SERIAL_BAUD   115200

// ---- I2C OLED (SSD1306 128x64) ----
#define PIN_I2C_SDA       8
#define PIN_I2C_SCL       9
#define OLED_ADDR         0x3C
#define OLED_WIDTH        128
#define OLED_HEIGHT       64

// ---- Status LED ring (WS2812 / NeoPixel) ----
#define PIN_RING          5
#define RING_PIXELS       12
#define RING_BRIGHTNESS   150     // 0-255. 12 LEDs at full white = ~720mA; our colours stay well under USB's 500mA at 150

// ---- Buttons (active LOW, internal pull-up) ----
#define PIN_BTN_WAKE      4
#define PIN_BTN_MUTE      6
#define BTN_DEBOUNCE_MS   30
#define BTN_LONG_MS       1500

// ---- Sensors ----
#define PIN_DHT           15
#define PIN_PIR           16
#define PIN_LDR           1       // ADC1_CH0 (ADC2 pins are unusable while WiFi is on)

// ---- Actuators ----
#define PIN_RELAY         17
#define PIN_BUZZER        18
#define PIN_STATUS_LED    2
#define RELAY_ACTIVE_HIGH 1

// ---- Timing ----
#define TELEMETRY_INTERVAL_MS   5000
#define DHT_MIN_INTERVAL_MS     2000   // DHT22 cannot be read faster than this
#define CONFIRM_TIMEOUT_MS      10000  // risky command must be confirmed within this window
#define HOST_TIMEOUT_MS         30000  // no message from host -> show "offline"

// ---- Protocol ----
#define CMD_MAX_LINE      256
