#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>
#include <set>
#include <vector>
#include <map>

class StyledHorizontalFader : public juce::Component
{
public:
    StyledHorizontalFader (const juce::String& labelText, double minV, double maxV, double defaultV, int decimals = 2);
    void setValue (double v, bool notify = false);
    double getValue() const { return currentValue; }
    void setStyleAndColour (int id, juce::Colour col);
    void setLabelText (const juce::String& t) { title = t; repaint(); }
    std::function<void(double)> onValueChanged;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    juce::String title;
    double minVal = 0.0, maxVal = 1.0, defaultVal = 0.0;
    int numDecimals = 2;
    double currentValue = 0.0;
    int styleId = 1;
    juce::Colour accentCol { 0xfff2f4f8 };
    bool isEditingText = false;
    juce::TextEditor editor;

    void updateFromX (int mx);
    void openTypeEditor();
    void commitText();
};

class CenterZeroPanFader : public juce::Component
{
public:
    CenterZeroPanFader (const juce::String& labelText);
    void setValue (float v, bool notify = false);
    float getValue() const { return value; }
    void setStyleAndColour (int id, juce::Colour col);
    std::function<void(float)> onValueChanged;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    juce::String title;
    float value = 0.0f;
    int styleId = 1;
    juce::Colour accentCol { 0xfff2f4f8 };
    bool isEditingText = false;
    juce::TextEditor editor;

    void updateFromX (int mx);
    void openTypeEditor();
    void commitText();
};

class DragTypeNumberBox : public juce::Component
{
public:
    DragTypeNumberBox (const juce::String& labelText, const juce::String& unitSuffix,
                       double minV, double maxV, double defaultV, int decimals = 1, bool showTopLabel = true);
    void setValue (double v, bool notify = false);
    double getValue() const { return currentValue; }
    void setStyleAndColour (int id, juce::Colour col);
    std::function<void(double)> onValueChanged;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    juce::String title;
    juce::String suffix;
    double minVal = 0.0, maxVal = 100.0, defaultVal = 0.0;
    int numDecimals = 1;
    bool hasLabel = true;
    double currentValue = 0.0;
    double dragStartValue = 0.0;
    int dragStartY = 0;
    int styleId = 1;
    juce::Colour accentCol { 0xfff2f4f8 };
    bool isEditingText = false;

    juce::TextEditor editor;
    void openTypeEditor();
    void commitText();
    juce::String formatDisplay() const;
};

class GearIconButton : public juce::Button
{
public:
    GearIconButton();
    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

class EditModeIconButton : public juce::Button
{
public:
    EditModeIconButton();
    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

class StyledSwitchButton : public juce::TextButton
{
public:
    StyledSwitchButton (const juce::String& text, bool isToggleable = true);
    void setStyleAndColour (int id, juce::Colour activeC);
    void paintButton (juce::Graphics& g, bool over, bool down) override;

private:
    int styleId = 1;
    juce::Colour activeCol { 0xfff2f4f8 };
};

// Top-Left Title & Scale Selector ("KINETIK") with 50%, 75%, 100% (Default), 120%, 150% popup
class LeftKinetikScaleButton : public juce::Component
{
public:
    LeftKinetikScaleButton (RythmeAudioProcessor& p, std::function<void(float)> onScaleSelected);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    RythmeAudioProcessor& processor;
    std::function<void(float)> onScale;
};

// Integrated Grid Box ("Grid" label + value dropdown inside one unit, matching BPM box)
class UnifiedGridBox : public juce::Component
{
public:
    UnifiedGridBox();
    juce::ComboBox combo;
    void paint (juce::Graphics& g) override;
    void resized() override;
};

// Top mini bar above Wave: IN / OUT animated level meters + center-zero Master Volume knob (no text label)
class TopIoMeterAndMasterKnob : public juce::Component
{
public:
    TopIoMeterAndMasterKnob (RythmeAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    RythmeAudioProcessor& processor;
    int dragStartY = 0;
    float dragStartDb = 0.0f;
    bool isDraggingKnob = false;
    juce::Rectangle<float> getKnobBounds() const;
};

// Pitch Bend & Mod Wheel strip placed on the left side of the Piano Keyboard (like Serum / classic synths)
class PitchModWheelsComponent : public juce::Component
{
public:
    PitchModWheelsComponent (RythmeAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    RythmeAudioProcessor& processor;
    int activeWheel = 0; // 1 = Pitch (spring-return), 2 = Mod (latching)
    void updateWheelFromY (int y);
};

// Preset Dropdown (v) + Editable Dashed Name Box + Save Icon Button
class PresetBarWidget : public juce::Component
{
public:
    PresetBarWidget (RythmeAudioProcessor& p,
                     std::function<void()> onSaveClicked,
                     std::function<void(const juce::File&)> onLoadFile,
                     std::function<void()> onSetRootFolder,
                     std::function<void()> onBrowseLoadPreset);

    void refreshTextFromProcessor();
    juce::String getCurrentTypedText() const { return libNameEditor.getText().trim(); }
    void flashSavedFeedback (bool success);
    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    RythmeAudioProcessor& processor;
    std::function<void()> saveCb;
    std::function<void(const juce::File&)> loadFileCb;
    std::function<void()> setRootCb;
    std::function<void()> browseLoadCb;

    juce::TextButton dropdownBtn { "v" };
    juce::TextEditor libNameEditor;
    juce::TextButton saveIconBtn;

    void openCascadingPresetMenu();
    void buildFolderSubMenu (juce::PopupMenu& menu, const juce::File& dir, int& nextId, std::map<int, juce::File>& idMap);
};

class AdsrGraphComponent : public juce::Component
{
public:
    AdsrGraphComponent (RythmeAudioProcessor& p);
    void paint (juce::Graphics& g) override;

private:
    RythmeAudioProcessor& processor;
};

class FabStyleFxDisplay : public juce::Component
{
public:
    FabStyleFxDisplay (RythmeAudioProcessor& p);
    void setSlotIndex (int idx) { slotIdx = idx; repaint(); }
    void paint (juce::Graphics& g) override;

private:
    RythmeAudioProcessor& processor;
    int slotIdx = 0;
};

class LiveMainWaveDisplay : public juce::Component
{
public:
    LiveMainWaveDisplay (RythmeAudioProcessor& p);
    void paint (juce::Graphics& g) override;

private:
    RythmeAudioProcessor& processor;
};

class ColorPianoStrip : public juce::Component
{
public:
    ColorPianoStrip (RythmeAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    RythmeAudioProcessor& processor;
    static constexpr int startNote = 0;
    static constexpr int numKeys = 121;
    int pressedNote = -1;
};

class KeyMapperComponent : public juce::Component
{
public:
    KeyMapperComponent (RythmeAudioProcessor& p);
    void setReadOnly (bool ro) { readOnly = ro; repaint(); }
    void setVelocityGridDivisions (int divs) { velGridDivs = divs; repaint(); }
    void setFilterToSelectedGroup (bool f) { filterToSelectedGroup = f; repaint(); }
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
    RythmeAudioProcessor& processor;
    static constexpr int startNote = 0;
    static constexpr int numKeys = 121;

    bool readOnly = true;
    bool filterToSelectedGroup = false;
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

    bool isZoneVisibleInMapper (const SampleZone& z) const;
    juce::Rectangle<int> getGridArea() const;
    juce::Rectangle<int> getKeyboardArea() const;
    juce::Rectangle<float> getZoneRect (const SampleZone& z, juce::Rectangle<int> gridArea) const;
    int snapVelocity (int rawVel) const;
    void sanitizeSelection();
};

class WaveEditorComponent : public juce::Component
{
public:
    WaveEditorComponent (RythmeAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    RythmeAudioProcessor& processor;
    enum class DragTarget { None, Start, End, LoopStart, LoopEnd, FadeIn, FadeOut };
    DragTarget activeDrag = DragTarget::None;
};

class RhythmSequencerComponent : public juce::Component
{
public:
    RhythmSequencerComponent (RythmeAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    RythmeAudioProcessor& processor;
    int draggedEventIdx = -1;
    float snapToGrid (float normX) const;
};

class LayoutCustomizeOverlay : public juce::Component
{
public:
    struct TargetEntry
    {
        juce::String key;
        juce::Component* comp = nullptr;
    };

    LayoutCustomizeOverlay (std::function<void(const juce::String&)> onDeleteKey,
                            std::function<void()> onBoundsChangedCb = nullptr);
    void setTargets (std::vector<TargetEntry> items);
    void setGridStep (int px) { gridStepPx = px; repaint(); }
    void deleteSelectedOrMarked();
    void clearPendingDeletes();
    const std::set<juce::String>& getPendingDeleteKeys() const { return pendingDeleteKeys; }

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    std::vector<TargetEntry> targets;
    int gridStepPx = 20;
    int activeIdx = -1;
    bool isResizing = false;
    juce::Point<int> dragStart;
    juce::Rectangle<int> startLocalBounds;
    std::set<juce::String> pendingDeleteKeys;
    std::function<void(const juce::String&)> onDeleteCallback;
    std::function<void()> onBoundsChangedCallback;

    int snap (int v) const;
};

class MainPluginView : public juce::Component
{
public:
    enum class ActiveTab { Main = 0, Edit };

    MainPluginView (RythmeAudioProcessor& p, std::function<void(float)> onScaleSelected);
    ~MainPluginView() override;
    void paint (juce::Graphics& g) override;
    void resized() override;

    void handleFilesDropped (const juce::StringArray& files, int localX, int localY);
    void syncPerZoneControls();
    void togglePlayPause();
    void selectAllInEditTab();
    void deleteSelectedInEditTab();
    void repaintDynamicAreas();

private:
    RythmeAudioProcessor& audioProcessor;
    std::function<void(float)> scaleCallback;
    ActiveTab activeTab = ActiveTab::Main;
    bool layoutEditMode = false;
    juce::Rectangle<int> gearToolbarBounds;
    uint32_t lastSeenStateRevision = 0;

    // Custom bounds & hidden keys are stored persistently on audioProcessor

    LeftKinetikScaleButton leftScaleBtn;
    PresetBarWidget presetBar;
    EditModeIconButton editModeBtn;
    GearIconButton topGearBtn;

    // ---------------- MAIN TAB ----------------
    TopIoMeterAndMasterKnob topIoMasterBar;
    LiveMainWaveDisplay mainWaveDisplay;
    juce::Label rhythmSeqHeaderLbl;
    RhythmSequencerComponent seqComp;
    StyledSwitchButton playPauseBtn { "PLAY", false };
    StyledSwitchButton hostSyncBtn { "SYNC", true };
    DragTypeNumberBox bpmBox { "BPM", "", 40.0, 240.0, 120.0, 1, true };
    StyledHorizontalFader shuffleFader { "Shuffle", 0.0, 1.0, 0.0, 2 };
    StyledHorizontalFader timeRandFader { "Time Rnd", 0.0, 1.0, 0.0, 2 };
    StyledHorizontalFader velRandFader { "Vel Rnd", 0.0, 1.0, 0.0, 2 };
    UnifiedGridBox gridUnit;
    StyledSwitchButton seqLoopBtn { "LOOP", true };
    StyledSwitchButton seqClearBtn { "Clr", false };

    // ---------------- EDIT TAB ----------------
    juce::Viewport editViewport;
    juce::Component editContent;

    StyledSwitchButton addGroupBtn { "+ Grp", false };
    StyledSwitchButton delGroupBtn { "Del Grp", false };
    juce::ComboBox groupSelectBox;
    juce::TextEditor groupNameEdit;
    StyledSwitchButton groupMuteBtn { "M", true };
    StyledSwitchButton groupSoloBtn { "S", true };
    StyledSwitchButton selGroupOnlyBtn { "Sel Grp", true };
    juce::ComboBox groupKsBox;
    StyledHorizontalFader grpVolFader { "Group Vol", 0.0, 100.0, 80.0, 0 };
    CenterZeroPanFader grpPanFader { "Group Pan" };

    StyledSwitchButton frrBtn { "FRR", true };
    StyledSwitchButton fvlBtn { "FVL", true };
    StyledSwitchButton pitchTrackBtn { "PITCH", true };
    StyledSwitchButton selectAllZonesBtn { "ALL", false };
    StyledSwitchButton delZoneBtn { "Del", false };
    StyledSwitchButton autoGridBtn { "Auto Grid", false };
    juce::ComboBox velGridBox;
    juce::ComboBox zoneColourBox;
    KeyMapperComponent editKeyMapper;

    StyledSwitchButton wavePlayBtn { "Play", false };
    juce::ComboBox trackSelectBox;
    StyledSwitchButton realRRBtn { "RR", true };
    StyledSwitchButton sampleLoopBtn { "LOOP", true };
    StyledSwitchButton autoLoopZeroBtn { "Auto Loop Z-Cross", true };
    StyledHorizontalFader crossfadeFader { "Crossfade", 0.0, 0.35, 0.04, 2 };
    StyledSwitchButton reverseBtn { "REV", true };
    StyledSwitchButton normalizeBtn { "NORM", false };
    StyledSwitchButton resetZoneBtn { "Reset", false };
    WaveEditorComponent waveEditor;

    DragTypeNumberBox pitchBox   { "Pitch", " st", -24.0, 24.0, 0.0, 1 };
    CenterZeroPanFader panFader  { "Pan" };
    DragTypeNumberBox volBox     { "Volume", " dB", 0.0, 100.0, 80.0, 0 };

    DragTypeNumberBox attackBox  { "Attack", " ms", 0.0, 5000.0, 0.10, 2 };
    DragTypeNumberBox decayBox   { "Decay", " ms", 1.0, 8000.0, 300.0, 1 };
    DragTypeNumberBox sustainBox { "Sustain", "%", 0.0, 100.0, 85.0, 0 };
    DragTypeNumberBox releaseBox { "Release", " ms", 1.0, 10000.0, 250.0, 1 };
    AdsrGraphComponent adsrGraph;

    StyledSwitchButton fxBypassBtn { "Bypass", true };
    juce::ComboBox fxSlotSelectBox;
    juce::ComboBox fxTypeBox;
    StyledSwitchButton fxEnableBtn { "ON", true };
    FabStyleFxDisplay fxDisplay;
    StyledHorizontalFader fxP1 { "P1", 0.0, 1.0, 0.5, 2 };
    StyledHorizontalFader fxP2 { "P2", 0.0, 1.0, 0.5, 2 };
    StyledHorizontalFader fxP3 { "P3", 0.0, 1.0, 0.5, 2 };
    StyledHorizontalFader fxP4 { "P4", 0.0, 1.0, 0.5, 2 };
    StyledHorizontalFader fxP5 { "P5", 0.0, 1.0, 0.5, 2 };

    juce::ComboBox gearGridBox;
    juce::ComboBox gearFaderStyleBox;
    juce::ComboBox gearFaderColBox;
    juce::ComboBox gearKnobStyleBox;
    juce::ComboBox gearKnobColBox;
    juce::ComboBox gearSwitchStyleBox;
    juce::ComboBox gearSwitchColBox;
    StyledSwitchButton gearDeleteBtn { "Delete", false };
    StyledSwitchButton gearResetBtn { "Reset", false };
    StyledSwitchButton gearDoneBtn { "DONE", false };
    LayoutCustomizeOverlay customizeOverlay;

    PitchModWheelsComponent pitchModWheels;
    ColorPianoStrip bottomPiano;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> frrAtt, fvlAtt, pitchTrackAtt, seqLoopAtt, hostSyncAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> gridAtt;

    std::unique_ptr<juce::FileChooser> fileChooser;

    void applyAllStyles();
    void setActiveTab (ActiveTab tab);
    void enterLayoutCustomizeMode (bool enable);
    void saveCustomizedBoundsFromOverlay();
    void layoutEditContent (int viewportW);
    void loadLibraryItem (const juce::File& file);
    void refreshGroupBoxes();
    void syncGroupControls();
    void syncFxControls();
    void syncAllUiFromProcessor();
    void savePresetAction();
    void browseLoadPresetAction();
    void chooseRootLibraryFolder();
};

class RythmeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   public juce::FileDragAndDropTarget,
                                   public juce::Timer
{
public:
    static constexpr int baseWidth = 780;
    static constexpr int baseHeight = 636;

    RythmeAudioProcessorEditor (RythmeAudioProcessor&);
    ~RythmeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    bool keyPressed (const juce::KeyPress& key) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    RythmeAudioProcessor& audioProcessor;
    float currentScale = 1.0f;
    bool isUpdatingScale = false;
    MainPluginView mainView;

    void applyUiScale (float newScale);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RythmeAudioProcessorEditor)
};
