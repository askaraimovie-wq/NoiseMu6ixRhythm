#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <atomic>
#include <cmath>

// Ultra-lightweight, CPU-friendly, copyable Cytomic TPT SVF Filter
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

enum class FxType
{
    None = 0,
    EQ,
    Compressor,
    Drive,
    Filter,
    Delay,
    Reverb,
    Width
};

struct InsertEffect
{
    FxType type = FxType::None;
    bool enabled = true;
    float p1 = 0.5f; // Primary param (e.g. Low/Thresh/Drive/Cutoff/Time/Size/Width)
    float p2 = 0.5f; // Secondary param (e.g. High/Ratio/Tone/Res/Feedback/Damp/Gain)
    float p3 = 0.5f; // Mix / Output

    // Lightweight DSP state (allocated lazily only for Delay/Reverb)
    FastSVF fL1, fR1, fL2, fR2;
    float envFollow = 0.0f;
    std::vector<float> delayBufL, delayBufR;
    int delayWritePos = 0;

    void prepare (double sr);
    void reset();
    void processStereo (float* L, float* R, int numSamples, double sr);
    static juce::String getTypeName (FxType t);
    static juce::String getParamName (FxType t, int idx);
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

struct SampleZone
{
    juce::String name { "Empty Track" };
    int lowKey = 60;
    int highKey = 60;
    int rootKey = 60;
    int lowVel = 0;
    int highVel = 127;
    juce::Colour colour { 0xffd9538f };

    // Per-Zone Independent Sample Wave Editor Parameters!
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

    // Mixer Controls (Volume/Pan shared with zone + Mute/Solo + Multi-Out + Unlimited Insert FX)
    bool muted = false;
    bool soloed = false;
    int outputBusIndex = 0; // 0 = Out 1 .. 7 = Out 8
    std::vector<InsertEffect> channelFx;

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
    static constexpr int numOutputBuses = 8;
    static constexpr int numGlobalFxSlots = 8;

    NoiseMu6ixRhythmAudioProcessor();
    ~NoiseMu6ixRhythmAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Noise Mu6ix Rhythm v1.0.4"; }
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

    int createEmptyZone (int key);
    bool addSampleToZone (int zoneIndex, const juce::File& file);
    bool loadSampleIntoNewZone (const juce::File& file, int lowKey, int highKey, int rootKey, int lowVel = 0, int highVel = 127);
    void deleteZone (int index);
    void addChannelFxToZone (int zoneIndex, FxType type);
    void removeChannelFxFromZone (int zoneIndex, int fxIndex);

    void triggerNote (int midiNote, float velocity);
    void releaseNote (int midiNote);
    void uiNoteOn (int midiNote, float velocity);
    void uiNoteOff (int midiNote);

    bool savePresetMn6x (const juce::File& file);
    bool loadPresetMn6x (const juce::File& file);

    int getGridDivisions() const;
    double getCurrentSampleRate() const { return currentSampleRate; }

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::CriticalSection zoneLock;
    std::vector<SampleZone> zones;
    int selectedZoneIndex = -1;

    // 8 Global Master FX Insert Slots (CPU-friendly: bypassed/None slots cost 0 CPU)
    std::array<InsertEffect, numGlobalFxSlots> globalFx;

    std::atomic<float> seqPlayheadNorm { 0.0f };
    std::atomic<int> lastPlayedNote { -1 };
    std::atomic<bool> internalPlaying { false };

    juce::String currentPresetName { "Rhythm Section 1" };
    juce::File currentPresetFile;

private:
    juce::AudioFormatManager formatManager;
    static constexpr int maxVoices = 32;
    std::array<ActiveVoice, maxVoices> voices;
    double currentSampleRate = 44100.0;
    juce::Random rng;

    // Pre-allocated scratch buffer for per-zone mixer channel FX (zero audio-thread allocations)
    juce::AudioBuffer<float> zoneScratchBuffer;

    uint64_t rrSeedCounter = 1;
    float lastRrPitch = 0.0f;

    std::array<float, 128> heldNotesVel {};
    double seqBarPhase = 0.0;
    bool wasTransportPlaying = false;

    void startVoice (int zoneIdx, int midiNote, float velocity);
    juce::Colour getNextZoneColour (int idx) const;
    std::vector<RhythmEvent> createDefaultEvents() const;
    bool hasAnyHeldNote() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseMu6ixRhythmAudioProcessor)
};
