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

// 10-Octave Compact Color-Coded Piano Strip (MIDI 0..120 -> C0..C10)
class ColorPianoStrip : public juce::Component
{
public:
    ColorPianoStrip (NoiseMu6ixRhythmAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    NoiseMu6ixRhythmAudioProcessor& processor;
    static constexpr int startNote = 0;
    static constexpr int numKeys = 121; // 10 full octaves
    int pressedNote = -1;
};

// 10-Octave Kontakt-Style Key Mapper
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
    std::function<void()> onSelectionChanged;

private:
    NoiseMu6ixRhythmAudioProcessor& processor;
    static constexpr int startNote = 0;
    static constexpr int numKeys = 121; // 10 full octaves

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

    std::function<void()> onMarkersChanged;

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

// Dedicated Global Master Effects Rack (8 Insert Slots + Parameter Controls below)
class GlobalEffectsView : public juce::Component
{
public:
    GlobalEffectsView (NoiseMu6ixRhythmAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void refreshFromProcessor();

private:
    NoiseMu6ixRhythmAudioProcessor& processor;
    int selectedSlot = 0;

    std::array<juce::ComboBox, NoiseMu6ixRhythmAudioProcessor::numGlobalFxSlots> slotBoxes;
    std::array<juce::ToggleButton, NoiseMu6ixRhythmAudioProcessor::numGlobalFxSlots> slotBypass;
    std::array<juce::TextButton, NoiseMu6ixRhythmAudioProcessor::numGlobalFxSlots> slotEditBtns;

    juce::Label activeSlotLabel;
    LabeledKnob p1Knob { "Param 1" }, p2Knob { "Param 2" }, p3Knob { "Mix" };

    void selectSlot (int idx);
};

// Dedicated Per-Zone Mixer View (Color Strips, Vol/Pan/Mute/Solo, Out 1..8, Unlimited Per-Channel Insert FX)
class MixerView : public juce::Component
{
public:
    MixerView (NoiseMu6ixRhythmAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void rebuildStrips();

private:
    NoiseMu6ixRhythmAudioProcessor& processor;
    int selectedFxZone = -1;
    int selectedFxSlot = -1;

    struct ChannelStrip : public juce::Component
    {
        ChannelStrip (NoiseMu6ixRhythmAudioProcessor& p, int zIdx, std::function<void(int, int)> onEditFx, std::function<void()> onRebuild);
        void paint (juce::Graphics& g) override;
        void resized() override;

        NoiseMu6ixRhythmAudioProcessor& processor;
        int zoneIndex = 0;
        juce::Colour stripColour;
        juce::Label nameLabel;
        juce::ComboBox outBusBox;
        juce::TextButton addFxBtn { "+ FX" };
        juce::OwnedArray<juce::TextButton> fxBtns;
        juce::Slider panSlider;
        juce::Slider volSlider;
        juce::ToggleButton muteBtn { "M" };
        juce::ToggleButton soloBtn { "S" };
    };

    juce::Viewport stripViewport;
    juce::Component stripContainer;
    juce::OwnedArray<ChannelStrip> strips;

    // Bottom Inspector for Selected Channel Insert FX
    juce::Label fxInspectorLabel;
    juce::ToggleButton fxEnableBtn { "ON" };
    juce::TextButton fxDeleteBtn { "REMOVE" };
    LabeledKnob fxP1 { "P1" }, fxP2 { "P2" }, fxP3 { "Mix" };

    void selectChannelFx (int zIdx, int fxIdx);
};

class MainPluginView : public juce::Component,
                       public juce::ListBoxModel
{
public:
    enum class ActiveTab { Main = 0, KeyMap, Wave, Rhythm, Effects, Mixer };

    MainPluginView (NoiseMu6ixRhythmAudioProcessor& p, std::function<void(float)> onScaleSelected);
    void paint (juce::Graphics& g) override;
    void resized() override;

    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

    void handleFilesDropped (const juce::StringArray& files, int localX, int localY);
    void syncPerZoneWaveControls();
    void togglePlayPause();
    void repaintDynamicAreas();

private:
    NoiseMu6ixRhythmAudioProcessor& audioProcessor;
    std::function<void(float)> scaleCallback;
    ActiveTab activeTab = ActiveTab::Main;

    // Clickable Title for Scale Menu
    juce::TextButton titleScaleBtn { "NOISE MU6IX RHYTHM" };

    // Top Navigation Tab Buttons
    juce::TextButton tabMainBtn { "MAIN" };
    juce::TextButton tabKeyMapBtn { "KEY MAP" };
    juce::TextButton tabWaveBtn { "WAVE" };
    juce::TextButton tabRhythmBtn { "RHYTHM" };
    juce::TextButton tabEffectsBtn { "EFFECTS" };
    juce::TextButton tabMixerBtn { "MIXER" };

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

    // Per-Zone Wave Editor Controls (NO global APVTS attachments -> bound per-zone!)
    juce::ComboBox trackSelectBox;
    juce::ToggleButton realRRBtn { "RR" };
    juce::ToggleButton sampleLoopBtn { "LOOP" };
    WaveEditorComponent waveEditor;

    LabeledKnob startKnob { "Start" }, endKnob { "End" }, loopStartKnob { "L-Start" }, loopEndKnob { "L-End" };
    LabeledKnob pitchKnob { "Pitch" }, panKnob { "Pan" }, volKnob { "Vol" };
    LabeledKnob attackKnob { "Atk" }, decayKnob { "Dec" }, sustainKnob { "Sus" }, releaseKnob { "Rel" };
    LabeledKnob cutoffKnob { "Cut" }, resKnob { "Res" };

    // Rhythm Sequencer + Transport
    RhythmSequencerComponent seqComp;
    juce::ToggleButton seqLoopBtn { "LOOP" };
    juce::TextButton playPauseBtn { "PLAY" };
    juce::ToggleButton hostSyncBtn { "SYNC" };
    LabeledKnob bpmKnob { "BPM" };
    LabeledKnob shuffleKnob { "Shuffle" }, timeRandKnob { "Time Rnd" }, velRandKnob { "Vel Rnd" };
    juce::Label gridLabel;
    juce::ComboBox gridBox;

    // Dedicated Effects & Mixer Views
    GlobalEffectsView effectsView;
    MixerView mixerView;

    // Bottom 10-Octave Piano Strip
    ColorPianoStrip bottomPiano;

    // Global APVTS Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> frrAtt, fvlAtt, pitchTrackAtt, seqLoopAtt, hostSyncAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAtt, shuffleAtt, timeRandAtt, velRandAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> gridAtt;

    std::unique_ptr<juce::FileChooser> fileChooser;

    void setActiveTab (ActiveTab tab);
    void refreshLibraryFolder (const juce::File& folder);
    void saveAsDialog();
    void showScaleMenu();
};

class NoiseMu6ixRhythmAudioProcessorEditor : public juce::AudioProcessorEditor,
                                             public juce::FileDragAndDropTarget,
                                             public juce::Timer
{
public:
    static constexpr int baseWidth = 940;
    static constexpr int baseHeight = 610;

    NoiseMu6ixRhythmAudioProcessorEditor (NoiseMu6ixRhythmAudioProcessor&);
    ~NoiseMu6ixRhythmAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    bool keyPressed (const juce::KeyPress& key) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    NoiseMu6ixRhythmAudioProcessor& audioProcessor;
    float currentScale = 1.0f;
    MainPluginView mainView;

    void applyUiScale (float newScale);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseMu6ixRhythmAudioProcessorEditor)
};
