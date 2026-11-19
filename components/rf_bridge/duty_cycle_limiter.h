#pragma once

// Pure C++17, no ESPHome dependencies: compiled on the host by the unit tests.

#include <array>
#include <cstdint>

namespace esphome::rf_bridge {

/// Sliding-window airtime budget, e.g. ETSI EN 300 220 limits most of the 868 MHz SRD band
/// to 1 % duty cycle (36 s of transmission per hour).
///
/// The window is split into BUCKETS equal slots; airtime is charged to the slot of the
/// transmission and slots older than the window are forgotten. This bounds memory to a few
/// bytes while erring on the safe side by at most one slot (window / BUCKETS).
///
/// A ratio of 0 disables the limiter.
class DutyCycleLimiter {
 public:
  static constexpr uint32_t BUCKETS = 60;

  DutyCycleLimiter() = default;
  DutyCycleLimiter(float ratio, uint32_t window_ms) { configure(ratio, window_ms); }

  void configure(float ratio, uint32_t window_ms) {
    const float clamped = ratio < 0.0f ? 0.0f : (ratio > 1.0f ? 1.0f : ratio);
    // Integer parts-per-million: 0.01f is 0.0099999998 in binary, which would shave a
    // microsecond off a budget that regulations define exactly.
    ratio_ppm_ = static_cast<uint32_t>(clamped * 1'000'000.0f + 0.5f);
    window_ms_ = window_ms < BUCKETS ? BUCKETS : window_ms;
    bucket_ms_ = window_ms_ / BUCKETS;
    budget_us_ = static_cast<uint64_t>(window_ms_) * ratio_ppm_ / 1000u;
    reset();
  }

  bool enabled() const { return ratio_ppm_ > 0; }
  uint64_t budget_us() const { return budget_us_; }

  /// @return true if a transmission of `airtime_us` fits into the remaining budget now.
  bool can_transmit(uint32_t now_ms, uint32_t airtime_us) {
    if (!enabled())
      return true;
    expire_(now_ms);
    return used_us_ + airtime_us <= budget_us_;
  }

  /// A single transmission longer than the whole budget can never be sent.
  bool fits_at_all(uint32_t airtime_us) const { return !enabled() || airtime_us <= budget_us_; }

  void record(uint32_t now_ms, uint32_t airtime_us) {
    if (!enabled())
      return;
    expire_(now_ms);
    buckets_[current_bucket_] += airtime_us;
    used_us_ += airtime_us;
  }

  uint64_t used_us(uint32_t now_ms) {
    expire_(now_ms);
    return used_us_;
  }

  /// Share of the budget in use, 0..100.
  float used_percent(uint32_t now_ms) {
    if (!enabled() || budget_us_ == 0)
      return 0.0f;
    return 100.0f * static_cast<float>(used_us(now_ms)) / static_cast<float>(budget_us_);
  }

  void reset() {
    buckets_.fill(0);
    used_us_ = 0;
    current_bucket_ = 0;
    started_ = false;
  }

 private:
  void expire_(uint32_t now_ms) {
    if (!started_) {
      started_ = true;
      bucket_start_ms_ = now_ms;
      return;
    }
    uint32_t elapsed = now_ms - bucket_start_ms_;
    if (elapsed < bucket_ms_)
      return;
    uint32_t steps = elapsed / bucket_ms_;
    if (steps > BUCKETS)
      steps = BUCKETS;
    for (uint32_t i = 0; i < steps; i++) {
      current_bucket_ = (current_bucket_ + 1) % BUCKETS;
      used_us_ -= buckets_[current_bucket_];
      buckets_[current_bucket_] = 0;
    }
    bucket_start_ms_ += (elapsed / bucket_ms_) * bucket_ms_;
  }

  uint32_t ratio_ppm_{0};
  uint32_t window_ms_{3600000};
  uint32_t bucket_ms_{3600000 / BUCKETS};
  uint64_t budget_us_{0};

  std::array<uint64_t, BUCKETS> buckets_{};
  uint64_t used_us_{0};
  uint32_t current_bucket_{0};
  uint32_t bucket_start_ms_{0};
  bool started_{false};
};

}  // namespace esphome::rf_bridge
