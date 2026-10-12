#include "Pm1000Dsp.h"

namespace pm1000 {
namespace {
constexpr double kPi = 3.14159265358979323846;
} // namespace

// —————————————————————————————————————————————————————————————————————
// Biquad coefficient setters (RBJ cookbook) — unchanged tuning
// —————————————————————————————————————————————————————————————————————
void Biquad::setLowShelf(double fs, double f0, float gainDb) {
    double A     = std::pow(10.0, gainDb / 40.0);
    double w0    = 2.0 * kPi * f0 / fs;
    double cosw0 = std::cos(w0), sinw0 = std::sin(w0);
    double alpha = sinw0 / 2.0 * std::sqrt(2.0);
    double sqA   = std::sqrt(A);

    double b0 =    A * ((A + 1.0) - (A - 1.0) * cosw0 + 2.0 * sqA * alpha);
    double b1 =  2.0 * A * ((A - 1.0) - (A + 1.0) * cosw0);
    double b2 =    A * ((A + 1.0) - (A - 1.0) * cosw0 - 2.0 * sqA * alpha);
    double a0 =        (A + 1.0) + (A - 1.0) * cosw0 + 2.0 * sqA * alpha;
    double a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosw0);
    double a2 =        (A + 1.0) + (A - 1.0) * cosw0 - 2.0 * sqA * alpha;

    b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
    this->b0 = (float)b0; this->b1 = (float)b1; this->b2 = (float)b2;
    this->a1 = (float)a1; this->a2 = (float)a2;
}

void Biquad::setHighShelf(double fs, double f0, float gainDb) {
    double A     = std::pow(10.0, gainDb / 40.0);
    double w0    = 2.0 * kPi * f0 / fs;
    double cosw0 = std::cos(w0), sinw0 = std::sin(w0);
    double alpha = sinw0 / 2.0 * std::sqrt(2.0);
    double sqA   = std::sqrt(A);

    double b0 =    A * ((A + 1.0) + (A - 1.0) * cosw0 + 2.0 * sqA * alpha);
    double b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosw0);
    double b2 =    A * ((A + 1.0) + (A - 1.0) * cosw0 - 2.0 * sqA * alpha);
    double a0 =        (A + 1.0) - (A - 1.0) * cosw0 + 2.0 * sqA * alpha;
    double a1 =  2.0 * ((A - 1.0) - (A + 1.0) * cosw0);
    double a2 =        (A + 1.0) - (A - 1.0) * cosw0 - 2.0 * sqA * alpha;

    b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
    this->b0 = (float)b0; this->b1 = (float)b1; this->b2 = (float)b2;
    this->a1 = (float)a1; this->a2 = (float)a2;
}

void Biquad::setPeak(double fs, double f0, float gainDb, float q) {
    double A     = std::pow(10.0, gainDb / 40.0);
    double w0    = 2.0 * kPi * f0 / fs;
    double cosw0 = std::cos(w0), sinw0 = std::sin(w0);
    double alpha = sinw0 / (2.0 * q);

    double b0 =  1.0 + alpha * A;
    double b1 = -2.0 * cosw0;
    double b2 =  1.0 - alpha * A;
    double a0 =  1.0 + alpha / A;
    double a1 = -2.0 * cosw0;
    double a2 =   1.0 - alpha / A;

    b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
    this->b0 = (float)b0; this->b1 = (float)b1; this->b2 = (float)b2;
    this->a1 = (float)a1; this->a2 = (float)a2;
}

void Biquad::setHighPass(double fs, double f0, float q) {
    double w0    = 2.0 * kPi * f0 / fs;
    double cosw0 = std::cos(w0), sinw0 = std::sin(w0);
    double alpha = sinw0 / (2.0 * q);

    double b0 =  (1.0 + cosw0) / 2.0;
    double b1 = -(1.0 + cosw0);
    double b2 =  (1.0 + cosw0) / 2.0;
    double a0 =   1.0 + alpha;
    double a1 =  -2.0 * cosw0;
    double a2 =   1.0 - alpha;

    b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
    this->b0 = (float)b0; this->b1 = (float)b1; this->b2 = (float)b2;
    this->a1 = (float)a1; this->a2 = (float)a2;
}

void Biquad::setLowPass(double fs, double f0, float q) {
    double w0    = 2.0 * kPi * f0 / fs;
    double cosw0 = std::cos(w0), sinw0 = std::sin(w0);
    double alpha = sinw0 / (2.0 * q);

    double b0 =  (1.0 - cosw0) / 2.0;
    double b1 =   1.0 - cosw0;
    double b2 =  (1.0 - cosw0) / 2.0;
    double a0 =   1.0 + alpha;
    double a1 =  -2.0 * cosw0;
    double a2 =   1.0 - alpha;

    b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
    this->b0 = (float)b0; this->b1 = (float)b1; this->b2 = (float)b2;
    this->a1 = (float)a1; this->a2 = (float)a2;
}

// —————————————————————————————————————————————————————————————————————
// Channel
// —————————————————————————————————————————————————————————————————————
void Channel::prepare(double sampleRate, std::uint32_t seed) {
    fsHost_ = sampleRate;
    fs_ = sampleRate * 2.0; // internal oversampled rate (item 3)

    noise_.prepare(fs_, seed);
    slew_.prepare(fs_);
    inXfmr_.prepare(fs_);
    up_.prepare(sampleRate);
    down_.prepare(sampleRate);

    // Miller: 30 kHz, very subtle (1% blend in process)
    float millerFreq = 30000.0f;
    if (millerFreq > fs_ * 0.45f) millerFreq = (float)(fs_ * 0.45f);
    miller_.setLowPass(fs_, millerFreq, 0.707f);
    // Coupling cap: 3 Hz highpass (LF phase shift, subtle)
    coupling_.prepare(fs_, 3.0f); // 3 Hz coupling cap (one-pole blocker)

    // VU ballistics from the HOST rate, matching the tuned 48 kHz
    // coefficients (attack 0.30, release 0.0005) as time constants:
    // attack tau = 58 us, release tau = 41.7 ms.
    vuCoefA_ = tauCoef(0.0000584f, fsHost_);
    vuCoefR_ = tauCoef(0.0417f, fsHost_);

    setParams(p_);
    // Start with smoothed EQ exactly at the targets (no glide on load)
    bassS_ = p_.bassDb; midS_ = p_.midDb; trebleS_ = p_.trebleDb;
    filtersDirty_ = true;
    refreshFilters();
    reset();
}

void Channel::setParams(const ChannelParams& p) {
    p_ = p;
    inGainLinTarget_ = dbToLinear(detentGainDb(p_.inputDetent) + (p_.mic ? 30.0f : 0.0f));

    // Per-channel OUTPUT knob: value/5, 5.0 = unity (12 o'clock)
    if (p_.gain <= 0.02f)
        gainLinTarget_ = dbToLinear(-93.0f);
    else
        gainLinTarget_ = p_.gain / 5.0f;

    filtersDirty_ = true; // EQ/switch changes land at the next refresh
}

void Channel::refreshFilters() {
    // HPF / LPF (12 dB/oct, slight analog resonance Q=0.75 as tuned)
    if (p_.hpfMode != 0) {
        const float f = (p_.hpfMode == 1 ? 40.0f : 80.0f) * tol_.hpfFreq;
        hpf_.setHighPass(fs_, f, 0.75f);
    }
    if (p_.lpfMode != 0) {
        const float f = (p_.lpfMode == 1 ? 20000.0f : 16000.0f);
        lpf_.setLowPass(fs_, f, 0.75f);
    }

    // Inductor EQ from the SMOOTHED knob values (item 5)
    bass_.setLowShelf(fs_, 100.0f * tol_.bassFreq, bassS_);
    // Bass inductor bump: 25% of shelf gain as a 55 Hz resonance,
    // reduced by LF saturation (item 4) — louder bass, softer bump.
    const float sat = lfEnv_ > 1.0f ? 1.0f : lfEnv_;
    const float bumpDb = bassS_ * 0.25f * (1.0f - 0.45f * sat);
    bassBump_.setPeak(fs_, 55.0f * tol_.bassFreq, bumpDb, 0.8f);
    bumpAppliedDb_ = bumpDb;

    const int ms = p_.midSel < 0 ? 0 : (p_.midSel > 2 ? 2 : p_.midSel);
    mid_.setPeak(fs_, kMidFreqs[ms] * tol_.midFreq, midS_, 0.7f);
    treble_.setHighShelf(fs_, 10000.0f * tol_.trebleFreq, trebleS_);
}

void Channel::reset() {
    hpf_.reset(); lpf_.reset();
    bass_.reset(); bassBump_.reset(); mid_.reset(); treble_.reset();
    miller_.reset(); coupling_.reset();
    inXfmr_.reset();
    noise_.reset(); slew_.reset();
    bias1_.reset(); bias2_.reset(); bias3_.reset();
    up_.reset(); down_.reset();
    lfEnv_ = 0.0f;
    vu_ = 0.0f;
    refreshCount_ = 0;
}

float Channel::processSample(float x, float rail) {
    // 2x oversampling (item 3): zero-stuffed interpolation (FIR gain 2),
    // two internal samples, then decimation (take the second output).
    const float u0 = up_.pushSample(x) * 2.0f;
    const float u1 = up_.pushSample(0.0f) * 2.0f;
    const float i0 = processInternal(u0, rail);
    const float i1 = processInternal(u1, rail);
    down_.pushSample(i0);
    const float y = down_.pushSample(i1);

    // VU tap on the host-rate output: fast attack / slow release
    const float ax = std::fabs(y);
    const float c = (ax > vu_) ? vuCoefA_ : vuCoefR_;
    vu_ += (ax - vu_) * c;
    return y;
}

float Channel::processInternal(float x, float rail) {
    // Hiss at the first-stage input (item 7): it is amplified by the
    // same gain staging as the signal, like real preamp noise.
    x += noise_.process();

    // Input gain smoothing (tau 4.2 ms = the tuned 48 kHz 0.005 feel)
    const float gCoef = tauCoef(0.0042f, fs_);
    inGainLin_ += (inGainLinTarget_ - inGainLin_) * gCoef;
    if (std::fabs(inGainLinTarget_ - inGainLin_) < 1e-6f) inGainLin_ = inGainLinTarget_;

    // Input transformer (Tamura 80071 class): rail sag deepens drive
    float y = inXfmr_.process(x * inGainLin_, (2.0f - rail) * tol_.xfmrDrive);

    // Discrete stages with bias memory (item 2)
    y = transistorStage(y, bias1_.process(y, fs_), tol_.stageKnee);
    y = coupling_.process(y);
    y = transistorStage(y, bias2_.process(y, fs_), tol_.stageKnee);
    // Miller capacitance: subtle 30 kHz rolloff (1% blend as tuned)
    y = y * 0.99f + miller_.process(y) * 0.01f;

    if (p_.phaseInvert) y = -y;
    if (p_.hpfMode != 0) y = hpf_.process(y);
    if (p_.lpfMode != 0) y = lpf_.process(y);

    // LF envelope feeding the inductor model (item 4)
    lfEnv_ += (std::fabs(y) - lfEnv_) * tauCoef(0.025f, fs_);

    y = bass_.process(y);
    y = bassBump_.process(y);
    y = mid_.process(y);
    y = treble_.process(y);

    // Post-EQ stage (item 4): EQ boosts push this stage, like hardware
    y = transistorStage(y, bias3_.process(y, fs_), tol_.stageKnee);
    y = slew_.process(y);

    // Warm-up trim (item 8): a cold unit sits ~0.26 dB down and settles
    y *= (0.97f + 0.03f * warmth_);

    // Channel gain smoothing
    gainLin_ += (gainLinTarget_ - gainLin_) * gCoef;
    if (std::fabs(gainLinTarget_ - gainLin_) < 1e-6f) gainLin_ = gainLinTarget_;
    y *= gainLin_;

    // Output section: three selectable characters; the shared rail
    // (item 1) deepens the drive as it sags.
    const float drive = 2.0f - rail;
    switch (p_.outMode) {
        case 0: // Tamura transformer iron
        default:
            y = std::tanh(y * drive + 0.10f * y * std::fabs(y));
            break;
        case 1: // Cinemag: hi-fi iron, open and transparent, subtle warmth
            y = std::tanh(y * drive * 1.005f + 0.05f * y * std::fabs(y));
            break;
        case 2: { // Sumthang (Inner Tube Atomic): tube + nickel iron
            // 12SL7 triode voltage gain: soft knee, 2nd-harmonic sweetness
            float t = std::tanh(y * drive * 1.12f + 0.30f * y * std::fabs(y));
            // 12V6 beam-tetrode output: 3rd-harmonic "hair" when pushed
            t = std::tanh(t * 1.04f + 0.10f * t * t * t);
            y = t;
            break;
        }
    }

    // Decimated housekeeping (items 4+5): glide the EQ params toward
    // their targets and refresh coefficients only when something moved.
    if (++refreshCount_ >= 32) {
        refreshCount_ = 0;
        const float step = 1.0f - (float)std::exp(-32.0 / (0.015 * fs_));
        bool changed = filtersDirty_;
        auto glide = [&](float& cur, float tgt) {
            const float d = tgt - cur;
            if (std::fabs(d) > 0.002f) { cur += d * step; changed = true; }
            else if (cur != tgt) { cur = tgt; changed = true; }
        };
        glide(bassS_, p_.bassDb);
        glide(midS_, p_.midDb);
        glide(trebleS_, p_.trebleDb);
        // Inductor bump may need a refresh from LF level alone
        const float sat = lfEnv_ > 1.0f ? 1.0f : lfEnv_;
        const float bumpDb = bassS_ * 0.25f * (1.0f - 0.45f * sat);
        if (std::fabs(bumpDb - bumpAppliedDb_) > 0.05f) changed = true;
        if (changed) { refreshFilters(); filtersDirty_ = false; }
    }

    return y;
}

float Channel::vuDb() const {
    const float ref = dbToLinear(kVuRefDbFs);
    return 20.0f * std::log10(vu_ / ref + 1e-9f);
}

// —————————————————————————————————————————————————————————————————————
// MasterStage
// —————————————————————————————————————————————————————————————————————
void MasterStage::prepare(double sampleRate) {
    fs_ = sampleRate;
    reset();
}

void MasterStage::setLevel(float level) {
    outLinTarget_ = (level <= 0.02f) ? 0.0f : level / 5.0f; // 5.0 = unity
    if (level <= 0.02f) outLin_ = 0.0f; // full-down is a hard mute
}

void MasterStage::reset() {}

void MasterStage::process(float inL, float inR, float& outL, float& outR) {
    // Discrete L/R: no crosstalk. Smoothing matches the tuned feel.
    outLin_ += (outLinTarget_ - outLin_) * tauCoef(0.0042f, fs_);
    if (std::fabs(outLinTarget_ - outLin_) < 1e-6f) outLin_ = outLinTarget_;
    outL = inL * outLin_;
    outR = inR * outLin_;
}

} // namespace pm1000
