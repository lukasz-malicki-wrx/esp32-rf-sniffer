#include <gtest/gtest.h>

#include <cstdint>

#include "duty_cycle_limiter.h"

using esphome::rf_bridge::DutyCycleLimiter;

namespace {
constexpr uint32_t HOUR_MS = 3600u * 1000u;
}

TEST(DutyCycleLimiter, DisabledLimiterAllowsEverything) {
  DutyCycleLimiter limiter;
  EXPECT_FALSE(limiter.enabled());
  EXPECT_TRUE(limiter.can_transmit(0, UINT32_MAX));
  EXPECT_TRUE(limiter.fits_at_all(UINT32_MAX));
}

TEST(DutyCycleLimiter, OnePercentOfAnHourIs36Seconds) {
  DutyCycleLimiter limiter(0.01f, HOUR_MS);
  EXPECT_EQ(limiter.budget_us(), 36u * 1000u * 1000u);
}

TEST(DutyCycleLimiter, BlocksOnceBudgetIsSpent) {
  DutyCycleLimiter limiter(0.01f, HOUR_MS);  // 36 s budget
  const uint32_t frame_us = 1000u * 1000u;   // 1 s per transmission
  uint32_t now = 0;
  for (int i = 0; i < 36; i++) {
    ASSERT_TRUE(limiter.can_transmit(now, frame_us)) << "transmission " << i;
    limiter.record(now, frame_us);
    now += 1000;
  }
  EXPECT_FALSE(limiter.can_transmit(now, frame_us));
  EXPECT_NEAR(limiter.used_percent(now), 100.0f, 0.01f);
}

TEST(DutyCycleLimiter, BudgetRecoversAfterTheWindow) {
  DutyCycleLimiter limiter(0.01f, HOUR_MS);
  limiter.record(0, 36u * 1000u * 1000u);
  EXPECT_FALSE(limiter.can_transmit(1000, 1));
  // Half an hour later the airtime is still inside the window.
  EXPECT_FALSE(limiter.can_transmit(HOUR_MS / 2, 1));
  // One full window (plus one bucket of slack) later it has expired.
  EXPECT_TRUE(limiter.can_transmit(HOUR_MS + HOUR_MS / DutyCycleLimiter::BUCKETS, 1));
  EXPECT_EQ(limiter.used_us(HOUR_MS + HOUR_MS / DutyCycleLimiter::BUCKETS), 0u);
}

TEST(DutyCycleLimiter, OnlyExpiredBucketsAreReleased) {
  DutyCycleLimiter limiter(0.01f, HOUR_MS);
  const uint32_t bucket = HOUR_MS / DutyCycleLimiter::BUCKETS;  // 60 s
  limiter.record(0, 10'000'000);                                // 10 s at t=0
  limiter.record(30 * bucket, 20'000'000);                      // 20 s at t=30 min
  EXPECT_EQ(limiter.used_us(59 * bucket), 30'000'000u);
  // The first transmission leaves the window, the second one stays.
  EXPECT_EQ(limiter.used_us(61 * bucket), 20'000'000u);
}

TEST(DutyCycleLimiter, SingleFrameLongerThanBudgetNeverFits) {
  DutyCycleLimiter limiter(0.001f, 10'000);  // 10 ms budget per 10 s
  EXPECT_FALSE(limiter.fits_at_all(10'001));
  EXPECT_TRUE(limiter.fits_at_all(10'000));
}

TEST(DutyCycleLimiter, SurvivesMillisWrapAround) {
  DutyCycleLimiter limiter(0.01f, HOUR_MS);
  const uint32_t start = UINT32_MAX - 1000;
  limiter.record(start, 36'000'000);
  EXPECT_FALSE(limiter.can_transmit(start + 2000, 1));  // wrapped, still inside the window
  EXPECT_TRUE(limiter.can_transmit(start + HOUR_MS + HOUR_MS / DutyCycleLimiter::BUCKETS, 1));
}

TEST(DutyCycleLimiter, RatioIsClampedToZeroToOne) {
  DutyCycleLimiter negative(-0.5f, 10'000);
  EXPECT_FALSE(negative.enabled());
  DutyCycleLimiter above_one(2.0f, 10'000);
  EXPECT_EQ(above_one.budget_us(), 10'000u * 1000u);  // 100 % of the window
}

TEST(DutyCycleLimiter, DisabledLimiterReportsZeroUsage) {
  DutyCycleLimiter limiter;
  limiter.record(0, 1'000'000);
  EXPECT_EQ(limiter.used_percent(0), 0.0f);
}

TEST(DutyCycleLimiter, TinyWindowIsRaisedToOneMillisecondPerBucket) {
  DutyCycleLimiter limiter(0.5f, 1);  // shorter than BUCKETS ms
  EXPECT_EQ(limiter.budget_us(), DutyCycleLimiter::BUCKETS * 1000u / 2u);
}
