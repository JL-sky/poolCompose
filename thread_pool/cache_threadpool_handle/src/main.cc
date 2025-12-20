#include"threadpool.h"
#include<chrono>
#include<iostream>
class MyTask :public Task
{
public:
    MyTask() = default;
    MyTask(int begin, int end) :_begin(begin), _end(end) {}
    Any run()
    {
        std::cout << "thread [" << std::this_thread::get_id() << "] begin start..." << std::endl;
        int sum = 0;
        for (int i = _begin; i < _end; i++)
            sum += i;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return sum;
    }
private:
    int _begin;
    int _end;
};

void test()
{
#if 0
    ThreadPool pool;
    pool.start();
    Result res = pool.submit(std::make_shared<MyTask>(1, 10000));
    std::cout<<"sum=" << res.get().cast<int>() << std::endl;
#endif
#if 1
    ThreadPool pool(4);
    pool.setMode(PoolMode::MODE_CACHED);
    pool.start();
    Result res1=pool.submit(std::make_shared<MyTask>(1,10000));
    Result res2=pool.submit(std::make_shared<MyTask>(10001,20000));
    Result res3=pool.submit(std::make_shared<MyTask>(20001,30000));

    pool.submit(std::make_shared<MyTask>(20001, 30000));
    pool.submit(std::make_shared<MyTask>(20001, 30000));
    pool.submit(std::make_shared<MyTask>(20001, 30000));

    int sum1 = res1.get().cast<int>();
    int sum2 = res2.get().cast<int>();
    int sum3 = res3.get().cast<int>();
    std::cout<<"============"<<std::endl;
    std::cout<<"sum=" <<(sum1 + sum2 + sum3)<< std::endl;
    std::cout<<"============"<<std::endl;
#endif
}

int main()
{
    test();
    return 0;
}