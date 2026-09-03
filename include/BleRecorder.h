#pragma once
// BLE HID keyboard emulation that toggles the Blackmagic Camera app's record
// state via its volume-button binding. That binding is a *toggle*, not
// distinct start/stop, so the firmware tracks isRecording as its own belief
// and separately exposes a resync for when that belief has drifted from what
// the phone is actually doing.
//
// Compiled out entirely with -DENABLE_BLE_HID=0, which frees a large chunk of
// flash and removes any WiFi/BLE coexistence question if you do not use it.

#include <Arduino.h>

#include "config.h"

#ifndef ENABLE_BLE_HID
#define ENABLE_BLE_HID 1
#endif

class BleRecorder {
 public:
  void begin();
  void toggleRecording();  // sends the keypress and flips the belief
  void resync();           // flips the belief only, sends nothing
  bool isRecording() const { return isRecording_; }
  bool isConnected();
  bool isEnabled() const { return started_; }

 private:
  bool isRecording_ = false;
  bool started_ = false;
};

extern BleRecorder bleRecorder;
