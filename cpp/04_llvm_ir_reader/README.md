# LLVM 实验 1：IR 阅读器与 use-def 遍历

工程目录：`cpp/04_llvm_ir_reader/`。

前置进度：Mini IR 的 Lexer、Parser、DefUser 和算术 RAW DAG 已完成。

本轮写一个独立的 C++ 命令行工具：读取 LLVM IR 文件，打印函数、基本块、指令，以及每条指令的操作数来源和使用位置。

完成后提交代码，再进行 Review。

## 1. 实验目标与范围

本轮完成：

1. 读取一个 `.ll` 文件，失败时打印诊断并返回非零退出码。
2. 遍历 Module → Function → BasicBlock → Instruction。
3. 打印每条指令的完整文本和 opcode。
4. 对每个操作数，打印位置编号及来源类别。
5. 对每条计算指令的结果，打印使用它的指令和操作数位置。
6. 正确处理重复使用、无人使用的结果和没有结果名字的指令。

这轮完成的是“读取真实 IR，理解 LLVM 已有的值关系”。

DAG、关键路径、Pass 插件、IR 修改和 GPU 指令映射在后续实验中加入。本轮不要求 GPU 或 CUDA 环境。

## 2. 与当前 DefUser 的关系

| Mini IR 中的概念 | 本轮对应的 LLVM 表示 |
|---|---|
| 文本程序 | Module 中的函数和基本块 |
| 一条解析后的指令 | Instruction |
| 指令产生的值 | 非 void 指令本身也是一个 Value |
| 外部输入 | 本轮示例中的函数参数 Argument |
| 整数常量 | ConstantInt |
| 某个操作数位置的来源 | Use 指向的 Value |
| 某个值的全部使用位置 | Value 的 uses() |

理解三个词：

- **Value**：可以被引用的、有类型的值。
- **User**：拥有操作数、引用其他 Value 的对象；Instruction 是 User。
- **Use**：某个 User 的某个操作数位置对 Value 的一次引用。

例如：

```llvm
%b = mul i32 %a, %a
```

这里：

- 两个操作数都指向同一个 Value：`%a`。
- 有两个 Use：operand 0 和 operand 1。
- 使用者指令是同一条 `%b`。
- 将来建 DAG 时，这两个位置可以去重成一条 `a → b` 边。

**本轮保留两个使用位置，不做边去重。**

LLVM SSA 值的来源通过对象关系表达，不需要再按字符串名字查找定义。名字用于显示；不要用 `getName()` 作为值身份。

## 3. 环境检查

在你实际写代码的机器上运行：

```bash
cmake --version
c++ --version
llvm-config --version
llvm-config --cmakedir
llvm-as --version
```

需要 LLVM 开发环境：头文件、库、`LLVMConfig.cmake` 和命令行工具。

如果系统使用带版本号的命令，例如：

```bash
llvm-config-20 --version
llvm-config-20 --cmakedir
llvm-as-20 --version
```

后续命令相应替换。这里的 20 只是示例，不要求安装特定版本。

LLVM 头文件、库和工具使用同一套安装。已有可用的 LLVM 开发环境时，先直接使用。

若 `llvm-config` 不存在，先确认是否有带版本后缀的命令；若缺少开发文件，再根据系统安装 LLVM 开发包。环境问题先解决到“能找到 LLVMConfig.cmake”为止。

本实验采用 C++20。

## 4. 文件安排

| 文件 | 用途 |
|---|---|
| `README.md` | 本实验说明 |
| `CMakeLists.txt` | 独立构建配置 |
| `main.cpp` | 你实现的 IR 阅读器 |
| `examples/arithmetic.ll` | 基础算术关系 |
| `examples/repeated_use.ll` | 同一值被一条指令读取两次 |
| `examples/unnamed.ll` | 没有符号名字的值和基本块 |
| `examples/unused.ll` | 无人使用的计算结果 |
| `examples/declaration.ll` | 只有函数声明，没有函数体 |
| `examples/empty.ll` | 空 Module |
| `examples/invalid.ll` | 错误输入 |

保留上一轮工程，不复制 Lexer、Parser、DefUser 或 DependencyDAG 的代码。

本轮使用 LLVM 解析文件。

## 5. 基础输入

创建 `examples/arithmetic.ll`：

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

先看懂这些表达：

| 表达 | 含义 |
|---|---|
| `define i32 @arithmetic(...)` | 定义返回 i32 的函数 |
| `i32 %x` | 类型为 i32 的函数参数 |
| `entry:` | 基本块名字 |
| `%a = add i32 %x, 1` | 整数加法，结果是 %a |
| `ret i32 %e` | 返回 %e；ret 自身不产生可供后续引用的结果 |

这个输入有：

- 1 个函数定义。
- 1 个基本块。
- 6 条指令：5 条算术指令和 1 条 ret。
- 2 个函数参数。

这与上一轮 DAG 的节点数不同：本轮是通用的指令阅读器，`ret` 也要打印。

## 6. 第一步：读取文件

命令行约定：

```bash
./build/llvm_ir_reader examples/arithmetic.ll
```

程序接收且只接收一个文件路径。

| 情况 | 行为 |
|---|---|
| 正常读取 | 输出分析信息，退出码 0 |
| 没有路径或参数过多 | 输出用法，返回非零 |
| 文件不存在 | 输出读取诊断，返回非零 |
| 文件内容无法解析 | 输出 LLVM 诊断，返回非零 |
| 合法空 Module | 成功，不输出指令 |

需要了解：

```cpp
llvm::LLVMContext
llvm::SMDiagnostic
llvm::parseIRFile
std::unique_ptr<llvm::Module>
```

读取部分可参考这个骨架：

```cpp
llvm::LLVMContext context;
llvm::SMDiagnostic diagnostic;

auto module = llvm::parseIRFile(path, diagnostic, context);
if (!module) {
    diagnostic.print(program_name, llvm::errs());
    return 1;
}

// 在这里继续遍历 module。
```

其中 `path` 是命令行提供的文件路径，`program_name` 可以使用 `argv[0]`。

注意生命周期：

- Context 先创建，Module 后创建。
- Module 销毁时，Context 仍要存在。
- `unique_ptr` 负责释放 Module。
- 遍历得到的指令、参数、操作数指针属于 LLVM 对象关系；不要对它们调用 delete。
- 本轮无需把这些指针保存到程序结束之后。

## 7. 第二步：遍历并打印

按照以下层次遍历：

```text
Module
  Function
    BasicBlock
      Instruction
```

建议先实现这一小步：

- 打印函数。
- 打印基本块。
- 打印完整指令。
- 打印指令的 opcode。
- 最后输出计数。

对只有声明、没有函数体的函数，打印 `declaration` 即可；它没有基本块可遍历。

相关 API：

| API | 用途 |
|---|---|
| `Function::isDeclaration()` | 判断是否只有声明 |
| `Instruction::getOpcodeName()` | 获取 opcode 文本 |
| `Instruction::print(...)` | 打印完整指令 |
| `llvm::outs()` | 正常输出 |
| `llvm::errs()` | 诊断输出 |

打印 LLVM 对象使用 LLVM 的输出接口，例如：

```cpp
llvm::outs() << instruction << "\n";
```

先确认基础输入打印了完整的 6 条指令，再加入来源分类。

计数时可以分别记录：

- 函数定义数。
- 函数声明数。
- 基本块数。
- 指令数。

函数声明不计入函数定义数。

## 8. 第三步：打印操作数来源

对每条指令的每个操作数打印：

1. 操作数位置，从 0 开始。
2. 操作数的文本表示。
3. 来源类别。

本轮分类：

| LLVM 对象 | 输出类别 |
|---|---|
| Instruction | Instruction |
| Argument | Argument |
| ConstantInt | ConstantInt |
| 其他 Value | Other |

其他类别至少打印原始 operand 文本即可；本轮不要求分析其语义。

建议使用：

```cpp
llvm::dyn_cast<llvm::Instruction>(value)
llvm::dyn_cast<llvm::Argument>(value)
llvm::dyn_cast<llvm::ConstantInt>(value)
```

`dyn_cast` 在类别匹配时返回对应类型的指针，否则返回空指针。

遍历骨架：

```cpp
unsigned operand_index = 0;
for (llvm::Use& use : instruction.operands()) {
    llvm::Value* value = use.get();

    // 打印 operand_index。
    // 判断 value 属于哪种类别，并打印它。

    ++operand_index;
}
```

基本案例应识别出：

```text
%a:
  operand 0: %x, Argument
  operand 1: 1, ConstantInt

%c:
  operand 0: %a, Instruction
  operand 1: %b, Instruction

ret:
  operand 0: %e, Instruction
```

不需要重新建立“名字 → 定义”的 map。若 value 是 Instruction，其指针就指向产生该值的那条指令。

## 9. 第四步：打印使用位置

对每条非 void 指令的结果，遍历 `uses()`，打印：

- 谁在使用它。
- 使用发生在该 User 的哪个操作数位置。

相关 API：

```cpp
value.uses()
use.getUser()
use.getOperandNo()
```

对于本轮案例，User 都是指令；如果遇到其他 User，打印 `OtherUser` 和原始表示。

示例预期：

```text
%a:
  used by %c, operand 0
  used by %d, operand 0

%b:
  used by %c, operand 1

%c:
  used by %e, operand 0

%d:
  used by %e, operand 1

%e:
  used by ret i32 %e, operand 0
```

**使用位置的遍历顺序不作要求。** 验收时比较包含哪些位置，不要求与上面打印顺序一致。

推荐遍历 `uses()`，因为本轮关心的是每一次使用的位置。不要把 `users()` 的遍历结果直接当作已去重的指令列表。

没有使用位置时，明确打印 `no uses`。

`ret` 的类型为 void：它有操作数，但没有可供使用的计算结果。可以打印 `no result (void)`，不用给它编造结果名字。

可以使用下面的判断区分：

```cpp
instruction.getType()->isVoidTy()
```

## 10. 名字与显示方式

不能假设每个 Value 都有符号名字。

例如下面的文件也是合法 LLVM IR，创建 `examples/unnamed.ll`：

```llvm
define i32 @unnamed(i32 %0) {
  %2 = add i32 %0, 1
  ret i32 %2
}
```

这个函数的入口块没有名字，并占用数字槽位 1，所以第一条结果使用 `%2`。

这些数字是文本表示中的槽位编号，不是 `getName()` 返回的符号名字。

打印值时，优先使用：

```cpp
value.printAsOperand(llvm::outs(), false);
```

打印完整指令时，使用 `Instruction::print` 或流输出。

基本块也可以通过 LLVM 的打印接口显示。不要因为名字为空就跳过这个块或指令。

内部身份使用 Value 对象关系；显示方式交给 LLVM。

## 11. 验收案例

### 11.1 正常算术关系

使用 `arithmetic.ll`，检查：

- 函数定义 1 个、函数声明 0 个、基本块 1 个、指令 6 条。
- `%a` 的来源是 Argument 和 ConstantInt。
- `%c` 的来源是指令 %a 和 %b。
- `%a` 的使用位置是 %c/operand 0 和 %d/operand 0。
- `%e` 被 ret 的 operand 0 使用。

### 11.2 重复操作数

创建 `examples/repeated_use.ll`：

```llvm
define i32 @repeated_use(i32 %x) {
entry:
  %a = add i32 %x, 1
  %b = mul i32 %a, %a
  ret i32 %b
}
```

检查：

- %b 的两个操作数都指向 %a。
- %a 有两个使用位置：%b/operand 0 和 %b/operand 1。
- 没有把第二次使用丢掉。
- 指令总数为 3。

### 11.3 无人使用的结果

创建 `examples/unused.ll`：

```llvm
define i32 @unused(i32 %x) {
entry:
  %unused = mul i32 %x, 2
  %answer = add i32 %x, 1
  ret i32 %answer
}
```

检查：

- %unused 仍然打印。
- %unused 的使用列表为空。
- 指令总数为 3。

### 11.4 没有符号名字

使用 `unnamed.ll`，检查：

- 参数、基本块、指令仍能正常打印。
- add 的 operand 0 分类为 Argument。
- add 的结果被 ret/operand 0 使用。
- 指令总数为 2。

### 11.5 只有函数声明

创建 `examples/declaration.ll`：

```llvm
declare i32 @external_function(i32)
```

检查：

- 识别为 declaration。
- 函数定义 0 个、函数声明 1 个。
- 没有基本块、没有指令。
- 成功退出。

### 11.6 空 Module

在工程目录执行：

```bash
: > examples/empty.ll
./build/llvm_ir_reader examples/empty.ll
```

空文件作为空 Module 接收：

- 函数定义数为 0。
- 函数声明数为 0。
- 基本块数为 0。
- 指令数为 0。
- 成功退出。

### 11.7 错误输入

创建 `examples/invalid.ll`：

```llvm
define i32 @invalid(i32 %x) {
entry:
  %a = add i32 %x,
  ret i32 %a
}
```

检查：

- 返回非零退出码。
- 输出解析诊断。
- 不继续遍历空 Module。

另外验证不存在的文件、未传参数和参数过多。

### 验收汇总

| 案例 | 关键检查 |
|---|---|
| arithmetic | 全部来源和使用位置正确 |
| repeated_use | 同一个 User 的两个操作数位置都保留 |
| unused | 无人使用的指令仍打印 |
| unnamed | 不依赖符号名字 |
| declaration | 无函数体也能处理 |
| empty | 成功，计数为 0 |
| invalid | 非零退出码，有诊断 |
| 文件不存在 | 非零退出码，不解引用空指针 |
| 参数数量错误 | 输出用法，非零退出码 |

这轮用小输入核对输出即可，不强制引入 Google Test。

输出措辞和排版不要求逐字固定；关系、位置和退出码必须正确。

## 12. CMake 构建配置

创建 `CMakeLists.txt`：

```cmake
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
```

启用 C 和 CXX，是为了兼容某些 LLVM 包在配置阶段执行的 C 依赖检查；你的工具仍用 C++ 编写。

这里使用 LLVM 自己的 `dyn_cast`，与 C++ 原生的 `dynamic_cast` 不同。

在仓库根目录执行：

```bash
cmake -S cpp/04_llvm_ir_reader -B cpp/04_llvm_ir_reader/build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DLLVM_DIR="$(llvm-config --cmakedir)"

cmake --build cpp/04_llvm_ir_reader/build -j
```

切到工程目录后运行：

```bash
cd cpp/04_llvm_ir_reader
./build/llvm_ir_reader examples/arithmetic.ll
```

可以先用 LLVM 自带工具检查合法输入：

```bash
llvm-as examples/arithmetic.ll -o build/arithmetic.bc
```

`parseIRFile` 也支持 bitcode。读取 `build/arithmetic.bc` 可作为可选检查，本轮必须完成的是 `.ll` 输入。

若配置报错：

| 报错现象 | 优先检查 |
|---|---|
| 找不到 LLVMConfig.cmake | LLVM_DIR 是否来自 llvm-config --cmakedir |
| 找不到 llvm/IR/... 头文件 | 是否安装开发文件，include 路径是否正确 |
| LLVM 符号链接失败 | 是否通过组件函数链接对应的库，头文件和库是否来自同一套安装 |
| 版本切换后出现异常 | 用新的 build 目录重新配置，避免旧 CMake 缓存 |

## 13. 建议包含的头文件

按实际用到的类型包含头文件：

```cpp
#include <memory>

#include "llvm/IR/Argument.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
```

第一版一个 `main.cpp` 足够。

可以拆成几个打印函数，让主函数负责参数检查、读取文件和启动遍历。

参考拆分：

```cpp
void print_operand_source(llvm::Value& value);
void print_operands(llvm::Instruction& instruction);
void print_result_uses(llvm::Instruction& instruction);
void inspect_module(llvm::Module& module);
```

函数名字可以自行调整，不要求完全一致。

## 14. 本轮 C++ 学习点

| 工具或概念 | 本轮用途 |
|---|---|
| std::unique_ptr | 管理 Module 生命周期 |
| auto | 接收解析结果和迭代对象 |
| 范围 for | 遍历函数、基本块、指令与 Use |
| 指针与引用 | 理解对象身份和 LLVM 返回的非拥有指针 |
| if 中声明变量 | 检查 dyn_cast 的结果 |
| 函数拆分 | 分离读取、来源打印和使用位置打印 |

完成后应能自己解释：

1. 为什么 %a 的两个使用位置可以指向同一个 Value？
2. 为什么不需要再根据名字查询定义？
3. 为什么 ret 有操作数，却没有计算结果？
4. 为什么 LLVM 指令指针不能随便 delete？
5. 为什么 Context 要比 Module 活得久？
6. 为什么 use-def 遍历还不能自动解决 load/store 之间的内存依赖？

## 15. 完成标准与下一轮

完成标准：

- 能用 CMake 构建工具。
- 能读取正常的 `.ll` 文件并处理错误。
- 能遍历并打印所有指令，包括 ret。
- 操作数分类正确。
- 使用者和操作数编号正确。
- 重复使用保留两个位置。
- 无名值、无人使用的结果、函数声明和空 Module 均处理正确。
- 不重新解析字符串、不按名字重建定义关系。
- 保留示例文件，提交代码后进行 Review。

建议顺序：

1. 环境和构建配置。
2. 读取文件、打印原始指令。
3. 操作数来源分类。
4. 使用位置遍历。
5. 边界案例验收。
6. 提交与 Review。

如果一次只有 60～90 分钟，先做到“能读取、遍历、打印指令”；剩余部分接着完成，不另加任务。

下一轮：将已完成的 RAW DAG 思路接到 LLVM 单个基本块上，再做分析 Pass。

随后穿插 Triton 算子实践与 MLIR 阅读，继续沿 GPU 编译与架构分析方向推进。

## 16. 官方参考

按需查相关小节，无需先读完整手册。

- [LLVM Programmer’s Manual：对象关系、遍历、use-def](https://llvm.org/docs/ProgrammersManual.html)
- [LLVM IRReader：parseIRFile](https://llvm.org/docs/doxygen/llvm_2IRReader_2IRReader_8h_source.html)
- [Building LLVM with CMake：外部项目链接 LLVM](https://llvm.org/docs/CMake.html#embedding-llvm-in-your-project)
- [llvm-as：检查文本 IR 并生成 bitcode](https://llvm.org/docs/CommandGuide/llvm-as.html)
- [LLVM Language Reference：IR 语法](https://llvm.org/docs/LangRef.html)