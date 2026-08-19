#pragma once
// Centralized firmware config: pin map, mechanical ratios, motion limits.
// See hardware/final_wiring_diagram_v3.svg for the wiring this matches,
// and docs/project-brief.md §5 for the source spec.
//
// Two kinds of value live here:
//
//   constexpr  — physical facts about how the board is wired and what parts
//                are fitted. Changing these in software does not rewire
//                anything, so they are compile-time and not exposed to the
//                web UI (see Settings.h for that reasoning).
//   extern     — tunables, defined in Settings.cpp, loaded from NVS at boot
//                and editable live over the web UI. Names are unchanged from
//                when these were constexpr, so call sites read the current
//                value with no syntax change.

#include <cstdint>

// ---------- Pin map (physical wiring — compile-time) ----------
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
// Physical: sense resistor value and the address straps soldered per driver.
constexpr float R_SENSE_OHMS = 0.110f;  // BigTreeTech TMC2209 V1.3 sense resistor
constexpr uint8_t ADDR_SLIDE = 0;
constexpr uint8_t ADDR_PAN = 1;
constexpr uint8_t ADDR_TILT = 2;
constexpr uint8_t ADDR_Z = 3;

// Tunable: written to the drivers at boot.
extern uint16_t MICROSTEPS;
extern uint16_t SLIDE_RMS_MA;
extern uint16_t PAN_RMS_MA;
extern uint16_t TILT_RMS_MA;
extern uint16_t Z_RMS_MA;
// Hold-current fraction of run current. Kept high on purpose: Z relies on
// holding torque if the leadscrew doesn't self-lock (brief §6.5), and EN is
// one shared line so this applies to all 4 drivers.
extern float HOLD_CURRENT_FRACTION;
}  // namespace tmc

// ---------- Mechanical ratios ----------
// Microstepping is firmware-set over UART at boot (tmc::MICROSTEPS) — the
// *_MICROSTEPPING values below mirror it so step math can never desync from
// the physical driver state the way trimpot/jumper strapping could.
// The *_STEPS_PER_* values are DERIVED: recomputed by
// settings::recomputeDerived() whenever an input changes. Never assign to
// them directly.
namespace mech {
extern float MOTOR_STEPS_PER_REV;  // 1.8 deg NEMA17 = 200

extern uint16_t SLIDE_MICROSTEPPING;  // derived from tmc::MICROSTEPS
extern float SLIDE_BELT_PITCH_MM;     // GT2 = 2.0
extern float SLIDE_PULLEY_TEETH;
extern float SLIDE_MM_PER_REV;   // derived
extern float SLIDE_STEPS_PER_MM;  // derived

extern uint16_t ROTARY_MICROSTEPPING;  // derived from tmc::MICROSTEPS
extern float ROTARY_BELT_RATIO;  // 1:1 jackshaft x 20T:80T final stage
extern float ROTARY_STEPS_PER_DEGREE;  // derived

extern uint16_t Z_MICROSTEPPING;  // derived from tmc::MICROSTEPS
// Leadscrew lead (mm traveled per full revolution), confirmed against the
// as-built hardware. Note this makes Z the finest-resolution axis by a wide
// margin: 1600 steps/mm vs. the slide's 320, which is why Z's step-unit
// constants below look large relative to its modest physical speeds.
extern float Z_LEAD_MM;
extern float Z_STEPS_PER_MM;  // derived
}  // namespace mech

// ---------- Motion tuning ----------
// Speed/accel/step-count constants below are all in stepper-driver step
// units (Hz, Hz/s, steps), so they scale linearly with mech::*_MICROSTEPPING
// (steps-per-physical-unit). Defaults were tuned at 1/16 and carried forward
// x4 for the 1/64 default, so the real-world mm/s, deg/s and physical
// tolerances stay the values they were tuned to. If you change microstepping
// from the web UI, these do NOT auto-scale — they are independent settings.
namespace motion {
extern bool SLIDE_DIR_INVERT;  // flip if jog CW moves the wrong way
extern uint32_t SLIDE_MAX_SPEED_HZ;
extern uint32_t SLIDE_ACCEL_HZ_PER_S;
extern uint32_t SLIDE_HOMING_SPEED_HZ;
extern int32_t SLIDE_HOMING_BACKOFF_STEPS;
extern bool SLIDE_HOME_TOWARD_MIN;  // which switch homing seeks

// Encoder counts (post-detent-divisor) needed to reach SLIDE_MAX_SPEED_HZ.
// Jog-knob detent counts, not stepper steps — unaffected by microstepping.
extern int32_t JOG_COUNTS_PER_MAX_SPEED;

extern uint32_t ROTARY_MAX_SPEED_HZ;
extern uint32_t ROTARY_ACCEL_HZ_PER_S;
extern float ANGLE_DEG_PER_CLICK;  // angle-encoder detent -> degrees

// Neither pan nor tilt has a limit switch, so these firmware soft limits are
// the only thing stopping the operator from jogging past the mechanical
// range (and winding up cabling indefinitely on pan). Placeholder values —
// MUST be tuned to the as-built mechanical range before real use.
extern float PAN_MIN_DEG;
extern float PAN_MAX_DEG;
extern float TILT_MIN_DEG;
extern float TILT_MAX_DEG;

// Periodic open-loop drift check: lost steps are caught by comparing
// against the AS5600. (UART mode makes StallGuard available in principle,
// but the AS5600 comparison is better anyway — it measures actual position,
// not motor load.) Only runs while the axis is idle, so a correction never
// yanks a target mid-move.
extern uint32_t ROTARY_DRIFT_CHECK_INTERVAL_MS;
extern int32_t ROTARY_DRIFT_THRESHOLD_STEPS;

// Z is a slow, low-duty-cycle axis (brief §2) — conservative speed/accel
// relative to slide/rotary, not a hardware ceiling. Physical equivalents at
// the default 1600 steps/mm are given since the step-unit numbers alone are
// misleading here (Z's steps/mm is 5x the slide's).
extern uint32_t Z_MAX_SPEED_HZ;      // default 10 mm/s
extern uint32_t Z_ACCEL_HZ_PER_S;    // default 20 mm/s^2
extern uint32_t Z_HOMING_SPEED_HZ;   // default 2.5 mm/s
// Backoff must clear the switch's release travel, or checkLimitSwitch()
// keeps seeing it triggered and force-stops every subsequent move in that
// direction. Default 2mm is well clear of a typical microswitch's
// differential travel, and comparable to the slide's 2.5mm backoff.
extern int32_t Z_HOMING_BACKOFF_STEPS;
// Which direction finds the home switch during the startup homing move.
// true = runForward(), false = runBackward() — flip if Z drives away from
// the switch instead of toward it on first power-up.
extern bool Z_HOME_DIR_FORWARD;

// Z has only one physical switch (home reference, see §2) — no second
// switch at full extension, unlike slide's min/max pair. Z_MAX_MM is
// therefore the ONLY thing stopping a bad keyframe from driving the
// carriage off the top of the 200mm rods; there is no hardware backstop
// behind it. Set to the as-built usable travel.
extern float Z_MIN_MM;
extern float Z_MAX_MM;

// Programmed shots (see Shot.h): EaseType::LINEAR moves use this multiplier
// on the axis's normal configured acceleration, shrinking the ramp phase to
// near-negligible so the move reads as a sharp snap rather than a cinematic
// ease.
extern float SHOT_LINEAR_ACCEL_MULTIPLIER;

// EaseType::EASE_IN_OUT moves are driven as a jerk-limited S-curve: the
// move's quintic position profile is sampled into this many waypoints and
// replayed as a timed sequence of short trapezoidal moves (see Shot.cpp).
// More segments track the curve more faithfully at the cost of more
// mid-move stepper command churn; 16 is a reasonable cinematic default.
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

// ---------- OLED (fitted part — compile-time) ----------
namespace oled {
constexpr uint8_t WIDTH = 128;
constexpr uint8_t HEIGHT = 64;
constexpr uint8_t I2C_ADDR = 0x3C;  // common default; verify against the module
}  // namespace oled

// ---------- BLE HID ----------
// Read once at BleRecorder construction, so these are compile-time.
namespace ble {
constexpr const char *DEVICE_NAME = "Camera Slider Controller";
constexpr const char *MANUFACTURER = "DIY";
constexpr uint8_t BATTERY_LEVEL = 100;
}  // namespace ble

// ---------- Web config AP ----------
// The rig hosts its own access point rather than joining an existing
// network: it gets used on location where there may be no WiFi, and an AP
// is reachable identically every time. WPA2 requires >= 8 characters.
namespace webcfg {
constexpr const char *AP_SSID = "CameraSlider";
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
