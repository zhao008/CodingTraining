#!/usr/bin/env bash
# 构建 SimplifyAddZero 插件，并对 tests/ 下每个 .ll 依次检查：
#   1. opt 能加载插件并运行 Pass，-verify-each 通过（IR 结构合法）
#   2. FileCheck 断言通过（变换结果符合预期）
#   3. 对输出再运行一次 Pass，FileCheck 仍然通过（不会越改越多）
# 任何一步失败，脚本立即以非零退出码结束。
#
# 用法：在任意目录执行  cpp/06_llvm_simplify_pass/run_tests.sh
# 可以用环境变量 OPT、FILECHECK、LLVM_CONFIG 换成别的工具路径。

set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "$project_dir/../.." && pwd)"
llvm_dir="$repo_dir/tool/llvm/LLVM-23.1.3-Linux-X64"

LLVM_CONFIG="${LLVM_CONFIG:-$llvm_dir/bin/llvm-config}"
OPT="${OPT:-$llvm_dir/bin/opt}"
FILECHECK="${FILECHECK:-$repo_dir/tool/filecheck/FileCheck}"

build_dir="$project_dir/build"
plugin="$build_dir/SimplifyAddZero.so"

for tool in "$LLVM_CONFIG" "$OPT" "$FILECHECK"; do
    if [[ ! -x "$tool" ]]; then
        echo "找不到工具：$tool" >&2
        exit 1
    fi
done

echo "== 构建插件"
if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
    cmake -S "$project_dir" -B "$build_dir" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DLLVM_DIR="$("$LLVM_CONFIG" --cmakedir)" \
        -DCMAKE_PREFIX_PATH="$repo_dir/tool/llvm/deps/zstd/usr" \
        > /dev/null
fi
cmake --build "$build_dir" -j > /dev/null

run_pass() {
    "$OPT" \
        -load-pass-plugin="$plugin" \
        -passes='function(simplify-add-zero)' \
        -verify-each \
        -S "$1" -o "$2"
}

failed=0
for test_file in "$project_dir"/tests/*.ll; do
    name="$(basename "$test_file" .ll)"
    after="$build_dir/$name.after.ll"
    again="$build_dir/$name.again.ll"

    echo "== $name"

    if ! run_pass "$test_file" "$after"; then
        echo "   FAIL: opt 运行失败或 IR 验证失败" >&2
        failed=1
        continue
    fi

    if ! "$FILECHECK" "$test_file" < "$after"; then
        echo "   FAIL: FileCheck 断言不通过（输出见 $after）" >&2
        failed=1
        continue
    fi

    if ! run_pass "$after" "$again" || ! "$FILECHECK" "$test_file" < "$again"; then
        echo "   FAIL: 对输出再次运行 Pass 后，断言不再成立（输出见 $again）" >&2
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
