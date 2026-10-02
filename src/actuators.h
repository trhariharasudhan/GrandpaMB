#pragma once
#include <Arduino.h>

// Outputs that change the physical world.
// The relay is treated as a HIGH-RISK action: it can only be switched ON
// through the two-step confirm flow in protocol.cpp. It always boots OFF.
namespace Actuators {
void begin();
void update();                    // call every loop() (ends beeps)

void setRelay(bool on);
bool relayOn();

void beep(uint16_t ms, uint16_t freqHz = 2000);  // non-blocking
void setStatusLed(bool on);
}
