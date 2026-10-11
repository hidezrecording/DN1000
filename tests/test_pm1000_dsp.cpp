// tests/test_pm1000_dsp.cpp — framework-free DSP tests for the
// dual-channel PM1000 rack strip (researched spec).
// Compile: g++ -std=c++17 -O2 -o build/test_dsp tests/test_pm1000_dsp.cpp dsp/Pm1000Dsp.cpp
#include "../dsp/Pm1000Dsp.h"
#include <cstdio>
#include <cmath>

static int failures = 0;
static void check(bool ok, const char* name, const char* detail = "") {
    std::printf("%s %-44s %s\n", ok ? "PASS" : "FAIL", name, detail);
    if (!ok) ++failures;
}

static float measureRms(pm1000::Channel& ch, float freq, float amp,
                        double fs, int nWarm = 9600, int nMeas = 48000) {
    const double inc = 2.0 * 3.14159265358979 * freq / fs;
    double phase = 0.0, sum = 0.0;
    for (int i = 0; i < nWarm; ++i) { ch.processSample(amp * (float)std::sin(phase)); phase += inc; }
    for (int i = 0; i < nMeas; ++i) {
        float y = ch.processSample(amp * (float)std::sin(phase)); phase += inc;
        sum += (double)y * y;
    }
    return (float)std::sqrt(sum / nMeas);
}

static float measurePeak(pm1000::Channel& ch, float freq, float amp, double fs) {
    const double inc = 2.0 * 3.14159265358979 * freq / fs;
    double phase = 0.0; float peak = 0.0f;
    for (int i = 0; i < 96000; ++i) {
        float y = ch.processSample(amp * (float)std::sin(phase)); phase += inc;
        float a = std::fabs(y);
        if (a > peak) peak = a;
    }
    return peak;
}

static bool finite(float v) { return std::isfinite(v); }

static void testLpf() {
    // LPF (high cut): 16 kHz setting must attenuate 20 kHz vs OFF.
    const double fs = 48000.0;
    auto rmsAt = [&](int lpfMode, float freq) {
        pm1000::Channel ch; ch.prepare(fs);
        pm1000::ChannelParams p;
        p.inputDetent = 9; p.lpfMode = lpfMode;
        ch.setParams(p);
        const int n = (int)(fs * 0.05);
        double sum = 0.0;
        for (int i = 0; i < n; ++i) {
            float s = std::sin(2.0f * 3.14159265f * freq * (float)i / (float)fs);
            float y = ch.processSample(0.1f * s);
            if (i > n / 2) sum += (double)y * (double)y;
        }
        return std::sqrt(sum / (n - n / 2));
    };
    float off = rmsAt(0, 20000.0f);
    float cut = rmsAt(2, 20000.0f);
    char b[96]; std::snprintf(b, sizeof b, "off=%.4f cut=%.4f", off, cut);
    check(off > 0.001f, "lpf off passes 20k", b);
    check(cut < off * 0.5f, "lpf 16k cuts 20k", b);
}

int main() {
    const double fs = 48000.0;
    char buf[160];

    // 1. Detent gain table: 76,66,60,54,48,42,36,30,24,12 dB
    {
        const float expect[10] = {76,66,60,54,48,42,36,30,24,12};
        bool ok = true;
        for (int d = 0; d < 10; ++d)
            if (std::fabs(pm1000::detentGainDb(d) - expect[d]) > 0.001f) ok = false;
        check(ok, "10-detent gains = 76..12 dB", "");
    }

    // 2. Flat at +4 dBm detent: 12 dB of clean gain (small signal)
    {
        pm1000::Channel ch; ch.prepare(fs);
        pm1000::ChannelParams p; p.inputDetent = 9; ch.setParams(p);
        float rms = measureRms(ch, 1000.0f, 0.05f, fs);
        float ratio = rms / (0.05f / (float)std::sqrt(2.0));
        std::snprintf(buf, sizeof buf, "ratio=%.2f (expect ~3.9)", ratio);
        check(ratio > 3.7f && ratio < 4.1f, "detent +4 dBm gives 12 dB", buf);
    }

    // 3. HPF 40 cuts 20 Hz; HPF 80 cuts 40 Hz (12 dB/oct)
    {
        pm1000::Channel ch; ch.prepare(fs);
        pm1000::ChannelParams p; p.inputDetent = 9; p.hpfMode = 1; ch.setParams(p);
        float r40 = measureRms(ch, 20.0f, 0.1f, fs);
        pm1000::Channel c2; c2.prepare(fs);
        pm1000::ChannelParams p2; p2.inputDetent = 9; c2.setParams(p2);
        float rOff = measureRms(c2, 20.0f, 0.1f, fs);
        std::snprintf(buf, sizeof buf, "20Hz ratio=%.3f", r40 / rOff);
        check(r40 / rOff < 0.40f, "HPF 40 Hz cuts 20 Hz", buf);

        pm1000::Channel ch3; ch3.prepare(fs);
        pm1000::ChannelParams p3; p3.inputDetent = 9; p3.hpfMode = 2; ch3.setParams(p3);
        float r80 = measureRms(ch3, 40.0f, 0.1f, fs);
        float rOff2 = measureRms(c2, 40.0f, 0.1f, fs);
        std::snprintf(buf, sizeof buf, "40Hz ratio=%.3f", r80 / rOff2);
        check(r80 / rOff2 < 0.40f, "HPF 80 Hz cuts 40 Hz", buf);
    }

    // 4. Treble shelf +15 dB lifts 18 kHz, spares 1 kHz
    {
        pm1000::Channel ch; ch.prepare(fs);
        pm1000::ChannelParams p; p.inputDetent = 9; p.trebleDb = 15.0f; ch.setParams(p);
        float rHi = measureRms(ch, 18000.0f, 0.05f, fs);
        pm1000::Channel c2; c2.prepare(fs);
        pm1000::ChannelParams p2; p2.inputDetent = 9; c2.setParams(p2);
        float ratio = rHi / measureRms(c2, 18000.0f, 0.05f, fs);
        std::snprintf(buf, sizeof buf, "18kHz ratio=%.2f", ratio);
        check(ratio > 4.3f && ratio < 6.5f, "treble +15 dB lifts 18 kHz", buf);

        float rMid = measureRms(ch, 1000.0f, 0.05f, fs)
                   / measureRms(c2, 1000.0f, 0.05f, fs);
        std::snprintf(buf, sizeof buf, "1kHz ratio=%.2f", rMid);
        check(rMid > 0.9f && rMid < 1.1f, "treble shelf spares 1 kHz", buf);
    }

    // 5. Mid bell +12 dB at 2 kHz; selector moves it 1 kHz / 4 kHz
    {
        pm1000::Channel ch; ch.prepare(fs);
        pm1000::ChannelParams p; p.inputDetent = 9; p.midDb = 12.0f; p.midSel = 1;
        ch.setParams(p);
        pm1000::Channel c2; c2.prepare(fs);
        pm1000::ChannelParams p2; p2.inputDetent = 9; c2.setParams(p2);
        float ratio = measureRms(ch, 2000.0f, 0.05f, fs)
                    / measureRms(c2, 2000.0f, 0.05f, fs);
        std::snprintf(buf, sizeof buf, "2kHz ratio=%.2f", ratio);
        check(ratio > 3.5f && ratio < 4.5f, "mid +12 dB peaks at 2 kHz", buf);

        pm1000::Channel ch3; ch3.prepare(fs);
        pm1000::ChannelParams p3; p3.inputDetent = 9; p3.midDb = 12.0f; p3.midSel = 0;
        ch3.setParams(p3);
        float r1k = measureRms(ch3, 1000.0f, 0.05f, fs) / measureRms(c2, 1000.0f, 0.05f, fs);
        float r4k = measureRms(ch3, 4000.0f, 0.05f, fs) / measureRms(c2, 4000.0f, 0.05f, fs);
        std::snprintf(buf, sizeof buf, "sel=1kHz: 1k=%.2f 4k=%.2f", r1k, r4k);
        check(r1k > 3.5f && r4k < 2.0f, "mid selector 1 kHz position", buf);

        pm1000::ChannelParams p4; p4.inputDetent = 9; p4.midDb = 12.0f; p4.midSel = 2;
        ch3.setParams(p4);
        float r1kB = measureRms(ch3, 1000.0f, 0.05f, fs) / measureRms(c2, 1000.0f, 0.05f, fs);
        float r4kB = measureRms(ch3, 4000.0f, 0.05f, fs) / measureRms(c2, 4000.0f, 0.05f, fs);
        std::snprintf(buf, sizeof buf, "sel=4kHz: 1k=%.2f 4k=%.2f", r1kB, r4kB);
        check(r4kB > 3.5f && r1kB < 2.0f, "mid selector 4 kHz position", buf);
    }

    // 6. Bass shelf +15 dB lifts 40 Hz
    {
        pm1000::Channel ch; ch.prepare(fs);
        pm1000::ChannelParams p; p.inputDetent = 9; p.bassDb = 15.0f; ch.setParams(p);
        pm1000::Channel c2; c2.prepare(fs);
        pm1000::ChannelParams p2; p2.inputDetent = 9; c2.setParams(p2);
        float ratio = measureRms(ch, 40.0f, 0.05f, fs)
                    / measureRms(c2, 40.0f, 0.05f, fs);
        std::snprintf(buf, sizeof buf, "40Hz ratio=%.2f", ratio);
        check(ratio > 4.0f && ratio < 6.5f, "bass +15 dB lifts 40 Hz", buf);
    }

    // 7. Per-channel GAIN knob: 7 = unity, 10 = +3.1 dB, 0 = kill
    {
        pm1000::Channel ch; ch.prepare(fs);
        pm1000::ChannelParams p; p.inputDetent = 9; p.gain = 7.0f; ch.setParams(p);
        float r7 = measureRms(ch, 1000.0f, 0.01f, fs);
        p.gain = 10.0f; ch.setParams(p);
        float ratio = measureRms(ch, 1000.0f, 0.01f, fs) / r7;
        std::snprintf(buf, sizeof buf, "gain 10/7 ratio=%.3f", ratio);
        check(ratio > 1.35f && ratio < 1.50f, "gain 10 = +3.1 dB over 7", buf);

        p.gain = 0.0f; ch.setParams(p);
        float r0 = measureRms(ch, 1000.0f, 0.5f, fs);
        check(r0 < 1e-4f, "gain full-down kills output", "");
    }

    // 8. Driving the -60 dBm detent slams the preamp into the rails
    // (then the output section colors it — still saturated, far from linear)
    {
        pm1000::Channel ch; ch.prepare(fs);
        pm1000::ChannelParams p; p.inputDetent = 0; ch.setParams(p);
        float peak = measurePeak(ch, 1000.0f, 0.01f, fs);
        std::snprintf(buf, sizeof buf, "peak=%.2f (linear would be 63)", peak);
        check(peak > 0.7f && peak < 0.95f, "hot preamp saturates", buf);
    }

    // 9. Channel VU tap reads sane level (peak-biased; GUI does ballistics).
    // 0.02 in at +4 dBm detent (+12 dB) peaks at -22 dBFS -> about -4 VU.
    {
        pm1000::Channel ch; ch.prepare(fs);
        pm1000::ChannelParams p; p.inputDetent = 9; ch.setParams(p);
        for (int i = 0; i < 48000; ++i)
            ch.processSample(0.02f * (float)std::sin(i * 2.0 * 3.14159265 * 440.0 / fs));
        float vu = ch.vuDb();
        std::snprintf(buf, sizeof buf, "vu=%.1f dB", vu);
        check(vu > -6.0f && vu < -2.0f, "channel VU tap reads peak level", buf);
    }

    // 10. Master stage: unity at 7, transformer stays musical hot
    {
        pm1000::MasterStage m; m.prepare(fs); m.setLevel(7.0f);
        float L, R;
        m.process(0.05f, -0.05f, L, R);
        std::snprintf(buf, sizeof buf, "L=%.4f R=%.4f", L, R);
        check(std::fabs(L - 0.05f) < 0.005f && std::fabs(R + 0.05f) < 0.005f,
              "master unity at 7", buf);
        m.process(2.0f, 2.0f, L, R);
        std::snprintf(buf, sizeof buf, "hot L=%.3f", L);
        check(std::fabs(L - 2.0f) < 0.01f, "master passes through cleanly", buf);
        m.setLevel(0.0f);
        m.process(0.5f, 0.5f, L, R);
        check(L == 0.0f && R == 0.0f, "master full-down mutes", "");
    }

    // 11. Dual channels are independent
    {
        pm1000::Channel a, b; a.prepare(fs); b.prepare(fs);
        pm1000::ChannelParams pa; pa.inputDetent = 9; pa.bassDb = 12.0f; a.setParams(pa);
        pm1000::ChannelParams pb; pb.inputDetent = 9; b.setParams(pb);
        float ra = measureRms(a, 50.0f, 0.05f, fs);
        float rb = measureRms(b, 50.0f, 0.05f, fs);
        std::snprintf(buf, sizeof buf, "A=%.4f B=%.4f", ra, rb);
        check(ra > rb * 2.5f, "channels process independently", buf);
    }

    // 12. MIC mode adds +30 dB preamp gain
    {
        pm1000::Channel ch; ch.prepare(fs);
        pm1000::ChannelParams p; p.inputDetent = 9; p.mic = false; ch.setParams(p);
        float rLine = measureRms(ch, 1000.0f, 0.001f, fs);
        p.mic = true; ch.setParams(p);
        float ratio = measureRms(ch, 1000.0f, 0.001f, fs) / rLine;
        char b2[80]; std::snprintf(b2, sizeof b2, "mic/line ratio=%.1f", ratio);
        check(ratio > 28.0f && ratio < 35.0f, "mic mode = +30 dB boost", b2);
    }

    // 13. Output modes: all finite, all distinct
    {
        pm1000::Channel ch; ch.prepare(fs);
        float rms[3];
        for (int m = 0; m < 3; ++m) {
            pm1000::ChannelParams p; p.inputDetent = 5; p.gain = 9.0f;
            p.outMode = m; ch.setParams(p);
            rms[m] = measureRms(ch, 440.0f, 0.05f, fs);
        }
        bool ok = true;
        for (int m = 0; m < 3; ++m)
            if (!(rms[m] > 0.001f && rms[m] < 10.0f)) ok = false;
        char b3[96]; std::snprintf(b3, sizeof b3, "rms=%.3f/%.3f/%.3f",
                                   rms[0], rms[1], rms[2]);
        check(ok, "output modes finite", b3);
        check(std::fabs(rms[0] - rms[1]) > 1e-4f &&
              std::fabs(rms[1] - rms[2]) > 1e-4f, "modes differ", b3);
    }

    // 14. Extreme settings stay finite
    {
        pm1000::Channel ch; ch.prepare(fs);
        pm1000::ChannelParams p;
        p.inputDetent = 0; p.bassDb = 15.0f; p.midDb = -15.0f;
        p.trebleDb = 15.0f; p.gain = 10.0f; p.hpfMode = 2;
        ch.setParams(p);
        pm1000::MasterStage m; m.prepare(fs); m.setLevel(10.0f);
        bool ok = true;
        for (int i = 0; i < 48000 && ok; ++i) {
            float y = ch.processSample(0.9f * (float)std::sin(i * 0.1));
            float L, R; m.process(y, -y, L, R);
            if (!finite(y) || !finite(L) || !finite(R)) ok = false;
        }
        check(ok, "extreme settings stay finite", "");
    }

    std::printf(failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
