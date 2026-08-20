// Firmware for the ORIGINAL PROTOTYPE build: 3 axes (slide/pan/tilt), 3
// TMC2209s on breakout boards, 3 dedicated per-axis encoders, 2 AS5600s
// (pan/tilt), limit switches on slide only. See prototype/README.md for the
// written wiring diagram this matches.
//
// Structured to mirror the final 4-axis rig's main.cpp as closely as the
// hardware difference allows, so the two stay easy to cross-port:
//   - Same setup()/loop() shape, same non-blocking style (no delay()).
//   - SlideAxis, RotaryAxis, Mux, Settings, WebConfig, BleRecorder,
//     DebouncedButton are used identically (RotaryAxis gained one extra
//     method, setTargetDeg(), for the recenter buttons below).
//   - Genuinely different because the hardware is different: no ZAxis, no
//     ShotSequencer/Shot keyframes (nothing yet needs 4-axis synchronized
//     moves), and the final rig's shared angle-encoder-with-select-button
//     scheme is replaced by one encoder per rotary axis. (The final rig's
//     current control scheme is itself expected to change in an upcoming
//     revision, so it isn't being treated as the thing to match here.)

#include <Arduino.h>
#include <ESP32Encoder.h>
#include <FastAccelStepper.h>
#include <Wire.h>

#include "BleRecorder.h"
#include "DebouncedButton.h"
#include "Display.h"
#include "RotaryAxis.h"
#include "Settings.h"
#include "SlideAxis.h"
#include "TmcDrivers.h"
#include "WebConfig.h"
#include "config.h"

namespace {

FastAccelStepperEngine engine;
SlideAxis slide;
RotaryAxis pan(pins::PAN_STEP, pins::PAN_DIR, mux_channel::PAN,
               calibration::PAN_ZERO_OFFSET_DEG, motion::PAN_MIN_DEG,
               motion::PAN_MAX_DEG);
RotaryAxis tilt(pins::TILT_STEP, pins::TILT_DIR, mux_channel::TILT,
                calibration::TILT_ZERO_OFFSET_DEG, motion::TILT_MIN_DEG,
                motion::TILT_MAX_DEG);

TwoWire &i2cMux = Wire;  // bus A: TCA9548A -> pan/tilt AS5600
TwoWire i2cOled(1);      // bus B: OLED (optional), isolated from the mux bus
Display display(i2cOled);

BleRecorder bleRecorder;

// One encoder per rotary axis, unlike the final rig's single shared angle
// encoder + axis-select button. Slide keeps its own dedicated jog encoder
// internally inside SlideAxis, same as the final rig.
ESP32Encoder panEncoder;
ESP32Encoder tiltEncoder;
int64_t lastPanCount = 0;
int64_t lastTiltCount = 0;

DebouncedButton slidePushButton;
DebouncedButton panPushButton;
DebouncedButton tiltPushButton;

void setupDriverEnable() {
  // EN is a single line shared across all 3 TMC2209s, active low. Managed
  // manually rather than via FastAccelStepper's per-axis auto-enable, same
  // reasoning as the final rig: that API assumes exclusive ownership of the
  // enable pin, which doesn't hold once pan/tilt share this line with slide.
  pinMode(pins::DRIVER_EN, OUTPUT);
  digitalWrite(pins::DRIVER_EN, LOW);
}

void setupRotaryEncoders() {
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  panEncoder.attachHalfQuad(pins::PAN_ENC_A, pins::PAN_ENC_B);
  panEncoder.setCount(0);
  tiltEncoder.attachHalfQuad(pins::TILT_ENC_A, pins::TILT_ENC_B);
  tiltEncoder.setCount(0);
}

// Each rotary encoder's delta nudges its own axis directly — no shared
// count, no axis-select state, so there's no phantom-jump bookkeeping to
// get right the way the final rig's shared encoder needs.
void updatePanTiltSetpoints() {
  const int64_t panCount = panEncoder.getCount();
  const int64_t panDelta = panCount - lastPanCount;
  lastPanCount = panCount;
  if (panDelta != 0) {
    pan.nudgeTargetDeg(static_cast<float>(panDelta) * motion::ANGLE_DEG_PER_CLICK);
  }

  const int64_t tiltCount = tiltEncoder.getCount();
  const int64_t tiltDelta = tiltCount - lastTiltCount;
  lastTiltCount = tiltCount;
  if (tiltDelta != 0) {
    tilt.nudgeTargetDeg(static_cast<float>(tiltDelta) * motion::ANGLE_DEG_PER_CLICK);
  }
}

// Short press = resync isRecording to the phone's actual state (no
// keypress sent). Long press = actually toggle recording. Identical to the
// final rig's jog-encoder button.
void updateRecordButton(uint32_t nowMs) {
  const DebouncedButton::Event event = slidePushButton.update(nowMs);
  if (event == DebouncedButton::Event::LONG_PRESS) {
    bleRecorder.toggleRecording();
  } else if (event == DebouncedButton::Event::SHORT_PRESS) {
    bleRecorder.resync();
  }
}

// Pan/tilt push buttons: short press recenters that axis's target to
// 0deg (the as-calibrated mechanical zero) -- a quick way back to a known
// pose during bring-up. Long press is unused/reserved. This is a minimal
// default, not a fixed requirement -- easy to repoint at something else
// once the prototype's actual UI needs are clearer.
void updateAxisRecenterButtons(uint32_t nowMs) {
  if (panPushButton.update(nowMs) == DebouncedButton::Event::SHORT_PRESS) {
    pan.setTargetDeg(0.0f);
  }
  if (tiltPushButton.update(nowMs) == DebouncedButton::Event::SHORT_PRESS) {
    tilt.setTargetDeg(0.0f);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);

  // Load saved settings first: everything below reads config values, and
  // the TMC driver setup writes some of them straight to hardware.
  settings::begin();

  setupDriverEnable();

  // Configure all 3 TMC2209s over UART (current, microstepping, comms
  // verification) BEFORE any axis starts moving — slide.begin() kicks off
  // its homing move immediately, and that move's step math assumes the
  // microstep register write already took.
  tmc_drivers::beginAll();

  engine.init();

  i2cMux.begin(pins::I2C_MUX_SDA, pins::I2C_MUX_SCL);
  i2cOled.begin(pins::I2C_OLED_SDA, pins::I2C_OLED_SCL);

  slide.begin(engine);
  pan.begin(engine, i2cMux);
  tilt.begin(engine, i2cMux);

  setupRotaryEncoders();
  slidePushButton.begin(pins::SLIDE_ENC_PUSH);
  panPushButton.begin(pins::PAN_ENC_PUSH);
  tiltPushButton.begin(pins::TILT_ENC_PUSH);

  display.begin();  // no-ops cleanly if no OLED is fitted
  bleRecorder.begin();

  // No ShotSequencer yet (see file header), so nothing ever holds the rig
  // "busy" -- config writes are always accepted.
  webconfig::begin([]() { return false; });
}

void loop() {
  const uint32_t nowMs = millis();

  slide.update();
  pan.update(nowMs);
  tilt.update(nowMs);

  updatePanTiltSetpoints();
  updateRecordButton(nowMs);
  updateAxisRecenterButtons(nowMs);
  webconfig::update();

  display.update(nowMs, slide.positionMm(), pan.currentDeg(), pan.targetDeg(),
                  tilt.currentDeg(), tilt.targetDeg(), slide.jogSignedHz(),
                  bleRecorder.isRecording(), bleRecorder.isConnected());
}
