#!/usr/bin/env bash
# 构建 training-opt，并对 tests/ 下每个 .mlir 依次检查：
#   1. training-opt 能运行 training-mul-one，--verify-each 通过
#   2. 输出 build/<name>.after.mlir 符合 FileCheck 断言
#   3. 把输出再跑一次 Pass，得到 build/<name>.again.mlir，仍然符合同一组断言
# 任一测试失败时保留输出，脚本最后以非零退出码结束。
#
# 用法：在任意目录执行  cpp/08_mlir_pattern_rewrite/run_tests.sh
# 可以用环境变量 MLIR_PREFIX、LLVM_DIR、MLIR_DIR、FILECHECK 换成别的路径。

set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "$project_dir/../.." && pwd)"

MLIR_PREFIX="${MLIR_PREFIX:-$repo_dir/tool/llvm/LLVM-23.1.3-Linux-X64}"
LLVM_DIR="${LLVM_DIR:-$MLIR_PREFIX/lib/cmake/llvm}"
MLIR_DIR="${MLIR_DIR:-$MLIR_PREFIX/lib/cmake/mlir}"
FILECHECK="${FILECHECK:-$repo_dir/tool/filecheck/FileCheck}"

build_dir="$project_dir/build"
tool="$build_dir/bin/training-opt"
pipeline='builtin.module(func.func(training-mul-one))'

if [[ ! -f "$LLVM_DIR/LLVMConfig.cmake" ]]; then
    echo "找不到 LLVMConfig.cmake：$LLVM_DIR" >&2
    exit 1
fi
if [[ ! -f "$MLIR_DIR/MLIRConfig.cmake" ]]; then
    echo "找不到 MLIRConfig.cmake：$MLIR_DIR" >&2
    exit 1
fi
if [[ ! -x "$FILECHECK" ]]; then
    echo "找不到工具：$FILECHECK" >&2
    exit 1
fi

echo "== 构建 training-opt"
cmake -S "$project_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DLLVM_DIR="$LLVM_DIR" \
    -DMLIR_DIR="$MLIR_DIR" \
    -DCMAKE_PREFIX_PATH="$repo_dir/tool/llvm/deps/zstd/usr" \
    > /dev/null
cmake --build "$build_dir" --target training-opt -j > /dev/null

run_pass() {
    "$tool" --pass-pipeline="$pipeline" --verify-each "$1" -o "$2"
}

failed=0
for test_file in "$project_dir"/tests/*.mlir; do
    name="$(basename "$test_file" .mlir)"
    after="$build_dir/$name.after.mlir"
    again="$build_dir/$name.again.mlir"

    echo "== $name"

    if ! run_pass "$test_file" "$after"; then
        echo "   FAIL: training-opt 运行失败或 IR 验证失败" >&2
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
