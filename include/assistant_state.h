#pragma once
#include <Arduino.h>

// Visual state of the assistant. Grandpa core (PC) drives this over serial;
// the board also changes it locally (wake button, mute button).
enum class AssistantState : uint8_t {
  Booting,
  Idle,
  Listening,
  Thinking,
  Speaking,
  Muted,
  Error,
  Offline,
};

const char* stateToString(AssistantState s);
bool stateFromString(const char* str, AssistantState& out);
