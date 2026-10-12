#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

DN1000Processor::DN1000Processor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Params", createParameterLayout())
{
}

DN1000Processor::~DN1000Processor() = default;

bool DN1000Processor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    // Support mono and stereo (mono uses Channel A only)
    if (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::mono())
        return true;
    if (in == juce::AudioChannelSet::stereo() && out == juce::AudioChannelSet::stereo())
        return true;
    return false;
}

juce::AudioProcessorValueTreeState::ParameterLayout
DN1000Processor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // EQ knobs are reverse-wired like the hardware: full CCW = +15 dB.
    auto dB = [](juce::String name, juce::String id, float min, float max, float def) {
        return std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float>(min, max, 0.1f), def,
            juce::String(), juce::AudioProcessorParameter::genericParameter,
            [](float v, int) { return juce::String(-v, 1) + " dB"; },
            [](const juce::String& t) { return -t.getFloatValue(); });
    };

    auto toggle = [](juce::String name, juce::String id, bool def) {
        return std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { id, 1 }, name, def);
    };

    auto choice = [](juce::String name, juce::String id,
                     juce::StringArray choices, int def) {
        return std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { id, 1 }, name, choices, def);
    };

    auto gain010 = [](juce::String name, juce::String id) {
        return std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f), 5.0f); // 5.0 = unity (12 o'clock)
    };

    for (auto prefix : { juce::String("a"), juce::String("b") })
    {
        const juce::String chName = prefix == "a" ? "Ch A " : "Ch B ";
        params.push_back(choice(chName + "Input", prefix + "InputLevel",
            { "-60 dBm", "-50 dBm", "-44 dBm", "-38 dBm", "-32 dBm",
              "-26 dBm", "-20 dBm", "-14 dBm", "-8 dBm", "+4 dBm" }, 5)); // 5 = unity (12 o'clock)
        params.push_back(toggle(chName + "Polarity", prefix + "Phase", false));
        params.push_back(toggle(chName + "Mic", prefix + "Mic", false));
        {
            juce::StringArray choices { "Sumthang", "Cinemag", "Tamura" };
            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID { prefix + "OutMode", 1 },
                chName + "Output", choices, 2));
        }
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { prefix + "Hpf", 1 }, chName + "Hi-Pass",
            juce::NormalisableRange<float>(0.0f, 2.0f, 1.0f), 0.0f)); // OFF
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { prefix + "Lpf", 1 }, chName + "Hi-Cut",
            juce::NormalisableRange<float>(0.0f, 2.0f, 1.0f), 0.0f)); // OFF
        params.push_back(dB(chName + "Bass", prefix + "Bass", -15.0f, 15.0f, 0.0f));
        params.push_back(dB(chName + "Mid-Range", prefix + "Mid", -15.0f, 15.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { prefix + "MidSel", 1 }, chName + "Mid-Range Freq",
            juce::NormalisableRange<float>(0.0f, 2.0f, 1.0f), 0.0f)); // 0 = 1K default
        params.push_back(dB(chName + "Treble", prefix + "Treble", -15.0f, 15.0f, 0.0f));
        params.push_back(gain010(chName + "Output", prefix + "Gain"));
    }
    params.push_back(toggle("Power", "power", true));
        params.push_back(toggle("Stereo Link", "link", false));

    return { params.begin(), params.end() };
}

void DN1000Processor::prepareToPlay(double sampleRate, int)
{
    chA_.prepare(sampleRate, 12345u);
    chB_.prepare(sampleRate, 987654321u); // different hiss seed (item 7)
    // Ch B component tolerances (item 9): real module pairs never match
    chB_.setTolerances({ 1.015f, 0.99f, 1.01f, 1.02f, 1.04f, 0.97f });
    master_.prepare(sampleRate);
    master_.setLevel(5.0f); // fixed unity (5.0 = 0dB) — per-channel OUTPUT knobs do the work
    psu_.reset();
    powerGain_ = 0.0f;
    warmth_ = 0.0f;
    wasPowerOn_ = false;
    latency_ = chA_.latencySamples(); // oversampling round trip
    setLatencySamples(latency_);
    std::fill(std::begin(dryL_), std::end(dryL_), 0.0f);
    std::fill(std::begin(dryR_), std::end(dryR_), 0.0f);
    dryPos_ = 0;
    syncParamsToDsp();
}

void DN1000Processor::releaseResources() {}

pm1000::ChannelParams DN1000Processor::channelParamsFor(const juce::String& prefix)
{
    pm1000::ChannelParams p;
    p.inputDetent = (int)std::round(apvts.getRawParameterValue(prefix + "InputLevel")->load());
    p.phaseInvert = apvts.getRawParameterValue(prefix + "Phase")->load() > 0.5f;
    p.mic         = apvts.getRawParameterValue(prefix + "Mic")->load() > 0.5f;
    {
        auto* cp = dynamic_cast<juce::AudioParameterChoice*>(
            apvts.getParameter(prefix + "OutMode"));
        p.outMode = cp ? (2 - cp->getIndex()) : 0;
    }
    p.hpfMode     = (int)std::round(apvts.getRawParameterValue(prefix + "Hpf")->load());
    p.lpfMode     = (int)std::round(apvts.getRawParameterValue(prefix + "Lpf")->load());
    p.bassDb      = apvts.getRawParameterValue(prefix + "Bass")->load();
    p.midDb       = apvts.getRawParameterValue(prefix + "Mid")->load();
    p.midSel      = (int)std::round(apvts.getRawParameterValue(prefix + "MidSel")->load());
    p.trebleDb    = apvts.getRawParameterValue(prefix + "Treble")->load();
    p.gain        = apvts.getRawParameterValue(prefix + "Gain")->load();
    return p;
}

void DN1000Processor::syncParamsToDsp()
{
    chA_.setParams(channelParamsFor("a"));
    chB_.setParams(channelParamsFor("b"));
}

void DN1000Processor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const bool powerOn = apvts.getRawParameterValue("power")->load() > 0.5f;
    syncParamsToDsp();

    const int n = buffer.getNumSamples();
    const double fs = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    const float* inL = buffer.getReadPointer(0);
    const float* inR = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1)
                                                   : buffer.getReadPointer(0);
    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    const bool isMono = (outR == nullptr);

    // Warm-up (item 8): after a power-on edge the unit settles over
    // ~2.5 s; while off it cools slowly toward a lukewarm state.
    if (powerOn) {
        if (!wasPowerOn_) { /* keep current warmth: gear cools, not resets */ }
        warmth_ += (1.0f - warmth_) * (float)(1.0 - std::exp(-(n / fs) / 2.5));
        chA_.setWarmth(warmth_);
        chB_.setWarmth(warmth_);
    } else {
        warmth_ += (0.25f - warmth_) * (float)(1.0 - std::exp(-(n / fs) / 60.0));
    }
    wasPowerOn_ = powerOn;

    const float pgTarget = powerOn ? 1.0f : 0.0f;
    const float pgCoef = pm1000::tauCoef(0.06f, fs); // ~300 ms power fade

    for (int i = 0; i < n; ++i)
    {
        // Power crossfade (item 8): ON = full analog path, OFF = clean
        // bypass. The DSP keeps running either way (no click, and the
        // unit stays warm while bypassed).
        powerGain_ += (pgTarget - powerGain_) * pgCoef;
        if (std::fabs(pgTarget - powerGain_) < 1e-4f) powerGain_ = pgTarget;

        // Dry input, delayed by exactly the processed latency
        dryL_[dryPos_] = inL[i];
        dryR_[dryPos_] = isMono ? inL[i] : inR[i];
        const int rp = (dryPos_ - latency_ + kDryCap) % kDryCap;
        const float dL = dryL_[rp], dR = dryR_[rp];
        dryPos_ = (dryPos_ + 1) % kDryCap;

        // Shared rail (item 1): both channels draw from one supply
        const float rail = psu_.getRail();
        float a, b;
        if (isMono) {
            a = chA_.processSample(inL[i], rail);
            b = a;
        } else {
            a = chA_.processSample(inL[i], rail);
            b = chB_.processSample(inR[i], rail);
        }
        // Rail demand from this sample's outputs drives the next sample
        psu_.update(std::fabs(a) + (isMono ? 0.0f : std::fabs(b)), fs);

        float L, R;
        master_.process(a, b, L, R);
        const float wet = powerGain_, dry = 1.0f - powerGain_;
        outL[i] = L * wet + dL * dry;
        if (!isMono) outR[i] = R * wet + dR * dry;
    }
}

juce::AudioProcessorEditor* DN1000Processor::createEditor()
{
    return new DN1000Editor(*this);
}

void DN1000Processor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void DN1000Processor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

// This function is called by the JUCE plugin wrappers to create the filter.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DN1000Processor();
}
