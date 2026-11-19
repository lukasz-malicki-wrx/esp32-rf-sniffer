#pragma once

#include <cstdint>

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif

#include "rf_bridge.h"

namespace esphome::rf_bridge {

#ifdef USE_BUTTON
class RfBridgeButton : public button::Button, public Parented<RfBridge> {
 public:
  enum class Kind : uint8_t { TRANSMIT_SELECTED, TRANSMIT_LAST, CLEAR_CAPTURES };

  explicit RfBridgeButton(Kind kind) : kind_(kind) {}

 protected:
  void press_action() override {
    switch (this->kind_) {
      case Kind::TRANSMIT_SELECTED:
        this->parent_->transmit_selected();
        break;
      case Kind::TRANSMIT_LAST:
        this->parent_->transmit_capture(0);
        break;
      case Kind::CLEAR_CAPTURES:
        this->parent_->clear_captures();
        break;
    }
  }

  Kind kind_;
};
#endif

#ifdef USE_SELECT
/// Picks a slot of the capture history: option N is the N-th newest frame.
/// The options are fixed slot numbers on purpose - ESPHome front-ends read select options
/// once, so labels that change with every capture would go stale. The content of the
/// chosen slot is published by the "selected capture" text sensor instead.
class CaptureSlotSelect : public select::Select, public Parented<RfBridge> {
 protected:
  void control(size_t index) override {
    this->parent_->select_capture_slot(index);
    this->publish_state(index);
  }
};
#endif

}  // namespace esphome::rf_bridge
