# 07 · LLVM Loop Structure Report Pass

## 1. 本轮核心目标

实现一个只读的 LLVM Function Pass：复用 LLVM 的 `LoopInfo`，输出函数中的循环数量，以及每个循环的 header、嵌套深度和基本块数量。

- 新目录：`cpp/07_llvm_loop_analysis/`。
- 保留 06，不修改上一轮的实现。
- 复用 06 已经跑通的 CMake、插件注册和 FileCheck 工具。
- 不自己实现循环识别算法，不写新的模拟器。
- 本轮重点：分析结果获取、树形遍历、确定的输出契约、自动化测试。

60 分钟完成最小闭环，90 分钟再做扩展。

## 2. 为什么值得做

06 练的是“安全修改 IR”，07 练的是“复用编译器已有分析，解释程序结构”。

后面研究 GEMM 的 K 循环、软件流水、Triton lowering 时，需要先识别循环结构。但本轮不判断性能瓶颈，不推导迭代次数，也不估计周期。

软件工程训练点：

1. 使用已有 API，而不是重复实现分析算法。
2. 用小函数拆开统计、递归遍历和输出。
3. 写能发现漏报、重复报告和计数错误的测试。
4. 区分静态代码结构和动态执行行为。

## 3. 时间安排与边界

| 时间 | 内容 | 最小交付物 |
| --- | --- | --- |
| 0–10 分钟 | 阅读最少接口，确定输出格式 | 理解 LoopInfo 与 Loop 的关系 |
| 10–40 分钟 | 复用插件框架，实现报告 | 可加载的 LoopReport 插件 |
| 40–55 分钟 | 跑三个手写 IR 测试并定位问题 | FileCheck 全通过 |
| 55–60 分钟 | 写 notes，提交 | 一条复现命令和实际报告 |
| 60–90 分钟，可选 | 统计循环内普通 load/store 的静态条数 | 新测试和计数说明 |

只有 30 分钟时：

- 5 分钟理解接口。
- 20 分钟实现单层循环报告。
- 5 分钟验证与 commit。

本轮不做：

- 自己实现 dominator、SCC 或循环发现算法。
- 修改 CFG、展开循环、循环外提或其他优化。
- 使用 ScalarEvolution 推导迭代次数。
- 推导 GPU 动态指令数、HBM 流量或执行周期。
- 接入 CUDA、Triton、MLIR 或大型真实工程。
- 为三个测试搭建通用分析框架。

## 4. 最少必要阅读

使用与你本地 LLVM 安装匹配的源码版本。下面源码链接指向 release/23.x，不要求重新编译 LLVM。

### 4.1 循环术语

文档：

https://llvm.org/docs/LoopTerminology.html

只读：

- Terminology：理解 header、latch、backedge。
- LoopInfo：理解顶层循环和子循环。

先记住：

- 顶层循环深度是 1，直接嵌套的内层循环深度是 2。
- 一个函数可以没有循环，也可以有多个并列的顶层循环。
- 遍历 LoopInfo 得到的是顶层循环，仍需递归遍历子循环。
- LoopInfo 不是任意 CFG 环的通用检测器；本轮只测试可达的、可归约的自然循环。

### 4.2 精确源码位置

仓库：

https://github.com/llvm/llvm-project

文件一：

`llvm/include/llvm/Analysis/LoopInfo.h`

https://github.com/llvm/llvm-project/blob/release/23.x/llvm/include/llvm/Analysis/LoopInfo.h

只看：

- `LoopAnalysis`
- `LoopInfo`

目的：知道如何获取函数的循环分析结果。

文件二：

`llvm/include/llvm/Support/GenericLoopInfo.h`

https://github.com/llvm/llvm-project/blob/release/23.x/llvm/include/llvm/Support/GenericLoopInfo.h

只看：

- `LoopBase::getHeader()`
- `LoopBase::getLoopDepth()`
- `LoopBase::getNumBlocks()`
- `LoopBase::getSubLoops()`
- 扩展时再看 `LoopBase::blocks()`

目的：访问循环结构。

不读整个 LoopInfo 实现，不读优化 Pass，不要求理解所有模板。

## 5. 文件组织

在仓库根目录执行：

```bash
mkdir -p cpp/07_llvm_loop_analysis/tests
```

本目录最终包含：

| 文件 | 要完成的工作 |
| --- | --- |
| README.md | 保存本说明 |
| CMakeLists.txt | 复用 06，改 target 和源文件名 |
| LoopReport.h | 声明 LoopReportPass |
| LoopReport.cpp | 获取分析、遍历循环、打印报告 |
| Plugin.cpp | 注册 loop-report |
| run_tests.sh | 配置、构建、运行 opt、检查报告 |
| tests/no_loop.ll | 无循环 |
| tests/single_loop.ll | 一个单层循环 |
| tests/nested_loops.ll | 两层嵌套循环 |
| notes.md | 记录命令、输出、卡点与复盘 |

不提交：

- `build/`
- 生成的 `.so`
- 构建缓存
- 新复制的 LLVM 工具链

无需增加第三方依赖。

## 6. 输出契约：先确定，再实现

注册管线名：

```text
loop-report
```

对每个有函数体的函数输出：

```text
function=<函数名> loops=<包含嵌套循环的总数量>
loop header=<基本块操作数形式> depth=<深度> blocks=<基本块数量>
```

例如：

```text
function=nested loops=2
loop header=%outer.header depth=1 blocks=5
loop header=%inner.header depth=2 blocks=2
```

要求：

1. 先输出函数汇总，再输出该函数的循环。
2. 每个循环只输出一次。
3. 对每棵循环树，先输出父循环，再递归输出子循环。
4. 多个顶层循环按 LoopInfo 的遍历顺序处理，不额外承诺按源码顺序排序。
5. header 使用 `BasicBlock::printAsOperand(stream, false)` 输出，不要手动拼接百分号。
6. 报告全部写到 `llvm::errs()`，不要和输出 IR 混合。
7. 无循环时仍输出 function 行，`loops=0`，不输出 loop 行。
8. 跳过函数声明，不要求为 declaration 输出报告。

计数口径：

- `loops`：Loop 对象总数，不是最大深度，也不只是顶层循环数量。
- `blocks`：调用当前循环的 `getNumBlocks()`。
- 外层循环的基本块集合包含内层循环的基本块。
- 不要把各循环的 blocks 相加，当作函数基本块总数。
- 不保证报告源语言中所有逻辑循环或 CFG 中所有环。

本轮手写测试的函数名和循环 header 都命名。未命名 header 仍应通过 `printAsOperand()` 正常输出，但不强制增加单独测试。

## 7. 核心接口与实现任务

### 7.1 Pass 入口

沿用 06 的 Function Pass 结构，类名改为 `LoopReportPass`：

```cpp
llvm::PreservedAnalyses run(
    llvm::Function& function,
    llvm::FunctionAnalysisManager& analysis_manager);
```

关键 include：

```cpp
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Support/raw_ostream.h"
```

获取分析结果：

```cpp
auto& loop_info =
    analysis_manager.getResult<llvm::LoopAnalysis>(function);
```

这里的类型关系是：

- `LoopAnalysis`：向 AnalysisManager 请求的分析类型。
- `LoopInfo`：分析返回的结果对象。
- `Loop`：一个具体的循环。

本轮是“消费已有分析结果的报告 Pass”，不用再实现或注册一套自己的 Analysis。

### 7.2 建议拆分

可以使用两个很小的辅助函数，不强制创建复杂的数据模型：

```cpp
unsigned countLoops(const llvm::Loop& loop);

void printLoop(
    const llvm::Loop& loop,
    llvm::raw_ostream& output);
```

由你实现：

1. `countLoops()`：计算当前循环和全部子循环的数量。
2. `printLoop()`：输出当前循环，再递归输出子循环。
3. `run()`：获取 LoopInfo，计算函数汇总，打印所有循环。
4. 只读，不调用修改 IR 的 API。
5. 最终返回 `PreservedAnalyses::all()`。

允许统计和输出各遍历一次，不为这点开销过早优化。

### 7.3 插件与构建

从 06 复用框架，修改：

- CMake 项目名：`llvm_loop_analysis`
- target：`LoopReport`
- 源文件：`LoopReport.cpp`、`Plugin.cpp`
- 插件显示名：`LoopReport`
- 管线解析名称：`loop-report`
- 注册的类：`training::LoopReportPass`
- 输出库名：`LoopReport.so`

沿用当前 LLVM 对应的 Pass mixin 和插件头文件，不照搬旧版本教程切换 API。

不重写 `llvmGetPassPluginInfo()` 的基本框架，不为 LoopInfo 手动添加静态 LLVM 库链接。

## 8. 三个最小测试

这些是测试输入和验收断言，不是 Pass 实现，可以直接复制。

### 8.1 tests/no_loop.ll

```llvm
; CHECK: function=no_loop loops=0
; CHECK-NOT: loop header=

define i32 @no_loop(i32 %x) {
entry:
  %y = add i32 %x, 1
  ret i32 %y
}
```

### 8.2 tests/single_loop.ll

```llvm
; CHECK: function=single_loop loops=1
; CHECK-NEXT: loop header=%header depth=1 blocks=2
; CHECK-NOT: loop header=

define void @single_loop(i32 %n) {
entry:
  br label %header

header:
  %i = phi i32 [ 0, %entry ], [ %i.next, %body ]
  %continue = icmp slt i32 %i, %n
  br i1 %continue, label %body, label %exit

body:
  %i.next = add i32 %i, 1
  br label %header

exit:
  ret void
}
```

解释：

- header 和 body 在循环内。
- entry、exit 不在循环内。
- 因此 `blocks=2`。

### 8.3 tests/nested_loops.ll

```llvm
; CHECK: function=nested loops=2
; CHECK-NEXT: loop header=%outer.header depth=1 blocks=5
; CHECK-NEXT: loop header=%inner.header depth=2 blocks=2
; CHECK-NOT: loop header=

define void @nested(i32 %n, i32 %m) {
entry:
  br label %outer.header

outer.header:
  %i = phi i32 [ 0, %entry ], [ %i.next, %outer.latch ]
  %outer.continue = icmp slt i32 %i, %n
  br i1 %outer.continue, label %inner.preheader, label %exit

inner.preheader:
  br label %inner.header

inner.header:
  %j = phi i32 [ 0, %inner.preheader ], [ %j.next, %inner.body ]
  %inner.continue = icmp slt i32 %j, %m
  br i1 %inner.continue, label %inner.body, label %outer.latch

inner.body:
  %j.next = add i32 %j, 1
  br label %inner.header

outer.latch:
  %i.next = add i32 %i, 1
  br label %outer.header

exit:
  ret void
}
```

外层循环的五个基本块：

- outer.header
- inner.preheader
- inner.header
- inner.body
- outer.latch

内层循环只有：

- inner.header
- inner.body

注意：inner.preheader 虽然不属于内层循环，却属于外层循环。

## 9. 构建与运行

以下按 Linux、仓库中已有 LLVM 23.1.3 和 Python FileCheck 的布局编写。

在仓库根目录执行。工具路径不同则调整对应变量。

### 9.1 配置工具与构建

```bash
REPO_DIR="$PWD"
PROJECT_DIR="$REPO_DIR/cpp/07_llvm_loop_analysis"

LLVM_CONFIG="$REPO_DIR/tool/llvm/LLVM-23.1.3-Linux-X64/bin/llvm-config"
LLVM_BINDIR="$("$LLVM_CONFIG" --bindir)"
OPT="$LLVM_BINDIR/opt"
FILECHECK="$REPO_DIR/tool/filecheck/FileCheck"

cmake -S "$PROJECT_DIR" -B "$PROJECT_DIR/build" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DLLVM_DIR="$("$LLVM_CONFIG" --cmakedir)" \
  -DCMAKE_PREFIX_PATH="$REPO_DIR/tool/llvm/deps/zstd/usr"

cmake --build "$PROJECT_DIR/build" -j
```

同一套 LLVM 提供构建配置与 opt。

重新运行时也执行配置命令。如果切换 LLVM 安装，建议使用独立的构建目录。

### 9.2 先运行一个测试，观察原始报告

```bash
"$OPT" \
  -load-pass-plugin="$PROJECT_DIR/build/LoopReport.so" \
  -passes='function(loop-report)' \
  -verify-each -disable-output \
  "$PROJECT_DIR/tests/nested_loops.ll" \
  2> "$PROJECT_DIR/build/nested_loops.report.txt"

"$FILECHECK" "$PROJECT_DIR/tests/nested_loops.ll" \
  < "$PROJECT_DIR/build/nested_loops.report.txt"
```

注意：报告在 stderr，必须使用 `2>`，不能只用 `>`。

### 9.3 写 run_tests.sh

把上面的配置、构建与运行流程放进脚本，并实现：

- 使用 `set -euo pipefail`。
- 从脚本所在位置确定项目目录和仓库目录，不依赖调用者当前目录。
- 支持 `LLVM_CONFIG`、`OPT`、`FILECHECK` 环境变量覆盖。
- 未指定 OPT 时，从所选 LLVM_CONFIG 的 `--bindir` 得到默认 OPT。
- 遍历 `tests/*.ll`。
- 每个输入生成独立的 `build/<name>.report.txt`。
- opt 失败或 FileCheck 失败时，脚本返回非零。
- 成功时打印各测试 PASS 和最终汇总。
- 失败时保留报告，便于定位。
- 不自动下载工具，不调用 sudo。

先让单个命令跑通，再封装脚本。

## 10. Debug 与只读性验证

### 10.1 与 LLVM 内置报告对照

```bash
"$OPT" \
  -passes='print<loops>' -disable-output \
  "$PROJECT_DIR/tests/nested_loops.ll" \
  2> "$PROJECT_DIR/build/nested_loops.reference.txt"
```

对照：

- header
- 深度
- 基本块集合

不要求两份报告文本格式相同。

不要为了“修复预期”额外加入 `-O2`、`loop-simplify`、循环展开或其他会修改测试结构的 Pass。

### 10.2 确认没有修改 IR

不要直接 diff 原始输入与打印输出：LLVM 的格式化和注释处理可能让文本不同。

使用相同输入、相同 opt 分别生成规范化文本：

```bash
"$OPT" -S "$PROJECT_DIR/tests/nested_loops.ll" \
  -o "$PROJECT_DIR/build/nested_loops.before.ll"

"$OPT" \
  -load-pass-plugin="$PROJECT_DIR/build/LoopReport.so" \
  -passes='function(loop-report)' \
  -verify-each -S \
  "$PROJECT_DIR/tests/nested_loops.ll" \
  -o "$PROJECT_DIR/build/nested_loops.after.ll" \
  2> "$PROJECT_DIR/build/nested_loops.readonly.report.txt"

diff -u \
  "$PROJECT_DIR/build/nested_loops.before.ll" \
  "$PROJECT_DIR/build/nested_loops.after.ll"
```

预期 diff 没有差异。可将同样的检查加入全部测试。

### 10.3 常见错误

| 现象 | 优先检查 |
| --- | --- |
| 嵌套测试只报告一个循环 | 是否只遍历了 LoopInfo 的顶层循环 |
| 内层循环输出两次 | 是否在多处重复递归 |
| loops=1，但打印了两行 loop | 汇总是否只数了顶层循环 |
| 外层 blocks 比自己手数的大 | 是否忘记外层包含内层基本块 |
| 报告文件为空 | 是否重定向了 stdout 而不是 stderr；管线是否注册正确 |
| 未命名 header 显示为空 | 是否只用了 getName，而没有 printAsOperand |
| LLVM 自带报告也不同 | 检查实际 CFG，不以源代码缩进判断循环成员 |

## 11. 90 分钟扩展：静态 load/store 条数

仅在基础版全部通过后执行，不同时开启其他 feature。

为每行循环报告增加：

```text
loads=<普通 load 的静态条数> stores=<普通 store 的静态条数>
```

例如：

```text
loop header=%header depth=1 blocks=2 loads=1 stores=1
```

要求：

1. 遍历当前 Loop 的 `blocks()`，再遍历块内指令。
2. 使用 `isa<LoadInst>`、`isa<StoreInst>` 判断。
3. 统计包含子循环的整个循环区域，外层含内层。
4. 不额外递归累加子循环结果，否则会重复计数。
5. 不将 atomicrmw、cmpxchg、memcpy 或其他调用统计成普通 load/store。
6. 保持结构字段不变。
7. 给无访存的测试补上 `loads=0 stores=0` 断言。

新增 `tests/memory_loop.ll`，可从 single_loop.ll 改造：

- 增加一个 ptr 参数。
- body 中加入一次普通 load 和一次普通 store。
- 断言 `loads=1 stores=1`。
- 若添加嵌套访存用例，验证外层按包含内层的口径统计。

必须在 notes 中说明：

> 一条静态 load 可以在运行时执行多次。静态 load/store 条数不是动态访问次数，不等于字节数，也不等于 HBM 流量。

本轮不继续扩展访问字节数、缓存命中或运行时间推算。

## 12. 完成标准

基础版：

- [ ] 使用新目录，06 保持不变。
- [ ] opt 能加载 LoopReport.so 并运行 loop-report。
- [ ] 无循环、单层循环、嵌套循环三个测试通过。
- [ ] 循环总数包含子循环，每个循环只报告一次。
- [ ] header、depth、blocks 符合约定。
- [ ] 报告格式稳定，写到 stderr。
- [ ] verifier 通过，规范化前后 IR 无差异。
- [ ] run_tests.sh 成功返回 0，失败返回非零。
- [ ] notes.md 有工具版本、复现命令、实际报告和复盘。
- [ ] 不提交 build/ 或二进制工具。

扩展版，可选：

- [ ] 普通 load/store 静态计数正确。
- [ ] 测试覆盖非零计数。
- [ ] 明确包含子循环的统计口径与静态计数边界。

基础版满足即算本轮完成，不以扩展版作为交付门槛。

## 13. 建议 commit

基础版：

```text
feat(llvm): add loop structure reporting pass
```

扩展版可以单独提交：

```text
feat(llvm): report static load and store counts per loop
```

提交前检查 `git diff` 和 `git status`，只暂存本轮相关文件。

## 14. notes.md 与三个复盘问题

建议 notes 只写以下内容，不写长篇教程：

1. LLVM 版本、编译器版本。
2. 实际运行命令。
3. 嵌套测试的实际输出。
4. 一个遇到的错误，以及如何定位；没有则如实记录。
5. 回答下面三个问题。

复盘：

1. 为什么只遍历一次 LoopInfo 会漏掉内层循环？
2. 为什么外层和内层的基本块数量不能直接相加？
3. 07 为什么返回 PreservedAnalyses::all()，而 06 修改 IR 后返回 none()？

完成后留下：可运行插件、三个最小 IR 测试、自动测试脚本和短 notes。本轮到此结束。