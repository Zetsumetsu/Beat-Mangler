// Headless smoke test for the Beat Mangler VST3.
//
// Two phases:
//  1. Bundle phase: load the built .vst3, instantiate, verify identity,
//     parameters, and that silence-with-no-file is finite.
//  2. Direct phase: instantiate BeatManglerProcessor in-process, load a
//     real WAV through the file path, render audio at two chaos settings,
//     verify it is sane, then create the REAL editor, paint it, and check
//     the result is non-blank.
//
// Usage: BeatManglerSmokeTest <path-to-.vst3>
#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdio>
#include <cmath>
#include <vector>

#include "PluginProcessor.h"
#include "PluginEditor.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("ok: %s\n", msg); } \
} while (0)

// 2s synthetic break at 120bpm: kick quarters, snare 2+4, hat 8ths
static void writeTestBreak(const char* path) {
    const double sr = 48000.0;
    const int N = (int)(2.0 * sr);
    std::vector<int16_t> pcm((size_t)N * 2, 0);
    auto add = [&](double t, auto fn) {
        int s = (int)(t * sr);
        for (int i = 0; s + i < N; ++i) {
            float v = fn(i / sr);
            size_t o = (size_t)(s + i) * 2;
            int nv = pcm[o] + (int)(v * 20000.0f);
            if (nv > 32767) nv = 32767;
            if (nv < -32768) nv = -32768;
            pcm[o] = pcm[o + 1] = (int16_t)nv;
        }
    };
    double phase = 0.0;
    for (int q = 0; q < 4; ++q) { // kick
        double t = q * 0.5;
        add(t, [&](double tt) {
            if (tt > 0.3) return 0.0f;
            double f = 45.0 + 110.0 * std::exp(-tt * 30.0);
            phase += 2 * 3.141592653589793 * f / 48000.0;
            return (float)(std::sin(phase) * std::exp(-tt * 12.0) * 0.9);
        });
    }
    for (double t : {0.5, 1.5}) { // snare
        add(t, [](double tt) {
            if (tt > 0.2) return 0.0f;
            return (float)(((rand() / (double)RAND_MAX) * 2 - 1) * 0.4 *
                           std::exp(-tt * 22.0));
        });
    }
    for (int e = 0; e < 8; ++e) { // hats
        double t = e * 0.25;
        add(t, [](double tt) {
            if (tt > 0.05) return 0.0f;
            return (float)(((rand() / (double)RAND_MAX) * 2 - 1) * 0.3 *
                           std::exp(-tt * 90.0));
        });
    }
    FILE* f = std::fopen(path, "wb");
    auto w32 = [&](uint32_t v) {
        uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16),
                        (uint8_t)(v >> 24)};
        std::fwrite(b, 1, 4, f);
    };
    auto w16 = [&](uint16_t v) {
        uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
        std::fwrite(b, 1, 2, f);
    };
    std::fwrite("RIFF", 1, 4, f);
    w32(36 + N * 4);
    std::fwrite("WAVEfmt ", 1, 8, f);
    w32(16);
    w16(1);
    w16(2);
    w32(48000);
    w32(192000);
    w16(4);
    w16(16);
    std::fwrite("data", 1, 4, f);
    w32(N * 4);
    std::fwrite(pcm.data(), 2, pcm.size(), f);
    std::fclose(f);
}

static bool finiteBuf(juce::AudioBuffer<float>& buf) {
    for (int ch = 0; ch < buf.getNumChannels(); ++ch) {
        auto* d = buf.getReadPointer(ch);
        for (int n = 0; n < buf.getNumSamples(); ++n)
            if (!std::isfinite(d[n])) return false;
    }
    return true;
}
static float peakBuf(juce::AudioBuffer<float>& buf) {
    float p = 0.f;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch) {
        auto* d = buf.getReadPointer(ch);
        for (int n = 0; n < buf.getNumSamples(); ++n) {
            float a = std::abs(d[n]);
            if (a > p) p = a;
        }
    }
    return p;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: BeatManglerSmokeTest <path-to-.vst3>\n");
        return 2;
    }
    juce::ScopedJuceInitialiser_GUI guiInit;

    // ---- phase 1: bundle ----
    {
        juce::VST3PluginFormat format;
        juce::OwnedArray<juce::PluginDescription> descs;
        format.findAllTypesForFile(descs, argv[1]);
        CHECK(!descs.isEmpty(), "bundle: plugin found");
        if (descs.isEmpty()) return 1;
        juce::String err;
        std::unique_ptr<juce::AudioPluginInstance> inst(
            format.createInstanceFromDescription(*descs[0], 48000.0, 512, err));
        CHECK(inst != nullptr, "bundle: instantiate");
        if (!inst) return 1;
        CHECK(inst->getName() == "Beat Mangler", "bundle: name is Beat Mangler");
        // 6 of ours + host-added Bypass
        CHECK(inst->getParameters().size() == 7, "bundle: 7 params (6 + Bypass)");
        inst->prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        inst->processBlock(buf, midi);
        CHECK(finiteBuf(buf) && peakBuf(buf) == 0.f,
              "bundle: silent + finite with no file");
    }

    // ---- phase 2: direct processor + real editor ----
    const char* wavPath = "/tmp/bm_test_break.wav";
    writeTestBreak(wavPath);

    BeatManglerProcessor proc;
    proc.prepareToPlay(48000.0, 512);

    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    proc.processBlock(buf, midi);
    CHECK(finiteBuf(buf) && peakBuf(buf) == 0.f, "direct: silent + finite, no file");

    CHECK(proc.loadAudioFile(juce::File(wavPath)), "direct: WAV loads");
    int ns = proc.sliceCount.load();
    std::printf("  slices: %d\n", ns);
    CHECK(ns >= 4, "direct: slices found");

    proc.playing.store(true);
    auto setParam = [&](const char* id, float v) {
        if (auto* p = proc.apvts.getParameter(id))
            p->setValueNotifyingHost(v);
    };
    // render 4s at chaos 0
    setParam("chaos", 0.0f);
    float peak0 = 0.f;
    bool fin0 = true;
    for (int b = 0; b < 375; ++b) {
        buf.clear();
        proc.processBlock(buf, midi);
        fin0 &= finiteBuf(buf);
        peak0 = std::max(peak0, peakBuf(buf));
    }
    std::printf("  chaos=0 peak: %.3f\n", peak0);
    CHECK(fin0 && peak0 > 0.01f && peak0 <= 1.0f, "direct: chaos=0 renders sane audio");

    // render 4s at chaos 1
    setParam("chaos", 1.0f);
    float peak1 = 0.f;
    bool fin1 = true;
    for (int b = 0; b < 375; ++b) {
        buf.clear();
        proc.processBlock(buf, midi);
        fin1 &= finiteBuf(buf);
        peak1 = std::max(peak1, peakBuf(buf));
    }
    std::printf("  chaos=1 peak: %.3f\n", peak1);
    CHECK(fin1 && peak1 > 0.01f && peak1 <= 1.0f, "direct: chaos=1 renders sane audio");

    // real editor
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    CHECK(ed != nullptr, "direct: editor created");
    if (ed) {
        CHECK(ed->getWidth() == 640 && ed->getHeight() == 440, "direct: editor 640x440");
        juce::Image img(juce::Image::ARGB, ed->getWidth(), ed->getHeight(), true);
        {
            juce::Graphics g(img);
            ed->paintEntireComponent(g, false);
        }
        juce::Image::BitmapData bd(img, juce::Image::BitmapData::readOnly);
        bool nonBlank = false;
        int distinct = 0;
        juce::uint32 last = 0;
        for (int y = 0; y < bd.height && distinct < 8; y += 5) {
            for (int x = 0; x < bd.width && distinct < 8; x += 5) {
                auto c = bd.getPixelColour(x, y).getARGB();
                if (c != 0xff000000) nonBlank = true;
                if (c != last) {
                    distinct++;
                    last = c;
                }
            }
        }
        std::printf("  editor non-blank=%d distinct-colors=%d\n", (int)nonBlank,
                    distinct);
        CHECK(nonBlank && distinct >= 4, "direct: editor paints real GUI");
        // control: direct virtual paint() call on the real editor
        {
            juce::Image imgD(juce::Image::ARGB, ed->getWidth(), ed->getHeight(),
                             true);
            {
                juce::Graphics gD(imgD);
                ed->paint(gD);
            }
            juce::Image::BitmapData bdD(imgD, juce::Image::BitmapData::readOnly);
            auto px = bdD.getPixelColour(320, 200).getARGB();
            std::printf("  direct paint() pixel(320,200): %08x (want ff0e0e11)\n",
                        (unsigned)px);
            if (auto os = juce::File("/tmp/bm-direct.png").createOutputStream()) {
                juce::PNGImageFormat png;
                png.writeImageToStream(imgD, *os);
            }
        }
        // sanctioned snapshot path
        {
            juce::Image snap = ed->createComponentSnapshot(ed->getLocalBounds(),
                                                           true, 1.0f);
            {
                juce::Image::BitmapData bdS(
                    snap, juce::Image::BitmapData::readOnly);
                auto px = bdS.getPixelColour(320, 200).getARGB();
                std::printf("  snapshot pixel(320,200): %08x\n", (unsigned)px);
            } // BitmapData released BEFORE the file write
            { // raw PPM dump: no JUCE image writer involved
                auto ppm = juce::File("/tmp/beat-mangler-editor.ppm").createOutputStream();
                if (ppm) {
                    juce::String hdr("P6\n640 440\n255\n");
                    ppm->write(hdr.toRawUTF8(), hdr.getNumBytesAsUTF8());
                    juce::Image::BitmapData bdP(
                        snap, juce::Image::BitmapData::readOnly);
                    for (int y = 0; y < 440; ++y)
                        for (int x = 0; x < 640; ++x) {
                            auto c = bdP.getPixelColour(x, y);
                            uint8_t rgb[3] = {c.getRed(), c.getGreen(),
                                              c.getBlue()};
                            ppm->write(rgb, 3);
                        }
                    ppm->flush();
                    std::printf("  editor ppm: /tmp/beat-mangler-editor.ppm\n");
                }
            }
        }
    }

    std::printf(failures ? "\n%d FAILURES\n" : "\nSMOKE TEST PASS\n", failures);
    return failures ? 1 : 0;
}
