#include "threadpool.h"
#include<climits>
const int TASKMAXSIZE = INT_MAX;
const int THREADMAXSIZE = 200;
const int IDLEMAXTIME = 60;//单位/秒

int Thread::_genertedId = 0;
Thread::Thread(threadWork threadfunc)
    :_threadfunc(threadfunc),
    _threadId(_genertedId++)
{}
Thread::~Thread() {}
int Thread::getThreadId()const {
    return _threadId;
}
void Thread::start()
{
    std::thread t(_threadfunc,_threadId);
    t.detach();
}

ThreadPool::ThreadPool(int initThreadSize)
    :_initThreadSize(initThreadSize),
    _curThreadSize(initThreadSize),
    _idleThreadSize(0),
    _maxThreadSize(THREADMAXSIZE),
    _curTaskSize(0),
    _maxTaskSize(TASKMAXSIZE),
    _poolMode(PoolMode::MODE_FIXED)
{}

ThreadPool::~ThreadPool() 
{
    //关闭线程池
    _isRunning = false;
    /*唤醒所有等待在_notEmpty条件上的线程*/
    //等待在_notEmpty条件上的线程有两种，一种是正在执行任务的线程，一种是阻塞等待任务执行的线程
    std::unique_lock<std::mutex> lock(_mtxPool);
    _notEmpty.notify_all();
    _condExit.wait(lock, [&]()->bool { return _pool.size() == 0; });
    std::cout << "threadPool exit!" << std::endl;
}
bool ThreadPool::getThreadPoolState()const
{
    return _isRunning;
}
void ThreadPool::setMode(PoolMode poolMode) {
    //如果线程池已经启动，就不能再继续对线程池进行任何设置
    if (getThreadPoolState())
        return;
    this->_poolMode = poolMode;
}
void ThreadPool::setTaskQueueMaxSize(int maxSize) {
    if (getThreadPoolState())
        return;
    _maxTaskSize = maxSize;
}

void ThreadPool::start()
{
    _isRunning = true;
    for (int i = 0; i < _initThreadSize; i++)
    {
        auto threadPtr = std::make_unique<Thread>(std::bind(&ThreadPool::threadWork, this, std::placeholders::_1));
        //_pool.emplace_back(std::move(threadPtr));
        _pool.emplace(threadPtr->getThreadId(), std::move(threadPtr));
    }

    for (auto& thread : _pool)
    {
        //threadPtr->start();
        thread.second->start();
        //启动任务，但并没有执行任务，属于空闲线程
        _idleThreadSize++;
    }
}

void ThreadPool::threadWork(int threadId)
{
    //记录线程空闲时的起始时间戳
    auto lasttime = std::chrono::high_resolution_clock().now();
    while(1)
    {
        std::shared_ptr<Task> taskPtr;
        {
            std::unique_lock<std::mutex> lock(_mtxPool);
            std::cout << threadId << " [" << std::this_thread::get_id() << "] 尝试获取任务..." << std::endl;
            /*
            阻塞的线程被唤醒有两种情况，分别是被任务队列唤醒，表示需要执行任务
            一种是线程池已经关闭，需要清理线程，判别这两种情况的办法就是看线程池的关闭标志
            */
            while(_curTaskSize==0)
            {
                if (!_isRunning)
                {
                    /*如果线程池已经关闭,需要清理线程资源，并通知线程池的析构函数，释放线程池*/
                    //关闭线程池后清理执行任务后的线程
                    _pool.erase(threadId);
                    _curThreadSize--;
                    _idleThreadSize--;
                    std::cout<<threadId<<"[" << std::this_thread::get_id() << "]exit!" << std::endl;
                    //线程清理完毕，通知线程池（析构函数）可以关闭了
                    _condExit.notify_all();
                    return;
                }
                if (_poolMode == PoolMode::MODE_CACHED)
                {
                    /*
                    等待在_notEmpty条件上的线程，有可能是在等待执行任务的线程，也有可能是确实空闲下来的线程
                    区别这两种线程的办法就是，每隔1s就判断等待在_notEmpty条件上的线程的时间是否超过60s
                    如果空闲时间超过了60s，并且线程池中的线程数超过初始线程数，就将该线程清理掉
                    */
                    //等待在_notEmpty条件上的线程在1s内如果被通知就不是超时的，否则就是超时了
                    if (std::cv_status::timeout == _notEmpty.wait_for(lock, std::chrono::seconds(1)))
                    {
                        auto now = std::chrono::high_resolution_clock().now();
                        //空闲时间
                        auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lasttime);
                        if (dur.count() >= IDLEMAXTIME && _curThreadSize>_initThreadSize)
                        {
                            _pool.erase(threadId);
                            _curThreadSize--;
                            _idleThreadSize--;
                            std::cout << std::this_thread::get_id() << "exit!" << std::endl;
                            return;
                        }
                    }
                }else {//FIXED模式
                    //只有当队列不空的条件下才继续向下执行，否则就阻塞
                    _notEmpty.wait(lock);
                }
            }

            /*取出任务*/
            taskPtr = _taskQ.front();
            _taskQ.pop();
            _curTaskSize--;
            std::cout<< threadId << " [" << std::this_thread::get_id() << "]获取任务成功..." << std::endl;
            //如果取出一个任务之后仍旧有其他任务，就继续通知其他线程继续取出任务
            if (_curTaskSize > 0)
                _notEmpty.notify_all();
            _notFull.notify_all();
        }

        //执行任务
        if (taskPtr)
        {
            //开始执行任务，空闲线程数减1
            _idleThreadSize--;
            taskPtr->exec();
        }
        //执行任务结束，空闲线程加1
        _idleThreadSize++;
        //更新线程空闲的起始时间戳
        lasttime = std::chrono::high_resolution_clock().now();
    }
}

Result ThreadPool::submit(std::shared_ptr<Task> taskPtr) {
    std::unique_lock<std::mutex> lock(_mtxPool);
    /*
    //只有满足任务队列不满条件才能继续向下执行，否则就进行阻塞
    //如果阻塞，说明此时任务队列已经满了
    //如果阻塞了1s后仍旧在阻塞，说明此时任务任务繁忙，没有多余的线程执行任务，就爆出错误
    */
    if (!_notFull.wait_for(lock, std::chrono::seconds(1),
        [&]()->bool {return _taskQ.size() < _maxTaskSize; }))
    {
        std::cerr << "the task queue is Full! submit task fail!" << std::endl;
        // return std::move(Result(taskPtr,false));
        return Result(taskPtr, false);
    }
    _taskQ.emplace(taskPtr);
    _curTaskSize++;
    _notEmpty.notify_all();

    //在线程模式处于cache模式下，如果当前任务小而重要，就需要对线程池进行扩容
    if (_poolMode == PoolMode::MODE_CACHED
        && _idleThreadSize < _curTaskSize
        && _curThreadSize < _maxThreadSize)
    {
        //创建新线程
        auto threadPtr = std::make_unique<Thread>(std::bind(&ThreadPool::threadWork, this, std::placeholders::_1));
        int threadId = threadPtr->getThreadId();

        std::cout << ">>>>>create new thread " << threadId << " [" << std::this_thread::get_id() << "]"<< std::endl;

        _pool.emplace(threadId, std::move(threadPtr));
        _pool[threadId]->start();
        _curThreadSize++;
        _idleThreadSize++;
    }
    return Result(taskPtr);
}

Task::Task() :
    _result(nullptr) {}
void Task::exec()
{
    if(_result)
        _result->setVal(this->run());
}

void Task::setResult(Result* result)
{
    _result = result;
}

Result::Result(std::shared_ptr<Task> task, bool isValid)
    :_taskPtr(task), _isValid(isValid) 
{
    //初始化task指针里的result指针成员，让其能够正常执行exec成员函数
    _taskPtr->setResult(this);
}

Any Result::get()
{
    if (!_isValid)
        return "";
    //如果任务没有执行完，在这里进行阻塞，不将返回值进行返回
    _sem.wait();
    return std::move(_any);
}
void Result::setVal(Any any)
{
    _any = std::move(any);
    //成功获取任务的返回值，增加信号量资源
    _sem.post();
}
