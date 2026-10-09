# Mini IR：文本解析与静态分析练习

使用 C++20，把简单的 IR 文本逐步转换成可分析的指令表示。

工程始终放在 `cpp/02_mini_ir_parser/` 中，各阶段在现有代码上继续。

**当前进度：Lexer 已完成，下一步实现 Parser。** 每个模块可以分几次训练完成，完成后先 Review，再进入下一阶段。

## 1. 项目目标与阶段

| 阶段 | 输入与输出 | 状态 |
|---|---|---|
| Lexer | 一行文本 → Token 序列和词法错误 | 已完成 |
| Parser | 一行 Token → Instruction 或语法错误 | 当前任务 |
| Definition/Use 分析 | 指令列表 → 值的定义与使用关系 | 后续 |
| 依赖 DAG | 纯算术指令 → 数据依赖图 | 后续 |
| 工具整合 | 读取 `.ir` 文件 → 输出解析和分析结果 | 后续 |

项目终点是完成“读文件、解析、建立 def-use、输出依赖图”。

随后转入真正的 LLVM IR 分析，再逐步接触数据流分析、LLVM/MLIR Pass 和 Triton 编译链。

后续分析阶段将约定每个值只定义一次，外部输入显式提供。`load/store` 可以解析；指令重排涉及的内存依赖在后续单独讨论。

## 2. 工程文件

以下路径均相对于 `cpp/02_mini_ir_parser/`：

| 文件 | 用途 | 当前动作 |
|---|---|---|
| `README.md` | 项目规则、当前任务和验收方式 | 随阶段更新 |
| `Lexer.h` | Token、位置、词法错误和 Lexer | 继续复用 |
| `Parser.h` | Instruction、解析错误和 Parser | 本轮新增 |
| `main.cpp` | 程序入口，目前尚未接入完整处理流程 | 后续整合 |
| `tests/lexer_test.cpp` | 已有 Lexer 测试 | 保留 |
| `tests/parser_test.cpp` | Parser 测试 | 本轮新增 |
| `CMakeLists.txt` | C++20、Google Test、Sanitizer 配置 | 加入 Parser 测试目标 |

## 3. 输入示例

每行最多包含一条指令：

```text
%r1 = add %r2, %r3
%r4 = mul %r1, %r5
%r6 = load %ptr
store %r4, %ptr
ret %r4
%r7 = add %r4, 42
```

空格和 Tab 都是空白；`#` 开始行注释。标点两侧不要求有空格：

```text
%r1=add %r2,42  # 等价于带空格的写法
```

本阶段允许出现尚未定义的 `%r2`、`%ptr` 等名字。是否已经定义属于后续语义分析。

## 4. 已确定的词法规则

这里的字母指 ASCII `A–Z`、`a–z`，数字指 `0–9`。

| Token 类型 | 规则 | 示例 |
|---|---|---|
| `Identifier` | 字母或 `_` 开头；后续允许字母、数字、`_`、`.` | `add`、`ase_1`、`add.int32` |
| `Value` | `%` 后必须以字母开头；后续允许字母、数字、`_` | `%r1`、`%ptr`、`%tmp_0` |
| `Integer` | 一个或多个十进制数字 | `0`、`42`、`1000` |
| `Equal` | 单个 `=` | `=` |
| `Comma` | 单个 `,` | `,` |

约定：

- 单独的 `%`、`%0`、`%_tmp` 报词法错误。
- `1000aad` 拆成 `Integer("1000")` 和 `Identifier("aad")`，由 Parser 检查组合是否合法。
- `add@` 先得到 `Identifier("add")`，然后在 `@` 处报错。
- 遇到第一个词法错误就停止本行扫描，保留此前已经识别的 Token。
- 行号、列号从 1 开始；当前 Tab 按一个字符计列，不计算终端显示宽度。
- 当前没有负数、浮点数、多进制数、字符串、括号和跨行语句的语法支持。

`add.int32` 是合法的 Identifier，但本轮 Parser 只支持下文列出的五种 opcode。词法上能识别一个名字，不等于语法上支持这条指令。

## 5. 现有 Lexer 接口

现有数据结构在 `Lexer.h` 中，Parser 直接复用，不需要重新定义：

```cpp
struct SourceLocation {
    size_t line;
    size_t column;
};

struct Token {
    TokenKind kind;
    std::string lexeme;
    SourceLocation location;
};

struct LexError {
    std::string message;
    SourceLocation location;
};

struct TokenizeResult {
    std::vector<Token> tokens;
    std::optional<LexError> error;
};
```

Lexer 当前是一个保存各行结果的类，其公开接口为：

```cpp
void tokenize_line(std::string_view line, size_t line_number);

const std::map<size_t, TokenizeResult>& results() const;
```

调用方式：

```cpp
Lexer lexer;
lexer.tokenize_line("%r1 = add %r2, 42", 1);

const auto& result = lexer.results().at(1);
```

每次调用会先清空该行的旧 Token 和错误，再重新扫描；其他行保持原样。空行和纯注释行得到空 Token 列表，无错误。

输入使用 `std::string_view`，Token 的 `lexeme` 使用 `std::string` 拥有文本。Lexer 不需要长期保存输入视图。

**只有在 `result.error` 为空时，才把 `result.tokens` 交给 Parser。** 词法出错时，已有 Token 只是一段不完整的输入。

## 6. 当前任务：实现 Parser

### 6.1 目标

把这一行的 Token：

```text
Value("%r1"), Equal("="), Identifier("add"),
Value("%r2"), Comma(","), Integer("42")
```

组织成一条指令：

| 字段 | 内容 |
|---|---|
| `destination` | `%r1` 对应的 Token |
| `opcode` | `add` 对应的 Token |
| `operands[0]` | `%r2` 对应的 Token |
| `operands[1]` | `42` 对应的 Token |

`=` 和 `,` 用于检查语法，不放入最终操作数列表。

### 6.2 数据结构和接口

在 `Parser.h` 中包含 `Lexer.h`，定义：

```cpp
#pragma once

#include "Lexer.h"

#include <optional>
#include <string>
#include <vector>

struct Instruction {
    std::optional<Token> destination;
    Token opcode;
    std::vector<Token> operands;
};

struct ParseError {
    std::string message;
    SourceLocation location;
};

struct ParseResult {
    std::optional<Instruction> instruction;
    std::optional<ParseError> error;
};

// 在 Parser.h 中直接定义这个自由函数时，保留 inline。
inline ParseResult parse_instruction(
    const std::vector<Token>& tokens);
```

本轮先复用 Token 保存文本、类别和位置；整数继续保存原始文本，无需转换数值。

返回状态约定：

| 输入结果 | `instruction` | `error` |
|---|---|---|
| 成功解析一条指令 | 有值 | 无值 |
| 解析失败 | 无值 | 有值 |
| 空 Token 列表 | 无值 | 无值 |

不要同时返回有效指令和错误。

### 6.3 本轮支持的指令

| Opcode | 目的值 | 操作数数量 | 示例 |
|---|---|---:|---|
| `add` | 必须有 | 2 | `%r1 = add %r2, 42` |
| `mul` | 必须有 | 2 | `%r1 = mul %r2, 3` |
| `load` | 必须有 | 1 | `%r1 = load %ptr` |
| `store` | 不能有 | 2 | `store %r1, %ptr` |
| `ret` | 不能有 | 1 | `ret %r1` |

Opcode 精确匹配上表的小写名称，未知 opcode 返回错误。

每个操作数暂时统一接受 `Value` 或 `Integer`。例如 load 地址的类型是否合理、值有没有定义，后续再由语义分析处理。

### 6.4 解析流程

使用索引 `pos` 从左到右读取 Token：

1. 空列表直接返回“没有指令”。
2. 如果第一个 Token 是 `Value`，记录目的值，随后必须有 `Equal`。
3. 接下来必须是 `Identifier`，记录为 opcode。
4. 读取操作数；操作数之间必须有逗号，逗号后必须有操作数。
5. 检查 opcode 是否支持、目的值是否符合要求、操作数数量是否正确。
6. 成功时必须消费完整行的 Token，不能忽略尾部内容。

访问 `tokens[pos]` 前，先确认 `pos < tokens.size()`。

先把过程写清楚；出现重复逻辑后，再考虑提取 `peek()`、`consume()`、`expect()`。

### 6.5 错误诊断

错误信息要能说明问题，文字不要求逐字固定。例如：

| 输入 | 应诊断的问题 |
|---|---|
| `%r1 add %r2, 42` | 目的值后缺少 `=` |
| `%r1 = add %r2 42` | 操作数之间缺少逗号 |
| `%r1 = add %r2,` | 逗号后缺少操作数 |
| `%r1 = add %r2` | `add` 需要两个操作数 |
| `add %r2, 42` | `add` 缺少目的值 |
| `%r1 = ret %r2` | `ret` 不允许目的值 |
| `%r1 = unknown %r2` | 不支持的 opcode |
| `ret %r1, 42` | `ret` 操作数过多 |

这些例子用于说明规则，不要求每一项都新增独立测试。

错误位置约定：

- 当前 Token 不符合预期：指向当前 Token。
- 输入已经结束但还缺内容：指向最后一个 Token 的末尾之后，即其列号加上 `lexeme.size()`。
- 未知 opcode、指令缺少目的值或操作数数量错误：可以指向 opcode。
- 不该出现的目的值：可以指向目的值。

当前接口只接收 Token，没有行尾空白和注释信息，因此缺内容的位置按最后一个 Token 计算即可。

## 7. 实现顺序与验收

本轮交付：

- `Parser.h`
- `tests/parser_test.cpp`
- 对应的 `CMakeLists.txt` 修改

可以分几次完成：

1. 定义数据结构，打通 `%r1 = add %r2, 42`。
2. 推广到五种指令，处理有目的值和无目的值两种形式。
3. 补上错误返回，运行代表性测试，再进行 Review。

先在 `tests/parser_test.cpp` 中保留三个代表性测试：

| 输入 | 检查内容 |
|---|---|
| `%r1 = add %r2, 42` | 成功；目的值、opcode、两个操作数及其类别正确 |
| `store %r1, %ptr` | 成功；没有目的值，两个操作数正确 |
| `%r1 = add %r2 42` | 失败；提示缺逗号，位置指向 `42`，即第 15 列 |

测试可以先调用 Lexer，确认没有词法错误，再调用 Parser。

保留原有 Lexer 测试；后续发现实际 Bug 时，再补对应回归用例。

三个测试是起步验收，不代表覆盖所有非法输入。Review 时结合代码检查五种指令及上述语法规则。

## 8. 编译与运行测试

工程使用 C++20、CMake 和 Google Test。

现有 CMake 配置会从 `~/.local` 等位置寻找 GTest，需要先确保本机已安装并可被 CMake 找到。

在项目目录执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Debug 配置默认启用 ASan/UBSan。需要关闭时，在配置命令中加入：

```text
-DENABLE_SANITIZERS=OFF
```

写好 `tests/parser_test.cpp` 后，在现有 `CMakeLists.txt` 的 `include(GoogleTest)` 之后加入：

```cmake
add_executable(parser_test tests/parser_test.cpp)
target_include_directories(parser_test PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(parser_test PRIVATE GTest::gtest_main)
gtest_discover_tests(parser_test)
```

只运行 Parser 测试时，若测试套件命名为 `ParserTest`，可以执行：

```bash
ctest --test-dir build -R ParserTest --output-on-failure
```

当前 `main.cpp` 仍是空入口，功能验证通过测试程序进行。

## 9. 本阶段需要理解的知识

| 内容 | 在本项目中的用途 |
|---|---|
| `std::optional` | 表示可选目的值、成功结果和错误 |
| `const std::vector<Token>&` | 只读访问 Lexer 输出，避免复制整组 Token |
| Token 游标 | 读取当前位置、判断、消费、推进 |
| 错误传播 | Lexer 错误先处理，Parser 错误带位置返回 |
| 结构体与接口 | 将识别过程和指令表示分开 |
| 少量代表性测试 | 验证主要路径和真实错误 |

完成后能说明三个问题即可：

1. Lexer 与 Parser 各负责什么？
2. 为什么 `destination` 是可选的？
3. 为什么 `%r2` 是否定义应该留给后续分析？