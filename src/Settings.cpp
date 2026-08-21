#include "Settings.h"

#include <Arduino.h>
#include <Preferences.h>
#include <math.h>
#include <string.h>

#include "config.h"

// ---------- Definitions + compiled-in defaults ----------
// These are the values used on a fresh board, or for any key not yet written
// to NVS. Changing a default here only affects boards that have never had
// that setting saved (or one that has been reset to defaults).

namespace tmc {
uint16_t MICROSTEPS = 64;
// RMS run current per motor. PLACEHOLDERS at a conservative value for
// generic NEMA 17s — set per motor's rated current (typically 70-85% of the
// motor nameplate rating) once the actual motors are in hand.
uint16_t SLIDE_RMS_MA = 800;
uint16_t PAN_RMS_MA = 800;
uint16_t TILT_RMS_MA = 800;
uint16_t Z_RMS_MA = 800;
float HOLD_CURRENT_FRACTION = 0.7f;
}  // namespace tmc

namespace mech {
float MOTOR_STEPS_PER_REV = 200.0f;

uint16_t SLIDE_MICROSTEPPING = 64;  // derived
float SLIDE_BELT_PITCH_MM = 2.0f;
float SLIDE_PULLEY_TEETH = 20.0f;
float SLIDE_MM_PER_REV = 40.0f;      // derived
float SLIDE_STEPS_PER_MM = 320.0f;   // derived

uint16_t ROTARY_MICROSTEPPING = 64;  // derived
float ROTARY_BELT_RATIO = 4.0f;
float ROTARY_STEPS_PER_DEGREE = 142.222f;  // derived

uint16_t Z_MICROSTEPPING = 64;  // derived
float Z_LEAD_MM = 8.0f;
float Z_STEPS_PER_MM = 1600.0f;  // derived
}  // namespace mech

namespace motion {
bool SLIDE_DIR_INVERT = false;
uint32_t SLIDE_MAX_SPEED_HZ = 32000;
uint32_t SLIDE_ACCEL_HZ_PER_S = 80000;
uint32_t SLIDE_HOMING_SPEED_HZ = 6000;
int32_t SLIDE_HOMING_BACKOFF_STEPS = 800;
bool SLIDE_HOME_TOWARD_MIN = true;

int32_t JOG_COUNTS_PER_MAX_SPEED = 20;

uint32_t ROTARY_MAX_SPEED_HZ = 16000;
uint32_t ROTARY_ACCEL_HZ_PER_S = 32000;
float ANGLE_DEG_PER_CLICK = 0.5f;

float PAN_MIN_DEG = -170.0f;
float PAN_MAX_DEG = 170.0f;
float TILT_MIN_DEG = -45.0f;
float TILT_MAX_DEG = 90.0f;

uint32_t ROTARY_DRIFT_CHECK_INTERVAL_MS = 2000;
int32_t ROTARY_DRIFT_THRESHOLD_STEPS = 80;

uint32_t Z_MAX_SPEED_HZ = 16000;
uint32_t Z_ACCEL_HZ_PER_S = 32000;
uint32_t Z_HOMING_SPEED_HZ = 4000;
int32_t Z_HOMING_BACKOFF_STEPS = 3200;
bool Z_HOME_DIR_FORWARD = false;

float Z_MIN_MM = 0.0f;
float Z_MAX_MM = 170.0f;

float SHOT_LINEAR_ACCEL_MULTIPLIER = 6.0f;
uint8_t SHOT_SCURVE_SEGMENTS = 16;
uint32_t SHOT_SCURVE_MIN_SEGMENT_MS = 40;
}  // namespace motion

namespace ui {
uint32_t BUTTON_DEBOUNCE_MS = 30;
uint32_t BUTTON_LONG_PRESS_MS = 600;
uint32_t OLED_REFRESH_INTERVAL_MS = 200;
}  // namespace ui

namespace calibration {
float PAN_ZERO_OFFSET_DEG = 0.0f;
float TILT_ZERO_OFFSET_DEG = 0.0f;
}  // namespace calibration

// ---------- Descriptor table ----------

namespace settings {

// NVS keys are capped at 15 characters, which is why these are abbreviated
// rather than matching the C++ identifier names.
const Desc kSettings[] = {
    // --- Drivers ---
    {"microsteps", "Drivers", "Microstepping", "x", Type::U16, &tmc::MICROSTEPS, 1, 256, true},
    {"slideRms", "Drivers", "Slide current", "mA RMS", Type::U16, &tmc::SLIDE_RMS_MA, 100, 2000, true},
    {"panRms", "Drivers", "Pan current", "mA RMS", Type::U16, &tmc::PAN_RMS_MA, 100, 2000, true},
    {"tiltRms", "Drivers", "Tilt current", "mA RMS", Type::U16, &tmc::TILT_RMS_MA, 100, 2000, true},
    {"zRms", "Drivers", "Z current", "mA RMS", Type::U16, &tmc::Z_RMS_MA, 100, 2000, true},
    {"holdFrac", "Drivers", "Hold current fraction", "x run", Type::F32, &tmc::HOLD_CURRENT_FRACTION, 0.1f, 1.0f, true},

    // --- Mechanical ---
    {"motorSteps", "Mechanical", "Motor steps/rev", "steps", Type::F32, &mech::MOTOR_STEPS_PER_REV, 20, 400, false},
    {"slideBeltP", "Mechanical", "Slide belt pitch", "mm", Type::F32, &mech::SLIDE_BELT_PITCH_MM, 0.5f, 10, false},
    {"slidePulley", "Mechanical", "Slide pulley teeth", "T", Type::F32, &mech::SLIDE_PULLEY_TEETH, 8, 100, false},
    {"rotBeltRatio", "Mechanical", "Pan/tilt reduction", ":1", Type::F32, &mech::ROTARY_BELT_RATIO, 1, 50, false},
    {"zLeadMm", "Mechanical", "Z leadscrew lead", "mm/rev", Type::F32, &mech::Z_LEAD_MM, 0.5f, 50, false},

    // --- Slide ---
    {"slideMaxSpd", "Slide", "Max speed", "Hz", Type::U32, &motion::SLIDE_MAX_SPEED_HZ, 100, 200000, false},
    {"slideAccel", "Slide", "Acceleration", "Hz/s", Type::U32, &motion::SLIDE_ACCEL_HZ_PER_S, 100, 1000000, false},
    {"slideHomeSpd", "Slide", "Homing speed", "Hz", Type::U32, &motion::SLIDE_HOMING_SPEED_HZ, 100, 50000, false},
    {"slideBackoff", "Slide", "Homing backoff", "steps", Type::I32, &motion::SLIDE_HOMING_BACKOFF_STEPS, 0, 100000, false},
    {"slideDirInv", "Slide", "Invert direction", "", Type::BOOL, &motion::SLIDE_DIR_INVERT, 0, 1, true},
    {"slideHomeMin", "Slide", "Home toward min switch", "", Type::BOOL, &motion::SLIDE_HOME_TOWARD_MIN, 0, 1, true},
    {"jogCounts", "Slide", "Jog detents to max speed", "clicks", Type::I32, &motion::JOG_COUNTS_PER_MAX_SPEED, 1, 500, false},

    // --- Pan / Tilt ---
    {"rotMaxSpd", "Pan / Tilt", "Max speed", "Hz", Type::U32, &motion::ROTARY_MAX_SPEED_HZ, 100, 200000, false},
    {"rotAccel", "Pan / Tilt", "Acceleration", "Hz/s", Type::U32, &motion::ROTARY_ACCEL_HZ_PER_S, 100, 1000000, false},
    {"angDegClick", "Pan / Tilt", "Degrees per detent", "deg", Type::F32, &motion::ANGLE_DEG_PER_CLICK, 0.01f, 45, false},
    {"panMin", "Pan / Tilt", "Pan soft limit min", "deg", Type::F32, &motion::PAN_MIN_DEG, -360, 360, false},
    {"panMax", "Pan / Tilt", "Pan soft limit max", "deg", Type::F32, &motion::PAN_MAX_DEG, -360, 360, false},
    {"tiltMin", "Pan / Tilt", "Tilt soft limit min", "deg", Type::F32, &motion::TILT_MIN_DEG, -360, 360, false},
    {"tiltMax", "Pan / Tilt", "Tilt soft limit max", "deg", Type::F32, &motion::TILT_MAX_DEG, -360, 360, false},
    {"rotDriftMs", "Pan / Tilt", "Drift check interval", "ms", Type::U32, &motion::ROTARY_DRIFT_CHECK_INTERVAL_MS, 100, 60000, false},
    {"rotDriftStp", "Pan / Tilt", "Drift correction threshold", "steps", Type::I32, &motion::ROTARY_DRIFT_THRESHOLD_STEPS, 1, 10000, false},
    {"panZero", "Pan / Tilt", "Pan magnet zero offset", "deg", Type::F32, &calibration::PAN_ZERO_OFFSET_DEG, -360, 360, true},
    {"tiltZero", "Pan / Tilt", "Tilt magnet zero offset", "deg", Type::F32, &calibration::TILT_ZERO_OFFSET_DEG, -360, 360, true},

    // --- Z ---
    {"zMaxSpd", "Z (height)", "Max speed", "Hz", Type::U32, &motion::Z_MAX_SPEED_HZ, 100, 200000, false},
    {"zAccel", "Z (height)", "Acceleration", "Hz/s", Type::U32, &motion::Z_ACCEL_HZ_PER_S, 100, 1000000, false},
    {"zHomeSpd", "Z (height)", "Homing speed", "Hz", Type::U32, &motion::Z_HOMING_SPEED_HZ, 100, 50000, false},
    {"zBackoff", "Z (height)", "Homing backoff", "steps", Type::I32, &motion::Z_HOMING_BACKOFF_STEPS, 0, 100000, false},
    {"zHomeFwd", "Z (height)", "Home in forward direction", "", Type::BOOL, &motion::Z_HOME_DIR_FORWARD, 0, 1, true},
    {"zMinMm", "Z (height)", "Soft limit min", "mm", Type::F32, &motion::Z_MIN_MM, -500, 500, false},
    {"zMaxMm", "Z (height)", "Soft limit max", "mm", Type::F32, &motion::Z_MAX_MM, -500, 500, false},

    // --- Shots ---
    {"shotLinAccel", "Shots", "LINEAR accel multiplier", "x", Type::F32, &motion::SHOT_LINEAR_ACCEL_MULTIPLIER, 1, 50, false},
    {"shotSegs", "Shots", "S-curve segments", "", Type::U8, &motion::SHOT_SCURVE_SEGMENTS, 2, 64, false},
    {"shotMinSegMs", "Shots", "Min segment duration", "ms", Type::U32, &motion::SHOT_SCURVE_MIN_SEGMENT_MS, 5, 1000, false},

    // --- Interface ---
    {"btnDebounce", "Interface", "Button debounce", "ms", Type::U32, &ui::BUTTON_DEBOUNCE_MS, 1, 500, false},
    {"btnLongMs", "Interface", "Long-press threshold", "ms", Type::U32, &ui::BUTTON_LONG_PRESS_MS, 100, 5000, false},
    {"oledRefresh", "Interface", "Display refresh interval", "ms", Type::U32, &ui::OLED_REFRESH_INTERVAL_MS, 20, 2000, false},
};

const uint16_t kSettingsCount = sizeof(kSettings) / sizeof(kSettings[0]);

namespace {
Preferences prefs;
constexpr const char *kNvsNamespace = "sliderCfg";
}  // namespace

float getValue(const Desc &d) {
  switch (d.type) {
    case Type::F32: return *static_cast<float *>(d.ptr);
    case Type::U32: return static_cast<float>(*static_cast<uint32_t *>(d.ptr));
    case Type::I32: return static_cast<float>(*static_cast<int32_t *>(d.ptr));
    case Type::U16: return static_cast<float>(*static_cast<uint16_t *>(d.ptr));
    case Type::U8:  return static_cast<float>(*static_cast<uint8_t *>(d.ptr));
    case Type::BOOL: return *static_cast<bool *>(d.ptr) ? 1.0f : 0.0f;
  }
  return 0.0f;
}

bool setValue(const Desc &d, float value) {
  if (isnan(value) || isinf(value)) return false;
  if (value < d.min) value = d.min;
  if (value > d.max) value = d.max;

  switch (d.type) {
    case Type::F32: *static_cast<float *>(d.ptr) = value; break;
    case Type::U32: *static_cast<uint32_t *>(d.ptr) = static_cast<uint32_t>(lroundf(value)); break;
    case Type::I32: *static_cast<int32_t *>(d.ptr) = static_cast<int32_t>(lroundf(value)); break;
    case Type::U16: *static_cast<uint16_t *>(d.ptr) = static_cast<uint16_t>(lroundf(value)); break;
    case Type::U8:  *static_cast<uint8_t *>(d.ptr) = static_cast<uint8_t>(lroundf(value)); break;
    case Type::BOOL: *static_cast<bool *>(d.ptr) = value >= 0.5f; break;
  }
  return true;
}

const Desc *find(const char *key) {
  for (uint16_t i = 0; i < kSettingsCount; i++) {
    if (strcmp(kSettings[i].key, key) == 0) return &kSettings[i];
  }
  return nullptr;
}

void recomputeDerived() {
  // Microstepping is one physical driver setting; the per-axis mirrors exist
  // only so step math reads naturally per axis.
  mech::SLIDE_MICROSTEPPING = tmc::MICROSTEPS;
  mech::ROTARY_MICROSTEPPING = tmc::MICROSTEPS;
  mech::Z_MICROSTEPPING = tmc::MICROSTEPS;

  mech::SLIDE_MM_PER_REV = mech::SLIDE_BELT_PITCH_MM * mech::SLIDE_PULLEY_TEETH;

  // Guard the divisions: a zero here would make every position NaN, and
  // these inputs are user-editable.
  mech::SLIDE_STEPS_PER_MM =
      mech::SLIDE_MM_PER_REV > 0.0f
          ? (mech::MOTOR_STEPS_PER_REV * mech::SLIDE_MICROSTEPPING) / mech::SLIDE_MM_PER_REV
          : 1.0f;
  mech::ROTARY_STEPS_PER_DEGREE =
      (mech::MOTOR_STEPS_PER_REV * mech::ROTARY_MICROSTEPPING * mech::ROTARY_BELT_RATIO) / 360.0f;
  mech::Z_STEPS_PER_MM =
      mech::Z_LEAD_MM > 0.0f
          ? (mech::MOTOR_STEPS_PER_REV * mech::Z_MICROSTEPPING) / mech::Z_LEAD_MM
          : 1.0f;
}

void begin() {
  if (!prefs.begin(kNvsNamespace, /*readOnly=*/false)) {
    Serial.println("WARNING: NVS open failed — settings will not persist");
    recomputeDerived();
    return;
  }

  uint16_t restored = 0;
  for (uint16_t i = 0; i < kSettingsCount; i++) {
    const Desc &d = kSettings[i];
    if (!prefs.isKey(d.key)) continue;  // never saved: keep compiled default
    setValue(d, prefs.getFloat(d.key, getValue(d)));
    restored++;
  }

  recomputeDerived();
  Serial.printf("Settings: %u of %u restored from NVS (rest at defaults)\n", restored,
                kSettingsCount);
}

void saveAll() {
  for (uint16_t i = 0; i < kSettingsCount; i++) {
    prefs.putFloat(kSettings[i].key, getValue(kSettings[i]));
  }
  Serial.printf("Settings: %u values saved to NVS\n", kSettingsCount);
}

void resetToDefaults() {
  prefs.clear();
  Serial.println("Settings: NVS cleared — compiled defaults apply after reboot");
}

}  // namespace settings
