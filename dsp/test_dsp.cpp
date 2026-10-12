// Host test for the Beat Mangler DSP core.
// Builds a synthetic break, runs loader/slicer/sequencer/voice, checks
// invariants, and writes WAV renders for ear-checking.
// Compile: g++ -std=c++17 -O2 -Wall -o test_dsp test_dsp.cpp && ./test_dsp

#include "beat_mangler_dsp.h"

#include <cstdio>
#include <cmath>
#include <vector>
#include <string>

using namespace beatmangler;

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("ok: %s\n", msg); } \
} while (0)

// --- tiny synth helpers ----------------------------------------------------
static void addKick(std::vector<float>& b, double sr, double t) {
    int start = (int)(t * sr);
    int n = (int)(0.35 * sr);
    for (int i = 0; i < n && start + i < (int)b.size(); ++i) {
        double tt = i / sr;
        double f = 45.0 + 110.0 * std::exp(-tt * 30.0);
        double env = std::exp(-tt * 12.0);
        // crude phase accumulation
        static thread_local double phase = 0.0;
        phase += 2.0 * 3.141592653589793 * f / sr;
        b[(size_t)(start + i)] += (float)(std::sin(phase) * env * 0.9);
    }
}
static unsigned snoise = 22222;
static float nz() {
    snoise = snoise * 1664525u + 1013904223u;
    return ((snoise >> 9) & 0x7FFFFF) / 4194304.0f - 1.0f;
}
static void addSnare(std::vector<float>& b, double sr, double t) {
    int start = (int)(t * sr);
    int n = (int)(0.22 * sr);
    for (int i = 0; i < n && start + i < (int)b.size(); ++i) {
        double tt = i / sr;
        double env = std::exp(-tt * 22.0);
        float v = nz() * 0.5f + (float)std::sin(2.0 * 3.141592653589793 * 190.0 * tt) * 0.4f;
        b[(size_t)(start + i)] += (float)(v * env * 0.7);
    }
}
static void addHat(std::vector<float>& b, double sr, double t, float gain = 0.35f) {
    int start = (int)(t * sr);
    int n = (int)(0.06 * sr);
    float prev = 0.0f;
    for (int i = 0; i < n && start + i < (int)b.size(); ++i) {
        double tt = i / sr;
        double env = std::exp(-tt * 90.0);
        float x = nz();
        float hp = x - prev; // crude highpass
        prev = x;
        b[(size_t)(start + i)] += (float)(hp * env * gain);
    }
}

// 2 bars of break at 120bpm: kick on quarters, snare on 2+4, hats on 8ths
static WavData synthBreak(double sr) {
    WavData w;
    w.sampleRate = sr;
    const int N = (int)(4.0 * sr);
    w.l.assign((size_t)N, 0.0f);
    for (int bar = 0; bar < 2; ++bar) {
        double base = bar * 2.0;
        for (int q = 0; q < 4; ++q) addKick(w.l, sr, base + q * 0.5);
        addSnare(w.l, sr, base + 0.5);
        addSnare(w.l, sr, base + 1.5);
        for (int e = 0; e < 8; ++e) addHat(w.l, sr, base + e * 0.25, e % 2 ? 0.25f : 0.4f);
    }
    w.r = w.l;
    return w;
}

static void writeWav(const char* path, const std::vector<float>& l,
                     const std::vector<float>& r, double sr) {
    FILE* f = std::fopen(path, "wb");
    if (!f) return;
    const int n = (int)l.size();
    const int dataBytes = n * 4;
    auto w32 = [&](uint32_t v) {
        uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
        std::fwrite(b, 1, 4, f);
    };
    auto w16 = [&](uint16_t v) {
        uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
        std::fwrite(b, 1, 2, f);
    };
    std::fwrite("RIFF", 1, 4, f); w32(36 + dataBytes);
    std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); w32(16); w16(1); w16(2);
    w32((uint32_t)sr); w32((uint32_t)sr * 4); w16(4); w16(16);
    std::fwrite("data", 1, 4, f); w32(dataBytes);
    for (int i = 0; i < n; ++i) {
        auto s16 = [&](float x) {
            if (x > 1) x = 1;
            if (x < -1) x = -1;
            return (int16_t)std::lround(x * 32767.0f);
        };
        w16((uint16_t)s16(l[(size_t)i]));
        w16((uint16_t)s16(r[(size_t)i]));
    }
    std::fclose(f);
}

static bool hasBad(const std::vector<float>& v) {
    for (float x : v)
        if (!std::isfinite(x)) return true;
    return false;
}
static float peakOf(const std::vector<float>& v) {
    float p = 0;
    for (float x : v) { float a = fabsf(x); if (a > p) p = a; }
    return p;
}
static float rmsOf(const std::vector<float>& v) {
    double s = 0;
    for (float x : v) s += (double)x * x;
    return (float)std::sqrt(s / v.size());
}
static double checksum(const std::vector<float>& v) {
    double s = 0;
    for (size_t i = 0; i < v.size(); i += 7) s += v[i] * (i % 13 + 1);
    return s;
}

static void render(Engine& e, float chaos, std::vector<float>& l,
                   std::vector<float>& r, double seconds) {
    Params p;
    p.chaos = chaos;
    p.rate = 1.0f;
    e.setTempo(120.0);
    e.setParams(p);
    e.reset();
    const int n = (int)(seconds * 48000.0);
    l.assign((size_t)n, 0.0f);
    r.assign((size_t)n, 0.0f);
    const int bs = 512;
    std::vector<float> bl(bs), br(bs);
    for (int off = 0; off < n; off += bs) {
        int m = std::min(bs, n - off);
        e.process(bl.data(), br.data(), m);
        for (int i = 0; i < m; ++i) {
            l[(size_t)(off + i)] = bl[(size_t)i];
            r[(size_t)(off + i)] = br[(size_t)i];
        }
    }
}

int main() {
    const double sr = 48000.0;

    // A: 2-bar break at host tempo -> grid slices (2 bars x 16)
    {
        Engine e;
        e.setSampleRate(sr);
        e.setTempo(120.0);
        WavData w = synthBreak(sr);
        CHECK(e.loadWavData(w), "2-bar break loads");
        int ns = e.numSlices();
        printf("  slices found: %d, bars: %d\n", ns, e.numBars());
        CHECK(e.numBars() == 2, "bar detector finds 2 bars");
        CHECK(ns == 32, "16th grid yields 32 slices");
    }

    // A2: beat grid -> 2 bars x 4
    {
        Engine e;
        e.setSampleRate(sr);
        e.setTempo(120.0);
        Params p;
        p.gridDiv = 4;
        e.setParams(p);
        CHECK(e.loadWavData(synthBreak(sr)), "beat-grid break loads");
        CHECK(e.numSlices() == 8, "beat grid yields 8 slices");
    }

    // B: chaos=0 renders clean, in-order, bounded
    std::vector<float> l0, r0;
    {
        Engine e;
        e.setSampleRate(sr);
        e.loadWavData(synthBreak(sr));
        render(e, 0.0f, l0, r0, 8.0);
        CHECK(!hasBad(l0) && !hasBad(r0), "chaos=0: no NaN/Inf");
        CHECK(peakOf(l0) <= 1.0f, "chaos=0: peak bounded");
        CHECK(rmsOf(l0) > 0.01f, "chaos=0: audible output");
        writeWav("renders/render_chaos0.wav", l0, r0, sr);
    }

    // C: chaos=1 renders clean and different
    {
        Engine e;
        e.setSampleRate(sr);
        e.loadWavData(synthBreak(sr));
        std::vector<float> l1, r1;
        render(e, 1.0f, l1, r1, 8.0);
        CHECK(!hasBad(l1) && !hasBad(r1), "chaos=1: no NaN/Inf");
        CHECK(peakOf(l1) <= 1.0f, "chaos=1: peak bounded");
        CHECK(rmsOf(l1) > 0.01f, "chaos=1: audible output");
        CHECK(std::fabs(checksum(l1) - checksum(l0)) > 1e-3, "chaos=1 differs from chaos=0");
        writeWav("renders/render_chaos1.wav", l1, r1, sr);
        std::vector<float> l5, r5;
        render(e, 0.5f, l5, r5, 8.0);
        writeWav("renders/render_chaos05.wav", l5, r5, sr);
    }

    // D: one-shot -> single slice
    {
        Engine e;
        e.setSampleRate(sr);
        WavData w;
        w.sampleRate = sr;
        const int N = (int)(0.4 * sr);
        w.l.assign((size_t)N, 0.0f);
        for (int i = 0; i < N; ++i) {
            double tt = i / sr;
            w.l[(size_t)i] = (float)(std::sin(2 * 3.141592653589793 * 220.0 * tt) *
                                     std::exp(-tt * 8.0));
        }
        w.r = w.l;
        CHECK(e.loadWavData(w), "one-shot loads");
        CHECK(e.numSlices() == 1, "one-shot becomes a single slice");
    }

    // E: odd-length pad (3s, not on a bar grid) -> transient fallback, 16 slices
    {
        Engine e;
        e.setSampleRate(sr);
        e.setTempo(120.0);
        WavData w;
        w.sampleRate = sr;
        const int N = (int)(3.0 * sr);
        w.l.assign((size_t)N, 0.0f);
        for (int i = 0; i < N; ++i) {
            double tt = i / sr;
            w.l[(size_t)i] = (float)(0.3 * std::sin(2 * 3.141592653589793 * 110.0 * tt) +
                                     0.2 * std::sin(2 * 3.141592653589793 * 165.0 * tt));
        }
        w.r = w.l;
        CHECK(e.loadWavData(w), "pad loads");
        CHECK(e.numBars() == 0, "odd length skips bar detection");
        CHECK(e.numSlices() == 16, "pad falls back to 16 grid slices");
    }

    // F: all pitch modes render clean
    for (auto mode : {PitchSnap::Glide, PitchSnap::Semitone, PitchSnap::Scale}) {
        Engine e;
        e.setSampleRate(sr);
        e.loadWavData(synthBreak(sr));
        Params p;
        p.chaos = 0.8f;
        p.pitchSnap = mode;
        e.setParams(p);
        e.reset();
        std::vector<float> l(48000), r(48000);
        e.process(l.data(), r.data(), 48000);
        CHECK(!hasBad(l), "pitch mode renders clean");
    }

    // G: determinism
    {
        Engine e;
        e.setSampleRate(sr);
        e.loadWavData(synthBreak(sr));
        std::vector<float> a, b, ar, br;
        render(e, 0.7f, a, ar, 4.0);
        render(e, 0.7f, b, br, 4.0);
        CHECK(checksum(a) == checksum(b), "same seed -> identical output");
    }

    // H: WAV bytes round-trip through the parser
    {
        // reuse a render as a fake file
        std::vector<uint8_t> bytes;
        // build minimal 16-bit mono WAV in memory
        const int N = 4800;
        bytes.insert(bytes.end(), {'R','I','F','F'});
        auto pu32 = [&](uint32_t v) {
            bytes.push_back(v & 0xFF); bytes.push_back((v >> 8) & 0xFF);
            bytes.push_back((v >> 16) & 0xFF); bytes.push_back((v >> 24) & 0xFF);
        };
        auto pu16 = [&](uint16_t v) {
            bytes.push_back(v & 0xFF); bytes.push_back((v >> 8) & 0xFF);
        };
        pu32(36 + N * 2);
        bytes.insert(bytes.end(), {'W','A','V','E','f','m','t',' '});
        pu32(16); pu16(1); pu16(1); pu32(48000); pu32(96000); pu16(2); pu16(16);
        bytes.insert(bytes.end(), {'d','a','t','a'});
        pu32(N * 2);
        for (int i = 0; i < N; ++i) {
            int16_t s = (int16_t)(std::sin(i * 0.1) * 20000);
            pu16((uint16_t)s);
        }
        Engine e;
        e.setSampleRate(sr);
        CHECK(e.loadWavBytes(bytes.data(), bytes.size()), "WAV bytes parse and load");
        CHECK(e.numSlices() >= 1, "parsed WAV slices");
    }

    // I: host sync — slices fire in order on the PPQ grid, silent when stopped
    {
        Engine e;
        e.setSampleRate(sr);
        e.setTempo(120.0);
        e.loadWavData(synthBreak(sr)); // 2 bars -> 32 slices
        Params p;
        p.chaos = 0.0f;
        p.rate = 1.0f;
        e.setParams(p);
        e.reset();
        const int bs = 512;
        std::vector<float> l(bs), r(bs);
        // stopped: silence
        e.processSynced(l.data(), r.data(), bs, 0.0, false);
        CHECK(peakOf(l) == 0.0f, "synced: silent while transport stopped");
        // play 1 bar (4 quarters); 16th = 0.25 ppq; block = 512 samples
        double ppq = 0.0;
        const double ppqPerBlock = bs * (120.0 / 60.0) / sr;
        std::vector<int> order;
        int blocks = (int)std::ceil(4.0 / ppqPerBlock);
        int prev = -1;
        for (int b = 0; b < blocks; ++b) {
            e.processSynced(l.data(), r.data(), bs, ppq, true);
            int ls = e.lastSlice();
            if (ls != prev) { order.push_back(ls); prev = ls; }
            ppq += ppqPerBlock;
        }
        bool inOrder = order.size() >= 16;
        for (size_t i = 0; i < order.size() && inOrder; ++i)
            if (order[i] != (int)i % 32) inOrder = false;
        CHECK(inOrder, "synced: chaos=0 fires slices 0,1,2,... on the grid");
        CHECK(rmsOf(l) > 0.0f, "synced: audible while playing");
    }

    // J: quantized launch — stop, then start waits for the next bar line
    {
        Engine e;
        e.setSampleRate(sr);
        e.setTempo(120.0);
        e.loadWavData(synthBreak(sr)); // 2 bars -> 32 slices
        Params p;
        p.chaos = 0.0f;
        p.rate = 1.0f;
        e.setParams(p);
        e.reset();
        const int bs = 512;
        std::vector<float> l(bs), r(bs);
        const double ppqPerBlock = bs * (120.0 / 60.0) / sr;
        // gate closed: silent even though transport runs
        e.stopNow();
        e.processSynced(l.data(), r.data(), bs, 0.0, true);
        CHECK(peakOf(l) == 0.0f, "launch: silent after stop");
        // arm mid-bar (ppq 1.3): silent until the bar line at ppq 4.0
        e.armLaunch();
        double ppq = 1.3;
        bool silentUntilBar = true;
        while (ppq + ppqPerBlock <= 4.0) {
            e.processSynced(l.data(), r.data(), bs, ppq, true);
            if (peakOf(l) != 0.0f) silentUntilBar = false;
            ppq += ppqPerBlock;
        }
        CHECK(silentUntilBar, "launch: silent while queued for the bar");
        // block crossing ppq 4.0 launches from slice 0
        e.processSynced(l.data(), r.data(), bs, ppq, true);
        CHECK(e.launchState() == Engine::Launch::Playing, "launch: fires at bar line");
        CHECK(e.lastSlice() == 0, "launch: restarts from slice 0");
        // and continues in order from there
        ppq += ppqPerBlock;
        std::vector<int> order;
        int prev = 0;
        for (int b = 0; b < 200 && order.size() < 8; ++b) {
            e.processSynced(l.data(), r.data(), bs, ppq, true);
            int ls = e.lastSlice();
            if (ls != prev) { order.push_back(ls); prev = ls; }
            ppq += ppqPerBlock;
        }
        bool seq = order.size() >= 8;
        for (size_t i = 0; i < order.size() && seq; ++i)
            if (order[i] != (int)(i + 1)) seq = false;
        CHECK(seq, "launch: continues 1,2,3,... after restart");
    }

    printf(failures ? "\n%d FAILURES\n" : "\nALL CHECKS PASSED\n", failures);
    return failures ? 1 : 0;
}
