#pragma once

// Pure C++17, no ESPHome dependencies: compiled on the host by the unit tests.

#include <cstdint>

namespace esphome::rf_bridge {

/// Collapses the repeats of a single button press into one event.
///
/// A remote sends the same frame several times per press, and the receiver reports each
/// burst separately. A frame is a duplicate when it matches the previous fingerprint and
/// arrives within `window_ms` of the *last* sighting - the window slides while the button
/// is held, so a long press is still one event.
///
/// Timestamps are millis()-style uint32_t; unsigned subtraction keeps it correct across
/// the 49.7-day wrap-around.
class FrameDeduplicator {
 public:
  explicit FrameDeduplicator(uint32_t window_ms = 1500) : window_ms_(window_ms) {}

  void set_window_ms(uint32_t window_ms) { window_ms_ = window_ms; }
  uint32_t window_ms() const { return window_ms_; }

  /// @return true if the frame is a repeat and should be ignored.
  bool is_duplicate(uint64_t fingerprint, uint32_t now_ms) {
    const bool duplicate = has_last_ && fingerprint == last_fingerprint_ && (now_ms - last_seen_ms_) < window_ms_;
    has_last_ = true;
    last_fingerprint_ = fingerprint;
    last_seen_ms_ = now_ms;
    return duplicate;
  }

  void reset() { has_last_ = false; }

 private:
  uint32_t window_ms_;
  bool has_last_{false};
  uint64_t last_fingerprint_{0};
  uint32_t last_seen_ms_{0};
};

}  // namespace esphome::rf_bridge
