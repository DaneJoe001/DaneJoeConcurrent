# DaneJoe Concurrent

一个轻量的 C++17 并发容器与组件集合。目前提供：
- 阻塞队列：`DaneJoe::Concurrent::Blocking::MpmcBoundedQueue<T>`（多生产者/多消费者，有界，阻塞/超时/非阻塞出队，支持批量入队/出队、关闭唤醒、动态扩容）
- 预留无锁容器位：`DaneJoe::Concurrent::LockFree::SpscRingQueue<T>`（占位，接口待完善）

## 特性
- **Blocking.MPMC 有界队列**：`std::mutex` + `std::condition_variable` 保证并发正确性
- **容量管理**：构造指定容量，满时 `push` 阻塞；支持 `set_max_size` 动态扩容
- **出队模式**：
  - `pop()` 阻塞直到有数据或队列关闭
  - `pop_for(...)` / `pop_until(...)` 超时等待
  - `try_pop()` 非阻塞
- **批量操作**：
  - `push(begin, end)` 容量不足时分批阻塞入队
  - `pop(int n)` 打包出队
- **关闭语义**：`close()` 唤醒等待中的 `push/pop` 并拒绝后续 `push`
- **现代 CMake**：导出别名目标 `danejoe::concurrent`，支持安装后 `find_package(DaneJoeConcurrent)`

## 目录结构
```text
include/
  concurrent/
    blocking/mpmc_bounded_queue.hpp   # 阻塞 MPMC 有界队列
    lock_free/spsc_ring_queue.hpp     # 预留：无锁 SPSC 环形队列（占位）
  version/concurrent_version.h.in
source/
  concurrent/blocking/mpmc_bounded_queue.cpp  # 静态库占位实现
  test/main.cpp                               # 示例/测试
cmake/
  danejoe_install_export.cmake
  DaneJoeConcurrentConfig.cmake.in
CMakeLists.txt
README.md
```

## 快速开始
### 方式一：仅头文件引入
将 `include` 目录加入编译器包含路径：
```cpp
#include "concurrent/blocking/mpmc_bounded_queue.hpp"
#include <iostream>

int main() {
    DaneJoe::Concurrent::Blocking::MpmcBoundedQueue<int> q(2);
    q.push(1);
    q.push(2);

    auto v1 = q.try_pop();
    auto v2 = q.pop();
    std::cout << v1.value_or(-1) << "," << v2.value_or(-1) << "\n";
}
```

### 方式二：通过 CMake 构建并链接静态库（子目录）
```bash
cmake -S . -B build/gcc-debug -D CMAKE_BUILD_TYPE=Debug
cmake --build build/gcc-debug -j
```
上层工程：
```cmake
add_subdirectory(path/to/library_danejoe_concurrent)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE danejoe::concurrent)
```

### 方式三：FetchContent 引入（无需安装）
```cmake
include(FetchContent)

FetchContent_Declare(DaneJoeConcurrent
  GIT_REPOSITORY https://github.com/DaneJoe001/DaneJoeConcurrent.git
  GIT_TAG        v1.0.0
)
FetchContent_MakeAvailable(DaneJoeConcurrent)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE danejoe::concurrent)
```

### 方式四：安装并通过 find_package 使用
在库根目录安装：
```bash
cmake -S . -B build -D CMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --install build
```
消费者工程：
```cmake
find_package(DaneJoeConcurrent CONFIG REQUIRED)
add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE danejoe::concurrent)
```

> 提示：导出/安装逻辑仅在本项目作为顶层工程时启用；作为子项目或通过 FetchContent 引入时不会执行安装步骤。

## API 概览（Blocking.MPMC）
头文件：`concurrent/blocking/mpmc_bounded_queue.hpp`

- 类型
  - `template<class T> class DaneJoe::Concurrent::Blocking::MpmcBoundedQueue`

- 构造
  - `MpmcBoundedQueue(int max_size = 50)`：设置容量（默认 50）

- 入队
  - `bool push(T item)`：队列已满则阻塞直到可用；关闭后返回 `false`
  - `template<typename It> bool push(It begin, It end)`：批量入队，必要时分批阻塞等待；关闭后返回 `false`

- 出队
  - `std::optional<T> pop()`：阻塞直到有元素或队列关闭；关闭且为空返回 `nullopt`
  - `std::optional<std::vector<T>> pop(int n)`：阻塞直到取满 `n` 个或队列关闭；若中途关闭且无元素可取，返回 `nullopt`
  - `std::optional<T> try_pop()`：非阻塞；空返回 `nullopt`
  - `std::vector<T> try_pop(std::size_t n)`：非阻塞批量；尽可能多地返回，可能小于 `n`
  - `template<class Period> std::optional<T> pop_for(Period)` / `pop_until(Period)`：超时等待

- 查询
  - `bool empty() const`，`bool full() const`，`std::size_t size() const`
  - `bool is_running() const`
  - `std::optional<T> front() const`：阻塞直到可读但不弹出（需 `T` 可拷贝）

- 运行控制
  - `void close()`：关闭队列，唤醒等待中的 `push/pop`
  - `void set_max_size(std::size_t)` / `std::size_t get_max_size() const`
  - 支持移动构造/赋值；禁用拷贝

## 示例
### 多生产者/多消费者
```cpp
#include "concurrent/blocking/mpmc_bounded_queue.hpp"
#include <thread>
#include <vector>
using DaneJoe::Concurrent::Blocking::MpmcBoundedQueue;

int main() {
    MpmcBoundedQueue<int> q(100);

    std::vector<std::thread> producers(3);
    for (int p = 0; p < 3; ++p) {
        producers[p] = std::thread([&, p]{
            for (int i = 0; i < 100; ++i) q.push(p * 100 + i);
        });
    }

    std::vector<std::thread> consumers(2);
    for (auto& t : consumers) {
        t = std::thread([&]{
            while (true) {
                auto v = q.pop_for(std::chrono::milliseconds(5));
                if (!v && !q.is_running()) break;
            }
        });
    }

    for (auto& t : producers) t.join();
    q.close();
    for (auto& t : consumers) t.join();
}
```

### 批量入队
```cpp
DaneJoe::Concurrent::Blocking::MpmcBoundedQueue<int> q(4);
std::vector<int> v{1,2,3,4,5};
q.push(v.begin(), v.end()); // 满则等待，消费者弹出后继续入队
```

## 版本宏
构建或安装后可包含：
```cpp
#include <version/concurrent_version.h>
// CONCURRENT_VERSION_MAJOR / _MINOR / _PATCH / _STR
```

## 兼容性
- 要求 C++17 及以上（以目标特性声明最低标准为 C++17）
- 支持 MSVC 与 GCC/Clang

## 构建与测试
- 开启测试（顶层工程时）：`-D DANEJOE_concurrent_BUILD_TESTS=ON`
- 运行：
```bash
cmake -S . -B build -D DANEJOE_concurrent_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build
```

## 注意事项
- `front()` 为读取但不弹出，需要 `T` 可拷贝
- 关闭后：所有等待中的 `push` 返回 `false`；`pop` 在队列被耗尽时返回 `nullopt`
- 批量入队在容量不足时会阻塞，直到有空间或队列被关闭

## 许可证
暂未设置

## 致谢
- 灵感来自常见的生产者-消费者并发模型
- 欢迎 Issue/PR 反馈与改进
