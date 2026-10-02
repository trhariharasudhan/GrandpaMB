#pragma once
#include "assistant_state.h"

// Non-blocking LED ring animations (one per AssistantState).
namespace StatusRing {
void begin();
void setState(AssistantState s);
void update();   // call every loop()
}
