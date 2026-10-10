#pragma once
// Beat Mangler DSP core — portable, header-only, C++17.
// Shared verbatim between the Daisy Patch firmware, the JUCE VST3, and the
// host test harness. No platform dependencies, no UI code.
//
// PORTING NOTE (Daisy M7): std::vector is used for buffers here. On the
// firmware these become static buffers in SDRAM. The audio thread never
// allocates: all allocation happens in loadWavData(), never in process().

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace beatmangler {

// ---------------------------------------------------------------------------
// Deterministic PRNG (mulberry-ish xorshift). Seeded for reproducible tests.
// ---------------------------------------------------------------------------
struct Rng {
    uint32_t s = 0xC0FFEEu;
    void seed(uint32_t v) { s = v ? v : 0xC0FFEEu; }
    uint32_t next() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return s;
    }
    float uniform() { return (next() >> 8) * (1.0f / 16777216.0f); } // [0,1)
    float bilateral() { return uniform() * 2.0f - 1.0f; }            // [-1,1)
};

// ---------------------------------------------------------------------------
// WAV loading
// ---------------------------------------------------------------------------
struct WavData {
    std::vector<float> l, r;   // deinterleaved, mono duplicated to stereo
    double sampleRate = 48000.0;
};

inline bool parseWav(const uint8_t* data, size_t size, WavData& out) {
    if (size < 44) return false;
    if (std::memcmp(data, "RIFF", 4) != 0) return false;
    if (std::memcmp(data + 8, "WAVE", 4) != 0) return false;
    auto rd16 = [&](size_t p) -> uint16_t {
        return (uint16_t)(data[p] | ((uint16_t)data[p + 1] << 8));
    };
    auto rd32 = [&](size_t p) -> uint32_t {
        return (uint32_t)data[p] | ((uint32_t)data[p + 1] << 8) |
               ((uint32_t)data[p + 2] << 16) | ((uint32_t)data[p + 3] << 24);
    };
    uint16_t audioFormat = 0, channels = 0, bitsPerSample = 0;
    uint32_t wavRate = 0;
    size_t dataOff = 0, dataSize = 0;
    size_t pos = 12;
    while (pos + 8 <= size) {
        uint32_t chunkSize = rd32(pos + 4);
        if (std::memcmp(data + pos, "fmt ", 4) == 0) {
            if (chunkSize < 16 || pos + 8 + chunkSize > size) return false;
            audioFormat = rd16(pos + 8);
            channels = rd16(pos + 10);
            wavRate = rd32(pos + 12);
            bitsPerSample = rd16(pos + 22);
        } else if (std::memcmp(data + pos, "data", 4) == 0) {
            if (pos + 8 + chunkSize > size) return false;
            dataOff = pos + 8;
            dataSize = chunkSize;
        }
        pos += 8 + chunkSize + (chunkSize & 1u);
    }
    if (!dataOff || !wavRate) return false;
    if (audioFormat != 1 && audioFormat != 3) return false; // PCM int or float
    if (channels < 1 || channels > 2) return false;
    if (audioFormat == 1 &&
        bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32)
        return false;
    if (audioFormat == 3 && bitsPerSample != 32) return false;
    const size_t bps = bitsPerSample / 8;
    const size_t frames = dataSize / (bps * channels);
    if (!frames) return false;
    out.l.resize(frames);
    out.r.resize(frames);
    for (size_t i = 0; i < frames; ++i) {
        float ch[2] = {0.0f, 0.0f};
        for (int c = 0; c < (int)channels; ++c) {
            const size_t off = dataOff + (i * channels + c) * bps;
            float v = 0.0f;
            if (audioFormat == 3) {
                uint32_t u = rd32(off);
                std::memcpy(&v, &u, 4);
            } else if (bitsPerSample == 16) {
                v = (float)(int16_t)rd16(off) / 32768.0f;
            } else if (bitsPerSample == 24) {
                int32_t s = (int32_t)(data[off] | (data[off + 1] << 8) |
                                      (data[off + 2] << 16));
                if (s & 0x800000) s |= (int32_t)0xFF000000;
                v = (float)s / 8388608.0f;
            } else { // 32-bit int
                v = (float)(int32_t)rd32(off) / 2147483648.0f;
            }
            ch[c] = v;
        }
        out.l[i] = ch[0];
        out.r[i] = (channels == 2) ? ch[1] : ch[0];
    }
    out.sampleRate = (double)wavRate;
    return true;
}

// Cheap linear resampler, used once at load time (never realtime).
inline void resampleLinear(const std::vector<float>& in, double inRate,
                           double outRate, std::vector<float>& out) {
    if (in.empty() || inRate == outRate) {
        out = in;
        return;
    }
    const double ratio = inRate / outRate;
    const size_t nOut = (size_t)((double)in.size() / ratio);
    out.resize(nOut);
    for (size_t i = 0; i < nOut; ++i) {
        const double p = (double)i * ratio;
        const size_t i0 = (size_t)p;
        const double fr = p - (double)i0;
        const float a = in[i0];
        const float b = (i0 + 1 < in.size()) ? in[i0 + 1] : a;
        out[i] = a + (b - a) * (float)fr;
    }
}

// ---------------------------------------------------------------------------
// Slicing
// ---------------------------------------------------------------------------
struct Slice {
    int start = 0, end = 0; // sample indices into the buffer
    float peak = 0.0f, rms = 0.0f;
};

// Onset detection: RMS-energy flux with an adaptive threshold, computed once
// at load. Returns (strength, samplePos) pairs, strongest first is NOT
// guaranteed — caller sorts/selects.
inline std::vector<std::pair<float, int>>
detectOnsets(const std::vector<float>& mono, double sr) {
    const int hop = 256;
    const int nHops = (int)(mono.size() / (size_t)hop);
    std::vector<std::pair<float, int>> onsets;
    if (nHops < 8) return onsets;
    std::vector<float> flux((size_t)nHops, 0.0f);
    float prevE = 0.0f, maxF = 0.0f;
    for (int h = 0; h < nHops; ++h) {
        double e = 0.0;
        const float* p = mono.data() + (size_t)h * hop;
        for (int i = 0; i < hop; ++i) e += (double)p[i] * p[i];
        e = std::sqrt(e / hop);
        float f = (float)(e - prevE);
        prevE = (float)e;
        if (f < 0.0f) f = 0.0f;
        flux[(size_t)h] = f;
        if (f > maxF) maxF = f;
    }
    if (maxF < 1e-5f) return onsets; // silence: no transients at all
    double mean = 0.0;
    for (float f : flux) mean += f;
    mean /= nHops;
    double var = 0.0;
    for (float f : flux) var += (f - mean) * (f - mean);
    var /= nHops;
    const float thresh = (float)(mean + 1.1 * std::sqrt(var));
    const int minSpacing = std::max(1, (int)(0.03 * sr / hop));
    for (int h = 0; h < nHops; ++h) {
        const bool isMax = (h == 0 || flux[(size_t)h] >= flux[(size_t)h - 1]) &&
                           (h == nHops - 1 || flux[(size_t)h] >= flux[(size_t)h + 1]);
        if (flux[(size_t)h] > thresh && isMax) {
            if (!onsets.empty() && h - onsets.back().second < minSpacing) {
                if (flux[(size_t)h] > onsets.back().first)
                    onsets.back() = {flux[(size_t)h], h};
            } else {
                onsets.push_back({flux[(size_t)h], h});
            }
        }
    }
    std::vector<std::pair<float, int>> result;
    result.reserve(onsets.size());
    for (auto& o : onsets) result.push_back({o.first, o.second * hop});
    return result;
}

inline std::vector<Slice> buildSlices(const std::vector<float>& l,
                                      const std::vector<float>& r, double sr) {
    const int N = (int)l.size();
    std::vector<Slice> slices;
    if (N <= 0) return slices;
    std::vector<float> mono((size_t)N);
    for (int i = 0; i < N; ++i) mono[(size_t)i] = 0.5f * (l[(size_t)i] + r[(size_t)i]);
    auto onsets = detectOnsets(mono, sr);
    const double durSec = (double)N / sr;
    std::vector<int> starts;
    if (durSec < 2.0 && onsets.size() <= 1) {
        starts.push_back(0); // one-shot: the whole file is one slice
    } else if (onsets.size() >= 4) {
        if (onsets.size() > 16) { // keep the 16 strongest, in time order
            std::nth_element(onsets.begin(), onsets.begin() + 16, onsets.end(),
                             [](const auto& a, const auto& b) {
                                 return a.first > b.first;
                             });
            onsets.resize(16);
        }
        for (auto& o : onsets) starts.push_back(o.second);
        std::sort(starts.begin(), starts.end());
    } else {
        for (int i = 0; i < 16; ++i) // grid fallback
            starts.push_back((int)((int64_t)i * N / 16));
    }
    const int minLen = std::max(1, (int)(0.005 * sr));
    for (size_t i = 0; i < starts.size(); ++i) {
        const int s = starts[i];
        const int e = (i + 1 < starts.size()) ? starts[i + 1] : N;
        if (e - s < minLen) continue;
        Slice sl;
        sl.start = s;
        sl.end = e;
        double peak = 0.0, sum = 0.0;
        for (int j = s; j < e; ++j) {
            const float m = 0.5f * (fabsf(l[(size_t)j]) + fabsf(r[(size_t)j]));
            if (m > peak) peak = m;
            sum += (double)m * m;
        }
        sl.peak = (float)peak;
        sl.rms = (float)std::sqrt(sum / (double)(e - s));
        slices.push_back(sl);
    }
    return slices;
}

// ---------------------------------------------------------------------------
// Parameters
// ---------------------------------------------------------------------------
enum class PitchSnap { Glide, Semitone, Scale }; // Scale = minor pentatonic

struct Params {
    float chaos = 0.0f;      // 0..1 — the macro: order -> beautiful chaos
    float density = 0.5f;    // stutter/retrigger trim, 0..1
    float pitchTrim = 0.5f;  // pitch deviation trim, 0..1
    float degradeTrim = 0.5f;// bitcrush/decimation trim, 0..1
    PitchSnap pitchSnap = PitchSnap::Semitone;
    float bpm = 120.0f;
};

// ---------------------------------------------------------------------------
// Voice — one triggered slice.
// ---------------------------------------------------------------------------
struct Voice {
    bool active = false;
    int sliceIdx = -1;
    bool reverse = false;
    double pos = 0.0;   // position within the slice, in samples
    double rate = 1.0;  // signed playback rate (+ forward, - reverse)
    int repeatsLeft = 1;
    int repeatLen = 1;  // output samples per repeat
    int repeatPos = 0;
    int played = 0;     // samples played in the current repeat (for fades)
    int bits = 16;
    int decim = 1, decimCnt = 0;
    float holdL = 0.0f, holdR = 0.0f;
    int age = 0;
};

inline float crushSample(float x, int bits) {
    if (bits >= 24) return x;
    const float levels = std::ldexp(1.0f, bits - 1); // 2^(bits-1)
    float y = std::round(x * levels) / levels;
    return y > 1.0f ? 1.0f : (y < -1.0f ? -1.0f : y);
}

// ---------------------------------------------------------------------------
// Engine
// ---------------------------------------------------------------------------
class Engine {
public:
    static constexpr int kMaxVoices = 8;
    static constexpr int kMaxSlices = 16;
    static constexpr double kMaxLoadSeconds = 90.0;

    Engine() { reset(); }

    void setSampleRate(double sr) {
        sampleRate_ = sr > 0.0 ? sr : 48000.0;
        reset();
    }

    // Load from decoded float audio. Resamples, caps length, normalizes,
    // then slices. Returns false if there is nothing usable.
    bool loadWavData(const WavData& wav) {
        if (wav.l.empty() || wav.sampleRate <= 0.0) return false;
        resampleLinear(wav.l, wav.sampleRate, sampleRate_, bufL_);
        resampleLinear(wav.r, wav.sampleRate, sampleRate_, bufR_);
        const size_t cap = (size_t)(kMaxLoadSeconds * sampleRate_);
        truncated_ = bufL_.size() > cap;
        if (truncated_) {
            bufL_.resize(cap);
            bufR_.resize(cap);
        }
        float peak = 0.0f; // auto-normalize: garbage in, music out
        for (size_t i = 0; i < bufL_.size(); ++i) {
            const float a = fabsf(bufL_[i]), b = fabsf(bufR_[i]);
            if (a > peak) peak = a;
            if (b > peak) peak = b;
        }
        if (peak > 1e-6f) {
            const float g = 0.89f / peak;
            for (size_t i = 0; i < bufL_.size(); ++i) {
                bufL_[i] *= g;
                bufR_[i] *= g;
            }
        }
        slices_ = buildSlices(bufL_, bufR_, sampleRate_);
        reset();
        return !slices_.empty();
    }

    // Load from a WAV file in memory.
    bool loadWavBytes(const uint8_t* data, size_t size) {
        WavData wav;
        if (!parseWav(data, size, wav)) return false;
        return loadWavData(wav);
    }

    void setParams(const Params& p) { params_ = p; }
    void setChaos(float c) { params_.chaos = c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : c); }

    void reset(uint32_t seed = 0xBEA7u) {
        rng_.seed(seed);
        seqPos_ = 0;
        samplesToStep_ = 1; // trigger immediately on first block
        for (auto& v : voices_) v.active = false;
    }

    int numSlices() const { return (int)slices_.size(); }
    const Slice& slice(int i) const { return slices_[(size_t)i]; }
    bool wasTruncated() const { return truncated_; }
    int numVoicesActive() const {
        int n = 0;
        for (auto& v : voices_) n += v.active ? 1 : 0;
        return n;
    }

    void process(float* outL, float* outR, int n) {
        for (int i = 0; i < n; ++i) {
            if (--samplesToStep_ <= 0) {
                triggerStep();
                samplesToStep_ += stepSamples();
            }
            float l = 0.0f, r = 0.0f;
            for (auto& v : voices_)
                if (v.active) renderVoice(v, l, r);
            outL[i] = std::tanh(l);
            outR[i] = std::tanh(r);
        }
    }

private:
    int stepSamples() const {
        const float bpm = params_.bpm > 0.0f ? params_.bpm : 120.0f;
        return std::max(1, (int)(sampleRate_ * 60.0 / bpm / 4.0)); // 16ths
    }

    void triggerStep() {
        const int ns = numSlices();
        if (ns == 0) return;
        const float chaos = params_.chaos;
        int idx;
        if (chaos <= 0.0001f) {
            idx = seqPos_ % ns; // faithful: in order, no transforms
            seqPos_++;
        } else {
            const float rp = std::pow(chaos, 1.5f);
            if (rng_.uniform() < rp)
                idx = weightedPick();
            else {
                idx = seqPos_ % ns;
                seqPos_++;
            }
        }
        float semis = 0.0f;
        bool reverse = false;
        int repeats = 1, bits = 16, decim = 1;
        if (chaos > 0.0001f) {
            semis = quantizePitch(rng_.bilateral() * 24.0f * chaos *
                                  (0.25f + 0.75f * params_.pitchTrim));
            reverse = rng_.uniform() < chaos * 0.5f;
            const float sp = chaos * chaos * (0.25f + 0.75f * params_.density);
            if (rng_.uniform() < sp * 0.7f)
                repeats = 2 + (int)(rng_.uniform() * 3.0f); // 2..4
            const float dg = chaos * (0.25f + 0.75f * params_.degradeTrim);
            bits = 16 - (int)(dg * 12.0f + 0.5f);
            if (bits < 4) bits = 4;
            decim = 1 + (int)(dg * 7.0f + 0.5f);
        }
        startVoice(idx, semis, reverse, repeats, bits, decim);
    }

    int weightedPick() {
        const int ns = numSlices();
        float total = 0.0f;
        for (auto& s : slices_) total += s.peak + 1e-4f;
        float r = rng_.uniform() * total;
        for (int i = 0; i < ns; ++i) {
            r -= slices_[(size_t)i].peak + 1e-4f;
            if (r <= 0.0f) return i;
        }
        return ns - 1;
    }

    float quantizePitch(float semis) {
        switch (params_.pitchSnap) {
            case PitchSnap::Glide: return semis;
            case PitchSnap::Semitone: return std::round(semis);
            case PitchSnap::Scale: {
                static const int pcs[5] = {0, 3, 5, 7, 10}; // minor pentatonic
                const int s = (int)std::round(semis);
                const int oct = s >= 0 ? s / 12 : -((11 - s) / 12);
                const int pc = s - oct * 12;
                int best = pcs[0], bd = 12;
                for (int p : pcs) {
                    const int d = std::abs(pc - p);
                    if (d < bd) {
                        bd = d;
                        best = p;
                    }
                }
                return (float)(oct * 12 + best);
            }
        }
        return semis;
    }

    void startVoice(int sliceIdx, float semis, bool reverse, int repeats,
                    int bits, int decim) {
        Voice* v = nullptr;
        for (auto& c : voices_)
            if (!c.active) {
                v = &c;
                break;
            }
        if (!v) { // steal oldest
            v = &voices_[0];
            for (auto& c : voices_)
                if (c.age > v->age) v = &c;
        }
        const Slice& s = slices_[(size_t)sliceIdx];
        const int len = s.end - s.start;
        if (len <= 0) return;
        const double ratio = std::pow(2.0, (double)semis / 12.0);
        v->active = true;
        v->sliceIdx = sliceIdx;
        v->reverse = reverse;
        v->rate = reverse ? -ratio : ratio;
        v->pos = reverse ? (double)(len - 1) : 0.0;
        v->repeatsLeft = repeats;
        v->repeatLen = std::max(1, stepSamples() / repeats);
        v->repeatPos = 0;
        v->played = 0;
        v->bits = bits;
        v->decim = decim;
        v->decimCnt = 0;
        v->holdL = v->holdR = 0.0f;
        v->age = 0;
    }

    void renderVoice(Voice& v, float& l, float& r) {
        const Slice& s = slices_[(size_t)v.sliceIdx];
        const int len = s.end - s.start;
        int i0 = (int)v.pos;
        if (i0 < 0) i0 = 0;
        if (i0 >= len - 1) i0 = len - 2;
        const double fr = v.pos - (double)((int)v.pos);
        const size_t b0 = (size_t)(s.start + i0);
        float sl = bufL_[b0] * (float)(1.0 - fr) + bufL_[b0 + 1] * (float)fr;
        float sr = bufR_[b0] * (float)(1.0 - fr) + bufR_[b0 + 1] * (float)fr;
        // click-free fades at repeat boundaries
        const int fade = std::max(1, std::min((int)(0.002 * sampleRate_), v.repeatLen / 4));
        float env = 1.0f;
        if (v.played < fade) env = (float)v.played / (float)fade;
        const int remain = v.repeatLen - v.repeatPos;
        if (remain < fade) env = std::min(env, (float)remain / (float)fade);
        // decimation (sample-rate crush)
        if (v.decim > 1) {
            if (v.decimCnt % v.decim == 0) {
                v.holdL = sl;
                v.holdR = sr;
            }
            v.decimCnt++;
            sl = v.holdL;
            sr = v.holdR;
        }
        sl = crushSample(sl, v.bits);
        sr = crushSample(sr, v.bits);
        l += sl * env;
        r += sr * env;
        // advance
        v.pos += v.rate;
        if (v.pos >= len) v.pos -= len;
        else if (v.pos < 0.0) v.pos += len;
        v.played++;
        v.repeatPos++;
        v.age++;
        if (v.repeatPos >= v.repeatLen) {
            v.repeatPos = 0;
            v.played = 0;
            v.pos = v.reverse ? (double)(len - 1) : 0.0;
            if (--v.repeatsLeft <= 0) v.active = false;
        }
    }

    double sampleRate_ = 48000.0;
    Params params_;
    Rng rng_;
    std::vector<float> bufL_, bufR_;
    std::vector<Slice> slices_;
    std::vector<Voice> voices_ = std::vector<Voice>(kMaxVoices);
    int seqPos_ = 0;
    int samplesToStep_ = 1;
    bool truncated_ = false;
};

} // namespace beatmangler
