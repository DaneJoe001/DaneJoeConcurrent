cmake_minimum_required(VERSION 3.20)
project(DaneJoeConcurrent VERSION 0.1.0 LANGUAGES CXX)
option(DANEJOE_CONCURRENT_BUILD_TESTS "Build tests for DaneJoeConcurrent" ON)

# Header-only (INTERFACE) for now; expose includes to consumers
add_library(DaneJoeConcurrent INTERFACE)
add_library(DaneJoe::Concurrent ALIAS DaneJoeConcurrent)

target_include_directories(DaneJoeConcurrent
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Consumers require C++20
target_compile_features(DaneJoeConcurrent INTERFACE cxx_std_20)

include(GNUInstallDirs)
install(TARGETS DaneJoeConcurrent
  EXPORT DaneJoeConcurrentTargets
)
install(DIRECTORY "include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

# version header (optional)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/danejoe/version")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/danejoe/version/concurrent_version.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/include/danejoe/version/concurrent_version.h"
  @ONLY
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/include/danejoe/version/concurrent_version.h"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/danejoe/version")

install(EXPORT DaneJoeConcurrentTargets
  NAMESPACE DaneJoe::
  FILE DaneJoeConcurrentTargets.cmake
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/DaneJoeConcurrent"
)

# Config + Version files
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/DaneJoeConcurrentConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

configure_file("cmake/DaneJoeConcurrentConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/DaneJoeConcurrentConfig.cmake" @ONLY)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/DaneJoeConcurrentConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/DaneJoeConcurrentConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/DaneJoeConcurrent"
)

if(DANEJOE_CONCURRENT_BUILD_TESTS)
  include(CTest)
  enable_testing()
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
  endif()
endif()
