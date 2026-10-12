#pragma once
// DN1000 — 19" rack unit GUI, identical to the owner's hardware:
// gray hammered faceplate, two black channel strips (MIC/LINE toggle,
// INPUT orange knob, HI-PASS + MID RANGE slide switches, BASS/MID-RANGE/
// TREBLE lime knobs, OUTPUT green knob), DI jacks + POL toggles (left),
// power toggle + red jewel (right), rack handles.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PmUI.h"

struct ChannelStrip {
    juce::ToggleButton micToggle;
    juce::Slider inputKnob, hpfSlide, lpfSlide, midSlide;
    juce::Slider bassKnob, midKnob, trebleKnob, outKnob;
    juce::Slider outMode; // 3-position: Tamura / Cinemag / Sumthang
};

class DN1000Editor : public juce::AudioProcessorEditor
{
public:
    explicit DN1000Editor(DN1000Processor&);
    ~DN1000Editor() override = default;
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    DN1000Processor& proc_;
    PmLookAndFeel laf_;

    ChannelStrip strips_[2];
    juce::ToggleButton polA_, polB_, powerToggle_, linkToggle_;
    bool ganging_ = false; // Prevents recursive link updates

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SAtt>> sAtts_;
    std::vector<std::unique_ptr<BAtt>> bAtts_;

    juce::Image faceplate_; // pre-rendered hammered texture

    void setupRotary(juce::Slider& s, float min, float max, float step,
                     juce::StringRef knobStyle);
    void setupSlide(juce::Slider& s);
    void setupBareToggle(juce::ToggleButton& t);
    void layoutStrip(ChannelStrip& s, int knobCenterY);

    void renderFaceplate();
    void paintStrip(juce::Graphics& g, ChannelStrip& s, int cy);
    void drawStripLabel(juce::Graphics& g, juce::StringRef text,
                        int cx, int y);
    void drawDetentLabels(juce::Graphics& g, juce::Slider& s,
                          const juce::StringArray& labels,
                          const std::vector<float>& anglesDeg,
                          const std::vector<float>& radialOffsets = {},
                          float fontSize = 8.0f);
    void drawToggleMarkers(juce::Graphics& g, juce::ToggleButton& t,
                           juce::StringRef top, juce::StringRef bottom);
    void drawDiJack(juce::Graphics& g, float x, float y);
    void drawLabelPlate(juce::Graphics& g, juce::StringRef text,
                        float x, float y);
    void drawScrew(juce::Graphics& g, float x, float y);
    void drawHandle(juce::Graphics& g, float x);
    void drawJewel(juce::Graphics& g, float x, float y, bool lit);
    void drawTransformerSymbol(juce::Graphics& g, float x, float y);
    void drawBoardTape(juce::Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DN1000Editor)
};
