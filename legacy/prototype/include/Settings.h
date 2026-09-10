#pragma once
// Runtime-settable configuration, persisted to NVS and editable over the web
// UI (WebConfig.*). The values themselves are declared in config.h under
// their original namespaces, so call sites read them exactly as they did
// when they were compile-time constants.
//
// Everything is driven off one descriptor table: JSON serialization, form
// rendering, range validation, and NVS persistence all walk kSettings rather
// than hand-listing fields, so adding a setting means adding one row.
//
// Unchanged from the final rig's Settings.h — this mechanism doesn't know
// or care how many axes exist; only the row list in Settings.cpp differs.
//
// NOT exposed here, deliberately:
//   - Pin assignments. Editing these in software cannot rewire the board;
//     it can only make firmware disagree with the physical wiring, and a
//     wrong entry drives STEP pulses onto whatever else is on that pin. They
//     are physical facts, so they stay compile-time.
//   - I2C addresses, OLED dimensions, TMC sense resistor and address straps.
//     Same reasoning: fixed by the parts fitted and how they're soldered.

#include <cstdint>

namespace settings {

enum class Type : uint8_t { F32, U32, I32, U16, U8, BOOL };

struct Desc {
  const char *key;    // NVS key AND JSON field name — max 15 chars (NVS limit)
  const char *group;  // UI section heading
  const char *label;  // human-readable name in the UI
  const char *unit;   // shown after the input; "" if unitless
  Type type;
  void *ptr;          // points at the extern in config.h
  float min;          // inclusive validation bounds, applied on every write
  float max;
  bool needsReboot;   // value is only read during init, so a live edit won't apply
};

extern const Desc kSettings[];
extern const uint16_t kSettingsCount;

// Loads every setting from NVS, falling back to the compiled-in default for
// any key not yet stored, then recomputes derived values. Call first in
// setup(), before anything reads config — in particular before the TMC
// drivers are configured and before any axis begin().
void begin();

// Persists every setting to NVS. Called by the web UI after a successful
// write; values are already live in RAM by then.
void saveAll();

// Clears the NVS store, so the next boot comes up on compiled-in defaults.
// Values currently in RAM are left alone — the rig keeps running on what it
// has until it reboots.
void resetToDefaults();

// Recomputes mech::*_STEPS_PER_* and the *_MICROSTEPPING mirrors from their
// inputs. Called automatically by begin() and after any web UI write.
void recomputeDerived();

// Finds a descriptor by key, or nullptr.
const Desc *find(const char *key);

// Writes one setting from a numeric value, clamped to the descriptor's
// range. Returns false if the key is unknown or the value is not finite.
bool setValue(const Desc &desc, float value);

// Reads one setting as a float, for JSON output.
float getValue(const Desc &desc);

}  // namespace settings
