#pragma once

// Pure C++17, no ESPHome dependencies: compiled on the host by the unit tests.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace esphome::rf_bridge {

/// Pulse train as produced by remote_receiver: positive = mark (carrier on), negative = space,
/// both in microseconds.
using Timings = std::vector<int32_t>;

enum class Protocol : uint8_t { DOOYA, RC_SWITCH, RAW };

const char *protocol_name(Protocol protocol);

struct DooyaCode {
  uint32_t id{0};  ///< 24-bit remote id
  uint8_t channel{0};
  uint8_t button{0};
  uint8_t check{0};  ///< equals `button` while held; 0xE on the "released" frame

  static constexpr uint8_t BUTTON_UP = 1;
  static constexpr uint8_t BUTTON_DOWN = 3;
  static constexpr uint8_t BUTTON_STOP = 5;

  /// Dooya remotes send a "press" burst and then a "release" frame (check != button).
  /// Only the press carries a command; the release frame must not be replayed or acted upon.
  bool is_press() const { return check == button; }
  const char *button_name() const;
};

struct RcSwitchCode {
  uint64_t code{0};
  uint8_t protocol{0};  ///< rc-switch protocol number (1-based)
  uint8_t nbits{0};
};

/// One decoded (or undecodable but plausible) RF frame.
struct RfFrame {
  Protocol protocol{Protocol::RAW};
  DooyaCode dooya{};
  RcSwitchCode rc_switch{};
  Timings raw{};  ///< kept for RAW frames only - decoded frames are re-encoded on transmit

  static RfFrame from_dooya(const DooyaCode &code);
  static RfFrame from_rc_switch(const RcSwitchCode &code);
  static RfFrame from_raw(Timings timings);

  /// Stable identity of the *command*, used to collapse remote repeats.
  /// Raw frames are quantised to 100 us so jitter between repeats does not change it.
  uint64_t fingerprint() const;

  /// Human readable one-liner, e.g. "Dooya id=0xA1B2C3 ch=0x21 UP". Always NUL-terminated.
  /// @return number of characters written (excluding NUL), truncated to `size - 1`.
  size_t describe(char *buffer, size_t size) const;
};

/// Total on-air time of one transmission: every |timing| summed, multiplied by repeats,
/// plus the pauses between repeats. Used by the duty-cycle limiter.
uint32_t airtime_us(const Timings &timings, uint32_t send_times, uint32_t send_wait_us);

}  // namespace esphome::rf_bridge
