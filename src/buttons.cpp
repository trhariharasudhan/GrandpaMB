#include "buttons.h"
#include "config.h"

void Button::begin() {
  pinMode(pin_, INPUT_PULLUP);
}

ButtonEvent Button::poll() {
  const uint32_t now = millis();
  const bool raw = digitalRead(pin_) == LOW;  // pressed pulls to GND

  if (raw != lastRaw_) {          // contact bounce or real change: restart timer
    lastRaw_ = raw;
    changedAt_ = now;
  }
  if (now - changedAt_ < BTN_DEBOUNCE_MS) return ButtonEvent::None;

  if (raw != stable_) {
    stable_ = raw;
    if (stable_) {                // pressed down
      pressedAt_ = now;
      longFired_ = false;
    } else if (!longFired_) {     // released before long-press threshold
      return ButtonEvent::Press;
    }
  }

  if (stable_ && !longFired_ && now - pressedAt_ >= BTN_LONG_MS) {
    longFired_ = true;
    return ButtonEvent::LongPress;
  }
  return ButtonEvent::None;
}
