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
constexpr uint8_t Z_STEP = 1;
constexpr uint8_t Z_DIR = 2;
constexpr uint8_t DRIVER_EN = 10;  // shared, active low, all 4 TMC2209s

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
constexpr uint8_t LIMIT_Z = 42;  // home reference, idle HIGH, LOW = triggered

// Shared half-duplex UART bus to all 4 TMC2209 PDN_UART pads (see tmc::
// below). Wiring: RX tied directly to the bus; TX joined to the bus through
// a ~1k resistor (standard TMC single-wire scheme). The Jeanoko carriers do
// NOT break out PDN_UART — this requires a soldered wire on each stepstick.
// GPIO48 remains the last free pin (reserved for an optional Z max switch).
constexpr uint8_t TMC_UART_RX = 41;
constexpr uint8_t TMC_UART_TX = 47;
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

// ---------- TMC2209 UART configuration ----------
// All 4 drivers run in UART mode on one shared half-duplex bus (see
// pins::TMC_UART_*). In UART mode MS1/MS2 stop selecting microstepping and
// become ADDRESS straps instead (addr = MS2<<1 | MS1):
//   Slide: MS1=LOW,  MS2=LOW  -> addr 0
//   Pan:   MS1=HIGH, MS2=LOW  -> addr 1
//   Tilt:  MS1=LOW,  MS2=HIGH -> addr 2
//   Z:     MS1=HIGH, MS2=HIGH -> addr 3
// Current and microstepping are set by firmware at boot (TmcDrivers.cpp),
// replacing the trimpot/jumper approach — MICROSTEPS below is the single
// source of truth for all step math, verified against the live driver
// register at startup instead of trusted blind.
namespace tmc {
constexpr float R_SENSE_OHMS = 0.110f;  // BigTreeTech TMC2209 V1.3 sense resistor
constexpr uint8_t ADDR_SLIDE = 0;
constexpr uint8_t ADDR_PAN = 1;
constexpr uint8_t ADDR_TILT = 2;
constexpr uint8_t ADDR_Z = 3;
constexpr uint16_t MICROSTEPS = 64;
// RMS run current per motor. PLACEHOLDERS at a conservative value for
// generic NEMA 17s — set per motor's rated current (typically 70-85% of the
// motor nameplate rating) once the actual motors are in hand.
constexpr uint16_t SLIDE_RMS_MA = 800;
constexpr uint16_t PAN_RMS_MA = 800;
constexpr uint16_t TILT_RMS_MA = 800;
constexpr uint16_t Z_RMS_MA = 800;
// Hold-current fraction of run current. Kept high on purpose: Z relies on
// holding torque if the leadscrew doesn't self-lock (brief §6.5), and EN is
// one shared line so this applies to all 4 drivers.
constexpr float HOLD_CURRENT_FRACTION = 0.7f;
}  // namespace tmc

// ---------- Mechanical ratios ----------
// Microstepping is firmware-set over UART at boot (tmc::MICROSTEPS) — the
// constants below alias it so step math can never desync from the physical
// driver state the way trimpot/jumper strapping could.
namespace mech {
constexpr float MOTOR_STEPS_PER_REV = 200.0f;  // 1.8 deg NEMA17

constexpr uint16_t SLIDE_MICROSTEPPING = tmc::MICROSTEPS;
constexpr float SLIDE_BELT_PITCH_MM = 2.0f;   // GT2
constexpr float SLIDE_PULLEY_TEETH = 20.0f;
constexpr float SLIDE_MM_PER_REV = SLIDE_BELT_PITCH_MM * SLIDE_PULLEY_TEETH;
constexpr float SLIDE_STEPS_PER_MM =
    (MOTOR_STEPS_PER_REV * SLIDE_MICROSTEPPING) / SLIDE_MM_PER_REV;

constexpr uint16_t ROTARY_MICROSTEPPING = tmc::MICROSTEPS;
constexpr float ROTARY_BELT_RATIO = 4.0f;  // 1:1 jackshaft x 20T:80T final stage
constexpr float ROTARY_STEPS_PER_DEGREE =
    (MOTOR_STEPS_PER_REV * ROTARY_MICROSTEPPING * ROTARY_BELT_RATIO) / 360.0f;

constexpr uint16_t Z_MICROSTEPPING = tmc::MICROSTEPS;
// Leadscrew lead (mm traveled per full revolution), confirmed against the
// as-built hardware. Note this makes Z the finest-resolution axis by a wide
// margin: 1600 steps/mm vs. the slide's 320, which is why Z's step-unit
// constants below look large relative to its modest physical speeds.
constexpr float Z_LEAD_MM = 8.0f;
constexpr float Z_STEPS_PER_MM = (MOTOR_STEPS_PER_REV * Z_MICROSTEPPING) / Z_LEAD_MM;
}  // namespace mech

// ---------- Motion tuning ----------
// Speed/accel/step-count constants below are all in stepper-driver step
// units (Hz, Hz/s, steps), so they scale linearly with mech::*_MICROSTEPPING
// (steps-per-physical-unit). All tuned at 1/16 originally; now carried
// forward x4 for the locked-in 1/64 (see mech:: above) so the real-world
// mm/s, deg/s, mm/s^2, deg/s^2, and physical tolerances stay the values
// they were tuned to, not silently a quarter of that.
namespace motion {
constexpr bool SLIDE_DIR_INVERT = false;  // flip if jog CW moves the wrong way
constexpr uint32_t SLIDE_MAX_SPEED_HZ = 32000;
constexpr uint32_t SLIDE_ACCEL_HZ_PER_S = 80000;
constexpr uint32_t SLIDE_HOMING_SPEED_HZ = 6000;
constexpr int32_t SLIDE_HOMING_BACKOFF_STEPS = 800;
constexpr bool SLIDE_HOME_TOWARD_MIN = true;  // which switch homing seeks

// Encoder counts (post-detent-divisor) needed to reach SLIDE_MAX_SPEED_HZ.
// Jog-knob detent counts, not stepper steps — unaffected by microstepping.
constexpr int32_t JOG_COUNTS_PER_MAX_SPEED = 20;

constexpr uint32_t ROTARY_MAX_SPEED_HZ = 16000;
constexpr uint32_t ROTARY_ACCEL_HZ_PER_S = 32000;
constexpr float ANGLE_DEG_PER_CLICK = 0.5f;  // angle-encoder detent -> degrees

// Neither pan nor tilt has a limit switch, so these firmware soft limits are
// the only thing stopping the operator from jogging past the mechanical
// range (and winding up cabling indefinitely on pan). Placeholder values —
// MUST be tuned to the as-built mechanical range before real use.
constexpr float PAN_MIN_DEG = -170.0f;
constexpr float PAN_MAX_DEG = 170.0f;
constexpr float TILT_MIN_DEG = -45.0f;
constexpr float TILT_MAX_DEG = 90.0f;

// Periodic open-loop drift check: lost steps are caught by comparing
// against the AS5600. (UART mode makes StallGuard available in principle,
// but the AS5600 comparison is better anyway — it measures actual position,
// not motor load.) Only runs while the axis is idle, so a correction never
// yanks a target mid-move.
constexpr uint32_t ROTARY_DRIFT_CHECK_INTERVAL_MS = 2000;
constexpr int32_t ROTARY_DRIFT_THRESHOLD_STEPS = 80;

// Z is a slow, low-duty-cycle axis (brief §2) — conservative speed/accel
// relative to slide/rotary, not a hardware ceiling. Physical equivalents at
// 1600 steps/mm are given since the step-unit numbers alone are misleading
// here (Z's steps/mm is 5x the slide's).
constexpr uint32_t Z_MAX_SPEED_HZ = 16000;      // 10 mm/s
constexpr uint32_t Z_ACCEL_HZ_PER_S = 32000;    // 20 mm/s^2
constexpr uint32_t Z_HOMING_SPEED_HZ = 4000;    // 2.5 mm/s
// Backoff must clear the switch's release travel, or checkLimitSwitch()
// keeps seeing it triggered and force-stops every subsequent move in that
// direction. 2mm is well clear of a typical microswitch's differential
// travel, and comparable to the slide's 2.5mm backoff.
constexpr int32_t Z_HOMING_BACKOFF_STEPS = 3200;  // 2 mm
// Which direction finds the home switch during the startup homing move.
// true = runForward(), false = runBackward() — flip if Z drives away from
// the switch instead of toward it on first power-up.
constexpr bool Z_HOME_DIR_FORWARD = false;

// Z has only one physical switch (home reference, see §2) — no second
// switch at full extension, unlike slide's min/max pair. Z_MAX_MM is
// therefore the ONLY thing stopping a bad keyframe from driving the
// carriage off the top of the 200mm rods; there is no hardware backstop
// behind it. Set to the as-built usable travel.
constexpr float Z_MIN_MM = 0.0f;
constexpr float Z_MAX_MM = 170.0f;

// Programmed shots (see Shot.h): EaseType::LINEAR moves use this multiplier
// on the axis's normal configured acceleration, shrinking the ramp phase to
// near-negligible so the move reads as a sharp snap rather than a cinematic
// ease.
constexpr float SHOT_LINEAR_ACCEL_MULTIPLIER = 6.0f;

// EaseType::EASE_IN_OUT moves are driven as a jerk-limited S-curve: the
// move's quintic position profile is sampled into this many waypoints and
// replayed as a timed sequence of short trapezoidal moves (see Shot.cpp).
// More segments track the curve more faithfully at the cost of more
// mid-move stepper command churn; 16 is a reasonable cinematic default.
// SHOT_SCURVE_MIN_SEGMENT_MS floors the segment count on short/fast moves
// so segments never get so brief they're meaningless.
constexpr uint8_t SHOT_SCURVE_SEGMENTS = 16;
constexpr uint32_t SHOT_SCURVE_MIN_SEGMENT_MS = 40;
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
