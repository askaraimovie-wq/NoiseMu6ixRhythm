#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

NoiseMu6ixRhythmAudioProcessor::NoiseMu6ixRhythmAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    formatManager.registerBasicFormats();
    heldNotesVel.fill (0.0f);
}

NoiseMu6ixRhythmAudioProcessor::~NoiseMu6ixRhythmAudioProcessor() = default;

std::vector<RhythmEvent> NoiseMu6ixRhythmAudioProcessor::createDefaultEvents() const
{
    return {
        { 0.000f, 0.95f },
        { 0.250f, 0.85f },
        { 0.500f, 0.95f },
        { 0.750f, 0.85f }
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout NoiseMu6ixRhythmAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterBool> ("frr", "FRR (Fake Round Robin)", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("fvl", "FVL (Fake Velocity Layer)", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("pitchTrack", "Pitch Track (Keys)", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("sampleLoop", "Sample Loop", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("seqLoop", "Rhythm Loop", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("start", "Sample Start", 0.0f, 0.95f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("end", "Sample End", 0.05f, 1.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("loopStart", "Loop Start", 0.0f, 0.95f, 0.15f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("loopEnd", "Loop End", 0.05f, 1.0f, 0.85f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("pitch", "Pitch", -24.0f, 24.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("pan", "Pan", -1.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("volume", "Volume", 0.0f, 2.0f, 1.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack", "Attack", 0.001f, 2.0f, 0.005f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("decay", "Decay", 0.01f, 3.0f, 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sustain", "Sustain", 0.0f, 1.0f, 0.85f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("release", "Release", 0.01f, 4.0f, 0.25f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("cutoff", "Filter Cutoff",
                                                                   juce::NormalisableRange<float> (80.0f, 20000.0f, 1.0f, 0.3f), 18000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("res", "Resonance", 0.1f, 4.0f, 0.707f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("shuffle", "Shuffle", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("timeRand", "Time Randomize", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("velRand", "Velocity Randomize", 0.0f, 1.0f, 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "grid", "Grid",
        juce::StringArray { "1/4", "1/8", "1/12 (T)", "1/16", "1/24 (T)" }, 1));

    return { params.begin(), params.end() };
}

int NoiseMu6ixRhythmAudioProcessor::getGridDivisions() const
{
    int idx = static_cast<int> (apvts.getRawParameterValue ("grid")->load());
    switch (idx)
    {
        case 0: return 4;
        case 1: return 8;
        case 2: return 12;
        case 3: return 16;
        case 4: return 24;
        default: return 8;
    }
}

juce::Colour NoiseMu6ixRhythmAudioProcessor::getNextZoneColour (int idx) const
{
    static const juce::Colour palette[8] = {
        juce::Colour (0xffe04f8a), // Pink
        juce::Colour (0xff4ca3ff), // Blue
        juce::Colour (0xffe8a138), // Amber
        juce::Colour (0xff4cc98f), // Emerald
        juce::Colour (0xffa66cff), // Purple
        juce::Colour (0xffee6c4d), // Coral
        juce::Colour (0xff38c4d9), // Cyan
        juce::Colour (0xffd9c838)  // Gold
    };
    return palette[idx % 8];
}

void NoiseMu6ixRhythmAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (samplesPerBlock), 1 };
    for (auto& v : voices)
    {
        v.filterL.prepare (spec);
        v.filterR.prepare (spec);
        v.filterL.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        v.filterR.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        v.active = false;
    }
}

void NoiseMu6ixRhythmAudioProcessor::releaseResources() {}

bool NoiseMu6ixRhythmAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

bool NoiseMu6ixRhythmAudioProcessor::loadSampleIntoZone (const juce::File& file, int lowKey, int highKey, int rootKey, int lowVel, int highVel)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
        return false;

    SampleZone z;
    z.name = file.getFileNameWithoutExtension();
    z.file = file;
    z.lowKey = juce::jlimit (36, 84, lowKey);
    z.highKey = juce::jlimit (z.lowKey, 84, highKey);
    z.rootKey = juce::jlimit (36, 84, rootKey);
    z.lowVel = juce::jlimit (0, 127, lowVel);
    z.highVel = juce::jlimit (z.lowVel, 127, highVel);
    z.sampleRate = reader->sampleRate;
    z.seqEvents = createDefaultEvents();
    z.buffer.setSize (static_cast<int> (reader->numChannels), static_cast<int> (reader->lengthInSamples));
    reader->read (&z.buffer, 0, static_cast<int> (reader->lengthInSamples), 0, true, true);

    const juce::ScopedLock sl (zoneLock);
    z.colour = getNextZoneColour (static_cast<int> (zones.size()));
    zones.push_back (std::move (z));
    selectedZoneIndex = static_cast<int> (zones.size()) - 1;
    return true;
}

void NoiseMu6ixRhythmAudioProcessor::deleteZone (int index)
{
    const juce::ScopedLock sl (zoneLock);
    if (index >= 0 && index < static_cast<int> (zones.size()))
    {
        zones.erase (zones.begin() + index);
        if (zones.empty())
            selectedZoneIndex = -1;
        else
            selectedZoneIndex = juce::jlimit (0, static_cast<int> (zones.size()) - 1, selectedZoneIndex);
    }
}

void NoiseMu6ixRhythmAudioProcessor::startVoice (int zoneIdx, int midiNote, float velocity)
{
    if (zoneIdx < 0 || zoneIdx >= static_cast<int> (zones.size()))
        return;

    const auto& z = zones[static_cast<size_t> (zoneIdx)];
    if (z.buffer.getNumSamples() == 0)
        return;

    int vIdx = 0;
    for (int i = 0; i < maxVoices; ++i)
    {
        if (! voices[static_cast<size_t> (i)].active)
        {
            vIdx = i;
            break;
        }
    }

    auto& v = voices[static_cast<size_t> (vIdx)];
    v.active = true;
    v.zoneIndex = zoneIdx;
    v.midiNote = midiNote;
    v.velocity = juce::jlimit (0.04f, 1.0f, velocity);
    v.envVal = 0.0f;
    v.envStage = 0;

    bool frrOn = apvts.getRawParameterValue ("frr")->load() > 0.5f;
    bool fvlOn = apvts.getRawParameterValue ("fvl")->load() > 0.5f;
    bool pitchTrackOn = apvts.getRawParameterValue ("pitchTrack")->load() > 0.5f;

    float pitchSemi = apvts.getRawParameterValue ("pitch")->load();
    float basePan = apvts.getRawParameterValue ("pan")->load();
    float startNorm = apvts.getRawParameterValue ("start")->load();
    float baseCutoff = apvts.getRawParameterValue ("cutoff")->load();
    float baseRes = apvts.getRawParameterValue ("res")->load();
    float attackSec = apvts.getRawParameterValue ("attack")->load();
    float velRandAmt = apvts.getRawParameterValue ("velRand")->load();

    if (velRandAmt > 0.001f)
    {
        float delta = (rng.nextFloat() * 2.0f - 1.0f) * 0.30f * velRandAmt;
        v.velocity = juce::jlimit (0.04f, 1.0f, v.velocity + delta);
    }

    // 1000+ Unique Generative Fake Round Robin (FRR)
    // Uses multi-dimensional irrational Weyl sequences + PRNG + anti-repeat memory
    float rrPitchCents = 0.0f;
    float rrPan = 0.0f;
    float rrStartOffsetSamples = 0.0f;
    float rrFilterMult = 1.0f;
    float rrGainMult = 1.0f;
    float rrExtraAttackSec = 0.0f;

    if (frrOn)
    {
        ++rrSeedCounter;
        double n = static_cast<double> (rrSeedCounter);
        // 5 mutually irrational constants ensure thousands of unique non-repeating combinations
        float u1 = static_cast<float> (std::fmod (n * 0.61803398875 + rng.nextFloat() * 0.13, 1.0));
        float u2 = static_cast<float> (std::fmod (n * 0.75487766625 + rng.nextFloat() * 0.17, 1.0));
        float u3 = static_cast<float> (std::fmod (n * 0.56984029099 + rng.nextFloat() * 0.19, 1.0));
        float u4 = static_cast<float> (std::fmod (n * 0.43015970900 + rng.nextFloat() * 0.11, 1.0));
        float u5 = static_cast<float> (std::fmod (n * 0.32471795724 + rng.nextFloat() * 0.23, 1.0));

        rrPitchCents = (u1 * 2.0f - 1.0f) * 18.0f; // +-18 cents continuous
        if (std::abs (rrPitchCents - lastRrPitch) < 4.5f)
            rrPitchCents = -rrPitchCents;
        lastRrPitch = rrPitchCents;

        rrPan = (u2 * 2.0f - 1.0f) * 0.28f;
        if (std::abs (rrPan - lastRrPan) < 0.06f)
            rrPan = -rrPan;
        lastRrPan = rrPan;

        rrStartOffsetSamples = u3 * static_cast<float> (currentSampleRate * 0.0042); // 0 to 4.2ms transient shift
        rrFilterMult = 0.68f + u4 * 0.50f;  // 0.68x to 1.18x tonal brightness shift
        rrGainMult = 0.88f + u5 * 0.24f;    // subtle +-1.2 dB organic hit weight
        rrExtraAttackSec = u3 * 0.0045f;    // subtle transient soften/sharpen
    }

    // Continuous Multi-Dimensional Fake Velocity Layer (FVL)
    float fvlCutoff = baseCutoff;
    float gain = v.velocity;
    float fvlExtraAttackSec = 0.0f;
    v.driveAmt = 1.0f;

    if (fvlOn)
    {
        float vel = v.velocity;
        float velCurve = std::pow (vel, 1.75f);
        // Soft hits = warmer/darker + softer transient attack; Hard hits = bright, punchy + subtle harmonic drive
        fvlCutoff = juce::jlimit (280.0f, 20000.0f, 320.0f + (baseCutoff - 320.0f) * velCurve);
        gain = 0.16f + 0.84f * velCurve;
        fvlExtraAttackSec = (1.0f - vel) * (1.0f - vel) * 0.014f; // up to 14ms softer attack on ghost hits
        v.driveAmt = 1.0f + std::max (0.0f, vel - 0.55f) * 2.4f;  // subtle saturation punch on hard hits
    }

    float totalAttackSec = std::max (0.0005f, attackSec + rrExtraAttackSec + fvlExtraAttackSec);
    v.attackInc = 1.0f / std::max (1.0f, static_cast<float> (totalAttackSec * currentSampleRate));

    // Pitch Track ON = follow keyboard pitch (strings/melodic); OFF = keep fixed root pitch (drums/percussion)
    double noteSemiOffset = pitchTrackOn ? static_cast<double> (midiNote - z.rootKey) : 0.0;
    double semitones = noteSemiOffset + static_cast<double> (pitchSemi) + (rrPitchCents / 100.0);
    v.pitchRatio = (z.sampleRate / currentSampleRate) * std::pow (2.0, semitones / 12.0);
    v.currentPos = static_cast<double> (startNorm * z.buffer.getNumSamples()) + rrStartOffsetSamples;

    // Combined Pan (User Pan Knob + FRR Micro-Pan)
    float finalPan = juce::jlimit (-1.0f, 1.0f, basePan + rrPan);
    float panAngle = (finalPan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    float finalGain = gain * rrGainMult;
    v.gainL = finalGain * std::cos (panAngle) * 1.41421356f;
    v.gainR = finalGain * std::sin (panAngle) * 1.41421356f;

    v.filterCutoff = juce::jlimit (80.0f, 20000.0f, fvlCutoff * rrFilterMult);
    v.filterRes = baseRes;
    v.filterL.reset();
    v.filterR.reset();
    v.filterL.setCutoffFrequency (v.filterCutoff);
    v.filterR.setCutoffFrequency (v.filterCutoff);
    v.filterL.setResonance (v.filterRes);
    v.filterR.setResonance (v.filterRes);
}

void NoiseMu6ixRhythmAudioProcessor::triggerNote (int midiNote, float velocity)
{
    lastPlayedNote.store (midiNote);
    const juce::ScopedLock sl (zoneLock);
    int velInt = juce::jlimit (0, 127, static_cast<int> (velocity * 127.0f));
    for (int i = 0; i < static_cast<int> (zones.size()); ++i)
    {
        const auto& z = zones[static_cast<size_t> (i)];
        if (midiNote >= z.lowKey && midiNote <= z.highKey && velInt >= z.lowVel && velInt <= z.highVel)
        {
            startVoice (i, midiNote, velocity);
            selectedZoneIndex = i;
        }
    }
}

void NoiseMu6ixRhythmAudioProcessor::releaseNote (int midiNote)
{
    if (lastPlayedNote.load() == midiNote)
        lastPlayedNote.store (-1);
    for (auto& v : voices)
    {
        if (v.active && v.midiNote == midiNote)
            v.envStage = 3;
    }
}

bool NoiseMu6ixRhythmAudioProcessor::hasAnyHeldNote() const
{
    if (masterAllHeld.load()) return true;
    for (float v : heldNotesVel)
        if (v > 0.0f) return true;
    return false;
}

void NoiseMu6ixRhythmAudioProcessor::uiNoteOn (int midiNote, float velocity)
{
    bool seqLoopOn = apvts.getRawParameterValue ("seqLoop")->load() > 0.5f;
    bool wasIdle = ! hasAnyHeldNote();

    if (midiNote == masterAllKey)
    {
        masterAllHeld.store (true);
        if (wasIdle) { seqBarPhase = 0.0; seqPlayheadNorm.store (0.0f); }
        const juce::ScopedLock sl (zoneLock);
        for (int i = 0; i < static_cast<int> (zones.size()); ++i)
        {
            if (! seqLoopOn)
                startVoice (i, zones[static_cast<size_t> (i)].rootKey, velocity);
            else
            {
                for (const auto& ev : zones[static_cast<size_t> (i)].seqEvents)
                    if (ev.position < 0.005f)
                        startVoice (i, zones[static_cast<size_t> (i)].rootKey, velocity * ev.velocity);
            }
        }
        return;
    }

    if (midiNote >= 0 && midiNote < 128)
        heldNotesVel[static_cast<size_t> (midiNote)] = velocity;

    if (! seqLoopOn)
    {
        triggerNote (midiNote, velocity);
    }
    else
    {
        if (wasIdle) { seqBarPhase = 0.0; seqPlayheadNorm.store (0.0f); }
        lastPlayedNote.store (midiNote);
        const juce::ScopedLock sl (zoneLock);
        for (int i = 0; i < static_cast<int> (zones.size()); ++i)
        {
            const auto& z = zones[static_cast<size_t> (i)];
            if (midiNote >= z.lowKey && midiNote <= z.highKey)
            {
                selectedZoneIndex = i;
                for (const auto& ev : z.seqEvents)
                    if (ev.position < 0.005f)
                        startVoice (i, midiNote, velocity * ev.velocity);
            }
        }
    }
}

void NoiseMu6ixRhythmAudioProcessor::uiNoteOff (int midiNote)
{
    if (midiNote == masterAllKey)
    {
        masterAllHeld.store (false);
        for (auto& v : voices)
            if (v.active) v.envStage = 3;
        return;
    }
    if (midiNote >= 0 && midiNote < 128)
        heldNotesVel[static_cast<size_t> (midiNote)] = 0.0f;
    releaseNote (midiNote);
}

void NoiseMu6ixRhythmAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    bool seqLoopOn = apvts.getRawParameterValue ("seqLoop")->load() > 0.5f;
    bool sampleLoopOn = apvts.getRawParameterValue ("sampleLoop")->load() > 0.5f;
    float vol = apvts.getRawParameterValue ("volume")->load();
    float startNorm = apvts.getRawParameterValue ("start")->load();
    float endNorm = apvts.getRawParameterValue ("end")->load();
    float loopStartNorm = apvts.getRawParameterValue ("loopStart")->load();
    float loopEndNorm = apvts.getRawParameterValue ("loopEnd")->load();

    float decaySec = apvts.getRawParameterValue ("decay")->load();
    float sustainLvl = apvts.getRawParameterValue ("sustain")->load();
    float releaseSec = apvts.getRawParameterValue ("release")->load();

    float shuffleAmt = apvts.getRawParameterValue ("shuffle")->load();
    float timeRandAmt = apvts.getRawParameterValue ("timeRand")->load();

    // Handle Incoming MIDI (Polyphonic + Master ALL key)
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            uiNoteOn (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            uiNoteOff (msg.getNoteNumber());
    }

    // Polyphonic Multi-Zone Rhythm Sequencer
    if (seqLoopOn && hasAnyHeldNote())
    {
        double bpm = 120.0;
        if (auto* ph = getPlayHead())
        {
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm())
                    bpm = *b;
        }
        double samplesPerBar = (60.0 / bpm) * 4.0 * currentSampleRate;
        double prevPhase = seqBarPhase;
        double deltaPhase = static_cast<double> (buffer.getNumSamples()) / std::max (1.0, samplesPerBar);
        seqBarPhase += deltaPhase;
        bool wrapped = false;
        if (seqBarPhase >= 1.0)
        {
            seqBarPhase -= 1.0;
            wrapped = true;
        }
        seqPlayheadNorm.store (static_cast<float> (seqBarPhase));

        int divs = getGridDivisions();
        bool allOn = masterAllHeld.load();

        const juce::ScopedLock sl (zoneLock);
        for (int zIdx = 0; zIdx < static_cast<int> (zones.size()); ++zIdx)
        {
            const auto& z = zones[static_cast<size_t> (zIdx)];

            // Find if this zone should play (either Master ALL key is held, or a key inside this zone is held)
            int triggerMidi = -1;
            float triggerVel = 0.0f;

            if (allOn)
            {
                triggerMidi = z.rootKey;
                triggerVel = 0.9f;
            }
            else
            {
                for (int k = z.lowKey; k <= z.highKey; ++k)
                {
                    if (k >= 0 && k < 128 && heldNotesVel[static_cast<size_t> (k)] > 0.0f)
                    {
                        triggerMidi = k;
                        triggerVel = heldNotesVel[static_cast<size_t> (k)];
                        break;
                    }
                }
            }

            if (triggerMidi < 0)
                continue;

            for (const auto& ev : z.seqEvents)
            {
                double evPos = static_cast<double> (ev.position);
                int subIdx = static_cast<int> (std::round (evPos * divs));
                if (subIdx % 2 == 1 && shuffleAmt > 0.001f)
                    evPos += (shuffleAmt * 0.35) / static_cast<double> (divs);
                if (timeRandAmt > 0.001f)
                    evPos += (rng.nextFloat() * 2.0f - 1.0f) * 0.012 * timeRandAmt;
                evPos = std::fmod (evPos + 1.0, 1.0);

                bool triggered = (! wrapped && evPos > prevPhase && evPos <= seqBarPhase)
                              || (wrapped && (evPos > prevPhase || evPos <= seqBarPhase));
                if (triggered)
                    startVoice (zIdx, triggerMidi, triggerVel * ev.velocity);
            }
        }
    }

    const juce::ScopedLock sl (zoneLock);
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getWritePointer (1);

    float decDec = 1.0f / std::max (1.0f, static_cast<float> (decaySec * currentSampleRate));
    float relDec = 1.0f / std::max (1.0f, static_cast<float> (releaseSec * currentSampleRate));

    for (auto& v : voices)
    {
        if (! v.active || v.zoneIndex < 0 || v.zoneIndex >= static_cast<int> (zones.size()))
            continue;

        const auto& z = zones[static_cast<size_t> (v.zoneIndex)];
        int numSamples = z.buffer.getNumSamples();
        int startSample = static_cast<int> (startNorm * numSamples);
        int endSample = std::max (startSample + 2, static_cast<int> (endNorm * numSamples));
        int lStartSample = juce::jlimit (startSample, endSample - 2, static_cast<int> (loopStartNorm * numSamples));
        int lEndSample = juce::jlimit (lStartSample + 2, endSample, static_cast<int> (loopEndNorm * numSamples));

        const float* srcL = z.buffer.getReadPointer (0);
        const float* srcR = z.buffer.getNumChannels() > 1 ? z.buffer.getReadPointer (1) : srcL;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            int posInt = static_cast<int> (v.currentPos);
            if (sampleLoopOn && posInt >= lEndSample - 1)
            {
                v.currentPos = static_cast<double> (lStartSample);
                posInt = lStartSample;
            }
            else if (posInt >= endSample - 1 || posInt >= numSamples - 1)
            {
                v.active = false;
                break;
            }

            float frac = static_cast<float> (v.currentPos - posInt);
            float sL = srcL[posInt] + frac * (srcL[posInt + 1] - srcL[posInt]);
            float sR = srcR[posInt] + frac * (srcR[posInt + 1] - srcR[posInt]);

            if (v.envStage == 0)
            {
                v.envVal += v.attackInc;
                if (v.envVal >= 1.0f) { v.envVal = 1.0f; v.envStage = 1; }
            }
            else if (v.envStage == 1)
            {
                v.envVal -= decDec;
                if (v.envVal <= sustainLvl) { v.envVal = sustainLvl; v.envStage = 2; }
            }
            else if (v.envStage == 3)
            {
                v.envVal -= relDec;
                if (v.envVal <= 0.0001f) { v.envVal = 0.0f; v.active = false; break; }
            }

            sL = v.filterL.processSample (0, sL);
            sR = v.filterR.processSample (0, sR);

            // Subtle FVL harmonic saturation on hard hits
            if (v.driveAmt > 1.01f)
            {
                sL = std::tanh (sL * v.driveAmt) / std::tanh (v.driveAmt);
                sR = std::tanh (sR * v.driveAmt) / std::tanh (v.driveAmt);
            }

            outL[i] += sL * v.envVal * v.gainL * vol;
            outR[i] += sR * v.envVal * v.gainR * vol;

            v.currentPos += v.pitchRatio;
        }
    }
}

bool NoiseMu6ixRhythmAudioProcessor::savePresetMn6x (const juce::File& file)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("presetName", currentPresetName);

    auto* zonesXml = xml->createNewChildElement ("ZONES");
    const juce::ScopedLock sl (zoneLock);
    for (const auto& z : zones)
    {
        auto* zx = zonesXml->createNewChildElement ("ZONE");
        zx->setAttribute ("file", z.file.getFullPathName());
        zx->setAttribute ("lowKey", z.lowKey);
        zx->setAttribute ("highKey", z.highKey);
        zx->setAttribute ("rootKey", z.rootKey);
        zx->setAttribute ("lowVel", z.lowVel);
        zx->setAttribute ("highVel", z.highVel);

        auto* seqXml = zx->createNewChildElement ("SEQ_EVENTS");
        for (const auto& ev : z.seqEvents)
        {
            auto* ex = seqXml->createNewChildElement ("EV");
            ex->setAttribute ("pos", ev.position);
            ex->setAttribute ("vel", ev.velocity);
        }
    }

    currentPresetFile = file;
    return xml->writeTo (file);
}

bool NoiseMu6ixRhythmAudioProcessor::loadPresetMn6x (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
        return false;

    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    currentPresetName = xml->getStringAttribute ("presetName", file.getFileNameWithoutExtension());
    currentPresetFile = file;

    if (auto* zonesXml = xml->getChildByName ("ZONES"))
    {
        {
            const juce::ScopedLock sl (zoneLock);
            zones.clear();
        }
        for (auto* zx : zonesXml->getChildIterator())
        {
            juce::File f (zx->getStringAttribute ("file"));
            if (f.existsAsFile())
            {
                if (loadSampleIntoZone (f,
                    zx->getIntAttribute ("lowKey", 60),
                    zx->getIntAttribute ("highKey", 60),
                    zx->getIntAttribute ("rootKey", 60),
                    zx->getIntAttribute ("lowVel", 0),
                    zx->getIntAttribute ("highVel", 127)))
                {
                    if (auto* seqXml = zx->getChildByName ("SEQ_EVENTS"))
                    {
                        const juce::ScopedLock sl (zoneLock);
                        auto& z = zones.back();
                        z.seqEvents.clear();
                        for (auto* ex : seqXml->getChildIterator())
                        {
                            RhythmEvent ev;
                            ev.position = static_cast<float> (ex->getDoubleAttribute ("pos", 0.0));
                            ev.velocity = static_cast<float> (ex->getDoubleAttribute ("vel", 0.85));
                            z.seqEvents.push_back (ev);
                        }
                    }
                }
            }
        }
    }
    return true;
}

void NoiseMu6ixRhythmAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NoiseMu6ixRhythmAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorEditor* NoiseMu6ixRhythmAudioProcessor::createEditor()
{
    return new NoiseMu6ixRhythmAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NoiseMu6ixRhythmAudioProcessor();
}
