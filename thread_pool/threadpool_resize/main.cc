#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class DynamicThreadPool {
 public:
  // 构造函数：核心线程数、最大线程数
  DynamicThreadPool(size_t core_threads, size_t max_threads)
      : core_threads_(core_threads),
        max_threads_(max_threads),
        is_stop_(false),
        active_threads_(core_threads) {
    if (core_threads > max_threads) {
      throw std::invalid_argument("core threads > max threads");
    }
    // 启动核心线程（常驻，不销毁）
    for (size_t i = 0; i < core_threads_; ++i) {
      threads_.emplace_back(&DynamicThreadPool::core_worker_loop, this);
    }
  }

  // 禁用拷贝移动
  DynamicThreadPool(const DynamicThreadPool&) = delete;
  DynamicThreadPool& operator=(const DynamicThreadPool&) = delete;
  DynamicThreadPool(DynamicThreadPool&&) = delete;
  DynamicThreadPool& operator=(DynamicThreadPool&&) = delete;

  // 析构函数：优雅关闭
  ~DynamicThreadPool() { shutdown(); }

  // 提交任务
  template <typename F, typename... Args>
  auto Submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
    using RetType = decltype(f(args...));
    auto task = std::make_shared<std::packaged_task<RetType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<RetType> fut = task->get_future();

    {
      std::lock_guard<std::mutex> lock(mtx_);
      if (is_stop_) throw std::runtime_error("pool stopped");
      tasks_.emplace([task]() { (*task)(); });
    }

    // 启动临时线程（不超过最大线程数）
    if (active_threads_.load() < max_threads_) {
      start_temp_thread();
    }

    cv_task_.notify_one();
    return fut;
  }

  // 获取当前活跃线程数
  size_t GetActiveThreadCount() const { return active_threads_.load(); }

 private:
  // 核心线程循环（常驻）
  void core_worker_loop() {
    while (!is_stop_) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_task_.wait(lock, [this]() { return !tasks_.empty() || is_stop_; });
        if (is_stop_ && tasks_.empty()) break;
        if (tasks_.empty()) continue;
        task = std::move(tasks_.front());
        tasks_.pop();
      }
      task();
    }
  }

  // 临时线程循环（执行完任务后自动退出）
  void temp_worker_loop() {
    active_threads_.fetch_add(1);
    while (!is_stop_) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(mtx_);
        // 临时线程：等待任务或1秒超时（超时则退出）
        if (!cv_task_.wait_for(lock, std::chrono::seconds(1), [this]() {
              return !tasks_.empty() || is_stop_;
            })) {
          break;  // 超时退出
        }
        if (is_stop_ && tasks_.empty()) break;
        if (tasks_.empty()) continue;
        task = std::move(tasks_.front());
        tasks_.pop();
      }
      task();
    }
    active_threads_.fetch_sub(1);
  }

  // 启动临时线程
  void start_temp_thread() {
    std::thread t(&DynamicThreadPool::temp_worker_loop, this);
    t.detach();  // 临时线程detach，自动管理生命周期
  }

  // 关闭线程池
  void shutdown() {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      is_stop_ = true;
    }
    cv_task_.notify_all();
    // 等待核心线程退出
    for (auto& t : threads_) {
      if (t.joinable()) t.join();
    }
  }

  const size_t core_threads_;         // 核心线程数（常驻）
  const size_t max_threads_;          // 最大线程数
  std::vector<std::thread> threads_;  // 核心线程列表
  std::queue<std::function<void()>> tasks_;
  std::mutex mtx_;
  std::condition_variable cv_task_;
  std::atomic<bool> is_stop_;
  std::atomic<size_t> active_threads_;  // 活跃线程数（核心+临时）
};

// 测试代码
int main() {
  DynamicThreadPool pool(2, 6);  // 核心2，最大6

  std::cout << "=== 初始状态 ===" << std::endl;
  std::cout << "核心线程数：2" << std::endl;
  std::cout << "当前活跃线程数：" << pool.GetActiveThreadCount() << std::endl;

  // 提交10个耗时任务
  std::vector<std::future<int>> futures;
  for (int i = 0; i < 10; ++i) {
    futures.emplace_back(pool.Submit([i]() -> int {
      std::cout << "Task " << i
                << " running in thread: " << std::this_thread::get_id()
                << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      return i;
    }));
  }

  // 高峰期查看线程数
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::cout << "\n=== 任务高峰期 ===" << std::endl;
  std::cout << "当前活跃线程数：" << pool.GetActiveThreadCount()
            << std::endl;  // 最多6

  // 等待任务完成
  for (auto& fut : futures) fut.get();
  std::cout << "\n=== 任务完成后 ===" << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(2));  // 等待临时线程超时退出
  std::cout << "当前活跃线程数：" << pool.GetActiveThreadCount()
            << std::endl;  // 回到2

  return 0;
}