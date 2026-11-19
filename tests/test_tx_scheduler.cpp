#include <gtest/gtest.h>

#include <string>

#include "tx_scheduler.h"

using esphome::rf_bridge::EnqueueResult;
using esphome::rf_bridge::TxJob;
using esphome::rf_bridge::TxScheduler;

namespace {

TxJob job(const std::string &label, int32_t mark_us = 1000) {
  TxJob j;
  j.timings = {mark_us, -mark_us};
  j.send_times = 1;
  j.label = label;
  return j;
}

TxScheduler scheduler(size_t capacity = 4, uint32_t gap_ms = 100) {
  TxScheduler s;
  s.set_capacity(capacity);
  s.set_gap_ms(gap_ms);
  return s;
}

}  // namespace

TEST(TxScheduler, EmptyQueueYieldsNothing) {
  auto s = scheduler();
  EXPECT_FALSE(s.next(0).has_value());
}

TEST(TxScheduler, FirstJobIsSentImmediately) {
  auto s = scheduler();
  ASSERT_EQ(s.enqueue(job("a")), EnqueueResult::QUEUED);
  auto next = s.next(0);
  ASSERT_TRUE(next.has_value());
  EXPECT_EQ(next->label, "a");
  EXPECT_EQ(s.pending(), 0u);
}

TEST(TxScheduler, JobsAreSerialisedWithAGap) {
  auto s = scheduler(4, 100);
  s.enqueue(job("a"));
  s.enqueue(job("b"));
  s.enqueue(job("c"));

  auto a = s.next(1000);
  ASSERT_TRUE(a);
  s.mark_sent(1010, a->airtime_us());  // transmission took 10 ms

  EXPECT_FALSE(s.next(1050).has_value()) << "gap counts from the end of the previous frame";
  EXPECT_FALSE(s.next(1109).has_value());
  auto b = s.next(1110);
  ASSERT_TRUE(b);
  EXPECT_EQ(b->label, "b");
  s.mark_sent(1120, b->airtime_us());

  auto c = s.next(1220);
  ASSERT_TRUE(c);
  EXPECT_EQ(c->label, "c");
}

TEST(TxScheduler, RejectsWhenQueueIsFull) {
  auto s = scheduler(2);
  EXPECT_EQ(s.enqueue(job("a")), EnqueueResult::QUEUED);
  EXPECT_EQ(s.enqueue(job("b")), EnqueueResult::QUEUED);
  EXPECT_EQ(s.enqueue(job("c")), EnqueueResult::QUEUE_FULL);
  EXPECT_EQ(s.pending(), 2u);
  EXPECT_EQ(s.next(0)->label, "a");
}

TEST(TxScheduler, RejectsEmptyJobs) {
  auto s = scheduler();
  TxJob empty;
  EXPECT_EQ(s.enqueue(empty), EnqueueResult::EMPTY);
  auto zero_repeats = job("z");
  zero_repeats.send_times = 0;
  EXPECT_EQ(s.enqueue(zero_repeats), EnqueueResult::EMPTY);
}

TEST(TxScheduler, DutyCycleHoldsTheQueueInOrder) {
  auto s = scheduler(4, 0);
  s.set_duty_cycle(0.01f, 100'000);  // 1 % of 100 s = 1 s of airtime
  // Each job: 300 ms mark + 300 ms space = 600 ms airtime -> only one fits.
  s.enqueue(job("first", 300'000));
  s.enqueue(job("second", 300'000));

  auto first = s.next(0);
  ASSERT_TRUE(first);
  s.mark_sent(0, first->airtime_us());

  EXPECT_FALSE(s.next(1000).has_value());
  EXPECT_TRUE(s.throttled());
  EXPECT_EQ(s.pending(), 1u);

  auto second = s.next(100'000 + 100'000 / 60);
  ASSERT_TRUE(second) << "budget released after the window";
  EXPECT_EQ(second->label, "second");
  EXPECT_FALSE(s.throttled());
}

TEST(TxScheduler, RejectsJobLongerThanWholeBudget) {
  auto s = scheduler();
  s.set_duty_cycle(0.01f, 100'000);                                               // 1 s budget
  EXPECT_EQ(s.enqueue(job("too long", 600'000)), EnqueueResult::EXCEEDS_BUDGET);  // 1.2 s
  EXPECT_EQ(s.pending(), 0u);
}

TEST(TxScheduler, ClearDropsPendingJobs) {
  auto s = scheduler();
  s.enqueue(job("a"));
  s.enqueue(job("b"));
  s.clear();
  EXPECT_EQ(s.pending(), 0u);
  EXPECT_FALSE(s.next(0).has_value());
}

TEST(TxScheduler, AirtimeIncludesRepeats) {
  auto j = job("r", 1000);  // 2 ms per frame
  j.send_times = 6;
  j.send_wait_us = 8000;
  EXPECT_EQ(j.airtime_us(), 6u * 2000u + 5u * 8000u);
}

TEST(TxScheduler, NamesEveryEnqueueResult) {
  using esphome::rf_bridge::enqueue_result_name;
  EXPECT_STREQ(enqueue_result_name(EnqueueResult::QUEUED), "queued");
  EXPECT_STREQ(enqueue_result_name(EnqueueResult::QUEUE_FULL), "queue full");
  EXPECT_STREQ(enqueue_result_name(EnqueueResult::EXCEEDS_BUDGET), "longer than the duty-cycle budget");
  EXPECT_STREQ(enqueue_result_name(EnqueueResult::EMPTY), "empty frame");
}

TEST(TxScheduler, ReportsDutyCycleUsage) {
  auto s = scheduler(4, 0);
  EXPECT_FALSE(s.duty_cycle_enabled());
  s.set_duty_cycle(0.01f, 100'000);  // 1 s budget
  EXPECT_TRUE(s.duty_cycle_enabled());
  auto j = s.enqueue(job("half", 250'000));  // 500 ms
  ASSERT_EQ(j, EnqueueResult::QUEUED);
  auto sent = s.next(0);
  ASSERT_TRUE(sent);
  s.mark_sent(0, sent->airtime_us());
  EXPECT_NEAR(s.duty_cycle_used_percent(10), 50.0f, 0.01f);
}
