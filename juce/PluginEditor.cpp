#include "PluginEditor.h"
#include "BinaryData.h"
#include <random>

namespace {
constexpr float kRotStart = -2.356f; // -135°
constexpr float kRotEnd   =  2.356f; // +135°
constexpr int kW = 973, kH = 400;

// Strip geometry — modules close together
constexpr int kStripX = 120, kStripW = 728;
constexpr int kStripAY = 50, kStripBY = 210, kStripH = 140;
constexpr int kRowAY = 120, kRowBY = 280;

constexpr int kMicX = 156, kInputX = 236; // +5: evens mic/input/low-cut spacing
constexpr int kHpfX = 320, kMidSwX = 380, kLpfX = 440;
constexpr int kBassX = 510, kMidX = 585, kTrebleX = 660, kOutX = 760, kAmpX = 825;
constexpr int kModeX = 815;
}


// ClipBulb implementation (rainbow transitioning indicator)
ClipBulb::ClipBulb() {
    startTimerHz(30);
}

void ClipBulb::setLevel(float level) {
    level_.store(level);
}

void ClipBulb::timerCallback() {
    const float target = level_.load();
    displayLevel_ += (target - displayLevel_) * 0.3f;
    if (std::abs(target - displayLevel_) > 0.001f)
        repaint();
}

void ClipBulb::paint(juce::Graphics& g) {
    // Bitey-style: chrome hex socket with glowing core (green->yellow->red)
    const float hexD = 16.0f; // Smaller (was 21.5 in Bitey)
    const float cx = getWidth() * 0.5f;
    const float cy = getHeight() * 0.5f - 4.0f; // Shift up to make room for CLIP text below

    const float lvl = juce::jlimit(0.0f, 1.2f, displayLevel_);
    juce::Colour coreCol;
    if (lvl < 0.6f) coreCol = juce::Colour(0xff2fe07a);
    else if (lvl < 0.85f) coreCol = juce::Colour(0xffffc020);
    else coreCol = juce::Colour(0xffff3020);

    // Hex socket (chrome)
    juce::Path hex;
    const float hx[6] = { 0.25f, 0.75f, 0.98f, 0.75f, 0.25f, 0.02f };
    const float hy[6] = { 0.02f, 0.02f, 0.50f, 0.98f, 0.98f, 0.50f };
    for (int i = 0; i < 6; ++i) {
        const float px = cx - hexD * 0.5f + hx[i] * hexD;
        const float py = cy - hexD * 0.5f + hy[i] * hexD;
        if (i == 0) hex.startNewSubPath(px, py);
        else hex.lineTo(px, py);
    }
    hex.closeSubPath();
    juce::ColourGradient hg(juce::Colour(0xffdedede), cx - hexD*0.5f, cy - hexD*0.5f,
                            juce::Colour(0xff686868), cx + hexD*0.5f, cy + hexD*0.5f, false);
    hg.addColour(0.38, juce::Colour(0xff878787));
    hg.addColour(0.58, juce::Colour(0xffcfcfcf));
    g.setGradientFill(hg);
    g.fillPath(hex);

    // Glowing core
    const float coreR = hexD * 0.28f;
    const float glowA = 0.25f + 0.55f * juce::jmin(1.0f, lvl);
    juce::ColourGradient glow(coreCol.withAlpha(glowA), cx, cy,
                              juce::Colour(0x00000000), cx, cy + coreR * 2.2f, true);
    g.setGradientFill(glow);
    g.fillEllipse(cx - coreR*2.2f, cy - coreR*2.2f, coreR*4.4f, coreR*4.4f);
    juce::ColourGradient core(juce::Colour(0xe6ffffff), cx - coreR*0.28f, cy - coreR*0.44f,
                              coreCol.withAlpha(0.85f), cx + coreR*0.3f, cy + coreR*0.4f, true);
    g.setGradientFill(core);
    g.fillEllipse(cx - coreR, cy - coreR, coreR*2.0f, coreR*2.0f);

    // "CLIP" label underneath, centered (space between L and I at bulb center)
    g.setColour(juce::Colour(0xffffffff).withAlpha(0.9f));
    g.setFont(juce::Font("Helvetica", 7.5f, juce::Font::bold));
    // Center the text: the middle of "CLIP" (between L and I) aligns with bulb center cx
    g.drawText("CLIP", 0, (int)(cy + hexD*0.5f + 4), getWidth(), 10,
               juce::Justification::centred);
}

DN1000Editor::DN1000Editor(DN1000Processor& p)
    : AudioProcessorEditor(p), proc_(p)
{
    setLookAndFeel(&laf_);
    setSize(kW, kH);
    renderFaceplate();

    auto& apvts = proc_.apvts;
    const char* prefixes[2] = { "a", "b" };

    for (int c = 0; c < 2; ++c)
    {
        ChannelStrip& s = strips_[c];
        const juce::String pre = prefixes[c];

        setupBareToggle(s.micToggle);
        setupRotary(s.inputKnob, 0, 9, 1, "orange");
        setupSlide(s.hpfSlide);
        setupSlide(s.lpfSlide);
        s.lpfSlide.setDoubleClickReturnValue(true, 2.0); // OFF
        setupSlide(s.midSlide);
        setupRotary(s.bassKnob, -15, 15, 0.1f, "lime");
        setupRotary(s.midKnob, -15, 15, 0.1f, "lime");
        setupRotary(s.trebleKnob, -15, 15, 0.1f, "lime");
        setupRotary(s.outKnob, 0, 10, 0.01f, "green");
        s.outMode.setSliderStyle(juce::Slider::LinearVertical);
        s.outMode.setRange(0.0, 2.0, 1.0);
        s.outMode.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        s.outMode.setDoubleClickReturnValue(true, 1.0);
        s.outMode.getProperties().set("slideSwitch", true);
        addAndMakeVisible(s.outMode);
        for (auto* k : { &s.bassKnob, &s.midKnob, &s.trebleKnob }) {
            k->getProperties().set("tickCount", 11);
            k->getProperties().set("tickSpanDeg", 270.0);
        }

        bAtts_.push_back(std::make_unique<BAtt>(apvts, pre + "Mic", s.micToggle));
        sAtts_.push_back(std::make_unique<SAtt>(apvts, pre + "InputLevel", s.inputKnob));
        sAtts_.push_back(std::make_unique<SAtt>(apvts, pre + "Hpf", s.hpfSlide));
        sAtts_.push_back(std::make_unique<SAtt>(apvts, pre + "Lpf", s.lpfSlide));
        sAtts_.push_back(std::make_unique<SAtt>(apvts, pre + "MidSel", s.midSlide));
        sAtts_.push_back(std::make_unique<SAtt>(apvts, pre + "Bass", s.bassKnob));
        sAtts_.push_back(std::make_unique<SAtt>(apvts, pre + "Mid", s.midKnob));
        sAtts_.push_back(std::make_unique<SAtt>(apvts, pre + "Treble", s.trebleKnob));
        sAtts_.push_back(std::make_unique<SAtt>(apvts, pre + "Gain", s.outKnob));
        sAtts_.push_back(std::make_unique<SAtt>(apvts, pre + "OutMode", s.outMode));

        layoutStrip(s, c == 0 ? kRowAY : kRowBY);
    }

    setupBareToggle(polA_);
    polA_.getProperties().set("invertToggle", true);
    setupBareToggle(polB_);
    polB_.getProperties().set("invertToggle", true);
    bAtts_.push_back(std::make_unique<BAtt>(apvts, "aPhase", polA_));
    bAtts_.push_back(std::make_unique<BAtt>(apvts, "bPhase", polB_));
    polA_.setBounds(71, kRowAY - 28, 30, 56);
    setupBareToggle(linkToggle_);
    linkToggle_.setBounds(71, 172, 30, 56); // Centered between phase toggles (y=200)

    // Stereo Link ganging: when LINK is on, moving a control on one channel
    // moves the matching control on the other channel
    auto gangPair = [this](juce::Slider& a, juce::Slider& b) {
        a.onValueChange = [this, &a, &b] {
            if (ganging_) return;
            if (!linkToggle_.getToggleState()) { // Note: toggle state is inverted (UP=linked)
                ganging_ = true;
                b.setValue(a.getValue(), juce::sendNotification);
                ganging_ = false;
            }
        };
        b.onValueChange = [this, &a, &b] {
            if (ganging_) return;
            if (!linkToggle_.getToggleState()) { // Note: toggle state is inverted (UP=linked)
                ganging_ = true;
                a.setValue(b.getValue(), juce::sendNotification);
                ganging_ = false;
            }
        };
    };
    auto gangTogglePair = [this](juce::ToggleButton& a, juce::ToggleButton& b) {
        a.onClick = [this, &a, &b] {
            if (ganging_) return;
            if (!linkToggle_.getToggleState()) { // Note: toggle state is inverted (UP=linked)
                ganging_ = true;
                b.setToggleState(a.getToggleState(), juce::sendNotification);
                ganging_ = false;
            }
        };
        b.onClick = [this, &a, &b] {
            if (ganging_) return;
            if (!linkToggle_.getToggleState()) { // Note: toggle state is inverted (UP=linked)
                ganging_ = true;
                a.setToggleState(b.getToggleState(), juce::sendNotification);
                ganging_ = false;
            }
        };
    };

    // Gang all channel strip controls
    gangPair(strips_[0].inputKnob, strips_[1].inputKnob);
    gangPair(strips_[0].hpfSlide, strips_[1].hpfSlide);
    gangPair(strips_[0].lpfSlide, strips_[1].lpfSlide);
    gangPair(strips_[0].midSlide, strips_[1].midSlide);
    gangPair(strips_[0].bassKnob, strips_[1].bassKnob);
    gangPair(strips_[0].midKnob, strips_[1].midKnob);
    gangPair(strips_[0].trebleKnob, strips_[1].trebleKnob);
    gangPair(strips_[0].outKnob, strips_[1].outKnob);
    gangPair(strips_[0].outMode, strips_[1].outMode);
    gangTogglePair(strips_[0].micToggle, strips_[1].micToggle);
    gangTogglePair(polA_, polB_);
    polB_.setBounds(71, kRowBY - 28, 30, 56);

    powerToggle_.setClickingTogglesState(true);
    powerToggle_.getProperties().set("toggle", "amp");
    addAndMakeVisible(powerToggle_);
    addAndMakeVisible(linkToggle_);
    // Clip indicators: between treble and output, up higher
    // Position: x=710 (center between treble 660 and output 760), y=cy-50
    addAndMakeVisible(clipA_);
    addAndMakeVisible(clipB_);
    clipA_.setBounds(702, kRowAY - 55, 20, 30); // Taller to fit CLIP text
    clipB_.setBounds(702, kRowBY - 55, 20, 30);
    startTimerHz(30); // Update clip indicators
    bAtts_.push_back(std::make_unique<BAtt>(apvts, "power", powerToggle_));
    bAtts_.push_back(std::make_unique<BAtt>(apvts, "link", linkToggle_));
    powerToggle_.setBounds(866, 205, 36, 70);
    powerToggle_.onClick = [this] { repaint(); }; // refresh the jewel
}

void DN1000Editor::setupRotary(juce::Slider& s, float min, float max,
                                      float step, juce::StringRef knobStyle) {
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setRotaryParameters(kRotStart, kRotEnd, true);
    s.setRange(min, max, step);
    s.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    s.setDoubleClickReturnValue(true, (min + max) * 0.5f);
    s.getProperties().set("knob", juce::String(knobStyle));
    addAndMakeVisible(s);
}

void DN1000Editor::setupSlide(juce::Slider& s) {
    s.setSliderStyle(juce::Slider::LinearVertical);
    s.setRange(0.0, 2.0, 1.0);
    s.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    s.setDoubleClickReturnValue(true, 1.0);
    s.getProperties().set("slideSwitch", true);
    addAndMakeVisible(s);
}

void DN1000Editor::setupBareToggle(juce::ToggleButton& t) {
    t.setClickingTogglesState(true);
    t.getProperties().set("toggle", "bare");
    addAndMakeVisible(t);
}

void DN1000Editor::layoutStrip(ChannelStrip& s, int cy) {
    s.micToggle.setBounds(kMicX - 15, cy - 28, 30, 56);
    s.inputKnob.setBounds(kInputX - 39, cy - 39, 78, 78);
    s.hpfSlide.setBounds(kHpfX - 8, cy - 36, 16, 72);
    s.lpfSlide.setBounds(kLpfX - 8, cy - 36, 16, 72);
    s.midSlide.setBounds(kMidSwX - 8, cy - 36, 16, 72);
    s.bassKnob.setBounds(kBassX - 32, cy - 32, 64, 64);
    s.midKnob.setBounds(kMidX - 32, cy - 32, 64, 64);
    s.trebleKnob.setBounds(kTrebleX - 32, cy - 32, 64, 64);
    s.outKnob.setBounds(kOutX - 39, cy - 39, 78, 78);
    s.outMode.setBounds(kAmpX - 6, cy - 36, 16, 72);
}

// —————————————————————————————————————————————————————————————————————
// Static illustration helpers
// —————————————————————————————————————————————————————————————————————
void DN1000Editor::renderFaceplate() {
    faceplate_ = juce::Image(juce::Image::RGB, kW, kH, false);
    juce::Graphics g(faceplate_);
    g.fillAll(juce::Colour(0xfffdfbf3)); // Ivory (not brushed aluminum)

    std::mt19937 rng(1234);
    #if 0 // Brushed metal skipped for ivory finish
    std::uniform_real_distribution<float> by(0, kH);
    std::uniform_real_distribution<float> bl(60.0f, 400.0f);
    std::uniform_real_distribution<float> ba(0.03f, 0.09f);
    for (int i = 0; i < 1400; ++i) {
        const float y = by(rng);
        const float x = std::uniform_real_distribution<float>(0, kW)(rng);
        const float len = bl(rng);
        const float a = ba(rng);
        const bool light = (i & 1) == 0;
        g.setColour((light ? juce::Colour(0xffe8eaec) : juce::Colour(0xff5a5e63))
                        .withAlpha(a));
        g.fillRect(x, y, len, 1.0f);
    }
#endif
    // Fine hammered grain
    std::uniform_real_distribution<float> dx(0, kW), dy(0, kH);
    std::uniform_real_distribution<float> ds(0.5f, 1.6f);
    for (int i = 0; i < 12000; ++i) {
        const float x = dx(rng), y = dy(rng), s = ds(rng);
        const bool light = (i & 1) == 0;
        g.setColour(light ? juce::Colour(0xffa8adb2).withAlpha(0.35f)
                          : juce::Colour(0xff6e7277).withAlpha(0.35f));
        g.fillEllipse(x, y, s, s * 0.7f);
    }
    // Directional studio light: soft top-left key, gentle falloff
    juce::ColourGradient key(juce::Colour(0xffffffff).withAlpha(0.07f), 0, 0,
                             juce::Colour(0xffffffff).withAlpha(0.0f), kW * 0.7f, kH * 0.7f, false);
    g.setGradientFill(key);
    g.fillRect(0, 0, kW, kH);
    juce::ColourGradient vg(juce::Colour(0xff000000).withAlpha(0.14f), 0, 0,
                            juce::Colour(0xff000000).withAlpha(0.0f), 0, 110, false);
    g.setGradientFill(vg);
    g.fillRect(0, 0, kW, 110);
    juce::ColourGradient vg2(juce::Colour(0xff000000).withAlpha(0.0f), 0, kH - 110,
                             juce::Colour(0xff000000).withAlpha(0.16f), 0, kH, false);
    g.setGradientFill(vg2);
    g.fillRect(0, kH - 110, kW, 110);
}

void DN1000Editor::drawScrew(juce::Graphics& g, float x, float y) {
    g.setColour(juce::Colour(0xff3a3a3e));
    g.fillEllipse(x - 7, y - 7, 14, 14);
    g.setColour(juce::Colour(0xff1e1e20));
    g.drawEllipse(x - 7, y - 7, 14, 14, 1.2f);
    g.setColour(juce::Colour(0xff101012));
    g.drawLine(x - 4.5f, y + 1.5f, x + 4.5f, y - 1.5f, 2.0f);
}

void DN1000Editor::drawHandle(juce::Graphics& g, float x) {
    const float y0 = 118.0f, y1 = 282.0f;
    const float barX = x, footX = x < kW / 2 ? x + 26.0f : x - 26.0f;
    auto metal = [&](float x0) {
        juce::ColourGradient mg(juce::Colour(0xffe8e8ec), x0 - 7, 0,
                                juce::Colour(0xff9a9aa0), x0 + 7, 0, false);
        g.setGradientFill(mg);
    };
    metal(footX);
    g.fillRoundedRectangle(footX - 9, y0 - 9, 18, 22, 4.0f);
    g.fillRoundedRectangle(footX - 9, y1 - 13, 18, 22, 4.0f);
    metal(barX);
    g.fillRoundedRectangle(barX - 7, y0, 14, y1 - y0, 7.0f);
    g.fillRoundedRectangle(std::min(barX, footX) - 7, y0 - 2, std::abs(barX - footX) + 14, 12, 6.0f);
    g.fillRoundedRectangle(std::min(barX, footX) - 7, y1 - 10, std::abs(barX - footX) + 14, 12, 6.0f);
    g.setColour(juce::Colour(0xff5a5a5e).withAlpha(0.6f));
    g.drawRoundedRectangle(barX - 7, y0, 14, y1 - y0, 7.0f, 1.0f);
}

void DN1000Editor::drawDiJack(juce::Graphics& g, float x, float y) {
    juce::ColourGradient nut(juce::Colour(0xffd8d8dc), x - 16, y - 16,
                             juce::Colour(0xff7e7e84), x + 16, y + 16, false);
    g.setGradientFill(nut);
    juce::Path hex;
    for (int i = 0; i < 6; ++i) {
        const float a = i * 1.0472f + 0.5236f;
        const auto pt = juce::Point<float>(x + std::cos(a) * 17.0f, y + std::sin(a) * 17.0f);
        if (i == 0) hex.startNewSubPath(pt); else hex.lineTo(pt);
    }
    hex.closeSubPath();
    g.fillPath(hex);
    g.setColour(juce::Colour(0xff0a0a0c));
    g.fillEllipse(x - 9, y - 9, 18, 18);
    g.setColour(juce::Colour(0xff2e2e32));
    g.drawEllipse(x - 9, y - 9, 18, 18, 1.5f);
}

void DN1000Editor::drawLabelPlate(juce::Graphics& g, juce::StringRef text,
                                         float x, float y) {
    g.setColour(juce::Colour(0xff141416));
    g.fillRoundedRectangle(x - 24, y - 9, 48, 18, 2.0f);
    g.setColour(PmStyle::silk());
    g.setFont(PmStyle::condensed(11.0f));
    g.drawText(text, (int)(x - 24), (int)(y - 9), 48, 18,
               juce::Justification::centred);
}

void DN1000Editor::drawStripLabel(juce::Graphics& g, juce::StringRef text,
                                         int cx, int y, float stretch) {
    juce::Graphics::ScopedSaveState ss(g);
    // Hardware silkscreen: bold, crisp white
    // Large labels (TREBLE, BASS): 1.30x stretch. Small switch labels: 1.0x (no stretch) to avoid overlap.
    g.addTransform(juce::AffineTransform::scale(stretch, 1.0f,
                                               (float)cx, (float)(y + 7)));
    g.setColour(juce::Colours::white.withAlpha(0.96f));
    g.setFont(PmStyle::condensed(10.5f).withExtraKerningFactor(-0.08f));
    g.drawText(text, cx - 60, y, 120, 15, juce::Justification::centred);
}

void DN1000Editor::drawDetentLabels(juce::Graphics& g, juce::Slider& s,
                                           const juce::StringArray& labels,
                                           const std::vector<float>& anglesDeg,
                                           const std::vector<float>& radialOffsets,
                                           float fontSize) {
    const auto b = s.getBounds().toFloat().reduced(8.0f);
    const float radius = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f;
    const auto centre = b.getCentre();
    const int n = labels.size();
    g.setFont(PmStyle::condensed(fontSize));
    // Selection glow: a label lights bright green as the knob reaches it
    const float aMin = anglesDeg.front(), aMax = anglesDeg.back();
    const double vRange = s.getMaximum() - s.getMinimum();
    const float vt = vRange > 0.0 ? (float)((s.getValue() - s.getMinimum()) / vRange) : 0.0f;
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < n; ++i) {
            const float a = anglesDeg[i] * 0.0174533f;
            const float ro = radialOffsets.empty() ? 22.0f : radialOffsets[i];
            const auto pt = centre.getPointOnCircumference(radius + ro, a);
            if (pass == 0) {
                g.setColour(PmStyle::silk());
            } else {
                const float ti = (anglesDeg[i] - aMin) / (aMax - aMin);
                const float w = juce::jmax(0.0f, 1.0f - std::fabs(vt - ti) / 0.20f);
                if (w <= 0.02f) continue;
                g.setColour(juce::Colour(0xff2fe07a).withAlpha(w * w));
            }
            g.drawText(labels[i], (int)(pt.x - 22), (int)(pt.y - 8), 44, 16,
                       juce::Justification::centred);
        }
    }
}

void DN1000Editor::drawToggleMarkers(juce::Graphics& g, juce::ToggleButton& t,
                                            juce::StringRef top,
                                            juce::StringRef bottom,
                                            juce::Colour col) {
    // Labels sit right up against the visible toggle hardware (bezel is
    // ~22px at the component centre; anchor to centre, not the 56px bounds)
    const auto b = t.getBounds();
    const int cx = b.getCentreX();
    const int cy = b.getCentreY();
    g.setFont(PmStyle::condensed(10.0f));
    // The label on the side the bat points to glows bright green
    const bool invert = t.getProperties().getWithDefault("invertToggle", false);
    const bool topActive = (t.getToggleState() != invert);
    const juce::Colour glow(0xff2fe07a);
    g.setColour(topActive ? glow : col);
    g.drawText(top, cx - 24, cy - 28, 48, 14, juce::Justification::centred);
    g.setColour(topActive ? col : glow);
    g.drawText(bottom, cx - 24, cy + 15, 48, 14, juce::Justification::centred);
}

void DN1000Editor::drawTransformerSymbol(juce::Graphics& g, float x, float y) {
    // Transformer glyph: two coupled coils (simplified schematic symbol)
    g.setColour(PmStyle::silk());
    const float r = 6.5f;
    g.drawEllipse(x - r - 4.0f, y - r, r * 2.0f, r * 2.0f, 1.8f);
    g.drawEllipse(x - r + 4.0f, y - r, r * 2.0f, r * 2.0f, 1.8f);
}

void DN1000Editor::drawBoardTape(juce::Graphics& g) {
    const float tw = 224.0f, th = 28.8f; // 80% of the old size
    // Left edge aligned with the modules' left edge; vertically centred
    // in the band between the top of the rack and the top module's top
    // (mirror of its old home below the lower module).
    const float tx = (float)kStripX;
    const float bandTop = 0.0f, bandBot = (float)kStripAY;
    const float ty = (bandTop + bandBot) * 0.5f - th * 0.5f;
    const float cx = tx + tw * 0.5f, cy = ty + th * 0.5f;
    juce::Graphics::ScopedSaveState ss(g);
    g.addTransform(juce::AffineTransform::rotation(-0.015f, cx, cy)); // ~0.86 deg (less tilt)
    // Torn tape shape: jagged left/right edges (ripped from a roll)
    juce::Path tapePath;
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> jag(-3.2f, 3.2f);
    const int nSeg = 12;
    tapePath.startNewSubPath(tx + jag(rng), ty);
    for (int i = 1; i <= nSeg; ++i) {
        const float y = ty + (th * i) / nSeg;
        tapePath.lineTo(tx + jag(rng), y);
    }
    for (int i = nSeg; i >= 0; --i) {
        const float y = ty + (th * i) / nSeg;
        tapePath.lineTo(tx + tw + jag(rng), y);
    }
    tapePath.closeSubPath();
    // Tape shadow
    g.setColour(juce::Colour(0xff000000).withAlpha(0.35f));
    g.fillPath(tapePath, juce::AffineTransform::translation(2.0f, 3.0f));
    // Tape body (no ruled lines)
    juce::ColourGradient tape(juce::Colour(0xffdccb9f), tx, ty,
                              juce::Colour(0xffc9b586), tx, ty + th, false);
    g.setGradientFill(tape);
    g.fillPath(tapePath);
    // Sharpie scribble — long and stretched out
    {
        juce::Graphics::ScopedSaveState ss2(g);
        g.addTransform(juce::AffineTransform::scale(1.18f, 1.0f, cx, cy));
        g.setColour(juce::Colour(0xff232323));
        g.setFont(PmStyle::marker(24.0f));
        g.drawText("DN1000", tx, ty - 2.0f, tw, th + 4.0f,
                   juce::Justification::centred);
    }
}

void DN1000Editor::drawJewel(juce::Graphics& g, float x, float y, bool lit) {
    // Realistic red pilot jewel: knurled bezel + glass lens
    const float r = 15.0f;
    if (lit) {
        // Halation: intense red glow spilling onto the panel (Fender jewel)
        juce::ColourGradient halo(juce::Colour(0xffff2a18).withAlpha(0.55f), x, y,
                                  juce::Colour(0xffff2a18).withAlpha(0.0f), x + r * 2.4f, y, true);
        g.setGradientFill(halo);
        g.fillEllipse(x - r * 2.4f, y - r * 2.4f, r * 4.8f, r * 4.8f);
        juce::ColourGradient halo2(juce::Colour(0xffff5040).withAlpha(0.5f), x, y,
                                   juce::Colour(0xffff5040).withAlpha(0.0f), x + r * 1.5f, y, true);
        g.setGradientFill(halo2);
        g.fillEllipse(x - r * 1.5f, y - r * 1.5f, r * 3.0f, r * 3.0f);
    }
    // Bezel
    juce::ColourGradient bez(juce::Colour(0xff4a4a4e), x - r - 6, y - r - 6,
                             juce::Colour(0xff1a1a1c), x + r + 6, y + r + 6, false);
    g.setGradientFill(bez);
    g.fillEllipse(x - r - 6, y - r - 6, (r + 6) * 2, (r + 6) * 2);
    // Knurl ticks
    g.setColour(juce::Colour(0xff0c0c0e).withAlpha(0.7f));
    for (int i = 0; i < 24; ++i) {
        const float a = i * 0.2618f;
        const auto p1 = juce::Point<float>(x + std::cos(a) * (r + 3.5f),
                                           y + std::sin(a) * (r + 3.5f));
        const auto p2 = juce::Point<float>(x + std::cos(a) * (r + 6.0f),
                                           y + std::sin(a) * (r + 6.0f));
        g.drawLine(p1.x, p1.y, p2.x, p2.y, 1.4f);
    }
    // Lens
    juce::Colour hi = lit ? juce::Colour(0xffff8a70) : juce::Colour(0xff4a1f18);
    juce::Colour lo = lit ? juce::Colour(0xffb81a10) : juce::Colour(0xff200b08);
    juce::ColourGradient lens(hi, x - r * 0.4f, y - r * 0.5f,
                              lo, x + r * 0.6f, y + r * 0.7f, false);
    g.setGradientFill(lens);
    g.fillEllipse(x - r, y - r, r * 2, r * 2);
    if (lit) {
        // Hot core + specular
        juce::ColourGradient core(juce::Colour(0xffffd8cc).withAlpha(1.0f),
                                  x, y - 2, juce::Colour(0xffff3820).withAlpha(0.0f),
                                  x, y + r, false);
        g.setGradientFill(core);
        g.fillEllipse(x - r, y - r, r * 2, r * 2);
        g.setColour(juce::Colour(0xffffffff).withAlpha(0.85f));
        g.fillEllipse(x - r * 0.52f, y - r * 0.66f, r * 0.5f, r * 0.38f);
    } else {
        g.setColour(juce::Colour(0xffffffff).withAlpha(0.12f));
        g.fillEllipse(x - r * 0.52f, y - r * 0.66f, r * 0.5f, r * 0.38f);
    }
    g.setColour(juce::Colour(0xff000000).withAlpha(0.55f));
    g.drawEllipse(x - r, y - r, r * 2, r * 2, 1.5f);
}

void DN1000Editor::paintStrip(juce::Graphics& g, ChannelStrip& s, int cy) {
    // Horizontal silkscreen labels below each control
    drawStripLabel(g, "INPUT", kInputX, cy + 37, 1.30f);
    drawStripLabel(g, "LOW CUT", kHpfX - 8, cy + 37, 1.30f);
    drawStripLabel(g, "MID FREQ", kMidSwX, cy + 37, 1.30f);
    drawStripLabel(g, "BASS", kBassX, cy + 37, 1.30f);
    drawStripLabel(g, "MID-RANGE", kMidX, cy + 37);
    drawStripLabel(g, "TREBLE", kTrebleX, cy + 37, 1.30f);
    drawStripLabel(g, "OUTPUT", kOutX, cy + 37, 1.30f);

    // Toggle markers
    drawToggleMarkers(g, s.micToggle, "MIC", "LINE", PmStyle::silk()); // White: sits on dark strip
    drawStripLabel(g, "AMP", kAmpX, cy + 37, 1.30f); // Stretched like the other labels
    // T / C / S to the LEFT of the switch now; the selected mode glows green
    g.setFont(PmStyle::condensed(10.5f));
    const int mx = kAmpX - 24;
    const char* modes[3] = { "T", "C", "S" };
    const int activeMode = (int)std::round(s.outMode.getValue()); // 0=S, 1=C, 2=T
    for (int i = 0; i < 3; ++i) {
        g.setColour(activeMode == 2 - i ? juce::Colour(0xff2fe07a)
                                        : juce::Colours::white.withAlpha(0.92f));
        g.drawText(modes[i], mx, cy - 36 + 6 + i * 30 - 7, 14, 14,
                   juce::Justification::centred);
    }
    drawDetentLabels(g, s.inputKnob,
        { "-60", "-50", "-44", "-38", "-32", "-26", "-20", "-14", "-8", "+4" },
        { -135, -105, -75, -45, -15, 15, 45, 75, 105, 135 }, {}, 9.0f);
    for (auto* k : { &s.bassKnob, &s.midKnob, &s.trebleKnob })
        drawDetentLabels(g, *k, { "-10", "0", "+10" }, { -135, 0, 135 }, { 18.0f, 22.0f, 18.0f }, 8.5f);
    drawDetentLabels(g, s.outKnob, { "0", "U", "10" }, { -135, 0, 135 }, {}, 8.5f);

    // Slide-switch position labels (left of each switch)
    auto slideLabels = [&](juce::Slider& sl, int x, const juce::StringArray& labs) {
        const auto b = sl.getBounds().toFloat();
        g.setFont(PmStyle::condensed(10.0f));
        const int active = 2 - (int)std::round(sl.getValue()); // labels run top=high value
        for (int i = 0; i < 3; ++i) {
            const float y = b.getY() + 6.0f + (float)i / 2.0f * (b.getHeight() - 12.0f);
            g.setColour(i == active ? juce::Colour(0xff2fe07a)
                                    : juce::Colours::white.withAlpha(0.94f));
            g.drawText(labs[i], x, (int)(y - 7), 26, 14, juce::Justification::right);
        }
    };
    slideLabels(s.hpfSlide, kHpfX - 36, { "80", "40", "OFF" });
    // HIGH CUT: 3 positions (OFF/20K/16K), labels lined up with HI-PASS
    slideLabels(s.lpfSlide, kLpfX - 36, { "16", "20", "OFF" });
    drawStripLabel(g, "HIGH CUT", kLpfX + 8, cy + 37, 1.30f);
    slideLabels(s.midSlide, kMidSwX - 36, { "4K", "2K", "1K" });
}

void DN1000Editor::resized() {
    // Fixed-size rack panel; layout done in the constructor.
}


void DN1000Editor::drawBiteyLogo(juce::Graphics& g) {
    // Bitey logo: mic emblem immediately LEFT of "BITEY", black
    // Centred as a group above the top module
    const auto green = juce::Colours::black; // Black (Nathan 2026-10-12)
    const float cy = 38.0f; // Logo zone: almost sits on the top module (strip top y=50)

    // "BITEY" wordmark in the EMBEDDED font (TeX Gyre Heros Condensed Bold,
    // a Helvetica clone in the Swiss 911 Compressed family) so it renders
    // identically on every machine, Mac or Windows. Black weight, tight
    // spacing, never thinned. Stretched out a touch (scaleX 1.10): the raw
    // condensed face read too smushed.
    g.setColour(green);
    static juce::Typeface::Ptr wordmarkTf = juce::Typeface::createSystemTypefaceFor(
        BinaryData::texgyreheroscnbold_otf,
        (size_t)BinaryData::texgyreheroscnbold_otfSize);
    juce::Font font(wordmarkTf);
    font.setHeight(26.0f);
    font = font.withExtraKerningFactor(0.005f); // Yamaha letters breathe a little
    g.setFont(font);
    const juce::String text = "BITEY";
    const float textW = (float)font.getStringWidth(text);
    const float sx = 1.15f; // Un-smushed and a touch blockier, like the Yamaha face
    // Y's right edge lines up with the right edge of the top module (848)
    const float textRight = 848.0f;
    const float textX = textRight - textW;

    // Measure the actual B glyph so the emblem is EXACTLY the height of
    // the B and centred on it, like another character in the word.
    juce::GlyphArrangement ga;
    ga.addLineOfText(font, "B", 0.0f, 0.0f); // baseline at y=0
    const auto bb = ga.getBoundingBox(0, 1, true);
    // NOTE: GlyphArrangement's box is the whole LINE box (26.0 here), not
    // the ink — sizing off it made the emblem 2x too tall. Render-measured
    // against the real font (harness-verified 2026-10-12): the B's ink is
    // 13.0 px tall at 26 pt, centred 0.5 px below the logo zone centre.
    const float inkH = font.getHeight() * 0.5f;            // B ink height
    const float discR = inkH * 0.5f;   // black disc = EXACT height of the B
    const float ringR = discR + 1.8f;  // outer ring extends slightly past
    const float emblemCy = cy + font.getHeight() * 0.019f; // B ink centre
    const float gap = 2.0f;
    // Where the B's ink actually lands after the scaleX (pivot: textRight)
    const float inkLeft = textRight + (textX + bb.getX() - textRight) * sx;
    const float ex = inkLeft - gap - ringR; // Emblem center x (left of the B)

    {
        juce::Graphics::ScopedSaveState ss(g);
        // Pivot on the right edge so the Y stays pinned to the module edge
        g.addTransform(juce::AffineTransform::scale(sx, 1.0f, textRight, cy));
        g.drawText(text, (int)textX, (int)(cy - 16), (int)(textW + 4), 32,
                   juce::Justification::left);
    }

    // Mic emblem, REVERSED like the Yamaha tuning-fork mark: a thin outer
    // ring, a black filled disc, and the microphones in white inside it.
    // The DISC is exactly the height of the B, centred on the B; the ring
    // extends slightly past the letters.
    g.setColour(green);
    g.drawEllipse(ex - ringR, emblemCy - ringR, ringR*2, ringR*2, 1.2f); // thin outer ring
    g.fillEllipse(ex - discR, emblemCy - discR, discR*2, discR*2); // black disc

    // Three white mics (Yamaha-style, tips form equilateral triangle),
    // scaled to the disc with a little clearance. Capsules narrow so the
    // three heads read as separate microphones.
    g.setColour(juce::Colours::white);
    const float ms = discR / 10.0f * 0.76f; // mics sit well inside the disc, tips clear of the edge
    const float mw = 0.9f; // finer lines for the smaller icons
    for (int i = 0; i < 3; ++i) {
        const float angle = juce::MathConstants<float>::pi * 2.0f * i / 3.0f;
        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(angle, ex, emblemCy));
        const float bx = ex, by = emblemCy;
        auto X = [bx, ms](float v) { return bx + v * ms; };
        auto Y = [by, ms](float v) { return by + v * ms; };
        // Base
        g.drawLine(X(-2.6f), Y(9), X(2.6f), Y(9), mw);
        // Stand
        g.drawLine(X(0), Y(9), X(0), Y(4), mw);
        // Yoke
        juce::Path yoke;
        yoke.startNewSubPath(X(-2.2f), Y(-1));
        yoke.quadraticTo(X(-2.2f), Y(4), X(0), Y(4));
        yoke.quadraticTo(X(2.2f), Y(4), X(2.2f), Y(-1));
        g.strokePath(yoke, juce::PathStrokeType(mw));
        // Capsule (narrow)
        juce::Path cap;
        cap.startNewSubPath(X(-1.6f), Y(-1));
        cap.lineTo(X(-1.6f), Y(-7));
        cap.quadraticTo(X(-1.6f), Y(-10), X(0), Y(-10));
        cap.quadraticTo(X(1.6f), Y(-10), X(1.6f), Y(-7));
        cap.lineTo(X(1.6f), Y(-1));
        cap.quadraticTo(X(1.6f), Y(1.5f), X(0), Y(1.5f));
        cap.quadraticTo(X(-1.6f), Y(1.5f), X(-1.6f), Y(-1));
        g.strokePath(cap, juce::PathStrokeType(mw));
        g.restoreState();
    }
}

void DN1000Editor::paint(juce::Graphics& g) {
    g.drawImageAt(faceplate_, 0, 0);
    drawBiteyLogo(g); // Bitey logo above top module

    // Rack ears: dark cherry wood (Yamaha PM1000 style, darker and redder)
    // Dark cherry: #8a3a22 -> #6b2a18 (30%) -> #4d1e10 (62%) -> #331208
    for (int ear = 0; ear < 2; ++ear) {
        const float x = (ear == 0) ? 0.0f : (float)(kW - 52);
        juce::ColourGradient wood(juce::Colour(0xff8a3a22), x, 0.0f,
                                  juce::Colour(0xff331208), x + 52.0f, 0.0f, false);
        wood.addColour(0.30, juce::Colour(0xff6b2a18));
        wood.addColour(0.62, juce::Colour(0xff4d1e10));
        g.setGradientFill(wood);
        g.fillRect(x, 0.0f, 52.0f, (float)kH);
    }
    g.setColour(juce::Colour(0xff6b4a2a));
    g.drawLine(52, 0, 52, kH, 1.0f);
    g.drawLine(kW - 52, 0, kW - 52, kH, 1.0f);
    drawScrew(g, 26, 26); drawScrew(g, 26, kH - 26);
    drawScrew(g, kW - 26, 26); drawScrew(g, kW - 26, kH - 26);
    drawHandle(g, 22);
    drawHandle(g, kW - 22);

    // — Left block: phase toggles —
    drawToggleMarkers(g, polA_, "0", "180", juce::Colours::black);
    drawToggleMarkers(g, polB_, "0", "180", juce::Colours::black);
    drawToggleMarkers(g, linkToggle_, "UNLINK", "LINK", juce::Colours::black);

    // — Channel strips —
    for (int c = 0; c < 2; ++c) {
        const int sy = c == 0 ? kStripAY : kStripBY;
        g.setColour(PmStyle::strip());
        g.fillRoundedRectangle((float)kStripX, (float)sy, (float)kStripW, (float)kStripH, 4.0f);
        g.setColour(juce::Colour(0xff333336));
        g.drawRoundedRectangle((float)kStripX, (float)sy, (float)kStripW, (float)kStripH, 4.0f, 1.0f);
    }
    paintStrip(g, strips_[0], kRowAY);
    paintStrip(g, strips_[1], kRowBY);

    // — Board tape —
    drawBoardTape(g);

    // — Right block: power toggle + jewel —
    g.setColour(juce::Colours::black); // Black for cream faceplate
    g.setFont(PmStyle::condensed(11.0f));
    g.drawText("POWER", 829, 263, 110, 14, juce::Justification::centred); // Right under the toggle
    drawJewel(g, 884, 160, powerToggle_.getToggleState());
}

void DN1000Editor::timerCallback() {
    // Update clip indicators from DSP levels
    clipA_.setLevel(proc_.getChannelALevel());
    clipB_.setLevel(proc_.getChannelBLevel());
    // Repaint every tick: the selection glows track knob/switch values
    // live, including host automation.
    lastAmpMode_ = proc_.getOutMode();
    repaint();
}
