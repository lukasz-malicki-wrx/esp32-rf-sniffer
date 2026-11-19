#pragma once

#include <cstdint>

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#include "rf_bridge.h"

namespace esphome::rf_bridge {

template<typename... Ts>
class TransmitDooyaAction : public Action<Ts...>, public Parented<RfBridge> {
 public:
  TEMPLATABLE_VALUE(uint32_t, remote_id)
  TEMPLATABLE_VALUE(uint8_t, channel)
  TEMPLATABLE_VALUE(uint8_t, button)
  TEMPLATABLE_VALUE(uint8_t, check)
  TEMPLATABLE_VALUE(uint32_t, send_times)
  TEMPLATABLE_VALUE(uint32_t, send_wait)

  void play(const Ts &...x) override {
    remote_base::DooyaData data{};
    data.id = this->remote_id_.value(x...);
    data.channel = this->channel_.value(x...);
    data.button = this->button_.value(x...);
    // A press frame repeats the button in the check field.
    data.check = this->check_.has_value() ? this->check_.value(x...) : data.button;
    this->parent_->transmit_dooya(data, this->send_times_.value_or(x..., 0), this->send_wait_.value_or(x..., 0));
  }
};

template<typename... Ts>
class TransmitRcSwitchAction : public Action<Ts...>, public Parented<RfBridge> {
 public:
  TEMPLATABLE_VALUE(uint8_t, protocol)
  TEMPLATABLE_VALUE(uint64_t, code)
  TEMPLATABLE_VALUE(uint8_t, nbits)
  TEMPLATABLE_VALUE(uint32_t, send_times)

  void play(const Ts &...x) override {
    this->parent_->transmit_rc_switch(this->protocol_.value(x...), this->code_.value(x...), this->nbits_.value(x...),
                                      this->send_times_.value_or(x..., 0));
  }
};

template<typename... Ts>
class TransmitCaptureAction : public Action<Ts...>, public Parented<RfBridge> {
 public:
  /// 1-based, as shown in the UI: 1 = newest capture.
  TEMPLATABLE_VALUE(uint8_t, slot)

  void play(const Ts &...x) override {
    const uint8_t slot = this->slot_.value(x...);
    this->parent_->transmit_capture(slot == 0 ? 0 : slot - 1);
  }
};

template<typename... Ts>
class ClearCapturesAction : public Action<Ts...>, public Parented<RfBridge> {
 public:
  void play(const Ts &...x) override { this->parent_->clear_captures(); }
};

}  // namespace esphome::rf_bridge
