#pragma once
// PmUI — hardware-matched look for the DN1000 rack unit.
// Visual authority: the owner's gray-hammered 19" rack unit — gray
// hammered faceplate, black channel strips, orange INPUT knobs, yellow
// EQ knobs, green OUTPUT knobs (black pointers), slide switches,
// DI jacks, POL / P-48 toggles, red mushroom power button.
#include <juce_gui_basics/juce_gui_basics.h>

struct PmStyle {
    static juce::Colour face()  { return juce::Colour(0xff8e9194); }
    static juce::Colour faceDark() { return juce::Colour(0xff6f7276); }
    static juce::Colour strip() { return juce::Colour(0xff181310); }
    static juce::Colour silk()  { return juce::Colour(0xfff2f2f2); }
    static juce::Colour dim()   { return juce::Colour(0xffcfcfcf); }

    static juce::Colour knobOrange() { return juce::Colour(0xffe8912d); }
    static juce::Colour knobOrangeDark() { return juce::Colour(0xff9a5c14); }
    static juce::Colour knobLime() { return juce::Colour(0xffadc12e); }
    static juce::Colour knobLimeDark() { return juce::Colour(0xff7a8c1f); }
    static juce::Colour knobGreen()  { return juce::Colour(0xff4a8b6b); }
    static juce::Colour knobGreenDark() { return juce::Colour(0xff2a5a44); }

    static juce::Font silkFont(float size, bool bold = false) {
        return juce::Font(juce::FontOptions(size,
            bold ? juce::Font::bold : juce::Font::plain));
    }
    // Condensed grotesque from the embedded Oswald Bold — matches the
    // PM1000's silkscreen lettering.
    static juce::Font condensed(float size);
    // Sharpie-style handwriting for the board tape.
    static juce::Font marker(float size);
};

class PmLookAndFeel : public juce::LookAndFeel_V4 {
public:
    PmLookAndFeel() = default;

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider& slider) override;
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool highlighted, bool down) override;
    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
                          float sliderPos, float minPos, float maxPos,
                          juce::Slider::SliderStyle style,
                          juce::Slider& slider) override;
    void drawTriToggle(juce::Graphics& g, juce::Slider& slider);
};
