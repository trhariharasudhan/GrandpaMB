#include "actuators.h"
#include "config.h"

namespace {
// Buzzer runs on a dedicated LEDC channel that we set up ourselves.
// (Arduino tone()/noTone() log "LEDC is not initialized" if noTone() runs before tone().)
constexpr uint8_t BUZZER_LEDC_CH = 0;
constexpr uint8_t BUZZER_LEDC_RES = 10;  // bits

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
  ledcSetup(BUZZER_LEDC_CH, 2000, BUZZER_LEDC_RES);
  ledcAttachPin(PIN_BUZZER, BUZZER_LEDC_CH);
  ledcWriteTone(BUZZER_LEDC_CH, 0);   // silent
}

void update() {
  if (beeping && (int32_t)(millis() - beepUntil) >= 0) {
    ledcWriteTone(BUZZER_LEDC_CH, 0);
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
  ledcWriteTone(BUZZER_LEDC_CH, freqHz);
  beepUntil = millis() + ms;
  beeping = true;
}

void setStatusLed(bool on) { digitalWrite(PIN_STATUS_LED, on ? HIGH : LOW); }
}  // namespace Actuators
