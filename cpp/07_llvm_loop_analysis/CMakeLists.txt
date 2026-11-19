cmake_minimum_required(VERSION 3.20)
project(llvm_loop_analysis LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVM_DIR: ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

separate_arguments(
  LLVM_DEFINITIONS_LIST NATIVE_COMMAND "${LLVM_DEFINITIONS}"
)
add_definitions(${LLVM_DEFINITIONS_LIST})

add_llvm_pass_plugin(LoopReport
  LoopReport.cpp
  Plugin.cpp
)

target_include_directories(
  LoopReport SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS}
)

set_target_properties(LoopReport PROPERTIES
  PREFIX ""
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(
    LoopReport PRIVATE -Wall -Wextra -Wpedantic
  )
  if(NOT LLVM_ENABLE_RTTI)
    target_compile_options(
      LoopReport PRIVATE -fno-rtti
    )
  endif()
endif()