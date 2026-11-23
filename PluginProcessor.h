#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <atomic>

struct RhythmEvent
{
    float position = 0.0f; // 0.0 to 1.0 across 1 bar (4 beats)
    float velocity = 0.85f; // 0.05 to 1.0
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

    // Multi-sample Real Round Robin layers (Track 1, Track 2, Track 3...)
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
    float driveAmt = 1.0f;
    juce::dsp::StateVariableTPTFilter<float> filterL;
    juce::dsp::StateVariableTPTFilter<float> filterR;
    float filterCutoff = 18000.0f;
    float filterRes = 0.707f;
};

class NoiseMu6ixRhythmAudioProcessor : public juce::AudioProcessor
{
public:
    NoiseMu6ixRhythmAudioProcessor();
    ~NoiseMu6ixRhythmAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Noise Mu6ix Rhythm"; }
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

    void triggerNote (int midiNote, float velocity);
    void releaseNote (int midiNote);
    void uiNoteOn (int midiNote, float velocity);
    void uiNoteOff (int midiNote);

    bool savePresetMn6x (const juce::File& file);
    bool loadPresetMn6x (const juce::File& file);

    int getGridDivisions() const; // 4, 8, 12, 16, 24

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::CriticalSection zoneLock;
    std::vector<SampleZone> zones;
    int selectedZoneIndex = -1;

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

    // 1000+ Generative Round-Robin state (NO panning!)
    uint64_t rrSeedCounter = 1;
    float lastRrPitch = 0.0f;

    // Polyphonic held notes for simultaneous multi-rhythm playback
    std::array<float, 128> heldNotesVel {};
    double seqBarPhase = 0.0; // 0.0 .. 1.0 over 1 bar (4 beats)
    bool wasTransportPlaying = false;

    void startVoice (int zoneIdx, int midiNote, float velocity);
    juce::Colour getNextZoneColour (int idx) const;
    std::vector<RhythmEvent> createDefaultEvents() const;
    bool hasAnyHeldNote() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseMu6ixRhythmAudioProcessor)
};
