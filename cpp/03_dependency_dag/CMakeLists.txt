cmake_minimum_required(VERSION 3.14)
project(dependency_dag CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

option(ENABLE_SANITIZERS "Enable ASan and UBSan" ON)
if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

list(APPEND CMAKE_PREFIX_PATH "$ENV{HOME}/.local")
find_package(GTest REQUIRED)

enable_testing()
include(GoogleTest)

add_executable(dependency_dag_test tests/dependency_dag.cpp)

target_include_directories(dependency_dag_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../02_mini_ir_parser
)

target_link_libraries(dependency_dag_test PRIVATE GTest::gtest_main)

gtest_discover_tests(dependency_dag_test)
