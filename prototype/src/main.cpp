// Firmware for the ORIGINAL PROTOTYPE build: 3 axes (slide/pan/tilt), 3
// TMC2209s on breakout boards, 3 dedicated per-axis encoders, 2 AS5600s
// (pan/tilt), limit switches on slide only. See prototype/README.md for the
// written wiring diagram this matches.
//
// Structured to mirror the final 4-axis rig's main.cpp as closely as the
// hardware difference allows, so the two stay easy to cross-port:
//   - Same setup()/loop() shape, same non-blocking style (no delay()).
//   - SlideAxis, RotaryAxis, Mux, Settings, WebConfig, BleRecorder,
//     DebouncedButton, Shot/ShotSequencer are used identically (RotaryAxis
//     gained one extra method, setTargetDeg(), for the recenter buttons
//     below; Shot/ShotSequencer are the 3-axis version -- no zMm field, no
//     ZAxis pointer -- everything else about a keyframe is unchanged).
//   - Genuinely different because the hardware is different: no ZAxis, and
//     the final rig's shared angle-encoder-with-select-button scheme is
//     replaced by one encoder per rotary axis. (The final rig's current
//     control scheme is itself expected to change in an upcoming revision,
//     so it isn't being treated as the thing to match here.)

#include <Arduino.h>
#include <ESP32Encoder.h>
#include <FastAccelStepper.h>
#include <Wire.h>

#include "BleRecorder.h"
#include "DebouncedButton.h"
#include "Display.h"
#include "RotaryAxis.h"
#include "Settings.h"
#include "Shot.h"
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

ShotSequencer shotSequencer;

// Same 3-axis test shot the final rig used for validating the
// duration-matching algorithm (its slide/pan/z shot, tilt pinned to 0deg,
// minus the z field). Triggered over Serial ('s' to run, 'c' to cancel)
// rather than a dedicated button -- all 3 encoder pushes are already
// assigned (see updateRecordButton/updateAxisRecenterButtons below), and
// this matches the final rig's own bring-up UX exactly.
const Keyframe kTestShot[] = {
    {/*slideMm=*/0.0f, /*panDeg=*/-45.0f, /*tiltDeg=*/0.0f,
     /*durationS=*/0.0f, EaseType::EASE_IN_OUT},
    {/*slideMm=*/200.0f, /*panDeg=*/45.0f, /*tiltDeg=*/0.0f,
     /*durationS=*/0.0f, EaseType::EASE_IN_OUT},
    {/*slideMm=*/0.0f, /*panDeg=*/-45.0f, /*tiltDeg=*/0.0f,
     /*durationS=*/0.0f, EaseType::EASE_IN_OUT},
};
constexpr uint8_t kTestShotCount = sizeof(kTestShot) / sizeof(kTestShot[0]);

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

// Settings must not change underneath a running shot: its S-curve waypoints
// were computed against the speeds, limits and step ratios in effect when
// the move started. Passed to webconfig::begin() below as a plain function
// (not a lambda) so it converts to the bool(*)() function pointer WebConfig
// expects while still reading the file-scope shotSequencer directly.
bool shotIsRunning() { return shotSequencer.isActive(); }

// Serial-triggered shot playback control, same as the final rig -- 's' to
// start the test shot, 'c' to cancel. Ahead of any dedicated UI for
// authoring/selecting shots.
void updateShotSerialTrigger(uint32_t nowMs) {
  while (Serial.available() > 0) {
    const int c = Serial.read();
    if (c == 's' && !shotSequencer.isActive()) {
      shotSequencer.start(kTestShot, kTestShotCount, nowMs);
    } else if (c == 'c') {
      shotSequencer.cancel();
    }
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
  shotSequencer.begin(slide, pan, tilt);
  webconfig::begin(shotIsRunning);
}

void loop() {
  const uint32_t nowMs = millis();

  slide.update();
  pan.update(nowMs);
  tilt.update(nowMs);
  shotSequencer.update(nowMs);

  updateShotSerialTrigger(nowMs);
  updatePanTiltSetpoints();
  updateRecordButton(nowMs);
  updateAxisRecenterButtons(nowMs);
  webconfig::update();

  display.update(nowMs, slide.positionMm(), pan.currentDeg(), pan.targetDeg(),
                  tilt.currentDeg(), tilt.targetDeg(), slide.jogSignedHz(),
                  bleRecorder.isRecording(), bleRecorder.isConnected());
}
