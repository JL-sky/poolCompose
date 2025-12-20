#include "threadPool.h"
#include <gtest/gtest.h>

#if 1
class ThreadPoolTest : public ::testing::Test {
 public:
    void SetUp() override {
        std::cout << ">>>> init threadpool...." << std::endl;
        pool_ = std::make_unique<ThreadPool>(4); // 假设线程池初始化为4个线程
    }
    void TearDown() override {
        std::cout << ">>>> shutdown threadPool...." << std::endl;
        pool_->ShutDown();
    }
    std::unique_ptr<ThreadPool> pool_;
};

// 矩阵乘法
std::vector<std::vector<int>>
MatrixMultiply(const std::vector<std::vector<int>> &A,
               const std::vector<std::vector<int>> &B, ThreadPool &pool) {
    size_t m = A.size();    // A的行数
    size_t n = A[0].size(); // A的列数
    size_t p = B[0].size(); // B的列数

    std::vector<std::vector<int>> C(m, std::vector<int>(p, 0)); // 结果矩阵

    std::vector<std::future<void>> futures;

    // 为结果矩阵中的每个元素提交一个计算任务
    for (size_t i = 0; i < m; ++i) {
        for (size_t j = 0; j < p; ++j) {
            futures.push_back(pool.Submit([i, j, &A, &B, &C, n]() {
                int sum = 0;
                for (size_t k = 0; k < n; ++k) {
                    sum += A[i][k] * B[k][j];
                }
                C[i][j] = sum;
            }));
        }
    }

    // 等待所有的任务完成
    for (auto &future : futures) {
        future.get();
    }

    return C;
}
// 验证矩阵乘法结果是否正确
TEST_F(ThreadPoolTest, TestMatrixMultiplication) {
    // 定义两个矩阵 A 和 B
    std::vector<std::vector<int>> A = {{1, 2, 3}, {4, 5, 6}};
    std::vector<std::vector<int>> B = {{7, 8}, {9, 10}, {11, 12}};

    // 计算矩阵 A 和 B 的乘积
    std::vector<std::vector<int>> C =
        MatrixMultiply(A, B, *pool_); // 使用 *pool_ 来解引用智能指针

    // 验证结果矩阵 C
    std::vector<std::vector<int>> expected = {{58, 64}, {139, 154}};
    EXPECT_EQ(C, expected);
}

int add(const int &num1, const int &num2) { return num1 + num2; }
TEST_F(ThreadPoolTest, TestThreadPoolSubmit) {
    // 提交加法任务
    auto task1 = pool_->Submit(add, 1, 2); // 1 + 2
    auto task2 = pool_->Submit(add, 3, 4); // 3 + 4

    // 获取任务的结果
    int sum = task1.get() + task2.get(); // 获取两个任务的结果

    // 验证最终结果
    EXPECT_EQ(sum, 10);
}
#endif

#if 1
int main(int argc, char **argv) {
    // testing::AddGlobalTestEnvironment(new FooEnvironment);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif