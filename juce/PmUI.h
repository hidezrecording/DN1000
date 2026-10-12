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
    static juce::Colour strip() { return juce::Colour(0xff211a13); } // A hair lighter/browner (Yamaha modules)
    static juce::Colour silk()  { return juce::Colour(0xfff2f2f2); }
    static juce::Colour dim()   { return juce::Colour(0xffcfcfcf); }

    static juce::Colour knobOrange() { return juce::Colour(0xffff9e1f); } // Saturated bright orange
    static juce::Colour knobOrangeDark() { return juce::Colour(0xffbf6509); }
    static juce::Colour knobLime() { return juce::Colour(0xffbcd633); }
    static juce::Colour knobLimeDark() { return juce::Colour(0xff84a024); }
    static juce::Colour knobGreen()  { return juce::Colour(0xff3ea653); } // Saturated deep Yamaha green
    static juce::Colour knobGreenDark() { return juce::Colour(0xff2a6e38); }

    static juce::Font silkFont(float size, bool bold = false) {
        return juce::Font(juce::FontOptions(size,
            bold ? juce::Font::bold : juce::Font::plain));
    }
    // Condensed grotesque from the embedded Oswald Bold — matches the
    // PM1000's silkscreen lettering.
    static juce::Font condensed(float size);
    static void drawStretchedText(juce::Graphics& g, const juce::String& text,
                                 float x, float y, float w, float h,
                                 juce::Justification just, float stretch = 1.25f);
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
