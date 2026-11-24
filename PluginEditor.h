#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>
#include <set>
#include <vector>
#include <map>

class HorizontalFader : public juce::Component
{
public:
    HorizontalFader (const juce::String& labelText, double defaultVal = 0.0);
    void resized() override;
    void applyFaderStyle (int styleId);
    juce::Slider slider;
    juce::Label label;
};

// Center-Zero Panning Fader (-50 .. 0 .. +50) with left-only / right-only fill & Double-Click 0 reset
class CenterZeroPanFader : public juce::Component
{
public:
    CenterZeroPanFader (const juce::String& labelText);
    void setValue (float v, bool notify = false);
    float getValue() const { return value; }
    void setAccentColour (juce::Colour c) { accentCol = c; repaint(); }
    std::function<void(float)> onValueChanged;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::String title;
    float value = 0.0f; // -50 .. +50
    juce::Colour accentCol { 0xff4ca3ff };
    void updateFromX (int mx);
};

// BPM-style Type-In + Vertical Mouse Drag + Double-Click Default Number Box
class DragTypeNumberBox : public juce::Component
{
public:
    DragTypeNumberBox (const juce::String& labelText, const juce::String& unitSuffix,
                       double minV, double maxV, double defaultV, int decimals = 1);
    void setValue (double v, bool notify = false);
    double getValue() const { return currentValue; }
    std::function<void(double)> onValueChanged;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::String title;
    juce::String suffix;
    double minVal = 0.0;
    double maxVal = 100.0;
    double defaultVal = 0.0;
    int numDecimals = 1;
    double currentValue = 0.0;
    double dragStartValue = 0.0;
    int dragStartY = 0;
    bool isEditingText = false;

    juce::TextEditor editor;
    void commitText();
    juce::String formatDisplay() const;
};

// ADSR Envelope Visualizer Graph (no Curve parameter)
class AdsrGraphComponent : public juce::Component
{
public:
    AdsrGraphComponent (RythmeAudioProcessor& p);
    void paint (juce::Graphics& g) override;

private:
    RythmeAudioProcessor& processor;
};

// FabFilter-style Interactive FX Curve & Signal Graph Display
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

// Customizable Brand Logo + Title Header (Logo shapes 1..4 + Logo Colour + Letters + Title)
class BrandLogoHeader : public juce::Component
{
public:
    BrandLogoHeader (RythmeAudioProcessor& p, std::function<void()> onClickMenu);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }

private:
    RythmeAudioProcessor& processor;
    std::function<void()> onClick;
};

class MountedSoundCard : public juce::Component
{
public:
    MountedSoundCard (std::function<void()> onCloseClicked);
    void setItem (const juce::String& name, bool active);
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::String itemName;
    bool active = false;
    juce::TextButton closeBtn { "X" };
    std::function<void()> onClose;
};

// Vintage Black & White Scrolling Digital Output Waveform Display on MAIN Page
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

// Inline Layout Customize Overlay (Move anywhere + Bottom-Right Corner Resize + Grid Free/10x..50x + DONE / Reset)
class LayoutCustomizeOverlay : public juce::Component
{
public:
    struct TargetEntry
    {
        juce::String name;
        juce::Component* comp = nullptr;
        juce::Rectangle<int> defaultBounds;
    };

    LayoutCustomizeOverlay (std::function<void()> onDoneClicked, std::function<void()> onResetClicked);
    void setTargets (std::vector<TargetEntry> items);
    void setGridStep (int px) { gridStepPx = px; repaint(); }
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    std::vector<TargetEntry> targets;
    int gridStepPx = 0;
    int activeIdx = -1;
    bool isResizing = false;
    juce::Point<int> dragStart;
    juce::Rectangle<int> startBounds;
    std::function<void()> onDone;
    std::function<void()> onReset;

    int snap (int v) const;
};

class MainPluginView : public juce::Component,
                       public juce::ListBoxModel
{
public:
    enum class ActiveTab { Main = 0, Edit };

    MainPluginView (RythmeAudioProcessor& p, std::function<void(float)> onScaleSelected);
    void paint (juce::Graphics& g) override;
    void resized() override;

    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

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
    bool brandSettingsVisible = false;
    std::map<juce::String, juce::Rectangle<int>> customBoundsMap;

    BrandLogoHeader brandHeader;

    juce::TextButton addLibBtn { "+ ADD LIBRARY" };
    juce::TextButton sortAzBtn { "A-Z" };
    juce::TextButton delLibBtn { "DEL" };

    struct VisibleLibRow
    {
        bool isCategory = false;
        int catIdx = -1;
        juce::String label;
        bool expanded = false;
        juce::File file;
    };
    std::vector<VisibleLibRow> visibleLibRows;
    juce::ListBox libraryList;

    // Top Bar: MAIN, EDIT + Preset Name + SAVE / SAVE AS + Top-Right Gear Settings
    juce::TextButton tabMainBtn { "MAIN" };
    juce::TextButton tabEditBtn { "EDIT" };
    juce::TextEditor presetNameBox;
    juce::TextButton saveBtn { "SAVE" };
    juce::TextButton saveAsBtn { "SAVE AS" };
    juce::TextButton topGearBtn { "SET" };

    // ---------------- MAIN TAB ----------------
    MountedSoundCard mountedCard;
    LiveMainWaveDisplay mainWaveDisplay;
    juce::Label rhythmSeqHeaderLbl;
    juce::TextButton seqBypassBtn { "Bypass" };
    RhythmSequencerComponent seqComp;
    juce::TextButton playPauseBtn { "PLAY" };
    juce::ToggleButton hostSyncBtn { "SYNC" };
    juce::Label bpmLabel;
    juce::TextEditor bpmInputBox;
    HorizontalFader shuffleFader { "Shuffle", 0.0 }, timeRandFader { "Time Rnd", 0.0 }, velRandFader { "Vel Rnd", 0.0 };
    juce::Label gridLabel;
    juce::ComboBox gridBox;
    juce::ToggleButton seqLoopBtn { "LOOP" };
    juce::TextButton seqClearBtn { "Clr" };

    // ---------------- EDIT TAB ----------------
    juce::Viewport editViewport;
    juce::Component editContent;

    juce::TextButton addGroupBtn { "+ Grp" };
    juce::TextButton delGroupBtn { "Del Grp" };
    juce::ComboBox groupSelectBox;
    juce::TextEditor groupNameEdit;
    juce::TextButton groupMuteBtn { "M" };
    juce::TextButton groupSoloBtn { "S" };
    juce::TextButton selGroupOnlyBtn { "Sel Grp" };
    juce::ComboBox groupKsBox;
    DragTypeNumberBox grpVolBox { "Group Vol", " dB", 0.0, 100.0, 80.0, 0 };
    CenterZeroPanFader grpPanFader { "Group Pan" };

    juce::TextButton frrBtn { "FRR" };
    juce::TextButton fvlBtn { "FVL" };
    juce::TextButton pitchTrackBtn { "PITCH" };
    juce::TextButton selectAllZonesBtn { "ALL" };
    juce::TextButton delZoneBtn { "Del" };
    juce::TextButton autoGridBtn { "Auto Grid" };
    juce::ComboBox velGridBox;
    juce::ComboBox zoneColourBox;
    KeyMapperComponent editKeyMapper;

    juce::TextButton wavePlayBtn { "Play" };
    juce::ComboBox trackSelectBox;
    juce::TextButton realRRBtn { "RR" };
    juce::TextButton sampleLoopBtn { "LOOP" };
    juce::TextButton autoLoopZeroBtn { "Auto Loop Z-Cross" };
    HorizontalFader crossfadeFader { "Crossfade", 0.04 };
    juce::TextButton reverseBtn { "REV" };
    juce::TextButton normalizeBtn { "NORM" };
    juce::TextButton resetZoneBtn { "Reset" };
    WaveEditorComponent waveEditor;

    // Sample Type-In/Drag + Center-Zero Pan + ADSR Graph
    DragTypeNumberBox pitchBox   { "Pitch", " st", -24.0, 24.0, 0.0, 1 };
    CenterZeroPanFader panFader  { "Pan" };
    DragTypeNumberBox volBox     { "Volume", " dB", 0.0, 100.0, 80.0, 0 };

    DragTypeNumberBox attackBox  { "Attack", " ms", 0.0, 5000.0, 0.10, 2 };
    DragTypeNumberBox decayBox   { "Decay", " ms", 1.0, 8000.0, 300.0, 1 };
    DragTypeNumberBox sustainBox { "Sustain", "%", 0.0, 100.0, 85.0, 0 };
    DragTypeNumberBox releaseBox { "Release", " ms", 1.0, 10000.0, 250.0, 1 };
    AdsrGraphComponent adsrGraph;

    // FabFilter-Style Effects (Curve/Signal Display + 5 Controls + ON/Bypass)
    juce::TextButton fxBypassBtn { "Bypass" };
    juce::ComboBox fxSlotSelectBox;
    juce::ComboBox fxTypeBox;
    juce::TextButton fxEnableBtn { "ON" };
    FabStyleFxDisplay fxDisplay;
    HorizontalFader fxP1 { "P1", 0.5 }, fxP2 { "P2", 0.5 }, fxP3 { "P3", 0.5 }, fxP4 { "P4", 0.5 }, fxP5 { "P5", 0.5 };

    // Top-Right Gear Layout & Global Styles Bar (inline when Gear clicked)
    juce::ComboBox gearGridBox;
    juce::ComboBox gearKnobStyleBox;
    juce::ComboBox gearFaderStyleBox;
    juce::ComboBox gearSwitchStyleBox;
    juce::TextButton gearResetBtn { "Reset" };
    juce::TextButton gearDoneBtn { "DONE" };
    LayoutCustomizeOverlay customizeOverlay;

    // Logo/Title Menu -> Settings Panel (all in one place side-by-side + Apply/OK)
    juce::ComboBox brandShapeBox;
    juce::ComboBox brandColourBox;
    juce::TextEditor brandLettersEdit;
    juce::TextEditor brandTitleEdit;
    juce::TextButton brandApplyBtn { "Apply / OK" };

    ColorPianoStrip bottomPiano;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> frrAtt, fvlAtt, pitchTrackAtt, seqLoopAtt, hostSyncAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shuffleAtt, timeRandAtt, velRandAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> gridAtt;

    std::unique_ptr<juce::FileChooser> fileChooser;

    void applyAllStyles();
    void styleUniformSwitchButton (juce::TextButton& b, juce::Colour activeCol);
    void setActiveTab (ActiveTab tab);
    void enterLayoutCustomizeMode (bool enable);
    void layoutEditContent (int viewportW);
    void rebuildLibraryRows();
    void loadLibraryItem (const juce::File& file);
    void refreshGroupBoxes();
    void syncGroupControls();
    void syncFxControls();
    void applyBpmText();
    void saveAsDialog();
    void showBrandAndScaleMenu();
};

class RythmeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   public juce::FileDragAndDropTarget,
                                   public juce::Timer
{
public:
    static constexpr int baseWidth = 980;
    static constexpr int baseHeight = 620;

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
    MainPluginView mainView;

    void applyUiScale (float newScale);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RythmeAudioProcessorEditor)
};
