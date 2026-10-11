# Mini IR：def-use 分析实验

工程目录：`cpp/02_mini_ir_parser/`。

当前进度：Lexer、Parser 已完成，本轮实现 def-use 分析。

## 1. 实验目标

接收 Parser 生成的指令列表，建立两种关系：

- use-def：某个操作数的值由哪条指令产生。
- def-use：某个值被哪些指令的哪些操作数使用。

同时检查：

- 使用未定义的值。
- 同一个值被重复定义。
- destination 与外部输入重名。

本轮不执行运算、不做调度、不分析内存别名。

## 2. 示例

声明 `%x` 是外部输入：

```text
%a = add %x, 1
%b = mul %a, 2
%c = add %a, %b
ret %c
```

指令编号采用 vector 下标，从 0 开始。

| 编号 | 定义的值 | 操作数来源 |
|---:|---|---|
| 0 | `%a` | 外部输入 `%x`、常量 `1` |
| 1 | `%b` | 指令 0、常量 `2` |
| 2 | `%c` | 指令 0、指令 1 |
| 3 | 无 | 指令 2 |

使用位置表示为 `(指令编号, 操作数下标)`：

| 值 | 使用位置 |
|---|---|
| `%x` | `(0, 0)` |
| `%a` | `(1, 0)`、`(2, 0)` |
| `%b` | `(2, 1)` |
| `%c` | `(3, 0)` |

指令编号不同于源码行号。错误诊断使用 Token 保存的 SourceLocation。

## 3. 文件安排

| 文件 | 本轮动作 |
|---|---|
| `Lexer.h` | 复用 |
| `Parser.h` | 复用 |
| `DefUse.h` | 新增分析结构和函数 |
| `tests/def_use_test.cpp` | 新增测试 |
| `CMakeLists.txt` | 加入测试目标 |

保留现有 Lexer 和 Parser 测试。

## 4. 输入准备

当前 Parser 的调用方式：

```cpp
Parser parser;
parser.parser_tokenize_line(lexer.results());
```

调用方先检查全部 ParseResult：

1. 任意一行存在 error，就报告错误，不进行 def-use 分析。
2. 全部无错误后，按源码行号顺序收集有效 Instruction。
3. 空行、注释行跳过，不占指令编号。

得到：

```cpp
std::vector<Instruction> instructions;
```

外部输入通过函数参数显式提供，不新增文本语法：

```cpp
std::unordered_set<std::string> external_inputs{"%x"};
```

## 5. 数据结构

建立关系时，用哈希表按名字查询定义者；建立后，保存指令编号，通过 vector 下标直接访问。

在 `DefUse.h` 中定义：

```cpp
#pragma once

#include "Parser.h"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using InstructionId = std::size_t;

// 来源分类，不是 int/float 等数值类型。
enum class SourceKind {
    Instruction,    // 前面某条指令产生的值
    ExternalInput,  // 调用方显式声明的外部输入
    Constant        // 直接写出的整数常量
};

struct SourceRef {
    SourceKind kind;
    std::optional<InstructionId> defining_instruction;
};

struct InstructionUseInfo {
    std::vector<SourceRef> sources;
};

struct UseSite {
    InstructionId instruction_index;
    std::size_t operand_index;
};

struct DefUseInfo {
    std::unordered_map<std::string, InstructionId> definitions;
    std::vector<InstructionUseInfo> instructions;
    std::unordered_map<std::string, std::vector<UseSite>> uses;
};

struct AnalysisError {
    std::string message;
    SourceLocation location;
};

struct AnalysisResult {
    std::optional<DefUseInfo> info;
    std::optional<AnalysisError> error;
};

// 实现由你完成；在头文件中定义时保留 inline。
inline AnalysisResult analyze_def_use(
    const std::vector<Instruction>& instructions,
    const std::unordered_set<std::string>& external_inputs);
```

字段约定：

- `definitions` 只记录程序内部定义的命名值。
- `info.instructions[i].sources[j]` 对应输入指令 i 的操作数 j。
- 来源为 Instruction 时，defining_instruction 有值；其他来源为空。
- 名字和常量文本从原 Instruction 中读取，不重复保存。
- `uses` 包含内部定义和外部输入；无人使用时保留空列表。
- 常量不加入 definitions 或 uses。
- 成功：info 有值、error 为空。
- 失败：info 为空、error 有值，不返回半成品。
- 每次调用重新建立结果，不累积旧数据。
- 空指令列表合法。
- 分析结果只对应本次指令列表；重排指令后需要重新分析。

## 6. 分析流程

按指令顺序从前往后处理：

1. 为外部输入建立空 uses 条目。
2. 检查当前 destination 是否已经定义，或与外部输入重名。
3. 按顺序分析每个 operand：
   - Integer：记录 Constant。
   - Value：查询 definitions，命中则记录定义者编号。
   - 未命中 definitions，但属于 external_inputs：记录 ExternalInput。
   - 两者都没有：报告未定义使用，位置指向当前 operand。
   - 每个合法 Value 都添加一次 UseSite。
4. 所有 source 成功后，再登记当前 destination。
5. 全部成功后返回完整结果。

本轮遇到第一个分析错误即可返回。

注意：可以提前检查 destination 重名，但不能提前登记新定义。

## 7. 必须正确处理的情况

### 自引用

```text
%a = add %a, 1
```

如果 `%a` 没有前序定义、也不是外部输入，必须报错。

不能先登记左侧 `%a`，让右侧查到当前指令自己。

### 前向引用

```text
%b = mul %a, 2
%a = add %x, 1
```

第一条指令仍然报错，本轮要求使用前已经定义。

### 重复定义

```text
%a = add 1, 2
%a = mul 3, 4
```

第二条报错，不覆盖旧定义。

### 同一个值使用两次

```text
%a = add %x, 1
%b = mul %a, %a
```

`%a` 的 uses 必须包含 `(1, 0)` 和 `(1, 1)`。

### 外部输入

查不到定义的名字不能自动当成外部输入，必须显式声明。

### load、store、ret

| 指令 | 命名值的 def | 命名值的 use |
|---|---|---|
| `%a = load %ptr` | `%a` | `%ptr` |
| `store %a, %ptr` | 无 | `%a`、`%ptr` |
| `ret %a` | 无 | `%a` |

store 会修改内存，但本轮只记录命名值的关系。

无人使用的定义不报错，也不删除指令。

## 8. 验收测试

先完成三个代表性测试组：

| 测试组 | 检查内容 |
|---|---|
| 正常关系 | 第 2 节示例的 definitions、sources、uses 全部正确 |
| 重复使用 | 同一值作为两个 source 时记录两次 use；store/ret 没有 def |
| 语义错误 | 未定义使用、重复定义、外部输入重名；错误位置正确且 info 为空 |

错误例子：

- 无外部输入时，`%a = add %missing, 1`：错误指向 `%missing`，第 10 列。
- 重复定义 `%a`：错误指向第二次定义的 destination。
- 声明 `%x` 为外部输入后出现 `%x = add 1, 2`：错误指向 destination。

错误文字不要求逐字固定，但要说明问题和相关名字。

测试直接检查数据结构，不只观察打印结果。

## 9. 编译与运行

在已有 CMakeLists.txt 中追加：

```cmake
add_executable(def_use_test tests/def_use_test.cpp)
target_include_directories(def_use_test PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(def_use_test PRIVATE GTest::gtest_main)
gtest_discover_tests(def_use_test)
```

保留已有 `include(GoogleTest)`，不要重复添加。

在项目目录执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

若测试套件命名为 DefUseTest：

```bash
ctest --test-dir build -R DefUseTest --output-on-failure
```

沿用现有 C++20、Google Test 和 Sanitizer 配置。

## 10. 完成标准

- 能查询每个命名值的定义者和所有使用位置。
- 能查询每个操作数的来源。
- 能区分指令结果、外部输入、常量。
- 能正确诊断未定义使用和重复定义。
- 代表性测试通过，提交后进行 Review。