#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <mutex>
#include <condition_variable>

class Semaphore {
public:
    Semaphore(int resLimit = 0);
    ~Semaphore() = default;
    Semaphore(const Semaphore&) = delete; // 禁用复制构造函数
    Semaphore& operator=(const Semaphore&) = delete; // 禁用复制赋值运算符

    Semaphore(Semaphore&&) noexcept = default; // 启用移动构造函数
    Semaphore& operator=(Semaphore&&) noexcept = default; // 启用移动赋值运算符

    void wait();
    void post();

private:
    int _resLimit;
    std::mutex _mtx;
    std::condition_variable _condMtx;
};

#endif // SEMAPHORE_H
