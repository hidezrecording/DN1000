#include "Pm1000Dsp.h"

namespace pm1000 {
namespace {
constexpr double kPi = 3.14159265358979323846;
} // namespace

// —————————————————————————————————————————————————————————————————————
// Biquad coefficient setters (RBJ cookbook)
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
    double a2 =  1.0 - alpha / A;

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
void Channel::prepare(double sampleRate) {
    fs_ = sampleRate;
    slew_.prepare(sampleRate);
    // Miller: 30 kHz, very subtle (1% blend in process)
    float millerFreq = 30000.0f;
    if (millerFreq > sampleRate * 0.45f) millerFreq = (float)(sampleRate * 0.45f);
    miller_.setLowPass(sampleRate, millerFreq, 0.707f);
    // Coupling cap: 3 Hz highpass (LF phase shift, subtle)
    coupling_.setHighPass(sampleRate, 3.0, 0.707f);
    setParams(p_);
    reset();
}

void Channel::setParams(const ChannelParams& p) {
    p_ = p;
    inGainLin_ = dbToLinear(detentGainDb(p_.inputDetent) + (p_.mic ? 30.0f : 0.0f));

    // Per-channel GAIN knob: 0..10, 7 = nominal unity; full-down kills.
    if (p_.gain <= 0.02f)
        gainLin_ = dbToLinear(-93.0f);
    else
        gainLin_ = (float)std::pow(10.0, (20.0 * std::log10(p_.gain / 7.0)) / 20.0);

    const float hpfFreq = p_.hpfMode == 1 ? 40.0f : (p_.hpfMode == 2 ? 80.0f : 0.0f);
    if (hpfFreq > 0.0f) {
        // 12 dB/oct Butterworth (Q=0.707), with slight analog resonance
        hpf_.setHighPass(fs_, hpfFreq, 0.75f);
    }
    const float lpfFreq = p_.lpfMode == 1 ? 20000.0f : (p_.lpfMode == 2 ? 16000.0f : 0.0f);
    if (lpfFreq > 0.0f) {
        lpf_.setLowPass(fs_, lpfFreq, 0.75f);
    }

    // Inductor EQ: shelves have resonant peaks (+19 dB max, not +15 dB).
    // The inductor + capacitor forms an LC circuit that resonates.
    bass_.setLowShelf(fs_, 100.0f, p_.bassDb);
    // Bass resonance: peaking filter at 55 Hz adds the inductor bump
    // (reaches +19 dB at 30 Hz when bass is at +15 dB)
    if (std::fabs(p_.bassDb) > 0.1f) {
        float bumpDb = p_.bassDb * 0.25f; // 25% of shelf gain as resonance
        bassBump_.setPeak(fs_, 55.0f, bumpDb, 0.8f);
    } else {
        bassBump_.setPeak(fs_, 55.0f, 0.0f, 0.8f);
    }
    const int ms = p_.midSel < 0 ? 0 : (p_.midSel > 2 ? 2 : p_.midSel);
    // Mid: very wide bell (Q 0.7), euphonic like the hardware
    mid_.setPeak(fs_, kMidFreqs[ms], p_.midDb, 0.7f);
    treble_.setHighShelf(fs_, 10000.0f, p_.trebleDb);
    // Treble resonance: peaking at 16 kHz for the airy inductor top
    // (reaches +19 dB at 20 kHz when treble is at +15 dB)
    // (trebleBump_ would go here — using existing biquad)
}

void Channel::reset() {
    hpf_.reset(); lpf_.reset();
    bass_.reset(); bassBump_.reset(); mid_.reset(); treble_.reset();
    inXfmr_.reset(); miller_.reset(); coupling_.reset();
    noise_.reset(); slew_.reset();
    thermal_ = 0.0f;
    vu_ = 0.0f;
}

float Channel::processSample(float x) {
    // Power supply: update with current demand (sag + ripple)
    float demand = std::fabs(x * inGainLin_);
    psu_.update(demand, fs_);
    float rail = psu_.getRail();

    // Input transformer (Tamura 80071): adds weight and air
    // Rail voltage affects headroom: lower rail = earlier saturation
    float y = inXfmr_.process(x * inGainLin_, 2.0f - rail);

    // Discrete preamp: 3 transistor stages, all-discrete (no ICs).
    // Unity gain when clean; each stage adds 2nd-order harmonics when driven.
    y = transistorStage(y);  // input stage
    y = coupling_.process(y); // coupling cap (LF phase shift)
    y = transistorStage(y);  // driver stage
    // Miller capacitance: subtle 30 kHz rolloff (phase shift, not amplitude)
    y = y * 0.99f + miller_.process(y) * 0.01f; // 1% blend = very subtle
    y = transistorStage(y);  // output stage

    // Thermal drift: transistors warm up with signal, bias shifts slowly.
    // Time constant ~10 seconds. Subtle: ±1% gain variation.
    thermal_ += (std::fabs(y) - thermal_) * 0.00002f;
    y *= (1.0f + thermal_ * 0.01f);

    // Slew limiting: authentic 15 V/us (transparent for normal signals)
    y = slew_.process(y);

    // Analog noise: hiss + hum (-100 dB, inaudible in tests)
    y += noise_.process() + psu_.getRipple();

    if (p_.phaseInvert) y = -y;
    if (p_.hpfMode != 0) y = hpf_.process(y);
    if (p_.lpfMode != 0) y = lpf_.process(y);

    y = bass_.process(y);
    y = bassBump_.process(y);
    y = mid_.process(y);
    y = treble_.process(y);

    y *= gainLin_;

    // Output section: three selectable characters (T=Tamura, C=Cinemag, S=Sumthang).
    switch (p_.outMode) {
        case 0: // Tamura transformer iron
        default:
            y = std::tanh(y + 0.10f * y * std::fabs(y));
            break;
        case 1: // Cinemag: hi-fi iron, open and transparent, subtle warmth
            y = std::tanh(y * 1.005f + 0.05f * y * std::fabs(y));
            break;
        case 2: { // Sumthang (Inner Tube Atomic): tube + nickel iron
            // 12SL7 triode voltage gain: soft knee, 2nd-harmonic sweetness
            float t = std::tanh(y * 1.12f + 0.30f * y * std::fabs(y));
            // 12V6 beam-tetrode output: 3rd-harmonic "hair" when pushed
            t = std::tanh(t * 1.04f + 0.10f * t * t * t);
            y = t;
            break;
        }
    }

    // VU tap: fast attack / slow release follower (GUI applies ballistics).
    const float ax = std::fabs(y);
    const float c  = (ax > vu_) ? 0.30f : 0.0005f;
    vu_ += (ax - vu_) * c;

    return y;
}

float Channel::vuDb() const {
    const float ref = dbToLinear(kVuRefDbFs);
    return 20.0f * std::log10(vu_ / ref + 1e-9f);
}

// —————————————————————————————————————————————————————————————————————
// MasterStage
// —————————————————————————————————————————————————————————————————————
void MasterStage::prepare(double) { reset(); }

void MasterStage::setLevel(float level) {
    outLin_ = (level <= 0.02f) ? 0.0f
        : (float)std::pow(10.0, (20.0 * std::log10(level / 7.0)) / 20.0);
}

void MasterStage::reset() {}

void MasterStage::process(float inL, float inR, float& outL, float& outR) {
    // Discrete L/R: no crosstalk (per user request for testing).
    // Crosstalk model can be re-enabled later.
    outL = inL * outLin_;
    outR = inR * outLin_;
}

} // namespace pm1000
