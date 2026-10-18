#pragma once
// ─────────────────────────────────────────────────────────────────────
// Pm1000Dsp — framework-free C++17 DSP core for the Yamaha PM1000
// dual-channel rack strip clone (DN1000).
//
// Two independent channels (A/B), each:
//   INPUT LEVEL (10-detent stepped gain) → input transformer →
//   discrete preamp stages → PHASE → HPF → LPF → 3-band inductor EQ
//   → post-EQ stage → per-channel GAIN → output color (T/C/S) → VU tap
// Then the master section: OUTPUT (fixed unity; per-channel knobs
// do the work) → stereo out.
//
// Analog behavior pass (2026-10-12, Nathan-approved, in order):
//   1. ONE shared power supply for both channels: rail sags under the
//      combined load and modulates stage/transformer headroom — the
//      channels breathe together without any signal crosstalk.
//   2. Transistor stages have bias memory: loud signal shifts each
//      stage's operating point, which droops and recovers (~80 ms) —
//      real "blocking" behavior, not a static curve.
//   3. The channel chain runs at 2x the host rate (windowed-sinc FIR
//      resampling) so the nonlinear stages don't alias.
//   4. One gain stage sits AFTER the EQ (EQ energy pushes it, like
//      the hardware), and the bass inductor bump saturates with LF
//      level — the EQ shape is level-dependent.
//   5. EQ knob parameters are smoothed and filter coefficients are
//      refreshed on a decimated schedule: no block-rate stepping.
//   6. Every time constant is derived from the sample rate (seconds,
//      not per-sample magic numbers): the unit behaves the same at
//      44.1/48/96/192 kHz, matching its tuned 48 kHz behavior.
//   7. Hiss is generated per channel (independent seeds) and injected
//      at the first stage input, so it tracks INPUT/MIC gain staging.
//   8. Warm-up: after power-on the unit settles over a few seconds
//      (gain/bias drift), driven by the processor's power state.
//   9. Optional per-channel component-tolerance offsets (Ch B) —
//      real module pairs never match exactly.
//  10. Slew limiting scaled by sample rate so its V/us is constant
//      (48 kHz behavior preserved exactly).
//
// Spec basis: Yamaha PM1000 operation/service manual + Studio Sound
// bench review, Dec 1976 (research dossier 2026-10-11).
// ─────────────────────────────────────────────────────────────────────
#include <cmath>
#include <cstdint>

namespace pm1000 {

inline float dbToLinear(float db) { return std::pow(10.0f, db / 20.0f); }

// One-pole smoothing coefficient for a time constant in SECONDS at fs.
// This is the only sanctioned way to make a time constant in this file.
inline float tauCoef(float tauSec, double fs) {
    return 1.0f - (float)std::exp(-1.0 / ((double)tauSec * fs));
}
// One-pole lowpass pole for a corner frequency in Hz at fs.
inline float poleForCorner(float fcHz, double fs) {
    return (float)std::exp(-2.0 * 3.14159265358979323846 * (double)fcHz / fs);
}

// —————————————————————————————————————————————————————————————————————
// Biquad — RBJ cookbook, Direct Form I
// —————————————————————————————————————————————————————————————————————
struct Biquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

    void reset() { x1 = x2 = y1 = y2 = 0.0f; }

    void setLowShelf(double fs, double f0, float gainDb);
    void setHighShelf(double fs, double f0, float gainDb);
    void setPeak(double fs, double f0, float gainDb, float q);
    void setHighPass(double fs, double f0, float q = 0.7071f);
    void setLowPass(double fs, double f0, float q = 0.7071f);

    float process(float x) {
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return y;
    }
};

// —————————————————————————————————————————————————————————————————————
// Input-level detents. Nathan's mapping (2026-10-11): practical 4 dB
// steps, less boost/cut than the original PM1000 switching network.
// Detent 5 (12 o'clock) = 0 dB unity.
// —————————————————————————————————————————————————————————————————————
inline float detentGainDb(int d) {
    d = d < 0 ? 0 : (d > 9 ? 9 : d);
    static constexpr float gains[10] =
        {-20.f, -16.f, -12.f, -8.f, -4.f, 0.f, 4.f, 8.f, 12.f, 16.f};
    return gains[d];
}

inline constexpr float kMidFreqs[3] = { 1000.f, 2000.f, 4000.f }; // per manual + hardware photo

// 0 VU = +4 dBm = -18 dBFS reference for the meters.
inline constexpr float kVuRefDbFs = -18.0f;

// —————————————————————————————————————————————————————————————————————
// ChannelParams — one field per hardware control, per channel
// —————————————————————————————————————————————————————————————————————
struct ChannelParams {
    int   inputDetent = 5;     // 0..9, 5 = unity (12 o'clock)
    bool  mic         = false; // MIC/LINE: mic adds +30 dB preamp gain
    bool  phaseInvert = false; // N/R
    int   hpfMode     = 0;     // 0=off, 1=40 Hz, 2=80 Hz
    int   lpfMode     = 0;     // 0=off, 1=20 kHz, 2=16 kHz
    float bassDb      = 0.0f;  // ±15 dB shelf
    float midDb       = 0.0f;  // ±15 dB bell
    int   midSel      = 0;     // 0=1 kHz (default), 1=2 kHz, 2=4 kHz
    float trebleDb    = 0.0f;  // ±15 dB shelf
    float gain        = 5.0f;  // per-channel OUTPUT knob, 0..10 (5 = unity, 12 o'clock)
    int   outMode     = 0;     // 0=Tamura, 1=Cinemag, 2=Sumthang
};

// —————————————————————————————————————————————————————————————————————
// Power supply — ONE per rack, shared by both channels (item 1).
// Sags under the combined current demand; the rail modulates headroom
// in both channels. No audio signal crosses between channels — only
// the rail moves. Ripple intentionally not modeled (removed earlier:
// it caused idle oscillation and Nathan approved hiss-only).
// —————————————————————————————————————————————————————————————————————
struct PowerSupply {
    float env_  = 0.0f;  // demand envelope (sum of |channel outputs|)
    float rail_ = 1.0f;  // normalized rail (1.0 = nominal)

    // demand: |outA| + |outB| of the previous sample (0..~2)
    void update(float demand, double fs) {
        const float c = (demand > env_) ? tauCoef(0.005f, fs)   // fast attack
                                        : tauCoef(0.060f, fs);  // slow recovery
        env_ += (demand - env_) * c;
        const float d = env_ > 1.5f ? 1.5f : env_;
        const float target = 1.0f - d * 0.045f; // up to ~6.75% sag, slammed
        rail_ += (target - rail_) * tauCoef(0.030f, fs);
    }
    float getRail() const { return rail_; }
    void reset() { env_ = 0.0f; rail_ = 1.0f; }
};

// —————————————————————————————————————————————————————————————————————
// Noise: transistor hiss (white + 1/f). Xoroshiro-ish xorshift32 —
// long period, and each channel gets a DIFFERENT seed (item 7) so
// the two channels' hiss is statistically independent like hardware.
// —————————————————————————————————————————————————————————————————————
struct AnalogNoise {
    float pink1_ = 0.0f, pink2_ = 0.0f; // 1/f approximation states
    float pole1_ = 0.98f, pole2_ = 0.94f; // set from fs in prepare
    std::uint32_t state_ = 12345u;

    void prepare(double fs, std::uint32_t seed) {
        state_ = seed ? seed : 1u;
        // Preserve the tuned 48 kHz corners (154 Hz / 472 Hz) at any rate
        pole1_ = poleForCorner(154.0f, fs);
        pole2_ = poleForCorner(472.0f, fs);
    }
    float nextWhite() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return ((state_ >> 8) & 0xFFFF) / 32768.0f - 1.0f;
    }
    float process() {
        float w = nextWhite();
        pink1_ = pink1_ * pole1_ + w * (1.0f - pole1_);
        pink2_ = pink2_ * pole2_ + w * (1.0f - pole2_);
        // ~-100 dB nominal: audible as "life", inaudible in measurements.
        // The (1-pole) state balance keeps the pink states at the same
        // variance as the old accumulate form, so the tuned level holds.
        return w * 0.000014f + (pink1_ + pink2_) * 0.000005f;
    }
    void reset() { pink1_ = pink2_ = 0.0f; }
};

// —————————————————————————————————————————————————————————————————————
// Slew limiter: discrete stages can't swing infinitely fast.
// maxDelta is derived from the sample rate (item 10) so the effective
// V/us is constant; the value preserves the tuned 48 kHz behavior
// (0.5 FS/sample at 48 kHz) exactly.
// —————————————————————————————————————————————————————————————————————
// One-pole DC blocker (coupling capacitor). A biquad highpass at 3 Hz
// cannot be represented in float at high sample rates (fc/fs ~ 1e-5
// collapses the DF1/TDF2 state arithmetic and the filter goes unstable
// at 192 kHz hosts). The subtractive one-pole form is stable at any
// rate, rejects DC exactly, and matches the cap's intent: nothing
// audible above the subsonic region.
struct DcBlocker {
    float s_ = 0.0f;
    float a_ = 0.0f;
    void prepare(double fs, float fcHz) {
        a_ = 1.0f - (float)std::exp(-2.0 * 3.14159265358979323846 * fcHz / fs);
    }
    float process(float x) {
        s_ += a_ * (x - s_);
        return x - s_;
    }
    void reset() { s_ = 0.0f; }
};

struct SlewLimiter {
    float last_ = 0.0f;
    float maxDelta_ = 0.5f;
    void prepare(double fs) { maxDelta_ = (float)(0.5 * 48000.0 / fs); }
    float process(float x) {
        float delta = x - last_;
        if (delta > maxDelta_) delta = maxDelta_;
        else if (delta < -maxDelta_) delta = -maxDelta_;
        last_ += delta;
        return last_;
    }
    void reset() { last_ = 0.0f; }
};

// —————————————————————————————————————————————————————————————————————
// Transformer: flux-based. The core saturates based on the integral
// of voltage (flux), not instantaneous voltage — the "glued" low end
// and airy top of real iron. Coefficients are time-based (item 6):
// integration rate 24/s and flux leak tau 41.7 ms reproduce the tuned
// 48 kHz constants (0.0005 / 0.9995) at any sample rate.
// —————————————————————————————————————————————————————————————————————
struct Transformer {
    float flux = 0.0f;
    float leakPole_ = 0.9995f; // set in prepare
    float rate_ = 0.0005f;     // set in prepare
    void prepare(double fs) {
        leakPole_ = (float)std::exp(-1.0 / (0.0417 * fs));
        rate_ = (float)(24.0 / fs);
    }
    float process(float x, float drive = 1.0f) {
        flux += x * rate_ * drive;
        flux *= leakPole_;
        float sat = std::tanh(flux * 8.0f);
        float linear = flux * 8.0f;
        float nonlinear = sat - linear;
        return x + nonlinear * 0.05f * drive;
    }
    void reset() { flux = 0.0f; }
};

// —————————————————————————————————————————————————————————————————————
// Bias memory (item 2): tracks a stage's recent signal level. Loud
// signal charges the coupling network; the stage's operating point
// shifts (curve deepens, gain droops slightly) and recovers with an
// ~80 ms time constant. At rest this returns 0 and the stage curve
// is exactly the shipped static curve.
// —————————————————————————————————————————————————————————————————————
struct BiasMemory {
    float env = 0.0f;
    float process(float x, double fs) {
        const float a = std::fabs(x);
        const float c = (a > env) ? tauCoef(0.005f, fs) : tauCoef(0.080f, fs);
        env += (a - env) * c;
        return env;
    }
    void reset() { env = 0.0f; }
};

// Discrete transistor stage with bias memory: asymmetric, 2nd-order
// dominant. The static coefficients are the shipped tuning (0.02 pos
// / 0.08 neg); bias deepens them under drive and droops gain a hair.
inline float transistorStage(float x, float biasEnv, float kneeTol = 1.0f) {
    const float b = biasEnv > 1.2f ? 1.2f : biasEnv;
    const float deepen = (1.0f + b * 1.2f) * kneeTol;
    float y;
    if (x > 0.0f)
        y = x / (1.0f + 0.02f * deepen * x * x);
    else
        y = x / (1.0f + 0.08f * deepen * x * x);
    return y * (1.0f - 0.02f * b); // bias droop: ~0.2 dB/stage slammed
}

// —————————————————————————————————————————————————————————————————————
// 2x resampler (item 3): windowed-sinc (Blackman) linear-phase FIR,
// 31 taps, generated at prepare. Used for both up and down sampling.
// Group delay: 15 samples at the 2x rate per pass → 15 host samples
// total for the round trip.
// —————————————————————————————————————————————————————————————————————
struct Resampler2x {
    static constexpr int kTaps = 63;
    float coef_[kTaps] = {};
    float buf_[kTaps] = {};
    int pos_ = 0;

    void prepare(double hostFs) {
        // Cutoff at 0.45 * hostFs, expressed at the 2x rate.
        const double fc = 0.45 * hostFs / (hostFs * 2.0); // cycles/sample @2x
        const int M = kTaps - 1;
        for (int i = 0; i < kTaps; ++i) {
            const double n = i - M / 2.0;
            double sinc = (std::fabs(n) < 1e-9) ? 2.0 * fc
                          : std::sin(2.0 * 3.14159265358979323846 * fc * n)
                                / (3.14159265358979323846 * n);
            const double w = 0.42 - 0.5 * std::cos(2.0 * 3.14159265358979323846 * i / M)
                                  + 0.08 * std::cos(4.0 * 3.14159265358979323846 * i / M);
            coef_[i] = (float)(sinc * w);
        }
        reset();
    }
    // Push one sample, return FIR output (caller manages rate change).
    float pushSample(float x) {
        buf_[pos_] = x;
        float y = 0.0f;
        int idx = pos_;
        for (int i = 0; i < kTaps; ++i) {
            y += coef_[i] * buf_[idx];
            idx = (idx == 0) ? kTaps - 1 : idx - 1;
        }
        pos_ = (pos_ + 1) % kTaps;
        return y;
    }
    void reset() { for (auto& v : buf_) v = 0.0f; pos_ = 0; }
};

// Per-channel component tolerances (item 9): 1.0 = schematic value.
struct Tolerances {
    float midFreq = 1.0f;
    float bassFreq = 1.0f;
    float trebleFreq = 1.0f;
    float hpfFreq = 1.0f;
    float stageKnee = 1.0f;
    float xfmrDrive = 1.0f;
};

class Channel {
public:
    // seed: per-channel noise seed (A and B MUST differ — item 7)
    void prepare(double sampleRate, std::uint32_t seed = 12345u);
    void setParams(const ChannelParams& p);
    void setTolerances(const Tolerances& t) { tol_ = t; filtersDirty_ = true; }
    void setWarmth(float w) { warmth_ = w < 0.0f ? 0.0f : (w > 1.0f ? 1.0f : w); }
    // rail: shared power-supply rail from the processor (item 1)
    float processSample(float x, float rail = 1.0f);
    void reset();
    // Round-trip latency in HOST samples: two FIR passes, each
    // (kTaps-1)/2 internal samples; 2 internal = 1 host sample.
    int latencySamples() const { return (Resampler2x::kTaps - 1) / 2; }

    float vuDb() const;
    float getLevel() const { return vu_; } // Linear peak level for clip indicator

private:
    float processInternal(float x, float rail);
    void refreshFilters(); // recomputes biquads from smoothed params

    double fsHost_ = 48000.0;
    double fs_ = 96000.0; // internal (2x) rate
    ChannelParams p_;
    Tolerances tol_;
    float warmth_ = 1.0f; // 1 = fully warmed (processor drives the settle)

    // Smoothed gains (tau-based, item 6)
    float inGainLin_ = 1.0f, inGainLinTarget_ = 1.0f;
    float compLin_ = 1.0f; // A/B staging fix M2: EQ-boost headroom compensation
    float gainLin_ = 1.0f, gainLinTarget_ = 1.0f;

    // Smoothed EQ params (item 5): targets live in p_, these glide
    float bassS_ = 0.0f, midS_ = 0.0f, trebleS_ = 0.0f;
    bool filtersDirty_ = true;
    int refreshCount_ = 0;

    // Inductor saturation state (item 4)
    float lfEnv_ = 0.0f;
    float bumpAppliedDb_ = 999.0f; // last programmed bump gain

    Biquad hpf_, lpf_, bass_, bassBump_, mid_, treble_;
    Biquad miller_;
    DcBlocker coupling_;
    Transformer inXfmr_;
    AnalogNoise noise_;
    SlewLimiter slew_;
    BiasMemory bias1_, bias2_, bias3_;
    Resampler2x up_, down_;

    float vu_ = 0.0f;
    float vuCoefA_ = 0.30f, vuCoefR_ = 0.0005f; // set from host fs
};

// —————————————————————————————————————————————————————————————————————
// MasterStage — OUTPUT knob → stereo out (master fixed at unity by
// the processor; per-channel OUTPUT knobs do the work)
// —————————————————————————————————————————————————————————————————————
class MasterStage {
public:
    void prepare(double sampleRate);
    void setLevel(float level /*0..10, 5=unity*/);
    void process(float inL, float inR, float& outL, float& outR);
    void reset();

private:
    double fs_ = 48000.0;
    float outLin_ = 1.0f;
    float outLinTarget_ = 1.0f;
};

} // namespace pm1000
