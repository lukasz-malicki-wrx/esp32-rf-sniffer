#include "rf_bridge.h"

#include <cinttypes>
#include <iterator>

#include "esphome/components/remote_base/rc_switch_protocol.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include "rf_bridge_entities.h"

namespace esphome::rf_bridge {

static const char *const TAG = "rf_bridge";

namespace {

/// RC-Switch decoders happily "decode" a few bits out of noise; real remotes send 12+ bits.
constexpr uint8_t RC_SWITCH_MIN_BITS = 12;
/// How many raw timings to print per log line when a frame cannot be decoded.
constexpr size_t RAW_LOG_CHUNK = 16;
constexpr uint32_t DUTY_CYCLE_PUBLISH_INTERVAL_MS = 10000;
constexpr size_t LABEL_SIZE = 64;

}  // namespace

void RfBridge::setup() {
  this->publish_selected_capture_();
  this->publish_queue_state_();
#ifdef USE_SENSOR
  if (this->frames_received_sensor_ != nullptr)
    this->frames_received_sensor_->publish_state(0);
#endif
#ifdef USE_SELECT
  if (this->capture_slot_select_ != nullptr)
    this->capture_slot_select_->publish_state(size_t{0});
#endif
  if (this->scheduler_.duty_cycle_enabled()) {
    this->set_interval("duty_cycle", DUTY_CYCLE_PUBLISH_INTERVAL_MS, [this]() {
#ifdef USE_SENSOR
      if (this->duty_cycle_sensor_ != nullptr)
        this->duty_cycle_sensor_->publish_state(this->scheduler_.duty_cycle_used_percent(millis()));
#endif
    });
  }
}

void RfBridge::dump_config() {
  ESP_LOGCONFIG(TAG,
                "RF bridge [%s]:\n"
                "  TX queue: %u jobs\n"
                "  Capture history: %u frames\n"
                "  Dedup window: %" PRIu32
                " ms\n"
                "  Capture raw: %s (min %u pulses)\n"
                "  Duty-cycle limit: %s",
                this->band_, static_cast<unsigned>(this->scheduler_.capacity()),
                static_cast<unsigned>(this->history_.capacity()), this->dedup_.window_ms(), YESNO(this->capture_raw_),
                static_cast<unsigned>(this->raw_min_pulses_), YESNO(this->scheduler_.duty_cycle_enabled()));
  if (this->transmitter_ == nullptr)
    ESP_LOGCONFIG(TAG, "  Receive only (no transmitter)");
}

void RfBridge::loop() { this->transmit_next_(); }

// ---------------------------------------------------------------------------------------
// Receive
// ---------------------------------------------------------------------------------------

bool RfBridge::decode_(remote_base::RemoteReceiveData &data, RfFrame &frame) const {
  data.reset();
  if (auto dooya = remote_base::DooyaProtocol().decode(data)) {
    DooyaCode code;
    code.id = dooya->id;
    code.channel = dooya->channel;
    code.button = dooya->button;
    code.check = dooya->check;
    frame = RfFrame::from_dooya(code);
    return true;
  }

  for (uint8_t protocol = 1; protocol < std::size(remote_base::RC_SWITCH_PROTOCOLS); protocol++) {
    data.reset();
    uint64_t value = 0;
    uint8_t nbits = 0;
    if (remote_base::rc_switch_protocol(protocol).decode(data, &value, &nbits) && nbits >= RC_SWITCH_MIN_BITS) {
      RcSwitchCode code;
      code.code = value;
      code.protocol = protocol;
      code.nbits = nbits;
      frame = RfFrame::from_rc_switch(code);
      return true;
    }
  }

  if (this->capture_raw_ && data.size() >= static_cast<int32_t>(this->raw_min_pulses_)) {
    frame = RfFrame::from_raw(data.get_raw_data());
    return true;
  }
  return false;
}

bool RfBridge::on_receive(remote_base::RemoteReceiveData data) {
  RfFrame frame;
  if (!this->decode_(data, frame))
    return false;

  if (frame.protocol == Protocol::DOOYA && !frame.dooya.is_press()) {
    ESP_LOGV(TAG, "[%s] Dooya release frame ignored", this->band_);
    return true;
  }

  const uint32_t now = millis();
  if (this->dedup_.is_duplicate(frame.fingerprint(), now)) {
    ESP_LOGV(TAG, "[%s] Repeat ignored", this->band_);
    return true;
  }

  char label[LABEL_SIZE];
  frame.describe(label, sizeof(label));
  ESP_LOGI(TAG, "[%s] RX %s", this->band_, label);

  if (frame.protocol == Protocol::RAW) {
    // Unknown protocol: dump the pulse train so it can be analysed or added as a decoder.
    for (size_t i = 0; i < frame.raw.size(); i += RAW_LOG_CHUNK) {
      char line[RAW_LOG_CHUNK * 8 + 1];
      size_t pos = 0;
      for (size_t j = i; j < frame.raw.size() && j < i + RAW_LOG_CHUNK; j++)
        pos += static_cast<size_t>(snprintf(line + pos, sizeof(line) - pos, "%" PRId32 " ", frame.raw[j]));
      ESP_LOGD(TAG, "[%s]   raw[%u]: %s", this->band_, static_cast<unsigned>(i), line);
    }
  }

  this->frames_received_++;
  this->history_.push_overwrite(frame);
  // The newest capture is what you usually want to replay - jump back to it.
  this->selected_slot_ = 0;

#ifdef USE_TEXT_SENSOR
  if (this->last_frame_text_ != nullptr)
    this->last_frame_text_->publish_state(label);
#endif
#ifdef USE_SENSOR
  if (this->frames_received_sensor_ != nullptr)
    this->frames_received_sensor_->publish_state(static_cast<float>(this->frames_received_));
#endif
#ifdef USE_SELECT
  if (this->capture_slot_select_ != nullptr)
    this->capture_slot_select_->publish_state(size_t{0});
#endif
  this->publish_selected_capture_();

  this->frame_callbacks_.call(frame);
  if (frame.protocol == Protocol::DOOYA) {
    remote_base::DooyaData dooya{};
    dooya.id = frame.dooya.id;
    dooya.channel = frame.dooya.channel;
    dooya.button = frame.dooya.button;
    dooya.check = frame.dooya.check;
    this->dooya_callbacks_.call(dooya);
  }
  return true;
}

// ---------------------------------------------------------------------------------------
// Transmit
// ---------------------------------------------------------------------------------------

bool RfBridge::encode_(const RfFrame &frame, uint32_t send_times, uint32_t send_wait_us, TxJob &job) const {
  remote_base::RemoteTransmitData data;
  switch (frame.protocol) {
    case Protocol::DOOYA: {
      remote_base::DooyaData dooya{};
      dooya.id = frame.dooya.id;
      dooya.channel = frame.dooya.channel;
      dooya.button = frame.dooya.button;
      dooya.check = frame.dooya.check;
      remote_base::DooyaProtocol().encode(&data, dooya);
      job.send_times = send_times != 0 ? send_times : this->dooya_send_times_;
      job.send_wait_us = send_wait_us != 0 ? send_wait_us : this->dooya_send_wait_us_;
      break;
    }
    case Protocol::RC_SWITCH: {
      if (frame.rc_switch.protocol == 0 || frame.rc_switch.protocol >= std::size(remote_base::RC_SWITCH_PROTOCOLS)) {
        ESP_LOGW(TAG, "[%s] Unknown RC-Switch protocol %u", this->band_,
                 static_cast<unsigned>(frame.rc_switch.protocol));
        return false;
      }
      remote_base::rc_switch_protocol(frame.rc_switch.protocol)
          .transmit(&data, frame.rc_switch.code, frame.rc_switch.nbits);
      // RC-Switch frames carry their own sync gap, so repeats go back-to-back.
      job.send_times = send_times != 0 ? send_times : this->rc_switch_send_times_;
      job.send_wait_us = send_wait_us;
      break;
    }
    case Protocol::RAW:
      data.set_data(frame.raw);
      job.send_times = send_times != 0 ? send_times : this->raw_send_times_;
      job.send_wait_us = send_wait_us != 0 ? send_wait_us : this->raw_send_wait_us_;
      break;
  }
  job.timings = data.get_data();

  char label[LABEL_SIZE];
  frame.describe(label, sizeof(label));
  job.label = label;
  return true;
}

bool RfBridge::enqueue_(TxJob job) {
  if (this->transmitter_ == nullptr) {
    ESP_LOGW(TAG, "[%s] Cannot send '%s': no transmitter configured", this->band_, job.label.c_str());
    return false;
  }
  const std::string label = job.label;
  const EnqueueResult result = this->scheduler_.enqueue(std::move(job));
  if (result != EnqueueResult::QUEUED) {
    ESP_LOGW(TAG, "[%s] TX '%s' rejected: %s", this->band_, label.c_str(), enqueue_result_name(result));
    return false;
  }
  ESP_LOGD(TAG, "[%s] TX '%s' queued (%u pending)", this->band_, label.c_str(),
           static_cast<unsigned>(this->scheduler_.pending()));
  this->publish_queue_state_();
  return true;
}

bool RfBridge::transmit_dooya(const remote_base::DooyaData &data, uint32_t send_times, uint32_t send_wait_us) {
  DooyaCode code;
  code.id = data.id;
  code.channel = data.channel;
  code.button = data.button;
  code.check = data.check;
  TxJob job;
  return this->encode_(RfFrame::from_dooya(code), send_times, send_wait_us, job) && this->enqueue_(std::move(job));
}

bool RfBridge::transmit_rc_switch(uint8_t protocol, uint64_t code, uint8_t nbits, uint32_t send_times) {
  RcSwitchCode rc;
  rc.code = code;
  rc.protocol = protocol;
  rc.nbits = nbits;
  TxJob job;
  return this->encode_(RfFrame::from_rc_switch(rc), send_times, 0, job) && this->enqueue_(std::move(job));
}

bool RfBridge::transmit_raw(const Timings &timings, uint32_t send_times, uint32_t send_wait_us) {
  TxJob job;
  return this->encode_(RfFrame::from_raw(timings), send_times, send_wait_us, job) && this->enqueue_(std::move(job));
}

bool RfBridge::transmit_capture(size_t slot) {
  const RfFrame *frame = this->history_.newest(slot);
  if (frame == nullptr) {
    ESP_LOGW(TAG, "[%s] Capture slot %u is empty", this->band_, static_cast<unsigned>(slot + 1));
    return false;
  }
  TxJob job;
  return this->encode_(*frame, 0, 0, job) && this->enqueue_(std::move(job));
}

void RfBridge::transmit_next_() {
  if (this->transmitter_ == nullptr)
    return;
  const uint32_t now = millis();
  auto job = this->scheduler_.next(now);

  if (this->scheduler_.throttled() != this->was_throttled_) {
    this->was_throttled_ = this->scheduler_.throttled();
    if (this->was_throttled_) {
      ESP_LOGW(TAG, "[%s] Duty-cycle budget used up, holding %u queued frame(s)", this->band_,
               static_cast<unsigned>(this->scheduler_.pending()));
    } else {
      ESP_LOGI(TAG, "[%s] Duty-cycle budget available again", this->band_);
    }
  }
  if (!job)
    return;

  // remote_transmitter must run with `non_blocking: false`: perform() returns once the last
  // repeat is on air, so the inter-frame gap is measured from the real end of transmission.
  auto call = this->transmitter_->transmit();
  call.get_data()->set_data(job->timings);
  call.set_send_times(job->send_times);
  call.set_send_wait(job->send_wait_us);
  call.perform();

  const uint32_t done = millis();
  this->scheduler_.mark_sent(done, job->airtime_us());
  ESP_LOGI(TAG, "[%s] TX %s (x%" PRIu32 ", %" PRIu32 " ms, %u pending)", this->band_, job->label.c_str(),
           job->send_times, done - now, static_cast<unsigned>(this->scheduler_.pending()));
  this->publish_queue_state_();
}

// ---------------------------------------------------------------------------------------
// Capture history / entities
// ---------------------------------------------------------------------------------------

void RfBridge::select_capture_slot(size_t slot) {
  if (slot >= this->history_.capacity())
    slot = this->history_.capacity() - 1;
  this->selected_slot_ = slot;
  this->publish_selected_capture_();
}

void RfBridge::clear_captures() {
  this->history_.clear();
  this->selected_slot_ = 0;
  ESP_LOGI(TAG, "[%s] Capture history cleared", this->band_);
#ifdef USE_SELECT
  if (this->capture_slot_select_ != nullptr)
    this->capture_slot_select_->publish_state(size_t{0});
#endif
  this->publish_selected_capture_();
}

void RfBridge::publish_selected_capture_() {
#ifdef USE_TEXT_SENSOR
  if (this->selected_capture_text_ == nullptr)
    return;
  const RfFrame *frame = this->history_.newest(this->selected_slot_);
  if (frame == nullptr) {
    this->selected_capture_text_->publish_state("(empty)");
    return;
  }
  char label[LABEL_SIZE];
  frame->describe(label, sizeof(label));
  this->selected_capture_text_->publish_state(label);
#endif
}

void RfBridge::publish_queue_state_() {
#ifdef USE_SENSOR
  if (this->queue_depth_sensor_ == nullptr)
    return;
  const auto pending = static_cast<float>(this->scheduler_.pending());
  if (!this->queue_depth_sensor_->has_state() || this->queue_depth_sensor_->get_raw_state() != pending)
    this->queue_depth_sensor_->publish_state(pending);
#endif
}

}  // namespace esphome::rf_bridge
