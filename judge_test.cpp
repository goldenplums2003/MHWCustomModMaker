// 判定窗口什么时候收 —— 回归测试。
//
// 背景：登龙命中却播了「落空」的音效。日志里是这么写的：
//     window open
//     action ended at ms=125 (offset +150ms, 最晚出伤 485ms)
//     timeout, fallback (dmg=0)        <- 开窗后 307ms 就判了
// 而 388 次命中的出伤时刻是 172~547ms、中位数 297ms —— 窗口在一半以上的命中
// 到达之前就关了。全部日志里登龙走「动作结束」这条路径收窗 39 次，35 次早于
// 已知出伤窗。
//
// 注意：登龙不管命中与否都会掉刃，所以 dAura 对它不携带命中信息，
// 唯一有效的信号是 dmg —— 也就是说窗口必须活到伤害可能到达的时刻。
#include "WeaponSoundEnhance.cpp"
#include <cstdio>

static int bad = 0;

static void Case(const char* what, bool actEnded, std::uint64_t sinceEnd, int el,
                 int maxDmg, int grace, bool want)
{
    const bool got = plugin::JudgeWindowEnded(actEnded, sinceEnd, el, maxDmg, grace);
    const bool ok = (got == want);
    if (!ok) ++bad;
    printf("  %-4s %-42s -> %s (want %s)\n", ok ? "OK" : "FAIL", what,
           got ? "收窗" : "继续等", want ? "收窗" : "继续等");
}

int main()
{
    plugin::gLogPath = L"judge_test.log";
    ::DeleteFileW(plugin::gLogPath.c_str());

    printf("==== 实测那次误判的时间线（最晚出伤 485ms，余量 150ms）====\n");
    // 动作 125ms 结束，之后每 60ms 轮询一次
    Case("125ms 动作刚结束",            true,   0, 125, 485, 150, false);
    Case("275ms 动作结束满余量",        true, 150, 275, 485, 150, false);  // 旧代码这里就收了
    Case("297ms 命中的中位出伤时刻",     true, 172, 297, 485, 150, false);
    Case("484ms 最晚出伤前一刻",        true, 359, 484, 485, 150, false);
    Case("485ms 到了最晚出伤",          true, 360, 485, 485, 150, true);
    printf("\n");

    printf("==== 还没学到「最晚出伤」时，保持原来的行为 ====\n");
    Case("没学到，动作结束满余量就收",   true, 150, 275,  -1, 150, true);
    Case("没学到，余量还没满",          true,  80, 205,  -1, 150, false);
    printf("\n");

    printf("==== 路径②：过了最晚出伤+余量，动作没结束也收 ====\n");
    Case("634ms 还差一点",             false,  0, 634, 485, 150, false);
    Case("635ms 到点（动作还没结束）",   false,  0, 635, 485, 150, true);
    printf("\n");

    printf("==== 大居那种动作不提前切走的，行为不变 ====\n");
    // 大居：最晚出伤 375ms，余量 150 -> 路径② 在 525ms 就到点了。
    // 实测那次正是在 ms=547 判的（轮询 60ms 一次，525 之后的第一拍）。
    Case("524ms 路径②还差一点",         false,  0, 524, 375, 150, false);
    Case("547ms 实测判定的那一拍",       true,   0, 547, 375, 150, true);
    // 动作在出伤窗之前就结束的情况，大居没有；真有也该等到 375ms
    Case("200ms 动作提前结束也要等",     true, 150, 200, 375, 150, false);

    printf("\n%s\n", bad ? "有失败项" : "全部通过");
    return bad ? 1 : 0;
}
