#pragma once

#include <atomic>
#include <barrier>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "platform/types.hpp"

namespace rasterizer::rasterization {

class ThreadPool {
public:
  /// @brief Creates a thread pool with a given number of worker threads.
  ///
  /// @param capacity Number of threads to create in the thread pool.
  ThreadPool(u32 capacity);

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  auto operator=(const ThreadPool&) -> ThreadPool& = delete;
  auto operator=(ThreadPool&&) -> ThreadPool& = delete;
  ~ThreadPool();

  /// @brief Schedules a task to run on the thread pool.
  ///
  /// Tasks are stored in an internal array and consumed sequentially by worker threads.
  /// The method sync() can be used to wait for completion of all tasks.
  ///
  /// @param task   Task to be scheduled for execution.
  /// @param notify If true, notifies one idle worker thread to pick up the task.
  ///               Set to false when batching multiple schedule() calls, followed by a manual call to notify().
  auto schedule(std::move_only_function<void()> task, bool notify = true) -> void;

  /// @brief Wakes all worker threads to pick up pending tasks.
  ///
  /// Use after batching multiple schedule() calls with notify=false.
  auto notify() -> void;

  /// @brief Blocks until all scheduled tasks have completed execution.
  ///
  /// This waits for the pending tasks counter to reach zero,
  /// meaning no thread is currently executing a task and the queue has been fully drained.
  ///
  /// @note: Relies on the assumption that only one caller invokes sync() at any given time.
  ///        Multiple concurrent callers may cause a deadlock.
  auto sync() -> void;

  /// @brief Blocks all worker threads until each has called this method once.
  ///
  /// Call from within a scheduled task to ensure all threads reach the same point before proceeding.
  auto barrier() -> void;

  /// @brief Clears the task queue.
  auto reset() -> void;

  /// @brief Returns the number of threads in the pool.
  auto capacity() -> u32 {
    return capacity_;
  }

private:
  // Put the variables prone to memory contention into separate cache lines.
  alignas(std::hardware_destructive_interference_size) std::mutex mutex_;
  alignas(std::hardware_destructive_interference_size) std::atomic<i32> pending_tasks_;

  std::condition_variable task_available_;
  std::condition_variable task_finished_;

  std::vector<std::move_only_function<void()>> tasks_;
  std::vector<std::jthread> threads_;
  std::barrier<> barrier_;

  u32 current_;
  u32 capacity_;
  bool running_;

  auto thread_execution_loop() -> void;
};

} // namespace rasterizer::rasterization
