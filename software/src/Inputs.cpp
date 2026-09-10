#include "Inputs.h"

#include "BleRecorder.h"
#include "Motion.h"
#include "Sequencer.h"

Inputs inputs;

void Inputs::begin() {
  for (uint8_t i = 0; i < fw::ENCODER_COUNT; ++i) {
    const EncoderConfig &e = settings.encoders[i];
    if (!e.enabled || e.mode == EncoderMode::OFF) continue;
    encoders_[i].begin(e.pinA, e.pinB);
    lastCount_[i] = 0;
    Serial.printf("[inputs] %s on GPIO%u/%u -> %s (%s)\n", e.name, e.pinA, e.pinB,
                  settings.axes[e.axis].name, encoderModeName(e.mode));
  }

  for (uint8_t i = 0; i < fw::BUTTON_COUNT; ++i) {
    const ButtonConfig &b = settings.buttons[i];
    if (!b.enabled) continue;
    buttons_[i].begin(b.pin, b.activeLow);
    Serial.printf("[inputs] %s on GPIO%u -> %s / %s\n", b.name, b.pin,
                  buttonActionName(b.shortPress), buttonActionName(b.longPress));
  }
}

// Touching a control during playback takes manual control rather than
// fighting the sequencer for the same axis.
void Inputs::interruptPlayback() {
  if (sequencer.state() == SeqState::MOVING ||
      sequencer.state() == SeqState::HOLDING) {
    sequencer.pause();
  }
}

void Inputs::updateEncoder(uint8_t i) {
  const EncoderConfig &e = settings.encoders[i];
  if (!encoders_[i].attached()) return;
  Axis &ax = motion_ctl.axis(e.axis);
  if (!ax.available()) return;

  const int32_t count = encoders_[i].count();
  const int32_t sign = e.invert ? -1 : 1;

  if (e.mode == EncoderMode::VELOCITY) {
    // Absolute knob position is the speed command: back to the detent it
    // started on means stop. Nothing is consumed, so the knob and the axis
    // never disagree about how fast it should be going.
    const int32_t maxCounts = e.detentsForMaxSpeed * e.countsPerDetent;
    const int32_t clamped = constrain(count * sign, -maxCounts, maxCounts);
    if (clamped != 0) interruptPlayback();
    const float fraction = static_cast<float>(clamped) / maxCounts;
    const int8_t dir = (clamped > 0) - (clamped < 0);
    if (dir == 0) {
      if (lastCount_[i] != count) ax.stop();
    } else if (!motion_ctl.estopped()) {
      ax.jog(dir, fabsf(fraction) * ax.cfg().maxSpeed);
    }
    lastCount_[i] = count;
    return;
  }

  // POSITION mode: consume whole detents, keeping the leftover counts so a
  // slow turn accumulates instead of being rounded away.
  const int32_t delta = count - lastCount_[i];
  if (delta == 0) return;
  lastCount_[i] = count;
  detentRemainder_[i] += delta * sign;
  const int32_t detents = detentRemainder_[i] / e.countsPerDetent;
  if (detents == 0) return;
  detentRemainder_[i] -= detents * e.countsPerDetent;

  if (motion_ctl.estopped()) return;
  interruptPlayback();
  String error;
  if (!ax.nudge(detents * e.unitsPerDetent, error)) {
    Serial.printf("[inputs] %s\n", error.c_str());
  }
}

void Inputs::runAction(ButtonAction action, uint8_t axisArg) {
  if (axisArg >= fw::AXIS_COUNT) axisArg = 0;
  String error;
  switch (action) {
    case ButtonAction::NONE:
      break;
    case ButtonAction::PLAY_PAUSE:
      sequencer.togglePlayPause();
      break;
    case ButtonAction::SEQ_STOP:
      sequencer.stop();
      break;
    case ButtonAction::SEQ_RESTART:
      sequencer.restart();
      break;
    case ButtonAction::KEYFRAME_ADD:
      if (sequencer.addFromCurrent(error)) {
        sequencer.save();
      } else {
        Serial.printf("[action] %s\n", error.c_str());
      }
      break;
    case ButtonAction::KEYFRAME_DELETE_LAST:
      sequencer.removeLast();
      sequencer.save();
      break;
    case ButtonAction::KEYFRAME_CLEAR:
      sequencer.clear();
      sequencer.save();
      break;
    case ButtonAction::GOTO_START:
      if (!sequencer.gotoKeyframe(0, error)) Serial.printf("[action] %s\n", error.c_str());
      break;
    case ButtonAction::HOME_ALL:
      sequencer.stop();
      motion_ctl.homeAll();
      break;
    case ButtonAction::HOME_AXIS:
      sequencer.stop();
      motion_ctl.axis(axisArg).startHoming();
      break;
    case ButtonAction::ESTOP:
      // Latching, so the same button clears it once the operator has dealt
      // with whatever caused the stop.
      if (motion_ctl.estopped()) {
        motion_ctl.clearEstop();
      } else {
        sequencer.stop();
        motion_ctl.estop();
      }
      break;
    case ButtonAction::ENABLE_TOGGLE:
      motion_ctl.setDriversEnabled(!motion_ctl.driversEnabled());
      break;
    case ButtonAction::REC_TOGGLE:
      bleRecorder.toggleRecording();
      break;
    case ButtonAction::REC_RESYNC:
      bleRecorder.resync();
      break;
    case ButtonAction::SELECT_NEXT_AXIS:
      selectedAxis_ = (selectedAxis_ + 1) % fw::AXIS_COUNT;
      break;
    case ButtonAction::ZERO_AXIS:
      motion_ctl.axis(axisArg).setPositionUnits(0.0f);
      break;
    default:
      break;
  }
}

void Inputs::rezeroVelocityEncoders() {
  for (uint8_t i = 0; i < fw::ENCODER_COUNT; ++i) {
    if (settings.encoders[i].mode != EncoderMode::VELOCITY) continue;
    encoders_[i].setCount(0);
    lastCount_[i] = 0;
  }
}

void Inputs::update(uint32_t nowMs) {
  const bool estop = motion_ctl.estopped();
  if (lastEstop_ && !estop) rezeroVelocityEncoders();
  lastEstop_ = estop;

  for (uint8_t i = 0; i < fw::ENCODER_COUNT; ++i) updateEncoder(i);

  for (uint8_t i = 0; i < fw::BUTTON_COUNT; ++i) {
    const ButtonConfig &b = settings.buttons[i];
    if (!b.enabled) continue;
    const DebouncedButton::Event event = buttons_[i].update(nowMs);
    if (event == DebouncedButton::Event::SHORT_PRESS) {
      runAction(b.shortPress, b.axisArg);
    } else if (event == DebouncedButton::Event::LONG_PRESS) {
      runAction(b.longPress, b.axisArg);
    }
  }
}

void Inputs::telemetryJson(JsonObject out) const {
  out["selected_axis"] = selectedAxis_;
  JsonArray enc = out["encoders"].to<JsonArray>();
  for (uint8_t i = 0; i < fw::ENCODER_COUNT; ++i) {
    JsonObject o = enc.add<JsonObject>();
    o["name"] = settings.encoders[i].name;
    o["attached"] = encoders_[i].attached();
    o["count"] = lastCount_[i];
  }
  JsonArray btn = out["buttons"].to<JsonArray>();
  for (uint8_t i = 0; i < fw::BUTTON_COUNT; ++i) {
    JsonObject o = btn.add<JsonObject>();
    o["name"] = settings.buttons[i].name;
    o["pressed"] = buttons_[i].isPressed();
  }
}
