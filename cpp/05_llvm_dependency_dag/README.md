# LLVM 实验 2：单基本块算术 RAW Dependency DAG

工程目录：`cpp/05_llvm_dependency_dag/`。

前置进度：

- Mini IR 的 Lexer、Parser、Def-Use 和算术 DAG 已完成。
- `04_llvm_ir_reader` 已能读取 LLVM IR，并遍历操作数及使用位置。

本轮将 LLVM 已有的值关系，转换成自己的依赖图。

## 1. 核心目标

实现一个类 `LLVMDependencyDAG`：

1. 接收一个 LLVM Function。
2. 检查它是否属于本实验支持的范围。
3. 给算术指令分配连续节点编号。
4. 从操作数找到 producer instruction。
5. 建立 producer → consumer 的 RAW 边。
6. 同时保存 predecessors 和 successors。
7. 去除重复边。
8. 支持同一个分析器连续分析多个函数，状态不残留。

本轮不做：

- latency、时间计算和关键路径。
- load/store 内存依赖。
- 多基本块、PHI 和控制依赖。
- LLVM Pass 插件。
- 修改或优化 LLVM IR。
- GPU 指令映射和真实硬件性能预测。

不需要 CUDA 或 GPU 环境。

## 2. 为什么值得做

上一轮解决：

> LLVM 中，一条指令使用了哪些 Value？

这一轮解决：

> 如何把这些 Value 关系转换成自己的分析数据结构？

与之前 Mini IR 的区别：

| 项目 | Mini IR | 本轮 LLVM IR |
|---|---|---|
| 读取程序 | 自己写 Lexer/Parser | LLVM 负责 |
| 找到操作数来源 | 自己维护定义表 | 从 Use 获取 Value |
| 找到 producer | 查询定义记录 | 判断 Value 是否为 Instruction |
| 图的节点编号 | 自己维护 | 仍由自己维护 |
| 图的边 | 自己建立 | 仍由自己建立 |

不要重新解析指令文本，也不要按名字寻找定义。

这一步为后续关键路径、调度和静态性能分析提供基础，但当前的 LLVM IR DAG 不是 SASS DAG，也不等于真实 GPU 执行模型。

## 3. 文件安排

在 `cpp/` 下新建 `05_llvm_dependency_dag/`。

| 文件 | 用途 |
|---|---|
| README.md | 实验说明 |
| CMakeLists.txt | 独立构建 |
| main.cpp | 参数检查、读取、验证、调用分析器和打印结果 |
| LLVMDependencyDAG.h | 数据结构和类声明 |
| LLVMDependencyDAG.cpp | 分析实现 |
| examples/arithmetic.ll | 基础依赖关系 |
| examples/repeated_use.ll | 重复操作数 |
| examples/unnamed.ll | 无符号名字 |
| examples/return_only.ll | 无算术节点 |
| examples/unused.ll | 无人使用的算术指令 |
| examples/unsupported.ll | 不支持的指令 |
| examples/multiple_blocks.ll | 多基本块 |
| examples/state_sequence.ll | 连续分析时的状态检查 |
| notes.md | 简短设计与测试记录 |

可以复用 04 的文件读取和 CMake 思路，不复制 Mini IR 的解析器。

## 4. 支持范围与错误约定

### 4.1 支持的函数

函数定义必须满足：

- 恰好一个基本块。
- 返回类型为标量 i32。
- 所有参数类型为标量 i32。
- 不接受可变参数函数。
- 指令只允许标量 i32 的 add、mul 和 ret。
- 算术操作数只接受当前函数参数、ConstantInt 或本块内已编号的算术指令。

其他指令或操作数类别明确报错，不静默忽略。

例如，sub、load、store、call、phi、br 都不属于本轮范围。

`add nsw`、`add nuw` 等附加标志不改变本轮 RAW 建边方式；本轮不分析溢出和 poison 语义。

### 4.2 哪些对象成为节点

只有 add 和 mul 成为图节点。

以下对象不是节点：

- 函数参数。
- 整数常量。
- ret。

例如：

```llvm
define i32 @example(i32 %x) {
entry:
  %a = add i32 %x, 1
  %b = mul i32 %a, 2
  ret i32 %b
}
```

图中只有两个节点：

- 节点 0：%a。
- 节点 1：%b。
- 边：0 → 1。

不要为参数、常量或 ret 分配图节点编号。

### 4.3 命令行行为

程序接收且只接收一个文件路径：

```bash
./build/llvm_dependency_dag examples/arithmetic.ll
```

约定：

| 情况 | 行为 |
|---|---|
| 参数数量错误 | 输出用法，非零退出码 |
| 文件不存在或解析失败 | 输出诊断，非零退出码 |
| LLVM IR 验证失败 | 输出诊断，非零退出码 |
| 函数声明 | 跳过，可打印 skipped declaration |
| 空 Module | 成功，退出码 0 |
| 支持的函数定义 | 输出 DAG |
| 不支持的函数定义 | 输出错误，不输出该函数的部分 DAG |

一个 Module 中包含多个函数时，逐个处理。

某个函数分析失败后，可以继续分析后面的函数，但程序最终返回非零退出码。

函数声明由 main.cpp 跳过；若直接把声明传给 analyze，也应返回错误，不能调用空函数的 front()。

## 5. 数据结构与接口

在 `LLVMDependencyDAG.h` 中声明：

```cpp
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace llvm {
class Function;
class Instruction;
}

using InstructionId = std::size_t;

struct LLVMDependencyNode {
    const llvm::Instruction* instruction = nullptr;

    std::vector<InstructionId> predecessors;
    std::vector<InstructionId> successors;
};

struct DAGAnalysisInfo {
    std::vector<LLVMDependencyNode> graph;
};

struct DAGAnalysisResult {
    std::optional<DAGAnalysisInfo> info;
    std::optional<std::string> error;
};

class LLVMDependencyDAG {
public:
    void analyze(const llvm::Function& function);

    const DAGAnalysisResult& results() const {
        return result_;
    }

private:
    DAGAnalysisResult result_;
};
```

### 5.1 节点编号

按算术指令在基本块中的出现顺序编号，从 0 开始。

`graph[id]` 就是编号为 id 的节点。

不要使用：

- LLVM 的符号名字。
- `%2` 等文本槽位编号。
- 指针转换得到的整数。

作为自己的节点编号。

### 5.2 结果状态

成功时：

- info 有值。
- error 无值。
- graph 可以为空。

失败时：

- info 无值。
- error 有值。
- 不允许保留上一次成功结果或本次部分结果。

建议先在局部变量中构建 DAG，全部成功后再移动到 result_。

每次 analyze 开始时清理上一轮结果。

### 5.3 生命周期

节点中的 Instruction* 是非拥有指针：

- 不负责释放 LLVM 指令。
- 不要 delete。
- 使用结果时，原 Module 和 LLVMContext 必须仍然存在。
- 本实验中不修改或删除被分析的指令。

results() 返回的是分析器内部结果的引用。

下一次 analyze 后，旧结果内容已经改变；不要跨调用保留旧 graph 中元素的引用或指针。

## 6. 实现步骤

### 第一步：复用文件读取并验证 Module

main.cpp 负责：

1. 检查参数。
2. 创建 LLVMContext。
3. 调用 parseIRFile。
4. 检查解析是否成功。
5. 调用 verifyModule。
6. 遍历函数并启动分析。

注意 verifyModule 的返回值：

```cpp
if (llvm::verifyModule(*module, &llvm::errs())) {
    return 1;
}
```

返回 true 表示发现错误，不是验证通过。

读取成功不代表 IR 一定满足全部结构约束，因此需要验证。

### 第二步：检查函数范围

进入 analyze 后，先检查：

- 是否为函数声明。
- 是否为可变参数函数。
- 返回类型和参数类型。
- 基本块数量。

这些检查通过后，才能取得唯一基本块。

### 第三步：第一遍遍历，编号和检查指令

使用：

```cpp
std::unordered_map<const llvm::Instruction*, InstructionId>
```

记录：

> LLVM 指令对象 → 自己的节点编号。

第一遍遍历基本块：

- add/mul：检查标量 i32 类型，创建节点并记录编号。
- ret：检查范围要求，但不创建节点。
- 其他指令：返回错误。

建议使用：

```cpp
llvm::Instruction::Add
llvm::Instruction::Mul
llvm::Instruction::Ret
```

判断 opcode，不根据打印文本判断。

注意：

- map 的 key 是指令指针，不是名字。
- graph 的顺序由基本块遍历决定，不由 unordered_map 的迭代顺序决定。
- 即使算术结果无人使用，也必须创建节点。

### 第四步：第二遍遍历，从 operands 建边

只处理已经创建的算术节点。

读取操作数的基本方式：

```cpp
for (const llvm::Use& use : instruction.operands()) {
    const llvm::Value* value = use.get();

    // 判断来源并处理。
}
```

按来源处理：

| 来源 | 行为 |
|---|---|
| 当前函数的 Argument | 外部输入，不建边 |
| ConstantInt | 常量输入，不建边 |
| 当前基本块的算术 Instruction | 查找 producer ID，建立边 |
| 其他来源 | 返回错误 |

对于 Instruction 来源：

1. 确认属于当前基本块。
2. 用 find 查询编号。
3. 确认 producer ID 小于 consumer ID。
4. 保存 producer → consumer。

不要通过 `map[key]` 查询必须存在的 producer。

因为 operator[] 在 key 不存在时会插入默认值，可能把错误来源误认为节点 0。

建立一条边时，需要同时更新：

```text
graph[consumer].predecessors
graph[producer].successors
```

这里保存的是指令节点的依赖关系。

不要把 BasicBlock 的 CFG predecessors/successors 当成算术指令依赖。

### 第五步：排序与去重

例如：

```llvm
%b = mul i32 %a, %a
```

存在两个操作数位置，但只有一条节点依赖边。

要求：

- predecessors 不重复。
- successors 不重复。
- 两种邻接表都按节点 ID 升序排列。

推荐使用：

- std::sort。
- std::unique。
- vector::erase。

需要理解：

> std::unique 返回新的逻辑结尾，但不会直接缩小 vector 的 size。

本轮建议统一收集边后，再整理所有节点的邻接表。

### 第六步：输出结果

输出至少包含：

- 函数名。
- 节点数。
- 唯一边数。
- 每个节点的 ID。
- 对应完整 LLVM 指令。
- predecessors。
- successors。

边数可以通过所有节点 successors 的长度之和得到。

不要把 predecessors 和 successors 的长度一起相加，否则每条边被计算两次。

无名指令也必须正常输出。

打印完整指令可以使用：

```cpp
llvm::outs() << *node.instruction << "\n";
```

## 7. 测试输入与预期结果

### 7.1 arithmetic.ll

```llvm
define i32 @arithmetic(i32 %x, i32 %y) {
entry:
  %a = add i32 %x, 1
  %b = mul i32 %y, 2
  %c = add i32 %a, %b
  %d = mul i32 %a, 3
  %e = add i32 %c, %d
  ret i32 %e
}
```

预期节点数 5，边数 5。

| ID | 指令结果 | predecessors | successors |
|---|---|---|---|
| 0 | %a | [] | [2, 3] |
| 1 | %b | [] | [2] |
| 2 | %c | [0, 1] | [4] |
| 3 | %d | [0] | [4] |
| 4 | %e | [2, 3] | [] |

ret 不创建节点，因此不存在节点 5。

### 7.2 repeated_use.ll

```llvm
define i32 @repeated_use(i32 %x) {
entry:
  %a = add i32 %x, 1
  %b = mul i32 %a, %a
  ret i32 %b
}
```

预期：

- 节点数 2。
- 边数 1。
- 节点 0 successors 为 [1]。
- 节点 1 predecessors 为 [0]。

不能出现 [1, 1] 或 [0, 0]。

### 7.3 unnamed.ll

```llvm
define i32 @unnamed(i32 %0) {
  %2 = add i32 %0, 1
  %3 = mul i32 %2, 2
  ret i32 %3
}
```

预期：

- 节点数 2，边数 1。
- 自己的节点编号为 0、1。
- 边为 0 → 1。

这里入口块占用数字槽位 1，因此第一条结果为 %2。

LLVM 的 %2 不等于图节点 2。

### 7.4 return_only.ll

```llvm
define i32 @return_argument(i32 %x) {
entry:
  ret i32 %x
}

define i32 @return_constant() {
entry:
  ret i32 7
}
```

两个函数都应分析成功：

- info 有值。
- graph 为空。
- 节点数 0，边数 0。

空图不是分析失败。

### 7.5 unused.ll

```llvm
define i32 @unused(i32 %x) {
entry:
  %unused = mul i32 %x, 2
  %answer = add i32 %x, 1
  ret i32 %answer
}
```

预期：

- 节点数 2，边数 0。
- %unused 仍保留为节点 0。
- 两个节点的 predecessors 和 successors 都为空。

本轮不执行死代码删除，也不只分析返回值的依赖切片。

### 7.6 unsupported.ll

```llvm
define i32 @unsupported(i32 %x) {
entry:
  %a = add i32 %x, 1
  %b = sub i32 %a, 2
  ret i32 %b
}
```

预期：

- 明确报告不支持 sub。
- info 无值，error 有值。
- 不输出只包含 %a 的部分成功图。
- 命令行最终返回非零退出码。

这个文件是合法 LLVM IR，只是不在本分析器的支持范围内。

区分：

> IR 不合法，与本工具不支持，是两类不同错误。

### 7.7 multiple_blocks.ll

```llvm
define i32 @multiple_blocks(i32 %x) {
entry:
  br label %exit

exit:
  ret i32 %x
}
```

预期：

- LLVM 验证通过。
- 分析器拒绝。
- 错误说明只支持一个基本块。
- 不把两个基本块展开后继续建图。

### 7.8 state_sequence.ll

```llvm
define i32 @first_ok(i32 %x) {
entry:
  %a = add i32 %x, 1
  %b = mul i32 %a, 2
  ret i32 %b
}

define i32 @then_fail(i32 %x) {
entry:
  %a = add i32 %x, 1
  %b = sub i32 %a, 2
  ret i32 %b
}

define i32 @then_empty(i32 %x) {
entry:
  ret i32 %x
}

define i32 @finally_ok(i32 %x) {
entry:
  %a = mul i32 %x, 3
  ret i32 %a
}
```

在函数循环外创建同一个 LLVMDependencyDAG 对象，依次调用 analyze。

预期：

| 函数 | info | error | 节点数 | 边数 |
|---|---|---|---|---|
| first_ok | 有值 | 无值 | 2 | 1 |
| then_fail | 无值 | 有值 | 不提供 | 不提供 |
| then_empty | 有值 | 无值 | 0 | 0 |
| finally_ok | 有值 | 无值 | 1 | 0 |

特别检查：

- 失败后没有保留 first_ok 的图。
- 空图成功后没有保留上一轮 error。
- finally_ok 的节点重新从 0 编号。

整个文件因为包含失败函数，最终退出码非零。

## 8. CMake 与构建

创建 `CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.20)
project(llvm_dependency_dag LANGUAGES C CXX)

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

separate_arguments(
  LLVM_DEFINITIONS_LIST NATIVE_COMMAND "${LLVM_DEFINITIONS}"
)
add_definitions(${LLVM_DEFINITIONS_LIST})

add_executable(llvm_dependency_dag
  main.cpp
  LLVMDependencyDAG.cpp
)

target_include_directories(
  llvm_dependency_dag SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS}
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(
    llvm_dependency_dag PRIVATE -Wall -Wextra -Wpedantic
  )
  if(NOT LLVM_ENABLE_RTTI)
    target_compile_options(
      llvm_dependency_dag PRIVATE -fno-rtti
    )
  endif()
endif()

llvm_map_components_to_libnames(
  llvm_libs Core IRReader Support
)

target_link_libraries(
  llvm_dependency_dag PRIVATE ${llvm_libs}
)
```

在仓库根目录执行：

```bash
cmake -S cpp/05_llvm_dependency_dag \
  -B cpp/05_llvm_dependency_dag/build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DLLVM_DIR="$(llvm-config --cmakedir)"

cmake --build cpp/05_llvm_dependency_dag/build -j
```

进入工程目录：

```bash
cd cpp/05_llvm_dependency_dag
./build/llvm_dependency_dag examples/arithmetic.ll
```

LLVM 的头文件、库、llvm-config 和工具使用同一套安装。

如果本机命令为 llvm-config-20 或其他版本后缀，替换对应命令，不要求重新安装特定版本。

不要提交 build 目录。

## 9. 测试与 Debug 方法

### 9.1 先确认输入合法

例如：

```bash
llvm-as examples/arithmetic.ll -o build/arithmetic.bc
```

unsupported.ll 和 multiple_blocks.ll 也应该能通过 LLVM 验证。

它们测试的是分析器边界，不是语法错误。

### 9.2 保存输出证据

```bash
./build/llvm_dependency_dag examples/arithmetic.ll \
  > build/arithmetic.out

./build/llvm_dependency_dag examples/repeated_use.ll \
  > build/repeated_use.out
```

对照第 7 节的节点、边和邻接表检查。

输出排版可以自行决定，节点编号和关系必须一致。

### 9.3 检查图的不变量

可以在 Debug 构建中增加断言或辅助检查：

1. 所有 instruction 指针非空。
2. 所有邻接节点 ID 小于 graph.size()。
3. 每条边 producer ID < consumer ID。
4. 邻接表有序且没有重复。
5. u.successors 包含 v，当且仅当 v.predecessors 包含 u。

不要只检查节点数正确，就认为建图正确。

### 9.4 建议断点

在以下位置设置断点：

- 每次 analyze 开始。
- 节点创建后。
- 操作数分类处。
- producer ID 查询后。
- 去重前后。
- 分析失败返回前。

重点观察 repeated_use.ll：

- LLVM 层面有两次 Use。
- 初始收集时可能出现两条相同记录。
- 最终邻接表中只有一条边。

### 9.5 额外错误检查

还应检查：

- 未传参数。
- 参数过多。
- 文件不存在。
- 无法解析的 IR。
- 返回类型或参数不是 i32。
- vector add/mul。
- 只有声明的 Module。
- 合法空 Module。

不强制本轮引入新的测试框架。

如果已有 Google Test 环境，可把邻接表和连续分析状态写成自动化测试；不要为了测试框架安装耗尽本轮时间。

## 10. 最少必要阅读

只定位以下内容，不要求通读 LLVM：

仓库：https://github.com/llvm/llvm-project

| 目录 → 文件 | 只看这些内容 |
|---|---|
| llvm/include/llvm/IR/Function.h | Function、isDeclaration、args、基本块遍历 |
| llvm/include/llvm/IR/BasicBlock.h | BasicBlock、指令遍历 |
| llvm/include/llvm/IR/Instruction.h | Instruction、getOpcode、getParent |
| llvm/include/llvm/IR/User.h | User::operands |
| llvm/include/llvm/IR/Use.h | Use::get |
| llvm/include/llvm/IR/Argument.h | Argument::getParent |
| llvm/include/llvm/IR/Constants.h | ConstantInt |
| llvm/include/llvm/IR/Type.h | isIntegerTy |
| llvm/include/llvm/IR/Verifier.h | verifyModule |
| llvm/include/llvm/Support/Casting.h | isa、dyn_cast 的使用方式 |

当前读取的是 use-def 方向：

> consumer instruction → operand Value → producer instruction。

这里只是得到 producer 的候选对象；还要判断其类型和所属范围，才能建边。

本轮不读 PassManager，不读 NVPTX backend。

官方参考：

- [LLVM Programmer’s Manual：use-def 遍历](https://llvm.org/docs/ProgrammersManual.html#iterating-over-def-use-use-def-chains)
- [LLVM Verifier 接口](https://llvm.org/doxygen/Verifier_8h_source.html)
- [CMake：Embedding LLVM in your project](https://llvm.org/docs/CMake.html#embedding-llvm-in-your-project)

## 11. 本轮 C++ 标准库学习点

| 工具 | 本轮用途 |
|---|---|
| std::vector | 保存节点和邻接表 |
| std::unordered_map | 指令指针映射为节点 ID |
| find | 查询已有 producer，避免隐式插入 |
| emplace | 插入指令与 ID 的映射 |
| std::sort | 统一邻接表顺序 |
| std::unique + erase | 删除重复边 |
| std::optional | 区分成功结果与失败 |
| std::move | 将局部构建结果提交到分析器 |
| const 指针与引用 | 表达只读分析和非拥有关系 |

特别注意 vector 扩容：

如果保存了 graph 某个元素的引用，后续 push_back 可能让该引用失效。

采用“第一遍创建全部节点、第二遍填充边”，可以避免建边过程中继续扩容外层 graph。

## 12. 时间安排

### 60 分钟最小闭环

| 时间 | 工作 |
|---|---|
| 10 分钟 | 确定节点、编号、边方向和结果状态 |
| 30 分钟 | 完成类、两遍遍历、建边与去重 |
| 15 分钟 | 验证 arithmetic、repeated_use、unnamed |
| 5 分钟 | commit，并记录实际通过的案例 |

最低产物：

- 可构建、可运行的程序。
- 三个基础输入。
- 正确的节点编号和邻接表。
- 简短 notes。

若没有完成，下一次延续，不开启 timing。

### 90 分钟扩展闭环

在 60 分钟版基础上增加：

- return_only 和 unused。
- unsupported 和 multiple_blocks。
- state_sequence，确认分析器复用正确。
- 图不变量检查和错误诊断整理。

仍然不加入关键路径和性能模型。

### 临时只有 30 分钟

- 5 分钟：明确本次只完成哪个步骤。
- 20 分钟：实现该步骤。
- 5 分钟：构建、验证并提交。

例如只完成“函数检查 + 节点编号”，notes 明确写出建边尚未完成，不把整个 05 标记为完成。

## 13. 完成标准

最小闭环：

- [ ] 可以独立构建和运行。
- [ ] LLVM 负责解析，程序不重新解析字符串。
- [ ] 节点仅包含 add/mul。
- [ ] 节点 ID 按 IR 顺序连续分配。
- [ ] 使用 Instruction* 查找 producer。
- [ ] predecessors 和 successors 正确。
- [ ] 重复操作数不产生重复边。
- [ ] unnamed 案例不依赖 getName()。
- [ ] 基础三个案例结果符合预期。

完整验收再增加：

- [ ] ret-only 返回成功空图。
- [ ] 无人使用的算术指令仍保留。
- [ ] 不支持的函数明确失败，不返回部分图。
- [ ] 连续分析时，图、错误和编号不残留。
- [ ] CLI 错误与退出码正确。
- [ ] notes 记录编译环境、测试案例和未完成项。

建议 commit message：

```text
feat(llvm): build single-block arithmetic RAW DAG
```

后续补边界测试时，可单独提交：

```text
test(llvm): cover RAW DAG edge cases and analyzer reuse
```

## 14. 结束后的三个复盘问题

1. LLVM 的 Use 如何让我们找到操作数的 producer？
   为什么不需要再建立“字符串名字 → 定义”的表？

2. 为什么节点保存 Instruction*，边保存 InstructionId？
   这两种数据分别依赖哪些生命周期条件？

3. 为什么当前 RAW DAG 还不能代表 GPU 的实际执行时间？
   如果加入 load/store、控制流和资源限制，还缺少什么信息？

在 notes.md 中简短记录：

- LLVM 和编译器版本。
- 构建、运行命令。
- 实际通过的测试。
- 一个踩坑点及其原因。
- 尚未完成的内容。

完成并 Review 后，再在当前 DAG 上加入抽象 latency 和关键路径。