#include "PmUI.h"
#include "BinaryData.h"
#include <cmath>

juce::Font PmStyle::condensed(float size) {
    static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor(
        BinaryData::OswaldBold_ttf, (size_t)BinaryData::OswaldBold_ttfSize);
    return juce::Font(tf).withHeight(size);
}

juce::Font PmStyle::marker(float size) {
    static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor(
        BinaryData::PermanentMarkerRegular_ttf,
        (size_t)BinaryData::PermanentMarkerRegular_ttfSize);
    return juce::Font(tf).withHeight(size);
}

// —————————————————————————————————————————————————————————————————————
// Colored skirted knob, black pointer, tick ring.
// knob property: "orange" (INPUT), "lime" (EQ), "green" (OUTPUT),
// "red", "black".
// —————————————————————————————————————————————————————————————————————
void PmLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w,
                                     int h, float sliderPos, float startAngle,
                                     float endAngle, juce::Slider& slider) {
    const juce::String style = slider.getProperties()
                                   .getWithDefault("knob", juce::String("black"))
                                   .toString();

    juce::Colour light, dark;
    bool blackPointer = true;
    if (style == "orange")      { light = PmStyle::knobOrange();      dark = PmStyle::knobOrangeDark(); }
    else if (style == "lime") { light = PmStyle::knobLime();        dark = PmStyle::knobLimeDark(); }
    else if (style == "green")  { light = PmStyle::knobGreen();       dark = PmStyle::knobGreenDark(); }
    else if (style == "red")    { light = juce::Colour(0xffb83434);   dark = juce::Colour(0xff6d1414); blackPointer = false; }
    else                        { light = juce::Colour(0xff2e2e32);   dark = juce::Colour(0xff17171a); blackPointer = false; }

    auto bounds = juce::Rectangle<int>(x, y, w, h).toFloat().reduced(8.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    // Soft drop shadow — fixed, stays put while the knob turns
    g.setColour(juce::Colour(0xff000000).withAlpha(0.28f));
    g.fillEllipse(centre.x - radius + 1.0f, centre.y - radius + 5.0f,
                  radius * 2.0f, radius * 2.0f);
    g.setColour(juce::Colour(0xff000000).withAlpha(0.18f));
    g.fillEllipse(centre.x - radius - 1.0f, centre.y - radius + 7.0f,
                  radius * 2.0f + 2.0f, radius * 2.0f);

    // Tick ring (indicator markings). tickCount/tickSpanDeg properties
    // let EQ knobs use the hardware's 9 ticks over +/-100 degrees.
    const int nTicks = (int)slider.getProperties().getWithDefault("tickCount", 11);
    const float spanDeg = (float)slider.getProperties().getWithDefault("tickSpanDeg", 270.0);
    const float spanRad = spanDeg * 0.0174533f;
    const float tickStart = -spanRad * 0.5f, tickEnd = spanRad * 0.5f;
    for (int i = 0; i < nTicks; ++i) {
        const float a = tickStart + (float)i / (nTicks - 1) * (tickEnd - tickStart);
        const auto p1 = centre.getPointOnCircumference(radius + 3.0f, a);
        const auto p2 = centre.getPointOnCircumference(radius + 8.0f, a);
        g.setColour(PmStyle::silk());
        g.drawLine(p1.x, p1.y, p2.x, p2.y, 1.6f);
    }

    // Skirt
    g.setColour(juce::Colour(0xff0c0c0e));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    // Body — fixed top-down lighting (bright crown, dark base), never rotates
    const float br = radius * 0.88f;
    juce::ColourGradient body(light.brighter(0.25f), centre.x, centre.y - br,
                              dark.darker(0.35f), centre.x, centre.y + br, false);
    g.setGradientFill(body);
    g.fillEllipse(centre.x - br, centre.y - br, br * 2.0f, br * 2.0f);
    // (Top sheen removed — its arc rendered as a ghost line at 10:30)
    g.setColour(juce::Colour(0xff000000).withAlpha(0.45f));
    g.drawEllipse(centre.x - br, centre.y - br, br * 2.0f, br * 2.0f, 1.5f);

    // Pointer
    const auto tip = centre.getPointOnCircumference(br * 0.82f, angle);
    const auto tail = centre.getPointOnCircumference(br * 0.12f, angle);
    g.setColour(blackPointer ? juce::Colour(0xff141414) : PmStyle::silk());
    g.drawLine(tail.x, tail.y, tip.x, tip.y, 3.4f);
}

// —————————————————————————————————————————————————————————————————————
// Realistic bat toggles: hex mounting nut + threaded bushing + a lever
// that visibly travels up (on) / down (off).
// "bare": small (MIC/LINE, POL). "amp": big old-school amp toggle (POWER).
// —————————————————————————————————————————————————————————————————————
void PmLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                     bool, bool) {
    const juce::String kind = button.getProperties()
                                    .getWithDefault("toggle", juce::String(""))
                                    .toString();
    const bool on = button.getToggleState();
    const auto bounds = button.getLocalBounds().toFloat();
    const auto pivot = bounds.getCentre();
    const float s = (kind == "amp") ? 1.45f : 1.0f;

    // Panel washer
    g.setColour(juce::Colour(0xff0e0e10));
    g.fillEllipse(pivot.x - 10.5f * s, pivot.y - 10.5f * s, 21.0f * s, 21.0f * s);

    // Hex mounting nut — the screw that holds the switch down
    juce::Path hex;
    for (int i = 0; i < 6; ++i) {
        const float a = i * 1.0472f + 0.5236f;
        const auto pt = juce::Point<float>(pivot.x + std::cos(a) * 9.0f * s,
                                           pivot.y + std::sin(a) * 9.0f * s);
        if (i == 0) hex.startNewSubPath(pt); else hex.lineTo(pt);
    }
    hex.closeSubPath();
    juce::ColourGradient nut(juce::Colour(0xffd9d9de), pivot.x - 9.0f * s, pivot.y - 9.0f * s,
                             juce::Colour(0xff7c7c83), pivot.x + 9.0f * s, pivot.y + 9.0f * s, false);
    g.setGradientFill(nut);
    g.fillPath(hex);
    g.setColour(juce::Colour(0xff2a2a2e));
    g.strokePath(hex, juce::PathStrokeType(1.0f));
    g.setColour(juce::Colour(0xffffffff).withAlpha(0.35f));
    for (int i = 4; i < 6; ++i) {
        const float a1 = i * 1.0472f + 0.5236f, a2 = (i + 1) * 1.0472f + 0.5236f;
        g.drawLine(pivot.x + std::cos(a1) * 9.0f * s, pivot.y + std::sin(a1) * 9.0f * s,
                   pivot.x + std::cos(a2) * 9.0f * s, pivot.y + std::sin(a2) * 9.0f * s, 1.2f);
    }

    // Threaded bushing
    juce::ColourGradient bush(juce::Colour(0xffb9b9bf), pivot.x - 4.0f * s, 0.0f,
                              juce::Colour(0xff6e6e75), pivot.x + 4.0f * s, 0.0f, false);
    g.setGradientFill(bush);
    g.fillRoundedRectangle(pivot.x - 4.0f * s, pivot.y - 7.0f * s,
                           8.0f * s, 14.0f * s, 3.0f * s);
    g.setColour(juce::Colour(0xff4a4a50).withAlpha(0.8f));
    for (int i = 0; i < 3; ++i) {
        const float yy = pivot.y - 3.5f * s + i * 3.5f * s;
        g.drawLine(pivot.x - 3.6f * s, yy, pivot.x + 3.6f * s, yy, 0.9f);
    }

    // Lever — travels up (on) / down (off) around the pivot.
    // Traditional bat: uniform shaft, fully rounded tip (never pointy).
    const float ang = on ? -0.30f : juce::MathConstants<float>::pi + 0.30f;
    const float len = 15.0f * s, bw = 3.5f * s;
    juce::Graphics::ScopedSaveState ss(g);
    g.addTransform(juce::AffineTransform::rotation(ang, pivot.x, pivot.y));
    juce::Rectangle<float> bat(pivot.x - bw, pivot.y - len, bw * 2.0f, len);
    // Cylindrical chrome: bright core, dark flanks
    juce::ColourGradient steel(juce::Colour(0xff6e6e75), pivot.x - bw, pivot.y,
                               juce::Colour(0xffffffff), pivot.x - bw * 0.25f, pivot.y, false);
    g.setGradientFill(steel);
    g.fillRoundedRectangle(bat, bw);
    juce::ColourGradient steelR(juce::Colour(0xffe8e8ec), pivot.x + bw * 0.25f, pivot.y,
                                juce::Colour(0xff5a5a61), pivot.x + bw, pivot.y, false);
    g.setGradientFill(steelR);
    g.fillRoundedRectangle(bat.withLeft(pivot.x + bw * 0.25f), bw);
    // Highlight ridge
    g.setColour(juce::Colour(0xffffffff).withAlpha(0.5f));
    g.drawLine(pivot.x - 1.1f * s, pivot.y - 3.0f * s,
               pivot.x - 1.1f * s, pivot.y - len + 6.0f * s, 1.3f);
    // Pivot dome
    g.setColour(juce::Colour(0xff55555c));
    g.fillEllipse(pivot.x - 3.0f * s, pivot.y - 4.5f * s, 6.0f * s, 6.0f * s);
    g.setColour(juce::Colour(0xff2c2c30));
    g.drawEllipse(pivot.x - 3.0f * s, pivot.y - 4.5f * s, 6.0f * s, 6.0f * s, 1.0f);
}

// —————————————————————————————————————————————————————————————————————
// 3-position slide switch (HI-PASS / MID RANGE), like the real Yamaha:
// a small tab that lives INSIDE the slot, never floating on top.
// —————————————————————————————————————————————————————————————————————
void PmLookAndFeel::drawTriToggle(juce::Graphics& g, juce::Slider& slider) {
    // 3-position bat: up / center / down. Center renders as the bat tip
    // seen end-on (a stub), like a real on-off-on toggle.
    const auto bounds = slider.getLocalBounds().toFloat();
    const auto pivot = bounds.getCentre();
    const float s = 1.0f;
    const int pos = (int)std::round(slider.getValue());

    g.setColour(juce::Colour(0xff0e0e10));
    g.fillEllipse(pivot.x - 10.5f, pivot.y - 10.5f, 21.0f, 21.0f);
    juce::Path hex;
    for (int i = 0; i < 6; ++i) {
        const float a = i * 1.0472f + 0.5236f;
        const auto pt = juce::Point<float>(pivot.x + std::cos(a) * 9.0f,
                                           pivot.y + std::sin(a) * 9.0f);
        if (i == 0) hex.startNewSubPath(pt); else hex.lineTo(pt);
    }
    hex.closeSubPath();
    juce::ColourGradient nut(juce::Colour(0xffd9d9de), pivot.x - 9.0f, pivot.y - 9.0f,
                             juce::Colour(0xff7c7c83), pivot.x + 9.0f, pivot.y + 9.0f, false);
    g.setGradientFill(nut);
    g.fillPath(hex);
    g.setColour(juce::Colour(0xff2a2a2e));
    g.strokePath(hex, juce::PathStrokeType(1.0f));
    juce::ColourGradient bush(juce::Colour(0xffb9b9bf), pivot.x - 4.0f, 0.0f,
                              juce::Colour(0xff6e6e75), pivot.x + 4.0f, 0.0f, false);
    g.setGradientFill(bush);
    g.fillRoundedRectangle(pivot.x - 4.0f, pivot.y - 7.0f, 8.0f, 14.0f, 3.0f);

    const float len = 15.0f, bw = 3.5f;
    if (pos == 1) {
        // Center: bat tip seen end-on
        g.setColour(juce::Colour(0xff000000).withAlpha(0.4f));
        g.fillEllipse(pivot.x - bw + 1.0f, pivot.y - bw + 2.0f, bw * 2.0f, bw * 2.0f);
        juce::ColourGradient tip(juce::Colour(0xffffffff), pivot.x - bw, pivot.y - bw,
                                 juce::Colour(0xff8a8a91), pivot.x + bw, pivot.y + bw, false);
        g.setGradientFill(tip);
        g.fillEllipse(pivot.x - bw, pivot.y - bw, bw * 2.0f, bw * 2.0f);
    } else {
        const float ang = (pos == 0) ? -0.30f : juce::MathConstants<float>::pi + 0.30f;
        juce::Graphics::ScopedSaveState ss(g);
        g.addTransform(juce::AffineTransform::rotation(ang, pivot.x, pivot.y));
        juce::Rectangle<float> bat(pivot.x - bw, pivot.y - len, bw * 2.0f, len);
        g.setColour(juce::Colour(0xff000000).withAlpha(0.4f));
        g.fillRoundedRectangle(bat.translated(1.5f, 2.0f), bw);
        juce::ColourGradient steel(juce::Colour(0xff6e6e75), pivot.x - bw, pivot.y,
                                   juce::Colour(0xffffffff), pivot.x - bw * 0.25f, pivot.y, false);
        g.setGradientFill(steel);
        g.fillRoundedRectangle(bat, bw);
        juce::ColourGradient steelR(juce::Colour(0xffe8e8ec), pivot.x + bw * 0.25f, pivot.y,
                                    juce::Colour(0xff5a5a61), pivot.x + bw, pivot.y, false);
        g.setGradientFill(steelR);
        g.fillRoundedRectangle(bat.withLeft(pivot.x + bw * 0.25f), bw);
    }
    g.setColour(juce::Colour(0xff55555c));
    g.fillEllipse(pivot.x - 3.0f, pivot.y - 4.5f, 6.0f, 6.0f);
}

void PmLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w,
                                     int h, float sliderPos, float minPos, float maxPos,
                                     juce::Slider::SliderStyle, juce::Slider& slider) {
    if (slider.getProperties().getWithDefault("triToggle", false)) {
        drawTriToggle(g, slider);
        return;
    }
    const auto bounds = juce::Rectangle<int>(x, y, w, h).toFloat();
    const float cx = bounds.getCentreX();

    // Slot: clean rectangular cutout
    const float slotW = 16.0f;
    juce::Rectangle<float> slot(cx - slotW * 0.5f, bounds.getY() + 2.0f,
                                slotW, bounds.getHeight() - 4.0f);
    g.setColour(juce::Colour(0xff050506));
    g.fillRoundedRectangle(slot, 2.0f);
    // Inner bevel: light catches the left edge
    g.setColour(juce::Colour(0xff3d3b36).withAlpha(0.9f));
    g.drawLine(slot.getX() + 1.5f, slot.getY() + 3.0f,
               slot.getX() + 1.5f, slot.getBottom() - 3.0f, 1.2f);
    g.setColour(juce::Colour(0xff000000).withAlpha(0.8f));
    g.drawLine(slot.getRight() - 1.5f, slot.getY() + 3.0f,
               slot.getRight() - 1.5f, slot.getBottom() - 3.0f, 1.2f);

    // Tab: sits inside the slot, ~1/3 of its height, 3 detents
    const float proportion = juce::jlimit(0.0f, 1.0f,
        (sliderPos - minPos) / (maxPos - minPos));
    const float tabH = 22.0f, tabW = slotW - 3.0f;
    const float travel = slot.getHeight() - tabH - 6.0f;
    const float ty = slot.getY() + 3.0f + (1.0f - proportion) * travel;
    juce::Rectangle<float> tab(cx - tabW * 0.5f, ty, tabW, tabH);
    // Gap shadow around the tab (it sits IN the opening)
    g.setColour(juce::Colour(0xff000000).withAlpha(0.65f));
    g.drawRoundedRectangle(tab.expanded(1.0f), 2.0f, 1.5f);
    juce::ColourGradient tg(juce::Colour(0xff35322c), tab.getX(), tab.getY(),
                            juce::Colour(0xff121110), tab.getX(), tab.getBottom(), false);
    g.setGradientFill(tg);
    g.fillRoundedRectangle(tab, 2.0f);
    // Finger grip: two raised ridges
    g.setColour(juce::Colour(0xff5a554a).withAlpha(0.95f));
    g.drawLine(tab.getX() + 2.5f, tab.getCentreY() - 2.5f,
               tab.getRight() - 2.5f, tab.getCentreY() - 2.5f, 1.6f);
    g.drawLine(tab.getX() + 2.5f, tab.getCentreY() + 2.5f,
               tab.getRight() - 2.5f, tab.getCentreY() + 2.5f, 1.6f);
    g.setColour(juce::Colour(0xff000000).withAlpha(0.6f));
    g.drawLine(tab.getX() + 2.5f, tab.getCentreY() - 1.0f,
               tab.getRight() - 2.5f, tab.getCentreY() - 1.0f, 1.0f);
    juce::ignoreUnused(slider);
}

