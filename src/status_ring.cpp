#include "status_ring.h"
#include <Adafruit_NeoPixel.h>
#include "config.h"

namespace {
Adafruit_NeoPixel ring(RING_PIXELS, PIN_RING, NEO_GRB + NEO_KHZ800);
AssistantState current = AssistantState::Booting;
uint32_t lastFrame = 0;
uint16_t tick = 0;

// 0..255..0 triangle wave, used for "breathing" effects
uint8_t breathe(uint16_t t, uint8_t period) {
  uint16_t p = t % (period * 2);
  return (p < period) ? (p * 255 / period) : ((period * 2 - p) * 255 / period);
}

uint32_t scale(uint8_t r, uint8_t g, uint8_t b, uint8_t level) {
  return ring.Color(r * level / 255, g * level / 255, b * level / 255);
}

void fill(uint32_t c) {
  for (uint16_t i = 0; i < RING_PIXELS; i++) ring.setPixelColor(i, c);
}

void spinner(uint8_t r, uint8_t g, uint8_t b) {
  ring.clear();
  uint16_t head = tick % RING_PIXELS;
  for (uint8_t tail = 0; tail < 4; tail++) {
    uint16_t idx = (head + RING_PIXELS - tail) % RING_PIXELS;
    ring.setPixelColor(idx, scale(r, g, b, 255 >> tail));
  }
}

void renderFrame() {
  switch (current) {
    case AssistantState::Booting:   spinner(255, 255, 255); break;
    case AssistantState::Idle:      fill(scale(0, 40, 120, 40 + breathe(tick, 40) / 4)); break;
    case AssistantState::Listening: fill(scale(0, 200, 255, 255)); break;
    case AssistantState::Thinking:  spinner(160, 0, 255); break;
    case AssistantState::Speaking:  fill(scale(0, 255, 80, 80 + breathe(tick, 8) * 2 / 3)); break;
    case AssistantState::Muted:     fill(scale(255, 0, 0, 60)); break;
    case AssistantState::Error:     fill((tick / 5) % 2 ? scale(255, 0, 0, 255) : 0); break;
    case AssistantState::Offline:   ring.clear(); ring.setPixelColor(0, scale(255, 120, 0, 120)); break;
  }
  ring.show();
}
}  // namespace

namespace StatusRing {
void begin() {
  ring.begin();
  ring.setBrightness(RING_BRIGHTNESS);
  ring.clear();
  ring.show();
}

void setState(AssistantState s) {
  if (s == current) return;
  current = s;
  tick = 0;
  renderFrame();  // react immediately, don't wait for next frame
}

void update() {
  const uint32_t now = millis();
  if (now - lastFrame < 50) return;  // ~20 FPS is enough and keeps CPU free
  lastFrame = now;
  tick++;
  renderFrame();
}
}  // namespace StatusRing
