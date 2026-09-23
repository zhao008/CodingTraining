# C++ Scoreboard Simulator

## 1. 任务目标

使用 C++20 实现一个最小化的 GPU 指令发射模拟器，模拟：

* 单个 warp；
* 顺序发射；
* 每周期最多发射一条指令；
* 基于 Scoreboard 的 RAW 数据依赖等待。

这个任务同时练习：

1. GPU 指令依赖与 Scoreboard 的基本建模；
2. C++ 数据结构与函数设计；
3. `std::vector`、`std::unordered_map`、`std::string` 等标准库组件；
4. 测试、断言和格式化输出。

预计完成时间：30～45 分钟。

---

## 2. 模型范围

### 2.1 本次实现的机制

每条指令包含：

```text
opcode
destination register
source registers
execution latency
```

模拟器需要维护每个寄存器的 ready cycle。

### 2.2 发射规则

1. 指令必须按照程序顺序发射；

2. 每周期最多发射一条指令；

3. 所有源寄存器 ready 后，指令才能发射；

4. 指令在周期 `c` 发射，执行延迟为 `latency`，则目的寄存器在以下周期 ready：

   ```text
   c + latency
   ```

5. 允许消费者在源寄存器变为 ready 的同一周期发射；

6. 从未被程序写过的寄存器默认在周期 `0` ready；

7. 当前只考虑 RAW 依赖。

### 2.3 暂不考虑的机制

本次不模拟：

* WAR和WAW依赖；
* 寄存器Bank Conflict；
* Operand Reuse；
* 执行单元结构冲突；
* 多warp调度；
* Scoreboard slot数量；
* Cache和Memory latency；
* 分支与控制流；
* NVIDIA SASS控制字段。

---

## 3. 工程目录

```text
cpp/
└── 01_scoreboard_simulator/
    ├── README.md
    ├── main.cpp
    └── expected_output.txt
```

当前版本可以全部写在一个 `main.cpp` 中，不需要拆分复杂的头文件、测试目录或CMake工程。

---

## 4. 指令数据结构

使用以下结构描述一条指令：

```cpp
struct Instruction {
    std::string opcode;
    std::string dst;
    std::vector<std::string> srcs;
    int latency;
};
```

字段含义：

| 字段        | 含义              |
| --------- | --------------- |
| `opcode`  | 指令名称            |
| `dst`     | 目的寄存器           |
| `srcs`    | 源寄存器列表          |
| `latency` | 从发射到结果ready的周期数 |

---

## 5. 测试程序

使用下面四条指令作为第一个测试：

```cpp
std::vector<Instruction> program{
    {"IADD", "R1", {"R2", "R3"}, 4},
    {"FFMA", "R4", {"R1", "R5"}, 4},
    {"IADD", "R6", {"R7", "R8"}, 4},
    {"FFMA", "R9", {"R4", "R6"}, 4},
};
```

对应依赖关系：

```text
I0: IADD R1 <- R2, R3
           |
           | R1
           v
I1: FFMA R4 <- R1, R5
           |
           | R4
           v
I3: FFMA R9 <- R4, R6
           ^
           | R6
           |
I2: IADD R6 <- R7, R8
```

---

## 6. Scoreboard状态

使用哈希表保存每个寄存器的ready周期：

```cpp
std::unordered_map<std::string, int> reg_ready;
```

逻辑含义：

```text
reg_ready["R1"] = 4
```

表示：

> `R1`从周期4开始可以被后续指令读取。

如果某个寄存器不在 `reg_ready` 中，则认为它是程序的输入寄存器，并在周期0 ready。

---

## 7. C++标准库练习重点

| 标准库组件                | 本任务中的用途          |
| -------------------- | ---------------- |
| `std::string`        | 保存opcode和寄存器名    |
| `std::vector`        | 保存指令、源寄存器和调度结果   |
| `std::unordered_map` | 保存寄存器ready cycle |
| `std::max`           | 计算最晚的源寄存器ready周期 |
| 范围`for`              | 遍历指令和源寄存器        |
| `std::setw`          | 格式化输出结果表格        |
| `std::assert`        | 验证模拟结果           |

建议包含的头文件：

```cpp
#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
```

### 7.1 哈希表查询方式

本任务需要比较以下接口：

```cpp
reg_ready.find(reg);
reg_ready.contains(reg);
reg_ready.at(reg);
reg_ready[reg];
```

它们的主要区别是：

* `find()`：查询元素，找不到时返回 `end()`，不会修改容器；
* `contains()`：C++20接口，只判断元素是否存在，不会修改容器；
* `at()`：读取已经存在的元素，元素不存在时抛出异常；
* `operator[]`：元素不存在时会自动插入一个默认值。

查询源寄存器时，不建议直接写：

```cpp
int ready = reg_ready[reg];
```

因为这会把原本不存在的输入寄存器插入哈希表。

更合适的逻辑是：

```text
如果寄存器存在：
    读取其ready cycle
否则：
    认为它在周期0 ready
```

更新目的寄存器时可以使用：

```cpp
reg_ready[dst] = ready_cycle;
```

---

## 8. 调度结果数据结构

为每条指令保存一条调度结果：

```cpp
struct ScheduleResult {
    int instruction_id;
    int issue_cycle;
    int stall_cycles;
    int ready_cycle;
    std::vector<std::string> blocking_registers;
};
```

字段含义：

| 字段                   | 含义             |
| -------------------- | -------------- |
| `instruction_id`     | 指令在程序中的编号      |
| `issue_cycle`        | 实际发射周期         |
| `stall_cycles`       | 因数据依赖而额外等待的周期数 |
| `ready_cycle`        | 目的寄存器ready周期   |
| `blocking_registers` | 导致该指令等待的源寄存器   |

---

## 9. 核心函数

实现下面的函数：

```cpp
std::vector<ScheduleResult>
simulate(const std::vector<Instruction>& program);
```

注意参数使用：

```cpp
const std::vector<Instruction>&
```

原因是：

* `&`避免复制整个指令数组；
* `const`保证模拟器不会修改输入程序；
* 这是只读大型容器参数的常用传递方式。

---

## 10. 调度算法要求

对于每条指令，依次完成以下步骤。

### Step 1：计算无依赖时的最早发射周期

第一条指令：

```text
earliest_issue_cycle = 0
```

后续指令：

```text
earliest_issue_cycle = previous_issue_cycle + 1
```

### Step 2：读取所有源寄存器的ready周期

对于每个源寄存器：

```text
如果寄存器存在于Scoreboard：
    使用Scoreboard中的ready cycle
否则：
    ready cycle = 0
```

### Step 3：计算实际发射周期

```text
issue_cycle =
    max(
        earliest_issue_cycle,
        所有源寄存器的ready cycle
    )
```

### Step 4：计算stall周期

```text
stall_cycles =
    issue_cycle - earliest_issue_cycle
```

### Step 5：寻找blocking registers

如果某个源寄存器满足：

```text
source_ready_cycle > earliest_issue_cycle
```

则将它记录到 `blocking_registers` 中。

### Step 6：更新目的寄存器

```text
destination_ready_cycle =
    issue_cycle + latency
```

然后更新Scoreboard：

```text
reg_ready[dst] = destination_ready_cycle
```

---

## 11. 预期调度结果

| ID | 指令        | 最早可发射周期 | 实际发射周期 | Stall | Blocking registers | 结果ready |
| -: | --------- | ------: | -----: | ----: | ------------------ | ------: |
|  0 | `IADD R1` |       0 |      0 |     0 | 无                  |       4 |
|  1 | `FFMA R4` |       1 |      4 |     3 | `R1`               |       8 |
|  2 | `IADD R6` |       5 |      5 |     0 | 无                  |       9 |
|  3 | `FFMA R9` |       6 |      9 |     3 | `R4, R6`           |      13 |

正确的issue cycle序列是：

```text
0, 4, 5, 9
```

程序的最后一个结果在周期13 ready。

---

## 12. 输出要求

程序至少输出以下字段：

```text
ID
Opcode
Issue
Stall
Blocking Registers
Destination Ready
```

输出格式可以类似：

```text
ID  Opcode  Issue  Stall  Blocking  Ready
0   IADD    0      0      -         4
1   FFMA    4      3      R1        8
2   IADD    5      0      -         9
3   FFMA    9      3      R4,R6     13

Program completion cycle: 13
```

不要求输出格式完全一致，但数值必须正确。

---

## 13. 自动验证

从调度结果中提取所有issue cycle：

```cpp
std::vector<int> issue_cycles;
```

加入断言：

```cpp
const std::vector<int> expected{0, 4, 5, 9};
assert(issue_cycles == expected);
```

断言通过后，可以输出：

```text
All tests passed.
```

---

## 14. 编译方法

使用GCC：

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic \
    main.cpp -o scoreboard
```

运行：

```bash
./scoreboard
```

使用Clang：

```bash
clang++ -std=c++20 -O2 -Wall -Wextra -Wpedantic \
    main.cpp -o scoreboard
```

编译时应尽量做到：

```text
0 warnings
0 errors
```

---

## 15. 验收标准

完成版本应满足：

* [ ] 使用C++20；
* [ ] 使用 `Instruction` 描述指令；
* [ ] 使用 `std::vector` 保存程序；
* [ ] 使用 `std::unordered_map` 保存Scoreboard；
* [ ] `simulate()`通过常量引用接收程序；
* [ ] 没有硬编码四条指令的发射周期；
* [ ] 正确处理不存在于Scoreboard的输入寄存器；
* [ ] issue cycle结果为 `{0, 4, 5, 9}`；
* [ ] 最终完成周期为 `13`；
* [ ] 使用 `assert`完成自动验证；
* [ ] 编译时没有警告；
* [ ] 输出每条指令的调度结果。

---

## 16. 完成后需要回答的问题

在README底部补充自己的回答。

### 问题1

为什么 `simulate()` 的参数使用：

```cpp
const std::vector<Instruction>& program
```

而不是：

```cpp
std::vector<Instruction> program
```

### 问题2

为什么读取源寄存器状态时，不建议直接使用：

```cpp
reg_ready[reg]
```

### 问题3

为什么第三条指令可以在周期5发射，而不需要等待第二条指令执行完成？

### 问题4

第四条指令同时依赖 `R4` 和 `R6`，为什么最终要等到周期9？

---

## 17. 可选进阶任务

如果基础任务在30分钟内完成，可以增加依赖边输出：

```text
I0 -> I1: R1
I1 -> I3: R4
I2 -> I3: R6
```

也可以统计：

```text
Total instructions
Total stall cycles
Program completion cycle
IPC
```

其中可以先定义：

```text
IPC = instruction count / program completion cycles
```

本次模型下：

```text
instruction count = 4
program completion cycles = 13
```

因此：

```text
IPC = 4 / 13
```

这里的IPC只是当前简化模型中的指标，不等同于真实GPU硬件计数器报告的IPC。

---

## 18. 后续扩展方向

完成当前版本后，可以逐步增加：

1. 多warp调度；
2. Round-Robin
