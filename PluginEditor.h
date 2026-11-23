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

// 10-Octave Compact Color-Coded Piano Strip (Red Keyswitch Keys)
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

// Kontakt-Style Group Editor Panel: Half-Width Compact Tiles (5x5 Grid, Row 6+ Scrolls) + Grp Vol/Pan + RR + Keyswitch
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

    juce::Viewport gridViewport;
    juce::Component gridContent;
    juce::OwnedArray<GroupTile> groupTiles;

    juce::Label startBarLabel;
    juce::Label nameFieldLabel;
    juce::TextEditor groupNameEdit;
    juce::Label rrSetLabel;
    juce::ComboBox rrSetBox;
    juce::Label ksLabel;
    juce::ComboBox keyswitchBox;
    LabeledKnob grpVolKnob { "Grp Vol" }, grpPanKnob { "Grp Pan" };

    void syncStartBar();
};

// 10-Octave Key Mapper (NO Right-Click Delete; Batch Multi-Select; Click-to-Audition; Auto-Layer)
class KeyMapperComponent : public juce::Component
{
public:
    KeyMapperComponent (NoiseMu6ixRhythmAudioProcessor& p);
    void setReadOnly (bool ro) { readOnly = ro; repaint(); }
    void setVelocityGridDivisions (int divs) { velGridDivs = divs; repaint(); }
    void setSelectedGroupsOnly (bool sgo) { selectedGroupsOnly = sgo; repaint(); }
    void setAuditionOnClick (bool aud) { auditionOnClick = aud; }
    bool isReadOnly() const { return readOnly; }

    void selectAllZones();
    void deleteSelectedZones();
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
    bool auditionOnClick = false;
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

// Zoomable Kontakt-Style Sample Wave Editor with Live Playhead, Grid & Snap
class WaveEditorComponent : public juce::Component
{
public:
    WaveEditorComponent (NoiseMu6ixRhythmAudioProcessor& p);
    void setSnapMode (int mode) { snapMode = mode; repaint(); }
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

// Live EQ 3 Curve Graph (100Hz - 10kHz, +-18dB) matching Kontakt EQ 3
class EqCurveDisplay : public juce::Component
{
public:
    EqCurveDisplay (NoiseMu6ixRhythmAudioProcessor& p);
    void setTarget (int rackType, int slotIdx) { targetRack = rackType; targetSlot = slotIdx; repaint(); }
    void paint (juce::Graphics& g) override;

private:
    NoiseMu6ixRhythmAudioProcessor& processor;
    int targetRack = 0; // 0=Group Insert, 1=Send FX, 2=Main FX
    int targetSlot = 0;
};

// Kontakt-Style Effects Panel (Amplifier + 8 Group Insert + 8 Send FX + 8 Main FX + EQ 3 Curve)
class KontaktEffectsPanel : public juce::Component
{
public:
    KontaktEffectsPanel (NoiseMu6ixRhythmAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void refreshFromProcessor();

private:
    NoiseMu6ixRhythmAudioProcessor& processor;

    // 0 = Group Insert FX, 1 = Send FX, 2 = Main FX
    int activeRack = 0;
    int activeSlot = 0;

    // Amplifier & Routing Strip
    juce::Label ampTitleLbl;
    LabeledKnob ampVolKnob { "Volume" }, ampPanKnob { "Pan" }, ampSendKnob { "Send" };
    juce::ToggleButton phaseInvBtn { "Phase Invert" };
    juce::ToggleButton lrSwapBtn { "L/R Swap" };
    juce::Label outLbl;
    juce::ComboBox outBusBox;

    // 3 Racks x 8 Slot Boxes (with B bypass & X remove)
    struct SlotBox : public juce::Component
    {
        SlotBox (int rIdx, int sIdx, std::function<void(int, int)> onSel,
                 std::function<void(int, int)> onByp, std::function<void(int, int)> onRem,
                 std::function<void(int, int)> onChoose);
        void paint (juce::Graphics& g) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent& e) override;

        int rackIdx = 0, slotIdx = 0;
        juce::String fxName { "Add FX +" };
        bool isEmpty = true;
        bool isBypassed = false;
        bool isSelected = false;

        juce::TextButton bypBtn { "B" };
        juce::TextButton remBtn { "X" };
        std::function<void(int, int)> onSelect, onBypass, onRemove, onPickMenu;
    };

    juce::Label insertRackLbl, sendRackLbl, mainRackLbl;
    std::array<std::unique_ptr<SlotBox>, 8> insertSlots;
    std::array<std::unique_ptr<SlotBox>, 8> sendSlots;
    std::array<std::unique_ptr<SlotBox>, 8> mainSlots;

    // Inline Effect Editor Area
    juce::Label editorHeaderLbl;

    // EQ 3 Controls (9 knobs + Live Graph)
    LabeledKnob eqF1 { "Freq 1" }, eqF2 { "Freq 2" }, eqF3 { "Freq 3" };
    LabeledKnob eqB1 { "BW 1" },   eqB2 { "BW 2" },   eqB3 { "BW 3" };
    LabeledKnob eqG1 { "Gain 1" }, eqG2 { "Gain 2" }, eqG3 { "Gain 3" };
    EqCurveDisplay eqGraph;

    // Generic 4-knob Controls for Compressor, Saturation, Filter, Transient, Delay, Reverb, Chorus, Width, Limiter
    LabeledKnob genP1 { "P1" }, genP2 { "P2" }, genP3 { "P3" }, genP4 { "P4" };

    InsertEffect* getTargetFx();
    void selectSlot (int rackIdx, int slotIdx);
    void showAddFxMenu (int rackIdx, int slotIdx);
};

class MainPluginView : public juce::Component,
                       public juce::ListBoxModel
{
public:
    enum class ActiveTab { Main = 0, Edit };
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
    void deleteSelectedInEditTab();
    void repaintDynamicAreas();

private:
    NoiseMu6ixRhythmAudioProcessor& audioProcessor;
    std::function<void(float)> scaleCallback;
    ActiveTab activeTab = ActiveTab::Main;
    WaveSubTab waveSubTab = WaveSubTab::Loop;

    juce::TextButton titleScaleBtn { "NOISE MU6IX RHYTHM" };

    // Top-level Tabs: MAIN and EDIT
    juce::TextButton tabMainBtn { "MAIN" };
    juce::TextButton tabEditBtn { "EDIT" };

    // Stackable Toggle Buttons in EDIT tab: Group Editor | Mapping Editor | Wave Editor | Effects
    juce::ToggleButton togGroupBtn { "Group Editor" };
    juce::ToggleButton togMapBtn   { "Mapping Editor" };
    juce::ToggleButton togWaveBtn  { "Wave Editor" };
    juce::ToggleButton togFxBtn    { "Effects" };

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

    // ---------------- SCROLLABLE STACKABLE EDIT PAGE ----------------
    juce::Viewport editViewport;
    juce::Component editContent;

    GroupEditorPanel groupPanel;

    // Mapping Editor Section inside editContent
    juce::Label mapHeaderLbl;
    juce::ToggleButton frrBtn { "FRR" };
    juce::ToggleButton fvlBtn { "FVL" };
    juce::ToggleButton pitchTrackBtn { "PITCH" };
    juce::TextButton selectAllZonesBtn { "ALL" };
    juce::TextButton delZoneBtn { "Del" };
    juce::ToggleButton zoneSoloBtn { "Zone Solo" };
    juce::ToggleButton selGroupsOnlyBtn { "Sel Grp Only" };
    juce::ToggleButton auditionBtn { "Audition" };
    juce::ToggleButton listViewBtn { "List View" };
    juce::TextButton autoGridBtn { "Auto Grid" };
    juce::TextButton autoMapBtn { "Auto-Map" };
    juce::TextButton spreadBtn { "Spread" };
    juce::TextButton dupZoneBtn { "Dup" };
    juce::ComboBox moveToGroupBox;
    juce::ComboBox mapZoomBox;
    juce::ComboBox velGridBox;
    juce::ComboBox zoneColourBox;

    juce::Label lkLbl, hkLbl, lvLbl, hvLbl, vfLbl, rkLbl;
    juce::TextEditor lkEdit, hkEdit, lvEdit, hvEdit, vfEdit, rkEdit;

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

    // Wave Editor Section inside editContent
    juce::Label waveHeaderLbl;
    juce::TextButton wavePlayBtn { "Play" };
    juce::ComboBox trackSelectBox;
    juce::ToggleButton realRRBtn { "RR" };
    juce::ComboBox waveZoomBox;
    juce::ComboBox waveSnapBox;
    juce::Label waveInfoLabel;

    juce::Viewport waveViewport;
    WaveEditorComponent waveEditor;
    float waveZoomFactor = 1.0f;

    juce::TextButton waveSubLoopBtn { "Sample Loop" };
    juce::TextButton waveSubGridBtn { "Grid / Sync" };
    juce::TextButton waveSubToneBtn { "Edit / Tone" };

    juce::ToggleButton sampleLoopBtn { "LOOP ON" };
    juce::ComboBox loopModeBox;
    LabeledKnob loopXfadeKnob { "X-Fade" }, loopTuneKnob { "L-Tune" };
    juce::Label loopCountLbl;
    juce::ComboBox loopCountBox;

    juce::ToggleButton waveGridOnBtn { "Grid ON" };
    juce::ComboBox waveGridModeBox;
    juce::ComboBox waveGridWidthBox;
    juce::TextButton sliceKeysBtn { "Slice -> Keys" };
    juce::Label waveBpmLbl;
    juce::TextEditor waveBpmEdit;

    juce::ToggleButton reverseBtn { "REV" };
    juce::TextButton normalizeBtn { "NORM" };
    juce::TextButton trimBtn { "TRIM" };
    LabeledKnob fadeInKnob { "Fade In" }, fadeOutKnob { "Fade Out" };
    LabeledKnob startKnob { "Start" }, endKnob { "End" }, loopStartKnob { "L-Start" }, loopEndKnob { "L-End" };
    LabeledKnob pitchKnob { "Pitch" }, panKnob { "Pan" }, volKnob { "Vol" };
    LabeledKnob attackKnob { "Atk" }, decayKnob { "Dec" }, sustainKnob { "Sus" }, releaseKnob { "Rel" };
    LabeledKnob cutoffKnob { "Cut" }, resKnob { "Res" };

    // Kontakt Effects Rack Section inside editContent
    KontaktEffectsPanel effectsPanel;

    // Bottom 10-Octave Piano Strip
    ColorPianoStrip bottomPiano;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> frrAtt, fvlAtt, pitchTrackAtt, seqLoopAtt, hostSyncAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shuffleAtt, timeRandAtt, velRandAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> gridAtt;

    std::unique_ptr<juce::FileChooser> fileChooser;

    void setActiveTab (ActiveTab tab);
    void setWaveSubTab (WaveSubTab sub);
    void layoutEditStack (int viewportWidth);
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
