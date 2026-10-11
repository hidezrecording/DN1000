#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Pm1000Dsp.h"

// Dual-channel PM1000 rack strip, modeled on the owner's hardware unit:
// stereo in → Ch A (L) / Ch B (R), each INPUT → POL → HI-PASS →
// BASS/MID-RANGE (700/1.6k/3.2k)/TREBLE → OUTPUT → output-transformer
// color → stereo out. POWER is a hard bypass.
class Hamamatsu1000Processor : public juce::AudioProcessor
{
public:
    Hamamatsu1000Processor();
    ~Hamamatsu1000Processor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Hamamatsu 1000"; }
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

private:
    pm1000::Channel chA_, chB_;
    pm1000::MasterStage master_;

    pm1000::ChannelParams channelParamsFor(const juce::String& prefix);
    void syncParamsToDsp();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Hamamatsu1000Processor)
};
