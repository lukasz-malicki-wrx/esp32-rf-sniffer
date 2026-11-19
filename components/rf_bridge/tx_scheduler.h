#pragma once

// Pure C++17, no ESPHome dependencies: compiled on the host by the unit tests.

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "bounded_queue.h"
#include "duty_cycle_limiter.h"
#include "rf_frame.h"

namespace esphome::rf_bridge {

/// A fully encoded transmission: what goes on air, how often, and a label for the logs.
struct TxJob {
  Timings timings;
  uint32_t send_times{1};
  uint32_t send_wait_us{0};
  std::string label;

  uint32_t airtime_us() const { return rf_bridge::airtime_us(timings, send_times, send_wait_us); }
};

enum class EnqueueResult : uint8_t {
  QUEUED,
  QUEUE_FULL,      ///< rejected - the queue already holds `capacity` jobs
  EXCEEDS_BUDGET,  ///< rejected - longer than the whole duty-cycle budget, would block forever
  EMPTY,           ///< rejected - nothing to send
};

const char *enqueue_result_name(EnqueueResult result);

/// Serialises transmissions for one radio.
///
/// An RF transmitter can put exactly one signal on air at a time, and receivers (motors,
/// sockets) need a quiet gap to tell two commands apart. Every producer - buttons, covers,
/// automations, replays - therefore only enqueues; the component's loop() asks `next()`
/// for the single job that may be sent now and reports back with `mark_sent()`.
///
/// Ordering is strict FIFO: a job held back by the duty-cycle budget also holds back the
/// jobs behind it, so commands to the same device are never reordered.
class TxScheduler {
 public:
  TxScheduler() = default;

  void set_capacity(size_t capacity) { queue_.set_capacity(capacity); }
  void set_gap_ms(uint32_t gap_ms) { gap_ms_ = gap_ms; }
  void set_duty_cycle(float ratio, uint32_t window_ms) { limiter_.configure(ratio, window_ms); }

  EnqueueResult enqueue(TxJob job) {
    if (job.timings.empty() || job.send_times == 0)
      return EnqueueResult::EMPTY;
    if (!limiter_.fits_at_all(job.airtime_us()))
      return EnqueueResult::EXCEEDS_BUDGET;
    return queue_.try_push(std::move(job)) ? EnqueueResult::QUEUED : EnqueueResult::QUEUE_FULL;
  }

  /// @return the job to transmit now (removed from the queue), or nothing if the queue is
  ///         empty, the inter-frame gap has not elapsed yet, or the duty-cycle budget is spent.
  std::optional<TxJob> next(uint32_t now_ms) {
    const TxJob *head = queue_.front();
    if (head == nullptr)
      return std::nullopt;
    if (has_sent_ && (now_ms - last_end_ms_) < gap_ms_)
      return std::nullopt;
    if (!limiter_.can_transmit(now_ms, head->airtime_us())) {
      throttled_ = true;
      return std::nullopt;
    }
    throttled_ = false;
    return queue_.pop();
  }

  /// Call when the transmission returned by next() has finished.
  void mark_sent(uint32_t now_ms, uint32_t airtime_us) {
    has_sent_ = true;
    last_end_ms_ = now_ms;
    limiter_.record(now_ms, airtime_us);
  }

  void clear() { queue_.clear(); }
  size_t pending() const { return queue_.size(); }
  size_t capacity() const { return queue_.capacity(); }
  /// True while the head of the queue is waiting for duty-cycle budget.
  bool throttled() const { return throttled_; }
  float duty_cycle_used_percent(uint32_t now_ms) { return limiter_.used_percent(now_ms); }
  bool duty_cycle_enabled() const { return limiter_.enabled(); }

 private:
  BoundedQueue<TxJob> queue_{16};
  DutyCycleLimiter limiter_{};
  uint32_t gap_ms_{100};
  uint32_t last_end_ms_{0};
  bool has_sent_{false};
  bool throttled_{false};
};

inline const char *enqueue_result_name(EnqueueResult result) {
  switch (result) {
    case EnqueueResult::QUEUED:
      return "queued";
    case EnqueueResult::QUEUE_FULL:
      return "queue full";
    case EnqueueResult::EXCEEDS_BUDGET:
      return "longer than the duty-cycle budget";
    case EnqueueResult::EMPTY:
      return "empty frame";
  }
  return "?";
}

}  // namespace esphome::rf_bridge
