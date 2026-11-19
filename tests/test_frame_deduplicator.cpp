#include <gtest/gtest.h>

#include <cstdint>

#include "frame_deduplicator.h"

using esphome::rf_bridge::FrameDeduplicator;

TEST(FrameDeduplicator, FirstFrameIsNeverDuplicate) {
  FrameDeduplicator dedup(1500);
  EXPECT_FALSE(dedup.is_duplicate(42, 1000));
}

TEST(FrameDeduplicator, RepeatWithinWindowIsDuplicate) {
  FrameDeduplicator dedup(1500);
  EXPECT_FALSE(dedup.is_duplicate(42, 1000));
  EXPECT_TRUE(dedup.is_duplicate(42, 1100));
  EXPECT_TRUE(dedup.is_duplicate(42, 2400));
}

TEST(FrameDeduplicator, WindowSlidesWhileButtonIsHeld) {
  FrameDeduplicator dedup(1500);
  EXPECT_FALSE(dedup.is_duplicate(42, 0));
  // Each repeat refreshes the window, so a 5 s press is still a single event.
  for (uint32_t t = 1000; t <= 5000; t += 1000)
    EXPECT_TRUE(dedup.is_duplicate(42, t)) << "at t=" << t;
}

TEST(FrameDeduplicator, SameFrameAfterWindowIsNewEvent) {
  FrameDeduplicator dedup(1500);
  EXPECT_FALSE(dedup.is_duplicate(42, 1000));
  EXPECT_FALSE(dedup.is_duplicate(42, 2500));
}

TEST(FrameDeduplicator, DifferentFrameIsNewEvent) {
  FrameDeduplicator dedup(1500);
  EXPECT_FALSE(dedup.is_duplicate(42, 1000));
  EXPECT_FALSE(dedup.is_duplicate(43, 1001));
  // ...and the previous one is forgotten once something else was seen.
  EXPECT_FALSE(dedup.is_duplicate(42, 1002));
}

TEST(FrameDeduplicator, SurvivesMillisWrapAround) {
  FrameDeduplicator dedup(1500);
  const uint32_t before_wrap = UINT32_MAX - 200;
  EXPECT_FALSE(dedup.is_duplicate(7, before_wrap));
  EXPECT_TRUE(dedup.is_duplicate(7, 300));  // 501 ms later, across the wrap
  EXPECT_FALSE(dedup.is_duplicate(7, 300 + 1500));
}

TEST(FrameDeduplicator, ResetForgetsLastFrame) {
  FrameDeduplicator dedup(1500);
  EXPECT_FALSE(dedup.is_duplicate(42, 1000));
  dedup.reset();
  EXPECT_FALSE(dedup.is_duplicate(42, 1001));
}
