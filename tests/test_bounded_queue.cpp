#include <gtest/gtest.h>

#include "bounded_queue.h"

using esphome::rf_bridge::BoundedQueue;

TEST(BoundedQueue, PopsInFifoOrder) {
  BoundedQueue<int> q(3);
  ASSERT_TRUE(q.try_push(1));
  ASSERT_TRUE(q.try_push(2));
  ASSERT_TRUE(q.try_push(3));
  EXPECT_EQ(q.pop(), 1);
  EXPECT_EQ(q.pop(), 2);
  EXPECT_EQ(q.pop(), 3);
  EXPECT_FALSE(q.pop().has_value());
}

TEST(BoundedQueue, TryPushRejectsWhenFullAndKeepsExistingItems) {
  BoundedQueue<int> q(2);
  ASSERT_TRUE(q.try_push(1));
  ASSERT_TRUE(q.try_push(2));
  EXPECT_FALSE(q.try_push(3));
  EXPECT_EQ(q.size(), 2u);
  EXPECT_EQ(q.pop(), 1);
  EXPECT_EQ(q.pop(), 2);
}

TEST(BoundedQueue, PushOverwriteEvictsOldest) {
  BoundedQueue<int> q(2);
  EXPECT_FALSE(q.push_overwrite(1));
  EXPECT_FALSE(q.push_overwrite(2));
  EXPECT_TRUE(q.push_overwrite(3));
  EXPECT_EQ(q.size(), 2u);
  EXPECT_EQ(q.pop(), 2);
  EXPECT_EQ(q.pop(), 3);
}

TEST(BoundedQueue, NewestIndexesFromMostRecent) {
  BoundedQueue<int> q(3);
  q.push_overwrite(10);
  q.push_overwrite(20);
  q.push_overwrite(30);
  q.push_overwrite(40);  // evicts 10
  ASSERT_NE(q.newest(0), nullptr);
  EXPECT_EQ(*q.newest(0), 40);
  EXPECT_EQ(*q.newest(1), 30);
  EXPECT_EQ(*q.newest(2), 20);
  EXPECT_EQ(q.newest(3), nullptr);
}

TEST(BoundedQueue, ZeroCapacityIsClampedToOne) {
  BoundedQueue<int> q(0);
  EXPECT_EQ(q.capacity(), 1u);
  EXPECT_TRUE(q.try_push(1));
  EXPECT_FALSE(q.try_push(2));
}

TEST(BoundedQueue, ShrinkingCapacityDropsOldest) {
  BoundedQueue<int> q(4);
  for (int i = 1; i <= 4; i++)
    q.try_push(i);
  q.set_capacity(2);
  EXPECT_EQ(q.size(), 2u);
  EXPECT_EQ(q.pop(), 3);
  EXPECT_EQ(q.pop(), 4);
}

TEST(BoundedQueue, ClearEmptiesTheQueue) {
  BoundedQueue<int> q(2);
  q.try_push(1);
  q.clear();
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(q.front(), nullptr);
}
