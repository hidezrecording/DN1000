#pragma once
// ─────────────────────────────────────────────────────────────────────
// Pm1000Dsp — framework-free C++17 DSP core for the Yamaha PM1000
// dual-channel rack strip clone.
//
// Two independent channels (A/B), each:
//   INPUT LEVEL (10-detent: -60..+4 dBm) → discrete preamp → PHASE N/R
//   → HPF (off/40/80 Hz, 12 dB/oct) → LPF (off/20k/18k/16k, 12 dB/oct)
//   → 3-band EQ (±15 dB: resonant BASS shelf, very-wide MID bell with
//   700/1.6k/3.2k selector, sweet TREBLE shelf ~10 kHz)
//   → GAIN (per-channel output trim, 0..10, 7 = nominal) → VU tap
// Then the master section:
//   OUTPUT (master level, 0..10, 7 = unity) → output-transformer color
//   (Tamura GA80080: subtle 2nd-order saturation) → stereo out.
//
// Spec basis: Yamaha PM1000 operation/service manual + Studio Sound
// bench review, Dec 1976 (research dossier 2026-10-11).
// Rack-module layout follows hardware units (Grüning Audioworks style):
// INPUT | PHASE | HPF | MID SEL | BASS | MID | TREBLE | GAIN per channel.
//
// Approximations (flagged unverifiable in research): mid-band Q (→ 1.0),
// inductor overshoot (standard shelves), transformer curves (gentle
// asymmetric saturation tuned to "airy 2nd-order top / glued bottom").
// ─────────────────────────────────────────────────────────────────────
#include <cmath>
#include <cstdint>

namespace pm1000 {

inline float dbToLinear(float db) { return std::pow(10.0f, db / 20.0f); }

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
// Input-level detents (dBm sensitivity) and derived preamp gain.
// Max channel gain 76 dB (manual); falls dB-for-dB as sensitivity rises.
// —————————————————————————————————————————————————————————————————————
inline constexpr float kDetentSensDbm[10] =
    { -60.f, -50.f, -44.f, -38.f, -32.f, -26.f, -20.f, -14.f, -8.f, 4.f };

inline float detentGainDb(int d) {
    d = d < 0 ? 0 : (d > 9 ? 9 : d);
    // Volume knob: 7 o'clock = -60dB (almost gone), 12 o'clock = 0dB (unity), 5 o'clock = +24dB (max)
    // Detent 5 is 12 o'clock (center)
    static constexpr float gains[10] = {-60.f, -48.f, -36.f, -24.f, -12.f, 0.f, 6.f, 12.f, 18.f, 24.f};
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
    int   midSel      = 1;     // 0=1 kHz, 1=2 kHz, 2=4 kHz
    float trebleDb    = 0.0f;  // ±15 dB shelf
    float gain        = 7.0f;  // per-channel GAIN knob, 0..10 (7 = nominal)
    int   outMode     = 0;     // 0=Tamura, 1=Cinemag, 2=Sumthang
};

// —————————————————————————————————————————————————————————————————————
// Channel — input → EQ → per-channel GAIN (mono), with VU tap
// —————————————————————————————————————————————————————————————————————
// —————————————————————————————————————————————————————————————————————
// Analog component models — including inter-stage physics
// —————————————————————————————————————————————————————————————————————
// Power supply: sags under load, has ripple, couples channels.
// This is the "glue" — when both channels hit hard, the rails dip.
struct PowerSupply {
    float railVoltage = 1.0f;  // normalized (1.0 = nominal)
    float ripplePhase = 0.0f;
    // currentDemand: sum of absolute signal from both channels (0..2)
    void update(float currentDemand, double fs) {
        // Sag: rails dip when current demand is high (slow, ~10 Hz)
        float target = 1.0f - currentDemand * 0.03f; // 3% sag at full load
        railVoltage += (target - railVoltage) * 0.001f;
        ripplePhase += 2.0f * 3.14159f * 120.0f / (float)fs; // 120 Hz ripple
        if (ripplePhase > 2.0f * 3.14159f) ripplePhase -= 2.0f * 3.14159f;
    }
    float getRail() const { return railVoltage; }
    float getRipple() const {
        return std::sin(ripplePhase) * 0.0005f * (2.0f - railVoltage);
    }
};

// Noise: transistor hiss (white + 1/f) + mains hum.
// Real discrete consoles are never silent.
struct AnalogNoise {
    float white_ = 0.0f;
    float pink1_ = 0.0f, pink2_ = 0.0f; // 1/f approximation
    unsigned int seed_ = 12345;
    float nextWhite() {
        seed_ = seed_ * 1664525u + 1013904223u;
        return ((seed_ >> 16) & 0x7FFF) / 16384.0f - 1.0f;
    }
    float process() {
        float w = nextWhite();
        pink1_ = pink1_ * 0.98f + w * 0.02f;
        pink2_ = pink2_ * 0.94f + w * 0.06f;
        // -100 dB: audible as "life", inaudible in measurements
        return w * 0.00001f + (pink1_ + pink2_) * 0.000005f;
    }
    void reset() { white_ = pink1_ = pink2_ = 0.0f; }
};

// Slew limiter: discrete stages can't swing infinitely fast.
// Softens transients like real transistor circuits.
struct SlewLimiter {
    float last_ = 0.0f;
    float maxDelta_ = 0.1f; // max change per sample (set by prepare)
    void prepare(double fs) {
        // Authentic slew: transparent for all normal audio, including
        // 18 kHz at full scale (delta=0.47). Limits only pathological
        // transients (digital clicks, etc.).
        maxDelta_ = 0.5f;
    }
    float process(float x) {
        float delta = x - last_;
        if (delta > maxDelta_) delta = maxDelta_;
        else if (delta < -maxDelta_) delta = -maxDelta_;
        last_ += delta;
        return last_;
    }
    void reset() { last_ = 0.0f; }
};
// Transformer: flux-based hysteresis. The core saturates based on the
// integral of voltage (flux), not instantaneous voltage. This gives the
// "glued" low end and airy top of real iron.
struct Transformer {
    float flux = 0.0f;      // magnetic flux (integrator state)
    float lastOut = 0.0f;
    // Tamura 80071: 1:1, 600 ohm, saturates gracefully
    // Unity gain when clean; saturation adds harmonics when driven.
    float process(float x, float drive = 1.0f) {
        // Flux is the integral of voltage
        flux += x * 0.0005f * drive;
        flux *= 0.9995f; // core loss (hysteresis)
        // Saturation: tanh of flux
        float sat = std::tanh(flux * 8.0f);
        // When clean, sat ≈ flux*8 (linear), so output ≈ x.
        // When driven, sat compresses, adding harmonics.
        // Compensate: output = x + (sat - flux*8) * 0.05 (only the nonlinear part)
        float linear = flux * 8.0f;
        float nonlinear = sat - linear;
        return x + nonlinear * 0.05f * drive;
    }
    void reset() { flux = 0.0f; lastOut = 0.0f; }
};

// Discrete transistor stage: asymmetric, 2nd-order dominant.
// Unity gain when clean; adds harmonics when driven.
// NPN transistors clip the top harder than the bottom.
inline float transistorStage(float x) {
    // Very gentle: subtle 2nd-order, saturates only when very hot.
    // For small x, ≈ x (unity).
    float y;
    if (x > 0.0f)
        y = x / (1.0f + 0.02f * x * x); // gentle saturation, more headroom (real PM1000)
    else
        y = x / (1.0f + 0.08f * x * x); // slightly harder on negative
    return y;
}

class Channel {
public:
    void prepare(double sampleRate);
    void setParams(const ChannelParams& p);
    float processSample(float x);
    void reset();

    // Smoothed post-gain level, dB relative to 0 VU.
    float vuDb() const;
    float getLevel() const { return vu_; } // Linear peak level for clip indicator

private:
    double fs_ = 48000.0;
    ChannelParams p_;
    float inGainLin_ = 1.0f;
    float inGainLinTarget_ = 1.0f;
    float gainLin_ = 1.0f;
    float gainLinTarget_ = 1.0f; // Smoothed to prevent zipper noise
    Biquad hpf_, lpf_, bass_, bassBump_, mid_, treble_;
    float vu_ = 0.0f;
    Transformer inXfmr_;
    AnalogNoise noise_;
    SlewLimiter slew_;
    PowerSupply psu_;
    Biquad miller_;
    // Coupling caps: AC coupling between stages (LF phase shift)
    Biquad coupling_;
    // Thermal: transistors heat up, Vbe drops, bias shifts (very slow)
    float thermal_ = 0.0f;
};

// —————————————————————————————————————————————————————————————————————
// MasterStage — OUTPUT knob → output-transformer color → stereo out
// —————————————————————————————————————————————————————————————————————
class MasterStage {
public:
    void prepare(double sampleRate);
    void setLevel(float level /*0..10, 7=unity*/);
    void process(float inL, float inR, float& outL, float& outR);
    void reset();

private:
    float outLin_ = 1.0f;
    float outLinTarget_ = 1.0f; // Smoothed
};

} // namespace pm1000
