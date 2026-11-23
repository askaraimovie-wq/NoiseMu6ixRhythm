#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

NoiseMu6ixRhythmAudioProcessor::NoiseMu6ixRhythmAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    formatManager.registerBasicFormats();
}

NoiseMu6ixRhythmAudioProcessor::~NoiseMu6ixRhythmAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NoiseMu6ixRhythmAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterBool> ("frr", "FRR (Fake Round Robin)", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("fvl", "FVL (Fake Velocity Layer)", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("sampleLoop", "Sample Loop", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("seqLoop", "Rhythm Loop", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("start", "Sample Start", 0.0f, 0.95f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("end", "Sample End", 0.05f, 1.0f, 1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("pitch", "Pitch", -24.0f, 24.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("volume", "Volume", 0.0f, 2.0f, 1.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("attack", "Attack", 0.001f, 2.0f, 0.005f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("decay", "Decay", 0.01f, 3.0f, 0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("sustain", "Sustain", 0.0f, 1.0f, 0.8f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("release", "Release", 0.01f, 4.0f, 0.25f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("cutoff", "Filter Cutoff",
                                                                   juce::NormalisableRange<float> (80.0f, 20000.0f, 1.0f, 0.3f), 18000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("res", "Resonance", 0.1f, 4.0f, 0.707f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("shuffle", "Shuffle", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("timeRand", "Time Randomize", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("velRand", "Velocity Randomize", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("grid", "Grid", juce::StringArray { "1/4", "1/8", "1/16" }, 1));

    return { params.begin(), params.end() };
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
    z.lowKey = lowKey;
    z.highKey = highKey;
    z.rootKey = rootKey;
    z.lowVel = lowVel;
    z.highVel = highVel;
    z.sampleRate = reader->sampleRate;
    z.buffer.setSize (static_cast<int> (reader->numChannels), static_cast<int> (reader->lengthInSamples));
    reader->read (&z.buffer, 0, static_cast<int> (reader->lengthInSamples), 0, true, true);

    const juce::ScopedLock sl (zoneLock);
    zones.push_back (std::move (z));
    selectedZoneIndex = static_cast<int> (zones.size()) - 1;
    return true;
}

void NoiseMu6ixRhythmAudioProcessor::startVoice (int zoneIdx, int midiNote, float velocity)
{
    if (zoneIdx < 0 || zoneIdx >= static_cast<int> (zones.size()))
        return;

    const auto& z = zones[static_cast<size_t> (zoneIdx)];
    if (z.buffer.getNumSamples() == 0)
        return;

    // Find free voice or steal oldest
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
    v.velocity = juce::jlimit (0.05f, 1.0f, velocity);
    v.envVal = 0.0f;
    v.envStage = 0;

    bool frrOn = *apvts.getRawParameterValue ("frr") > 0.5f;
    bool fvlOn = *apvts.getRawParameterValue ("fvl") > 0.5f;
    float pitchSemi = *apvts.getRawParameterValue ("pitch");
    float startNorm = *apvts.getRawParameterValue ("start");
    float baseCutoff = *apvts.getRawParameterValue ("cutoff");
    float baseRes = *apvts.getRawParameterValue ("res");
    float velRandAmt = *apvts.getRawParameterValue ("velRand");

    if (velRandAmt > 0.001f)
    {
        float delta = (rng.nextFloat() * 2.0f - 1.0f) * 0.25f * velRandAmt;
        v.velocity = juce::jlimit (0.05f, 1.0f, v.velocity + delta);
    }

    // Fake Round Robin (FRR): micro-pitch, micro start-offset, subtle pan & filter tilt
    float rrPitchCents = 0.0f;
    float rrPan = 0.0f;
    float rrStartOffsetSamples = 0.0f;
    float rrFilterMult = 1.0f;

    if (frrOn)
    {
        rrCounter = (rrCounter + 1) % 8;
        static constexpr float rrPitchPattern[8] = { 0.0f, 6.5f, -5.0f, 9.0f, -8.5f, 4.0f, -3.5f, 11.0f };
        static constexpr float rrPanPattern[8]   = { 0.0f, 0.12f, -0.10f, 0.16f, -0.14f, 0.08f, -0.06f, 0.18f };
        static constexpr float rrFiltPattern[8]  = { 1.0f, 0.92f, 1.06f, 0.85f, 1.08f, 0.95f, 0.88f, 1.04f };

        rrPitchCents = rrPitchPattern[rrCounter] + (rng.nextFloat() * 4.0f - 2.0f);
        rrPan = rrPanPattern[rrCounter];
        rrStartOffsetSamples = rng.nextFloat() * static_cast<float> (currentSampleRate * 0.0025); // up to 2.5ms
        rrFilterMult = rrFiltPattern[rrCounter];
    }

    // Fake Velocity Layer (FVL): dynamic lowpass brightness + gain curve
    float fvlCutoff = baseCutoff;
    float gain = v.velocity;
    if (fvlOn)
    {
        // Soft hits = warmer/darker, Hard hits = bright/punchy
        float velCurve = std::pow (v.velocity, 1.6f);
        fvlCutoff = juce::jlimit (350.0f, 20000.0f, 450.0f + (baseCutoff - 450.0f) * velCurve);
        gain = 0.25f + 0.75f * velCurve;
    }

    double semitones = static_cast<double> (midiNote - z.rootKey) + static_cast<double> (pitchSemi) + (rrPitchCents / 100.0);
    v.pitchRatio = (z.sampleRate / currentSampleRate) * std::pow (2.0, semitones / 12.0);
    v.currentPos = static_cast<double> (startNorm * z.buffer.getNumSamples()) + rrStartOffsetSamples;

    v.gainL = gain * std::cos ((rrPan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * 1.414f;
    v.gainR = gain * std::sin ((rrPan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * 1.414f;

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
    for (auto& v : voices)
    {
        if (v.active && v.midiNote == midiNote)
            v.envStage = 3;
    }
}

void NoiseMu6ixRhythmAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    bool seqLoopOn = *apvts.getRawParameterValue ("seqLoop") > 0.5f;
    bool sampleLoopOn = *apvts.getRawParameterValue ("sampleLoop") > 0.5f;
    float vol = *apvts.getRawParameterValue ("volume");
    float endNorm = *apvts.getRawParameterValue ("end");
    float startNorm = *apvts.getRawParameterValue ("start");

    float attackSec = *apvts.getRawParameterValue ("attack");
    float decaySec = *apvts.getRawParameterValue ("decay");
    float sustainLvl = *apvts.getRawParameterValue ("sustain");
    float releaseSec = *apvts.getRawParameterValue ("release");

    int gridIdx = static_cast<int> (*apvts.getRawParameterValue ("grid"));
    double stepsPerBeat = (gridIdx == 0) ? 1.0 : (gridIdx == 1 ? 2.0 : 4.0);

    // Handle incoming MIDI
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            heldMidiNote = msg.getNoteNumber();
            heldVelocity = msg.getFloatVelocity();
            if (! seqLoopOn)
                triggerNote (heldMidiNote, heldVelocity);
            else
            {
                seqPhaseSamples = 0.0;
                currentSeqStep.store (0);
                float sv = stepVelocities[0];
                if (sv > 0.01f)
                    triggerNote (heldMidiNote, heldVelocity * sv);
            }
        }
        else if (msg.isNoteOff())
        {
            if (msg.getNoteNumber() == heldMidiNote)
                heldMidiNote = -1;
            releaseNote (msg.getNoteNumber());
        }
    }

    // Rhythm Sequencer stepping when seqLoopOn && a key is held
    if (seqLoopOn && heldMidiNote >= 0)
    {
        double bpm = 120.0;
        if (auto* ph = getPlayHead())
        {
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm())
                    bpm = *b;
        }
        double samplesPerStep = (60.0 / bpm) * currentSampleRate / stepsPerBeat;
        seqPhaseSamples += buffer.getNumSamples();
        if (seqPhaseSamples >= samplesPerStep)
        {
            seqPhaseSamples -= samplesPerStep;
            int nextStep = (currentSeqStep.load() + 1) % 16;
            currentSeqStep.store (nextStep);
            float sv = stepVelocities[static_cast<size_t> (nextStep)];
            if (sv > 0.01f)
                triggerNote (heldMidiNote, heldVelocity * sv);
        }
    }

    const juce::ScopedLock sl (zoneLock);
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getWritePointer (1);

    float attInc = 1.0f / std::max (1.0f, static_cast<float> (attackSec * currentSampleRate));
    float decDec = 1.0f / std::max (1.0f, static_cast<float> (decaySec * currentSampleRate));
    float relDec = 1.0f / std::max (1.0f, static_cast<float> (releaseSec * currentSampleRate));

    for (auto& v : voices)
    {
        if (! v.active || v.zoneIndex < 0 || v.zoneIndex >= static_cast<int> (zones.size()))
            continue;

        const auto& z = zones[static_cast<size_t> (v.zoneIndex)];
        int numSamples = z.buffer.getNumSamples();
        int startSample = static_cast<int> (startNorm * numSamples);
        int endSample = std::max (startSample + 1, static_cast<int> (endNorm * numSamples));
        const float* srcL = z.buffer.getReadPointer (0);
        const float* srcR = z.buffer.getNumChannels() > 1 ? z.buffer.getReadPointer (1) : srcL;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            int posInt = static_cast<int> (v.currentPos);
            if (posInt >= endSample - 1)
            {
                if (sampleLoopOn)
                {
                    v.currentPos = static_cast<double> (startSample);
                    posInt = startSample;
                }
                else
                {
                    v.active = false;
                    break;
                }
            }

            float frac = static_cast<float> (v.currentPos - posInt);
            float sL = srcL[posInt] + frac * (srcL[posInt + 1] - srcL[posInt]);
            float sR = srcR[posInt] + frac * (srcR[posInt + 1] - srcR[posInt]);

            // Envelope
            if (v.envStage == 0)
            {
                v.envVal += attInc;
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
    xml->setAttribute ("presetName", file.getFileName());

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
    }

    auto* seqXml = xml->createNewChildElement ("SEQUENCER");
    for (int i = 0; i < 16; ++i)
        seqXml->setAttribute ("step" + juce::String (i), stepVelocities[static_cast<size_t> (i)]);

    currentPresetName = file.getFileName();
    return xml->writeTo (file);
}

bool NoiseMu6ixRhythmAudioProcessor::loadPresetMn6x (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
        return false;

    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    currentPresetName = file.getFileName();

    if (auto* seqXml = xml->getChildByName ("SEQUENCER"))
    {
        for (int i = 0; i < 16; ++i)
            stepVelocities[static_cast<size_t> (i)] = static_cast<float> (seqXml->getDoubleAttribute ("step" + juce::String (i), 0.0));
    }

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
                loadSampleIntoZone (f,
                    zx->getIntAttribute ("lowKey", 36),
                    zx->getIntAttribute ("highKey", 84),
                    zx->getIntAttribute ("rootKey", 60),
                    zx->getIntAttribute ("lowVel", 0),
                    zx->getIntAttribute ("highVel", 127));
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
