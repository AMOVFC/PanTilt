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

constexpr uint8_t AXIS_SLIDE = 0;
constexpr uint8_t AXIS_PAN = 1;
constexpr uint8_t AXIS_TILT = 2;
constexpr uint8_t AXIS_AUX = 3;
}  // namespace fw

// ---------- Pin map (defaults) ----------
namespace pins {
// U1..U4 STEP/DIR. Enable is one shared active-low line across all four.
constexpr uint8_t SLIDE_STEP = 4, SLIDE_DIR = 5;
constexpr uint8_t PAN_STEP = 6, PAN_DIR = 7;
constexpr uint8_t TILT_STEP = 8, TILT_DIR = 9;
constexpr uint8_t AUX_STEP = 47, AUX_DIR = 48;
constexpr uint8_t DRIVER_EN = 10;

// Shared half-duplex TMC2209 UART. TX goes to header pin 12 (through the
// module's onboard 1k), RX to pin 11 (PDN, direct) -- both are the same
// PDN_UART node on the chip.
constexpr uint8_t TMC_TX = 41;
constexpr uint8_t TMC_RX = 42;

constexpr uint8_t I2C_MUX_SDA = 11;   // bus A: TCA9548A -> AS5600s
constexpr uint8_t I2C_MUX_SCL = 12;
constexpr uint8_t I2C_OLED_SDA = 13;  // bus B: OLED, isolated from the mux bus
constexpr uint8_t I2C_OLED_SCL = 14;

// One quadrature encoder per axis (J29..J32). The board leaves each
// encoder's push switch as a no-connect, so encoders are A/B only.
constexpr uint8_t ENC_SLIDE_A = 15, ENC_SLIDE_B = 16;
constexpr uint8_t ENC_PAN_A = 17, ENC_PAN_B = 18;
constexpr uint8_t ENC_TILT_A = 21, ENC_TILT_B = 38;
// GPIO19/20 are the ESP32-S3's native USB D-/D+. They work as an encoder
// input on a DevKitC-1 (which enumerates over the separate UART bridge),
// but the USB PHY holds them, so this encoder ships disabled by default.
constexpr uint8_t ENC_AUX_A = 19, ENC_AUX_B = 20;

constexpr uint8_t LIMIT_SLIDE_MIN = 39;  // idle HIGH, LOW = triggered
constexpr uint8_t LIMIT_SLIDE_MAX = 40;

// Transport buttons, all active-low to GND on internal pull-ups.
constexpr uint8_t BTN_SET_KEYFRAME = 1;
constexpr uint8_t BTN_CLEAR_KEYFRAME = 3;
constexpr uint8_t BTN_PLAY_PAUSE = 45;
constexpr uint8_t BTN_RESET = 46;
}  // namespace pins

// ---------- I2C ----------
namespace i2c_addr {
constexpr uint8_t TCA9548A = 0x70;
constexpr uint8_t AS5600 = 0x36;
constexpr uint8_t OLED = 0x3C;
}  // namespace i2c_addr

namespace mux_channel {
constexpr uint8_t PAN = 0;
constexpr uint8_t TILT = 1;
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
constexpr float SLIDE_BELT_PITCH_MM = 2.0f;    // GT2
constexpr float SLIDE_PULLEY_TEETH = 20.0f;
constexpr float ROTARY_GEAR_RATIO = 4.0f;      // 1:1 jackshaft x 20T:80T
}  // namespace mech

// ---------- Motion ----------
namespace motion {
constexpr float SLIDE_MAX_SPEED_MM_S = 200.0f;
constexpr float SLIDE_ACCEL_MM_S2 = 500.0f;
constexpr float SLIDE_HOMING_SPEED_MM_S = 37.5f;
constexpr float SLIDE_HOMING_BACKOFF_MM = 5.0f;
constexpr float SLIDE_MIN_MM = 0.0f;
constexpr float SLIDE_MAX_MM = 800.0f;  // measure your rail and set this

constexpr float ROTARY_MAX_SPEED_DEG_S = 45.0f;
constexpr float ROTARY_ACCEL_DEG_S2 = 90.0f;

// Neither pan nor tilt has a limit switch, so these soft limits are the only
// thing stopping the operator from winding cabling past the mechanical
// range. Placeholders -- tune to the as-built range in the web UI.
constexpr float PAN_MIN_DEG = -170.0f, PAN_MAX_DEG = 170.0f;
constexpr float TILT_MIN_DEG = -45.0f, TILT_MAX_DEG = 90.0f;

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
