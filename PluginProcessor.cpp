#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

NoiseMu6ixRhythmAudioProcessor::NoiseMu6ixRhythmAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    formatManager.registerBasicFormats();

    // Default starter rhythm events (free movable positions 0..1 across 1 bar)
    seqEvents = {
        { 0.000f, 0.95f },
        { 0.125f, 0.55f },
        { 0.250f, 0.85f },
        { 0.375f, 0.70f },
        { 0.500f, 0.95f },
        { 0.625f, 0.60f },
        { 0.750f, 0.85f },
        { 0.875f, 0.75f }
    };
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
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("loopStart", "Loop Start", 0.0f, 0.95f, 0.15f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("loopEnd", "Loop End", 0.05f, 1.0f, 0.85f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("pitch", "Pitch", -24.0f, 24.0f, 0.0f));
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

    // Includes standard + Triplets (1/12 = 1/8T, 1/24 = 1/16T)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "grid", "Grid",
        juce::StringArray { "1/4", "1/8", "1/12 (T)", "1/16", "1/24 (T)" }, 1));

    return { params.begin(), params.end() };
}

int NoiseMu6ixRhythmAudioProcessor::getGridDivisions() const
{
    int idx = static_cast<int> (*apvts.getRawParameterValue ("grid"));
    switch (idx)
    {
        case 0: return 4;   // 1/4
        case 1: return 8;   // 1/8
        case 2: return 12;  // 1/12 (8th triplet)
        case 3: return 16;  // 1/16
        case 4: return 24;  // 1/24 (16th triplet)
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
        float delta = (rng.nextFloat() * 2.0f - 1.0f) * 0.30f * velRandAmt;
        v.velocity = juce::jlimit (0.05f, 1.0f, v.velocity + delta);
    }

    // Fake Round Robin (FRR): distinct micro-pitch, transient start offset, stereo pan & filter tilt
    float rrPitchCents = 0.0f;
    float rrPan = 0.0f;
    float rrStartOffsetSamples = 0.0f;
    float rrFilterMult = 1.0f;

    if (frrOn)
    {
        rrCounter = (rrCounter + 1) % 8;
        static constexpr float rrPitchPattern[8] = { 0.0f, 10.0f, -8.5f, 14.0f, -12.0f, 6.5f, -5.5f, 16.0f };
        static constexpr float rrPanPattern[8]   = { 0.0f, 0.16f, -0.14f, 0.22f, -0.18f, 0.10f, -0.09f, 0.24f };
        static constexpr float rrFiltPattern[8]  = { 1.0f, 0.84f, 1.08f, 0.72f, 1.12f, 0.90f, 0.78f, 1.04f };

        rrPitchCents = rrPitchPattern[rrCounter] + (rng.nextFloat() * 5.0f - 2.5f);
        rrPan = rrPanPattern[rrCounter];
        rrStartOffsetSamples = rng.nextFloat() * static_cast<float> (currentSampleRate * 0.003);
        rrFilterMult = rrFiltPattern[rrCounter];
    }

    // Fake Velocity Layer (FVL): dynamic lowpass brightness + punch curve
    float fvlCutoff = baseCutoff;
    float gain = v.velocity;
    if (fvlOn)
    {
        float velCurve = std::pow (v.velocity, 1.7f);
        fvlCutoff = juce::jlimit (300.0f, 20000.0f, 380.0f + (baseCutoff - 380.0f) * velCurve);
        gain = 0.20f + 0.80f * velCurve;
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

void NoiseMu6ixRhythmAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    bool seqLoopOn = *apvts.getRawParameterValue ("seqLoop") > 0.5f;
    bool sampleLoopOn = *apvts.getRawParameterValue ("sampleLoop") > 0.5f;
    float vol = *apvts.getRawParameterValue ("volume");
    float startNorm = *apvts.getRawParameterValue ("start");
    float endNorm = *apvts.getRawParameterValue ("end");
    float loopStartNorm = *apvts.getRawParameterValue ("loopStart");
    float loopEndNorm = *apvts.getRawParameterValue ("loopEnd");

    float attackSec = *apvts.getRawParameterValue ("attack");
    float decaySec = *apvts.getRawParameterValue ("decay");
    float sustainLvl = *apvts.getRawParameterValue ("sustain");
    float releaseSec = *apvts.getRawParameterValue ("release");

    float shuffleAmt = *apvts.getRawParameterValue ("shuffle");
    float timeRandAmt = *apvts.getRawParameterValue ("timeRand");

    // Incoming MIDI
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            heldMidiNote = msg.getNoteNumber();
            heldVelocity = msg.getFloatVelocity();
            if (! seqLoopOn)
            {
                triggerNote (heldMidiNote, heldVelocity);
            }
            else
            {
                seqBarPhase = 0.0;
                seqPlayheadNorm.store (0.0f);
                const juce::ScopedLock sl (seqLock);
                for (const auto& ev : seqEvents)
                {
                    if (ev.position < 0.005f)
                        triggerNote (heldMidiNote, heldVelocity * ev.velocity);
                }
            }
        }
        else if (msg.isNoteOff())
        {
            if (msg.getNoteNumber() == heldMidiNote)
                heldMidiNote = -1;
            releaseNote (msg.getNoteNumber());
        }
    }

    // Free-position + Grid/Triplet Rhythm Sequencer when seqLoopOn && note held
    if (seqLoopOn && heldMidiNote >= 0)
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
        const juce::ScopedLock sl (seqLock);
        for (const auto& ev : seqEvents)
        {
            double evPos = static_cast<double> (ev.position);
            // Apply subtle shuffle to odd subdivisions
            int subIdx = static_cast<int> (std::round (evPos * divs));
            if (subIdx % 2 == 1 && shuffleAmt > 0.001f)
                evPos += (shuffleAmt * 0.35) / static_cast<double> (divs);
            if (timeRandAmt > 0.001f)
                evPos += (rng.nextFloat() * 2.0f - 1.0f) * 0.012 * timeRandAmt;
            evPos = std::fmod (evPos + 1.0, 1.0);

            bool triggered = (! wrapped && evPos > prevPhase && evPos <= seqBarPhase)
                          || (wrapped && (evPos > prevPhase || evPos <= seqBarPhase));
            if (triggered)
                triggerNote (heldMidiNote, heldVelocity * ev.velocity);
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
    xml->setAttribute ("presetName", currentPresetName);

    auto* zonesXml = xml->createNewChildElement ("ZONES");
    {
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
    }

    auto* seqXml = xml->createNewChildElement ("SEQ_EVENTS");
    {
        const juce::ScopedLock sl (seqLock);
        for (const auto& ev : seqEvents)
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

    if (auto* seqXml = xml->getChildByName ("SEQ_EVENTS"))
    {
        const juce::ScopedLock sl (seqLock);
        seqEvents.clear();
        for (auto* ex : seqXml->getChildIterator())
        {
            RhythmEvent ev;
            ev.position = static_cast<float> (ex->getDoubleAttribute ("pos", 0.0));
            ev.velocity = static_cast<float> (ex->getDoubleAttribute ("vel", 0.85));
            seqEvents.push_back (ev);
        }
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
                    zx->getIntAttribute ("lowKey", 60),
                    zx->getIntAttribute ("highKey", 60),
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
