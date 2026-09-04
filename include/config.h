#pragma once
// Compile-time hardware defaults for the 3+1-axis camera slider.
//
// NOTHING in this file is authoritative at run time. It only seeds the
// factory defaults that Settings falls back to when /config.json is missing
// or unreadable; every value here can be changed from the web UI and is
// persisted to LittleFS. Reflashing does NOT reset a saved config -- use
// "Factory reset" in the web UI's System tab for that.
//
// Pin numbers match pantiltslide/tools/gen_wiring.py, which is the generator
// that produced the current schematic. hardware/final_wiring_diagram_v3.svg
// predates the 4-axis / UART revision and is stale.

#include <cstdint>

namespace fw {
constexpr const char *VERSION = "2.0.0";
constexpr uint16_t SETTINGS_VERSION = 2;

constexpr uint8_t AXIS_COUNT = 4;
constexpr uint8_t ENCODER_COUNT = 4;
constexpr uint8_t BUTTON_COUNT = 4;
constexpr uint8_t MAX_KEYFRAMES = 32;

// Steppers are identified by letter, matching the board silkscreen and the
// TMC2209 UART addresses. The axis each one drives is a property of the
// machine, not of the electronics, so it lives in the comment rather than in
// the name.
constexpr uint8_t AXIS_A = 0;  // slide  -- GT2 belt along the rail
constexpr uint8_t AXIS_B = 1;  // pan    -- 2-stage belt, AS5600 on mux ch0
constexpr uint8_t AXIS_C = 2;  // tilt   -- 2-stage belt, AS5600 on mux ch1
constexpr uint8_t AXIS_Z = 3;  // z      -- leadscrew column
}  // namespace fw

// ---------- Pin map (defaults) ----------
namespace pins {
// ---------------------------------------------------------------------------
// LOCKED PIN MAP -- hybrid controller, rev A.
//
// This must agree with hybrid/schematic/ exactly. Every value here is a
// physical fact about the board, not a preference: getting one wrong drives
// STEP pulses onto whatever else sits on that line. See docs/pinout.md for
// the full source/destination table.
//
// Free after this allocation: GPIO0 (BOOT strap) only.
// (plus GPIO0 = BOOT strap, GPIO43/44 = USB-serial console).
// ---------------------------------------------------------------------------

// Stepper A/B/C/Z STEP+DIR. Enable is one shared active-low line across all four.
constexpr uint8_t A_STEP = 4, A_DIR = 5;   // stepper A (slide), addr 0
constexpr uint8_t B_STEP = 6, B_DIR = 7;   // stepper B (pan),   addr 1
constexpr uint8_t C_STEP = 8, C_DIR = 9;   // stepper C (tilt),  addr 2
// Z is on GPIO1/2, NOT 47/48. 47 is the TMC UART TX; putting Z STEP there
// would tie two outputs together.
constexpr uint8_t Z_STEP = 1, Z_DIR = 2;   // stepper Z (z),     addr 3
constexpr uint8_t DRIVER_EN = 10;

// Shared half-duplex TMC2209 UART. TX joins the bus through a 1k series
// resistor (R10), RX sits directly on it; both are the same PDN_UART node.
constexpr uint8_t TMC_TX = 47;
constexpr uint8_t TMC_RX = 41;

constexpr uint8_t I2C_MUX_SDA = 11;   // bus A: TCA9548A -> AS5600s
constexpr uint8_t I2C_MUX_SCL = 12;
constexpr uint8_t I2C_OLED_SDA = 13;  // bus B: OLED, isolated from the mux bus
constexpr uint8_t I2C_OLED_SCL = 14;

// One encoder per stepper, A/B only -- the connectors leave each encoder's
// push switch as a no-connect, so the transport buttons below are separate
// parts rather than the wheels' own switches.
constexpr uint8_t ENC_A_A = 15, ENC_A_B = 16;   // J29
constexpr uint8_t ENC_B_A = 17, ENC_B_B = 18;   // J30
constexpr uint8_t ENC_C_A = 21, ENC_C_B = 38;   // J31
// GPIO19/20 are the native USB D-/D+. Wired here because the DevKitC-1
// enumerates over its separate UART bridge, but it does rule out native USB.
constexpr uint8_t ENC_Z_A = 19, ENC_Z_B = 20;   // J32

constexpr uint8_t LIMIT_A_MIN = 39;  // J26, idle HIGH, LOW = triggered
constexpr uint8_t LIMIT_A_MAX = 40;  // J27
// Stepper Z has a single switch. Which end it represents is a mechanical
// choice, not an electrical one, so it is runtime config: set Homing to
// limit_min or limit_max in the web UI. Defaults to limit_min.
// (The board connector is silkscreened "Aux_Max" -- worth renaming to Z_Home.)
constexpr uint8_t LIMIT_Z_HOME = 42;  // J41

// Transport buttons, active-low to GND on internal pull-ups.
// GPIO48 also drives the DevKitC-1's onboard RGB LED; harmless as a switch
// input (the WS2812 data pin is high-Z) as long as no LED library runs.
constexpr uint8_t BTN_SET_KEYFRAME = 48;    // J33
constexpr uint8_t BTN_CLEAR_KEYFRAME = 3;   // J34
constexpr uint8_t BTN_PLAY_PAUSE = 45;      // J35
constexpr uint8_t BTN_RESET = 46;           // J36
}  // namespace pins

// ---------- I2C ----------
namespace i2c_addr {
constexpr uint8_t TCA9548A = 0x70;
constexpr uint8_t AS5600 = 0x36;
constexpr uint8_t OLED = 0x3C;
}  // namespace i2c_addr

namespace mux_channel {
constexpr uint8_t B_PAN = 0;   // stepper B / pan joint AS5600
constexpr uint8_t C_TILT = 1;  // stepper C / tilt joint AS5600
}  // namespace mux_channel

// ---------- TMC2209 ----------
namespace tmc {
constexpr uint32_t BAUD = 115200;
// SilentStepStick sense resistors. Wrong value here scales motor current
// proportionally wrong, so verify against your actual modules.
constexpr float R_SENSE = 0.11f;
constexpr uint16_t RUN_CURRENT_MA = 800;
constexpr uint8_t HOLD_CURRENT_PCT = 50;
constexpr uint16_t MICROSTEPS = 16;
}  // namespace tmc

// ---------- Mechanics ----------
namespace mech {
constexpr float MOTOR_STEPS_PER_REV = 200.0f;  // 1.8 deg NEMA17
constexpr float A_BELT_PITCH_MM = 2.0f;    // GT2
constexpr float A_PULLEY_TEETH = 20.0f;
constexpr float ROTARY_GEAR_RATIO = 4.0f;      // 1:1 jackshaft x 20T:80T
// Z is a leadscrew. The linear axis model computes mm/rev as
// belt_pitch * pulley_teeth, so a lead of 8mm is expressed as 8.0 x 1.
constexpr float Z_LEAD_MM = 8.0f;
}  // namespace mech

// ---------- Motion ----------
namespace motion {
constexpr float A_MAX_SPEED_MM_S = 200.0f;
constexpr float A_ACCEL_MM_S2 = 500.0f;
constexpr float A_HOMING_SPEED_MM_S = 37.5f;
constexpr float A_HOMING_BACKOFF_MM = 5.0f;
constexpr float A_MIN_MM = 0.0f;
constexpr float A_MAX_MM = 800.0f;  // measure your rail and set this

constexpr float Z_MAX_SPEED_MM_S = 15.0f;
constexpr float Z_ACCEL_MM_S2 = 60.0f;
constexpr float Z_MIN_MM = 0.0f;
constexpr float Z_MAX_MM = 150.0f;      // measure your column and set this
constexpr float Z_HOMING_SPEED_MM_S = 5.0f;
constexpr float Z_HOMING_BACKOFF_MM = 3.0f;

constexpr float ROTARY_MAX_SPEED_DEG_S = 45.0f;
constexpr float ROTARY_ACCEL_DEG_S2 = 90.0f;

// Neither pan nor tilt has a limit switch, so these soft limits are the only
// thing stopping the operator from winding cabling past the mechanical
// range. Placeholders -- tune to the as-built range in the web UI.
constexpr float B_MIN_DEG = -170.0f, B_MAX_DEG = 170.0f;
constexpr float C_MIN_DEG = -45.0f, C_MAX_DEG = 90.0f;

// Open-loop drift check against the AS5600. Runs only while the axis is
// idle, so a correction never yanks a target mid-move.
constexpr uint32_t DRIFT_CHECK_INTERVAL_MS = 2000;
constexpr float DRIFT_THRESHOLD_DEG = 1.0f;

// FastAccelStepper's practical ceiling on an ESP32-S3.
constexpr uint32_t MAX_STEP_RATE_HZ = 200000;
}  // namespace motion

// ---------- Controls ----------
namespace controls {
// Full-quadrature counts per mechanical detent on a typical EC11 (one
// complete Gray-code cycle per click).
constexpr uint8_t COUNTS_PER_DETENT = 4;
constexpr float DEG_PER_DETENT = 0.5f;
constexpr float MM_PER_DETENT = 1.0f;
constexpr int32_t COUNTS_FOR_MAX_SPEED = 20;
}  // namespace controls

// ---------- UI ----------
namespace ui {
constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;
constexpr uint32_t BUTTON_LONG_PRESS_MS = 600;
constexpr uint32_t OLED_REFRESH_INTERVAL_MS = 200;
constexpr uint32_t TELEMETRY_INTERVAL_MS = 150;
}  // namespace ui

namespace oled {
constexpr uint8_t WIDTH = 128;
constexpr uint8_t HEIGHT = 64;
}  // namespace oled

// ---------- Network ----------
namespace net {
constexpr const char *AP_SSID = "CamSlider";
constexpr const char *AP_PASS = "sliderpad";  // >=8 chars or the AP is open
constexpr const char *HOSTNAME = "camslider";
}  // namespace net

namespace ble {
constexpr const char *DEVICE_NAME = "Camera Slider Controller";
constexpr const char *MANUFACTURER = "DIY";
constexpr uint8_t BATTERY_LEVEL = 100;
}  // namespace ble
