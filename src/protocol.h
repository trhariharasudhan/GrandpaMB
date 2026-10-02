#pragma once
#include <Arduino.h>
#include "sensors.h"

// Line-based protocol between GrandpaMB and Grandpa core (PC).
//
// Host -> board : one JSON object per line, e.g.
//     {"id":1,"cmd":"state","value":"thinking"}
//   or plain text for manual testing in a serial monitor:
//     state thinking
//
// Board -> host : one JSON object per line with "type":
//     "hello" | "reply" | "event" | "telemetry" | "error"
namespace Protocol {
void begin(Stream& io);
void update();                                   // read + handle incoming lines

void sendHello();
void sendEvent(const char* name, const char* detail = nullptr);
void sendTelemetry(const SensorReadings& r);
}
