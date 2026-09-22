// 拿真文件跑真解析器：ParseWav + ResampleTo 是插件里实际用的那两个函数。
// 用法: wav_test <file.wav>
#include "WeaponSoundEnhance.cpp"

#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    if (argc < 2) { printf("用法: wav_test <file.wav>\n"); return 2; }
    plugin::gLogPath = L"wav_test.log";
    ::DeleteFileW(plugin::gLogPath.c_str());

    FILE* f = fopen(argv[1], "rb");
    if (!f) { printf("打不开 %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END);
    const long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<std::uint8_t> raw((std::size_t)sz);
    if (fread(raw.data(), 1, raw.size(), f) != raw.size()) { printf("读失败\n"); return 1; }
    fclose(f);

    printf("文件      : %s\n", argv[1]);
    printf("大小      : %.2f MB\n", sz / 1048576.0);
    printf("当前上限  : %d MB  -> %s\n\n", plugin::g_maxWavMB,
           sz <= ((long long)plugin::g_maxWavMB << 20) ? "放得下" : "**会被拒**");

    int bad = 0;

    audio::Wav w;
    const bool ok = audio::ParseWav(raw.data(), raw.size(), w);
    printf("  %-4s ParseWav\n", ok ? "OK" : "FAIL");
    if (!ok) { printf("\n解析失败，后面不用看了\n"); return 1; }

    const double secs = (double)w.data.size() /
                        (w.sampleRate * w.channels * (w.bitsPer / 8.0));
    printf("       声道 %u  采样率 %u  位深 %u  时长 %.1f 秒 (%d:%02d)\n",
           w.channels, w.sampleRate, w.bitsPer, secs, (int)secs / 60, (int)secs % 60);
    printf("       PCM 数据 %.2f MB\n\n", w.data.size() / 1048576.0);

    const std::uint32_t srcRate = w.sampleRate;
    const std::size_t   srcSize = w.data.size();

    audio::ResampleTo(w, 44100);
    const bool resampled = (w.sampleRate == 44100);
    printf("  %-4s ResampleTo(44100)\n", resampled ? "OK" : "FAIL");
    if (!resampled) ++bad;
    printf("       %u Hz -> %u Hz，数据 %.2f MB -> %.2f MB\n",
           srcRate, w.sampleRate, srcSize / 1048576.0, w.data.size() / 1048576.0);

    // 重采样后时长必须基本不变，差太多说明算错了
    const double secs2 = (double)w.data.size() /
                         (w.sampleRate * w.channels * (w.bitsPer / 8.0));
    const double drift = secs2 - secs;
    const bool sameLen = (drift > -0.05 && drift < 0.05);
    printf("  %-4s 时长没变：%.2f 秒 -> %.2f 秒（差 %+.3f 秒）\n",
           sameLen ? "OK" : "FAIL", secs, secs2, drift);
    if (!sameLen) ++bad;

    // 全静音说明数据被搞坏了
    const std::int16_t* p = (const std::int16_t*)w.data.data();
    const std::size_t n = w.data.size() / 2;
    long long peak = 0, sum = 0;
    for (std::size_t i = 0; i < n; ++i) {
        long long v = p[i] < 0 ? -(long long)p[i] : p[i];
        if (v > peak) peak = v;
        sum += v;
    }
    const bool alive = (peak > 100);
    printf("  %-4s 有声音：峰值 %lld，平均 %lld\n", alive ? "OK" : "FAIL",
           peak, n ? sum / (long long)n : 0);
    if (!alive) ++bad;

    printf("\n%s\n", bad ? "有失败项" : "这个文件能正常解析和播放");
    return bad ? 1 : 0;
}
