#include "QuadEncoder.h"

#include <soc/gpio_reg.h>

namespace {
// Gray-code transition table indexed by (previous << 2) | current. Invalid
// transitions (both inputs changing at once, i.e. a missed edge) yield 0
// rather than guessing a direction.
const int8_t QUAD_TABLE[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

// Read the GPIO input registers directly rather than going through
// digitalRead(), to keep the handler short and free of any flash access.
//
// Note that this core build does not install the GPIO ISR service with
// ESP_INTR_FLAG_IRAM, so these interrupts are masked for the few milliseconds
// a LittleFS write holds the flash cache down. A click turned during a config
// save can therefore be missed -- harmless for a hand-turned knob, but it is
// why the encoder count is not treated as an authoritative position anywhere.
inline int IRAM_ATTR fastRead(uint8_t pin) {
  if (pin < 32) return (REG_READ(GPIO_IN_REG) >> pin) & 0x1;
  return (REG_READ(GPIO_IN1_REG) >> (pin - 32)) & 0x1;
}
}  // namespace

bool QuadEncoder::begin(uint8_t pinA, uint8_t pinB) {
  end();
  pinA_ = pinA;
  pinB_ = pinB;
  pinMode(pinA_, INPUT_PULLUP);
  pinMode(pinB_, INPUT_PULLUP);
  state_ = static_cast<uint8_t>((fastRead(pinA_) << 1) | fastRead(pinB_));
  count_ = 0;
  attachInterruptArg(digitalPinToInterrupt(pinA_), isrTrampoline, this, CHANGE);
  attachInterruptArg(digitalPinToInterrupt(pinB_), isrTrampoline, this, CHANGE);
  attached_ = true;
  return true;
}

void QuadEncoder::end() {
  if (!attached_) return;
  detachInterrupt(digitalPinToInterrupt(pinA_));
  detachInterrupt(digitalPinToInterrupt(pinB_));
  attached_ = false;
}

void IRAM_ATTR QuadEncoder::isrTrampoline(void *arg) {
  static_cast<QuadEncoder *>(arg)->handleEdge();
}

void IRAM_ATTR QuadEncoder::handleEdge() {
  const uint8_t now = static_cast<uint8_t>((fastRead(pinA_) << 1) | fastRead(pinB_));
  const uint8_t index = static_cast<uint8_t>((state_ << 2) | now);
  state_ = now;
  count_ += QUAD_TABLE[index & 0x0F];
}
