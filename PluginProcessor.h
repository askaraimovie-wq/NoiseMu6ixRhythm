#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

struct SampleZone
{
    juce::String name;
    juce::File file;
    int lowKey = 36;
    int highKey = 84;
    int rootKey = 60;
    int lowVel = 0;
    int highVel = 127;
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
};

struct ActiveVoice
{
    bool active = false;
    int zoneIndex = -1;
    int midiNote = 60;
    double currentPos = 0.0;
    double pitchRatio = 1.0;
    float gainL = 1.0f;
    float gainR = 1.0f;
    float velocity = 1.0f;
    float envVal = 0.0f;
    int envStage = 0; // 0=attack, 1=decay, 2=sustain, 3=release
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

    bool loadSampleIntoZone (const juce::File& file, int lowKey, int highKey, int rootKey, int lowVel = 0, int highVel = 127);
    void triggerNote (int midiNote, float velocity);
    void releaseNote (int midiNote);

    bool savePresetMn6x (const juce::File& file);
    bool loadPresetMn6x (const juce::File& file);

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::CriticalSection zoneLock;
    std::vector<SampleZone> zones;
    int selectedZoneIndex = 0;

    // 16-step rhythm sequencer velocities (0.0 = off, >0.0 = active)
    std::array<float, 16> stepVelocities { 1.0f, 0.0f, 0.5f, 0.8f, 0.0f, 0.9f, 0.0f, 0.7f,
                                           1.0f, 0.0f, 0.6f, 0.85f, 0.0f, 0.9f, 0.5f, 0.0f };
    std::atomic<int> currentSeqStep { 0 };
    juce::String currentPresetName { "Rhythm Section 1.mn6x" };

private:
    juce::AudioFormatManager formatManager;
    static constexpr int maxVoices = 16;
    std::array<ActiveVoice, maxVoices> voices;
    double currentSampleRate = 44100.0;
    juce::Random rng;
    int rrCounter = 0;

    // Sequencer state
    double seqPhaseSamples = 0.0;
    int heldMidiNote = -1;
    float heldVelocity = 0.8f;

    void startVoice (int zoneIdx, int midiNote, float velocity);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseMu6ixRhythmAudioProcessor)
};
