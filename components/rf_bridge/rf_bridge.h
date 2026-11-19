#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "esphome/components/remote_base/dooya_protocol.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include "bounded_queue.h"
#include "frame_deduplicator.h"
#include "rf_frame.h"
#include "tx_scheduler.h"

namespace esphome::rf_bridge {

#ifdef USE_SELECT
class CaptureSlotSelect;
#endif

/// One radio: a remote_receiver + remote_transmitter pair (typically a CC1101 in
/// asynchronous serial mode) turned into a sniffer, a capture history and a serialised
/// transmitter.
///
///  RX: remote_receiver -> on_receive() -> decode (Dooya, RC-Switch, raw) -> de-duplicate
///      -> log + history + entities + on_frame / on_dooya automations
///  TX: actions / buttons / covers -> encode -> TxScheduler (FIFO, gap, duty cycle)
///      -> loop() -> remote_transmitter, one job at a time
///
/// Everything is logged locally (serial / web_server log); nothing depends on Home Assistant.
class RfBridge : public Component, public remote_base::RemoteReceiverListener {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  bool on_receive(remote_base::RemoteReceiveData data) override;

  // --- configuration (generated code) ----------------------------------------------------
  void set_transmitter(remote_base::RemoteTransmitterBase *transmitter) { this->transmitter_ = transmitter; }
  void set_band(const char *band) { this->band_ = band; }
  void set_queue_size(size_t size) { this->scheduler_.set_capacity(size); }
  void set_frame_gap(uint32_t gap_ms) { this->scheduler_.set_gap_ms(gap_ms); }
  void set_dedup_window(uint32_t window_ms) { this->dedup_.set_window_ms(window_ms); }
  void set_history_size(size_t size) { this->history_.set_capacity(size); }
  void set_duty_cycle(float ratio, uint32_t window_ms) { this->scheduler_.set_duty_cycle(ratio, window_ms); }
  void set_capture_raw(bool capture) { this->capture_raw_ = capture; }
  void set_raw_min_pulses(uint16_t pulses) { this->raw_min_pulses_ = pulses; }
  void set_raw_send_times(uint32_t times) { this->raw_send_times_ = times; }
  void set_raw_send_wait(uint32_t wait_us) { this->raw_send_wait_us_ = wait_us; }
  void set_dooya_send_times(uint32_t times) { this->dooya_send_times_ = times; }
  void set_dooya_send_wait(uint32_t wait_us) { this->dooya_send_wait_us_ = wait_us; }
  void set_rc_switch_send_times(uint32_t times) { this->rc_switch_send_times_ = times; }

#ifdef USE_TEXT_SENSOR
  void set_last_frame_text_sensor(text_sensor::TextSensor *sensor) { this->last_frame_text_ = sensor; }
  void set_selected_capture_text_sensor(text_sensor::TextSensor *sensor) { this->selected_capture_text_ = sensor; }
#endif
#ifdef USE_SENSOR
  void set_queue_depth_sensor(sensor::Sensor *sensor) { this->queue_depth_sensor_ = sensor; }
  void set_frames_received_sensor(sensor::Sensor *sensor) { this->frames_received_sensor_ = sensor; }
  void set_duty_cycle_sensor(sensor::Sensor *sensor) { this->duty_cycle_sensor_ = sensor; }
#endif
#ifdef USE_SELECT
  void set_capture_slot_select(CaptureSlotSelect *select) { this->capture_slot_select_ = select; }
#endif

  // --- automations ----------------------------------------------------------------------
  /// Every new (de-duplicated) frame.
  template<typename F>
  void add_on_frame_callback(F &&callback) {
    this->frame_callbacks_.add(std::forward<F>(callback));
  }
  /// Every new Dooya button press - the usual hook for keeping cover state in sync with
  /// physical remotes.
  template<typename F>
  void add_on_dooya_callback(F &&callback) {
    this->dooya_callbacks_.add(std::forward<F>(callback));
  }

  // --- transmit API (actions, buttons, lambdas) -----------------------------------------
  // All of these only enqueue; loop() puts the frames on air one at a time.
  // send_times/send_wait of 0 mean "use the protocol default from the YAML config".
  bool transmit_dooya(const remote_base::DooyaData &data, uint32_t send_times = 0, uint32_t send_wait_us = 0);
  bool transmit_rc_switch(uint8_t protocol, uint64_t code, uint8_t nbits, uint32_t send_times = 0);
  bool transmit_raw(const Timings &timings, uint32_t send_times = 0, uint32_t send_wait_us = 0);
  /// Re-transmit a captured frame; slot 0 is the newest capture.
  bool transmit_capture(size_t slot);
  bool transmit_selected() { return this->transmit_capture(this->selected_slot_); }

  void select_capture_slot(size_t slot);
  void clear_captures();

  const RfFrame *capture(size_t slot) const { return this->history_.newest(slot); }
  size_t history_size() const { return this->history_.capacity(); }
  size_t pending_transmissions() const { return this->scheduler_.pending(); }
  const char *band() const { return this->band_; }

 protected:
  bool decode_(remote_base::RemoteReceiveData &data, RfFrame &frame) const;
  bool encode_(const RfFrame &frame, uint32_t send_times, uint32_t send_wait_us, TxJob &job) const;
  bool enqueue_(TxJob job);
  void transmit_next_();
  void publish_selected_capture_();
  void publish_queue_state_();

  remote_base::RemoteTransmitterBase *transmitter_{nullptr};
  const char *band_{"RF"};

  FrameDeduplicator dedup_{1500};
  BoundedQueue<RfFrame> history_{8};
  TxScheduler scheduler_{};
  size_t selected_slot_{0};
  uint32_t frames_received_{0};
  bool was_throttled_{false};

  bool capture_raw_{true};
  uint16_t raw_min_pulses_{24};
  uint32_t raw_send_times_{5};
  uint32_t raw_send_wait_us_{10000};
  uint32_t dooya_send_times_{6};
  uint32_t dooya_send_wait_us_{8000};
  uint32_t rc_switch_send_times_{10};

  CallbackManager<void(const RfFrame &)> frame_callbacks_{};
  CallbackManager<void(remote_base::DooyaData)> dooya_callbacks_{};

#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *last_frame_text_{nullptr};
  text_sensor::TextSensor *selected_capture_text_{nullptr};
#endif
#ifdef USE_SENSOR
  sensor::Sensor *queue_depth_sensor_{nullptr};
  sensor::Sensor *frames_received_sensor_{nullptr};
  sensor::Sensor *duty_cycle_sensor_{nullptr};
#endif
#ifdef USE_SELECT
  CaptureSlotSelect *capture_slot_select_{nullptr};
#endif
};

}  // namespace esphome::rf_bridge
