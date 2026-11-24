#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <atomic>
#include <cmath>
#include <functional>
#include <string>

#ifndef RYTHME_AUTO_VERSION_STR
#define RYTHME_AUTO_VERSION_STR "v1.2.5"
#endif

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
        float fc = juce::jlimit (30.0f, static_cast<float> (sampleRate * 0.45), cutoffHz);
        g = std::tan (juce::MathConstants<float>::pi * fc / static_cast<float> (sampleRate));
        k = 1.0f / juce::jlimit (0.3f, 8.0f, q);
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
};

enum class SimpleFxType
{
    Off = 0,
    EQ,
    Compressor,
    Saturation,
    Delay,
    Reverb
};

struct SimpleFxSlot
{
    SimpleFxType type = SimpleFxType::Off;
    bool enabled = true;
    float p1 = 0.5f;
    float p2 = 0.5f;
    float p3 = 0.5f;
    float p4 = 0.5f;
    float p5 = 0.5f;

    FastSVF fL, fR, midL, midR;
    float env = 0.0f;
    float lastGrDb = 0.0f;
    std::vector<float> bufL, bufR;
    int writePos = 0;

    void prepare (double sr);
    void processStereo (float* L, float* R, int numSamples, double sr);
    static juce::String getTypeName (SimpleFxType t);
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
    static constexpr int numFxSlots = 4;

    juce::String name { "Group 1" };
    bool muted = false;
    bool soloed = false;
    int keyswitchNote = -1;
    float volumeDb100 = 80.0f;
    float pan50 = 0.0f;
    std::array<SimpleFxSlot, numFxSlots> fxSlots;
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
    juce::Colour colour { 0xff4ca3ff };
    int colourIndex = 0;

    float startNorm = 0.0f;
    float endNorm = 1.0f;
    float loopStartNorm = 0.15f;
    float loopEndNorm = 0.85f;
    bool sampleLoop = false;
    bool autoLoopZeroCross = true;
    float loopCrossfade = 0.04f;
    float fadeInNorm = 0.0f;
    float fadeOutNorm = 0.0f;
    bool reverse = false;

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
    bool realRREnabled = true;

    std::vector<RhythmEvent> seqEvents;
};

struct ActiveVoice
{
    bool active = false;
    int zoneIndex = -1;
    int layerIndex = 0;
    int midiNote = 60;
    double currentPos = 0.0;
    double pitchRatio = 1.0;
    bool playingReverse = false;
    float gainL = 1.0f;
    float gainR = 1.0f;
    float velocity = 1.0f;
    float envVal = 0.0f;
    int envStage = 0;
    float attackInc = 0.01f;
    float decayDec = 0.001f;
    float sustainLvl = 0.85f;
    float releaseDec = 0.001f;
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

    RythmeAudioProcessor();
    ~RythmeAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RYTHME " + getBuildVersionString(); }
    static juce::String getBuildVersionString() { return juce::String (RYTHME_AUTO_VERSION_STR); }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

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
    void deleteZone (int index);
    void deleteZones (std::vector<int> indices);
    void normalizeSelectedSample();
    void resetSelectedZoneDefaults();
    void clearSelectedZoneRhythm();
    float snapNormToZeroCrossing (int zoneIndex, float rawNorm) const;

    static constexpr int scopeHistorySize = 360;
    void copyScopeHistory (std::array<float, scopeHistorySize>& dest, bool& isSounding) const;

    void triggerNote (int midiNote, float velocity);
    void releaseNote (int midiNote);
    void uiNoteOn (int midiNote, float velocity);
    void uiNoteOff (int midiNote);

    bool savePresetMn6x (const juce::File& file);
    bool loadPresetMn6x (const juce::File& file);

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

    std::vector<LibraryCategoryEntry> libraryCategories;
    bool loadedItemActive = false;
    juce::String loadedItemName;
    juce::File loadedItemFile;
    int loadedItemKey = -1;

    juce::String brandLetters { "N6" };
    juce::String pluginTitleText { "RYTHME" };
    int logoStyleId = 1;    // 1..4
    int logoColourIdx = 0;  // 0..11
    int knobStyleId = 1;    // 1..10
    int faderStyleId = 1;   // 1..10
    int switchStyleId = 1;  // 1..10

    std::atomic<bool> selectGroupOnly { true };
    std::atomic<bool> seqBypass { false };
    std::atomic<bool> fxBypass { false };
    std::atomic<int> activeKeyswitchNote { -1 };
    std::atomic<float> seqPlayheadNorm { 0.0f };
    std::atomic<int> lastPlayedNote { -1 };
    std::atomic<bool> internalPlaying { false };

    juce::String currentPresetName { "" };
    juce::File currentPresetFile;

private:
    juce::AudioFormatManager formatManager;
    static constexpr int maxVoices = 32;
    std::array<ActiveVoice, maxVoices> voices;
    double currentSampleRate = 44100.0;
    juce::Random rng;

    juce::AudioBuffer<float> groupScratchBuf;
    uint64_t rrSeedCounter = 1;
    float lastRrPitch = 0.0f;

    mutable juce::SpinLock scopeLock;
    std::array<float, scopeHistorySize> scopeRing {};
    int scopeWriteIdx = 0;
    int silentSamplesHold = 999999;

    std::array<float, 128> heldNotesVel {};
    double seqBarPhase = 0.0;
    bool wasTransportPlaying = false;

    juce::File getRegistryFile() const;
    void saveLibraryRegistry();
    void loadLibraryRegistry();

    void startVoice (int zoneIdx, int midiNote, float velocity);
    bool isGroupActiveForNote (int groupIdx) const;
    bool hasAnySoloedGroup() const;
    std::vector<RhythmEvent> createDefaultEvents() const;
    bool hasAnyHeldNote() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RythmeAudioProcessor)
};
