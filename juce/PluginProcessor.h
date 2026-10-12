#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Pm1000Dsp.h"

// Dual-channel PM1000 rack strip, modeled on the owner's hardware unit:
// stereo in → Ch A (L) / Ch B (R), each INPUT → POL → HI-PASS →
// BASS/MID-RANGE (700/1.6k/3.2k)/TREBLE → OUTPUT → output-transformer
// color → stereo out. POWER is a hard bypass.
class DN1000Processor : public juce::AudioProcessor
{
public:
    DN1000Processor();
    ~DN1000Processor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DN1000"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Clip indicator levels (0.0 to 1.0+)
    float getChannelALevel() const { return chA_.getLevel(); }
    int getOutMode() const {
        // Get from APVTS (outModeA parameter)
        if (auto* p = apvts.getRawParameterValue("aOutMode"))
            return (int)p->load();
        return 0;
    }
    float getChannelBLevel() const { return chB_.getLevel(); }

private:
    pm1000::Channel chA_, chB_;
    pm1000::MasterStage master_;
    pm1000::PowerSupply psu_;   // ONE shared supply for both channels
    float powerGain_ = 0.0f;    // smoothed power crossfade (item 8)
    // Dry bypass path, delayed to match the processed latency so the
    // power crossfade never combs. Power OFF = clean bypass.
    static constexpr int kDryCap = 4096;
    float dryL_[kDryCap] = {};
    float dryR_[kDryCap] = {};
    int dryPos_ = 0;
    int latency_ = 0;
    float warmth_ = 0.0f;       // warm-up settle state 0..1 (item 8)
    bool wasPowerOn_ = false;

    pm1000::ChannelParams channelParamsFor(const juce::String& prefix);
    void syncParamsToDsp();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DN1000Processor)
};
