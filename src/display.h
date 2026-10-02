#pragma once
#include "assistant_state.h"
#include "sensors.h"

// OLED dashboard. Redraws only when something changed (I2C is slow).
namespace Display {
bool begin();                         // false if OLED not found -> board keeps working without it
void setState(AssistantState s);
void setMessage(const char* text);    // last message from Grandpa core
void setSensors(const SensorReadings& r);
void setRelay(bool on);
void update();                        // call every loop()
}
