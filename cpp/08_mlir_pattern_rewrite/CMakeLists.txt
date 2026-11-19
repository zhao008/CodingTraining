cmake_minimum_required(VERSION 3.20)
project(mlir_pattern_rewrite LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(MLIR REQUIRED CONFIG)

message(STATUS "LLVM_DIR: ${LLVM_DIR}")
message(STATUS "MLIR_DIR: ${MLIR_DIR}")

list(APPEND CMAKE_MODULE_PATH
    "${LLVM_CMAKE_DIR}"
    "${MLIR_CMAKE_DIR}"
)

include(AddLLVM)
include(AddMLIR)
include(HandleLLVMOptions)

set(LLVM_RUNTIME_OUTPUT_INTDIR
    "${CMAKE_BINARY_DIR}/bin")

include_directories(SYSTEM
    ${LLVM_INCLUDE_DIRS}
    ${MLIR_INCLUDE_DIRS}
)

separate_arguments(
    LLVM_DEFINITIONS_LIST
    NATIVE_COMMAND
    "${LLVM_DEFINITIONS}"
)
add_definitions(${LLVM_DEFINITIONS_LIST})

add_llvm_executable(training-opt
    training-opt.cpp
    MulOne.cpp
)

llvm_update_compile_flags(training-opt)

target_link_libraries(training-opt PRIVATE
    MLIROptLib
    MLIRArithDialect
    MLIRFuncDialect
    MLIRTransformUtils
    MLIRRewrite
    MLIRPass
    MLIRIR
    MLIRSupport
)
