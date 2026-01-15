#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class DynamicThreadPool {
 public:
  // C++17：inline static constexpr 无需类外定义
  inline static constexpr std::chrono::seconds IDLE_TIMEOUT =
      std::chrono::seconds(2);

  // 构造函数：启动核心线程
  DynamicThreadPool(size_t minThreads, size_t maxThreads)
      : mMinThreads(minThreads), mMaxThreads(maxThreads), mStopping(false) {
    if (minThreads > maxThreads) {
      throw std::invalid_argument(
          "minThreads cannot be greater than maxThreads");
    }
    // 启动核心线程（由主线程执行，安全）
    std::unique_lock<std::mutex> lock(mMutex);
    for (size_t i = 0; i < minThreads; ++i) {
      mThreads.emplace_back(&DynamicThreadPool::workerLoop, this);
    }
  }

  // 析构函数：安全停止线程池
  ~DynamicThreadPool() { stop(); }

  // 提交任务
  void enqueue(std::function<void()> task) {
    {
      std::unique_lock<std::mutex> lock(mMutex);
      if (mStopping) {
        throw std::runtime_error(
            "Cannot enqueue task: thread pool is stopping");
      }
      mTasks.emplace(std::move(task));
    }
    // 唤醒一个空闲线程处理任务
    mCv.notify_one();
    // 主线程管理线程池大小（扩容+回收空闲线程）
    managePoolSize();
  }

 private:
  size_t mMinThreads;                        // 核心线程数（常驻）
  size_t mMaxThreads;                        // 最大线程数（扩容上限）
  std::vector<std::thread> mThreads;         // 工作线程列表（仅主线程修改）
  std::queue<std::function<void()>> mTasks;  // 任务队列
  std::condition_variable mCv;               // 条件变量：任务/停止通知
  std::mutex mMutex;                         // 全局互斥锁：保护所有共享资源
  std::atomic<bool> mStopping;  // 原子变量：线程池停止标记（避免竞态）

  // 工作线程核心逻辑：仅处理任务，不修改mThreads
  void workerLoop() {
    while (!mStopping) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(mMutex);
        // 限时等待：超时则退出（缩容）
        bool hasTask = mCv.wait_for(lock, IDLE_TIMEOUT, [this] {
          return mStopping || !mTasks.empty();
        });

        // 场景1：线程池停止 → 退出
        if (mStopping) {
          break;
        }

        // 场景2：超时无任务 → 非核心线程退出（缩容）
        if (!hasTask) {
          // 仅当当前线程数>核心线程数时，空闲线程退出
          // 注意：这里只读取mThreads.size()，不修改！
          if (mThreads.size() > mMinThreads) {
            break;
          } else {
            continue;  // 核心线程继续等待
          }
        }

        // 场景3：有任务 → 取出执行
        task = std::move(mTasks.front());
        mTasks.pop();
      }

      task();
    }
    // 工作线程退出：仅自身结束，不修改mThreads！
  }

  // 停止线程池：主线程统一join所有线程
  void stop() {
    mStopping = true;
    mCv.notify_all();  // 唤醒所有等待的线程

    // 主线程统一处理：join所有线程（确保线程退出后再析构）
    std::unique_lock<std::mutex> lock(mMutex);
    for (auto& t : mThreads) {
      if (t.joinable()) {
        t.join();  // 等待线程执行完毕，此时t.joinable()变为false
      }
    }
    mThreads.clear();  // 此时erase是安全的（所有线程已join）
  }

  // 管理线程池：主线程执行（扩容 + 回收已退出的线程）
  void managePoolSize() {
    std::unique_lock<std::mutex> lock(mMutex);

    // 第一步：回收已退出的线程（仅主线程执行，安全！）
    // 遍历列表，移除所有已退出（!joinable()）的线程
    auto it =
        std::remove_if(mThreads.begin(), mThreads.end(), [](std::thread& t) {
          if (!t.joinable()) {
            // 线程已退出，析构是安全的
            return true;
          }
          return false;
        });
    mThreads.erase(it, mThreads.end());

    // 第二步：扩容逻辑（仅主线程执行）
    size_t currentThreads = mThreads.size();
    // 扩容条件：任务数>当前线程数 且 未达最大线程数
    if (mTasks.size() > currentThreads && currentThreads < mMaxThreads) {
      // 新增线程（主线程操作，安全）
      mThreads.emplace_back(&DynamicThreadPool::workerLoop, this);
    }
  }
};

// 示例任务：打印线程ID + 模拟耗时
void exampleTask() {
  std::cout << "Task executed by thread " << std::this_thread::get_id()
            << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

int main() {
  try {
    // 创建线程池：核心2个，最大8个
    DynamicThreadPool pool(2, 8);

    // 提交16个任务（触发扩容）
    std::cout << "=== Submitting 16 tasks ===" << std::endl;
    for (int i = 0; i < 16; ++i) {
      pool.enqueue(exampleTask);
    }

    // 等待3秒：任务执行完毕 + 空闲线程超时退出（缩容）
    std::cout << "=== Waiting for idle timeout ===" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // 提交2个任务（核心线程处理）
    std::cout << "=== Submitting 2 more tasks ===" << std::endl;
    for (int i = 0; i < 2; ++i) {
      pool.enqueue(exampleTask);
    }

    // 等待任务执行完毕
    std::this_thread::sleep_for(std::chrono::seconds(1));
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  std::cout << "=== Program exited normally ===" << std::endl;
  return 0;
}