#include "protocol.h"
#include <ArduinoJson.h>
#include "config.h"
#include "app.h"
#include "actuators.h"
#include "display.h"

namespace {
Stream* io = nullptr;
char lineBuf[CMD_MAX_LINE];
size_t lineLen = 0;
bool lineOverflow = false;

// ---- pending confirmation for risky actions ----
struct Pending {
  bool active = false;
  char token[8] = {0};
  char action[16] = {0};      // only "relay_on" today; add more here
  uint32_t createdAt = 0;
} pending;

void send(JsonDocument& doc) {
  if (!io) return;
  serializeJson(doc, *io);
  io->println();
}

void reply(JsonVariantConst id, bool ok, const char* msg = nullptr) {
  JsonDocument d;
  d["type"] = "reply";
  if (!id.isNull()) d["id"] = id;
  d["ok"] = ok;
  if (msg) d[ok ? "msg" : "error"] = msg;
  send(d);
}

void newToken(char* out, size_t len) {
  snprintf(out, len, "%06lX", (unsigned long)(esp_random() & 0xFFFFFF));
}

// ---- command handlers ----------------------------------------------
void cmdHelp(JsonVariantConst id) {
  JsonDocument d;
  d["type"] = "reply";
  if (!id.isNull()) d["id"] = id;
  d["ok"] = true;
  JsonArray c = d["commands"].to<JsonArray>();
  for (const char* n : {"ping", "info", "help", "telemetry", "state <idle|listening|thinking|speaking|muted|error>",
                        "say <text>", "beep [ms]", "led <on|off>", "relay <on|off>", "confirm <token>", "cancel"})
    c.add(n);
  send(d);
}

void cmdInfo(JsonVariantConst id) {
  JsonDocument d;
  d["type"] = "reply";
  if (!id.isNull()) d["id"] = id;
  d["ok"] = true;
  d["device"] = GMB_DEVICE_NAME;
  d["fw"] = GMB_FW_VERSION;
  d["uptime_ms"] = millis();
  d["state"] = stateToString(App::state());
  d["relay"] = Actuators::relayOn();
  d["free_heap"] = ESP.getFreeHeap();
  send(d);
}

void cmdRelay(JsonVariantConst id, const char* value) {
  if (!value) return reply(id, false, "relay needs value on|off");

  if (strcasecmp(value, "off") == 0) {          // turning OFF is the safe direction: no confirm
    Actuators::setRelay(false);
    Display::setRelay(false);
    pending.active = false;
    return reply(id, true, "relay off");
  }
  if (strcasecmp(value, "on") != 0) return reply(id, false, "relay value must be on|off");

  // Risky: ask for confirmation instead of acting.
  pending.active = true;
  newToken(pending.token, sizeof(pending.token));
  strlcpy(pending.action, "relay_on", sizeof(pending.action));
  pending.createdAt = millis();

  JsonDocument d;
  d["type"] = "reply";
  if (!id.isNull()) d["id"] = id;
  d["ok"] = true;
  d["confirm_required"] = true;
  d["action"] = pending.action;
  d["token"] = pending.token;
  d["expires_ms"] = CONFIRM_TIMEOUT_MS;
  send(d);
  Display::setMessage("Confirm relay ON?");
  Actuators::beep(60, 1500);
}

void cmdConfirm(JsonVariantConst id, const char* token) {
  if (!pending.active) return reply(id, false, "nothing to confirm");
  if (millis() - pending.createdAt > CONFIRM_TIMEOUT_MS) {
    pending.active = false;
    return reply(id, false, "confirmation expired");
  }
  if (!token || strcmp(token, pending.token) != 0) {
    pending.active = false;                     // one wrong guess cancels it
    return reply(id, false, "wrong token, action cancelled");
  }

  pending.active = false;
  if (strcmp(pending.action, "relay_on") == 0) {
    Actuators::setRelay(true);
    Display::setRelay(true);
    Display::setMessage("Relay ON");
    return reply(id, true, "relay on");
  }
  reply(id, false, "unknown pending action");
}

void handle(JsonDocument& doc) {
  JsonVariantConst id = doc["id"];
  const char* cmd = doc["cmd"];
  if (!cmd) return reply(id, false, "missing 'cmd'");

  App::hostSeen();

  if      (!strcasecmp(cmd, "ping"))      reply(id, true, "pong");
  else if (!strcasecmp(cmd, "help"))      cmdHelp(id);
  else if (!strcasecmp(cmd, "info"))      cmdInfo(id);
  else if (!strcasecmp(cmd, "telemetry")) { Protocol::sendTelemetry(Sensors::read()); reply(id, true); }
  else if (!strcasecmp(cmd, "state")) {
    AssistantState s;
    if (!stateFromString(doc["value"] | "", s)) return reply(id, false, "unknown state");
    App::setState(s);
    reply(id, true, stateToString(s));
  }
  else if (!strcasecmp(cmd, "say") || !strcasecmp(cmd, "display")) {
    App::setMessage(doc["text"] | "");
    reply(id, true);
  }
  else if (!strcasecmp(cmd, "beep")) {
    Actuators::beep(doc["ms"] | 120, doc["freq"] | 2000);
    reply(id, true);
  }
  else if (!strcasecmp(cmd, "led")) {
    const char* v = doc["value"] | "off";
    Actuators::setStatusLed(!strcasecmp(v, "on") || !strcasecmp(v, "true") || !strcasecmp(v, "1"));
    reply(id, true);
  }
  else if (!strcasecmp(cmd, "relay"))     cmdRelay(id, doc["value"]);
  else if (!strcasecmp(cmd, "confirm"))   cmdConfirm(id, doc["token"]);
  else if (!strcasecmp(cmd, "cancel"))    { pending.active = false; reply(id, true, "cancelled"); }
  else                                    reply(id, false, "unknown cmd (try: help)");
}

// "relay on" -> {"cmd":"relay","value":"on"}  (for humans typing in a serial monitor)
void parsePlainText(char* line, JsonDocument& doc) {
  char* rest = strchr(line, ' ');
  if (rest) { *rest++ = '\0'; while (*rest == ' ') rest++; }
  doc["cmd"] = line;
  if (!rest || !*rest) return;
  if (!strcasecmp(line, "say") || !strcasecmp(line, "display")) doc["text"] = rest;
  else if (!strcasecmp(line, "confirm"))                         doc["token"] = rest;
  else if (!strcasecmp(line, "beep"))                            doc["ms"] = atoi(rest);
  else                                                           doc["value"] = rest;
}

void processLine(char* line) {
  // trim
  while (*line == ' ' || *line == '\t') line++;
  size_t n = strlen(line);
  while (n && (line[n - 1] == ' ' || line[n - 1] == '\r')) line[--n] = '\0';
  if (!n) return;

  JsonDocument doc;
  if (line[0] == '{') {
    DeserializationError err = deserializeJson(doc, line);
    if (err) {
      JsonDocument e;
      e["type"] = "error";
      e["error"] = "bad json";
      e["detail"] = err.c_str();
      send(e);
      return;
    }
  } else {
    parsePlainText(line, doc);
  }
  handle(doc);
}
}  // namespace

namespace Protocol {
void begin(Stream& s) { io = &s; }

void update() {
  if (!io) return;

  // expire stale confirmations even if host never answers
  if (pending.active && millis() - pending.createdAt > CONFIRM_TIMEOUT_MS) {
    pending.active = false;
    sendEvent("confirm_expired");
    App::setMessage("Confirm timed out");
  }

  while (io->available()) {
    char c = (char)io->read();
    if (c == '\n') {
      lineBuf[lineLen] = '\0';
      if (lineOverflow) {
        JsonDocument e;
        e["type"] = "error";
        e["error"] = "line too long";
        send(e);
      } else {
        processLine(lineBuf);
      }
      lineLen = 0;
      lineOverflow = false;
    } else if (lineLen < CMD_MAX_LINE - 1) {
      lineBuf[lineLen++] = c;
    } else {
      lineOverflow = true;  // drop the rest of this line instead of executing a truncated command
    }
  }
}

void sendHello() {
  JsonDocument d;
  d["type"] = "hello";
  d["device"] = GMB_DEVICE_NAME;
  d["fw"] = GMB_FW_VERSION;
  send(d);
}

void sendEvent(const char* name, const char* detail) {
  JsonDocument d;
  d["type"] = "event";
  d["name"] = name;
  if (detail) d["detail"] = detail;
  d["t"] = millis();
  send(d);
}

void sendTelemetry(const SensorReadings& r) {
  JsonDocument d;
  d["type"] = "telemetry";
  if (r.dhtOk) {
    d["temp_c"] = serialized(String(r.temperatureC, 1));
    d["humidity"] = serialized(String(r.humidity, 1));
  } else {
    d["dht_error"] = true;
  }
  d["light_pct"] = r.lightPercent;
  d["motion"] = r.motion;
  d["relay"] = Actuators::relayOn();
  d["state"] = stateToString(App::state());
  send(d);
}
}  // namespace Protocol
