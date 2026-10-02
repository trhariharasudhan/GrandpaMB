#pragma once
#include "assistant_state.h"

// Small facade so protocol.cpp can change app state without knowing
// about the ring / display modules directly. Implemented in main.cpp.
namespace App {
void setState(AssistantState s);
AssistantState state();
void setMessage(const char* text);
void hostSeen();          // any valid host message counts as a heartbeat
}
