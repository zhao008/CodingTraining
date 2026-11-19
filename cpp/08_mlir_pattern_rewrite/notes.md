# 08 notes

## 1. 环境

| 项目 | 版本 / 来源 |
| --- | --- |
| LLVM / MLIR | 23.1.3，仓库内 `tool/llvm/LLVM-23.1.3-Linux-X64`（同一前缀同时提供 LLVM 与 MLIR 的 CMake 配置、头文件、静态库） |
| 编译器 | c++ (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0 |
| CMake | 3.22.1 |
| FileCheck | 仓库内 `tool/filecheck/FileCheck`（Python filecheck 1.0.6） |

## 2. 复现命令

在仓库根目录：

```bash
cpp/08_mlir_pattern_rewrite/run_tests.sh
```

实际输出：

```text
== 构建 training-opt
== basic
   PASS
== negative
   PASS
全部通过
```

只跑一个文件：

```bash
cpp/08_mlir_pattern_rewrite/build/bin/training-opt \
  --pass-pipeline='builtin.module(func.func(training-mul-one))' \
  --verify-each \
  cpp/08_mlir_pattern_rewrite/tests/basic.mlir
```

## 3. 变换前后

`@multiple_uses` 变换前：

```mlir
func.func @multiple_uses(%x: i32) -> i32 {
  %one = arith.constant 1 : i32
  %a = arith.muli %x, %one : i32
  %sum = arith.addi %a, %a : i32
  return %sum : i32
}
```

变换后（`build/basic.after.mlir`）：

```mlir
func.func @multiple_uses(%arg0: i32) -> i32 {
  %c1_i32 = arith.constant 1 : i32
  %0 = arith.addi %arg0, %arg0 : i32
  return %0 : i32
}
```

- `%a` 的两处使用都换成了函数参数。
- 常量 1 留在输出中：walk driver 不做死代码删除，本轮也不要求删除。
- SSA 名字由打印器重新生成（`%x` → `%arg0`），所以测试用 FileCheck 捕获变量，不写死名字。

## 4. 关闭规则的对照

在 `matchAndRewrite` 开头临时加 `return mlir::failure();`，在临时目录的副本中构建运行：

```text
[basic] FAIL
tests/basic.mlir:4: error: Couldn't match "return %[[R]] : i32".
[negative] PASS
```

关闭规则后，`basic.after.mlir` 中仍有 6 条 `arith.muli`，和输入中的数量相同（right_one 1、left_one 1、chain 3、multiple_uses 1）。

恢复原实现后，两个测试都通过。

结论：乘法是被自己的 pattern 消除的，不是被 folding 或其他 Pass 消除的。

## 5. 遇到的问题

1. **在 struct 内定义的辅助函数无法调用。** `isConstantOne` 写成 `MulOnePattern` 的成员函数，但没有加 `const`，编译时报错：

   ```text
   error: passing 'const {anonymous}::MulOnePattern' as 'this' argument discards qualifiers
   ```

   原因：`matchAndRewrite` 是 `const` 成员函数（父类就是这样声明的），在 `const` 成员函数里只能调用 `const` 成员函数。给 `isConstantOne` 加上 `const` 后解决；加 `static` 或者把它移到 struct 外也可以。

2. **类型检查条件写反。** 最初写成 `!(isSignless() & getWidth() != 32)`，结果 i32 被拒绝、i64 被放过。改成"不是 signless 或宽度不是 32 就返回 failure"后正确。

3. **替换后返回了 failure。** 调用 `replaceOp` 之后写的是 `return failure()`，两个测试照样通过。原因是 walk driver 只遍历一次，不依赖返回值。改为 `return success()`，以符合"修改 IR 当且仅当返回 success"的约定。

## 6. 复盘

1. **为什么不能用默认带 folding 的 driver 来证明 pattern 正确？**
   `arith.muli` 自己就有乘 1 的 fold。`applyPatternsGreedily` 默认会执行 fold，即使自己的 pattern 一直返回 failure，乘法也可能被消除，测试照样通过，不能说明自己的代码是对的。`walkAndApplyPatterns` 不执行 fold，所以第 4 节关闭规则后，乘法确实留了下来。

2. **为什么匹配失败前不能修改 IR，修改又必须经过 PatternRewriter？**
   driver 根据返回值判断 IR 有没有变化：如果先改了 IR 再返回 failure，driver 就会以为没有变化，而 IR 实际已被改动，可能是改了一半的状态。修改经过 rewriter，driver 才能知道哪些 op 被删除、哪些被新建，从而不再访问已删除的 op。

3. **06 的 RAUW + erase 与本轮 replaceOp 有什么区别？**
   06 自己分两步完成：先 `replaceAllUsesWith`，再 `eraseFromParent`，删除的时机和安全性都要自己保证。08 只调用一次 `rewriter.replaceOp(op, value)`，替换所有使用和删除原 op 都由 rewriter 完成，并会通知 driver。调用之后不能再访问 `op`，也不能再手动删除它。
