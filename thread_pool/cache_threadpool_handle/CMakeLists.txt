cmake_minimum_required(VERSION 3.5)
project(ThreadPoolExample)

# 设置 C++ 版本和编译选项
# set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-std=c++17 -g")

# 设置源文件路径
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(LINK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# 设置全局的编译路径
# 设置头文件寻找路劲
include_directories(${INCLUDE_DIR})
# 设置链接文件寻找路径
link_directories(${LINK_DIR})

# 生成动态库
add_library(threadpool SHARED
    ${SRC_DIR}/semaphore.cc
    ${SRC_DIR}/threadpool.cc
)
# 设置动态库的输出路径
set_target_properties(threadpool PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${LINK_DIR}
)

# 编译生成测试代码
add_executable(main
    ${SRC_DIR}/main.cc
)
# 设置可执行文件的输出路径
set_target_properties(main PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/example
)

# 链接动态库和线程库
target_link_libraries(main
    threadpool
    pthread
)