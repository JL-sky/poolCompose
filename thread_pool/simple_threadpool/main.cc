#include <iostream>

#include "threadPool.h"

int add(const int& num1, const int& num2) { return num1 + num2; }
void addTest() {
  SimpleThreadPool pool;
  auto task1 = pool.Submit(add, 1, 2);
  auto task2 = pool.Submit(add, 3, 4);
  int sum = task1.get() + task2.get();
  std::cout << "sum=" << sum << std::endl;
}

// 矩阵乘法
std::vector<std::vector<int>> MatrixMultiply(
    const std::vector<std::vector<int>>& A,
    const std::vector<std::vector<int>>& B, SimpleThreadPool& pool) {
  size_t m = A.size();     // A的行数
  size_t n = A[0].size();  // A的列数
  size_t p = B[0].size();  // B的列数

  std::vector<std::vector<int>> C(m, std::vector<int>(p, 0));  // 结果矩阵

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
  for (auto& future : futures) {
    future.get();
  }

  return C;
}

std::ostream& operator<<(std::ostream& os,
                         std::vector<std::vector<int>>& nums) {
  int col = 0;
  int m = nums.size();
  int n = nums[0].size();
  for (int i = 0; i < m; ++i) {
    for (int j = 0; j < n; ++j) {
      os << nums[i][j] << " ";
      col++;
      if (col % n == 0) os << "\n";
    }
  }
  return os;
}

void multiTest() {
  // 定义两个矩阵 A 和 B
  std::vector<std::vector<int>> A = {{1, 2, 3}, {4, 5, 6}};
  std::vector<std::vector<int>> B = {{7, 8}, {9, 10}, {11, 12}};

  SimpleThreadPool pool(A.size() * B[0].size());
  // 计算矩阵 A 和 B 的乘积
  std::vector<std::vector<int>> C =
      MatrixMultiply(A, B, pool);  // 使用 *pool_ 来解引用智能指针

  // std::vector<std::vector<int>> expected = {{58, 64}, {139, 154}};
  std::cout << C;
}

int main() {
  addTest();
  multiTest();
  return 0;
}