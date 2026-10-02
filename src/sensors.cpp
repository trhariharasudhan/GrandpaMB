#include "sensors.h"
#include <DHT.h>
#include "config.h"

namespace {
DHT dht(PIN_DHT, DHT22);
SensorReadings r;
uint32_t lastDht = 0;
bool lastMotion = false;
bool motionEdge = false;
}  // namespace

namespace Sensors {
void begin() {
  dht.begin();
  pinMode(PIN_PIR, INPUT);
  analogReadResolution(12);  // 0..4095
}

void update() {
  const uint32_t now = millis();

  // PIR is cheap -> read every loop so we never miss an edge
  r.motion = digitalRead(PIN_PIR) == HIGH;
  if (r.motion != lastMotion) {
    lastMotion = r.motion;
    motionEdge = true;
  }

  // DHT22 is slow (~2s minimum between reads)
  if (now - lastDht >= DHT_MIN_INTERVAL_MS || lastDht == 0) {
    lastDht = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    r.dhtOk = !(isnan(t) || isnan(h));
    if (r.dhtOk) { r.temperatureC = t; r.humidity = h; }

    // Wokwi LDR module: AO is HIGH voltage in the dark, LOW in bright light.
    // Invert so 100% = bright.
    int raw = analogRead(PIN_LDR);
    r.lightPercent = constrain(100 - (raw * 100 / 4095), 0, 100);
  }
}

const SensorReadings& read() { return r; }

bool motionChanged() {
  bool e = motionEdge;
  motionEdge = false;
  return e;
}
}  // namespace Sensors
