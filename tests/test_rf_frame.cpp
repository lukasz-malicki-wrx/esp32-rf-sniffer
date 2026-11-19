#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "rf_frame.h"

using esphome::rf_bridge::airtime_us;
using esphome::rf_bridge::DooyaCode;
using esphome::rf_bridge::Protocol;
using esphome::rf_bridge::RcSwitchCode;
using esphome::rf_bridge::RfFrame;
using esphome::rf_bridge::Timings;

namespace {

std::string describe(const RfFrame &frame) {
  char buffer[64];
  frame.describe(buffer, sizeof(buffer));
  return buffer;
}

DooyaCode dooya(uint8_t button, uint8_t check) {
  DooyaCode code;
  code.id = 0xA1B2C3;
  code.channel = 0x21;
  code.button = button;
  code.check = check;
  return code;
}

}  // namespace

TEST(DooyaCode, PressAndReleaseFrames) {
  EXPECT_TRUE(dooya(DooyaCode::BUTTON_UP, DooyaCode::BUTTON_UP).is_press());
  EXPECT_FALSE(dooya(DooyaCode::BUTTON_UP, 0x0E).is_press());
}

TEST(RfFrame, DescribesDooyaWithButtonName) {
  EXPECT_EQ(describe(RfFrame::from_dooya(dooya(DooyaCode::BUTTON_UP, 1))), "Dooya id=0xA1B2C3 ch=0x21 UP");
  EXPECT_EQ(describe(RfFrame::from_dooya(dooya(DooyaCode::BUTTON_DOWN, 3))), "Dooya id=0xA1B2C3 ch=0x21 DOWN");
  EXPECT_EQ(describe(RfFrame::from_dooya(dooya(DooyaCode::BUTTON_STOP, 5))), "Dooya id=0xA1B2C3 ch=0x21 STOP");
  EXPECT_EQ(describe(RfFrame::from_dooya(dooya(9, 9))), "Dooya id=0xA1B2C3 ch=0x21 btn=9");
}

TEST(RfFrame, DescribesRcSwitch) {
  RcSwitchCode code;
  code.code = 0x5A5A5A;
  code.protocol = 1;
  code.nbits = 24;
  EXPECT_EQ(describe(RfFrame::from_rc_switch(code)), "RC-Switch p1 24bit 0x5A5A5A");
}

TEST(RfFrame, DescribesRawWithPulseCountAndDuration) {
  EXPECT_EQ(describe(RfFrame::from_raw({4000, -2000, 500, -1500})), "Raw 4 pulses 8ms");
}

TEST(RfFrame, DescribeTruncatesAndAlwaysTerminates) {
  char buffer[8];
  std::memset(buffer, 'x', sizeof(buffer));
  const size_t written = RfFrame::from_dooya(dooya(1, 1)).describe(buffer, sizeof(buffer));
  EXPECT_EQ(written, 7u);
  EXPECT_EQ(std::string(buffer), "Dooya i");
}

TEST(RfFrame, DescribeHandlesEmptyBuffer) { EXPECT_EQ(RfFrame::from_dooya(dooya(1, 1)).describe(nullptr, 0), 0u); }

TEST(RfFrame, DooyaFingerprintIgnoresCheckSoPressAndReleaseCollapse) {
  EXPECT_EQ(RfFrame::from_dooya(dooya(1, 1)).fingerprint(), RfFrame::from_dooya(dooya(1, 0x0E)).fingerprint());
}

TEST(RfFrame, FingerprintSeparatesCommands) {
  const auto up = RfFrame::from_dooya(dooya(DooyaCode::BUTTON_UP, 1)).fingerprint();
  const auto down = RfFrame::from_dooya(dooya(DooyaCode::BUTTON_DOWN, 3)).fingerprint();
  auto other_channel = dooya(DooyaCode::BUTTON_UP, 1);
  other_channel.channel = 0x22;
  EXPECT_NE(up, down);
  EXPECT_NE(up, RfFrame::from_dooya(other_channel).fingerprint());
}

TEST(RfFrame, FingerprintSeparatesProtocols) {
  RcSwitchCode rc;
  rc.code = 0xA1B2C3;
  EXPECT_NE(RfFrame::from_rc_switch(rc).fingerprint(), RfFrame::from_raw({}).fingerprint());
}

TEST(RfFrame, RawFingerprintToleratesJitter) {
  const Timings a{4000, -2000, 500, -1500, 500, -1500};
  const Timings jittered{4030, -1980, 520, -1460, 470, -1520};
  EXPECT_EQ(RfFrame::from_raw(a).fingerprint(), RfFrame::from_raw(jittered).fingerprint());
}

TEST(RfFrame, RawFingerprintDetectsDifferentShape) {
  const Timings a{4000, -2000, 500, -1500};
  const Timings b{4000, -2000, 1500, -500};
  EXPECT_NE(RfFrame::from_raw(a).fingerprint(), RfFrame::from_raw(b).fingerprint());
}

TEST(Airtime, SumsMarksAndSpacesTimesRepeatsPlusPauses) {
  const Timings frame{1000, -500, 250, -250};  // 2 ms
  EXPECT_EQ(airtime_us(frame, 1, 8000), 2000u);
  EXPECT_EQ(airtime_us(frame, 6, 8000), 6u * 2000u + 5u * 8000u);
  EXPECT_EQ(airtime_us(frame, 0, 8000), 0u);
  EXPECT_EQ(airtime_us({}, 3, 0), 0u);
}

TEST(Airtime, SaturatesInsteadOfOverflowing) {
  const Timings huge{INT32_MAX, INT32_MIN + 1};
  EXPECT_EQ(airtime_us(huge, 10, 0), UINT32_MAX);
}

TEST(ProtocolName, NamesEveryProtocol) {
  EXPECT_STREQ(esphome::rf_bridge::protocol_name(Protocol::DOOYA), "Dooya");
  EXPECT_STREQ(esphome::rf_bridge::protocol_name(Protocol::RC_SWITCH), "RC-Switch");
  EXPECT_STREQ(esphome::rf_bridge::protocol_name(Protocol::RAW), "Raw");
}
