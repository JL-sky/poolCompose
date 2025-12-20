#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class SimpleThreadPool {
 public:
  explicit SimpleThreadPool(
      size_t core_size = std::thread::hardware_concurrency())
      : is_stop_(false) {
    for (size_t i = 0; i < core_size; ++i) {
      threads_.emplace_back([this]() { this->work(); });
    }
  }

  SimpleThreadPool(const SimpleThreadPool&) = delete;
  SimpleThreadPool& operator=(const SimpleThreadPool&) = delete;
  SimpleThreadPool(SimpleThreadPool&&) = delete;
  SimpleThreadPool& operator=(SimpleThreadPool&&) = delete;

  ~SimpleThreadPool() { shutdown(); }

  template <typename F, typename... Args>
  auto Submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
    using funcReturnType = decltype(f(args...));
    // 包装任务为packaged_task，用shared_ptr管理生命周期
    auto task_ptr = std::make_shared<std::packaged_task<funcReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    // 获取future，用于外部获取结果
    std::future<funcReturnType> res = task_ptr->get_future();

    {
      std::lock_guard<std::mutex> lock(mtx_);
      if (is_stop_) {
        throw std::runtime_error("threadpool has stopped, cannot submit task");
      }
      tasks_.emplace([task_ptr]() {
        (*task_ptr)();  // 执行任务，结果会写入packaged_task
      });
    }

    cv_.notify_one();

    return res;
  }

 private:
  // 工作线程核心逻辑
  void work() {
    while (true) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(mtx_);
        // 等待条件：有任务 或 线程池停止
        cv_.wait(lock, [this]() { return !tasks_.empty() || is_stop_; });

        // 退出条件：线程池停止 且 任务队列为空（避免丢弃任务）
        if (is_stop_ && tasks_.empty()) {
          break;
        }

        // 取出任务（移动语义，减少拷贝）
        task = std::move(tasks_.front());
        tasks_.pop();
      }

      // 执行任务（解锁后执行，提高并发）
      try {
        task();
      } catch (const std::exception& e) {
        // 捕获任务执行异常，避免线程崩溃
        std::cerr << "Task execution error: " << e.what() << std::endl;
      }
    }
  }

  // 关闭线程池：停止接受任务，唤醒所有线程，等待退出
  void shutdown() {
    // 双重检查，避免重复调用
    if (is_stop_) {
      return;
    }

    {
      std::lock_guard<std::mutex> lock(mtx_);
      is_stop_ = true;
    }

    // 唤醒所有等待的线程
    cv_.notify_all();

    // 等待所有线程退出
    for (auto& thread : threads_) {
      if (thread.joinable()) {
        thread.join();
      }
    }

    // 清空线程列表
    threads_.clear();
  }

  // 成员变量定义（统一命名+初始化）
  std::vector<std::thread> threads_;         // 工作线程列表
  std::queue<std::function<void()>> tasks_;  // 任务队列
  std::condition_variable cv_;               // 任务队列非空条件变量
  std::mutex mtx_;                           // 保护任务队列和停止标志
  std::atomic<bool> is_stop_;  // 线程池停止标志（原子变量，默认初始化）
};

// // 测试代码：验证线程池功能
// int main() {
//   // 创建4个核心线程的线程池
//   SimpleThreadPool pool(4);

//   // 提交10个任务，每个任务返回自己的编号
//   std::vector<std::future<int>> futures;
//   for (int i = 0; i < 10; ++i) {
//     auto fut = pool.Submit([i]() -> int {
//       std::cout << "Task " << i
//                 << " is running in thread: " << std::this_thread::get_id()
//                 << std::endl;
//       std::this_thread::sleep_for(
//           std::chrono::milliseconds(100));  // 模拟任务耗时
//       return i;
//     });
//     futures.push_back(std::move(fut));
//   }

//   // 获取所有任务结果
//   for (auto& fut : futures) {
//     std::cout << "Task " << fut.get() << " completed" << std::endl;
//   }

//   return 0;
// }