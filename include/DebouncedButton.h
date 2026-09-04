#pragma once
// Debounced, non-blocking push-button reader with short/long-press
// discrimination. The long press fires as soon as the hold threshold passes
// (not on release) so the operator gets feedback while still holding, and the
// matching release is then swallowed rather than also firing a short press.

#include <Arduino.h>

#include "config.h"

class DebouncedButton {
 public:
  enum class Event : uint8_t { NONE, SHORT_PRESS, LONG_PRESS };

  void begin(uint8_t pin, bool activeLow) {
    pin_ = pin;
    activeLow_ = activeLow;
    pinMode(pin_, activeLow ? INPUT_PULLUP : INPUT_PULLDOWN);
    rawState_ = stableState_ = false;
    lastEdgeMs_ = millis();
  }

  // Call every loop iteration.
  Event update(uint32_t nowMs) {
    const bool raw = (digitalRead(pin_) == LOW) == activeLow_;
    if (raw != rawState_) {
      rawState_ = raw;
      lastEdgeMs_ = nowMs;
    }

    Event event = Event::NONE;
    if (nowMs - lastEdgeMs_ >= ui::BUTTON_DEBOUNCE_MS && stableState_ != rawState_) {
      stableState_ = rawState_;
      if (stableState_) {
        pressStartMs_ = nowMs;
        longFired_ = false;
      } else if (!longFired_) {
        event = Event::SHORT_PRESS;
      }
    }

    if (stableState_ && !longFired_ &&
        nowMs - pressStartMs_ >= ui::BUTTON_LONG_PRESS_MS) {
      longFired_ = true;
      event = Event::LONG_PRESS;
    }
    return event;
  }

  bool isPressed() const { return stableState_; }

 private:
  uint8_t pin_ = 0;
  bool activeLow_ = true;
  bool rawState_ = false;
  bool stableState_ = false;
  bool longFired_ = false;
  uint32_t lastEdgeMs_ = 0;
  uint32_t pressStartMs_ = 0;
};
