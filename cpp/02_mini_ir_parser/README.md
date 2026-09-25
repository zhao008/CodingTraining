# Mini IR Tokenizer：今日编译器训练实验

## 1. 今日目标

使用 **C++20** 实现一个最小但完整的词法分析器（Tokenizer/Lexer），把一行 Mini IR 文本转换成 Token 序列。

今天只完成编译器前端的第一步：

```text
源代码文本 → Token序列
```

不继续扩展 Scoreboard，也不模拟周期、发射、Pipeline 或 GPU Core。

预计时间：**60～90分钟**。如果当天没有完成，可以保留完整要求，在下一次训练继续，不需要缩减项目。

---

## 2. 为什么做这个实验

上一个 Scoreboard 项目已经练习了：

- `std::vector`；
- `std::unordered_map`；
- 结构体；
- 迭代器；
- 格式化输出；
- Sanitizer脚本。

本实验开始进入编译器方向，同时针对上一个项目暴露出的几个编程问题进行训练：

- 使用只读视图，避免不必要的字符串和容器复制；
- 不再假设输入永远只有两个操作数；
- 明确表示成功与错误状态；
- 用自动化测试覆盖正常输入和异常输入；
- 将源代码位置保留下来，为后续Parser错误诊断做准备。

---

## 3. 工程目录

建议建立：

```text
CodingTraining/
└── cpp/
    └── 02_mini_ir_parser/
        ├── README.md
        └── main.cpp
```

将本文件内容放入：

```text
cpp/02_mini_ir_parser/README.md
```

今天所有代码可以写在一个 `main.cpp` 中，不需要CMake、头文件或第三方测试框架。

---

## 4. 输入语言

今天需要支持以下形式的Mini IR：

```text
%r1 = add %r2, %r3
%r4 = mul %r1, %r5
%r6 = load %ptr
store %r4, %ptr
ret %r4
%r7 = add %r4, 42
```

允许使用 `#` 表示行注释：

```text
%r1 = add %r2, %r3   # calculate r1
```

`#`及其后面的内容不产生Token。

---

## 5. Token规则

本实验支持五种Token：

| Token类型 | 示例 | 规则 |
|---|---|---|
| `Identifier` | `add`、`load`、`ret` | 字母或下划线开头，后续可包含字母、数字、下划线和`.` |
| `Value` | `%r1`、`%ptr`、`%0` | `%`开头，后面至少有一个字母、数字或下划线 |
| `Integer` | `0`、`42`、`7168` | 一个或多个十进制数字 |
| `Equal` | `=` | 单字符Token |
| `Comma` | `,` | 单字符Token |

空格和Tab只用于分隔Token，不产生Token。

遇到其他字符时必须返回词法错误。例如：

```text
@r1 = add %r2, %r3
```

应报告第1列出现非法字符 `@`。

### 本次不支持

- 浮点数字；
- 负数；
- 字符串字面量；
- 类型，如 `i32`、`f32`；
- 括号和方括号；
- 多行语句；
- LLVM IR的完整语法。

---

## 6. 数据结构要求

### 6.1 Token类型

```cpp
enum class TokenKind {
    Identifier,
    Value,
    Integer,
    Equal,
    Comma,
};
```

不要使用字符串表示Token类型。`enum class`可以避免把Token类型和普通整数混用。

### 6.2 源代码位置

```cpp
struct SourceLocation {
    int line;
    int column;
};
```

行号和列号均从 **1** 开始。

### 6.3 Token

```cpp
struct Token {
    TokenKind kind;
    std::string lexeme;
    SourceLocation location;
};
```

例如：

```text
输入：%r1 = add %r2, %r3
Token：Value, "%r1", line=1, column=1
```

### 6.4 错误

```cpp
struct LexError {
    std::string message;
    SourceLocation location;
};
```

### 6.5 返回结果

由于C++20没有标准库 `std::expected`，本实验使用：

```cpp
struct TokenizeResult {
    std::vector<Token> tokens;
    std::optional<LexError> error;
};
```

约定：

- `error == std::nullopt`：词法分析成功；
- `error.has_value()`：遇到第一个非法字符，停止分析并返回错误；
- 出错前已经识别出的Token可以保留在 `tokens` 中。

---

## 7. 核心函数

实现：

```cpp
TokenizeResult tokenize_line(std::string_view line, int line_number);
```

这里使用 `std::string_view`，因为Tokenizer只需要读取输入，不需要复制或修改整行字符串。

Token中的 `lexeme`仍使用 `std::string`，因为返回结果需要拥有自己的字符数据，不能依赖输入字符串的生命周期。

还需要实现：

```cpp
std::string_view token_kind_name(TokenKind kind);

void print_tokens(const std::vector<Token>& tokens);
```

`token_kind_name()`建议使用 `switch`，不要建立全局可变哈希表。

---

## 8. Tokenizer算法

使用索引 `i` 从左向右扫描字符串：

```text
i = 0

while i < line.size():
    如果是空白：
        跳过

    如果是 #：
        本行结束

    如果是 %：
        扫描Value

    如果是字母或下划线：
        扫描Identifier

    如果是数字：
        扫描Integer

    如果是 = 或 ,：
        生成单字符Token

    否则：
        返回LexError
```

扫描一个多字符Token时：

1. 记录 `start = i`；
2. 移动 `i`，直到字符不再满足规则；
3. 使用 `line.substr(start, i - start)`取得视图；
4. 构造 `std::string`保存到Token中；
5. Token列号为 `start + 1`。

### 字符分类注意事项

使用 `<cctype>`中的函数时，不要直接把可能为负值的 `char`传入：

```cpp
std::isspace(static_cast<unsigned char>(ch))
```

同样适用于：

```cpp
std::isalpha(...)
std::isdigit(...)
std::isalnum(...)
```

这是今天需要理解的一个C++细节。

---

## 9. 输出格式

输入：

```text
%r1 = add %r2, %r3
```

建议输出：

```text
Line  Col  Kind        Lexeme
1     1    Value       %r1
1     5    Equal       =
1     7    Identifier  add
1     11   Value       %r2
1     14   Comma       ,
1     16   Value       %r3
```

可以使用：

```cpp
std::left
std::setw(...)
```

---

## 10. 必测用例

### Test 1：普通二元指令

```text
%r1 = add %r2, %r3
```

预期Token：

```text
Value("%r1")
Equal("=")
Identifier("add")
Value("%r2")
Comma(",")
Value("%r3")
```

预期列号：

```text
1, 5, 7, 11, 14, 16
```

### Test 2：无目的寄存器指令

```text
store %r4, %ptr
```

预期Token类型：

```text
Identifier, Value, Comma, Value
```

### Test 3：一个操作数

```text
ret %r4
```

预期Token数量为2。

### Test 4：整数字面量

```text
%r7 = add %r4, 42
```

最后一个Token必须为：

```text
Integer("42")
```

### Test 5：空白和注释

```text
   ret %r4   # return value
```

只产生两个Token，第一个Token的列号为4。

### Test 6：空行

```text

```

返回空Token数组，不产生错误。

### Test 7：非法字符

```text
@r1 = add %r2, %r3
```

要求：

```text
error.has_value() == true
error.location.line == 1
error.location.column == 1
```

### Test 8：不完整Value

```text
% = add %r1, %r2
```

单独的 `%`不是合法Value，应在第1列报告错误。

---

## 11. 自动验证要求

至少使用 `assert`验证：

- Token数量；
- Token类型；
- Token文本；
- 行号和列号；
- 合法输入没有错误；
- 非法输入产生错误。

可以编写辅助函数降低测试重复：

```cpp
void expect_token(
    const Token& token,
    TokenKind expected_kind,
    std::string_view expected_lexeme,
    int expected_line,
    int expected_column);
```

测试通过后输出：

```text
All tokenizer tests passed.
```

---

## 12. C++标准库重点

今天重点理解：

| 组件 | 用途 |
|---|---|
| `std::string_view` | 只读查看输入行，避免复制 |
| `std::string` | 让Token拥有自己的文本 |
| `std::vector` | 保存任意数量的Token |
| `std::optional` | 表示“可能存在”的词法错误 |
| `enum class` | 类型安全地表示Token类别 |
| `<cctype>` | 字符分类 |
| `std::setw` | 打印Token表格 |
| `assert` | 自动验证行为 |

今天不要使用：

- `using namespace std;`
- 裸指针；
- `new/delete`；
- 正则表达式；
- 第三方Parser库。

---

## 13. 时间安排

### 0～10分钟：建立类型

完成：

- `TokenKind`；
- `SourceLocation`；
- `Token`；
- `LexError`；
- `TokenizeResult`。

### 10～40分钟：实现Tokenizer

先完成：

- 空白；
- `%value`；
- Identifier；
- `=`和`,`；
- Integer；
- 注释和错误。

### 40～65分钟：加入测试

优先完成Test 1、4、7和8，再补其余用例。

### 65～80分钟：输出与整理

- 格式化打印Token；
- 修正编译警告；
- 运行Sanitizer；
- 整理变量和函数命名。

### 80～90分钟：复盘

回答README末尾的四个问题。

---

## 14. 编译与运行

Debug版本：

```bash
g++ -std=c++20 -g -O0 \
    -Wall -Wextra -Wpedantic \
    -fsanitize=address,undefined \
    main.cpp -o mini_ir_tokenizer

./mini_ir_tokenizer
```

Release版本：

```bash
g++ -std=c++20 -O2 \
    -Wall -Wextra -Wpedantic \
    main.cpp -o mini_ir_tokenizer

./mini_ir_tokenizer
```

目标：

```text
0 errors
0 warnings
All tokenizer tests passed.
```

---

## 15. 今日验收标准

- [ ] 建立 `cpp/02_mini_ir_parser/`；
- [ ] 使用C++20；
- [ ] 使用 `enum class TokenKind`；
- [ ] 使用 `std::string_view`接收输入行；
- [ ] 使用 `std::optional<LexError>`表示错误；
- [ ] 支持Identifier、Value、Integer、`=`和`,`；
- [ ] 忽略空白和 `#`注释；
- [ ] Token保存从1开始的行号、列号；
- [ ] 非法字符返回准确位置；
- [ ] 单独的 `%`返回错误；
- [ ] 至少覆盖8个测试用例；
- [ ] 不假设固定操作数数量；
- [ ] 编译无警告；
- [ ] AddressSanitizer和UBSan无错误。

---

## 16. 今天不要继续做的内容

完成Tokenizer后即停止。今天不实现：

- Parser；
- `Instruction` AST；
- def-use表；
- CFG；
- LLVM API；
- 文件级多行解析；
- 优化Pass。

这些会在后续阶段逐步加入。

---

## 17. 完成后回答

### 问题1

为什么核心函数接收 `std::string_view`，而Token的 `lexeme`使用 `std::string`？

### 问题2

为什么使用 `std::optional<LexError>`，而不是使用一个空字符串表示没有错误？

### 问题3

为什么调用 `std::isspace`、`std::isalpha`时应先把 `char`转换成 `unsigned char`？

### 问题4

Tokenizer和Parser分别解决什么问题？为什么今天不直接把文本解析成 `Instruction`？

---

## 18. 提交内容

完成后提交：

```text
cpp/02_mini_ir_parser/README.md
cpp/02_mini_ir_parser/main.cpp
```

建议Commit Message：

```text
feat: add mini IR tokenizer
```

提交后把GitHub链接发给我。下一步会先做Code Review，再决定是补充测试，还是进入Parser阶段。
