#ifndef THREADPOOL_H
#define THREADPOOL_H
#include<iostream>
#include<memory>
#include<thread>
#include<mutex>
#include<atomic>
#include<future>
#include<functional>
#include<unordered_map>
#include<queue>
#include "any.h"
#include "semaphore.h"

//线程类型
class Thread {
public:
    using threadWork = std::function<void(int)>;
    Thread(threadWork threadfunc);
    ~Thread();

    void start();
    int getThreadId()const;
private:
    threadWork _threadfunc;
    //使用全局变量，每次创建线程对象的时候就将id自增
    static int _genertedId;
    int _threadId;
};

//任务返回类型的声明
class Result;
//任务类型
class Task {
public:
    Task();
    ~Task() = default;
    //提供给用户使用的可重写任务函数
    virtual Any run() = 0;
    //任务执行函数
    void exec();
    void setResult(Result* result);
private:
    Result* _result;
};

//任务的返回类型
class Result {
public:
    Result(std::shared_ptr<Task> task, bool isValid = true);
    ~Result() = default;
    //获取任务的返回值，提供给用户使用
    Any get();
    //设置任务的返回值，由任务执行函数调用
    void setVal(Any any);
private:
    //包装的任务
    std::shared_ptr<Task> _taskPtr;
    //封装的任务返回值
    Any _any;
    Semaphore _sem;
    //判断返回值是否有效
    std::atomic<bool> _isValid;
};

enum class PoolMode {
    MODE_FIXED, //固定数量线程
    MODE_CACHED,//线程数量动态增长
};

class ThreadPool {
public:
    ThreadPool(int initThreadSize = std::thread::hardware_concurrency());
    // ThreadPool(int initThreadSize=4);
    ~ThreadPool();
    bool getThreadPoolState()const;
    void setMode(PoolMode poolMode);
    void setTaskQueueMaxSize(int maxSize);
    void start();
    Result submit(std::shared_ptr<Task> taskPtr);
    void threadWork(int threadId);

private:
    //线程队列
    //std::vector<std::unique_ptr<Thread>> _pool;
    std::unordered_map<int, std::unique_ptr<Thread>> _pool;
    //线程池的初始线程数
    int _initThreadSize;
    //线程池中当前已经有的线程数,不能使用vector来获取，因为vector不是线程安全的
    std::atomic_int _curThreadSize;
    //线程池中空闲的线程数
    std::atomic_int _idleThreadSize;
    //线程池中最大线程数
    int _maxThreadSize;
    PoolMode _poolMode;

    //任务队列
    std::queue<std::shared_ptr<Task>> _taskQ;
    //当前任务队列中的任务数
    std::atomic<int> _curTaskSize;
    //任务队列的最大值
    int _maxTaskSize;

    /*锁资源*/
    //互斥锁，用于保证任务队列的互斥性
    std::mutex _mtxPool;
    //线程需要的条件变量
    std::condition_variable _notEmpty;
    //任务队列需要的条件变量
    std::condition_variable _notFull;

    //标志线程池是否正在运行
    std::atomic<bool> _isRunning;

    //线程池的资源回收需要等到所有线程的资源回收后进行，因此需要一个条件变量进行通信控制
    std::condition_variable _condExit;
};
#endif