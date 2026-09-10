#include "Display.h"

#include "BleRecorder.h"
#include "Inputs.h"
#include "Motion.h"
#include "Sequencer.h"
#include "Settings.h"

Display display;

void Display::begin() {
  if (!settings.displayEnabled) {
    Serial.println("[oled] disabled in settings");
    return;
  }
  bus_.begin(settings.oledSdaPin, settings.oledSclPin, 400000);
  oled_ = new Adafruit_SSD1306(oled::WIDTH, oled::HEIGHT, &bus_, -1);
  ready_ = oled_->begin(SSD1306_SWITCHCAPVCC, settings.oledAddress);
  if (!ready_) {
    Serial.printf("[oled] init failed at 0x%02X -- check the address jumper\n",
                  settings.oledAddress);
    return;
  }
  oled_->clearDisplay();
  oled_->setTextSize(1);
  oled_->setTextColor(SSD1306_WHITE);
  oled_->setCursor(0, 0);
  oled_->println("Camera Slider");
  oled_->println(fw::VERSION);
  oled_->display();
}

void Display::update(uint32_t nowMs) {
  if (!ready_) return;
  if (nowMs - lastRefreshMs_ < settings.displayRefreshMs) return;
  lastRefreshMs_ = nowMs;

  oled_->clearDisplay();
  oled_->setTextSize(1);
  oled_->setTextColor(SSD1306_WHITE);
  oled_->setCursor(0, 0);

  for (uint8_t i = 0; i < fw::AXIS_COUNT; ++i) {
    const AxisConfig &c = settings.axes[i];
    const Axis &ax = motion_ctl.axis(i);
    if (!c.enabled) {
      oled_->printf("%-5.5s   ---\n", c.name);
      continue;
    }
    // A leading '*' marks the axis the "select next axis" action is pointing
    // at, so button bindings that act on the selection are readable at a
    // glance.
    oled_->printf("%c%-4.4s%6.1f>%6.1f\n", i == inputs.selectedAxis() ? '*' : ' ',
                  c.name, ax.positionUnits(), ax.targetUnits());
  }

  const char *seqState = "idle";
  switch (sequencer.state()) {
    case SeqState::MOVING: seqState = "play"; break;
    case SeqState::HOLDING: seqState = "hold"; break;
    case SeqState::PAUSED: seqState = "paus"; break;
    default: break;
  }
  oled_->printf("SEQ %s %u/%u %s\n", seqState,
                sequencer.count() ? sequencer.currentIndex() + 1 : 0,
                sequencer.count(), bleRecorder.isRecording() ? "REC" : "");

  oled_->printf("%s %s %s\n", motion_ctl.estopped() ? "ESTOP" : "RUN",
                motion_ctl.driversEnabled() ? "EN" : "off",
                bleRecorder.isConnected() ? "BLE" : "---");

  oled_->print(networkLine_);
  oled_->display();
}
