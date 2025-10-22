cmake_minimum_required(VERSION 3.20)

add_executable(danejoe_concurrent_demo
  "${CMAKE_CURRENT_LIST_DIR}/source/main.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/source/demo_concurrent.cpp"
)

# concurrent 目前为 INTERFACE 头文件库，直接链接命名空间目标
 target_link_libraries(danejoe_concurrent_demo PRIVATE DaneJoe::Concurrent)
 target_include_directories(danejoe_concurrent_demo PRIVATE
   "${CMAKE_CURRENT_LIST_DIR}/include"
 )

add_test(NAME concurrent.demo COMMAND danejoe_concurrent_demo)
