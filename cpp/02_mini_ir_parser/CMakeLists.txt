cmake_minimum_required(VERSION 3.14)
project(mini_ir_parser CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

# Debug 下默认打开 AddressSanitizer + UBSan，关掉用 -DENABLE_SANITIZERS=OFF
option(ENABLE_SANITIZERS "Enable ASan and UBSan" ON)
if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# 主程序
add_executable(mini_ir main.cpp)

# ---- Google Test ----
# gtest 1.14.0 安装在 ~/.local 下
list(APPEND CMAKE_PREFIX_PATH "$ENV{HOME}/.local")
find_package(GTest REQUIRED)

enable_testing()

add_executable(lexer_test tests/lexer_test.cpp)
target_include_directories(lexer_test PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(lexer_test PRIVATE GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(lexer_test)
