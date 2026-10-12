#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
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
            juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f), 10.0f);
    };

    for (auto prefix : { juce::String("a"), juce::String("b") })
    {
        const juce::String chName = prefix == "a" ? "Ch A " : "Ch B ";
        params.push_back(choice(chName + "Input", prefix + "InputLevel",
            { "-60 dBm", "-50 dBm", "-44 dBm", "-38 dBm", "-32 dBm",
              "-26 dBm", "-20 dBm", "-14 dBm", "-8 dBm", "+4 dBm" }, 9));
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
            juce::NormalisableRange<float>(0.0f, 2.0f, 1.0f), 2.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { prefix + "Lpf", 1 }, chName + "Hi-Cut",
            juce::NormalisableRange<float>(0.0f, 2.0f, 1.0f), 2.0f));
        params.push_back(dB(chName + "Bass", prefix + "Bass", -15.0f, 15.0f, 0.0f));
        params.push_back(dB(chName + "Mid-Range", prefix + "Mid", -15.0f, 15.0f, 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { prefix + "MidSel", 1 }, chName + "Mid-Range Freq",
            juce::NormalisableRange<float>(0.0f, 2.0f, 1.0f), 1.0f));
        params.push_back(dB(chName + "Treble", prefix + "Treble", -15.0f, 15.0f, 0.0f));
        params.push_back(gain010(chName + "Output", prefix + "Gain"));
    }
    params.push_back(toggle("Power", "power", true));

    return { params.begin(), params.end() };
}

void DN1000Processor::prepareToPlay(double sampleRate, int)
{
    chA_.prepare(sampleRate);
    chB_.prepare(sampleRate);
    master_.prepare(sampleRate);
    master_.setLevel(7.0f); // fixed unity — per-channel OUTPUT knobs do the work
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

    // POWER off = hard bypass.
    if (apvts.getRawParameterValue("power")->load() < 0.5f)
        return;

    syncParamsToDsp();

    const int n = buffer.getNumSamples();
    const float* inL = buffer.getReadPointer(0);
    const float* inR = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1)
                                                   : buffer.getReadPointer(0);
    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        const float a = chA_.processSample(inL[i]);
        const float b = chB_.processSample(inR[i]);
        float L, R;
        master_.process(a, b, L, R);
        outL[i] = L;
        if (outR != nullptr)
            outR[i] = R;
        else
            outL[i] = 0.5f * (L + R);
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
