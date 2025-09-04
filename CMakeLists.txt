cmake_minimum_required(VERSION 4.0.1)

project(DaneJoeMTQueue LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 添加编码设置
if(MSVC)
    add_compile_options(/utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

set(TEST_MT_QUEUE on)

file(GLOB LIB_SOURCE
    "source/mt_queue/*.cpp"
)

file(GLOB TEST_SOURCE
    "source/test/*.cpp"
)

add_library(DaneJoeMTQueue STATIC ${LIB_SOURCE})

target_include_directories(DaneJoeMTQueue PUBLIC "include")

# 创建测试可执行文件
if(TEST_MT_QUEUE)
    add_executable(DaneJoeMTQueueTest ${TEST_SOURCE})
    # 设置测试的include目录
    target_include_directories(DaneJoeMTQueueTest PRIVATE include)
    target_link_libraries(DaneJoeMTQueueTest PRIVATE DaneJoeMTQueue)
endif()