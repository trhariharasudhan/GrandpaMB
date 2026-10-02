#include "assistant_state.h"

namespace {
struct Entry { AssistantState state; const char* name; };
const Entry kStates[] = {
  {AssistantState::Booting,   "booting"},
  {AssistantState::Idle,      "idle"},
  {AssistantState::Listening, "listening"},
  {AssistantState::Thinking,  "thinking"},
  {AssistantState::Speaking,  "speaking"},
  {AssistantState::Muted,     "muted"},
  {AssistantState::Error,     "error"},
  {AssistantState::Offline,   "offline"},
};
}  // namespace

const char* stateToString(AssistantState s) {
  for (const auto& e : kStates) if (e.state == s) return e.name;
  return "unknown";
}

bool stateFromString(const char* str, AssistantState& out) {
  if (!str) return false;
  for (const auto& e : kStates) {
    if (strcasecmp(str, e.name) == 0) { out = e.state; return true; }
  }
  return false;
}
