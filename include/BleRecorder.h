#pragma once
// BLE HID keyboard emulation that toggles the Blackmagic Camera app's
// record state via its volume-button binding. That binding is a *toggle*,
// not distinct start/stop, so firmware tracks isRecording as its own belief
// and separately exposes a resync (see main.cpp for how the two buttons on
// the jog encoder's push button - short vs. long press - map to these).

#include <Arduino.h>
#include <BleKeyboard.h>

#include "config.h"

class BleRecorder {
 public:
  void begin();

  // Sends the BLE volume-up keypress and flips the belief. No-op (logged)
  // if nothing is connected yet.
  void toggleRecording();

  // Flips the belief only, no keypress - for when it's drifted from the
  // phone's actual state and the operator has visually confirmed as much.
  void resync();

  bool isRecording() const { return isRecording_; }
  bool isConnected() { return keyboard_.isConnected(); }

 private:
  BleKeyboard keyboard_{ble::DEVICE_NAME, ble::MANUFACTURER, ble::BATTERY_LEVEL};
  bool isRecording_ = false;
};
