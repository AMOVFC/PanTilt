#include "Display.h"

#include "config.h"

Display::Display(TwoWire &bus)
    : bus_(bus), oled_(oled::WIDTH, oled::HEIGHT, &bus_, -1) {}

void Display::begin() {
  ready_ = oled_.begin(SSD1306_SWITCHCAPVCC, oled::I2C_ADDR);
  if (!ready_) {
    Serial.println("ERROR: OLED init failed (not fitted? fine if intentional)");
    return;
  }
  oled_.clearDisplay();
  oled_.display();
}

void Display::update(uint32_t nowMs, float slideMm, float panDeg,
                      float panTargetDeg, float tiltDeg, float tiltTargetDeg,
                      int32_t jogSignedHz, bool recording, bool bleConnected) {
  if (!ready_) return;
  if (nowMs - lastRefreshMs_ < ui::OLED_REFRESH_INTERVAL_MS) return;
  lastRefreshMs_ = nowMs;

  oled_.clearDisplay();
  oled_.setTextSize(1);
  oled_.setTextColor(SSD1306_WHITE);
  oled_.setCursor(0, 0);
  oled_.printf("Slide %6.1fmm\n", slideMm);
  oled_.printf("Pan  %6.1f->%6.1f\n", panDeg, panTargetDeg);
  oled_.printf("Tilt %6.1f->%6.1f\n", tiltDeg, tiltTargetDeg);
  oled_.printf("Jog %5ldHz\n", static_cast<long>(jogSignedHz));
  oled_.printf("REC:%s BLE:%s\n", recording ? "ON" : "off",
               bleConnected ? "conn" : "---");
  oled_.display();
}
