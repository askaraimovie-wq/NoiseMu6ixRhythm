#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class KeyMapperComponent : public juce::Component
{
public:
    KeyMapperComponent (NoiseMu6ixRhythmAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    NoiseMu6ixRhythmAudioProcessor& processor;
    static constexpr int startNote = 36; // C1
    static constexpr int numKeys = 49;   // 4 octaves
};

class WaveEditorComponent : public juce::Component
{
public:
    WaveEditorComponent (NoiseMu6ixRhythmAudioProcessor& p);
    void paint (juce::Graphics& g) override;

private:
    NoiseMu6ixRhythmAudioProcessor& processor;
};

class RhythmSequencerComponent : public juce::Component
{
public:
    RhythmSequencerComponent (NoiseMu6ixRhythmAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

private:
    NoiseMu6ixRhythmAudioProcessor& processor;
    void updateStepFromMouse (const juce::MouseEvent& e);
};

class NoiseMu6ixRhythmAudioProcessorEditor : public juce::AudioProcessorEditor,
                                             public juce::FileDragAndDropTarget,
                                             public juce::Timer,
                                             public juce::ListBoxModel
{
public:
    NoiseMu6ixRhythmAudioProcessorEditor (NoiseMu6ixRhythmAudioProcessor&);
    ~NoiseMu6ixRhythmAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

private:
    NoiseMu6ixRhythmAudioProcessor& audioProcessor;

    // Library Panel
    juce::TextButton addLibBtn { "+ ADD LIBRARY" };
    juce::TextButton sortAzBtn { "A-Z" };
    juce::TextButton delLibBtn { "DEL" };
    juce::ListBox libraryList;
    juce::Array<juce::File> libraryFiles;

    // Top Bar
    juce::Label presetLabel;
    juce::TextButton saveBtn { "SAVE .mn6x" };
    juce::TextButton saveAsBtn { "SAVE AS" };

    // Key Mapper + FRR & FVL Buttons
    KeyMapperComponent keyMapper;
    juce::ToggleButton frrBtn { "FRR" };
    juce::ToggleButton fvlBtn { "FVL" };

    // Wave Editor + Controls
    WaveEditorComponent waveEditor;
    juce::ToggleButton sampleLoopBtn { "LOOP" };
    juce::Slider startSlider, endSlider, pitchSlider, volSlider;
    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::Slider cutoffSlider, resSlider;

    // Rhythm Sequencer + Bottom Controls
    RhythmSequencerComponent seqComp;
    juce::ToggleButton seqLoopBtn { "LOOP" };
    juce::Slider shuffleSlider, timeRandSlider, velRandSlider;
    juce::ComboBox gridBox;

    // APVTS Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> frrAtt, fvlAtt, sampleLoopAtt, seqLoopAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> startAtt, endAtt, pitchAtt, volAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAtt, decayAtt, sustainAtt, releaseAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAtt, resAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shuffleAtt, timeRandAtt, velRandAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> gridAtt;

    std::unique_ptr<juce::FileChooser> fileChooser;

    void setupKnob (juce::Slider& s, const juce::String& name);
    void refreshLibraryFolder (const juce::File& folder);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseMu6ixRhythmAudioProcessorEditor)
};
