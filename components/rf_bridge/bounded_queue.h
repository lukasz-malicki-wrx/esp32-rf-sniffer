#pragma once

// Pure C++17, no ESPHome dependencies: compiled on the host by the unit tests.

#include <cstddef>
#include <deque>
#include <optional>
#include <utility>

namespace esphome::rf_bridge {

/// FIFO with a hard capacity, chosen at runtime from the YAML config.
///
/// Two overflow policies, one per use case:
///  - try_push(): rejects the new item when full (TX queue - never silently drop a command
///    that is already waiting, report the rejected one instead),
///  - push_overwrite(): evicts the oldest item (capture history - newest frames matter most).
template<typename T>
class BoundedQueue {
 public:
  explicit BoundedQueue(size_t capacity = 1) : capacity_(capacity == 0 ? 1 : capacity) {}

  void set_capacity(size_t capacity) {
    capacity_ = capacity == 0 ? 1 : capacity;
    while (items_.size() > capacity_)
      items_.pop_front();
  }

  /// @return false (and leaves the queue untouched) when the queue is full.
  bool try_push(T item) {
    if (full())
      return false;
    items_.push_back(std::move(item));
    return true;
  }

  /// @return true when the oldest item had to be evicted to make room.
  bool push_overwrite(T item) {
    const bool evicted = full();
    if (evicted)
      items_.pop_front();
    items_.push_back(std::move(item));
    return evicted;
  }

  std::optional<T> pop() {
    if (items_.empty())
      return std::nullopt;
    T item = std::move(items_.front());
    items_.pop_front();
    return item;
  }

  const T *front() const { return items_.empty() ? nullptr : &items_.front(); }

  /// Newest-first access: 0 is the most recently pushed item.
  const T *newest(size_t age) const {
    if (age >= items_.size())
      return nullptr;
    return &items_[items_.size() - 1 - age];
  }

  void clear() { items_.clear(); }
  size_t size() const { return items_.size(); }
  size_t capacity() const { return capacity_; }
  bool empty() const { return items_.empty(); }
  bool full() const { return items_.size() >= capacity_; }

 private:
  size_t capacity_;
  std::deque<T> items_;
};

}  // namespace esphome::rf_bridge
