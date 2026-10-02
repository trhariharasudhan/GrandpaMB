// ============================================================
//  GrandpaMB v0 — Grandpa edge node firmware
//
//  Role: the physical "body" of Grandpa. The brain (LLM, memory,
//  tools) stays on the PC / Pironman. This board:
//    - shows assistant state (LED ring + OLED)
//    - sends button presses + room sensor data to Grandpa core
//    - executes small physical actions (beep, LED, relay)
//      with confirmation for risky ones.
// ============================================================
#include <Arduino.h>
#include "config.h"
#include "app.h"
#include "status_ring.h"
#include "display.h"
#include "sensors.h"
#include "buttons.h"
#include "actuators.h"
#include "protocol.h"

namespace {
Button btnWake(PIN_BTN_WAKE);
Button btnMute(PIN_BTN_MUTE);

AssistantState appState = AssistantState::Booting;
AssistantState stateBeforeMute = AssistantState::Idle;
uint32_t lastHostMsg = 0;
bool hostEverSeen = false;
uint32_t lastTelemetry = 0;
}  // namespace

// ---------------- App facade (used by protocol.cpp) ----------------
namespace App {
void setState(AssistantState s) {
  // While muted, only an explicit unmute (button) or error may change state.
  if (appState == AssistantState::Muted && s != AssistantState::Error && s != AssistantState::Muted) return;
  appState = s;
  StatusRing::setState(s);
  Display::setState(s);
}
AssistantState state() { return appState; }
void setMessage(const char* text) { Display::setMessage(text); }
void hostSeen() {
  lastHostMsg = millis();
  if (!hostEverSeen || appState == AssistantState::Offline) {
    hostEverSeen = true;
    if (appState == AssistantState::Offline || appState == AssistantState::Booting) {
      setState(AssistantState::Idle);
      Display::setMessage("Grandpa connected");
    }
  }
}
}  // namespace App

// ---------------- local input handling ----------------
static void handleButtons() {
  switch (btnWake.poll()) {
    case ButtonEvent::Press:
      if (appState == AssistantState::Muted) {
        Actuators::beep(40, 600);              // "you're muted" feedback
        Protocol::sendEvent("wake_ignored", "muted");
        break;
      }
      App::setState(AssistantState::Listening); // instant local feedback; host decides what's next
      Actuators::beep(60, 2400);
      Protocol::sendEvent("wake");
      break;
    case ButtonEvent::LongPress:
      Protocol::sendEvent("wake_long");         // e.g. "cancel" / "stop speaking" in Grandpa core
      break;
    default: break;
  }

  switch (btnMute.poll()) {
    case ButtonEvent::Press:
      if (appState == AssistantState::Muted) {
        appState = stateBeforeMute;             // bypass the mute guard in App::setState
        App::setState(hostEverSeen ? AssistantState::Idle : AssistantState::Offline);
        Protocol::sendEvent("unmuted");
      } else {
        stateBeforeMute = appState;
        App::setState(AssistantState::Muted);
        Protocol::sendEvent("muted");
      }
      break;
    case ButtonEvent::LongPress:
      // Physical kill switch: always allowed, no confirmation needed (it makes things SAFER).
      Actuators::setRelay(false);
      Display::setRelay(false);
      Display::setMessage("Relay forced OFF");
      Protocol::sendEvent("relay_forced_off");
      break;
    default: break;
  }
}

static void handleHostTimeout() {
  if (!hostEverSeen) return;
  if (appState == AssistantState::Muted || appState == AssistantState::Offline) return;
  if (millis() - lastHostMsg > HOST_TIMEOUT_MS) {
    App::setState(AssistantState::Offline);
    Display::setMessage("Grandpa core lost");
  }
}

void setup() {
  Serial.begin(GMB_SERIAL_BAUD);
  delay(50);

  Actuators::begin();       // first: make sure relay is OFF as early as possible
  StatusRing::begin();
  StatusRing::setState(AssistantState::Booting);
  bool oledOk = Display::begin();
  Sensors::begin();
  btnWake.begin();
  btnMute.begin();
  Protocol::begin(Serial);

  Protocol::sendHello();
  if (!oledOk) Protocol::sendEvent("hw_warning", "oled_not_found");

  Actuators::beep(80, 1800);
  App::setState(AssistantState::Offline);  // waiting for Grandpa core
  Display::setMessage("Waiting for Grandpa");
}

void loop() {
  Protocol::update();
  handleButtons();
  Sensors::update();
  handleHostTimeout();

  if (Sensors::motionChanged()) {
    Protocol::sendEvent(Sensors::read().motion ? "motion_start" : "motion_end");
  }

  const uint32_t now = millis();
  if (now - lastTelemetry >= TELEMETRY_INTERVAL_MS) {
    lastTelemetry = now;
    Display::setSensors(Sensors::read());
    Protocol::sendTelemetry(Sensors::read());
  }

  StatusRing::update();
  Display::update();
  Actuators::update();
}
