# LLVM 实验 3：第一个 IR 变换 Pass

工程目录：`cpp/06_llvm_simplify_pass/`。

前置进度：

- 04：读取 LLVM IR，遍历 use-def。
- 05：从 LLVM 指令建立 RAW DAG。

这一轮不继续扩展 DAG。

核心任务：实现一个由 opt 加载的 LLVM Pass，将整数加零替换为原操作数，并安全删除旧指令。

## 1. 核心目标与意义

只实现下面两个规则：

```text
add i32 %x, 0 → %x
add i32 0, %x → %x
```

例如，输入：

```llvm
define i32 @example(i32 %x) {
entry:
  %a = add i32 %x, 0
  %b = mul i32 %a, 2
  ret i32 %b
}
```

输出：

```llvm
define i32 @example(i32 %x) {
entry:
  %b = mul i32 %x, 2
  ret i32 %b
}
```

规则本身很简单，真正要学的是：

1. LLVM Pass 怎样被编译器工具加载和调用。
2. 怎样将某个 Value 的全部使用替换掉。
3. 怎样在遍历时安全删除指令。
4. 怎样验证修改后的 IR。
5. 怎样用自动化测试防止误改和漏改。

这是从“分析程序”进入“修改程序”的第一步，后续可连接 MLIR pattern rewrite 和 Triton compiler。

不要把本轮理解成一个新的优化算法研究。

## 2. 范围与行为约定

### 必须修改

- 标量 i32 的整数 add。
- 左操作数或右操作数是 ConstantInt 零。
- 被消除指令的所有使用都更新。
- 连续的加零指令能够在一次 Pass 执行中消除。

例如：

```llvm
%a = add i32 %x, 0
%b = add i32 %a, 0
ret i32 %b
```

应变为：

```llvm
ret i32 %x
```

### 必须保留

本轮不处理：

- add i64。
- vector add。
- 浮点 fadd。
- 加数不是零的 add。
- sub、mul 等其他 opcode。

不匹配的指令直接跳过，不报错。

注意：这与 05 不同。

05 是限定输入范围的分析器；06 是局部变换 Pass，函数里出现其他合法指令很正常。

### 其他约定

- 遍历函数的全部基本块，不再限制单基本块。
- 不修改分支、基本块或函数签名。
- 不实现通用常量折叠或死代码删除。
- 不直接调用 InstCombine 帮你完成整个任务。
- 不把整数规则直接套到浮点上。
- 输入必须是合法 LLVM IR。
- 测试先使用手写 IR，不加 optnone 属性。

标量 i32 的 add 即使带 nsw/nuw，加零仍可按本规则处理；不要把这推广成“所有代数变换都不必考虑 flags”。

本轮不用跑 GPU benchmark，也不以性能提升幅度作为验收标准。

## 3. 文件安排

| 文件 | 用途 |
|---|---|
| README.md | 实验说明 |
| CMakeLists.txt | 构建动态 Pass 插件 |
| SimplifyAddZero.h | Pass 类声明 |
| SimplifyAddZero.cpp | 你实现的变换逻辑 |
| Plugin.cpp | 插件入口和 Pass 注册 |
| tests/basic.ll | 基础变换与回归测试 |
| tests/types.ll | 不应修改的类型 |
| notes.md | 环境、测试结果和复盘 |

本轮没有 main.cpp。

程序入口由 opt 提供；你的代码作为插件被加载。

名称约定：

| 名称 | 值 |
|---|---|
| CMake target | SimplifyAddZero |
| Pass pipeline 名称 | simplify-add-zero |
| Linux 插件文件 | build/SimplifyAddZero.so |
| Pass 类 | training::SimplifyAddZeroPass |

不链接或修改 05。

## 4. 环境与构建

继续使用 05 已经构建成功的 LLVM 开发环境。

需要：

- LLVM 头文件、库和 CMake 配置。
- llvm-config。
- opt。
- FileCheck。
- C++ 编译器和 CMake。

插件与 opt 必须来自同一套匹配的 LLVM 安装。

下面的接口骨架以你记录的 LLVM 23 为基准。其他版本可能使用不同的 Pass mixin 或插件头文件位置，不要混用网上不同版本的教程。

### 4.1 CMakeLists.txt

构建骨架可以直接使用：

```cmake
cmake_minimum_required(VERSION 3.20)
project(llvm_simplify_pass LANGUAGES C CXX)

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

add_llvm_pass_plugin(SimplifyAddZero
  SimplifyAddZero.cpp
  Plugin.cpp
)

target_include_directories(
  SimplifyAddZero SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS}
)

set_target_properties(SimplifyAddZero PROPERTIES
  PREFIX ""
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(
    SimplifyAddZero PRIVATE -Wall -Wextra -Wpedantic
  )
  if(NOT LLVM_ENABLE_RTTI)
    target_compile_options(
      SimplifyAddZero PRIVATE -fno-rtti
    )
  endif()
endif()
```

使用 LLVM 的 add_llvm_pass_plugin，不要直接照搬 05 的可执行程序链接方式，手工把一组静态 LLVM 库塞进插件。

### 4.2 配置命令

若 llvm-config 已经在 PATH 中，在仓库根目录执行：

```bash
cmake -S cpp/06_llvm_simplify_pass \
  -B cpp/06_llvm_simplify_pass/build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DLLVM_DIR="$(llvm-config --cmakedir)"

cmake --build cpp/06_llvm_simplify_pass/build -j
```

若继续使用 notes 中的本地安装，可以执行：

```bash
llvm_config_bin="$PWD/tool/llvm/LLVM-23.1.3-Linux-X64/bin/llvm-config"

cmake -S cpp/06_llvm_simplify_pass \
  -B cpp/06_llvm_simplify_pass/build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DLLVM_DIR="$("$llvm_config_bin" --cmakedir)" \
  -DCMAKE_PREFIX_PATH="$PWD/tool/llvm/deps/zstd/usr"

cmake --build cpp/06_llvm_simplify_pass/build -j
```

CMAKE_PREFIX_PATH 沿用你在 05 解决的 zstd 依赖配置；不是所有 LLVM 安装都需要它。

后续运行 opt 和 FileCheck 时，也使用这套安装中的工具，必要时写完整路径。

以下命令按 Linux 环境给出。

## 5. Pass 和插件骨架

### 5.1 SimplifyAddZero.h

```cpp
#pragma once

#include "llvm/IR/PassManager.h"

namespace training {

class SimplifyAddZeroPass
    : public llvm::OptionalPassInfoMixin<SimplifyAddZeroPass> {
public:
    llvm::PreservedAnalyses run(
        llvm::Function& function,
        llvm::FunctionAnalysisManager& analysis_manager);
};

} // namespace training
```

### 5.2 SimplifyAddZero.cpp

先写一个不修改 IR 的骨架，确认插件可以运行：

```cpp
#include "SimplifyAddZero.h"

#include "llvm/IR/Function.h"

llvm::PreservedAnalyses training::SimplifyAddZeroPass::run(
    llvm::Function& function,
    llvm::FunctionAnalysisManager& /*analysis_manager*/) {
    (void)function;

    // TODO：
    // 1. 遍历基本块和指令。
    // 2. 识别标量 i32 add。
    // 3. 检查左右操作数中的 ConstantInt 零。
    // 4. 替换所有使用。
    // 5. 安全删除旧指令。
    // 6. 根据是否发生修改返回分析保留信息。

    return llvm::PreservedAnalyses::all();
}
```

这个骨架可以加载，但不会通过变换测试。

不要把“opt 成功退出”当成 Pass 已经完成。

### 5.3 Plugin.cpp

注册骨架可以直接使用：

```cpp
#include "SimplifyAddZero.h"

#include "llvm/Config/llvm-config.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Plugins/PassPlugin.h"

extern "C" LLVM_ATTRIBUTE_WEAK llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION,
        "SimplifyAddZero",
        LLVM_VERSION_STRING,
        [](llvm::PassBuilder& builder) {
            builder.registerPipelineParsingCallback(
                [](llvm::StringRef name,
                   llvm::FunctionPassManager& manager,
                   llvm::ArrayRef<
                       llvm::PassBuilder::PipelineElement>) {
                    if (name != "simplify-add-zero") {
                        return false;
                    }

                    manager.addPass(
                        training::SimplifyAddZeroPass{});
                    return true;
                });
        }
    };
}
```

只理解这条调用关系即可：

- opt 加载动态库。
- 找到 llvmGetPassPluginInfo。
- 注册 pipeline 名称。
- 遇到 simplify-add-zero 时，将你的 Pass 加入 FunctionPassManager。
- Pass manager 调用 run。

本轮不需要读懂 PassBuilder 的整个实现。

## 6. 你要实现的变换逻辑

只修改 SimplifyAddZero.cpp 中的 run。

### 6.1 判断匹配条件

依次判断：

1. opcode 是否为 Instruction::Add。
2. 结果类型是否为标量 i32。
3. operand 0 或 operand 1 是否为 ConstantInt。
4. 该 ConstantInt 是否为零。

可以使用：

```cpp
instruction.getOpcode()
instruction.getType()->isIntegerTy(32)
instruction.getOperand(0)
instruction.getOperand(1)
llvm::dyn_cast<llvm::ConstantInt>(...)
constant->isZero()
```

不要把“常量”当成“零”。

不要用 getName 或打印出来的字符串做匹配。

推荐把匹配过程提取为一个小函数：

```cpp
llvm::Value* find_add_zero_replacement(
    llvm::Instruction& instruction);
```

约定：

- 匹配成功：返回替换后的 Value。
- 不匹配：返回 nullptr。
- 这个辅助函数只检查，不修改 IR。

### 6.2 先替换全部使用，再删除指令

需要学习两个 API：

```cpp
instruction.replaceAllUsesWith(replacement);
instruction.eraseFromParent();
```

顺序不能反过来。

例如：

```llvm
%a = add i32 %x, 0
%b = mul i32 %a, %a
```

两个操作数位置都必须变成 %x。

不要只修改第一个 user，也不要只修改 ret。

删除后，旧 Instruction 对象已经失效，不得再读取其名字、类型或操作数。

### 6.3 安全遍历

不要在普通范围 for 中删除当前指令后，继续使用失效的迭代器。

本轮选择一种安全方式实现即可：

- 在删除前先推进迭代器。
- 或使用 llvm::make_early_inc_range。

推荐先使用显式迭代器，清楚地理解：

> 当前指令被删除后，下一次访问的位置从哪里来？

不要预先收集一堆可能被后续删除的 replacement 指针，再统一执行替换；先使用边遍历边匹配、替换、删除的方式。

### 6.4 返回 PreservedAnalyses

维护一个函数内局部 changed 标志。

本轮采用保守策略：

| 情况 | 返回 |
|---|---|
| 没有修改任何指令 | PreservedAnalyses::all() |
| 至少修改一条指令 | PreservedAnalyses::none() |

all 表示已有分析结果仍然有效，不是“Pass 执行成功”。

none 是保守地不承诺保留已有分析，并不表示每一种分析实际上都失效。

不要修改了 IR 却无条件返回 all。

### 6.5 头文件

按实际使用补齐直接依赖，例如：

```cpp
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
```

若使用 make_early_inc_range，再包含对应的 ADT/STLExtras.h。

不要只依赖其他头文件偶然间接包含所需声明。

## 7. 基础测试

创建 tests/basic.ll：

```llvm
; CHECK-LABEL: define i32 @right_zero(
; CHECK-NOT: add i32
; CHECK: ret i32 %x
; CHECK-NEXT: }
define i32 @right_zero(i32 %x) {
entry:
  %a = add i32 %x, 0
  ret i32 %a
}

; CHECK-LABEL: define i32 @left_zero(
; CHECK-NOT: add i32
; CHECK: ret i32 %x
; CHECK-NEXT: }
define i32 @left_zero(i32 %x) {
entry:
  %a = add i32 0, %x
  ret i32 %a
}

; CHECK-LABEL: define i32 @chain(
; CHECK-NOT: add i32
; CHECK: ret i32 %x
; CHECK-NEXT: }
define i32 @chain(i32 %x) {
entry:
  %a = add i32 %x, 0
  %b = add i32 0, %a
  %c = add i32 %b, 0
  ret i32 %c
}

; CHECK-LABEL: define i32 @multiple_uses(
; CHECK-NOT: %a =
; CHECK: %b = mul i32 %x, %x
; CHECK-NEXT: %c = add i32 %x, %b
; CHECK-NEXT: ret i32 %c
define i32 @multiple_uses(i32 %x) {
entry:
  %a = add i32 %x, 0
  %b = mul i32 %a, %a
  %c = add i32 %a, %b
  ret i32 %c
}

; CHECK-LABEL: define i32 @unchanged(
; CHECK: %a = add i32 %x, 1
; CHECK-NEXT: %b = mul i32 %a, 0
; CHECK-NEXT: ret i32 %b
define i32 @unchanged(i32 %x) {
entry:
  %a = add i32 %x, 1
  %b = mul i32 %a, 0
  ret i32 %b
}
```

这五个函数分别检查：

| 函数 | 检查点 |
|---|---|
| right_zero | 右侧零 |
| left_zero | 左侧零 |
| chain | 连续删除不漏指令，替换关系继续传播 |
| multiple_uses | 所有 Use 更新，包括重复操作数 |
| unchanged | 不误改其他常量或 opcode |

### 7.1 运行 Pass

进入工程目录：

```bash
cd cpp/06_llvm_simplify_pass
```

运行：

```bash
opt \
  -load-pass-plugin=./build/SimplifyAddZero.so \
  -passes='function(simplify-add-zero)' \
  -verify-each \
  -S tests/basic.ll \
  -o build/basic.after.ll
```

参数含义：

- load-pass-plugin：加载你的动态库。
- passes：只运行指定 Pass。
- function(...)：明确这是函数级 pipeline。
- verify-each：在 Pass 后执行 IR 结构验证。
- S：输出文本 IR。
- o：保存输出。

不要额外加 default<O2>、instcombine 或其他优化 Pass。

否则测试通过可能是别的 Pass 帮你完成了变换。

### 7.2 自动检查

```bash
FileCheck tests/basic.ll < build/basic.after.ll
```

退出码 0 表示检查通过。

FileCheck 读取注释中的 CHECK 指令，检查输出文本。

本轮先使用 FileCheck 即可，不要求安装 lit 或 Google Test。

注意：

- verifier 检查 IR 的结构和约束。
- FileCheck 检查你期望的变换结果。
- 两者都不能单独证明变换对所有输入语义正确。

这条规则的正确性还需要你自己解释。

## 8. 90 分钟扩展测试

创建 tests/types.ll：

```llvm
; CHECK-LABEL: define i64 @keep_i64(
; CHECK: %a = add i64 %x, 0
; CHECK-NEXT: ret i64 %a
define i64 @keep_i64(i64 %x) {
entry:
  %a = add i64 %x, 0
  ret i64 %a
}

; CHECK-LABEL: define float @keep_float(
; CHECK: %a = fadd float %x,
; CHECK-NEXT: ret float %a
define float @keep_float(float %x) {
entry:
  %a = fadd float %x, 0.000000e+00
  ret float %a
}

; CHECK-LABEL: define <2 x i32> @keep_vector(
; CHECK: %a = add <2 x i32> %x, zeroinitializer
; CHECK-NEXT: ret <2 x i32> %a
define <2 x i32> @keep_vector(<2 x i32> %x) {
entry:
  %a = add <2 x i32> %x, zeroinitializer
  ret <2 x i32> %a
}
```

运行：

```bash
opt \
  -load-pass-plugin=./build/SimplifyAddZero.so \
  -passes='function(simplify-add-zero)' \
  -verify-each \
  -S tests/types.ll \
  -o build/types.after.ll

FileCheck tests/types.ll < build/types.after.ll
```

然后自己增加两个小案例：

1. 同一个函数有多个基本块，分别存在可消除的 add。
2. 带 nsw/nuw 的标量 i32 加零。

仍然只做同一条规则，不增加乘一、乘零或通用常量折叠。

可选：将 Pass 再运行一次，检查结果仍满足相同 FileCheck 断言，即不会越改越多。

## 9. Debug 方法与常见问题

### 9.1 建议顺序

1. 先让空 Pass 插件能够加载。
2. 临时向 llvm::errs() 打印函数名，确认 run 被调用。
3. 只实现 right_zero。
4. 加入 left_zero。
5. 跑 chain，检查删除时的遍历方式。
6. 跑 multiple_uses，检查 replaceAllUsesWith。
7. 最后跑负例。

临时诊断写 stderr，不要污染输出 IR；完成后清理调试打印。

### 9.2 观察 Pass 执行

```bash
opt \
  -load-pass-plugin=./build/SimplifyAddZero.so \
  -passes='function(simplify-add-zero)' \
  -debug-pass-manager \
  -disable-output tests/basic.ll
```

这能帮助区分：

- 插件根本没加载。
- pipeline 名称没注册。
- Pass 被调用但没有匹配。
- Pass 因 optnone 等条件被跳过。

### 9.3 常见问题

| 现象 | 优先检查 |
|---|---|
| 找不到插件文件 | build 是否成功，输出路径是否正确 |
| unknown pass name | pipeline 名称、回调注册及插件加载诊断 |
| undefined symbol 或加载失败 | opt 与插件是否来自匹配的 LLVM 安装 |
| duplicate option 等异常 | 是否误将静态 LLVM 库重复链接进插件 |
| 能运行但 IR 没变化 | 空骨架未实现、匹配条件错误、Pass 被跳过 |
| chain 只删除部分指令 | 删除后迭代器推进方式 |
| 删除时断言仍有 uses | 是否先 replaceAllUsesWith |
| 修改后崩溃 | 是否继续访问被删除的 Instruction |
| FileCheck 意外通过 | 是否运行了额外优化，断言是否足够明确 |

不要通过关闭 verifier 来“修复”变换错误。

## 10. 最少必要材料

优先查看本机 LLVM 23 对应版本的代码，不通读 LLVM。

仓库：llvm/llvm-project。

| 路径 | 只看这些位置 |
|---|---|
| llvm/examples/Bye/Bye.cpp | New PM 的 run、pipeline 注册、llvmGetPassPluginInfo |
| llvm/cmake/modules/AddLLVM.cmake | add_llvm_pass_plugin 的调用方式 |
| llvm/include/llvm/IR/PassManager.h | OptionalPassInfoMixin、PreservedAnalyses |
| llvm/include/llvm/Passes/PassBuilder.h | registerPipelineParsingCallback |
| llvm/include/llvm/Plugins/PassPlugin.h | PassPluginLibraryInfo |
| llvm/include/llvm/IR/Value.h | replaceAllUsesWith |
| llvm/include/llvm/IR/Instruction.h | eraseFromParent |
| llvm/include/llvm/IR/Constants.h | ConstantInt::isZero |

官方参考：

- [Writing an LLVM Pass](https://llvm.org/docs/WritingAnLLVMNewPMPass.html)
- [Programmer’s Manual：替换与删除指令](https://llvm.org/docs/ProgrammersManual.html#making-simple-changes)
- [FileCheck](https://llvm.org/docs/CommandGuide/FileCheck.html)

在线主分支可能比本机新。若接口不一致，以当前安装对应版本的头文件和示例为准。

只参考 Bye 的 New PM 部分，不复制 legacy PM、自动 pipeline 插入或 codegen 回调。

## 11. 时间安排

### 60 分钟最小闭环

| 时间 | 工作 |
|---|---|
| 10 分钟 | 理解插件入口、run 接口和变换规则 |
| 30 分钟 | 构建插件，实现匹配、替换和安全删除 |
| 15 分钟 | 跑 basic.ll、verifier 和 FileCheck |
| 5 分钟 | commit 和 notes |

最低交付：

- 可以被 opt 加载的插件。
- 正确的左右加零消除。
- basic.ll 中五个测试通过。
- 保存可复现命令。

### 90 分钟扩展闭环

额外完成：

- types.ll 的负例。
- 多基本块和 flags 案例。
- 将测试命令整理成一个失败即返回非零的脚本。
- 清理调试打印，完善 notes。

不增加新优化规则。

### 只有 30 分钟

- 5 分钟：理解接口。
- 20 分钟：让空 Pass 插件成功加载并确认 run 被调用。
- 5 分钟：记录构建命令和当前状态，提交骨架。

此时明确记录“变换逻辑尚未完成”，下次延续。

## 12. 完成标准与提交

- [ ] 06 独立构建，不修改 05。
- [ ] opt 能加载插件并找到 simplify-add-zero。
- [ ] x + 0 和 0 + x 都正确消除。
- [ ] 连续可删除指令不会漏掉。
- [ ] 全部使用位置均更新。
- [ ] 删除后不再使用旧指令指针。
- [ ] 不匹配的 opcode 和类型保持不变。
- [ ] changed 与 PreservedAnalyses 返回值对应。
- [ ] verifier 和基础 FileCheck 测试通过。
- [ ] notes 保存实际环境、命令、结果和未完成项。

建议 commit message：

```text
feat(llvm): add integer add-zero simplification pass
```

notes 不必写长文，但要区分：

- 插件加载成功。
- IR 验证通过。
- 变换断言通过。

这三件事不是同一件事。

## 13. 三个复盘问题

1. replaceAllUsesWith 和 eraseFromParent 分别完成什么？
   为什么必须先替换，再删除？

2. 为什么删除指令会影响遍历？
   你的实现怎样保证不会漏掉连续的加零指令，也不会访问已释放对象？

3. 为什么 verifier 通过不代表优化语义正确？
   本轮的正例、负例和规则推理分别提供了什么保证？

完成这一轮后，再选择一个有新知识点的分析或 MLIR rewrite 任务，不继续机械堆积类似算术规则。