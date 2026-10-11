// DN1000 Test Host — standalone app for auditioning audio files.
// Drag-and-drop a WAV/AIFF/MP3, press play, hear it through the DN1000.
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../../dsp/Pm1000Dsp.h"

class TestHostComponent : public juce::AudioAppComponent,
                          public juce::FileDragAndDropTarget,
                          public juce::ChangeListener,
                          private juce::Timer {
public:
    TestHostComponent() {
        formatManager.registerBasicFormats();
        transport.addChangeListener(this);
        
        addAndMakeVisible(openButton);
        openButton.setButtonText("Open Audio File...");
        openButton.onClick = [this] { openFileChooser(); };
        
        addAndMakeVisible(playButton);
        playButton.setButtonText("Play");
        playButton.onClick = [this] { togglePlay(); };
        playButton.setEnabled(false);
        
        addAndMakeVisible(loopButton);
        loopButton.setButtonText("Loop");
        loopButton.setClickingTogglesState(true);
        
        addAndMakeVisible(statusLabel);
        statusLabel.setText("Drag & drop an audio file here", juce::dontSendNotification);
        
        setSize(600, 400);
        setAudioChannels(0, 2); // no input, stereo output
        startTimer(100);
    }
    
    ~TestHostComponent() override {
        shutdownAudio();
    }
    
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override {
        transport.prepareToPlay(samplesPerBlockExpected, sampleRate);
        dspA.prepare(sampleRate);
        dspB.prepare(sampleRate);
        // Default: flat, clean (all knobs center, power on)
        pm1000::ChannelParams p;
        dspA.setParams(p);
        dspB.setParams(p);
    }
    
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override {
        bufferToFill.buffer->clear(bufferToFill.startSample, bufferToFill.numSamples);
        if (!transport.isPlaying()) return;
        
        juce::AudioSourceChannelInfo info(bufferToFill);
        transport.getNextAudioBlock(info);
        
        // Process through DN1000 (A=left, B=right)
        auto* left = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
        auto* right = bufferToFill.buffer->getNumChannels() > 1
            ? bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample)
            : left;
        
        for (int i = 0; i < bufferToFill.numSamples; ++i) {
            left[i] = dspA.processSample(left[i]);
            right[i] = dspB.processSample(right[i]);
        }
    }
    
    void releaseResources() override {
        transport.releaseResources();
    }
    
    void resized() override {
        auto r = getLocalBounds().reduced(20);
        statusLabel.setBounds(r.removeFromTop(30));
        auto btnRow = r.removeFromTop(40);
        openButton.setBounds(btnRow.removeFromLeft(150));
        btnRow.removeFromLeft(10);
        playButton.setBounds(btnRow.removeFromLeft(100));
        btnRow.removeFromLeft(10);
        loopButton.setBounds(btnRow.removeFromLeft(80));
    }
    
    bool isInterestedInFileDrag(const juce::StringArray& files) override {
        return true;
    }
    
    void filesDropped(const juce::StringArray& files, int, int) override {
        if (files.size() > 0) loadFile(juce::File(files[0]));
    }
    
    void timerCallback() override {
        // Update play button state
    }
    
private:
    void openFileChooser() {
        chooser = std::make_unique<juce::FileChooser>(
            "Select audio file...", juce::File(), "*.wav;*.aif;*.aiff;*.mp3;*.flac");
        chooser->launchAsync(juce::FileBrowserComponent::openMode,
            [this](const juce::FileChooser& c) {
                if (c.getResult().existsAsFile())
                    loadFile(c.getResult());
            });
    }
    
    void loadFile(const juce::File& file) {
        auto* reader = formatManager.createReaderFor(file);
        if (reader) {
            auto source = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
            transport.setSource(source.get(), 0, nullptr, reader->sampleRate);
            readerSource = std::move(source);
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
    
    void changeListenerCallback(juce::ChangeBroadcaster*) override {
        if (transport.hasStreamFinished() && loopButton.getToggleState()) {
            transport.setPosition(0.0);
            transport.start();
        }
    }
    
    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::FileChooser> chooser;
    
    juce::TextButton openButton, playButton;
    juce::ToggleButton loopButton;
    juce::Label statusLabel;
    
    pm1000::Channel dspA, dspB;
};

class TestHostApp : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "DN1000 Test Host"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    
    void initialise(const juce::String&) override {
        mainWindow = std::make_unique<MainWindow>();
    }
    
    void shutdown() override { mainWindow = nullptr; }
    
private:
    class MainWindow : public juce::DocumentWindow {
    public:
        MainWindow() : DocumentWindow("DN1000 Test Host",
            juce::Colours::darkgrey, DocumentWindow::allButtons) {
            setContentOwned(new TestHostComponent(), true);
            centreWithSize(600, 400);
            setVisible(true);
        }
        void closeButtonPressed() override {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(TestHostApp)
