#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>
#include <set>
#include <vector>

// Horizontal Fader Slider with Label
class HorizontalFader : public juce::Component
{
public:
    HorizontalFader (const juce::String& labelText);
    void resized() override;
    juce::Slider slider;
    juce::Label label;
};

// Modern Geometric N6 Logo + RYTHME Title Badge
class N6BrandHeader : public juce::Component
{
public:
    N6BrandHeader (std::function<void()> onClickMenu);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }

private:
    std::function<void()> onClick;
};

// Expandable / Collapsible Category Header Row (e.g., DRUMS -> click to expand/collapse items below)
class CategoryHeaderRow : public juce::Component
{
public:
    CategoryHeaderRow (const juce::String& titleText, bool isExpanded, std::function<void()> onToggle);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override { if (onToggle) onToggle(); }

private:
    juce::String title;
    bool expanded = false;
    std::function<void()> onToggle;
};

// Mounted Sound Item Card on MAIN page with Close 'X' button (clears back to empty state)
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

// Wave Editor with Mouse Drag for Start, End, Loop Start, Loop End + Fade In/Out handles on Start & End
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

    // Top-Left N6 Logo + RYTHME Brand Header
    N6BrandHeader brandHeader;

    // Persistent Library Panel: + ADD LIBRARY / A-Z / DEL + Expandable Category Boxes
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

    // Top Bar: MAIN & EDIT buttons + Preset Name + SAVE / SAVE AS
    juce::TextButton tabMainBtn { "MAIN" };
    juce::TextButton tabEditBtn { "EDIT" };
    juce::TextEditor presetNameBox;
    juce::TextButton saveBtn { "SAVE" };
    juce::TextButton saveAsBtn { "SAVE AS" };

    // ---------------- MAIN TAB ----------------
    MountedSoundCard mountedCard;
    KeyMapperComponent mainKeyMapper;
    juce::Label rhythmSeqHeaderLbl;
    RhythmSequencerComponent seqComp;
    juce::TextButton playPauseBtn { "PLAY" };
    juce::ToggleButton hostSyncBtn { "SYNC" };
    juce::Label bpmLabel;
    juce::TextEditor bpmInputBox;
    HorizontalFader shuffleFader { "Shuffle" }, timeRandFader { "Time Rnd" }, velRandFader { "Vel Rnd" };
    juce::Label gridLabel;
    juce::ComboBox gridBox;
    juce::ToggleButton seqLoopBtn { "LOOP" };
    juce::TextButton seqClearBtn { "Clr" };

    // ---------------- EDIT TAB (Scrollable Spacious Layout with Horizontal Faders) ----------------
    juce::Viewport editViewport;
    juce::Component editContent;

    // Group Strip: + Grp, Del Grp, Group Select, Rename, M, S, Sel Grp, KS + Horizontal Vol & Pan Faders
    juce::TextButton addGroupBtn { "+ Grp" };
    juce::TextButton delGroupBtn { "Del Grp" };
    juce::ComboBox groupSelectBox;
    juce::TextEditor groupNameEdit;
    juce::TextButton groupMuteBtn { "M" };
    juce::TextButton groupSoloBtn { "S" };
    juce::TextButton selGroupOnlyBtn { "Sel Grp" };
    juce::ComboBox groupKsBox;
    HorizontalFader grpVolFader { "Group Vol" }, grpPanFader { "Group Pan" };

    // Key Mapper Uniform Switch/Button Bar (No FVL!) + Tall Key Mapper
    juce::TextButton frrBtn { "FRR" };
    juce::TextButton pitchTrackBtn { "PITCH" };
    juce::TextButton selectAllZonesBtn { "ALL" };
    juce::TextButton delZoneBtn { "Del" };
    juce::TextButton autoGridBtn { "Auto Grid" };
    juce::ComboBox velGridBox;
    juce::ComboBox zoneColourBox;
    KeyMapperComponent editKeyMapper;

    // Wave Editor Uniform Switch/Button Bar (Sample Name Only, Play, RR, LOOP, Auto Loop Z-Cross, REV, NORM, Reset)
    juce::TextButton wavePlayBtn { "Play" };
    juce::ComboBox trackSelectBox;
    juce::TextButton realRRBtn { "RR" };
    juce::TextButton sampleLoopBtn { "LOOP" };
    juce::TextButton autoLoopZeroBtn { "Auto Loop Z-Cross" };
    HorizontalFader loopXfadeFader { "Crossfade" };
    juce::TextButton reverseBtn { "REV" };
    juce::TextButton normalizeBtn { "NORM" };
    juce::TextButton resetZoneBtn { "Reset" };
    WaveEditorComponent waveEditor;

    // Sample Horizontal Faders (Pitch, Pan, Vol, Atk, Dec, Sus, Rel, Cut, Res)
    HorizontalFader pitchFader { "Pitch" }, panFader { "Pan" }, volFader { "Vol" };
    HorizontalFader attackFader { "Atk" }, decayFader { "Dec" }, sustainFader { "Sus" }, releaseFader { "Rel" };
    HorizontalFader cutoffFader { "Cut" }, resFader { "Res" };

    // Effects Section below Divider Line + Horizontal FX Faders
    juce::ComboBox fxSlotSelectBox;
    juce::ComboBox fxTypeBox;
    juce::TextButton fxEnableBtn { "ON" };
    HorizontalFader fxP1 { "P1" }, fxP2 { "P2" }, fxP3 { "P3" };

    // Bottom 10-Octave Piano Strip
    ColorPianoStrip bottomPiano;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> frrAtt, pitchTrackAtt, seqLoopAtt, hostSyncAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shuffleAtt, timeRandAtt, velRandAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> gridAtt;

    std::unique_ptr<juce::FileChooser> fileChooser;

    void styleUniformSwitchButton (juce::TextButton& b, juce::Colour activeCol);
    void setActiveTab (ActiveTab tab);
    void layoutEditContent (int viewportW);
    void rebuildLibraryRows();
    void loadLibraryItem (const juce::File& file);
    void refreshGroupBoxes();
    void syncGroupControls();
    void syncFxControls();
    void applyBpmText();
    void saveAsDialog();
    void showScaleMenu();
};

class RythmeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   public juce::FileDragAndDropTarget,
                                   public juce::Timer
{
public:
    static constexpr int baseWidth = 960;
    static constexpr int baseHeight = 600;

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
