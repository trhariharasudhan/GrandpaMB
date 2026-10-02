#pragma once
#include <Arduino.h>

struct SensorReadings {
  bool  dhtOk = false;
  float temperatureC = NAN;
  float humidity = NAN;
  int   lightPercent = 0;   // 0 = dark, 100 = bright
  bool  motion = false;
};

// Room awareness: temperature, humidity, light, presence.
namespace Sensors {
void begin();
void update();                       // call every loop(); rate-limits itself
const SensorReadings& read();
bool motionChanged();                // true once per PIR edge (for events)
}
