#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>
#include <cctype>

juce::Colour NoiseMu6ixRhythmAudioProcessor::getPaletteColour (int idx)
{
    static const juce::Colour palette[numPaletteColours] = {
        juce::Colour (0xff4ca3ff), // 0: Blue (Default)
        juce::Colour (0xffe04f8a), // 1: Pink
        juce::Colour (0xffee5253), // 2: Red
        juce::Colour (0xffee7b38), // 3: Orange
        juce::Colour (0xffe8a138), // 4: Amber
        juce::Colour (0xffd9c838), // 5: Gold
        juce::Colour (0xff4cc98f), // 6: Emerald
        juce::Colour (0xff2eb872), // 7: Green
        juce::Colour (0xff38c4d9), // 8: Cyan
        juce::Colour (0xffa66cff), // 9: Purple
        juce::Colour (0xffc56cf0), // 10: Magenta
        juce::Colour (0xff9aa8bf)  // 11: Silver
    };
    return palette[juce::jlimit (0, numPaletteColours - 1, idx)];
}

juce::String NoiseMu6ixRhythmAudioProcessor::getPaletteColourName (int idx)
{
    static const char* names[numPaletteColours] = {
        "Blue (Default)", "Pink", "Red", "Orange", "Amber", "Gold",
        "Emerald", "Green", "Cyan", "Purple", "Magenta", "Silver"
    };
    return names[juce::jlimit (0, numPaletteColours - 1, idx)];
}

int NoiseMu6ixRhythmAudioProcessor::parseMidiNoteFromFilename (const juce::String& name)
{
    // Looks for tokens like C3, D#4, Fs2, Bb3, A0, etc. (where C5 = 60 in FL Studio / C0 = 0)
    auto upper = name.toUpperCase();
    int len = upper.length();
    for (int i = len - 1; i >= 0; --i)
    {
        juce::juce_wchar c = upper[i];
        int baseSemi = -1;
        if      (c == 'C') baseSemi = 0;
        else if (c == 'D') baseSemi = 2;
        else if (c == 'E') baseSemi = 4;
        else if (c == 'F') baseSemi = 5;
        else if (c == 'G') baseSemi = 7;
        else if (c == 'A') baseSemi = 9;
        else if (c == 'B') baseSemi = 11;
        else continue;

        // Require boundary before note letter (start of string or separator)
        if (i > 0 && std::isalnum (static_cast<unsigned char> (upper[i - 1])))
            continue;

        int pos = i + 1;
        int acc = 0;
        if (pos < len)
        {
            if (upper[pos] == '#' || upper[pos] == 'S') { acc = 1; ++pos; }
            else if (upper[pos] == 'B') { acc = -1; ++pos; }
        }
        if (pos < len && std::isdigit (static_cast<unsigned char> (upper[pos])))
        {
            int oct = static_cast<int> (upper[pos] - '0');
            if (pos + 1 < len && std::isdigit (static_cast<unsigned char> (upper[pos + 1])))
                oct = oct * 10 + static_cast<int> (upper[pos + 1] - '0');
            int note = oct * 12 + baseSemi + acc;
            if (note >= 0 && note <= 120)
                return note;
        }
    }
    return -1;
}

NoiseMu6ixRhythmAudioProcessor::NoiseMu6ixRhythmAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    formatManager.registerBasicFormats();
    heldNotesVel.fill (0.0f);
    groupRRCounters.fill (0);
    groups.push_back ({ "Group 1", false, 0, -1 });
    selectedGroupIndex = 0;
}

NoiseMu6ixRhythmAudioProcessor::~NoiseMu6ixRhythmAudioProcessor() = default;

std::vector<RhythmEvent> NoiseMu6ixRhythmAudioProcessor::createDefaultEvents() const
{
    return { { 0.000f, 0.95f }, { 0.250f, 0.85f }, { 0.500f, 0.95f }, { 0.750f, 0.85f } };
}

juce::AudioProcessorValueTreeState::ParameterLayout NoiseMu6ixRhythmAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterBool> ("frr", "FRR (Fake Round Robin)", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("fvl", "FVL (Fake Velocity Layer)", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("pitchTrack", "Pitch Track (Keys)", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("seqLoop", "Rhythm Loop", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("hostSync", "DAW Sync", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("internalBpm", "Internal BPM", 40.0f, 240.0f, 120.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("shuffle", "Shuffle", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("timeRand", "Time Randomize", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("velRand", "Velocity Randomize", 0.0f, 1.0f, 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "grid", "Grid",
        juce::StringArray { "1/4", "1/8", "1/12 (T)", "1/16", "1/24 (T)", "1/32", "1/48 (T)", "1/64" }, 1));

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
        case 5: return 32;
        case 6: return 48;
        case 7: return 64;
        default: return 8;
    }
}

void NoiseMu6ixRhythmAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    for (auto& v : voices)
    {
        v.filterL.reset();
        v.filterR.reset();
        v.active = false;
    }
}

void NoiseMu6ixRhythmAudioProcessor::releaseResources() {}

bool NoiseMu6ixRhythmAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

int NoiseMu6ixRhythmAudioProcessor::createEmptyGroup()
{
    const juce::ScopedLock sl (zoneLock);
    int nextNum = static_cast<int> (groups.size()) + 1;
    groups.push_back ({ "Group " + juce::String (nextNum), false, 0, -1 });
    selectedGroupIndex = static_cast<int> (groups.size()) - 1;
    return selectedGroupIndex;
}

void NoiseMu6ixRhythmAudioProcessor::deleteGroup (int groupIdx)
{
    const juce::ScopedLock sl (zoneLock);
    if (groups.size() <= 1 || groupIdx < 0 || groupIdx >= static_cast<int> (groups.size()))
        return;

    for (int i = static_cast<int> (zones.size()) - 1; i >= 0; --i)
    {
        if (zones[static_cast<size_t> (i)].groupIndex == groupIdx)
            deleteZone (i);
        else if (zones[static_cast<size_t> (i)].groupIndex > groupIdx)
            zones[static_cast<size_t> (i)].groupIndex--;
    }

    groups.erase (groups.begin() + groupIdx);
    selectedGroupIndex = juce::jlimit (0, static_cast<int> (groups.size()) - 1, selectedGroupIndex);
}

bool NoiseMu6ixRhythmAudioProcessor::isKeyswitchNote (int midiNote) const
{
    for (const auto& g : groups)
        if (g.keyswitchNote >= 0 && g.keyswitchNote == midiNote)
            return true;
    return false;
}

int NoiseMu6ixRhythmAudioProcessor::createEmptyZone (int key)
{
    const juce::ScopedLock sl (zoneLock);
    if (groups.empty())
    {
        groups.push_back ({ "Group 1", false, 0, -1 });
        selectedGroupIndex = 0;
    }
    int k = juce::jlimit (0, 120, key);
    SampleZone z;
    z.name = "Key " + juce::MidiMessage::getMidiNoteName (k, true, true, 5);
    z.groupIndex = juce::jlimit (0, static_cast<int> (groups.size()) - 1, selectedGroupIndex);
    z.lowKey = k;
    z.highKey = k;
    z.rootKey = k;
    z.lowVel = 0;
    z.highVel = 127;
    z.realRREnabled = true;
    z.seqEvents = createDefaultEvents();
    z.colourIndex = 0;
    z.colour = getPaletteColour (0);
    zones.push_back (std::move (z));
    selectedZoneIndex = static_cast<int> (zones.size()) - 1;
    return selectedZoneIndex;
}

bool NoiseMu6ixRhythmAudioProcessor::addSampleToZone (int zoneIndex, const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
        return false;

    SampleLayer layer;
    layer.name = file.getFileNameWithoutExtension();
    layer.file = file;
    layer.sampleRate = reader->sampleRate;
    layer.buffer.setSize (static_cast<int> (reader->numChannels), static_cast<int> (reader->lengthInSamples));
    reader->read (&layer.buffer, 0, static_cast<int> (reader->lengthInSamples), 0, true, true);

    const juce::ScopedLock sl (zoneLock);
    if (zoneIndex < 0 || zoneIndex >= static_cast<int> (zones.size()))
        return false;

    auto& z = zones[static_cast<size_t> (zoneIndex)];
    if (z.layers.empty())
        z.name = layer.name;
    z.layers.push_back (std::move (layer));
    z.selectedLayerIndex = static_cast<int> (z.layers.size()) - 1;
    if (z.layers.size() > 1)
        z.realRREnabled = true;
    return true;
}

bool NoiseMu6ixRhythmAudioProcessor::loadSampleIntoNewZone (const juce::File& file, int lowKey, int highKey, int rootKey, int lowVel, int highVel)
{
    int idx = createEmptyZone (rootKey);
    {
        const juce::ScopedLock sl (zoneLock);
        auto& z = zones[static_cast<size_t> (idx)];
        z.lowKey = juce::jlimit (0, 120, lowKey);
        z.highKey = juce::jlimit (z.lowKey, 120, highKey);
        z.rootKey = juce::jlimit (0, 120, rootKey);
        z.lowVel = juce::jlimit (0, 127, lowVel);
        z.highVel = juce::jlimit (z.lowVel, 127, highVel);
    }
    return addSampleToZone (idx, file);
}

void NoiseMu6ixRhythmAudioProcessor::deleteZone (int index)
{
    const juce::ScopedLock sl (zoneLock);
    if (index >= 0 && index < static_cast<int> (zones.size()))
    {
        for (auto& v : voices)
            if (v.zoneIndex == index) v.active = false;
            else if (v.zoneIndex > index) v.zoneIndex--;
        zones.erase (zones.begin() + index);
        if (zones.empty())
            selectedZoneIndex = -1;
        else
            selectedZoneIndex = juce::jlimit (0, static_cast<int> (zones.size()) - 1, selectedZoneIndex);
    }
}

void NoiseMu6ixRhythmAudioProcessor::deleteZones (std::vector<int> indices)
{
    std::sort (indices.begin(), indices.end(), std::greater<int>());
    indices.erase (std::unique (indices.begin(), indices.end()), indices.end());
    for (int idx : indices)
        deleteZone (idx);
}

int NoiseMu6ixRhythmAudioProcessor::duplicateZone (int index)
{
    const juce::ScopedLock sl (zoneLock);
    if (index < 0 || index >= static_cast<int> (zones.size()))
        return -1;
    SampleZone copy = zones[static_cast<size_t> (index)];
    copy.name = copy.name + " Copy";
    zones.push_back (std::move (copy));
    selectedZoneIndex = static_cast<int> (zones.size()) - 1;
    return selectedZoneIndex;
}

void NoiseMu6ixRhythmAudioProcessor::normalizeSelectedSample()
{
    const juce::ScopedLock sl (zoneLock);
    if (selectedZoneIndex < 0 || selectedZoneIndex >= static_cast<int> (zones.size())) return;
    auto& z = zones[static_cast<size_t> (selectedZoneIndex)];
    if (z.layers.empty()) return;
    int lIdx = juce::jlimit (0, static_cast<int> (z.layers.size()) - 1, z.selectedLayerIndex);
    auto& buf = z.layers[static_cast<size_t> (lIdx)].buffer;
    float peak = buf.getMagnitude (0, buf.getNumSamples());
    if (peak > 1e-5f)
        buf.applyGain (0.98f / peak);
}

bool NoiseMu6ixRhythmAudioProcessor::isGroupActiveForNote (int groupIdx) const
{
    if (groupIdx < 0 || groupIdx >= static_cast<int> (groups.size()))
        return false;
    const auto& g = groups[static_cast<size_t> (groupIdx)];
    if (g.muted)
        return false;
    if (groupSoloEnabled.load() && groupIdx != selectedGroupIndex)
        return false;

    // Keyswitch filter: if group has a keyswitch assigned, it only plays when that keyswitch is active
    if (g.keyswitchNote >= 0)
    {
        int activeKs = activeKeyswitchNote.load();
        if (activeKs >= 0 && g.keyswitchNote != activeKs)
            return false;
        // If no keyswitch has been pressed yet, allow the first keyswitch group to sound as default
        if (activeKs < 0)
        {
            for (int i = 0; i < static_cast<int> (groups.size()); ++i)
            {
                if (groups[static_cast<size_t> (i)].keyswitchNote >= 0)
                    return groups[static_cast<size_t> (i)].keyswitchNote == g.keyswitchNote;
            }
        }
    }
    return true;
}

bool NoiseMu6ixRhythmAudioProcessor::isZoneAudible (int zoneIdx) const
{
    if (zoneIdx < 0 || zoneIdx >= static_cast<int> (zones.size()))
        return false;
    const auto& z = zones[static_cast<size_t> (zoneIdx)];
    if (! isGroupActiveForNote (z.groupIndex))
        return false;
    if (zoneSoloEnabled.load() && zoneIdx != selectedZoneIndex)
        return false;
    return true;
}

void NoiseMu6ixRhythmAudioProcessor::startVoice (int zoneIdx, int midiNote, float velocity)
{
    if (! isZoneAudible (zoneIdx))
        return;

    auto& z = zones[static_cast<size_t> (zoneIdx)];
    if (z.layers.empty())
        return;

    int layerIdx = juce::jlimit (0, static_cast<int> (z.layers.size()) - 1, z.selectedLayerIndex);
    if (z.layers.size() > 1 && z.realRREnabled)
    {
        layerIdx = z.rrPlayIndex % static_cast<int> (z.layers.size());
        z.rrPlayIndex = (z.rrPlayIndex + 1) % static_cast<int> (z.layers.size());
    }

    const auto& layer = z.layers[static_cast<size_t> (layerIdx)];
    int totalSamps = layer.buffer.getNumSamples();
    if (totalSamps < 2)
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
    v.layerIndex = layerIdx;
    v.midiNote = midiNote;
    v.velocity = juce::jlimit (0.04f, 1.0f, velocity);
    v.envVal = 0.0f;
    v.envStage = 0;
    v.playingReverse = z.reverse;
    v.inLoopRegion = false;
    v.noteReleased = false;
    v.loopsCompleted = 0;

    bool frrOn = (apvts.getRawParameterValue ("frr")->load() > 0.5f) && (z.layers.size() == 1);
    bool fvlOn = apvts.getRawParameterValue ("fvl")->load() > 0.5f;
    bool pitchTrackOn = apvts.getRawParameterValue ("pitchTrack")->load() > 0.5f;
    float velRandAmt = apvts.getRawParameterValue ("velRand")->load();

    if (velRandAmt > 0.001f)
    {
        float delta = (rng.nextFloat() * 2.0f - 1.0f) * 0.30f * velRandAmt;
        v.velocity = juce::jlimit (0.04f, 1.0f, v.velocity + delta);
    }

    // 1000+ Generative FRR (NO Panning)
    float rrPitchCents = 0.0f;
    float rrStartOffsetSamples = 0.0f;
    float rrFilterMult = 1.0f;
    float rrGainMult = 1.0f;
    float rrExtraAttackSec = 0.0f;

    if (frrOn)
    {
        ++rrSeedCounter;
        double n = static_cast<double> (rrSeedCounter);
        float u1 = static_cast<float> (std::fmod (n * 0.61803398875 + rng.nextFloat() * 0.13, 1.0));
        float u3 = static_cast<float> (std::fmod (n * 0.56984029099 + rng.nextFloat() * 0.19, 1.0));
        float u4 = static_cast<float> (std::fmod (n * 0.43015970900 + rng.nextFloat() * 0.11, 1.0));
        float u5 = static_cast<float> (std::fmod (n * 0.32471795724 + rng.nextFloat() * 0.23, 1.0));

        rrPitchCents = (u1 * 2.0f - 1.0f) * 18.0f;
        if (std::abs (rrPitchCents - lastRrPitch) < 4.5f)
            rrPitchCents = -rrPitchCents;
        lastRrPitch = rrPitchCents;

        rrStartOffsetSamples = u3 * static_cast<float> (currentSampleRate * 0.0042);
        rrFilterMult = 0.68f + u4 * 0.50f;
        rrGainMult = 0.88f + u5 * 0.24f;
        rrExtraAttackSec = u3 * 0.0045f;
    }

    // Continuous FVL
    float fvlCutoff = z.cutoffHz;
    float gain = v.velocity;
    float fvlExtraAttackSec = 0.0f;
    v.driveAmt = 1.0f;

    if (fvlOn)
    {
        float vel = v.velocity;
        float velCurve = std::pow (vel, 1.75f);
        fvlCutoff = juce::jlimit (280.0f, 20000.0f, 320.0f + (z.cutoffHz - 320.0f) * velCurve);
        gain = 0.16f + 0.84f * velCurve;
        fvlExtraAttackSec = (1.0f - vel) * (1.0f - vel) * 0.014f;
        v.driveAmt = 1.0f + std::max (0.0f, vel - 0.55f) * 2.4f;
    }

    // Per-Zone ADSR
    float totalAttackSec = std::max (0.0005f, z.attackSec + rrExtraAttackSec + fvlExtraAttackSec);
    v.attackInc = 1.0f / std::max (1.0f, static_cast<float> (totalAttackSec * currentSampleRate));
    v.decayDec = 1.0f / std::max (1.0f, static_cast<float> (z.decaySec * currentSampleRate));
    v.sustainLvl = z.sustainLvl;
    v.releaseDec = 1.0f / std::max (1.0f, static_cast<float> (z.releaseSec * currentSampleRate));

    double noteSemiOffset = pitchTrackOn ? static_cast<double> (midiNote - z.rootKey) : 0.0;
    double semitones = noteSemiOffset + static_cast<double> (z.pitchSemi) + (rrPitchCents / 100.0);
    v.pitchRatio = (layer.sampleRate / currentSampleRate) * std::pow (2.0, semitones / 12.0);
    v.loopPitchRatio = (layer.sampleRate / currentSampleRate) * std::pow (2.0, (semitones + z.loopTuneCents / 100.0) / 12.0);

    int startSamp = static_cast<int> (z.startNorm * totalSamps);
    int endSamp = std::max (startSamp + 2, static_cast<int> (z.endNorm * totalSamps));
    if (z.reverse)
        v.currentPos = static_cast<double> (std::max (startSamp, endSamp - 2));
    else
        v.currentPos = static_cast<double> (startSamp) + rrStartOffsetSamples;

    float finalPan = juce::jlimit (-1.0f, 1.0f, z.pan);
    float panAngle = (finalPan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    float finalGain = gain * rrGainMult * z.volume;
    v.gainL = finalGain * std::cos (panAngle) * 1.41421356f;
    v.gainR = finalGain * std::sin (panAngle) * 1.41421356f;

    v.filterL.reset();
    v.filterR.reset();
    float finalCut = juce::jlimit (80.0f, 20000.0f, fvlCutoff * rrFilterMult);
    v.filterL.setParams (finalCut, z.resQ, currentSampleRate);
    v.filterR.setParams (finalCut, z.resQ, currentSampleRate);
}

void NoiseMu6ixRhythmAudioProcessor::triggerNote (int midiNote, float velocity)
{
    lastPlayedNote.store (midiNote);
    const juce::ScopedLock sl (zoneLock);

    // Check if this MIDI note is a Keyswitch!
    if (isKeyswitchNote (midiNote))
    {
        activeKeyswitchNote.store (midiNote);
        for (int i = 0; i < static_cast<int> (groups.size()); ++i)
        {
            if (groups[static_cast<size_t> (i)].keyswitchNote == midiNote)
            {
                selectedGroupIndex = i;
                break;
            }
        }
        return; // Keyswitch switches articulation without triggering audio
    }

    int velInt = juce::jlimit (0, 127, static_cast<int> (velocity * 127.0f));

    // Group-Level Round Robin selection per RR Set (1..8)
    std::array<int, 9> chosenGroupForRRSet;
    chosenGroupForRRSet.fill (-1);
    for (int rrSet = 1; rrSet <= 8; ++rrSet)
    {
        std::vector<int> candidates;
        for (int gIdx = 0; gIdx < static_cast<int> (groups.size()); ++gIdx)
        {
            if (groups[static_cast<size_t> (gIdx)].rrSetId == rrSet && isGroupActiveForNote (gIdx))
                candidates.push_back (gIdx);
        }
        if (! candidates.empty())
        {
            int pick = groupRRCounters[static_cast<size_t> (rrSet)] % static_cast<int> (candidates.size());
            groupRRCounters[static_cast<size_t> (rrSet)] = (pick + 1) % static_cast<int> (candidates.size());
            chosenGroupForRRSet[static_cast<size_t> (rrSet)] = candidates[static_cast<size_t> (pick)];
        }
    }

    for (int i = 0; i < static_cast<int> (zones.size()); ++i)
    {
        const auto& z = zones[static_cast<size_t> (i)];
        if (midiNote >= z.lowKey && midiNote <= z.highKey && velInt >= z.lowVel && velInt <= z.highVel)
        {
            if (z.groupIndex >= 0 && z.groupIndex < static_cast<int> (groups.size()))
            {
                int rrSet = groups[static_cast<size_t> (z.groupIndex)].rrSetId;
                if (rrSet >= 1 && rrSet <= 8 && chosenGroupForRRSet[static_cast<size_t> (rrSet)] != z.groupIndex)
                    continue; // Skip non-active RR group for this hit
            }
            startVoice (i, midiNote, velocity);
            if (selectByMidi.load())
            {
                selectedZoneIndex = i;
                selectedGroupIndex = juce::jlimit (0, static_cast<int> (groups.size()) - 1, z.groupIndex);
            }
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
        {
            v.noteReleased = true;
            // In UntilRelease loop mode, let the sample finish naturally after exiting loop; otherwise enter release envelope
            bool isUntilRel = false;
            if (v.zoneIndex >= 0 && v.zoneIndex < static_cast<int> (zones.size()))
                isUntilRel = (zones[static_cast<size_t> (v.zoneIndex)].sampleLoop && zones[static_cast<size_t> (v.zoneIndex)].loopMode == 2);
            if (! isUntilRel)
                v.envStage = 3;
        }
    }
}

bool NoiseMu6ixRhythmAudioProcessor::hasAnyHeldNote() const
{
    for (float v : heldNotesVel)
        if (v > 0.0f) return true;
    return false;
}

void NoiseMu6ixRhythmAudioProcessor::uiNoteOn (int midiNote, float velocity)
{
    {
        const juce::ScopedLock sl (zoneLock);
        if (isKeyswitchNote (midiNote))
        {
            activeKeyswitchNote.store (midiNote);
            for (int i = 0; i < static_cast<int> (groups.size()); ++i)
                if (groups[static_cast<size_t> (i)].keyswitchNote == midiNote)
                {
                    selectedGroupIndex = i;
                    break;
                }
            return;
        }
    }

    bool seqLoopOn = apvts.getRawParameterValue ("seqLoop")->load() > 0.5f;
    bool wasIdle = ! hasAnyHeldNote() && ! internalPlaying.load();

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
                if (selectByMidi.load())
                {
                    selectedZoneIndex = i;
                    selectedGroupIndex = juce::jlimit (0, static_cast<int> (groups.size()) - 1, z.groupIndex);
                }
                for (const auto& ev : z.seqEvents)
                    if (ev.position < 0.004f)
                        startVoice (i, midiNote, velocity * ev.velocity);
            }
        }
    }
}

void NoiseMu6ixRhythmAudioProcessor::uiNoteOff (int midiNote)
{
    if (midiNote >= 0 && midiNote < 128)
        heldNotesVel[static_cast<size_t> (midiNote)] = 0.0f;
    releaseNote (midiNote);
}

void NoiseMu6ixRhythmAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    int numSamples = buffer.getNumSamples();
    if (numSamples <= 0) return;

    bool seqLoopOn = apvts.getRawParameterValue ("seqLoop")->load() > 0.5f;
    bool hostSyncOn = apvts.getRawParameterValue ("hostSync")->load() > 0.5f;
    double bpm = static_cast<double> (apvts.getRawParameterValue ("internalBpm")->load());
    bool dawPlaying = false;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (hostSyncOn)
            {
                if (auto b = pos->getBpm()) bpm = *b;
                dawPlaying = pos->getIsPlaying();
            }
        }
    }

    float shuffleAmt = apvts.getRawParameterValue ("shuffle")->load();
    float timeRandAmt = apvts.getRawParameterValue ("timeRand")->load();

    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            uiNoteOn (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            uiNoteOff (msg.getNoteNumber());
    }

    bool transportAllPlay = seqLoopOn && (internalPlaying.load() || (hostSyncOn && dawPlaying));
    bool anyMidiHeld = seqLoopOn && hasAnyHeldNote();
    bool shouldRunSeq = transportAllPlay || anyMidiHeld;

    if (transportAllPlay && ! wasTransportPlaying)
    {
        seqBarPhase = 0.0;
        seqPlayheadNorm.store (0.0f);
        const juce::ScopedLock sl (zoneLock);
        for (int zIdx = 0; zIdx < static_cast<int> (zones.size()); ++zIdx)
            for (const auto& ev : zones[static_cast<size_t> (zIdx)].seqEvents)
                if (ev.position < 0.004f)
                    startVoice (zIdx, zones[static_cast<size_t> (zIdx)].rootKey, 0.9f * ev.velocity);
    }
    wasTransportPlaying = transportAllPlay;

    if (shouldRunSeq)
    {
        double samplesPerBar = (60.0 / std::max (20.0, bpm)) * 4.0 * currentSampleRate;
        double prevPhase = seqBarPhase;
        double deltaPhase = static_cast<double> (numSamples) / std::max (1.0, samplesPerBar);
        seqBarPhase += deltaPhase;
        bool wrapped = false;
        if (seqBarPhase >= 1.0)
        {
            seqBarPhase -= 1.0;
            wrapped = true;
        }
        seqPlayheadNorm.store (static_cast<float> (seqBarPhase));

        int divs = getGridDivisions();
        const juce::ScopedLock sl (zoneLock);
        for (int zIdx = 0; zIdx < static_cast<int> (zones.size()); ++zIdx)
        {
            const auto& z = zones[static_cast<size_t> (zIdx)];
            if (z.layers.empty()) continue;

            int triggerMidi = -1;
            float triggerVel = 0.0f;

            if (transportAllPlay)
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

            if (triggerMidi < 0) continue;

            for (const auto& ev : z.seqEvents)
            {
                double evPos = static_cast<double> (ev.position);
                int subIdx = static_cast<int> (std::round (evPos * divs));
                if (subIdx % 2 == 1 && shuffleAmt > 0.001f)
                    evPos += (shuffleAmt * 0.35) / static_cast<double> (divs);
                if (timeRandAmt > 0.001f)
                    evPos += (rng.nextFloat() * 2.0f - 1.0f) * 0.010 * timeRandAmt;
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

    for (auto& v : voices)
    {
        if (! v.active || v.zoneIndex < 0 || v.zoneIndex >= static_cast<int> (zones.size()))
            continue;

        const auto& z = zones[static_cast<size_t> (v.zoneIndex)];
        if (v.layerIndex < 0 || v.layerIndex >= static_cast<int> (z.layers.size()))
            continue;

        const auto& layer = z.layers[static_cast<size_t> (v.layerIndex)];
        int totalSamps = layer.buffer.getNumSamples();
        if (totalSamps < 2) continue;

        int startSample = static_cast<int> (z.startNorm * totalSamps);
        int endSample = std::max (startSample + 2, static_cast<int> (z.endNorm * totalSamps));
        int lStartSample = juce::jlimit (startSample, endSample - 2, static_cast<int> (z.loopStartNorm * totalSamps));
        int lEndSample = juce::jlimit (lStartSample + 2, endSample, static_cast<int> (z.loopEndNorm * totalSamps));
        int loopLen = std::max (2, lEndSample - lStartSample);
        int xfadeSamps = static_cast<int> (z.loopCrossfade * loopLen);
        int activeLen = std::max (2, endSample - startSample);
        int fadeInSamps = static_cast<int> (z.fadeInNorm * activeLen);
        int fadeOutSamps = static_cast<int> (z.fadeOutNorm * activeLen);

        const float* srcL = layer.buffer.getReadPointer (0);
        const float* srcR = layer.buffer.getNumChannels() > 1 ? layer.buffer.getReadPointer (1) : srcL;

        for (int i = 0; i < numSamples; ++i)
        {
            int posInt = static_cast<int> (v.currentPos);
            bool loopAllowed = z.sampleLoop && (z.loopCount == 0 || v.loopsCompleted < z.loopCount)
                               && ! (z.loopMode == 2 && v.noteReleased);

            if (loopAllowed && ! z.reverse)
            {
                if (! v.playingReverse && posInt >= lEndSample - 1)
                {
                    v.inLoopRegion = true;
                    v.loopsCompleted++;
                    if (z.loopMode == 1) // Ping-Pong
                    {
                        v.playingReverse = true;
                        v.currentPos = static_cast<double> (lEndSample - 2);
                        posInt = lEndSample - 2;
                    }
                    else
                    {
                        v.currentPos = static_cast<double> (lStartSample);
                        posInt = lStartSample;
                    }
                }
                else if (v.playingReverse && posInt <= lStartSample)
                {
                    v.playingReverse = false;
                    v.currentPos = static_cast<double> (lStartSample);
                    posInt = lStartSample;
                }
            }

            if (posInt < startSample || posInt >= endSample - 1 || posInt >= totalSamps - 1)
            {
                v.active = false;
                break;
            }

            float frac = static_cast<float> (v.currentPos - posInt);
            float sL = srcL[posInt] + frac * (srcL[posInt + 1] - srcL[posInt]);
            float sR = srcR[posInt] + frac * (srcR[posInt + 1] - srcR[posInt]);

            // Smooth Loop Crossfade near loop end (eliminates clicks on strings/sustains!)
            if (loopAllowed && z.loopMode != 1 && xfadeSamps > 8 && posInt >= lEndSample - xfadeSamps)
            {
                float alpha = static_cast<float> (posInt - (lEndSample - xfadeSamps)) / static_cast<float> (xfadeSamps);
                int altPos = juce::jlimit (startSample, totalSamps - 2, lStartSample + (posInt - (lEndSample - xfadeSamps)));
                sL = sL * (1.0f - alpha) + srcL[altPos] * alpha;
                sR = sR * (1.0f - alpha) + srcR[altPos] * alpha;
            }

            // Sample Fade In / Fade Out
            float fadeGain = 1.0f;
            if (fadeInSamps > 4 && (posInt - startSample) < fadeInSamps)
                fadeGain *= static_cast<float> (posInt - startSample) / static_cast<float> (fadeInSamps);
            if (fadeOutSamps > 4 && (endSample - posInt) < fadeOutSamps)
                fadeGain *= static_cast<float> (endSample - posInt) / static_cast<float> (fadeOutSamps);

            if (v.envStage == 0)
            {
                v.envVal += v.attackInc;
                if (v.envVal >= 1.0f) { v.envVal = 1.0f; v.envStage = 1; }
            }
            else if (v.envStage == 1)
            {
                v.envVal -= v.decayDec;
                if (v.envVal <= v.sustainLvl) { v.envVal = v.sustainLvl; v.envStage = 2; }
            }
            else if (v.envStage == 3)
            {
                v.envVal -= v.releaseDec;
                if (v.envVal <= 0.0001f) { v.envVal = 0.0f; v.active = false; break; }
            }

            sL = v.filterL.processLP (sL);
            sR = v.filterR.processLP (sR);

            if (v.driveAmt > 1.01f)
            {
                sL = std::tanh (sL * v.driveAmt) / std::tanh (v.driveAmt);
                sR = std::tanh (sR * v.driveAmt) / std::tanh (v.driveAmt);
            }

            float amp = v.envVal * fadeGain;
            outL[i] += sL * amp * v.gainL;
            outR[i] += sR * amp * v.gainR;

            double stepRatio = v.inLoopRegion ? v.loopPitchRatio : v.pitchRatio;
            v.currentPos += (v.playingReverse ? -stepRatio : stepRatio);
        }
    }
}

bool NoiseMu6ixRhythmAudioProcessor::savePresetMn6x (const juce::File& file)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("presetName", currentPresetName);

    const juce::ScopedLock sl (zoneLock);

    auto* groupsXml = xml->createNewChildElement ("GROUPS");
    for (const auto& g : groups)
    {
        auto* gx = groupsXml->createNewChildElement ("GROUP");
        gx->setAttribute ("name", g.name);
        gx->setAttribute ("muted", g.muted ? 1 : 0);
        gx->setAttribute ("rrSet", g.rrSetId);
        gx->setAttribute ("ksNote", g.keyswitchNote);
    }

    auto* zonesXml = xml->createNewChildElement ("ZONES");
    for (const auto& z : zones)
    {
        auto* zx = zonesXml->createNewChildElement ("ZONE");
        zx->setAttribute ("name", z.name);
        zx->setAttribute ("groupIdx", z.groupIndex);
        zx->setAttribute ("lowKey", z.lowKey);
        zx->setAttribute ("highKey", z.highKey);
        zx->setAttribute ("rootKey", z.rootKey);
        zx->setAttribute ("lowVel", z.lowVel);
        zx->setAttribute ("highVel", z.highVel);
        zx->setAttribute ("colIdx", z.colourIndex);
        zx->setAttribute ("realRR", z.realRREnabled ? 1 : 0);

        zx->setAttribute ("start", z.startNorm);
        zx->setAttribute ("end", z.endNorm);
        zx->setAttribute ("lStart", z.loopStartNorm);
        zx->setAttribute ("lEnd", z.loopEndNorm);
        zx->setAttribute ("sLoop", z.sampleLoop ? 1 : 0);
        zx->setAttribute ("lMode", z.loopMode);
        zx->setAttribute ("lXfade", z.loopCrossfade);
        zx->setAttribute ("lTune", z.loopTuneCents);
        zx->setAttribute ("lCount", z.loopCount);
        zx->setAttribute ("rev", z.reverse ? 1 : 0);
        zx->setAttribute ("fIn", z.fadeInNorm);
        zx->setAttribute ("fOut", z.fadeOutNorm);
        zx->setAttribute ("pitch", z.pitchSemi);
        zx->setAttribute ("pan", z.pan);
        zx->setAttribute ("vol", z.volume);
        zx->setAttribute ("atk", z.attackSec);
        zx->setAttribute ("dec", z.decaySec);
        zx->setAttribute ("sus", z.sustainLvl);
        zx->setAttribute ("rel", z.releaseSec);
        zx->setAttribute ("cut", z.cutoffHz);
        zx->setAttribute ("res", z.resQ);

        auto* layersXml = zx->createNewChildElement ("LAYERS");
        for (const auto& l : z.layers)
        {
            auto* lx = layersXml->createNewChildElement ("LAYER");
            lx->setAttribute ("file", l.file.getFullPathName());
        }

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
    if (xml == nullptr) return false;

    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    currentPresetName = xml->getStringAttribute ("presetName", file.getFileNameWithoutExtension());
    currentPresetFile = file;

    if (auto* groupsXml = xml->getChildByName ("GROUPS"))
    {
        const juce::ScopedLock sl (zoneLock);
        groups.clear();
        for (auto* gx : groupsXml->getChildIterator())
        {
            SampleGroup g;
            g.name = gx->getStringAttribute ("name", "Group");
            g.muted = gx->getIntAttribute ("muted", 0) != 0;
            g.rrSetId = gx->getIntAttribute ("rrSet", 0);
            g.keyswitchNote = gx->getIntAttribute ("ksNote", -1);
            groups.push_back (g);
        }
        if (groups.empty())
            groups.push_back ({ "Group 1", false, 0, -1 });
        selectedGroupIndex = 0;
    }

    if (auto* zonesXml = xml->getChildByName ("ZONES"))
    {
        {
            const juce::ScopedLock sl (zoneLock);
            zones.clear();
        }
        for (auto* zx : zonesXml->getChildIterator())
        {
            int idx = createEmptyZone (zx->getIntAttribute ("rootKey", 60));
            {
                const juce::ScopedLock sl (zoneLock);
                auto& z = zones[static_cast<size_t> (idx)];
                z.name = zx->getStringAttribute ("name", z.name);
                z.groupIndex = juce::jlimit (0, std::max (0, static_cast<int> (groups.size()) - 1), zx->getIntAttribute ("groupIdx", 0));
                z.lowKey = zx->getIntAttribute ("lowKey", 60);
                z.highKey = zx->getIntAttribute ("highKey", 60);
                z.rootKey = zx->getIntAttribute ("rootKey", 60);
                z.lowVel = zx->getIntAttribute ("lowVel", 0);
                z.highVel = zx->getIntAttribute ("highVel", 127);
                z.colourIndex = zx->getIntAttribute ("colIdx", 0);
                z.colour = getPaletteColour (z.colourIndex);
                z.realRREnabled = zx->getIntAttribute ("realRR", 1) != 0;

                z.startNorm = static_cast<float> (zx->getDoubleAttribute ("start", 0.0));
                z.endNorm = static_cast<float> (zx->getDoubleAttribute ("end", 1.0));
                z.loopStartNorm = static_cast<float> (zx->getDoubleAttribute ("lStart", 0.15));
                z.loopEndNorm = static_cast<float> (zx->getDoubleAttribute ("lEnd", 0.85));
                z.sampleLoop = zx->getIntAttribute ("sLoop", 0) != 0;
                z.loopMode = zx->getIntAttribute ("lMode", 0);
                z.loopCrossfade = static_cast<float> (zx->getDoubleAttribute ("lXfade", 0.05));
                z.loopTuneCents = static_cast<float> (zx->getDoubleAttribute ("lTune", 0.0));
                z.loopCount = zx->getIntAttribute ("lCount", 0);
                z.reverse = zx->getIntAttribute ("rev", 0) != 0;
                z.fadeInNorm = static_cast<float> (zx->getDoubleAttribute ("fIn", 0.0));
                z.fadeOutNorm = static_cast<float> (zx->getDoubleAttribute ("fOut", 0.0));
                z.pitchSemi = static_cast<float> (zx->getDoubleAttribute ("pitch", 0.0));
                z.pan = static_cast<float> (zx->getDoubleAttribute ("pan", 0.0));
                z.volume = static_cast<float> (zx->getDoubleAttribute ("vol", 1.0));
                z.attackSec = static_cast<float> (zx->getDoubleAttribute ("atk", 0.005));
                z.decaySec = static_cast<float> (zx->getDoubleAttribute ("dec", 0.30));
                z.sustainLvl = static_cast<float> (zx->getDoubleAttribute ("sus", 0.85));
                z.releaseSec = static_cast<float> (zx->getDoubleAttribute ("rel", 0.25));
                z.cutoffHz = static_cast<float> (zx->getDoubleAttribute ("cut", 18000.0));
                z.resQ = static_cast<float> (zx->getDoubleAttribute ("res", 0.707));
            }
            if (auto* layersXml = zx->getChildByName ("LAYERS"))
            {
                for (auto* lx : layersXml->getChildIterator())
                {
                    juce::File f (lx->getStringAttribute ("file"));
                    if (f.existsAsFile())
                        addSampleToZone (idx, f);
                }
            }
            if (auto* seqXml = zx->getChildByName ("SEQ_EVENTS"))
            {
                const juce::ScopedLock sl (zoneLock);
                auto& z = zones[static_cast<size_t> (idx)];
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
