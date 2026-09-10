#pragma once
// Runtime-editable configuration, persisted as JSON on LittleFS.
//
// This is the single source of truth for the whole firmware: pin map, axis
// mechanics, motion limits, control mapping and network setup all live here
// rather than in constexprs, so the web UI can change any of them without a
// reflash. config.h only supplies the factory defaults.
//
// Anything that can only be applied at boot (pin assignments, encoder pins,
// I2C pins) sets requiresReboot on save; the UI surfaces that to the user.

#include <ArduinoJson.h>
#include <Arduino.h>

#include "config.h"

enum class AxisKind : uint8_t { LINEAR = 0, ROTARY = 1 };
enum class HomingMode : uint8_t { NONE = 0, LIMIT_MIN = 1, LIMIT_MAX = 2, SENSOR = 3 };
enum class FeedbackType : uint8_t { NONE = 0, AS5600 = 1 };
enum class EncoderMode : uint8_t { OFF = 0, VELOCITY = 1, POSITION = 2 };

// Everything a physical button can be bound to. Kept as one flat list so the
// web UI can render it as a plain dropdown and so short- and long-press can
// draw from the same set.
enum class ButtonAction : uint8_t {
  NONE = 0,
  PLAY_PAUSE,
  SEQ_STOP,
  SEQ_RESTART,
  KEYFRAME_ADD,
  KEYFRAME_DELETE_LAST,
  KEYFRAME_CLEAR,
  GOTO_START,
  HOME_ALL,
  HOME_AXIS,
  ESTOP,
  ENABLE_TOGGLE,
  REC_TOGGLE,
  REC_RESYNC,
  SELECT_NEXT_AXIS,
  ZERO_AXIS,
  ACTION_COUNT
};

const char *axisKindName(AxisKind v);
const char *homingModeName(HomingMode v);
const char *feedbackTypeName(FeedbackType v);
const char *encoderModeName(EncoderMode v);
const char *buttonActionName(ButtonAction v);

struct AxisConfig {
  char name[16];
  bool enabled;
  AxisKind kind;

  uint8_t stepPin;
  uint8_t dirPin;
  bool invertDir;

  // TMC2209 over the shared UART bus. address is strapped by MS1/MS2 on the
  // board (U1=0, U2=1, U3=2, U4=3) and must match the hardware straps.
  bool tmcEnabled;
  uint8_t tmcAddress;
  uint16_t microsteps;      // 1,2,4,8,16,32,64,128,256
  uint16_t runCurrentMa;
  uint8_t holdCurrentPct;
  bool stealthChop;

  float motorStepsPerRev;
  float beltPitchMm;   // LINEAR
  float pulleyTeeth;   // LINEAR
  float gearRatio;     // ROTARY

  float maxSpeed;   // units/s   (mm/s or deg/s)
  float accel;      // units/s^2
  float minLimit;   // units
  float maxLimit;   // units
  bool softLimits;

  HomingMode homing;
  uint8_t limitMinPin;
  uint8_t limitMaxPin;
  bool limitActiveLow;
  float homingSpeed;    // units/s
  float homingBackoff;  // units

  FeedbackType feedback;
  uint8_t muxChannel;
  float zeroOffsetDeg;
  uint32_t driftCheckMs;
  float driftThresholdDeg;

  const char *unitLabel() const { return kind == AxisKind::ROTARY ? "deg" : "mm"; }

  // Steps per user unit, derived from the mechanics above. Everything that
  // converts between units and steps goes through this.
  float stepsPerUnit() const {
    const float full = motorStepsPerRev * static_cast<float>(microsteps);
    if (kind == AxisKind::ROTARY) {
      return (full * gearRatio) / 360.0f;
    }
    const float mmPerRev = beltPitchMm * pulleyTeeth;
    return mmPerRev > 0.0f ? full / mmPerRev : 0.0f;
  }
};

struct EncoderConfig {
  char name[16];
  bool enabled;
  uint8_t pinA;
  uint8_t pinB;
  EncoderMode mode;
  uint8_t axis;             // index into Settings::axes
  bool invert;
  uint8_t countsPerDetent;  // raw quadrature counts per mechanical click
  float unitsPerDetent;     // POSITION mode: how far one click nudges
  int32_t detentsForMaxSpeed;  // VELOCITY mode: clicks from stop to full speed
};

struct ButtonConfig {
  char name[16];
  bool enabled;
  uint8_t pin;
  bool activeLow;
  ButtonAction shortPress;
  ButtonAction longPress;
  uint8_t axisArg;  // target axis for HOME_AXIS / ZERO_AXIS
};

struct WifiConfig {
  bool staEnabled;      // try to join `ssid` first
  bool apFallback;      // bring up the AP if the join fails (or always, if !sta)
  char ssid[33];
  char pass[65];
  char apSsid[33];
  char apPass[65];
  char hostname[33];
};

struct Settings {
  uint16_t version;

  AxisConfig axes[fw::AXIS_COUNT];
  EncoderConfig encoders[fw::ENCODER_COUNT];
  ButtonConfig buttons[fw::BUTTON_COUNT];
  WifiConfig wifi;

  // Shared driver enable line (active low on this board).
  uint8_t driverEnablePin;
  bool driverEnableActiveLow;

  // Shared TMC2209 UART bus.
  uint8_t tmcTxPin;
  uint8_t tmcRxPin;
  uint32_t tmcBaud;
  float tmcRSense;

  uint8_t muxSdaPin, muxSclPin, muxAddress;
  uint8_t oledSdaPin, oledSclPin, oledAddress;
  bool displayEnabled;
  uint16_t displayRefreshMs;

  bool bleEnabled;
  char bleName[32];

  bool sequencerLoop;
  bool sequencerEase;  // s-curve blend between keyframes

  void setDefaults();
  void toJson(JsonObject root) const;
  // Merges `root` over the current values; absent keys keep their value, so
  // the UI can PATCH a single field. Returns false if a value was rejected.
  bool applyJson(JsonObjectConst root, String &error);

  // Rejects a config where two live roles want the same GPIO -- two axes on
  // one STEP pin, an encoder on a limit switch, and so on.
  bool checkPinConflicts(String &error) const;

  bool load();
  bool save() const;
  static const char *path() { return "/config.json"; }
};

extern Settings settings;
