#!/bin/sh
# 把所有测试编出来跑一遍（w64devkit / MinGW-w64）。
#
#   用法：  sh run-tests.sh
#
# 测试一律 #include 真源码（.cpp），不抄副本 —— 抄副本的测试迟早和实际跑的版本走偏。
# 所以每个测试都是一个独立的小 exe，编译慢一点，但测的确实是发出去的那份代码。
set -e
export PATH="/d/w64devkit/bin:$PATH"
cd "$(dirname "$0")"
OUT="${TMPDIR:-/tmp}/wse-tests"
mkdir -p "$OUT"

CXX="g++ -O1 -std=c++17 -DWIN32_LEAN_AND_MEAN -DNOMINMAX"
DLL_LIBS="-lole32 -luuid -lshell32 -lwinmm"

fail=0
run() {   # run <名字> <exe> [参数...]
    name="$1"; shift
    if "$@" > "$OUT/$name.out" 2>&1; then
        echo "  通过  $name"
    else
        echo "  失败  $name"
        sed 's/^/        /' "$OUT/$name.out" | tail -25
        fail=1
    fi
}

echo "==> 插件（DLL）"
for t in cond_test kms_expr_test scan_test shortcut_test resample_test judge_test stop_test; do
    $CXX -o "$OUT/$t.exe" "$t.cpp" $DLL_LIBS
    run "$t" "$OUT/$t.exe"
done

# wav_test 要一个真 wav 文件。仓库里不带音频（发布包也不带），现造一个：
# 48kHz 立体声，左右给不同的恒定值 —— 重采样把左右搅在一起的话一眼就看得出来。
# 注意别直接信 command -v：Windows 上 python3 往往指到应用商店那个占位程序，
# 一跑就打开商店并以 49 退出。所以每个候选都试跑一下再用。
PY=""
for c in python python3 py; do
    p=$(command -v "$c" 2>/dev/null) || continue
    if "$p" -c "pass" >/dev/null 2>&1; then PY="$p"; break; fi
done
if [ -n "$PY" ]; then
    "$PY" - "$OUT/probe.wav" <<'MAKEWAV'
import struct, sys
rate = 48000
n = rate // 2                       # 半秒
data = b''.join(struct.pack('<hh', 12000, -12000) for _ in range(n))
hdr = (b'RIFF' + struct.pack('<I', 36 + len(data)) + b'WAVEfmt ' +
       struct.pack('<IHHIIHH', 16, 1, 2, rate, rate * 4, 4, 16) +
       b'data' + struct.pack('<I', len(data)))
open(sys.argv[1], 'wb').write(hdr + data)
MAKEWAV
    $CXX -o "$OUT/wav_test.exe" wav_test.cpp $DLL_LIBS
    run wav_test "$OUT/wav_test.exe" "$OUT/probe.wav"
else
    echo "  跳过  wav_test（没找到 python，造不出测试用的 wav）"
fi

# 聊天文本测试同时用到插件和 GUI 两边的实现（两边必须一致）
$CXX -I gui/src -o "$OUT/chat_test.exe" chat_test.cpp gui/src/config.cpp $DLL_LIBS
run chat_test "$OUT/chat_test.exe"

echo "==> 配置工具（GUI）"
$CXX -I gui/src -o "$OUT/mondb_test.exe" gui/mondb_test.cpp gui/src/fsmdb.cpp gui/src/config.cpp
run mondb_test "$OUT/mondb_test.exe"

$CXX -I gui/src -o "$OUT/cfg_roundtrip.exe" gui/cfg_roundtrip.cpp gui/src/config.cpp
# 用样例配置跑，不用随包 ini —— 后者的条目全是注释掉的示例，
# 零条条目怎么存都"一致"，等于什么都没测。
run "cfg_roundtrip(样例条目)" "$OUT/cfg_roundtrip.exe" gui/test_entries.ini "$OUT/rt1.ini"
# 第二遍跑上一遍写出来的文件：读->写->读 必须收敛，不能每存一次就变一点
run "cfg_roundtrip(再存一次)" "$OUT/cfg_roundtrip.exe" "$OUT/rt1.ini" "$OUT/rt2.ini"
# StopOnEnd 关着时不写进 ini，所以单独确认开着的那两条确实写出来了
if [ "$(grep -c '^StopOnEnd=1' "$OUT/rt1.ini")" = 2 ]; then
    echo "  通过  StopOnEnd=1 存得住"
else
    echo "  失败  StopOnEnd=1 没存住"; grep -n 'StopOnEnd' "$OUT/rt1.ini"; fail=1
fi
if cmp -s "$OUT/rt1.ini" "$OUT/rt2.ini"; then
    echo "  通过  存两次结果一致"
else
    echo "  失败  存两次结果不一致"; diff "$OUT/rt1.ini" "$OUT/rt2.ini" | head -20; fail=1
fi

echo
if [ "$fail" = 0 ]; then echo "全部通过"; else echo "有失败项"; exit 1; fi
