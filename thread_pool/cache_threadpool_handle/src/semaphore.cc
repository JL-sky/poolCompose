#include "semaphore.h"
Semaphore::Semaphore(int resLimit) :_resLimit(resLimit) {}
void Semaphore::wait()
{
    std::unique_lock<std::mutex> lock(_mtx);
    _condMtx.wait(lock,
        [this]() {
            return _resLimit > 0;
        });

    _resLimit--;
}

void Semaphore::post()
{
    std::unique_lock<std::mutex> lock(_mtx);
    _resLimit++;
    _condMtx.notify_all();
}