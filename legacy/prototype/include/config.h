#pragma once
// Centralized firmware config for the ORIGINAL PROTOTYPE build: 3 axes
// (slide/pan/tilt), 3 TMC2209s on generic breakout boards, 3 dedicated
// per-axis encoders, 2 AS5600s (pan/tilt), limit switches on slide only.
// No Z axis, no programmed "shot" keyframe system (see prototype/README.md).
//
// This is a sibling of the 4-axis build's include/config.h, not a fork of
// it: every namespace/name that also exists in the final rig's config.h
// means the same thing here, so RotaryAxis/SlideAxis/Mux/Settings/WebConfig
// port over with no logic changes. Only the things that differ because the
// hardware differs (pin map, driver count, no Z) are actually different.
//
// See prototype/README.md for the full written wiring diagram this matches.
//
// Two kinds of value live here, same convention as the final rig:
//
//   constexpr  — physical facts about how the board is wired and what parts
//                are fitted. Compile-time, not exposed to the web UI.
//   extern     — tunables, defined in Settings.cpp, loaded from NVS at boot
//                and editable live over the web UI.
//
// Now includes the Shot/ShotSequencer keyframe system (motion::SHOT_*
// below): same duration-matching + S-curve algorithm as the final rig,
// scoped to slide/pan/tilt so movements meant for the final build can be
// authored, tuned, and played back here before Z hardware exists.

#include <cstdint>

// ---------- Pin map (physical wiring — compile-time) ----------
// Deliberately kept numerically identical to the final rig's config.h
// wherever the same signal exists (STEP/DIR, mux bus, OLED bus, slide
// encoder+push, slide limits, TMC UART, driver EN). The final rig's
// shared angle-encoder + axis-select scheme is dropped in favor of one
// dedicated encoder per rotary axis; the pins that scheme used (18/21/38)
// become the PAN encoder, and the pins the final rig spent on the Z axis
// (1/2/42, freed here since there's no Z motor) become the TILT encoder.
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
constexpr uint8_t I2C_OLED_SDA = 13;  // bus B: OLED (optional), isolated from mux bus
constexpr uint8_t I2C_OLED_SCL = 14;

constexpr uint8_t SLIDE_ENC_A = 15;
constexpr uint8_t SLIDE_ENC_B = 16;
constexpr uint8_t SLIDE_ENC_PUSH = 17;  // short: BLE resync, long: toggle record

constexpr uint8_t PAN_ENC_A = 18;
constexpr uint8_t PAN_ENC_B = 21;
constexpr uint8_t PAN_ENC_PUSH = 38;  // short press: recenter pan to 0deg

constexpr uint8_t TILT_ENC_A = 1;
constexpr uint8_t TILT_ENC_B = 2;
constexpr uint8_t TILT_ENC_PUSH = 42;  // short press: recenter tilt to 0deg

constexpr uint8_t LIMIT_SLIDE_MIN = 39;  // idle HIGH, LOW = triggered, no wiring
constexpr uint8_t LIMIT_SLIDE_MAX = 40;

// Shared half-duplex UART bus to all 3 TMC2209 UART pads. Unlike the final
// rig's bare stepstick carriers, these breakout boards expose PDN_UART on
// a header pin, so this is a plain Dupont-wire connection — no bodge wire
// to the driver IC needed. Still follow the standard single-wire scheme:
// ESP32 TX -> ~1k resistor -> shared bus -> all 3 PDN_UART pins; ESP32 RX
// tied directly to the same bus. (Skip the resistor only if your breakout
// already has one in-line with its UART pin — check the silkscreen/schematic.)
constexpr uint8_t TMC_UART_RX = 41;
constexpr uint8_t TMC_UART_TX = 47;
// GPIO48 is left free (spare button, DIAG breakout for StallGuard, etc).
}  // namespace pins

// ---------- I2C addressing (fixed by the parts — compile-time) ----------
namespace i2c_addr {
constexpr uint8_t TCA9548A = 0x70;
constexpr uint8_t AS5600 = 0x36;
}  // namespace i2c_addr

namespace mux_channel {
constexpr uint8_t PAN = 0;
constexpr uint8_t TILT = 1;
}  // namespace mux_channel

// ---------- TMC2209 UART configuration ----------
// All 3 drivers run in UART mode on one shared half-duplex bus (see
// pins::TMC_UART_*). In UART mode MS1/MS2 stop selecting microstepping and
// become ADDRESS straps instead (addr = MS2<<1 | MS1):
//   Slide: MS1=LOW,  MS2=LOW  -> addr 0
//   Pan:   MS1=HIGH, MS2=LOW  -> addr 1
//   Tilt:  MS1=LOW,  MS2=HIGH -> addr 2
// Current and microstepping are set by firmware at boot (TmcDrivers.cpp),
// then read back and verified against the live driver register.
namespace tmc {
// Physical: sense resistor value and the address straps wired per driver.
// 0.11ohm matches BigTreeTech/Watterott TMC2209 V1.2/1.3 breakout boards —
// verify against your specific board's sense resistor (check silkscreen or
// datasheet; some clones use 0.15ohm and current math will be wrong if so).
constexpr float R_SENSE_OHMS = 0.110f;
constexpr uint8_t ADDR_SLIDE = 0;
constexpr uint8_t ADDR_PAN = 1;
constexpr uint8_t ADDR_TILT = 2;

// Tunable: written to the drivers at boot.
extern uint16_t MICROSTEPS;
extern uint16_t SLIDE_RMS_MA;
extern uint16_t PAN_RMS_MA;
extern uint16_t TILT_RMS_MA;
extern float HOLD_CURRENT_FRACTION;
}  // namespace tmc

// ---------- Mechanical ratios ----------
// Microstepping is firmware-set over UART at boot (tmc::MICROSTEPS) — the
// *_MICROSTEPPING values below mirror it so step math can never desync from
// the physical driver state. The *_STEPS_PER_* values are DERIVED:
// recomputed by settings::recomputeDerived() whenever an input changes.
namespace mech {
extern float MOTOR_STEPS_PER_REV;  // 1.8 deg NEMA17 = 200

extern uint16_t SLIDE_MICROSTEPPING;  // derived from tmc::MICROSTEPS
extern float SLIDE_BELT_PITCH_MM;     // GT2 = 2.0
extern float SLIDE_PULLEY_TEETH;
extern float SLIDE_MM_PER_REV;    // derived
extern float SLIDE_STEPS_PER_MM;  // derived

extern uint16_t ROTARY_MICROSTEPPING;  // derived from tmc::MICROSTEPS
extern float ROTARY_BELT_RATIO;        // reduction between motor and pan/tilt shaft
extern float ROTARY_STEPS_PER_DEGREE;  // derived
}  // namespace mech

// ---------- Motion tuning ----------
namespace motion {
extern bool SLIDE_DIR_INVERT;  // flip if jog CW moves the wrong way
extern uint32_t SLIDE_MAX_SPEED_HZ;
extern uint32_t SLIDE_ACCEL_HZ_PER_S;
extern uint32_t SLIDE_HOMING_SPEED_HZ;
extern int32_t SLIDE_HOMING_BACKOFF_STEPS;
extern bool SLIDE_HOME_TOWARD_MIN;  // which switch homing seeks

// Encoder counts (post-detent-divisor) needed to reach SLIDE_MAX_SPEED_HZ.
// Slide encoder detent counts, not stepper steps — unaffected by microstepping.
extern int32_t JOG_COUNTS_PER_MAX_SPEED;

extern uint32_t ROTARY_MAX_SPEED_HZ;
extern uint32_t ROTARY_ACCEL_HZ_PER_S;
extern float ANGLE_DEG_PER_CLICK;  // pan/tilt encoder detent -> degrees

// Neither pan nor tilt has a limit switch, so these firmware soft limits are
// the only thing stopping the operator from jogging past the mechanical
// range. Placeholder values — MUST be tuned to the as-built mechanical range.
extern float PAN_MIN_DEG;
extern float PAN_MAX_DEG;
extern float TILT_MIN_DEG;
extern float TILT_MAX_DEG;

// Periodic open-loop drift check: lost steps are caught by comparing
// against the AS5600. Only runs while the axis is idle.
extern uint32_t ROTARY_DRIFT_CHECK_INTERVAL_MS;
extern int32_t ROTARY_DRIFT_THRESHOLD_STEPS;

// Programmed shots (see Shot.h): EaseType::LINEAR moves use this multiplier
// on the axis's normal configured acceleration, shrinking the ramp phase to
// near-negligible so the move reads as a sharp snap rather than a cinematic
// ease.
extern float SHOT_LINEAR_ACCEL_MULTIPLIER;

// EaseType::EASE_IN_OUT moves are driven as a jerk-limited S-curve: the
// move's quintic position profile is sampled into this many waypoints and
// replayed as a timed sequence of short trapezoidal moves (see Shot.cpp).
// SHOT_SCURVE_MIN_SEGMENT_MS floors the segment count on short/fast moves
// so segments never get so brief they're meaningless.
extern uint8_t SHOT_SCURVE_SEGMENTS;
extern uint32_t SHOT_SCURVE_MIN_SEGMENT_MS;
}  // namespace motion

// ---------- UI timing ----------
namespace ui {
extern uint32_t BUTTON_DEBOUNCE_MS;
extern uint32_t BUTTON_LONG_PRESS_MS;
extern uint32_t OLED_REFRESH_INTERVAL_MS;
}  // namespace ui

// ---------- OLED (fitted part — compile-time, optional) ----------
// Not part of the requested BOM, but wired on the same bus/pins as the
// final rig and left in: Display::begin() no-ops cleanly if nothing is
// fitted, so there's no cost to leaving the code in for a build without one.
namespace oled {
constexpr uint8_t WIDTH = 128;
constexpr uint8_t HEIGHT = 64;
constexpr uint8_t I2C_ADDR = 0x3C;  // common default; verify against the module
}  // namespace oled

// ---------- BLE HID ----------
namespace ble {
constexpr const char *DEVICE_NAME = "Camera Slider Controller (Proto)";
constexpr const char *MANUFACTURER = "DIY";
constexpr uint8_t BATTERY_LEVEL = 100;
}  // namespace ble

// ---------- Web config AP ----------
namespace webcfg {
constexpr const char *AP_SSID = "CameraSliderProto";
constexpr const char *AP_PASSWORD = "slider1234";  // change before first use
constexpr uint16_t HTTP_PORT = 80;
}  // namespace webcfg

// ---------- AS5600 zero-offset calibration ----------
// One-time-per-build value: magnet mount angle vs. true mechanical zero.
// Measure once per physical unit, then set from the web UI.
namespace calibration {
extern float PAN_ZERO_OFFSET_DEG;
extern float TILT_ZERO_OFFSET_DEG;
}  // namespace calibration
