// 验一件事：ResampleTo 对立体声是不是按「帧」处理的。
//
// 立体声的 PCM 是 L R L R … 交错存的。重采样必须按帧（一对 LR）走；
// 要是把整块当成一条平坦的采样流来插值，输出的每个点就会落在一个 L 和一个 R
// 之间，左右声道互相串进对方 —— 听感上是立体声塌掉、声像漂移。
//
// 造一段极端信号：左声道恒为 +12000，右声道恒为 -12000。
// 按帧重采样的话，输出还应该是 +12000/-12000 交替；
// 串了的话，会出现大量接近 0 的值。
#include "WeaponSoundEnhance.cpp"
#include <cstdio>
#include <vector>

int main()
{
    plugin::gLogPath = L"resample_test.log";
    ::DeleteFileW(plugin::gLogPath.c_str());

    const int FRAMES = 48000;              // 1 秒
    audio::Wav w;
    w.channels = 2;
    w.sampleRate = 48000;
    w.bitsPer = 16;
    w.valid = true;
    w.data.resize((std::size_t)FRAMES * 2 * 2);          // 帧 × 声道 × 2字节
    std::int16_t* p = (std::int16_t*)w.data.data();
    for (int i = 0; i < FRAMES; ++i) {
        p[i * 2 + 0] = (std::int16_t)+12000;   // L
        p[i * 2 + 1] = (std::int16_t)-12000;   // R
    }

    printf("输入: 1 秒 48kHz 立体声，左声道恒 +12000，右声道恒 -12000\n\n");

    audio::ResampleTo(w, 44100);

    const std::int16_t* q = (const std::int16_t*)w.data.data();
    const std::size_t n = w.data.size() / 2;
    // 掐头去尾，只看中段，避开边界
    const std::size_t a = n / 4, b = n - n / 4;

    int clean = 0, muddy = 0;
    for (std::size_t i = a; i < b; ++i) {
        const int v = q[i];
        if (v > 11000 || v < -11000) ++clean;      // 还是原来的幅度
        else if (v > -6000 && v < 6000) ++muddy;   // 被对面声道拉平了
    }
    const std::size_t total = b - a;
    printf("中段 %zu 个采样点里：\n", total);
    printf("  保持原幅度(|v|>11000) : %d  (%.1f%%)\n", clean, 100.0 * clean / total);
    printf("  被拉向 0 (|v|<6000)   : %d  (%.1f%%)\n", muddy, 100.0 * muddy / total);

    const bool perFrame = (muddy * 20 < (int)total);   // 串扰应该几乎没有
    printf("\n%s\n", perFrame
           ? "按帧重采样，左右没串"
           : "左右声道互相串了，立体声会塌掉 —— ResampleTo 又按采样点走了？");
    return perFrame ? 0 : 1;
}
