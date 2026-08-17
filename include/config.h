#pragma once
// Centralized firmware config: pin map, mechanical ratios, motion limits.
// See hardware/final_wiring_diagram_v3.svg for the wiring this matches,
// and docs/project-brief.md §5 for the source spec.

#include <cstdint>

// ---------- Pin map ----------
namespace pins {
constexpr uint8_t SLIDE_STEP = 4;
constexpr uint8_t SLIDE_DIR = 5;
constexpr uint8_t PAN_STEP = 6;
constexpr uint8_t PAN_DIR = 7;
constexpr uint8_t TILT_STEP = 8;
constexpr uint8_t TILT_DIR = 9;
constexpr uint8_t DRIVER_EN = 10;  // shared, active low, all 3 TMC2209s

constexpr uint8_t I2C_MUX_SDA = 11;   // bus A: TCA9548A -> pan/tilt AS5600
constexpr uint8_t I2C_MUX_SCL = 12;
constexpr uint8_t I2C_OLED_SDA = 13;  // bus B: OLED, kept isolated from mux bus
constexpr uint8_t I2C_OLED_SCL = 14;

constexpr uint8_t JOG_ENC_A = 15;
constexpr uint8_t JOG_ENC_B = 16;
constexpr uint8_t JOG_ENC_PUSH = 17;  // BLE record-state resync

constexpr uint8_t ANGLE_ENC_A = 18;
constexpr uint8_t ANGLE_ENC_B = 21;
constexpr uint8_t ANGLE_ENC_PUSH = 38;  // pan/tilt axis select

constexpr uint8_t LIMIT_SLIDE_MIN = 39;  // idle HIGH, LOW = triggered, no wiring
constexpr uint8_t LIMIT_SLIDE_MAX = 40;
}  // namespace pins

// ---------- I2C addressing ----------
namespace i2c_addr {
constexpr uint8_t TCA9548A = 0x70;
constexpr uint8_t AS5600 = 0x36;
}  // namespace i2c_addr

namespace mux_channel {
constexpr uint8_t PAN = 0;
constexpr uint8_t TILT = 1;
}  // namespace mux_channel

// ---------- Mechanical ratios ----------
// Microstepping values below are FIXED BY MS1/MS2 PIN-STRAPPING on each
// TMC2209 (standalone mode, no UART readback). If you change the physical
// jumpers, you MUST update these to match or all position math is wrong.
namespace mech {
constexpr float MOTOR_STEPS_PER_REV = 200.0f;  // 1.8 deg NEMA17

constexpr uint16_t SLIDE_MICROSTEPPING = 16;
constexpr float SLIDE_BELT_PITCH_MM = 2.0f;   // GT2
constexpr float SLIDE_PULLEY_TEETH = 20.0f;
constexpr float SLIDE_MM_PER_REV = SLIDE_BELT_PITCH_MM * SLIDE_PULLEY_TEETH;
constexpr float SLIDE_STEPS_PER_MM =
    (MOTOR_STEPS_PER_REV * SLIDE_MICROSTEPPING) / SLIDE_MM_PER_REV;

constexpr uint16_t ROTARY_MICROSTEPPING = 16;
constexpr float ROTARY_BELT_RATIO = 4.0f;  // 1:1 jackshaft x 20T:80T final stage
constexpr float ROTARY_STEPS_PER_DEGREE =
    (MOTOR_STEPS_PER_REV * ROTARY_MICROSTEPPING * ROTARY_BELT_RATIO) / 360.0f;
}  // namespace mech

// ---------- Motion tuning ----------
namespace motion {
constexpr bool SLIDE_DIR_INVERT = false;  // flip if jog CW moves the wrong way
constexpr uint32_t SLIDE_MAX_SPEED_HZ = 8000;
constexpr uint32_t SLIDE_ACCEL_HZ_PER_S = 20000;
constexpr uint32_t SLIDE_HOMING_SPEED_HZ = 1500;
constexpr int32_t SLIDE_HOMING_BACKOFF_STEPS = 200;
constexpr bool SLIDE_HOME_TOWARD_MIN = true;  // which switch homing seeks

// Encoder counts (post-detent-divisor) needed to reach SLIDE_MAX_SPEED_HZ.
constexpr int32_t JOG_COUNTS_PER_MAX_SPEED = 20;

constexpr uint32_t ROTARY_MAX_SPEED_HZ = 4000;
constexpr uint32_t ROTARY_ACCEL_HZ_PER_S = 8000;
constexpr float ANGLE_DEG_PER_CLICK = 0.5f;  // angle-encoder detent -> degrees

// Neither pan nor tilt has a limit switch, so these firmware soft limits are
// the only thing stopping the operator from jogging past the mechanical
// range (and winding up cabling indefinitely on pan). Placeholder values —
// MUST be tuned to the as-built mechanical range before real use.
constexpr float PAN_MIN_DEG = -170.0f;
constexpr float PAN_MAX_DEG = 170.0f;
constexpr float TILT_MIN_DEG = -45.0f;
constexpr float TILT_MAX_DEG = 90.0f;

// Periodic open-loop drift check: with standalone TMC2209 (no StallGuard),
// lost steps are only caught by comparing against the AS5600. Only runs
// while the axis is idle, so a correction never yanks a target mid-move.
constexpr uint32_t ROTARY_DRIFT_CHECK_INTERVAL_MS = 2000;
constexpr int32_t ROTARY_DRIFT_THRESHOLD_STEPS = 20;
}  // namespace motion

// ---------- UI timing ----------
namespace ui {
constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;
constexpr uint32_t BUTTON_LONG_PRESS_MS = 600;
constexpr uint32_t OLED_REFRESH_INTERVAL_MS = 200;
}  // namespace ui

// ---------- OLED ----------
namespace oled {
constexpr uint8_t WIDTH = 128;
constexpr uint8_t HEIGHT = 64;
constexpr uint8_t I2C_ADDR = 0x3C;  // common default; verify against the module
}  // namespace oled

// ---------- BLE HID ----------
namespace ble {
constexpr const char *DEVICE_NAME = "Camera Slider Controller";
constexpr const char *MANUFACTURER = "DIY";
constexpr uint8_t BATTERY_LEVEL = 100;
}  // namespace ble

// ---------- AS5600 zero-offset calibration ----------
// One-time-per-build value: magnet mount angle vs. true mechanical zero.
// Measure once per physical unit and hardcode here.
namespace calibration {
constexpr float PAN_ZERO_OFFSET_DEG = 0.0f;   // TODO: measure on hardware
constexpr float TILT_ZERO_OFFSET_DEG = 0.0f;  // TODO: measure on hardware
}  // namespace calibration
