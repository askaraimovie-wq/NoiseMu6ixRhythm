#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>

class LabeledKnob : public juce::Component
{
public:
    LabeledKnob (const juce::String& labelText);
    void resized() override;
    juce::Slider slider;
    juce::Label label;
};

class ColorPianoStrip : public juce::Component
{
public:
    ColorPianoStrip (NoiseMu6ixRhythmAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    NoiseMu6ixRhythmAudioProcessor& processor;
    static constexpr int startNote = 35; // 35 = Master ALL black key, 36..84 = C1..C5
    static constexpr int numKeys = 50;
    int pressedNote = -1;
};

class KeyMapperComponent : public juce::Component
{
public:
    KeyMapperComponent (NoiseMu6ixRhythmAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    int noteAtX (int x) const;
    std::function<void(int)> onDoubleClickKey;

private:
    NoiseMu6ixRhythmAudioProcessor& processor;
    static constexpr int startNote = 35; // 35 = Master ALL black key, 36..84 = C1..C5
    static constexpr int numKeys = 50;

    enum class DragMode { None, Move, LeftEdge, RightEdge, TopEdge, BottomEdge };
    DragMode dragMode = DragMode::None;
    int dragZoneIdx = -1;
    int dragStartNoteOffset = 0;
    int origLowKey = 60, origHighKey = 60;
    int pressedKey = -1;

    juce::Rectangle<int> getGridArea() const;
    juce::Rectangle<int> getKeyboardArea() const;
    juce::Rectangle<float> getZoneRect (const SampleZone& z, juce::Rectangle<int> gridArea) const;
};

class WaveEditorComponent : public juce::Component
{
public:
    WaveEditorComponent (NoiseMu6ixRhythmAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    NoiseMu6ixRhythmAudioProcessor& processor;
    enum class MarkerDrag { None, Start, End, LoopStart, LoopEnd };
    MarkerDrag activeDrag = MarkerDrag::None;
};

class RhythmSequencerComponent : public juce::Component
{
public:
    RhythmSequencerComponent (NoiseMu6ixRhythmAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    NoiseMu6ixRhythmAudioProcessor& processor;
    int draggedEventIdx = -1;
    float snapToGrid (float normX) const;
};

class MainPluginView : public juce::Component,
                       public juce::ListBoxModel
{
public:
    MainPluginView (NoiseMu6ixRhythmAudioProcessor& p, std::function<void(float)> onScaleSelected);
    void paint (juce::Graphics& g) override;
    void resized() override;

    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

    void handleFilesDropped (const juce::StringArray& files, int localX, int localY);
    void openSampleChooserForKey (int targetKey);
    void repaintDynamicAreas();

private:
    NoiseMu6ixRhythmAudioProcessor& audioProcessor;
    std::function<void(float)> scaleCallback;

    // Clickable Title Button for Scale Menu (50%, 75%, 100%, 125%)
    juce::TextButton titleScaleBtn { "NOISE MU6IX RHYTHM" };

    // Library Panel
    juce::TextButton addLibBtn { "+ ADD LIBRARY" };
    juce::TextButton sortAzBtn { "A-Z" };
    juce::TextButton delLibBtn { "DEL" };
    juce::ListBox libraryList;
    juce::Array<juce::File> libraryFiles;

    // Top Bar
    juce::Label presetNameBox;
    juce::TextButton saveBtn { "SAVE" };
    juce::TextButton saveAsBtn { "SAVE AS" };

    // Key Mapper + FRR / FVL / PITCH
    KeyMapperComponent keyMapper;
    juce::ToggleButton frrBtn { "FRR" };
    juce::ToggleButton fvlBtn { "FVL" };
    juce::ToggleButton pitchTrackBtn { "PITCH" };

    // Wave Editor + Labeled Knobs (including PAN!)
    WaveEditorComponent waveEditor;
    juce::ToggleButton sampleLoopBtn { "LOOP" };
    LabeledKnob startKnob { "Start" }, endKnob { "End" }, loopStartKnob { "L-Start" }, loopEndKnob { "L-End" };
    LabeledKnob pitchKnob { "Pitch" }, panKnob { "Pan" }, volKnob { "Vol" };
    LabeledKnob attackKnob { "Atk" }, decayKnob { "Dec" }, sustainKnob { "Sus" }, releaseKnob { "Rel" };
    LabeledKnob cutoffKnob { "Cut" }, resKnob { "Res" };

    // Rhythm Sequencer + Bottom Controls
    RhythmSequencerComponent seqComp;
    juce::ToggleButton seqLoopBtn { "LOOP" };
    LabeledKnob shuffleKnob { "Shuffle" }, timeRandKnob { "Time Rnd" }, velRandKnob { "Vel Rnd" };
    juce::Label gridLabel;
    juce::ComboBox gridBox;

    // Bottom Playable Color-Coded Keyboard
    ColorPianoStrip bottomPiano;

    // APVTS Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> frrAtt, fvlAtt, pitchTrackAtt, sampleLoopAtt, seqLoopAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> startAtt, endAtt, loopStartAtt, loopEndAtt, pitchAtt, panAtt, volAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAtt, decayAtt, sustainAtt, releaseAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAtt, resAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shuffleAtt, timeRandAtt, velRandAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> gridAtt;

    std::unique_ptr<juce::FileChooser> fileChooser;

    void refreshLibraryFolder (const juce::File& folder);
    void saveAsDialog();
    void showScaleMenu();
};

class NoiseMu6ixRhythmAudioProcessorEditor : public juce::AudioProcessorEditor,
                                             public juce::FileDragAndDropTarget,
                                             public juce::Timer
{
public:
    static constexpr int baseWidth = 920;
    static constexpr int baseHeight = 590;

    NoiseMu6ixRhythmAudioProcessorEditor (NoiseMu6ixRhythmAudioProcessor&);
    ~NoiseMu6ixRhythmAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    NoiseMu6ixRhythmAudioProcessor& audioProcessor;
    float currentScale = 1.0f;
    MainPluginView mainView;

    void applyUiScale (float newScale);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseMu6ixRhythmAudioProcessorEditor)
};
