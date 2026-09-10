#pragma once
// OLED status display on its own isolated I2C bus (Wire1). Refreshed on a
// rate limit, never every loop, so a 128x64 frame push can never add jitter
// to the step generators.

#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

class Display {
 public:
  void begin();
  void update(uint32_t nowMs);
  bool ready() const { return ready_; }
  // Shown on the bottom line so the operator can find the web UI without a
  // serial console.
  void setNetworkLine(const String &line) { networkLine_ = line; }

 private:
  TwoWire bus_{1};
  Adafruit_SSD1306 *oled_ = nullptr;
  uint32_t lastRefreshMs_ = 0;
  bool ready_ = false;
  String networkLine_ = "starting...";
};

extern Display display;
