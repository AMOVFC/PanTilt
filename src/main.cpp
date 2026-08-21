// Full firmware for the 4-axis camera slider (slide/pan/tilt/z), assuming
// the designed hardware (see docs/project-brief.md and
// hardware/final_wiring_diagram_v3.svg) is built and working as spec'd.

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
#include "ZAxis.h"
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
ZAxis z;

ShotSequencer shotSequencer;

// Simple 3-axis (slide + pan + z) test shot for validating the synchronized
// duration-matching algorithm on real hardware, per brief §6.3/milestone 11.
// Triggered over Serial (send 's') rather than a dedicated button, since the
// pin map has no spare input assigned to shot playback. Tilt is pinned to a
// fixed level (0deg) throughout — this shot isn't exercising tilt.
const Keyframe kTestShot[] = {
    {/*slideMm=*/0.0f, /*panDeg=*/-45.0f, /*tiltDeg=*/0.0f, /*zMm=*/0.0f,
     /*durationS=*/0.0f, EaseType::EASE_IN_OUT},
    {/*slideMm=*/200.0f, /*panDeg=*/45.0f, /*tiltDeg=*/0.0f, /*zMm=*/80.0f,
     /*durationS=*/0.0f, EaseType::EASE_IN_OUT},
    {/*slideMm=*/0.0f, /*panDeg=*/-45.0f, /*tiltDeg=*/0.0f, /*zMm=*/0.0f,
     /*durationS=*/0.0f, EaseType::EASE_IN_OUT},
};
constexpr uint8_t kTestShotCount = sizeof(kTestShot) / sizeof(kTestShot[0]);

TwoWire &i2cMux = Wire;  // bus A: TCA9548A -> pan/tilt AS5600
TwoWire i2cOled(1);      // bus B: OLED, isolated from the mux bus
Display display(i2cOled);

BleRecorder bleRecorder;

ESP32Encoder angleEncoder;
int64_t lastAngleEncoderCount = 0;
SelectedAxis selectedAxis = SelectedAxis::PAN;

DebouncedButton jogPushButton;
DebouncedButton anglePushButton;

void setupDriverEnable() {
  // EN is a single line shared across all 4 TMC2209s, active low. Managed
  // manually (not via FastAccelStepper's per-axis auto-enable) because that
  // API assumes exclusive ownership of the enable pin per stepper, which
  // doesn't hold once pan/tilt/z share this same line. Never toggled again
  // after this, so holding torque is never lost — see ZAxis.h re: §6.5's
  // leadscrew self-locking concern.
  pinMode(pins::DRIVER_EN, OUTPUT);
  digitalWrite(pins::DRIVER_EN, LOW);
}

void setupAngleEncoder() {
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  angleEncoder.attachHalfQuad(pins::ANGLE_ENC_A, pins::ANGLE_ENC_B);
  angleEncoder.setCount(0);
}

// Angle encoder accumulates a target angle for whichever of pan/tilt is
// currently selected. Delta is computed from the running count so switching
// axes mid-turn never produces a phantom jump on either axis.
void updateAngleSetpoint() {
  const int64_t count = angleEncoder.getCount();
  const int64_t delta = count - lastAngleEncoderCount;
  lastAngleEncoderCount = count;
  if (delta == 0) return;

  const float deltaDeg = static_cast<float>(delta) * motion::ANGLE_DEG_PER_CLICK;
  if (selectedAxis == SelectedAxis::PAN) {
    pan.nudgeTargetDeg(deltaDeg);
  } else {
    tilt.nudgeTargetDeg(deltaDeg);
  }
}

void updateAxisSelectButton(uint32_t nowMs) {
  const DebouncedButton::Event event = anglePushButton.update(nowMs);
  if (event == DebouncedButton::Event::NONE) return;
  selectedAxis =
      selectedAxis == SelectedAxis::PAN ? SelectedAxis::TILT : SelectedAxis::PAN;
}

// Short press = resync isRecording to the phone's actual state (no
// keypress sent). Long press = actually toggle recording. The brief's pin
// map only assigns this button to "resync"; there's no separate dedicated
// record-trigger pin, so this reuses the one button for both rather than
// requiring new wiring.
void updateRecordButton(uint32_t nowMs) {
  const DebouncedButton::Event event = jogPushButton.update(nowMs);
  if (event == DebouncedButton::Event::LONG_PRESS) {
    bleRecorder.toggleRecording();
  } else if (event == DebouncedButton::Event::SHORT_PRESS) {
    bleRecorder.resync();
  }
}

// Settings must not change underneath a running shot: its S-curve waypoints
// were computed against the speeds, limits and step ratios in effect when
// the move started.
bool shotIsRunning() { return shotSequencer.isActive(); }

// Serial-triggered shot playback control for bring-up/validation (§6,
// milestone 11) ahead of any dedicated UI for authoring/selecting shots.
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

  // Configure all 4 TMC2209s over UART (current, microstepping, comms
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
  z.begin(engine);

  setupAngleEncoder();
  jogPushButton.begin(pins::JOG_ENC_PUSH);
  anglePushButton.begin(pins::ANGLE_ENC_PUSH);

  display.begin();
  bleRecorder.begin();
  shotSequencer.begin(slide, pan, tilt, z);
  webconfig::begin(shotIsRunning);
}

void loop() {
  const uint32_t nowMs = millis();

  slide.update();
  pan.update(nowMs);
  tilt.update(nowMs);
  z.update();
  shotSequencer.update(nowMs);

  updateShotSerialTrigger(nowMs);
  updateAngleSetpoint();
  updateAxisSelectButton(nowMs);
  updateRecordButton(nowMs);
  webconfig::update();

  display.update(nowMs, slide.positionMm(), pan.currentDeg(), pan.targetDeg(),
                  tilt.currentDeg(), tilt.targetDeg(), z.positionMm(),
                  slide.jogSignedHz(), selectedAxis, bleRecorder.isRecording(),
                  bleRecorder.isConnected());
}
