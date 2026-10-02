#include "display.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

namespace {
Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
bool ok = false;
bool dirty = true;
uint32_t lastDraw = 0;

AssistantState state = AssistantState::Booting;
char message[64] = "Booting...";
SensorReadings readings{};
bool relayOn = false;

void draw() {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);

  // Header bar
  oled.fillRect(0, 0, OLED_WIDTH, 11, SSD1306_WHITE);
  oled.setTextColor(SSD1306_BLACK);
  oled.setCursor(2, 2);
  oled.print(GMB_DEVICE_NAME " v" GMB_FW_VERSION);
  oled.setTextColor(SSD1306_WHITE);

  // State
  oled.setCursor(0, 14);
  oled.print("State: ");
  oled.print(stateToString(state));

  // Sensors
  oled.setCursor(0, 26);
  if (readings.dhtOk) {
    oled.printf("%.1fC  %.0f%%RH", readings.temperatureC, readings.humidity);
  } else {
    oled.print("DHT: --");
  }
  oled.setCursor(0, 36);
  oled.printf("Light:%3d%% %s R:%s", readings.lightPercent,
              readings.motion ? "MOVE" : "    ", relayOn ? "ON" : "off");

  // Message (wraps to 2 lines max)
  oled.drawFastHLine(0, 46, OLED_WIDTH, SSD1306_WHITE);
  oled.setCursor(0, 49);
  oled.print(message);

  oled.display();
}
}  // namespace

namespace Display {
bool begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  ok = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (ok) draw();
  return ok;
}

void setState(AssistantState s)          { if (s != state) { state = s; dirty = true; } }
void setRelay(bool on)                   { if (on != relayOn) { relayOn = on; dirty = true; } }
void setSensors(const SensorReadings& r) { readings = r; dirty = true; }

void setMessage(const char* text) {
  strlcpy(message, text ? text : "", sizeof(message));
  dirty = true;
}

void update() {
  if (!ok || !dirty) return;
  const uint32_t now = millis();
  if (now - lastDraw < 100) return;  // max 10 redraws/sec
  lastDraw = now;
  dirty = false;
  draw();
}
}  // namespace Display
