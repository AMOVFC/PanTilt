#include "Mux.h"

#include "config.h"

void selectMuxChannel(TwoWire &bus, uint8_t channel) {
  bus.beginTransmission(i2c_addr::TCA9548A);
  bus.write(static_cast<uint8_t>(1 << channel));
  bus.endTransmission();
}

namespace {
// AS5600 RAW ANGLE register (0x0C/0x0D): unprocessed 12-bit shaft angle.
uint16_t readAS5600Raw(TwoWire &bus) {
  bus.beginTransmission(i2c_addr::AS5600);
  bus.write(static_cast<uint8_t>(0x0C));
  if (bus.endTransmission(false) != 0) return 0;

  bus.requestFrom(i2c_addr::AS5600, static_cast<uint8_t>(2));
  if (bus.available() < 2) return 0;
  const uint16_t hi = bus.read();
  const uint16_t lo = bus.read();
  return ((hi << 8) | lo) & 0x0FFF;
}
}  // namespace

float readAS5600DegreesOnChannel(TwoWire &bus, uint8_t channel) {
  selectMuxChannel(bus, channel);
  const uint16_t raw = readAS5600Raw(bus);
  return raw * (360.0f / 4096.0f);
}
