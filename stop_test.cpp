// 「动作结束就掐掉音效」—— 回归测试。
//
// 两件事要测：
//   ① 什么时候算动作结束（TickStopOnEnd）。判早了会把正放着的音效拦腰切断，
//      而这种事在日志里看不出来 —— 只有玩的人听得见。
//   ② 掐谁（StopOwner）。按条目的 voiceTag 分，别的条目的声音不能受牵连。
//
// ② 里不碰真设备：还没开设备（hwo 为空）的声音照样要能被掐 —— 配了延时的音效
// 就处在这个状态，动作都结束了它还没出声，这时候更该拦住。
#include "WeaponSoundEnhance.cpp"
#include <cstdio>
#include <cmath>

static int bad = 0;

static void Chk(const char* what, bool ok)
{
    if (!ok) ++bad;
    printf("  %-4s %s\n", ok ? "OK" : "FAIL", what);
}

// 轮询是 60ms 一拍，确认时长 100ms —— 也就是要连着两拍不匹配才算结束
static const int kPoll = 60;
static const int kConfirm = plugin::kStopConfirmMs;

int main()
{
    plugin::gLogPath = L"stop_test.log";
    ::DeleteFileW(plugin::gLogPath.c_str());

    printf("==== 没开 StopOnEnd：永远不掐 ====\n");
    {
        plugin::StopRt st;
        bool any = false;
        for (int t = 0; t <= 600; t += kPoll)
            if (plugin::TickStopOnEnd(st, false, t < 180, (std::uint64_t)t, kConfirm))
                any = true;
        Chk("整段跑下来一次都没掐", !any);
    }

    printf("\n==== 动作正常结束：确认满 100ms 才掐，而且只掐一次 ====\n");
    {
        plugin::StopRt st;
        int cuts = 0, firstAt = -1;
        // 0~120ms 动作在，180ms 起不在了
        for (int t = 0; t <= 600; t += kPoll) {
            const bool match = (t <= 120);
            if (plugin::TickStopOnEnd(st, true, match, (std::uint64_t)t, kConfirm)) {
                ++cuts;
                if (firstAt < 0) firstAt = t;
            }
        }
        Chk("掐了，而且只掐一次", cuts == 1);
        // 180ms 那拍记下「动作走了」，240ms 才满 60ms，300ms 满 120ms >= 100ms
        Chk("在 300ms 掐的（动作离开后第 120ms）", firstAt == 300);
    }

    printf("\n==== 动作 ID 抖一下：不能掐 ====\n");
    {
        // 轮询 60ms，游戏里派生、硬直切换都可能让某一拍读不到本条目的动作。
        // 立刻掐的话好端端的音效会被切断 —— 确认时长就是为这个加的。
        plugin::StopRt st;
        bool any = false;
        const bool seq[] = { true, true, false, true, true, true };
        for (int i = 0; i < 6; ++i)
            if (plugin::TickStopOnEnd(st, true, seq[i], (std::uint64_t)(i * kPoll), kConfirm))
                any = true;
        Chk("中间漏了一拍，没掐", !any);
    }

    printf("\n==== 掐完之后：下一招还能再掐 ====\n");
    {
        plugin::StopRt st;
        int cuts = 0;
        // 第一招：0~60 在，之后走人
        for (int t = 0; t <= 400; t += kPoll)
            if (plugin::TickStopOnEnd(st, true, t <= 60, (std::uint64_t)t, kConfirm)) ++cuts;
        Chk("第一招掐了一次", cuts == 1);
        // 第二招：1000~1060 在，之后走人
        for (int t = 1000; t <= 1400; t += kPoll)
            if (plugin::TickStopOnEnd(st, true, t <= 1060, (std::uint64_t)t, kConfirm)) ++cuts;
        Chk("第二招又掐了一次", cuts == 2);
    }

    printf("\n==== 从来没匹配过：没什么好掐的 ====\n");
    {
        plugin::StopRt st;
        bool any = false;
        for (int t = 0; t <= 600; t += kPoll)
            if (plugin::TickStopOnEnd(st, true, false, (std::uint64_t)t, kConfirm)) any = true;
        Chk("一直不匹配，不掐", !any);
    }

    printf("\n==== 确认时长设 0：动作一走就掐 ====\n");
    {
        plugin::StopRt st;
        Chk("匹配中不掐", !plugin::TickStopOnEnd(st, true, true, 0, 0));
        Chk("下一拍不匹配，立刻掐", plugin::TickStopOnEnd(st, true, false, 60, 0));
    }

    printf("\n==== 掐谁：按 voiceTag 分，别人的声音不受牵连 ====\n");
    {
        audio::gLive.clear();
        std::shared_ptr<audio::LiveVoice> a1 = audio::Register(5);
        std::shared_ptr<audio::LiveVoice> a2 = audio::Register(5);
        std::shared_ptr<audio::LiveVoice> b1 = audio::Register(7);
        std::shared_ptr<audio::LiveVoice> n1 = audio::Register(-1);   // 试听之类，不归条目

        Chk("掐 5 号条目，掐掉两条", audio::StopOwner(5) == 2);
        Chk("5 号那两条都标上了", a1->stopped.load() && a2->stopped.load());
        Chk("7 号没被牵连", !b1->stopped.load());
        Chk("再掐一次 5 号，已经掐过了不重复", audio::StopOwner(5) == 0);
        Chk("掐 7 号，掐掉一条", audio::StopOwner(7) == 1);
        Chk("owner=-1 的谁也掐不着", audio::StopOwner(-1) == 0 && !n1->stopped.load());

        // 还没开设备（hwo 为空）就被掐的，Publish 必须拦住它 ——
        // 不拦的话延时音效会在动作早就结束之后才冒出来。
        Chk("被掐过的不许再开口", !audio::Publish(a1, nullptr));
        Chk("没被掐的正常登记句柄", audio::Publish(n1, nullptr) == true);

        audio::Unregister(a1); audio::Unregister(a2);
        audio::Unregister(b1); audio::Unregister(n1);
        Chk("摘干净了", audio::gLive.empty());
        Chk("空表上掐不报错", audio::StopOwner(5) == 0);
    }

    printf("\n==== 真放一条再掐：设备这条路也得通 ====\n");
    {
        // 上面测的是账本，这里测真家伙：waveOutReset 下去，播放线程得自己醒过来退出。
        // 3 秒的音，放到 300ms 掐掉，正常的话 100ms 出头就收干净了；要是 reset 不
        // 起作用，就得等满 3 秒。
        // 振幅压到 60/32767（约 -55dB），电脑上基本听不见，不吵人。
        audio::Wav w;
        w.channels = 1; w.sampleRate = 44100; w.bitsPer = 16; w.valid = true;
        const std::size_t frames = 44100 * 3;
        std::vector<std::int16_t> pcm(frames);
        for (std::size_t i = 0; i < frames; ++i)
            pcm[i] = (std::int16_t)(60.0 * std::sin(2.0 * 3.14159265 * 440.0 * i / 44100.0));
        w.data.resize(pcm.size() * 2);
        std::memcpy(w.data.data(), pcm.data(), w.data.size());

        const bool queued = audio::g_audio.Play(w, 1.0f, 0, 42);
        Chk("投出去了", queued);

        // 等它真的开起来
        int waited = 0;
        while (audio::gVoices.load() == 0 && waited < 2000) { ::Sleep(10); waited += 10; }
        if (audio::gVoices.load() == 0) {
            printf("  跳过 这台机器开不了 waveOut 设备，设备这段测不了\n");
        } else {
            ::Sleep(300);
            Chk("放着的时候还在表里", audio::StopOwner(42) == 1);
            const DWORD t0 = ::GetTickCount();
            while (audio::gVoices.load() != 0 && ::GetTickCount() - t0 < 3000) ::Sleep(5);
            const DWORD took = ::GetTickCount() - t0;
            printf("       掐下去到线程退出用了 %lu ms\n", (unsigned long)took);
            Chk("线程醒过来退出了（没等满 3 秒）", audio::gVoices.load() == 0 && took < 1000);
            Chk("登记表自己清干净了", audio::gLive.empty());
        }
    }

    printf("\n%s\n", bad ? "有失败项" : "全部通过");
    return bad ? 1 : 0;
}
