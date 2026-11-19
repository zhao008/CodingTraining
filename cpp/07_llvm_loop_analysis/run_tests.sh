#!/usr/bin/env bash
# 构建 LoopReport 插件，并对 tests/ 下每个 .ll 依次检查：
#   1. opt 能加载插件并运行 loop-report，-verify-each 通过
#   2. 报告（stderr）符合 FileCheck 断言，报告保存在 build/<name>.report.txt
#   3. Pass 是只读的：运行前后规范化输出的 IR 没有差异
# 任一测试失败时保留报告，脚本最后以非零退出码结束。
#
# 用法：在任意目录执行  cpp/07_llvm_loop_analysis/run_tests.sh
# 可以用环境变量 LLVM_CONFIG、OPT、FILECHECK 换成别的工具路径。

set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "$project_dir/../.." && pwd)"
llvm_dir="$repo_dir/tool/llvm/LLVM-23.1.3-Linux-X64"

LLVM_CONFIG="${LLVM_CONFIG:-$llvm_dir/bin/llvm-config}"
if [[ ! -x "$LLVM_CONFIG" ]]; then
    echo "找不到工具：$LLVM_CONFIG" >&2
    exit 1
fi
OPT="${OPT:-$("$LLVM_CONFIG" --bindir)/opt}"
FILECHECK="${FILECHECK:-$repo_dir/tool/filecheck/FileCheck}"

build_dir="$project_dir/build"
plugin="$build_dir/LoopReport.so"

for tool in "$OPT" "$FILECHECK"; do
    if [[ ! -x "$tool" ]]; then
        echo "找不到工具：$tool" >&2
        exit 1
    fi
done

echo "== 构建插件"
cmake -S "$project_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DLLVM_DIR="$("$LLVM_CONFIG" --cmakedir)" \
    -DCMAKE_PREFIX_PATH="$repo_dir/tool/llvm/deps/zstd/usr" \
    > /dev/null
cmake --build "$build_dir" -j > /dev/null

failed=0
for test_file in "$project_dir"/tests/*.ll; do
    name="$(basename "$test_file" .ll)"
    report="$build_dir/$name.report.txt"
    before="$build_dir/$name.before.ll"
    after="$build_dir/$name.after.ll"

    echo "== $name"

    # 运行 Pass：报告写到 stderr，同时输出 IR 供只读检查
    if ! "$OPT" \
        -load-pass-plugin="$plugin" \
        -passes='function(loop-report)' \
        -verify-each \
        -S "$test_file" -o "$after" \
        2> "$report"; then
        echo "   FAIL: opt 运行失败或 IR 验证失败（输出见 $report）" >&2
        failed=1
        continue
    fi

    if ! "$FILECHECK" "$test_file" < "$report"; then
        echo "   FAIL: 报告与断言不符（报告见 $report）" >&2
        failed=1
        continue
    fi

    # 只读检查：不加载插件时 opt 规范化输出的 IR，应与运行 Pass 后的完全相同
    "$OPT" -S "$test_file" -o "$before"
    if ! diff -u "$before" "$after" > /dev/null; then
        echo "   FAIL: Pass 修改了 IR（对比 $before 与 $after）" >&2
        failed=1
        continue
    fi

    echo "   PASS"
done

if [[ "$failed" -ne 0 ]]; then
    echo "有测试失败" >&2
    exit 1
fi
echo "全部通过"
