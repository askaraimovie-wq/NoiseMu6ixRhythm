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

// 10-Octave Compact Color-Coded Piano Strip (with RED Keyswitch highlight!)
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

// Kontakt-Style Group Editor Sub-Tab: 5x5 Vertical Grid (25 visible, Row 6+ Scrolls) + Rename + Mute + RR Set + Keyswitch
class GroupEditorPanel : public juce::Component
{
public:
    GroupEditorPanel (NoiseMu6ixRhythmAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void rebuildGroupGrid();

    std::function<void()> onGroupChanged;

private:
    NoiseMu6ixRhythmAudioProcessor& processor;

    struct GroupTile : public juce::Component
    {
        GroupTile (NoiseMu6ixRhythmAudioProcessor& p, int idx, std::function<void(int)> onSel, std::function<void()> onMut);
        void paint (juce::Graphics& g) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent& e) override;

        NoiseMu6ixRhythmAudioProcessor& processor;
        int groupIndex = 0;
        std::function<void(int)> onSelect;
        juce::Label nameLbl;
        juce::ToggleButton muteBtn { "M" };
    };

    juce::Label titleLabel;
    juce::ToggleButton editAllGroupsBtn { "Edit All Groups" };
    juce::ToggleButton groupSoloBtn { "Group Solo" };
    juce::ToggleButton selectByMidiBtn { "Select by MIDI" };
    juce::TextButton createGroupBtn { "+ Empty Group" };
    juce::TextButton deleteGroupBtn { "Del Grp" };

    // 5x5 Vertical Scrollable Grid
    juce::Viewport gridViewport;
    juce::Component gridContent;
    juce::OwnedArray<GroupTile> groupTiles;

    // Group Start Options Bar (Name, RR Set, Keyswitch)
    juce::Label startBarLabel;
    juce::Label nameFieldLabel;
    juce::TextEditor groupNameEdit;
    juce::Label rrSetLabel;
    juce::ComboBox rrSetBox;
    juce::Label ksLabel;
    juce::ComboBox keyswitchBox;

    void syncStartBar();
};

// 10-Octave Key Mapper (Zoomable, Multi-Select, Auto-Grid, Auto-Map, Auto-Spread, Red Keyswitch)
class KeyMapperComponent : public juce::Component
{
public:
    KeyMapperComponent (NoiseMu6ixRhythmAudioProcessor& p);
    void setReadOnly (bool ro) { readOnly = ro; repaint(); }
    void setVelocityGridDivisions (int divs) { velGridDivs = divs; repaint(); }
    void setSelectedGroupsOnly (bool sgo) { selectedGroupsOnly = sgo; repaint(); }
    bool isReadOnly() const { return readOnly; }

    void selectAllZones();
    void clearSelection();
    void selectSingleZone (int idx);
    void autoGridSelectedZones();
    void autoMapSelectedByFilename();
    void autoSpreadKeyRanges();
    void moveSelectedZonesToGroup (int newGroupIdx);
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
    bool selectedGroupsOnly = true;
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

    bool isZoneVisible (const SampleZone& z) const;
    juce::Rectangle<int> getGridArea() const;
    juce::Rectangle<int> getKeyboardArea() const;
    juce::Rectangle<float> getZoneRect (const SampleZone& z, juce::Rectangle<int> gridArea) const;
    int snapVelocity (int rawVel) const;
    void sanitizeSelection();
};

// Zoomable Kontakt-Style Sample Wave Editor with Grid & Snap
class WaveEditorComponent : public juce::Component
{
public:
    WaveEditorComponent (NoiseMu6ixRhythmAudioProcessor& p);
    void setSnapMode (int mode) { snapMode = mode; repaint(); } // 0=Off, 1=Grid, 2=Zero-X
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    std::function<void()> onMarkersChanged;

private:
    NoiseMu6ixRhythmAudioProcessor& processor;
    int snapMode = 0;
    enum class MarkerDrag { None, Start, End, LoopStart, LoopEnd };
    MarkerDrag activeDrag = MarkerDrag::None;

    float applySnap (float normX, const SampleZone& z) const;
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
    enum class EditSubTab { Group = 0, Mapping, Wave };
    enum class WaveSubTab { Loop = 0, GridSync, EditTone };

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
    void repaintDynamicAreas();

private:
    NoiseMu6ixRhythmAudioProcessor& audioProcessor;
    std::function<void(float)> scaleCallback;
    ActiveTab activeTab = ActiveTab::Main;
    EditSubTab editSubTab = EditSubTab::Mapping;
    WaveSubTab waveSubTab = WaveSubTab::Loop;

    // Clickable Title for Scale Menu
    juce::TextButton titleScaleBtn { "NOISE MU6IX RHYTHM" };

    // Top-level Tabs: MAIN and EDIT
    juce::TextButton tabMainBtn { "MAIN" };
    juce::TextButton tabEditBtn { "EDIT" };

    // EDIT Sub-Tab Buttons: Group Editor | Mapping Editor | Wave Editor
    juce::TextButton subGroupBtn { "Group Editor" };
    juce::TextButton subMapBtn   { "Mapping Editor" };
    juce::TextButton subWaveBtn  { "Wave Editor" };

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

    // ---------------- EDIT SUB-TAB 1: GROUP EDITOR ----------------
    GroupEditorPanel groupPanel;

    // ---------------- EDIT SUB-TAB 2: MAPPING EDITOR ----------------
    juce::ToggleButton frrBtn { "FRR" };
    juce::ToggleButton fvlBtn { "FVL" };
    juce::ToggleButton pitchTrackBtn { "PITCH" };
    juce::TextButton selectAllZonesBtn { "ALL" };
    juce::ToggleButton zoneSoloBtn { "Zone Solo" };
    juce::ToggleButton selGroupsOnlyBtn { "Sel Grp Only" };
    juce::ToggleButton listViewBtn { "List View" };
    juce::TextButton autoGridBtn { "Auto Grid" };
    juce::TextButton autoMapBtn { "Auto-Map" };
    juce::TextButton spreadBtn { "Spread" };
    juce::TextButton dupZoneBtn { "Dup" };
    juce::ComboBox moveToGroupBox;
    juce::ComboBox mapZoomBox;
    juce::ComboBox velGridBox;
    juce::ComboBox zoneColourBox;

    // Editable Numeric Zone Info Bar
    juce::Label lkLbl, hkLbl, lvLbl, hvLbl, rkLbl;
    juce::TextEditor lkEdit, hkEdit, lvEdit, hvEdit, rkEdit;

    // Zoomable Mapping Viewport + List View
    juce::Viewport mapViewport;
    KeyMapperComponent editKeyMapper;
    float mapZoomFactor = 1.0f;

    struct ZoneListModel : public juce::ListBoxModel
    {
        ZoneListModel (NoiseMu6ixRhythmAudioProcessor& p, std::function<void(int)> onSel) : proc (p), onSelect (std::move (onSel)) {}
        int getNumRows() override { return static_cast<int> (proc.zones.size()); }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override { if (onSelect) onSelect (row); }
        NoiseMu6ixRhythmAudioProcessor& proc;
        std::function<void(int)> onSelect;
    };
    ZoneListModel zoneListModel;
    juce::ListBox zoneListBox;

    // ---------------- EDIT SUB-TAB 3: WAVE EDITOR ----------------
    juce::ComboBox trackSelectBox;
    juce::ToggleButton realRRBtn { "RR" };
    juce::ComboBox waveZoomBox;
    juce::ComboBox waveSnapBox;
    juce::Label waveInfoLabel;

    juce::Viewport waveViewport;
    WaveEditorComponent waveEditor;
    float waveZoomFactor = 1.0f;

    // Kontakt-style 3 Wave Sub-Tabs below waveform
    juce::TextButton waveSubLoopBtn { "Sample Loop" };
    juce::TextButton waveSubGridBtn { "Grid / Sync" };
    juce::TextButton waveSubToneBtn { "Edit / Tone" };

    // Wave Sub-Tab 1: Loop
    juce::ToggleButton sampleLoopBtn { "LOOP ON" };
    juce::ComboBox loopModeBox;
    LabeledKnob loopXfadeKnob { "X-Fade" }, loopTuneKnob { "L-Tune" };
    juce::Label loopCountLbl;
    juce::ComboBox loopCountBox;

    // Wave Sub-Tab 2: Grid / Sync
    juce::ToggleButton waveGridOnBtn { "Grid ON" };
    juce::ComboBox waveGridModeBox;
    juce::ComboBox waveGridWidthBox;
    juce::Label waveBpmLbl;
    juce::TextEditor waveBpmEdit;

    // Wave Sub-Tab 3: Edit / Tone
    juce::ToggleButton reverseBtn { "REV" };
    juce::TextButton normalizeBtn { "NORM" };
    LabeledKnob fadeInKnob { "Fade In" }, fadeOutKnob { "Fade Out" };
    LabeledKnob startKnob { "Start" }, endKnob { "End" }, loopStartKnob { "L-Start" }, loopEndKnob { "L-End" };
    LabeledKnob pitchKnob { "Pitch" }, panKnob { "Pan" }, volKnob { "Vol" };
    LabeledKnob attackKnob { "Atk" }, decayKnob { "Dec" }, sustainKnob { "Sus" }, releaseKnob { "Rel" };
    LabeledKnob cutoffKnob { "Cut" }, resKnob { "Res" };

    // Bottom 10-Octave Piano Strip
    ColorPianoStrip bottomPiano;

    // Global APVTS Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> frrAtt, fvlAtt, pitchTrackAtt, seqLoopAtt, hostSyncAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shuffleAtt, timeRandAtt, velRandAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> gridAtt;

    std::unique_ptr<juce::FileChooser> fileChooser;

    void setActiveTab (ActiveTab tab);
    void setEditSubTab (EditSubTab sub);
    void setWaveSubTab (WaveSubTab sub);
    void refreshMoveToGroupBox();
    void applyNumericZoneEdits();
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
