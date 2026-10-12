#include "PluginEditor.h"
#include <random>

namespace {
constexpr float kRotStart = -2.356f; // -135°
constexpr float kRotEnd   =  2.356f; // +135°
constexpr int kW = 973, kH = 400;

// Strip geometry — modules close together
constexpr int kStripX = 120, kStripW = 728;
constexpr int kStripAY = 50, kStripBY = 210, kStripH = 140;
constexpr int kRowAY = 120, kRowBY = 280;

constexpr int kMicX = 156, kInputX = 231;
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
    // (Brushed metal skipped for ivory finish)
    if (false) // Brushed metal: fine horizontal streaks (anisotropic highlight)
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
    g.setColour(PmStyle::silk());
    g.setFont(PmStyle::condensed(fontSize));
    for (int i = 0; i < n; ++i) {
        const float a = anglesDeg[i] * 0.0174533f;
        const float ro = radialOffsets.empty() ? 22.0f : radialOffsets[i];
        const auto pt = centre.getPointOnCircumference(radius + ro, a);
        g.drawText(labels[i], (int)(pt.x - 22), (int)(pt.y - 8), 44, 16,
                   juce::Justification::centred);
    }
}

void DN1000Editor::drawToggleMarkers(juce::Graphics& g, juce::ToggleButton& t,
                                            juce::StringRef top,
                                            juce::StringRef bottom) {
    const auto b = t.getBounds();
    const int cx = b.getCentreX();
    g.setColour(PmStyle::silk());
    g.setFont(PmStyle::condensed(10.0f));
    g.drawText(top, cx - 24, b.getY() - 6, 48, 14, juce::Justification::centred);
    g.drawText(bottom, cx - 24, b.getBottom() + 0, 48, 14, juce::Justification::centred);
}

void DN1000Editor::drawTransformerSymbol(juce::Graphics& g, float x, float y) {
    // Transformer glyph: two coupled coils (simplified schematic symbol)
    g.setColour(PmStyle::silk());
    const float r = 6.5f;
    g.drawEllipse(x - r - 4.0f, y - r, r * 2.0f, r * 2.0f, 1.8f);
    g.drawEllipse(x - r + 4.0f, y - r, r * 2.0f, r * 2.0f, 1.8f);
}

void DN1000Editor::drawBoardTape(juce::Graphics& g) {
    const float tw = 280.0f, th = 36.0f;
    const float tx = (973.0f - tw) * 0.5f, ty = 359.0f;
    const float cx = tx + tw * 0.5f, cy = ty + th * 0.5f;
    juce::Graphics::ScopedSaveState ss(g);
    g.addTransform(juce::AffineTransform::rotation(-0.015f, cx, cy)); // ~0.86 deg (less tilt)
    // Torn tape shape: jagged left/right edges (ripped from a roll)
    juce::Path tapePath;
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> jag(-4.0f, 4.0f);
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
        g.setFont(PmStyle::marker(30.0f));
        g.drawText("DN1000", tx, ty - 2.0f, tw, th + 4.0f,
                   juce::Justification::centred);
    }
}

void DN1000Editor::drawJewel(juce::Graphics& g, float x, float y, bool lit) {
    // Realistic red pilot jewel: knurled bezel + glass lens
    const float r = 15.0f;
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
    juce::Colour hi = lit ? juce::Colour(0xffff6a55) : juce::Colour(0xff4a1f18);
    juce::Colour lo = lit ? juce::Colour(0xff8f130c) : juce::Colour(0xff200b08);
    juce::ColourGradient lens(hi, x - r * 0.4f, y - r * 0.5f,
                              lo, x + r * 0.6f, y + r * 0.7f, false);
    g.setGradientFill(lens);
    g.fillEllipse(x - r, y - r, r * 2, r * 2);
    if (lit) {
        // Hot core + specular
        juce::ColourGradient core(juce::Colour(0xffffb0a0).withAlpha(0.9f),
                                  x, y - 2, juce::Colour(0xffff5040).withAlpha(0.0f),
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
    drawStripLabel(g, "LOW CUT", kHpfX, cy + 37, 1.0f);
    drawStripLabel(g, "MID RANGE", kMidSwX, cy + 37, 1.0f);
    drawStripLabel(g, "BASS", kBassX, cy + 37, 1.30f);
    drawStripLabel(g, "MID-RANGE", kMidX, cy + 37);
    drawStripLabel(g, "TREBLE", kTrebleX, cy + 37, 1.30f);
    drawStripLabel(g, "OUTPUT", kOutX, cy + 37, 1.30f);

    // Toggle markers
    drawToggleMarkers(g, s.micToggle, "MIC", "LINE");
    drawStripLabel(g, "AMP", kAmpX, cy + 37, 1.0f);
    // T / C / S to the LEFT of the switch now
    g.setColour(juce::Colours::white.withAlpha(0.92f));
    g.setFont(PmStyle::condensed(10.5f));
    const int mx = kAmpX - 24;
    const char* modes[3] = { "T", "C", "S" };
    for (int i = 0; i < 3; ++i)
        g.drawText(modes[i], mx, cy - 36 + 6 + i * 30 - 7, 14, 14,
                   juce::Justification::centred);
    drawDetentLabels(g, s.inputKnob,
        { "-60", "-50", "-44", "-38", "-32", "-26", "-20", "-14", "-8", "+4" },
        { -135, -105, -75, -45, -15, 15, 45, 75, 105, 135 }, {}, 9.0f);
    for (auto* k : { &s.bassKnob, &s.midKnob, &s.trebleKnob })
        drawDetentLabels(g, *k, { "-10", "0", "+10" }, { -135, 0, 135 }, { 18.0f, 22.0f, 18.0f }, 8.5f);
    drawDetentLabels(g, s.outKnob, { "0", "U", "10" }, { -135, 0, 135 }, {}, 8.5f);

    // Slide-switch position labels (left of each switch)
    auto slideLabels = [&](juce::Slider& sl, int x, const juce::StringArray& labs) {
        const auto b = sl.getBounds().toFloat();
        g.setColour(juce::Colours::white.withAlpha(0.94f));
        g.setFont(PmStyle::condensed(10.0f));
        for (int i = 0; i < 3; ++i) {
            const float y = b.getY() + 6.0f + (float)i / 2.0f * (b.getHeight() - 12.0f);
            g.drawText(labs[i], x, (int)(y - 7), 26, 14, juce::Justification::right);
        }
    };
    slideLabels(s.hpfSlide, kHpfX - 36, { "80", "40", "OFF" });
    // HIGH CUT: 3 positions (OFF/20K/16K), labels lined up with HI-PASS
    slideLabels(s.lpfSlide, kLpfX - 36, { "16", "20", "OFF" });
    drawStripLabel(g, "HIGH CUT", kLpfX, cy + 37, 1.0f);
    slideLabels(s.midSlide, kMidSwX - 36, { "4K", "2K", "1K" });
}

void DN1000Editor::resized() {
    // Fixed-size rack panel; layout done in the constructor.
}


void DN1000Editor::drawBiteyLogo(juce::Graphics& g) {
    // Bitey logo: "BITEY" + mic emblem
    // Color changes with amp selector: T=green, C=hot pink, S=blue
    // Centered above top module, fits in y=8 to y=52
    const int mode = proc_.getOutMode();
    juce::Colour logoCol;
    if (mode == 1) logoCol = juce::Colour(0xffff69b4); // C: hot pink
    else if (mode == 2) logoCol = juce::Colour(0xff0080ff); // S: blue
    else logoCol = juce::Colour(0xff2fe07a); // T: green
    const auto green = logoCol;
    const float cy = 30.0f; // Vertical center of logo area
    const float cx = kW * 0.5f; // Horizontal center

    // "BITEY" text: Arial Narrow Black (Swiss 911 equivalent)
    g.setColour(green);
    juce::Font font("Arial Narrow", 32.0f, juce::Font::bold);
    // Condense it (Swiss 911 Compressed vibe)
    g.setFont(font);
    const juce::String text = "BITEY";
    const float textW = 110.0f;
    // Draw text centered, slightly left to make room for emblem
    g.drawText(text, (int)(cx - textW/2 - 30), (int)(cy - 20), (int)textW, 40,
               juce::Justification::centred);

    // Mic emblem circle to the right of text
    const float ex = cx + 45.0f; // Emblem center x
    const float er = 18.0f; // Emblem radius
    g.drawEllipse(ex - er, cy - er, er*2, er*2, 2.0f);

    // Three mics (simplified, green lines)
    g.setColour(green);
    const float mw = 2.0f; // mic line width
    for (int i = 0; i < 3; ++i) {
        const float angle = juce::MathConstants<float>::pi * 2.0f * i / 3.0f;
        g.saveState();
        g.addTransform(juce::AffineTransform::translation(ex, cy).rotated(angle));
        // Mic: base at bottom, capsule at top (simplified)
        g.drawLine(-4, 12, 4, 12, mw); // base
        g.drawLine(0, 12, 0, 5, mw); // stand
        // Yoke
        juce::Path yoke;
        yoke.startNewSubPath(-5, -2);
        yoke.quadraticTo(-5, 5, 0, 5);
        yoke.quadraticTo(5, 5, 5, -2);
        g.strokePath(yoke, juce::PathStrokeType(mw));
        // Capsule
        juce::Path cap;
        cap.startNewSubPath(-4, -2);
        cap.lineTo(-4, -10);
        cap.quadraticTo(-4, -14, 0, -14);
        cap.quadraticTo(4, -14, 4, -10);
        cap.lineTo(4, -2);
        cap.quadraticTo(4, 2, 0, 2);
        cap.quadraticTo(-4, 2, -4, -2);
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
    drawToggleMarkers(g, polA_, "0", "180");
    drawToggleMarkers(g, polB_, "0", "180");
    drawToggleMarkers(g, linkToggle_, "UNLINK", "LINK");

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
    g.setColour(PmStyle::silk());
    g.setFont(PmStyle::condensed(11.0f));
    g.drawText("POWER", 829, 282, 110, 14, juce::Justification::centred);
    drawJewel(g, 884, 160, powerToggle_.getToggleState());
}

void DN1000Editor::timerCallback() {
    // Update clip indicators from DSP levels
    clipA_.setLevel(proc_.getChannelALevel());
    clipB_.setLevel(proc_.getChannelBLevel());
    // Repaint logo if amp mode changed (color changes)
    const int mode = proc_.getOutMode();
    if (mode != lastAmpMode_) {
        lastAmpMode_ = mode;
        repaint(); // Redraw logo with new color
    }
}
