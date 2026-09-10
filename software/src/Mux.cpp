#include "Mux.h"

bool selectMuxChannel(TwoWire &bus, uint8_t muxAddress, uint8_t channel) {
  if (channel > 7) return false;
  bus.beginTransmission(muxAddress);
  bus.write(static_cast<uint8_t>(1 << channel));
  return bus.endTransmission() == 0;
}

namespace {
// AS5600 RAW ANGLE register (0x0C/0x0D): unprocessed 12-bit shaft angle.
bool readRegister16(TwoWire &bus, uint8_t reg, uint16_t &out) {
  bus.beginTransmission(0x36);
  bus.write(reg);
  if (bus.endTransmission(false) != 0) return false;
  if (bus.requestFrom(static_cast<uint8_t>(0x36), static_cast<uint8_t>(2)) != 2) {
    return false;
  }
  const uint16_t hi = bus.read();
  const uint16_t lo = bus.read();
  out = static_cast<uint16_t>((hi << 8) | lo) & 0x0FFF;
  return true;
}

bool readRegister8(TwoWire &bus, uint8_t reg, uint8_t &out) {
  bus.beginTransmission(0x36);
  bus.write(reg);
  if (bus.endTransmission(false) != 0) return false;
  if (bus.requestFrom(static_cast<uint8_t>(0x36), static_cast<uint8_t>(1)) != 1) {
    return false;
  }
  out = bus.read();
  return true;
}
}  // namespace

bool readAS5600Degrees(TwoWire &bus, uint8_t muxAddress, uint8_t channel,
                       float &outDeg) {
  if (!selectMuxChannel(bus, muxAddress, channel)) return false;
  uint16_t raw = 0;
  if (!readRegister16(bus, 0x0C, raw)) return false;
  outDeg = raw * (360.0f / 4096.0f);
  return true;
}

AS5600Status readAS5600Status(TwoWire &bus, uint8_t muxAddress, uint8_t channel) {
  AS5600Status s;
  if (!selectMuxChannel(bus, muxAddress, channel)) return s;
  uint8_t status = 0;
  if (!readRegister8(bus, 0x0B, status)) return s;
  s.responded = true;
  s.magnetDetected = (status & 0x20) != 0;  // MD
  s.magnetTooWeak = (status & 0x10) != 0;   // ML
  s.magnetTooStrong = (status & 0x08) != 0; // MH
  readRegister16(bus, 0x0C, s.rawAngle);
  return s;
}
