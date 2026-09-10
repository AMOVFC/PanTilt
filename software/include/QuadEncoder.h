#pragma once
// Interrupt-driven quadrature decoder.
//
// This deliberately does NOT use the PCNT peripheral, and that is the whole
// point of the file. On an ESP32-S3 FastAccelStepper's first four steppers
// use the MCPWM+PCNT backend and consume PCNT units 0-3 -- every unit the
// part has -- while ESP32Encoder allocates units from 0 upwards with no
// knowledge of that. On a four-axis machine the two libraries silently
// reconfigure each other's counters: the encoders read garbage and, far
// worse, the steppers lose step counting. Decoding in a GPIO ISR sidesteps
// the peripheral entirely.
//
// A hand-turned EC11 produces a few hundred edges per second at most, so the
// ISR cost is irrelevant, and there is no longer any limit on how many
// encoders can be attached.

#include <Arduino.h>

class QuadEncoder {
 public:
  bool begin(uint8_t pinA, uint8_t pinB);
  void end();

  // 4 counts per full quadrature cycle, which is one detent on a typical
  // EC11. Reads are atomic (aligned 32-bit).
  int32_t count() const { return count_; }
  void setCount(int32_t value) { count_ = value; }
  bool attached() const { return attached_; }

 private:
  static void IRAM_ATTR isrTrampoline(void *arg);
  void IRAM_ATTR handleEdge();

  volatile int32_t count_ = 0;
  volatile uint8_t state_ = 0;
  uint8_t pinA_ = 0;
  uint8_t pinB_ = 0;
  bool attached_ = false;
};
