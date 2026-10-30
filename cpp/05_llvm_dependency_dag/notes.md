# 05 LLVM 单基本块算术 RAW DAG：记录

## 环境

- LLVM 23.1.3，位置：`tool/llvm/LLVM-23.1.3-Linux-X64`
- 编译器：g++ 11.4.0（Ubuntu 22.04），C++20
- CMake 3.22.1

## 构建与运行

在仓库根目录：

```bash
cmake -S cpp/05_llvm_dependency_dag \
  -B cpp/05_llvm_dependency_dag/build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DLLVM_DIR="$PWD/tool/llvm/LLVM-23.1.3-Linux-X64/lib/cmake/llvm" \
  -DCMAKE_PREFIX_PATH="$PWD/tool/llvm/deps/zstd/usr"

cmake --build cpp/05_llvm_dependency_dag/build -j

cd cpp/05_llvm_dependency_dag
./build/llvm_dependency_dag examples/arithmetic.ll
```

## 实际通过的测试

输出与 README 第 7 节的预期逐项对照过。

| 输入 | 结果 | 退出码 |
|---|---|---|
| examples/arithmetic.ll | 5 个节点、5 条边，邻接表与第 7.1 节的表一致 | 0 |
| examples/repeated_use.ll | 2 个节点、1 条边，无重复边 | 0 |
| examples/unnamed.ll | 2 个节点、1 条边，编号 0、1 | 0 |
| examples/return_only.ll | 两个函数都是 0 个节点、0 条边，成功 | 0 |
| examples/unused.ll | 2 个节点、0 条边 | 0 |
| examples/unsupported.ll | 报 `Unsupported instruction: sub`，没有部分图 | 1 |
| examples/multiple_blocks.ll | 报 `Only one basic block is supported, but found 2` | 1 |
| examples/state_sequence.ll | first_ok 2/1，then_fail 报错，then_empty 0/0，finally_ok 1/0，节点重新从 0 编号 | 1 |

README 之外自己加的范围外输入，全部报错、退出码 1：

| 输入 | 错误信息 |
|---|---|
| examples/vector_add.ll | `Argument type must be i32, argument index 0` |
| examples/wide_i64.ll | `Return type must be i32` |
| examples/varargs.ll | `Variadic functions are not supported` |
| examples/return_void.ll | `Return type must be i32` |
| examples/narrow_add.ll | `Arithmetic result must be i32: add` |

命令行：

| 情况 | 结果 | 退出码 |
|---|---|---|
| 不传参数 | 输出用法 | 1 |
| 两个参数 | 输出用法 | 1 |
| 文件不存在 | `Could not open input file: No such file or directory` | 1 |
| IR 语法错误（04 的 invalid.ll） | 输出行号、列号和原因 | 1 |
| 空文件（04 的 empty.ll） | 无输出 | 0 |
| 只有声明（04 的 declaration.ll） | `skipped declaration` | 0 |

## 踩坑点

README 第 8 节的 cmake 命令没有 `CMAKE_PREFIX_PATH`。直接照抄，配置阶段报
`Target ... links to target "zstd::libzstd_static" but the target was not found`。

原因：这份 LLVM 的 CMake 配置文件引用了 zstd 的静态库目标，系统里没有装对应的开发包。
zstd 放在 `tool/llvm/deps/zstd/usr`，要通过 `-DCMAKE_PREFIX_PATH` 告诉 CMake。
04 的 build 缓存里已经有这个设置，所以 04 能构建。

## 尚未完成

- README 第 14 节的三个复盘问题，还没有写。
- 没有检查 Argument 是否属于当前函数（`Argument::getParent`）。`verifyModule` 通过后，
  合法 IR 里不会出现别的函数的参数，所以没有加。
- 没有自动化测试，只有手工对照输出。
- 还没有 commit。
