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
    return 76.0f - (kDetentSensDbm[d] + 60.0f); // 76,66,60,54,48,42,36,30,24,12
}

inline constexpr float kMidFreqs[3] = { 1000.f, 2000.f, 4000.f }; // per manual + hardware photo

// 0 VU = +4 dBm = -18 dBFS reference for the meters.
inline constexpr float kVuRefDbFs = -18.0f;

// —————————————————————————————————————————————————————————————————————
// ChannelParams — one field per hardware control, per channel
// —————————————————————————————————————————————————————————————————————
struct ChannelParams {
    int   inputDetent = 9;     // 0..9 → -60..+4 dBm (default: +4 line)
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
class Channel {
public:
    void prepare(double sampleRate);
    void setParams(const ChannelParams& p);
    float processSample(float x);
    void reset();

    // Smoothed post-gain level, dB relative to 0 VU.
    float vuDb() const;

private:
    double fs_ = 48000.0;
    ChannelParams p_;
    float inGainLin_ = 1.0f;
    float gainLin_   = 1.0f;
    Biquad hpf_, lpf_, bass_, bassBump_, mid_, treble_;
    float vu_ = 0.0f;
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
};

} // namespace pm1000
