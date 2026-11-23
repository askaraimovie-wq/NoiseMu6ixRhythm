#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>
#include <set>
#include <vector>

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
    static constexpr int startNote = 0;
    static constexpr int numKeys = 121;
    int pressedNote = -1;
};

class KeyMapperComponent : public juce::Component
{
public:
    KeyMapperComponent (NoiseMu6ixRhythmAudioProcessor& p);
    void setReadOnly (bool ro) { readOnly = ro; repaint(); }
    void setVelocityGridDivisions (int divs) { velGridDivs = divs; repaint(); }
    bool isReadOnly() const { return readOnly; }

    void selectAllZones();
    void deleteSelectedZones();
    void clearSelection();
    void selectSingleZone (int idx);
    void autoGridSelectedZones();
    const std::set<int>& getSelectedZoneIndices() const { return selectedZoneIndices; }

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    int noteAtX (int x) const;
    std::function<void()> onSelectionChanged;

private:
    NoiseMu6ixRhythmAudioProcessor& processor;
    static constexpr int startNote = 0;
    static constexpr int numKeys = 121;

    bool readOnly = true;
    int velGridDivs = 4;
    std::set<int> selectedZoneIndices;

    bool isMarqueeDragging = false;
    juce::Point<float> marqueeStart;
    juce::Point<float> marqueeCurrent;

    struct ZoneKeySnapshot { int index = 0; int lowKey = 60; int highKey = 60; };
    enum class DragMode { None, Move, LeftEdge, RightEdge, TopEdge, BottomEdge };
    DragMode dragMode = DragMode::None;
    int dragZoneIdx = -1;
    int dragStartNote = 60;
    std::vector<ZoneKeySnapshot> dragSnapshots;
    int pressedKey = -1;

    juce::Rectangle<int> getGridArea() const;
    juce::Rectangle<int> getKeyboardArea() const;
    juce::Rectangle<float> getZoneRect (const SampleZone& z, juce::Rectangle<int> gridArea) const;
    int snapVelocity (int rawVel) const;
    void sanitizeSelection();
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

class MainPluginView : public juce::Component,
                       public juce::ListBoxModel
{
public:
    enum class ActiveTab { Main = 0, Edit };

    MainPluginView (NoiseMu6ixRhythmAudioProcessor& p, std::function<void(float)> onScaleSelected);
    void paint (juce::Graphics& g) override;
    void resized() override;

    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

    void handleFilesDropped (const juce::StringArray& files, int localX, int localY);
    void syncPerZoneControls();
    void togglePlayPause();
    void selectAllInEditTab();
    void deleteSelectedInEditTab();
    void repaintDynamicAreas();

private:
    NoiseMu6ixRhythmAudioProcessor& audioProcessor;
    std::function<void(float)> scaleCallback;
    ActiveTab activeTab = ActiveTab::Main;

    juce::TextButton titleScaleBtn { "NOISE MU6IX RHYTHM" };
    juce::TextButton tabMainBtn { "MAIN" };
    juce::TextButton tabEditBtn { "EDIT" };

    // Library Panel
    juce::TextButton addLibBtn { "+ ADD LIBRARY" };
    juce::TextButton sortAzBtn { "A-Z" };
    juce::TextButton delLibBtn { "DEL" };
    juce::ListBox libraryList;
    juce::Array<juce::File> libraryFiles;

    // Top Bar
    juce::TextEditor presetNameBox;
    juce::TextButton saveBtn { "SAVE" };
    juce::TextButton saveAsBtn { "SAVE AS" };

    // ---------------- MAIN TAB ----------------
    KeyMapperComponent mainKeyMapper;
    RhythmSequencerComponent seqComp;
    juce::TextButton playPauseBtn { "PLAY" };
    juce::ToggleButton hostSyncBtn { "SYNC" };
    juce::Label bpmLabel;
    juce::TextEditor bpmInputBox;
    LabeledKnob shuffleKnob { "Shuffle" }, timeRandKnob { "Time Rnd" }, velRandKnob { "Vel Rnd" };
    juce::Label gridLabel;
    juce::ComboBox gridBox;
    juce::ToggleButton seqLoopBtn { "LOOP" };
    juce::TextButton seqClearBtn { "Clr" };

    // ---------------- EDIT TAB (Clean Simple Layout) ----------------
    // Compact Group Strip
    juce::TextButton addGroupBtn { "+ Grp" };
    juce::ComboBox groupSelectBox;
    juce::TextEditor groupNameEdit;
    juce::ToggleButton groupMuteBtn { "M" };
    juce::ComboBox groupRrBox;
    juce::ComboBox groupKsBox;
    LabeledKnob grpVolKnob { "G.Vol" }, grpPanKnob { "G.Pan" };

    // Compact Key Mapper Header
    juce::ToggleButton frrBtn { "FRR" };
    juce::ToggleButton fvlBtn { "FVL" };
    juce::ToggleButton pitchTrackBtn { "PITCH" };
    juce::TextButton selectAllZonesBtn { "ALL" };
    juce::TextButton delZoneBtn { "Del" };
    juce::TextButton autoGridBtn { "Auto Grid" };
    juce::ComboBox velGridBox;
    juce::ComboBox zoneColourBox;
    KeyMapperComponent editKeyMapper;

    // Compact Wave Editor + Knobs
    juce::ComboBox trackSelectBox;
    juce::ToggleButton realRRBtn { "RR" };
    juce::ToggleButton sampleLoopBtn { "LOOP" };
    juce::ToggleButton reverseBtn { "REV" };
    juce::TextButton normalizeBtn { "NORM" };
    WaveEditorComponent waveEditor;

    LabeledKnob startKnob { "Start" }, endKnob { "End" }, loopStartKnob { "L-Start" }, loopEndKnob { "L-End" };
    LabeledKnob pitchKnob { "Pitch" }, panKnob { "Pan" }, volKnob { "Vol" };
    LabeledKnob attackKnob { "Atk" }, decayKnob { "Dec" }, sustainKnob { "Sus" }, releaseKnob { "Rel" };
    LabeledKnob cutoffKnob { "Cut" }, resKnob { "Res" };

    // Simple FX Strip (4 slots + 3 knobs)
    juce::ComboBox fxSlotSelectBox;
    juce::ComboBox fxTypeBox;
    juce::ToggleButton fxEnableBtn { "ON" };
    LabeledKnob fxP1 { "P1" }, fxP2 { "P2" }, fxP3 { "P3" };

    // Bottom 10-Octave Piano Strip
    ColorPianoStrip bottomPiano;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> frrAtt, fvlAtt, pitchTrackAtt, seqLoopAtt, hostSyncAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shuffleAtt, timeRandAtt, velRandAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> gridAtt;

    std::unique_ptr<juce::FileChooser> fileChooser;

    void setActiveTab (ActiveTab tab);
    void refreshGroupBoxes();
    void syncGroupControls();
    void syncFxControls();
    void applyBpmText();
    void refreshLibraryFolder (const juce::File& folder);
    void saveAsDialog();
    void showScaleMenu();
};

class NoiseMu6ixRhythmAudioProcessorEditor : public juce::AudioProcessorEditor,
                                             public juce::FileDragAndDropTarget,
                                             public juce::Timer
{
public:
    static constexpr int baseWidth = 960;
    static constexpr int baseHeight = 600;

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
