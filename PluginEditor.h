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

// Vertical Fader for Group Volume & Pan
class VerticalFaderControl : public juce::Component
{
public:
    VerticalFaderControl (const juce::String& labelText);
    void resized() override;
    juce::Slider slider;
    juce::Label label;
};

// Geometric N6 Logo + RYTHME Title Badge (Clickable for UI Scale Menu)
class N6BrandHeader : public juce::Component
{
public:
    N6BrandHeader (std::function<void()> onClickMenu);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }

private:
    std::function<void()> onClick;
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
    WaveEditorComponent (RythmeAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    std::function<void()> onMarkersChanged;

private:
    RythmeAudioProcessor& processor;
    enum class MarkerDrag { None, Start, End, LoopStart, LoopEnd };
    MarkerDrag activeDrag = MarkerDrag::None;
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

    // Native Instruments / Kontakt-Style Library Panel (below logo/title)
    juce::TextButton addLibBtn { "+ ADD LIBRARY" };
    juce::TextButton sortAzBtn { "A-Z" };
    juce::TextButton delLibBtn { "DEL" };
    juce::Label activeLibraryBoxLabel;
    juce::ListBox libraryList;
    juce::String currentLibraryFolderName { "NO LIBRARY LOADED" };
    juce::Array<juce::File> libraryFiles;

    // Top Bar: MAIN & EDIT buttons where old title was + Preset Name + SAVE / SAVE AS
    juce::TextButton tabMainBtn { "MAIN" };
    juce::TextButton tabEditBtn { "EDIT" };
    juce::TextEditor presetNameBox;
    juce::TextButton saveBtn { "SAVE" };
    juce::TextButton saveAsBtn { "SAVE AS" };

    // ---------------- MAIN TAB ----------------
    KeyMapperComponent mainKeyMapper;
    juce::Label rhythmSeqHeaderLbl;
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

    // ---------------- EDIT TAB (Scrollable Spacious Layout) ----------------
    juce::Viewport editViewport;
    juce::Component editContent;

    // Group Strip: + Grp, Del Grp, Group Select, Rename, M, KS (No Group RR!), Vertical Vol & Pan Faders
    juce::TextButton addGroupBtn { "+ Grp" };
    juce::TextButton delGroupBtn { "Del Grp" };
    juce::ComboBox groupSelectBox;
    juce::TextEditor groupNameEdit;
    juce::TextButton groupMuteBtn { "M" };
    juce::ComboBox groupKsBox;
    VerticalFaderControl grpVolFader { "Vol" }, grpPanFader { "Pan" };

    // Key Mapper Uniform Switch/Button Bar (No FVL!) + Tall Key Mapper
    juce::TextButton frrBtn { "FRR" };
    juce::TextButton pitchTrackBtn { "PITCH" };
    juce::TextButton selectAllZonesBtn { "ALL" };
    juce::TextButton delZoneBtn { "Del" };
    juce::TextButton autoGridBtn { "Auto Grid" };
    juce::ComboBox velGridBox;
    juce::ComboBox zoneColourBox;
    KeyMapperComponent editKeyMapper;

    // Wave Editor Uniform Switch/Button Bar (Sample Name Only, Play, RR, LOOP, Z-Cross, REV, NORM, Reset)
    juce::TextButton wavePlayBtn { "Play" };
    juce::ComboBox trackSelectBox;
    juce::TextButton realRRBtn { "RR" };
    juce::TextButton sampleLoopBtn { "LOOP" };
    juce::TextButton zeroCrossBtn { "Z-Cross" };
    juce::TextButton reverseBtn { "REV" };
    juce::TextButton normalizeBtn { "NORM" };
    juce::TextButton resetZoneBtn { "Reset" };
    WaveEditorComponent waveEditor;

    // 2 Spacious Rows of Large Zone Knobs
    LabeledKnob startKnob { "Start" }, endKnob { "End" }, loopStartKnob { "L-Start" }, loopEndKnob { "L-End" };
    LabeledKnob pitchKnob { "Pitch" }, panKnob { "Pan" }, volKnob { "Vol" };
    LabeledKnob attackKnob { "Atk" }, decayKnob { "Dec" }, sustainKnob { "Sus" }, releaseKnob { "Rel" };
    LabeledKnob cutoffKnob { "Cut" }, resKnob { "Res" };

    // Simple FX Strip (4 slots + Uniform ON button + 3 Large Knobs)
    juce::ComboBox fxSlotSelectBox;
    juce::ComboBox fxTypeBox;
    juce::TextButton fxEnableBtn { "ON" };
    LabeledKnob fxP1 { "P1" }, fxP2 { "P2" }, fxP3 { "P3" };

    // Bottom 10-Octave Piano Strip
    ColorPianoStrip bottomPiano;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> frrAtt, pitchTrackAtt, seqLoopAtt, hostSyncAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shuffleAtt, timeRandAtt, velRandAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> gridAtt;

    std::unique_ptr<juce::FileChooser> fileChooser;

    void styleUniformSwitchButton (juce::TextButton& b, juce::Colour activeCol);
    void setActiveTab (ActiveTab tab);
    void layoutEditContent (int viewportW);
    void refreshGroupBoxes();
    void syncGroupControls();
    void syncFxControls();
    void applyBpmText();
    void refreshLibraryFolder (const juce::File& folder);
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
