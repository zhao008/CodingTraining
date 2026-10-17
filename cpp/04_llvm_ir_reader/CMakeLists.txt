cmake_minimum_required(VERSION 3.20)
project(llvm_ir_reader LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND "${LLVM_DEFINITIONS}")
add_definitions(${LLVM_DEFINITIONS_LIST})

add_executable(llvm_ir_reader main.cpp)

target_include_directories(llvm_ir_reader SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(llvm_ir_reader PRIVATE -Wall -Wextra -Wpedantic)
  if(NOT LLVM_ENABLE_RTTI)
    target_compile_options(llvm_ir_reader PRIVATE -fno-rtti)
  endif()
endif()

llvm_map_components_to_libnames(llvm_libs Core IRReader Support)
target_link_libraries(llvm_ir_reader PRIVATE ${llvm_libs})