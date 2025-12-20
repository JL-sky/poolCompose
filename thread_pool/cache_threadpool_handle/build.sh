# 编译生成动态库
g++ -fPIC -shared -I ./include/  ./src/semaphore.cc ./src/threadpool.cc  -std=c++17  -o ./lib/libthreadpool.so
# 将动态库移动到系统库目录下
cp ./lib/libthreadpool.so /usr/local/lib
# 将头文件放到系统include目录下
cp ./include/threadpool.h /usr/local/include
# 编译生成测试代码
g++ -I ./include/ ./src/main.cc -std=c++17 -lthreadpool -lpthread -g -o ./example/main
# 更新动态链接库配置
echo '/usr/local/lib' > /etc/ld.so.conf.d/mylib.conf
# 刷新动态链接库的配置使其生效
ldconfig