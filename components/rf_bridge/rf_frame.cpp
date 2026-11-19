#include "rf_frame.h"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace esphome::rf_bridge {

namespace {

constexpr uint64_t FNV_OFFSET = 1469598103934665603ULL;
constexpr uint64_t FNV_PRIME = 1099511628211ULL;
constexpr int32_t RAW_QUANTUM_US = 100;

uint64_t fnv1a(uint64_t hash, uint64_t value) {
  for (int i = 0; i < 8; i++) {
    hash ^= (value >> (i * 8)) & 0xFF;
    hash *= FNV_PRIME;
  }
  return hash;
}

size_t clamp_written(int written, size_t size) {
  if (written < 0)
    return 0;
  return static_cast<size_t>(written) >= size ? size - 1 : static_cast<size_t>(written);
}

}  // namespace

const char *protocol_name(Protocol protocol) {
  switch (protocol) {
    case Protocol::DOOYA:
      return "Dooya";
    case Protocol::RC_SWITCH:
      return "RC-Switch";
    case Protocol::RAW:
      return "Raw";
  }
  return "?";
}

const char *DooyaCode::button_name() const {
  switch (button) {
    case BUTTON_UP:
      return "UP";
    case BUTTON_DOWN:
      return "DOWN";
    case BUTTON_STOP:
      return "STOP";
    default:
      return nullptr;
  }
}

RfFrame RfFrame::from_dooya(const DooyaCode &code) {
  RfFrame frame;
  frame.protocol = Protocol::DOOYA;
  frame.dooya = code;
  return frame;
}

RfFrame RfFrame::from_rc_switch(const RcSwitchCode &code) {
  RfFrame frame;
  frame.protocol = Protocol::RC_SWITCH;
  frame.rc_switch = code;
  return frame;
}

RfFrame RfFrame::from_raw(Timings timings) {
  RfFrame frame;
  frame.protocol = Protocol::RAW;
  frame.raw = std::move(timings);
  return frame;
}

uint64_t RfFrame::fingerprint() const {
  uint64_t hash = fnv1a(FNV_OFFSET, static_cast<uint64_t>(protocol));
  switch (protocol) {
    case Protocol::DOOYA:
      // `check` deliberately excluded: press and release of the same button are one command.
      hash = fnv1a(hash, dooya.id);
      hash = fnv1a(hash, dooya.channel);
      hash = fnv1a(hash, dooya.button);
      break;
    case Protocol::RC_SWITCH:
      hash = fnv1a(hash, rc_switch.code);
      hash = fnv1a(hash, rc_switch.protocol);
      hash = fnv1a(hash, rc_switch.nbits);
      break;
    case Protocol::RAW:
      for (int32_t t : raw) {
        // Round half away from zero, keeping the mark/space sign.
        const int32_t q = (t >= 0 ? t + RAW_QUANTUM_US / 2 : t - RAW_QUANTUM_US / 2) / RAW_QUANTUM_US;
        hash = fnv1a(hash, static_cast<uint64_t>(static_cast<int64_t>(q)));
      }
      break;
  }
  return hash;
}

size_t RfFrame::describe(char *buffer, size_t size) const {
  if (buffer == nullptr || size == 0)
    return 0;
  int written = 0;
  switch (protocol) {
    case Protocol::DOOYA: {
      const char *name = dooya.button_name();
      if (name != nullptr) {
        written = std::snprintf(buffer, size, "Dooya id=0x%06" PRIX32 " ch=0x%02X %s", dooya.id,
                                static_cast<unsigned>(dooya.channel), name);
      } else {
        written = std::snprintf(buffer, size, "Dooya id=0x%06" PRIX32 " ch=0x%02X btn=%u", dooya.id,
                                static_cast<unsigned>(dooya.channel), static_cast<unsigned>(dooya.button));
      }
      break;
    }
    case Protocol::RC_SWITCH:
      written = std::snprintf(buffer, size, "RC-Switch p%u %ubit 0x%" PRIX64, static_cast<unsigned>(rc_switch.protocol),
                              static_cast<unsigned>(rc_switch.nbits), rc_switch.code);
      break;
    case Protocol::RAW:
      written = std::snprintf(buffer, size, "Raw %u pulses %" PRIu32 "ms", static_cast<unsigned>(raw.size()),
                              airtime_us(raw, 1, 0) / 1000);
      break;
  }
  return clamp_written(written, size);
}

uint32_t airtime_us(const Timings &timings, uint32_t send_times, uint32_t send_wait_us) {
  if (send_times == 0)
    return 0;
  uint64_t single = 0;
  for (int32_t t : timings)
    single += static_cast<uint64_t>(std::llabs(static_cast<long long>(t)));
  const uint64_t total = single * send_times + static_cast<uint64_t>(send_wait_us) * (send_times - 1);
  return total > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(total);
}

}  // namespace esphome::rf_bridge
