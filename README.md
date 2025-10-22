# DaneJoe Concurrent

并发组件（阻塞/无锁队列、线程池等）。当前为头文件库（INTERFACE）。

## 构建
```bash
cmake -S . -B build --preset gcc-debug -DBUILD_TESTING=ON
cmake --build build
```

## 运行示例/测试
```bash
ctest --test-dir build -V -R concurrent\.demo
# 或直接运行
./build/library/concurrent/tests/danejoe_concurrent_demo
```

## 作为依赖使用
CMake:
```cmake
find_package(DaneJoeConcurrent CONFIG REQUIRED)
add_executable(app main.cpp)
target_link_libraries(app PRIVATE DaneJoe::Concurrent)
```
