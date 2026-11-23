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
    juce::String name { "Group 1" };
    bool muted = false;
};

struct SampleZone
{
    juce::String name { "Sample" };
    int groupIndex = 0; // Belongs to a SampleGroup (Kontakt-style)
    int lowKey = 60;
    int highKey = 60;
    int rootKey = 60;
    int lowVel = 0;
    int highVel = 127;
    // Default color is ALWAYS Blue (user can change manually via Color dropdown)
    juce::Colour colour { 0xff4ca3ff };
    int colourIndex = 0;

    // Per-Zone Independent Sample Wave Editor Parameters
    float startNorm = 0.0f;
    float endNorm = 1.0f;
    float loopStartNorm = 0.15f;
    float loopEndNorm = 0.85f;
    bool sampleLoop = false;
    float pitchSemi = 0.0f;
    float pan = 0.0f;
    float volume = 1.0f;
    float attackSec = 0.005f;
    float decaySec = 0.30f;
    float sustainLvl = 0.85f;
    float releaseSec = 0.25f;
    float cutoffHz = 18000.0f;
    float resQ = 0.707f;

    // Multi-sample Real Round Robin layers
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
    float gainL = 1.0f;
    float gainR = 1.0f;
    float velocity = 1.0f;
    float envVal = 0.0f;
    int envStage = 0; // 0=attack, 1=decay, 2=sustain, 3=release
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
    static juce::Colour getPaletteColour (int idx);
    static juce::String getPaletteColourName (int idx);

    NoiseMu6ixRhythmAudioProcessor();
    ~NoiseMu6ixRhythmAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Noise Mu6ix Rhythm v1.0.8"; }
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

    // Group management (Kontakt-style)
    int createEmptyGroup();
    void deleteGroup (int groupIdx);

    // Zone management
    int createEmptyZone (int key);
    bool addSampleToZone (int zoneIndex, const juce::File& file);
    bool loadSampleIntoNewZone (const juce::File& file, int lowKey, int highKey, int rootKey, int lowVel = 0, int highVel = 127);
    void deleteZone (int index);
    void deleteZones (std::vector<int> indices);

    void triggerNote (int midiNote, float velocity);
    void releaseNote (int midiNote);
    void uiNoteOn (int midiNote, float velocity);
    void uiNoteOff (int midiNote);

    bool savePresetMn6x (const juce::File& file);
    bool loadPresetMn6x (const juce::File& file);

    int getGridDivisions() const; // 4, 8, 12, 16, 24, 32, 48, 64

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::CriticalSection zoneLock;
    std::vector<SampleGroup> groups;
    int selectedGroupIndex = 0;

    std::vector<SampleZone> zones;
    int selectedZoneIndex = -1;

    // Kontakt-style Group & Mapping Editor flags
    std::atomic<bool> editAllGroups { false };
    std::atomic<bool> groupSoloEnabled { false };
    std::atomic<bool> zoneSoloEnabled { false };
    std::atomic<bool> selectByMidi { false };

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

    uint64_t rrSeedCounter = 1;
    float lastRrPitch = 0.0f;

    std::array<float, 128> heldNotesVel {};
    double seqBarPhase = 0.0;
    bool wasTransportPlaying = false;

    void startVoice (int zoneIdx, int midiNote, float velocity);
    bool isZoneAudible (int zoneIdx) const;
    std::vector<RhythmEvent> createDefaultEvents() const;
    bool hasAnyHeldNote() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseMu6ixRhythmAudioProcessor)
};
