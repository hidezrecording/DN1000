// tests/snapshot.cpp — headless GUI snapshot for visual verification.
// Renders the plugin editor to /tmp/pm1000-gui.png without a display.
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main() {
    juce::ScopedJuceInitialiser_GUI gui;

    Hamamatsu1000Processor proc;
    proc.prepareToPlay(48000.0, 512);

    // Run some audio through so the DSP state is live.
    juce::AudioBuffer<float> buf(2, 4096);
    juce::MidiBuffer midi;
    for (int r = 0; r < 10; ++r) {
        auto* dl = buf.getWritePointer(0);
        auto* dr = buf.getWritePointer(1);
        for (int i = 0; i < 4096; ++i) {
            dl[i] = 0.03f * (float)std::sin(i * 0.05);
            dr[i] = 0.02f * (float)std::sin(i * 0.063 + 1.0);
        }
        proc.processBlock(buf, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setVisible(true);

    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), false, 1.25f);
    juce::File f("/tmp/pm1000-gui.png");
    f.deleteFile();
    juce::FileOutputStream fos(f);
    if (fos.openedOk()) {
        juce::PNGImageFormat png;
        png.writeImageToStream(img, fos);
    }
    return 0;
}
