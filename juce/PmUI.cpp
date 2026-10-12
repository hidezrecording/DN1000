#include "PmUI.h"
#include "BinaryData.h"
#include <cmath>

juce::Font PmStyle::condensed(float size) {
    // Yamaha PM1000: clean grotesque sans silkscreen
    // Regular weight: bold read slightly too heavy (Nathan 2026-10-12)
    return juce::Font("Helvetica", size, juce::Font::plain);
}

// Draw text with horizontal stretch (Yamaha PM1000 uses extended letterforms)
// stretch: 1.0 = normal, 1.25 = 25% wider (matches Yamaha silkscreen)
void PmStyle::drawStretchedText(juce::Graphics& g, const juce::String& text,
                               float x, float y, float w, float h,
                               juce::Justification just, float stretch) {
    juce::Graphics::ScopedSaveState ss(g);
    const float cx = x + w * 0.5f;
    // Scale horizontally around the center x
    g.addTransform(juce::AffineTransform::scale(stretch, 1.0f, cx, y));
    g.drawText(text, x, y, w, h, just);
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

    // Selection glow (PV-style): the tick at the knob's current value
    // glows bright green; between ticks the two neighbours share the
    // glow, handing off smoothly as the knob turns.
    {
        const double vmin = slider.getMinimum(), vmax = slider.getMaximum();
        const float ft = vmax > vmin
            ? (float)((slider.getValue() - vmin) / (vmax - vmin)) * (float)(nTicks - 1)
            : 0.0f;
        for (int i = 0; i < nTicks; ++i) {
            const float w = juce::jmax(0.0f, 1.0f - std::fabs(ft - (float)i));
            if (w <= 0.01f) continue;
            const float a = tickStart + (float)i / (nTicks - 1) * (tickEnd - tickStart);
            const auto p1 = centre.getPointOnCircumference(radius + 3.0f, a);
            const auto p2 = centre.getPointOnCircumference(radius + 8.0f, a);
            g.setColour(juce::Colour(0xffffff00).withAlpha(0.22f * w));
            g.drawLine(p1.x, p1.y, p2.x, p2.y, 4.4f); // halo
            g.setColour(juce::Colour(0xffffff00).withAlpha(0.95f * w));
            g.drawLine(p1.x, p1.y, p2.x, p2.y, 1.8f); // core
        }
    }

    // Skirt — glossy black shaft (differentiates from faceplate)
    juce::ColourGradient shaftG(juce::Colour(0xff2a2a2e), centre.x, centre.y - radius,
                                juce::Colour(0xff050506), centre.x, centre.y + radius, false);
    g.setGradientFill(shaftG);
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    // Glossy specular highlight (top-left, fixed light source, PV-style)
    // Near-white and strong: reads as a studio-light reflection on the pot
    g.setColour(juce::Colour(0xffffffff).withAlpha(0.50f));
    g.fillEllipse(centre.x - radius * 0.62f, centre.y - radius * 0.78f,
                  radius * 0.58f, radius * 0.36f);
    g.setColour(juce::Colour(0xffffffff).withAlpha(0.20f));
    g.fillEllipse(centre.x - radius * 0.72f, centre.y - radius * 0.86f,
                  radius * 0.85f, radius * 0.55f);

    // Body — fixed top-down lighting (bright crown, dark base), never rotates
    const float br = radius * 0.88f;
    juce::ColourGradient body(light.brighter(0.3f), centre.x - br * 0.3f, centre.y - br,
                              dark.darker(0.4f), centre.x + br * 0.2f, centre.y + br, false);
    g.setGradientFill(body);
    g.fillEllipse(centre.x - br, centre.y - br, br * 2.0f, br * 2.0f);
    // Knurled grip: fine radial ridges around the edge (like real hardware)
    const int nKnurl = 48;
    for (int i = 0; i < nKnurl; ++i) {
        const float a = (float)i / nKnurl * 6.28318f;
        const auto p1 = centre.getPointOnCircumference(br * 0.97f, a);
        const auto p2 = centre.getPointOnCircumference(br * 1.0f, a);
        // Alternate light/dark for ridged effect
        g.setColour((i & 1) ? juce::Colour(0x000000).withAlpha(0.0f)
                            : dark.darker(0.5f).withAlpha(0.55f));
        if ((i & 1) == 0)
            g.drawLine(p1.x, p1.y, p2.x, p2.y, 1.8f);
    }
    // (Rim light and sheen removed — arcs render as ghost lines)
    g.setColour(juce::Colour(0xff000000).withAlpha(0.5f));
    g.drawEllipse(centre.x - br, centre.y - br, br * 2.0f, br * 2.0f, 1.5f);

    // Soft sheen on the exposed black flute (skirt ring), top-left, fixed
    // light like the PV knobs — a soft gradient spot, not a hard arc
    {
        const float fx = centre.x - radius * 0.66f, fy = centre.y - radius * 0.66f;
        juce::ColourGradient sheen(juce::Colour(0xffffffff).withAlpha(0.27f), fx, fy,
                                   juce::Colour(0x00ffffff), fx + radius * 0.30f, fy + radius * 0.30f, true);
        g.setGradientFill(sheen);
        g.fillEllipse(fx - radius * 0.30f, fy - radius * 0.30f, radius * 0.60f, radius * 0.60f);
    }

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
    const bool invert = button.getProperties().getWithDefault("invertToggle", false);
    const auto bounds = button.getLocalBounds().toFloat();
    const auto pivot = bounds.getCentre();
    const float s = (kind == "amp") ? 1.45f : 1.0f;

    // Outer chrome bezel (round, like the UA reference)
    const float bezR = 11.0f * s;
    juce::ColourGradient bez(juce::Colour(0xffe8e8ec), pivot.x - bezR, pivot.y - bezR,
                             juce::Colour(0xff6a6a72), pivot.x + bezR, pivot.y + bezR, false);
    g.setGradientFill(bez);
    g.fillEllipse(pivot.x - bezR, pivot.y - bezR, bezR * 2.0f, bezR * 2.0f);
    // Bezel highlight (top-left)
    g.setColour(juce::Colour(0xffffffff).withAlpha(0.75f));
    g.drawEllipse(pivot.x - bezR + 1.0f * s, pivot.y - bezR + 1.0f * s,
                  (bezR - 1.0f * s) * 2.0f, (bezR - 1.0f * s) * 2.0f, 1.5f);
    // Dark inner circle
    const float inR = 7.5f * s;
    juce::ColourGradient inner(juce::Colour(0xff3a3a40), pivot.x - inR, pivot.y - inR,
                               juce::Colour(0xff0e0e10), pivot.x + inR, pivot.y + inR, false);
    g.setGradientFill(inner);
    g.fillEllipse(pivot.x - inR, pivot.y - inR, inR * 2.0f, inR * 2.0f);

    // Lever — short stubby bat with ball tip (UA reference)
    const float ang = (on != invert) ? -0.32f : juce::MathConstants<float>::pi + 0.32f;
    const float len = 12.0f * s, bw = 3.5f * s;
    juce::Graphics::ScopedSaveState ss(g);
    g.addTransform(juce::AffineTransform::rotation(ang, pivot.x, pivot.y));
    const float bx = pivot.x, by = pivot.y - len;
    // Bat shaft: polished chrome cylinder
    // Left flank (dark) → center highlight → right flank (mid-dark)
    juce::ColourGradient shaft(juce::Colour(0xff4a4a52), bx - bw, pivot.y,
                               juce::Colour(0xffffffff), bx - bw * 0.2f, pivot.y, false);
    g.setGradientFill(shaft);
    g.fillRoundedRectangle(bx - bw, by, bw * 1.8f, len, bw * 0.9f);
    juce::ColourGradient shaftR(juce::Colour(0xffffffff), bx - bw * 0.2f, pivot.y,
                                juce::Colour(0xff5a5a62), bx + bw, pivot.y, false);
    g.setGradientFill(shaftR);
    g.fillRoundedRectangle(bx - bw * 0.2f, by, bw * 1.2f, len, bw * 0.6f);
    // Bright specular streak (left-of-center, like reference)
    g.setColour(juce::Colour(0xffffffff).withAlpha(0.95f));
    g.fillRoundedRectangle(bx - bw * 0.55f, by + 1.5f * s,
                           bw * 0.32f, len - 3.0f * s, bw * 0.16f);
    // Ball tip: sphere at the end
    const float ballR = bw * 1.1f;
    const float ballX = bx, ballY = by - ballR * 0.3f;
    juce::ColourGradient ball(juce::Colour(0xffffffff), ballX - ballR, ballY - ballR,
                              juce::Colour(0xff5a5a62), ballX + ballR, ballY + ballR, false);
    g.setGradientFill(ball);
    g.fillEllipse(ballX - ballR, ballY - ballR, ballR * 2.0f, ballR * 2.0f);
    // Ball highlight (fixed light source, near-white studio reflection)
    g.setColour(juce::Colour(0xffffffff).withAlpha(0.55f));
    g.fillEllipse(ballX - ballR * 0.5f, ballY - ballR * 0.6f,
                  ballR * 0.6f, ballR * 0.45f);
    // Highlight ridge
    g.setColour(juce::Colour(0xffffffff).withAlpha(0.5f));
    g.drawLine(pivot.x - 1.1f * s, pivot.y - 3.0f * s,
               pivot.x - 1.1f * s, pivot.y - len + 6.0f * s, 1.3f);
    // (Pivot dome removed)
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
        // Specular streak (studio reflection, matches the big toggles)
        g.setColour(juce::Colour(0xffffffff).withAlpha(0.8f));
        g.fillRoundedRectangle(pivot.x - bw * 0.55f, pivot.y - len + 1.5f,
                               bw * 0.32f, len - 3.0f, bw * 0.16f);
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

    // Tab: aligns with labels at top/bottom
    // Labels at b.y+6 (top) and b.y+h-6 (bottom), thumb center must match
    const float val = juce::jlimit(0.0f, 2.0f, (float)slider.getValue());
    const float proportion = val / 2.0f; // 0.0=bottom, 1.0=top
    const float tabH = 22.0f, tabW = slotW - 3.0f;
    const float travel = slot.getHeight() - 8.0f;
    const float ty = slot.getY() - 7.0f + (1.0f - proportion) * travel;
    juce::Rectangle<float> tab(cx - tabW * 0.5f, ty, tabW, tabH);
    // Gap shadow around the tab (it sits IN the opening)
    g.setColour(juce::Colour(0xff000000).withAlpha(0.65f));
    g.drawRoundedRectangle(tab.expanded(1.0f), 2.0f, 1.5f);
    juce::ColourGradient tg(juce::Colour(0xff35322c), tab.getX(), tab.getY(),
                            juce::Colour(0xff121110), tab.getX(), tab.getBottom(), false);
    g.setGradientFill(tg);
    g.fillRoundedRectangle(tab, 2.0f);
    // Studio reflection: soft white sheen across the top of the tab
    juce::ColourGradient tsh(juce::Colour(0xffffffff).withAlpha(0.18f), tab.getX(), tab.getY(),
                             juce::Colour(0x00ffffff), tab.getX(), tab.getY() + tabH * 0.55f, false);
    g.setGradientFill(tsh);
    g.fillRoundedRectangle(tab, 2.0f);
    g.setColour(juce::Colour(0xffffffff).withAlpha(0.32f));
    g.drawLine(tab.getX() + 2.0f, tab.getY() + 1.0f,
               tab.getRight() - 2.0f, tab.getY() + 1.0f, 1.2f);
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

