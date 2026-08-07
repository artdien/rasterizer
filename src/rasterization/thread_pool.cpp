#include "rasterization/thread_pool.hpp"

#include <iostream>
#include <print>
#include <stddef.h>
#include <stdexcept>
#include <utility>

namespace rasterizer::rasterization {

ThreadPool::ThreadPool(u32 capacity) : pending_tasks_ {0}, barrier_ {static_cast<ptrdiff_t>(capacity)}, current_ {0u}, capacity_ {capacity}, running_ {true} {
  // For our use-case, this is a good initial value
  tasks_.reserve(capacity);

  threads_.reserve(capacity);
  for (auto i {0u}; i < capacity; ++i) {
    threads_.emplace_back([this]() { thread_execution_loop(); });
  }
}

ThreadPool::~ThreadPool() {
  {
    const auto lock {std::unique_lock {mutex_}};
    running_ = false;
  }

  task_available_.notify_all();
}

auto ThreadPool::schedule(std::move_only_function<void()> task, bool notify) -> void {
  pending_tasks_.fetch_add(1, std::memory_order_release);

  {
    const auto lock {std::lock_guard {mutex_}};

    if (!running_) {
      throw std::runtime_error("Trying to schedule a task on a non-running thread pool");
    }

    tasks_.emplace_back(std::move(task));
  }

  if (notify) {
    task_available_.notify_one();
  }
}

auto ThreadPool::notify() -> void {
  task_available_.notify_all();
}

auto ThreadPool::sync() -> void {
  auto lock {std::unique_lock {mutex_}};
  task_finished_.wait(lock, [this] { return pending_tasks_.load(std::memory_order_acquire) == 0; });
}

auto ThreadPool::barrier() -> void {
  barrier_.arrive_and_wait();
}

auto ThreadPool::reset() -> void {
  auto lock {std::lock_guard {mutex_}};
  tasks_.clear();
  current_ = 0u;
}

auto ThreadPool::thread_execution_loop() -> void {
  while (true) {
    auto task {std::move_only_function<void()> {}};

    {
      auto lock {std::unique_lock {mutex_}};
      task_available_.wait(lock, [this] { return !running_ || current_ < tasks_.size(); });

      if (!running_ && current_ >= tasks_.size()) {
        return;
      }

      if (current_ < tasks_.size()) {
        task = std::move(tasks_[current_]);
        current_++;
      }
    }

    if (task) {
      try {
        task();
      } catch (const std::exception& e) {
        std::println(std::cerr, "Task in thread pool threw an exception: {}", e.what());
      }

      if (pending_tasks_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        auto lock {std::lock_guard {mutex_}};

        // Assumption:
        // sync() is only ever called by one thread at any given time.
        // Using this assumption, we can notify only one thread to wake up.
        // If this assumption does not hold, it will likely end up in a deadlock.
        // Waking only one thread up is used to avoid the 'thundering herd' problem
        // of too many woken up threads trying to access the same mutex.
        task_finished_.notify_one();
      }
    }
  }
}

} // namespace rasterizer::rasterization
