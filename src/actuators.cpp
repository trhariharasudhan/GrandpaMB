#include "actuators.h"
#include "config.h"

namespace {
bool relayState = false;
uint32_t beepUntil = 0;
bool beeping = false;
}  // namespace

namespace Actuators {
void begin() {
  pinMode(PIN_RELAY, OUTPUT);
  setRelay(false);                // fail-safe: load is OFF after every reset
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);
  pinMode(PIN_BUZZER, OUTPUT);
  noTone(PIN_BUZZER);
}

void update() {
  if (beeping && (int32_t)(millis() - beepUntil) >= 0) {
    noTone(PIN_BUZZER);
    beeping = false;
  }
}

void setRelay(bool on) {
  relayState = on;
  digitalWrite(PIN_RELAY, (on == (RELAY_ACTIVE_HIGH == 1)) ? HIGH : LOW);
}

bool relayOn() { return relayState; }

void beep(uint16_t ms, uint16_t freqHz) {
  ms = constrain(ms, 10, 2000);   // never let a host command lock the buzzer on
  tone(PIN_BUZZER, freqHz);
  beepUntil = millis() + ms;
  beeping = true;
}

void setStatusLed(bool on) { digitalWrite(PIN_STATUS_LED, on ? HIGH : LOW); }
}  // namespace Actuators
