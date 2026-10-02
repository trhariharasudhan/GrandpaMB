#pragma once
#include <Arduino.h>

enum class ButtonEvent : uint8_t { None, Press, LongPress };

// Debounced active-LOW push button. Non-blocking: call poll() every loop().
class Button {
 public:
  explicit Button(uint8_t pin) : pin_(pin) {}
  void begin();
  ButtonEvent poll();

 private:
  uint8_t  pin_;
  bool     stable_ = false;      // debounced "is pressed"
  bool     lastRaw_ = false;
  bool     longFired_ = false;
  uint32_t changedAt_ = 0;
  uint32_t pressedAt_ = 0;
};
