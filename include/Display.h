#pragma once
// OLED status display on its own isolated I2C bus (Wire1). Refreshed on a
// rate limit, never every loop, so it can't add jitter to motion control.

#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

enum class SelectedAxis : uint8_t { PAN, TILT };

class Display {
 public:
  explicit Display(TwoWire &bus);

  void begin();
  void update(uint32_t nowMs, float slideMm, float panDeg, float panTargetDeg,
              float tiltDeg, float tiltTargetDeg, int32_t jogSignedHz,
              SelectedAxis selected, bool recording, bool bleConnected);

 private:
  TwoWire &bus_;
  Adafruit_SSD1306 oled_;
  uint32_t lastRefreshMs_ = 0;
  bool ready_ = false;
};
