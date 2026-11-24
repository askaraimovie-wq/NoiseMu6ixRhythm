#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <map>
#include <set>

#ifndef KINETIK_AUTO_VERSION_STR
#define KINETIK_AUTO_VERSION_STR "v1.3.5"
#endif

struct KinetikThemePalette
{
    const char* name;
    juce::Colour bg;
    juce::Colour panel;
    juce::Colour surface;
    juce::Colour grid;
    juce::Colour border;
    juce::Colour text;
    juce::Colour mutedText;
    juce::Colour accent;
    bool isLight;
};

struct FastSVF
{
    float ic1eq = 0.0f;
    float ic2eq = 0.0f;
    float g = 0.5f;
    float k = 1.414f;
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

    void reset() { ic1eq = 0.0f; ic2eq = 0.0f; }

    void setParams (float cutoffHz, float q, double sampleRate)
    {
        float fc = juce::jlimit (22.0f, static_cast<float> (sampleRate * 0.45), cutoffHz);
        g = std::tan (juce::MathConstants<float>::pi * fc / static_cast<float> (sampleRate));
        k = 1.0f / juce::jlimit (0.35f, 8.0f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    inline float processLP (float v0)
    {
        float v3 = v0 - ic2eq;
        float v1 = a1 * ic1eq + a2 * v3;
        float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;
        return v2;
    }

    inline float processHP (float v0)
    {
        float v3 = v0 - ic2eq;
        float v1 = a1 * ic1eq + a2 * v3;
        float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;
        return v0 - k * v1 - v2;
    }

    inline float processBP (float v0)
    {
        float v3 = v0 - ic2eq;
        float v1 = a1 * ic1eq + a2 * v3;
        float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;
        return v1;
    }
};

enum class SimpleFxType
{
    Off = 0,
    EQ,
    Compressor,
    Saturation,
    Transient,
    Width,
    Delay,
    Reverb
};

struct SimpleFxSlot
{
    SimpleFxType type = SimpleFxType::Off;
    bool enabled = false;
    float p1 = 0.5f;
    float p2 = 0.5f;
    float p3 = 0.5f;
    float p4 = 0.5f;
    float p5 = 0.5f;

    FastSVF hpL, hpR, lpL, lpR, midL, midR;
    float env = 0.0f;
    float envSlow = 0.0f;
    float lastGrDb = 0.0f;
    float lastInputLevel = 0.0f;
    float chorusPhase = 0.0f;

    std::vector<float> bufL, bufR;
    int writePos = 0;

    std::array<std::vector<float>, 4> combL, combR;
    std::array<int, 4> combPosL {}, combPosR {};
    std::array<float, 4> combDampL {}, combDampR {};
    std::array<std::vector<float>, 2> apL, apR;
    std::array<int, 2> apPosL {}, apPosR {};

    static constexpr int grHistSize = 96;
    std::array<float, grHistSize> grHistory {};
    int grWriteIdx = 0;

    void setDefaultsForType (SimpleFxType t);
    void prepare (double sr);
    void processStereo (float* L, float* R, int numSamples, double sr);
    static juce::String getTypeName (SimpleFxType t);
    static juce::String getShortCardName (SimpleFxType t);
    static juce::String getParamLabel (SimpleFxType t, int idx);
};

struct RhythmEvent
{
    float position = 0.0f;
    float velocity = 0.85f;
};

struct SampleLayer
{
    juce::String name;
    juce::File file;
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
};

struct SampleGroup
{
    static constexpr int numFxSlots = 7;

    juce::String name { "Group 1" };
    bool muted = false;
    bool soloed = false;
    int keyswitchNote = -1;
    float volumeDb100 = 80.0f;
    float pan50 = 0.0f;
    int silentSamples = 999999;
    std::array<SimpleFxSlot, numFxSlots> fxSlots;

    SampleGroup();
};

struct SampleZone
{
    juce::String name { "Sample" };
    int groupIndex = 0;
    int lowKey = 60;
    int highKey = 60;
    int rootKey = 60;
    int lowVel = 0;
    int highVel = 127;
    juce::Colour colour { 0xff1b4fd8 };
    int colourIndex = 0;

    float startNorm = 0.0f;
    float endNorm = 1.0f;
    float loopStartNorm = 0.15f;
    float loopEndNorm = 0.85f;
    bool sampleLoop = false;
    bool autoLoopZeroCross = true;
    float loopCrossfade = 0.08f;
    float fadeInNorm = 0.0f;
    float fadeOutNorm = 0.0f;
    bool reverse = false;
    bool oneShot = false;
    int chokeGroup = 0;

    float pitchSemi = 0.0f;
    float pan50 = 0.0f;
    float volumeDb100 = 80.0f;
    float attackMs = 0.10f;
    float decayMs = 300.0f;
    float sustainPct = 85.0f;
    float releaseMs = 250.0f;
    float cutoffHz = 20000.0f;
    float resQ = 0.707f;

    std::vector<SampleLayer> layers;
    int selectedLayerIndex = 0;
    int rrPlayIndex = 0;
    int lastPlayedLayerIdx = -1;
    bool realRREnabled = true;

    std::vector<RhythmEvent> seqEvents;
};

struct ActiveVoice
{
    bool active = false;
    bool isPreview = false;
    int zoneIndex = -1;
    int layerIndex = 0;
    int midiNote = 60;
    int chokeGroup = 0;
    bool oneShot = false;
    uint64_t ageOrder = 0;

    double currentPos = 0.0;
    double baseSampleRateRatio = 1.0;
    double currentSemi = 0.0;
    double startSemi = 0.0;
    double targetSemi = 0.0;
    int glideTotalSamples = 0;
    int glideElapsedSamples = 0;

    int legatoDipTotal = 0;
    int legatoDipElapsed = 0;

    bool playingReverse = false;
    float gainL = 1.0f;
    float gainR = 1.0f;
    float velocity = 1.0f;
    float envVal = 0.0f;
    int envStage = 0;
    float attackInc = 0.01f;
    float decayCoeff = 0.999f;
    float sustainLvl = 0.85f;
    float releaseCoeff = 0.999f;
    float fastKillDec = 0.006f;

    float baseCutoffHz = 20000.0f;
    float baseResQ = 0.707f;
    bool useFilter = false;
    FastSVF filterL;
    FastSVF filterR;
};

struct LibraryCategoryEntry
{
    juce::String name;
    juce::File folder;
    bool expanded = false;
    std::vector<juce::File> items;
};

class RythmeAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numPaletteColours = 12;
    static juce::Colour getPaletteColour (int idx);
    static juce::String getPaletteColourName (int idx);

    static constexpr int numUiThemes = 8;
    static KinetikThemePalette getThemePalette (int themeIdx);

    RythmeAudioProcessor();
    ~RythmeAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "KINETIK " + getBuildVersionString(); }
    static juce::String getBuildVersionString() { return juce::String (KINETIK_AUTO_VERSION_STR); }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    int createEmptyGroup();
    bool deleteSelectedGroup();
    bool isKeyswitchNote (int midiNote) const;

    int createEmptyZone (int key);
    bool addSampleToZone (int zoneIndex, const juce::File& file);
    bool loadSampleIntoNewZone (const juce::File& file, int lowKey, int highKey, int rootKey, int lowVel = 0, int highVel = 127);
    bool deleteLayerFromZone (int zoneIndex, int layerIndex);
    void deleteZone (int index);
    void deleteZones (std::vector<int> indices);
    void normalizeSelectedSample();
    void resetSelectedZoneDefaults();
    void clearSelectedZoneRhythm();
    float snapNormToZeroCrossing (int zoneIndex, float rawNorm) const;
    void recomputeSampleRamMb();

    static constexpr int scopeHistorySize = 360;
    void copyScopeHistory (std::array<float, scopeHistorySize>& dest, bool& isSounding) const;

    static constexpr int fftSize = 512;
    void copyRecentAudioForFft (std::array<float, fftSize>& dest, bool& isSounding) const;

    void triggerNote (int midiNote, float velocity);
    void releaseNote (int midiNote);
    void uiNoteOn (int midiNote, float velocity);
    void uiNoteOff (int midiNote);
    void toggleZonePreview (int zoneIdx);
    bool isZonePreviewActive() const { return previewActive.load(); }
    void stopAllVoicesSmooth();

    bool savePresetMn6x (const juce::File& file);
    bool loadPresetMn6x (const juce::File& file);

    void saveUiLayoutToDisk();
    void loadUiLayoutFromDisk();
    void refreshLibraryFolderIfMatches (const juce::File& file);

    void setRootLibraryFolder (const juce::File& folder);
    void addLibraryFolderPersistent (const juce::File& folder);
    void sortLibraryPersistent();
    void removeLibraryEntryPersistent (int catIdx);
    void toggleLibraryCategoryExpanded (int catIdx);
    void setLoadedItemInPanel (const juce::String& displayName, const juce::File& file, int keyHint = -1);
    void clearLoadedItemPanel();

    static float vol100ToLinearGain (float vol100);

    int getGridDivisions() const;
    double getCurrentSampleRate() const { return currentSampleRate; }

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::CriticalSection zoneLock;
    std::vector<SampleGroup> groups;
    int selectedGroupIndex = 0;

    std::vector<SampleZone> zones;
    int selectedZoneIndex = -1;

    juce::File rootLibraryFolder;
    std::vector<LibraryCategoryEntry> libraryCategories;
    bool loadedItemActive = false;
    juce::String loadedItemName;
    juce::File loadedItemFile;
    int loadedItemKey = -1;

    juce::String brandLetters { "N6" };
    juce::String pluginTitleText { "KINETIK" };
    juce::String libraryNameText { "" };
    int uiThemeId = 0;
    int logoStyleId = 1;
    int logoColourIdx = 0;
    int knobStyleId = 1;
    int knobColourIdx = 0;
    int faderStyleId = 1;
    int faderColourIdx = 0;
    int switchStyleId = 1;
    int switchColourIdx = 0;

    float uiScale = 1.0f;
    int customizeGridStep = 20;
    std::map<juce::String, juce::Rectangle<int>> mainCustomBounds;
    std::map<juce::String, juce::Rectangle<int>> editCustomBounds;
    std::set<juce::String> mainHiddenKeys;
    std::set<juce::String> editHiddenKeys;

    std::atomic<bool> selectGroupOnly { true };
    std::atomic<bool> fxBypass { false };
    std::atomic<int> activeKeyswitchNote { -1 };
    std::atomic<float> seqPlayheadNorm { 0.0f };
    std::atomic<int> lastPlayedNote { -1 };
    std::array<std::atomic<uint8_t>, 128> activeNotesUi {};
    std::atomic<uint32_t> stateRevision { 0 };
    std::atomic<bool> internalPlaying { false };
    std::atomic<bool> previewActive { false };
    std::atomic<float> selectedZonePlayheadNorm { -1.0f };

    std::atomic<float> pitchWheelNorm { 0.0f };
    std::atomic<float> modWheelNorm { 0.0f };
    std::atomic<float> expressionNorm { 1.0f };
    std::atomic<float> masterGainDb { 0.0f };
    std::atomic<float> outputMeterL { 0.0f };
    std::atomic<float> outputMeterR { 0.0f };
    std::atomic<float> cpuUsagePercent { 0.0f };
    std::atomic<float> sampleRamMb { 0.0f };

    juce::String currentPresetName { "" };
    juce::File currentPresetFile;

private:
    juce::AudioFormatManager formatManager;
    static constexpr int maxVoices = 64;
    std::array<ActiveVoice, maxVoices> voices;
    double currentSampleRate = 44100.0;
    juce::Random rng;
    uint64_t voiceAgeCounter = 1;

    juce::AudioBuffer<float> groupScratchBuf;
    uint64_t rrSeedCounter = 1;
    float lastRrPitch = 0.0f;
    double vibratoPhase = 0.0;

    bool sustainPedalDown = false;
    std::array<bool, 128> sustainedNotes {};
    struct HeldNoteEntry { int note = 60; float vel = 0.8f; };
    std::vector<HeldNoteEntry> legatoNoteStack;
    int previewNote = -1;

    mutable juce::SpinLock scopeLock;
    std::array<float, scopeHistorySize> scopeRing {};
    int scopeWriteIdx = 0;
    int silentSamplesHold = 999999;

    static constexpr int fftRingSize = 1024;
    std::array<float, fftRingSize> fftRing {};
    std::atomic<int> fftWriteIdx { 0 };

    std::array<float, 128> heldNotesVel {};
    double seqBarPhase = 0.0;
    bool wasTransportPlaying = false;

    juce::File getRegistryFile() const;
    juce::File getUiLayoutFile() const;
    std::unique_ptr<juce::XmlElement> createFullStateXml (bool includeUiLayout);
    bool restoreFullStateFromXml (const juce::XmlElement& xml, bool restoreUiLayout);
    void scanFolderIntoCategories (const juce::File& folder);
    void saveLibraryRegistry();
    void loadLibraryRegistry();

    void startVoice (int zoneIdx, int midiNote, float velocity, bool isPreviewVoice = false);
    void glideExistingLegatoVoices (int newMidiNote, float velocity);
    void triggerNoteInternal (int midiNote, float velocity, bool isPreviewVoice = false);
    void releaseNoteInternal (int midiNote);
    bool isGroupActiveForNote (int groupIdx) const;
    bool hasAnySoloedGroup() const;
    std::vector<RhythmEvent> createDefaultEvents() const;
    bool hasAnyHeldNote() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RythmeAudioProcessor)
};
