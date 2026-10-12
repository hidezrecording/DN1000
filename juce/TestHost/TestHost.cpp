// DN1000 Test Host — full plugin GUI + file playback.
// Drag-and-drop audio, use all the knobs/switches, hear the real thing.
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../PluginProcessor.h"
#include "../PluginEditor.h"

class TestHostComponent : public juce::AudioAppComponent,
                          public juce::FileDragAndDropTarget,
                          public juce::ChangeListener {
public:
    TestHostComponent() {
        formatManager.registerBasicFormats();
        
        // Create the real DN1000 processor and editor
        processor = std::make_unique<DN1000Processor>();
        editor = std::unique_ptr<juce::AudioProcessorEditor>(processor->createEditor());
        addAndMakeVisible(editor.get());
        
        // Transport controls
        addAndMakeVisible(openButton);
        openButton.setButtonText("Open File...");
        openButton.onClick = [this] { openFileChooser(); };
        
        addAndMakeVisible(playButton);
        playButton.setButtonText("Play");
        playButton.onClick = [this] { togglePlay(); };
        playButton.setEnabled(false);
        
        addAndMakeVisible(loopButton);
        loopButton.setButtonText("Loop");
        loopButton.setClickingTogglesState(true);
        
        addAndMakeVisible(statusLabel);
        statusLabel.setText("Drag & drop audio file, then use the DN1000 controls", juce::dontSendNotification);
        
        setSize(1000, 600);
        setAudioChannels(0, 2);
    }
    
    ~TestHostComponent() override { shutdownAudio(); }
    
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override {
        transport.prepareToPlay(samplesPerBlockExpected, sampleRate);
        processor->prepareToPlay(sampleRate, samplesPerBlockExpected);
        // Default state: phase 0, LINE, INPUT center (as requested)
        // (Processor defaults already handle this)
    }
    
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override {
        bufferToFill.buffer->clear(bufferToFill.startSample, bufferToFill.numSamples);
        if (!transport.isPlaying()) return;
        
        juce::AudioSourceChannelInfo info(bufferToFill);
        transport.getNextAudioBlock(info);
        
        // Route through the DN1000 processor
        juce::MidiBuffer midi;
        processor->processBlock(*bufferToFill.buffer, midi);
    }
    
    void releaseResources() override {
        transport.releaseResources();
        processor->releaseResources();
    }
    
    void resized() override {
        auto r = getLocalBounds();
        auto topBar = r.removeFromTop(50);
        statusLabel.setBounds(topBar.removeFromLeft(400).reduced(5));
        openButton.setBounds(topBar.removeFromLeft(120).reduced(5));
        playButton.setBounds(topBar.removeFromLeft(80).reduced(5));
        loopButton.setBounds(topBar.removeFromLeft(80).reduced(5));
        if (editor) editor->setBounds(r);
    }
    
    bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
    void filesDropped(const juce::StringArray& files, int, int) override {
        if (files.size() > 0) loadFile(juce::File(files[0]));
    }
    
    void changeListenerCallback(juce::ChangeBroadcaster* src) override {
        if (src == &transport && transport.hasStreamFinished() && loopButton.getToggleState()) {
            transport.setPosition(0.0);
            transport.start();
        }
    }
    
private:
    void openFileChooser() {
        chooser = std::make_unique<juce::FileChooser>("Select audio...",
            juce::File(), "*.wav;*.aif;*.aiff;*.mp3;*.flac");
        chooser->launchAsync(juce::FileBrowserComponent::openMode,
            [this](const juce::FileChooser& c) {
                if (c.getResult().existsAsFile()) loadFile(c.getResult());
            });
    }
    
    void loadFile(const juce::File& file) {
        if (auto* reader = formatManager.createReaderFor(file)) {
            auto source = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
            transport.setSource(source.get(), 0, nullptr, reader->sampleRate);
            readerSource = std::move(source);
            transport.addChangeListener(this);
            statusLabel.setText("Loaded: " + file.getFileName(), juce::dontSendNotification);
            playButton.setEnabled(true);
        }
    }
    
    void togglePlay() {
        if (transport.isPlaying()) {
            transport.stop();
            playButton.setButtonText("Play");
        } else {
            transport.setPosition(0.0);
            transport.start();
            playButton.setButtonText("Stop");
        }
    }
    
    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::FileChooser> chooser;
    
    std::unique_ptr<DN1000Processor> processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    
    juce::TextButton openButton, playButton;
    juce::ToggleButton loopButton;
    juce::Label statusLabel;
};

class TestHostApp : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "DN1000 Test Host"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    void initialise(const juce::String&) override { mainWindow = std::make_unique<MainWindow>(); }
    void shutdown() override { mainWindow = nullptr; }
private:
    class MainWindow : public juce::DocumentWindow {
    public:
        MainWindow() : DocumentWindow("DN1000 Test Host", juce::Colours::darkgrey, DocumentWindow::allButtons) {
            setContentOwned(new TestHostComponent(), true);
            centreWithSize(1000, 600);
            setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(TestHostApp)
