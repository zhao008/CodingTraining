# 08 · MLIR Pattern Rewrite：消除整数乘以 1

## 1. 本轮核心目标

实现一个最小的 MLIR Pattern Rewrite Pass，将以下两种标量 i32 运算：

```text
x * 1
1 * x
```

替换为：

```text
x
```

本轮只处理：

- `arith.muli`
- 标量、signless `i32`
- 至少一侧是直接由 `arith.constant` 定义的整数常量 1

其他情况保持不变：

- 非 1 常量。
- i64、index、vector、tensor。
- 浮点乘法。
- 无法直接识别为常量 1 的操作数。

本轮不自定义 dialect，不做 lowering，不接 GPU，不实现完整编译器。

## 2. 为什么值得做

前面几轮的关系：

| 实验 | 核心能力 |
| --- | --- |
| 06 | 直接修改 LLVM 指令，替换用途并安全删除 |
| 07 | 使用 LLVM 已有分析，读取循环结构 |
| 08 | 使用 MLIR 的匹配与重写机制表达局部变换 |

本轮将通过具体代码理解：

- operation：一条结构化 IR 操作。
- dialect：一组相关操作、类型与规则。
- SSA value：操作结果或 block argument。
- rewrite pattern：匹配条件与替换规则。
- PatternRewriter：执行 IR 修改的接口。

这些是后续阅读 MLIR/Triton rewrite 和 lowering 的基础。

MLIR 已经具有类似的乘以 1 折叠能力。本轮不是提出新优化，而是学习如何实现、注册和测试自己的规则。

## 3. 时间安排

前提：本地已经有可用的 LLVM/MLIR C++ 开发环境。

| 时间 | 工作 |
| --- | --- |
| 0–10 分钟 | 确认环境，阅读必要接口 |
| 10–40 分钟 | 打通工具框架，实现匹配与替换 |
| 40–55 分钟 | 测试、Debug、检查变换结果 |
| 55–60 分钟 | notes 与 commit |
| 60–90 分钟，可选 | 增加嵌套 region 测试与失败路径检查 |

只有 30 分钟时：

- 5 分钟理解输入与预期。
- 20 分钟实现右侧乘以 1。
- 5 分钟验证一个正例和一个反例，提交。

如果缺少 MLIR 开发组件：

- 本次先以环境检查、准备 SDK、编译空工具为最小闭环。
- 不要求同时完成全部重写测试。
- 不把完整 LLVM/MLIR 源码构建时间算作本轮编码时间。
- 不为了练习盲目重编整套工具链。

## 4. 最少必要阅读

源码版本应与你实际使用的 LLVM/MLIR 开发包匹配。

下面链接以 release/23.x 为例，不要求升级已有环境。

### 4.1 Pattern Rewrite 的规则

文档：

https://mlir.llvm.org/docs/PatternRewriter/

只读：

- `matchAndRewrite` implementation
- Restrictions
- Walk Pattern Rewrite Driver

先记住：

1. 确认匹配成功后，才能修改 IR。
2. 修改必须通过传入的 PatternRewriter。
3. 没有修改 IR 时返回 failure。
4. 成功替换后返回 success。
5. 不直接套用 06 的 RAUW + erase 写法。

### 4.2 精确源码位置

仓库：

https://github.com/llvm/llvm-project

| 文件 | 只看这些接口 |
| --- | --- |
| `mlir/include/mlir/IR/PatternMatch.h` | `OpRewritePattern`、`PatternRewriter` |
| `mlir/include/mlir/IR/Value.h` | `Value::getDefiningOp` |
| `mlir/include/mlir/IR/BuiltinAttributes.h` | `IntegerAttr` |
| `mlir/include/mlir/Transforms/WalkPatternRewriteDriver.h` | `walkAndApplyPatterns` |
| `mlir/include/mlir/Pass/Pass.h` | `PassWrapper`、`OperationPass` |
| `mlir/include/mlir/Pass/PassRegistry.h` | `PassRegistration` |

作为程序入口参考，只看：

`mlir/examples/standalone/standalone-opt/standalone-opt.cpp`

https://github.com/llvm/llvm-project/blob/release/23.x/mlir/examples/standalone/standalone-opt/standalone-opt.cpp

不通读 MLIR，不阅读所有 dialect，不学习 TableGen。

## 5. 一个重要的测试陷阱

不要直接使用：

```cpp
applyPatternsGreedily(...)
```

并保留其默认 folding 行为来验证本实验。

原因：

- `arith.muli` 已有乘以 1 的内置 folding。
- 即使自己的 pattern 没写对，其他折叠机制也可能把乘法消除。
- 这样“测试通过”不代表自己的代码执行正确。
- folding 还可能修改本轮要求保持不变的 i64 等反例。

本轮明确使用：

```cpp
mlir::walkAndApplyPatterns(...)
```

这个 driver：

- 应用你提供的 pattern。
- 不执行内置 folding。
- 不执行死代码删除。
- 不反复迭代到不动点。
- 不访问本轮重写中新建或替换出的 operation。

本轮规则不创建新的乘法，一次遍历就足够。

不要额外开启：

- canonicalize
- CSE
- 其他算术简化 Pass

未使用的常量 1 可以留在输出中，不需要额外删除。

## 6. 目录与交付文件

新建：

```bash
mkdir -p cpp/08_mlir_pattern_rewrite/tests
```

保留 06、07 不动。

| 文件 | 作用 |
| --- | --- |
| README.md | 本说明 |
| CMakeLists.txt | 构建 training-opt |
| training-opt.cpp | 注册 dialect 与自己的 Pass，提供 CLI |
| MulOne.h | 声明 Pass 注册函数 |
| MulOne.cpp | Pass 框架与重写规则 |
| tests/basic.mlir | 正例 |
| tests/negative.mlir | 反例 |
| run_tests.sh | 自动构建与检查 |
| notes.md | 运行证据与复盘 |

不需要：

- Plugin.cpp
- 自定义 dialect
- `.td` 文件
- Python binding
- GPU

不要提交 build/、SDK 或生成的二进制。

## 7. 环境检查

06、07 能编译，不代表已有 MLIR 开发环境。

至少需要：

- LLVM CMake 配置。
- MLIR CMake 配置。
- MLIR 头文件及生成头文件。
- MLIR 链接库。
- 与该 SDK 兼容的 C++ 编译器。

只有 `mlir-opt` 可执行文件，不一定足够编译 C++ Pass。

以下按 Linux、同一安装前缀提供 LLVM 和 MLIR 的布局编写。

在仓库根目录设置：

```bash
REPO_DIR="$PWD"
PROJECT_DIR="$REPO_DIR/cpp/08_mlir_pattern_rewrite"

# 改成实际包含 LLVM + MLIR 开发组件的安装前缀。
MLIR_PREFIX="$REPO_DIR/tool/llvm/LLVM-23.1.3-Linux-X64"

LLVM_DIR="$MLIR_PREFIX/lib/cmake/llvm"
MLIR_DIR="$MLIR_PREFIX/lib/cmake/mlir"
FILECHECK="$REPO_DIR/tool/filecheck/FileCheck"

test -f "$LLVM_DIR/LLVMConfig.cmake"
test -f "$MLIR_DIR/MLIRConfig.cmake"
test -f "$MLIR_PREFIX/include/mlir/IR/PatternMatch.h"
```

若这些检查失败，先解决 SDK 缺失。

- 不把两个不匹配的 LLVM/MLIR 安装混用。
- 不在不了解缺失内容时随意复制头文件或链接库。
- 下面的 API 和构建框架按 release/23.x 核对；使用其他版本时，以对应版本源码为准。

## 8. 工具框架

框架可以直接使用，核心 pattern 由你实现。

### 8.1 MulOne.h

```cpp
#pragma once

namespace training {

void registerMulOnePass();

} // namespace training
```

### 8.2 training-opt.cpp

```cpp
#include "MulOne.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"

int main(int argc, char** argv) {
    training::registerMulOnePass();

    mlir::DialectRegistry registry;
    registry.insert<
        mlir::arith::ArithDialect,
        mlir::func::FuncDialect>();

    return mlir::asMainReturnCode(
        mlir::MlirOptMain(
            argc,
            argv,
            "CodingTraining MLIR optimizer\n",
            registry));
}
```

理解两个不同动作：

- 注册 dialect：让工具能够解析这些操作。
- 注册 Pass：让命令行能够运行自己的变换。

本轮只注册 arith、func，不注册所有 dialect 和所有 Pass。

### 8.3 MulOne.cpp 骨架

```cpp
#include "MulOne.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassRegistry.h"
#include "mlir/Transforms/WalkPatternRewriteDriver.h"

namespace {

struct MulOnePattern
    : public mlir::OpRewritePattern<mlir::arith::MulIOp> {
    using mlir::OpRewritePattern<
        mlir::arith::MulIOp>::OpRewritePattern;

    mlir::LogicalResult matchAndRewrite(
        mlir::arith::MulIOp op,
        mlir::PatternRewriter& rewriter) const override {

        // TODO 1：确认结果类型是标量 signless i32。
        //
        // TODO 2：检查左操作数是否为直接定义的整数常量 1。
        //
        // TODO 3：检查右操作数是否为直接定义的整数常量 1。
        //
        // TODO 4：匹配时通过 rewriter 替换整个乘法，
        //         然后返回 success。
        //
        // 未匹配时不修改任何 IR，返回 failure。

        return mlir::failure();
    }
};

struct MulOnePass
    : public mlir::PassWrapper<
          MulOnePass,
          mlir::OperationPass<mlir::func::FuncOp>> {

    MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(MulOnePass)

    llvm::StringRef getArgument() const override {
        return "training-mul-one";
    }

    llvm::StringRef getDescription() const override {
        return "Remove scalar i32 multiplication by one";
    }

    void runOnOperation() override {
        mlir::RewritePatternSet patterns(&getContext());
        patterns.add<MulOnePattern>(&getContext());

        mlir::FrozenRewritePatternSet frozen(
            std::move(patterns));

        mlir::walkAndApplyPatterns(
            getOperation().getOperation(), frozen);
    }
};

} // namespace

namespace training {

void registerMulOnePass() {
    mlir::PassRegistration<MulOnePass>();
}

} // namespace training
```

如果使用 `std::move`，在源文件中显式添加：

```cpp
#include <utility>
```

骨架中的 pattern 默认返回 failure，因此可以先编译出一个不修改 IR 的工具。

这时正例测试失败是正常现象：接下来才实现自己的规则。

## 9. 你要完成的核心逻辑

### 9.1 类型检查

只接受：

```text
i32
```

不接受：

```text
i64
index
vector<2xi32>
tensor<2xi32>
```

建议：

- 将结果类型转换为 `mlir::IntegerType`。
- 检查 `isSignless()`。
- 检查 `getWidth() == 32`。

先检查类型，再检查常量。

### 9.2 常量检查

建议写一个小函数：

```cpp
bool isConstantOne(mlir::Value value);
```

实现思路：

1. 使用 `getDefiningOp<mlir::arith::ConstantOp>()`。
2. 如果没有对应定义操作，返回 false。
3. 将常量的 attribute 检查为 `mlir::IntegerAttr`。
4. 检查其 APInt 值是否为 1。

可查的接口：

- `ConstantOp::getValue()`
- `mlir::dyn_cast<mlir::IntegerAttr>()`
- `IntegerAttr::getValue()`
- `APInt::isOne()`

不能通过：

- SSA 名字包含 one。
- 打印文本包含 1。
- 正则表达式。
- 假设常量总在右侧。

来判断常量。

### 9.3 替换

判断：

- 左边是 1：保留右边。
- 否则右边是 1：保留左边。
- 否则不匹配。

使用：

```cpp
rewriter.replaceOp(op, replacement);
```

其中 replacement 是保留的 `mlir::Value`。

注意：

- replaceOp 已经负责替换结果用途并移除原操作。
- 不要再手动 erase。
- 替换后不要继续访问被删除的 op。
- 匹配失败时不能已经修改 IR。
- 两边都是 1 时，只选择一个替换分支即可。

本轮不要增加乘以 0、加 0 等额外规则。

## 10. CMakeLists.txt

以下是最小外部工具构建框架：

```cmake
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
```

说明：

- 这里构建可执行程序，不使用 `add_llvm_pass_plugin`。
- MLIRTransformUtils 提供本轮使用的 walk rewrite driver。
- 使用 SDK 导出的 CMake target，不手写一长串 `.a` 文件路径。
- 不使用 `registerAllPasses()`，因此也不需要为所有 Pass 添加链接依赖。

## 11. 正例测试：tests/basic.mlir

FileCheck 中不要依赖输入 SSA 名字原样保留，使用捕获变量。

```mlir
// CHECK-LABEL: func.func @right_one(
// CHECK-SAME: %[[R:[a-zA-Z0-9_]+]]: i32
// CHECK-NOT: arith.muli
// CHECK: return %[[R]] : i32
// CHECK-NEXT: }
func.func @right_one(%x: i32) -> i32 {
  %one = arith.constant 1 : i32
  %result = arith.muli %x, %one : i32
  return %result : i32
}

// CHECK-LABEL: func.func @left_one(
// CHECK-SAME: %[[L:[a-zA-Z0-9_]+]]: i32
// CHECK-NOT: arith.muli
// CHECK: return %[[L]] : i32
// CHECK-NEXT: }
func.func @left_one(%x: i32) -> i32 {
  %one = arith.constant 1 : i32
  %result = arith.muli %one, %x : i32
  return %result : i32
}

// CHECK-LABEL: func.func @chain(
// CHECK-SAME: %[[C:[a-zA-Z0-9_]+]]: i32
// CHECK-NOT: arith.muli
// CHECK: return %[[C]] : i32
// CHECK-NEXT: }
func.func @chain(%x: i32) -> i32 {
  %one = arith.constant 1 : i32
  %a = arith.muli %x, %one : i32
  %b = arith.muli %one, %a : i32
  %c = arith.muli %b, %one : i32
  return %c : i32
}

// CHECK-LABEL: func.func @multiple_uses(
// CHECK-SAME: %[[M:[a-zA-Z0-9_]+]]: i32
// CHECK-NOT: arith.muli
// CHECK: %[[SUM:[a-zA-Z0-9_]+]] = arith.addi %[[M]], %[[M]] : i32
// CHECK-NEXT: return %[[SUM]] : i32
// CHECK-NEXT: }
func.func @multiple_uses(%x: i32) -> i32 {
  %one = arith.constant 1 : i32
  %a = arith.muli %x, %one : i32
  %sum = arith.addi %a, %a : i32
  return %sum : i32
}
```

这些测试分别检查：

- 右侧常量。
- 左侧常量。
- 连续链。
- 一个结果的多个使用位置。

不要求删除变换后无用的常量。

## 12. 反例测试：tests/negative.mlir

```mlir
// CHECK-LABEL: func.func @keep_two(
// CHECK: %[[T:[a-zA-Z0-9_]+]] = arith.muli
// CHECK-NEXT: return %[[T]] : i32
func.func @keep_two(%x: i32) -> i32 {
  %two = arith.constant 2 : i32
  %result = arith.muli %x, %two : i32
  return %result : i32
}

// CHECK-LABEL: func.func @keep_i64(
// CHECK: %[[W:[a-zA-Z0-9_]+]] = arith.muli
// CHECK-NEXT: return %[[W]] : i64
func.func @keep_i64(%x: i64) -> i64 {
  %one = arith.constant 1 : i64
  %result = arith.muli %x, %one : i64
  return %result : i64
}

// CHECK-LABEL: func.func @keep_vector(
// CHECK: %[[V:[a-zA-Z0-9_]+]] = arith.muli
// CHECK-NEXT: return %[[V]] : vector<2xi32>
func.func @keep_vector(
    %x: vector<2xi32>) -> vector<2xi32> {
  %one = arith.constant dense<1> : vector<2xi32>
  %result = arith.muli %x, %one : vector<2xi32>
  return %result : vector<2xi32>
}

// CHECK-LABEL: func.func @keep_float(
// CHECK: %[[F:[a-zA-Z0-9_]+]] = arith.mulf
// CHECK-NEXT: return %[[F]] : f32
func.func @keep_float(%x: f32) -> f32 {
  %one = arith.constant 1.0 : f32
  %result = arith.mulf %x, %one : f32
  return %result : f32
}
```

本轮不是判断这些情况能不能优化，而是验证自己的规则没有越过约定范围。

## 13. 构建与运行

沿用前面设置的变量，在仓库根目录执行。

### 13.1 构建

```bash
cmake -S "$PROJECT_DIR" -B "$PROJECT_DIR/build" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DLLVM_DIR="$LLVM_DIR" \
  -DMLIR_DIR="$MLIR_DIR"

cmake --build "$PROJECT_DIR/build" \
  --target training-opt -j
```

如果 SDK 需要额外的 zstd 等依赖，沿用已经验证过的依赖前缀配置，不修改系统库。

### 13.2 先确认 Pass 注册

```bash
"$PROJECT_DIR/build/bin/training-opt" --help
```

检查帮助中包含：

```text
training-mul-one
```

### 13.3 运行自己的 Pass

```bash
"$PROJECT_DIR/build/bin/training-opt" \
  --pass-pipeline='builtin.module(func.func(training-mul-one))' \
  --verify-each \
  "$PROJECT_DIR/tests/basic.mlir" \
  -o "$PROJECT_DIR/build/basic.after.mlir"

"$FILECHECK" "$PROJECT_DIR/tests/basic.mlir" \
  < "$PROJECT_DIR/build/basic.after.mlir"
```

本轮：

- 变换后的 MLIR 写到输出文件。
- stderr 保留给诊断信息。
- 不要沿用 07 “从 stderr 读取循环报告”的检查方式。

### 13.4 运行反例

```bash
"$PROJECT_DIR/build/bin/training-opt" \
  --pass-pipeline='builtin.module(func.func(training-mul-one))' \
  --verify-each \
  "$PROJECT_DIR/tests/negative.mlir" \
  -o "$PROJECT_DIR/build/negative.after.mlir"

"$FILECHECK" "$PROJECT_DIR/tests/negative.mlir" \
  < "$PROJECT_DIR/build/negative.after.mlir"
```

## 14. 验证规则确实来自自己的代码

必须做一次“关闭规则”的对照：

1. 临时让 matchAndRewrite 始终返回 failure。
2. 重新编译。
3. 运行正例测试。
4. 确认正例失败，乘法仍在。
5. 恢复自己的实现。
6. 重新编译，确认正例通过。

这一步用于排除：

- 忘记编译新代码。
- 实际运行了旧二进制。
- 使用其他优化替自己完成变换。
- FileCheck 断言没有检查到目标行为。

临时关闭规则的修改不要提交。

不要通过开启 canonicalize 来让正例通过。

## 15. 自动测试脚本

复用 07 的脚本组织方式，但调整检查对象。

run_tests.sh 应完成：

1. 从脚本位置确定项目目录，不依赖当前工作目录。
2. 检查 MLIR_DIR、LLVM_DIR、FileCheck。
3. 配置与构建 training-opt。
4. 遍历 tests/*.mlir。
5. 每个输入生成独立的 build/<name>.after.mlir。
6. 运行 verifier 和 FileCheck。
7. 任一命令失败时返回非零。
8. 保留失败输出，方便定位。

建议增加第二次运行：

- 将第一次输出作为第二次输入。
- 生成 `<name>.again.mlir`。
- 两次输出都通过同一组 FileCheck。

如果要直接 diff 两次结果，注意默认打印的 `module` 位置可能来自不同输入文件。

可为两次运行加入：

```text
--mlir-print-debuginfo=false
```

再比较文本；若实际版本仍输出不同的源位置，只比较经过一致处理的文本，不把路径变化误判为 IR 语义变化。

基础验收不强制精确文本幂等性检查；优先确保变换和反例正确。

## 16. Debug 方法

### 16.1 查看 Pass 前后 IR

```bash
"$PROJECT_DIR/build/bin/training-opt" \
  --pass-pipeline='builtin.module(func.func(training-mul-one))' \
  --mlir-print-ir-before-all \
  --mlir-print-ir-after-all \
  --verify-each \
  "$PROJECT_DIR/tests/basic.mlir" \
  -o "$PROJECT_DIR/build/basic.debug.after.mlir" \
  2> "$PROJECT_DIR/build/basic.debug.log"
```

### 16.2 matcher 内部观察

必要时临时打印：

- 当前操作类型。
- 结果类型。
- 哪一侧匹配到 ConstantOp。
- 常量 attribute 的类型和值。
- 最终选择的 replacement。

不要默认每条指令都打印大量日志。

### 16.3 常见错误

| 现象 | 优先检查 |
| --- | --- |
| 找不到 MLIRConfig.cmake | SDK 是否真的包含 MLIR 开发组件 |
| unknown operation | arith、func dialect 是否注册 |
| 找不到 training-mul-one | Pass 是否注册、管线名称是否一致 |
| 右侧可以，左侧不行 | 是否只检查了 RHS |
| i64 也被消除 | 类型限制或 driver 是否正确 |
| matcher 没写，测试却通过 | 是否有内置 folding 或其他优化 |
| multiple_uses 失败 | 是否替换了整个 operation 的结果 |
| 替换后崩溃 | 是否再次访问或删除了原 op |
| 无用常量仍在 | 正常，本轮不做 DCE |

## 17. 90 分钟扩展

只在基础版全部通过后进行。

增加两个测试，不增加新优化规则：

### 扩展 A：动态操作数反例

输入：

```mlir
func.func @dynamic_operand(
    %x: i32, %y: i32) -> i32 {
  %result = arith.muli %x, %y : i32
  return %result : i32
}
```

预期：乘法保留。

检查 block argument 没有 defining operation 时，不会崩溃。

### 扩展 B：1 * 1

输入两个直接常量 1：

```mlir
func.func @both_one() -> i32 {
  %one = arith.constant 1 : i32
  %result = arith.muli %one, %one : i32
  return %result : i32
}
```

预期：

- 乘法消除。
- 返回一个常量 1 的结果。
- 不重复删除，不访问已删除的 op。

嵌套 region 的遍历可以作为后续练习，本轮不强制引入 scf dialect。

## 18. 完成标准

基础版：

- [ ] 新目录独立，06、07 不变。
- [ ] training-opt 可以编译运行。
- [ ] 只注册需要的 dialect 和自己的 Pass。
- [ ] 正确消除 i32 的左右乘以 1。
- [ ] 连续链和多个用途正确。
- [ ] 非 1、i64、vector、float 测试保持不变。
- [ ] 使用 PatternRewriter 修改 IR。
- [ ] 匹配失败时不修改 IR。
- [ ] 使用 walk driver，不依赖 folding、canonicalize 或 CSE。
- [ ] 关闭自己的规则时，正例测试确实失败。
- [ ] verifier、FileCheck 通过。
- [ ] 自动测试失败时返回非零。
- [ ] 保存变换前后 IR 和短 notes。
- [ ] 不提交 build/ 或 SDK。

基础版完成即算本轮完成。

## 19. 建议 commit message

```text
feat(mlir): add scalar i32 multiply-by-one rewrite pass
```

提交前检查：

```bash
git status
git diff
```

只提交本轮相关内容，不提交临时调试修改。

## 20. notes 与三个复盘问题

notes 只记录：

1. LLVM/MLIR 来源、版本及编译器版本。
2. 一条可复现的构建与运行命令。
3. 一个变换前后 IR 示例。
4. 关闭规则时测试失败、恢复后通过的结果。
5. 一个实际遇到的问题及定位过程。

复盘问题：

1. 为什么这次不能直接用默认带 folding 的 driver 来证明自己的 pattern 正确？
2. 为什么匹配失败前不能修改 IR，修改又必须经过 PatternRewriter？
3. 06 的“RAUW + erase”与本轮 replaceOp，在使用方式和维护职责上有什么区别？

本轮最终交付：

可运行工具 + 一个受限重写规则 + 正反例测试 + 自动测试脚本 + 短 notes。