#pragma once
// Debounced, non-blocking push-button reader with short/long-press
// discrimination on release. Shared by all 3 encoder push buttons (slide:
// BLE resync/record toggle; pan/tilt: recenter to 0deg on short press).
//
// Unchanged from the final rig's DebouncedButton.

#include <Arduino.h>

#include "config.h"

class DebouncedButton {
 public:
  enum class Event : uint8_t { NONE, SHORT_PRESS, LONG_PRESS };

  void begin(uint8_t pin) {
    pin_ = pin;
    pinMode(pin_, INPUT_PULLUP);
  }

  // Call every loop iteration. Fires once, on release, after the debounce
  // window settles.
  Event update(uint32_t nowMs) {
    const bool raw = digitalRead(pin_) == LOW;  // pressed = LOW (pull-up)
    if (raw != rawState_) {
      rawState_ = raw;
      lastEdgeMs_ = nowMs;
    }

    Event event = Event::NONE;
    if (nowMs - lastEdgeMs_ >= ui::BUTTON_DEBOUNCE_MS &&
        stableState_ != rawState_) {
      stableState_ = raw;
      if (stableState_) {
        pressStartMs_ = nowMs;
      } else {
        const uint32_t heldMs = nowMs - pressStartMs_;
        event = heldMs >= ui::BUTTON_LONG_PRESS_MS ? Event::LONG_PRESS
                                                    : Event::SHORT_PRESS;
      }
    }
    return event;
  }

 private:
  uint8_t pin_ = 0;
  bool rawState_ = false;
  bool stableState_ = false;
  uint32_t lastEdgeMs_ = 0;
  uint32_t pressStartMs_ = 0;
};
