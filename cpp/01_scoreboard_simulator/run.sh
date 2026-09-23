#!/usr/bin/env bash
# 用法:
#   ./run.sh           调试模式：带 AddressSanitizer/UBSan，出错能定位到行号
#   ./run.sh release   发布模式：-O2 优化，不带检查
set -euo pipefail

cd "$(dirname "$0")"

MODE="${1:-debug}"
COMMON="-std=c++20 -Wall -Wextra -Wpedantic"

case "$MODE" in
    debug)
        OUT=scoreboard_asan
        g++ $COMMON -g -O0 -fsanitize=address,undefined main.cpp -o "$OUT"
        ;;
    release)
        OUT=scoreboard
        g++ $COMMON -O2 main.cpp -o "$OUT"
        ;;
    *)
        echo "未知模式: $MODE（可选 debug / release）" >&2
        exit 1
        ;;
esac

echo "== 编译成功 ($MODE)，运行 ./$OUT =="
./"$OUT" | tee /tmp/scoreboard_actual.txt

# expected_output.txt 不为空时，自动对比输出
if [ -s expected_output.txt ]; then
    if diff -u expected_output.txt /tmp/scoreboard_actual.txt; then
        echo "== 输出与 expected_output.txt 一致 =="
    else
        echo "== 输出与 expected_output.txt 不一致（见上方 diff）==" >&2
        exit 1
    fi
fi
