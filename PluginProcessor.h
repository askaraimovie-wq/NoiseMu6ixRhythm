#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <atomic>
#include <cmath>
#include <functional>

// Lightweight CPU-friendly Cytomic TPT SVF Filter
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
        k = 1.0f / juce::jlimit (0.3f, 10.0f, q);
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

enum class FxType
{
    None = 0,
    EQ3,
    Compressor,
    Saturation,
    Filter,
    Transient,
    Delay,
    Reverb,
    Chorus,
    Width,
    Limiter
};

struct InsertEffect
{
    FxType type = FxType::None;
    bool bypassed = false;

    // 3-Band EQ (Kontakt EQ 3 style: Freq 1..3, BW 1..3, Gain 1..3)
    float eqFreq[3] = { 250.0f, 1500.0f, 7500.0f }; // Hz
    float eqBw[3]   = { 0.50f,  0.50f,   0.50f };   // octaves (0.1..2.0)
    float eqGain[3] = { 0.0f,   0.0f,    0.0f };    // dB (-18..+18)

    // Generic 4 normalized parameters (0.0..1.0) for other effects
    float p1 = 0.5f;
    float p2 = 0.5f;
    float p3 = 0.5f;
    float p4 = 0.5f;

    // DSP State
    FastSVF fL1, fR1, fL2, fR2, fL3, fR3;
    float envFollow = 0.0f;
    float envFast = 0.0f;
    float lfoPhase = 0.0f;
    std::vector<float> bufL, bufR;
    int writePos = 0;

    void prepare (double sr);
    void reset();
    void processStereo (float* L, float* R, int numSamples, double sr);
    static juce::String getTypeName (FxType t);
    static juce::String getParamLabel (FxType t, int idx);
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
    static constexpr int numInsertSlots = 8;

    juce::String name { "Group 1" };
    bool muted = false;
    int rrSetId = 0;
    int keyswitchNote = -1;

    // Per-Group Volume, Pan, Routing & Insert FX Rack (8 slots)
    float volume = 1.0f;
    float pan = 0.0f;
    float sendLevel = 0.0f;
    bool phaseInvert = false;
    bool lrSwap = false;
    int outputBusIndex = 0; // 0 = Out 1 .. 7 = Out 8

    std::array<InsertEffect, numInsertSlots> insertFx;
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
    int velFade = 0; // Velocity crossfade range (0..64)
    juce::Colour colour { 0xff4ca3ff };
    int colourIndex = 0;

    // Per-Zone Sample Wave Editor Parameters
    float startNorm = 0.0f;
    float endNorm = 1.0f;
    float loopStartNorm = 0.15f;
    float loopEndNorm = 0.85f;
    bool sampleLoop = false;
    int loopMode = 0;        // 0=Forward, 1=PingPong, 2=UntilRelease
    float loopCrossfade = 0.05f;
    float loopTuneCents = 0.0f;
    int loopCount = 0;

    bool reverse = false;
    float fadeInNorm = 0.0f;
    float fadeOutNorm = 0.0f;

    bool waveGridOn = false;
    bool waveGridAuto = true;
    int waveGridWidthIdx = 1;
    float waveGridBpm = 120.0f;

    float pitchSemi = 0.0f;
    float pan = 0.0f;
    float volume = 1.0f;
    float attackSec = 0.005f;
    float decaySec = 0.30f;
    float sustainLvl = 0.85f;
    float releaseSec = 0.25f;
    float cutoffHz = 18000.0f;
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
    double loopPitchRatio = 1.0;
    bool playingReverse = false;
    bool inLoopRegion = false;
    bool noteReleased = false;
    int loopsCompleted = 0;
    float gainL = 1.0f;
    float gainR = 1.0f;
    float velocity = 1.0f;
    float envVal = 0.0f;
    int envStage = 0;
    float attackInc = 0.01f;
    float decayDec = 0.001f;
    float sustainLvl = 0.85f;
    float releaseDec = 0.001f;
    float driveAmt = 1.0f;
    FastSVF filterL;
    FastSVF filterR;
};

class NoiseMu6ixRhythmAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numPaletteColours = 12;
    static constexpr int numOutputBuses = 8;
    static constexpr int numFxSlots = 8;

    static juce::Colour getPaletteColour (int idx);
    static juce::String getPaletteColourName (int idx);
    static int parseMidiNoteFromFilename (const juce::String& name);

    NoiseMu6ixRhythmAudioProcessor();
    ~NoiseMu6ixRhythmAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Noise Mu6ix Rhythm v1.1.0"; }
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
    void deleteGroup (int groupIdx);
    bool isKeyswitchNote (int midiNote) const;

    int createEmptyZone (int key);
    bool addSampleToZone (int zoneIndex, const juce::File& file);
    bool loadSampleIntoNewZone (const juce::File& file, int lowKey, int highKey, int rootKey, int lowVel = 0, int highVel = 127);
    void deleteZone (int index);
    void deleteZones (std::vector<int> indices);
    int duplicateZone (int index);
    void normalizeSelectedSample();
    void trimSelectedSample();
    void sliceSelectedSampleToKeys();
    void clearSelectedZoneRhythm();

    void triggerNote (int midiNote, float velocity);
    void releaseNote (int midiNote);
    void uiNoteOn (int midiNote, float velocity);
    void uiNoteOff (int midiNote);

    bool savePresetMn6x (const juce::File& file);
    bool loadPresetMn6x (const juce::File& file);

    int getGridDivisions() const;
    double getCurrentSampleRate() const { return currentSampleRate; }
    float getSelectedZonePlayheadNorm() const;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::CriticalSection zoneLock;
    std::vector<SampleGroup> groups;
    int selectedGroupIndex = 0;

    std::vector<SampleZone> zones;
    int selectedZoneIndex = -1;

    // 8 Send FX Slots + 8 Main Master FX Slots
    std::array<InsertEffect, numFxSlots> sendFx;
    std::array<InsertEffect, numFxSlots> mainFx;

    std::atomic<bool> editAllGroups { false };
    std::atomic<bool> groupSoloEnabled { false };
    std::atomic<bool> zoneSoloEnabled { false };
    std::atomic<bool> selectByMidi { false };
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
    juce::AudioBuffer<float> sendBusBuf;

    uint64_t rrSeedCounter = 1;
    float lastRrPitch = 0.0f;
    std::array<int, 9> groupRRCounters {};

    std::array<float, 128> heldNotesVel {};
    double seqBarPhase = 0.0;
    bool wasTransportPlaying = false;

    void startVoice (int zoneIdx, int midiNote, float velocity, float velFadeGain = 1.0f);
    bool isGroupActiveForNote (int groupIdx) const;
    bool isZoneAudible (int zoneIdx) const;
    std::vector<RhythmEvent> createDefaultEvents() const;
    bool hasAnyHeldNote() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseMu6ixRhythmAudioProcessor)
};
