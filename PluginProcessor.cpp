#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

void SimpleFxSlot::prepare (double sr)
{
    fL.reset(); fR.reset();
    env = 0.0f;
    if (type == SimpleFxType::Delay || type == SimpleFxType::Reverb)
    {
        int len = static_cast<int> (sr * 1.2) + 64;
        if (static_cast<int> (bufL.size()) < len)
        {
            bufL.assign (static_cast<size_t> (len), 0.0f);
            bufR.assign (static_cast<size_t> (len), 0.0f);
        }
        writePos = 0;
    }
}

juce::String SimpleFxSlot::getTypeName (SimpleFxType t)
{
    switch (t)
    {
        case SimpleFxType::Off:        return "FX Off";
        case SimpleFxType::EQ:         return "EQ";
        case SimpleFxType::Compressor: return "Comp";
        case SimpleFxType::Saturation: return "Sat";
        case SimpleFxType::Delay:      return "Delay";
        case SimpleFxType::Reverb:     return "Reverb";
        default:                       return "FX Off";
    }
}

juce::String SimpleFxSlot::getParamLabel (SimpleFxType t, int idx)
{
    switch (t)
    {
        case SimpleFxType::EQ:         return idx == 0 ? "Low"    : (idx == 1 ? "High"   : "Gain");
        case SimpleFxType::Compressor: return idx == 0 ? "Thresh" : (idx == 1 ? "Ratio"  : "Gain");
        case SimpleFxType::Saturation: return idx == 0 ? "Drive"  : (idx == 1 ? "Tone"   : "Mix");
        case SimpleFxType::Delay:      return idx == 0 ? "Time"   : (idx == 1 ? "Feedbk" : "Mix");
        case SimpleFxType::Reverb:     return idx == 0 ? "Size"   : (idx == 1 ? "Damp"   : "Mix");
        default:                       return "P" + juce::String (idx + 1);
    }
}

void SimpleFxSlot::processStereo (float* L, float* R, int numSamples, double sr)
{
    if (! enabled || type == SimpleFxType::Off)
        return;

    if (type == SimpleFxType::EQ)
    {
        float lowHz = 40.0f + p1 * 560.0f;
        float highHz = 1200.0f + p2 * 18000.0f;
        float gain = 0.5f + p3 * 1.5f;
        fL.setParams (highHz, 0.707f, sr);
        fR.setParams (highHz, 0.707f, sr);
        for (int i = 0; i < numSamples; ++i)
        {
            float hpL = fL.processLP (L[i]) * gain;
            float hpR = fR.processLP (R[i]) * gain;
            float lowAtt = juce::jlimit (0.0f, 0.35f, (lowHz - 40.0f) / 1600.0f);
            L[i] = hpL * (1.0f - lowAtt);
            R[i] = hpR * (1.0f - lowAtt);
        }
    }
    else if (type == SimpleFxType::Compressor)
    {
        float thresh = 0.08f + (1.0f - p1) * 0.82f;
        float ratio = 1.0f + p2 * 8.0f;
        float gain = 0.6f + p3 * 1.4f;
        for (int i = 0; i < numSamples; ++i)
        {
            float pk = std::max (std::abs (L[i]), std::abs (R[i]));
            env += (pk > env ? 0.08f : 0.002f) * (pk - env);
            float gr = (env > thresh && env > 1e-5f) ? std::pow (env / thresh, (1.0f / ratio) - 1.0f) : 1.0f;
            L[i] *= gr * gain;
            R[i] *= gr * gain;
        }
    }
    else if (type == SimpleFxType::Saturation)
    {
        float drive = 1.0f + p1 * 8.0f;
        float norm = 1.0f / std::tanh (drive);
        float cutHz = 1500.0f + p2 * 16500.0f;
        float mix = p3;
        fL.setParams (cutHz, 0.707f, sr);
        fR.setParams (cutHz, 0.707f, sr);
        for (int i = 0; i < numSamples; ++i)
        {
            float sL = fL.processLP (std::tanh (L[i] * drive) * norm);
            float sR = fR.processLP (std::tanh (R[i] * drive) * norm);
            L[i] = L[i] * (1.0f - mix) + sL * mix;
            R[i] = R[i] * (1.0f - mix) + sR * mix;
        }
    }
    else if (type == SimpleFxType::Delay || type == SimpleFxType::Reverb)
    {
        if (bufL.empty()) prepare (sr);
        int bufLen = static_cast<int> (bufL.size());
        if (bufLen < 64) return;

        int dL = (type == SimpleFxType::Delay)
            ? juce::jlimit (64, bufLen - 2, static_cast<int> ((0.05f + p1 * 0.65f) * sr))
            : juce::jlimit (64, bufLen - 2, static_cast<int> ((0.025f + p1 * 0.085f) * sr));
        int dR = juce::jlimit (64, bufLen - 2, static_cast<int> (dL * 1.15f));
        float fb = (type == SimpleFxType::Delay) ? (p2 * 0.75f) : (0.42f + p2 * 0.42f);
        float mix = p3;

        for (int i = 0; i < numSamples; ++i)
        {
            int rL = (writePos - dL + bufLen) % bufLen;
            int rR = (writePos - dR + bufLen) % bufLen;
            float tL = bufL[static_cast<size_t> (rL)];
            float tR = bufR[static_cast<size_t> (rR)];
            bufL[static_cast<size_t> (writePos)] = L[i] + tR * fb;
            bufR[static_cast<size_t> (writePos)] = R[i] + tL * fb;
            writePos = (writePos + 1) % bufLen;
            L[i] = L[i] * (1.0f - mix * 0.4f) + tL * mix;
            R[i] = R[i] * (1.0f - mix * 0.4f) + tR * mix;
        }
    }
}

juce::Colour RythmeAudioProcessor::getPaletteColour (int idx)
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

juce::String RythmeAudioProcessor::getPaletteColourName (int idx)
{
    static const char* names[numPaletteColours] = {
        "Blue (Default)", "Pink", "Red", "Orange", "Amber", "Gold",
        "Emerald", "Green", "Cyan", "Purple", "Magenta", "Silver"
    };
    return names[juce::jlimit (0, numPaletteColours - 1, idx)];
}

RythmeAudioProcessor::RythmeAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    formatManager.registerBasicFormats();
    heldNotesVel.fill (0.0f);
    groups.emplace_back();
    selectedGroupIndex = 0;
}

RythmeAudioProcessor::~RythmeAudioProcessor() = default;

std::vector<RhythmEvent> RythmeAudioProcessor::createDefaultEvents() const
{
    return { { 0.000f, 0.95f }, { 0.250f, 0.85f }, { 0.500f, 0.95f }, { 0.750f, 0.85f } };
}

juce::AudioProcessorValueTreeState::ParameterLayout RythmeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterBool> ("frr", "FRR (Fake Round Robin)", false));
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

int RythmeAudioProcessor::getGridDivisions() const
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

void RythmeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    groupScratchBuf.setSize (2, std::max (512, samplesPerBlock));

    for (auto& v : voices)
    {
        v.filterL.reset();
        v.filterR.reset();
        v.active = false;
    }

    const juce::ScopedLock sl (zoneLock);
    for (auto& g : groups)
        for (auto& slot : g.fxSlots)
            slot.prepare (sampleRate);
}

void RythmeAudioProcessor::releaseResources() {}

bool RythmeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

int RythmeAudioProcessor::createEmptyGroup()
{
    const juce::ScopedLock sl (zoneLock);
    int nextNum = static_cast<int> (groups.size()) + 1;
    SampleGroup g;
    g.name = "Group " + juce::String (nextNum);
    for (auto& s : g.fxSlots) s.prepare (currentSampleRate);
    groups.push_back (std::move (g));
    selectedGroupIndex = static_cast<int> (groups.size()) - 1;
    return selectedGroupIndex;
}

bool RythmeAudioProcessor::deleteSelectedGroup()
{
    const juce::ScopedLock sl (zoneLock);
    // Safe guard: keep at least 1 group
    if (groups.size() <= 1 || selectedGroupIndex < 0 || selectedGroupIndex >= static_cast<int> (groups.size()))
        return false;

    int delIdx = selectedGroupIndex;
    for (int i = static_cast<int> (zones.size()) - 1; i >= 0; --i)
    {
        if (zones[static_cast<size_t> (i)].groupIndex == delIdx)
            deleteZone (i);
        else if (zones[static_cast<size_t> (i)].groupIndex > delIdx)
            zones[static_cast<size_t> (i)].groupIndex--;
    }

    groups.erase (groups.begin() + delIdx);
    selectedGroupIndex = juce::jlimit (0, static_cast<int> (groups.size()) - 1, delIdx - 1);
    return true;
}

bool RythmeAudioProcessor::isKeyswitchNote (int midiNote) const
{
    for (const auto& g : groups)
        if (g.keyswitchNote >= 0 && g.keyswitchNote == midiNote)
            return true;
    return false;
}

int RythmeAudioProcessor::createEmptyZone (int key)
{
    const juce::ScopedLock sl (zoneLock);
    if (groups.empty())
    {
        groups.emplace_back();
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
    z.autoZeroCross = true;
    z.seqEvents = createDefaultEvents();
    z.colourIndex = 0;
    z.colour = getPaletteColour (0);
    zones.push_back (std::move (z));
    selectedZoneIndex = static_cast<int> (zones.size()) - 1;
    return selectedZoneIndex;
}

bool RythmeAudioProcessor::addSampleToZone (int zoneIndex, const juce::File& file)
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

bool RythmeAudioProcessor::loadSampleIntoNewZone (const juce::File& file, int lowKey, int highKey, int rootKey, int lowVel, int highVel)
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

void RythmeAudioProcessor::deleteZone (int index)
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

void RythmeAudioProcessor::deleteZones (std::vector<int> indices)
{
    std::sort (indices.begin(), indices.end(), std::greater<int>());
    indices.erase (std::unique (indices.begin(), indices.end()), indices.end());
    for (int idx : indices)
        deleteZone (idx);
}

void RythmeAudioProcessor::normalizeSelectedSample()
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

void RythmeAudioProcessor::resetSelectedZoneDefaults()
{
    const juce::ScopedLock sl (zoneLock);
    if (selectedZoneIndex < 0 || selectedZoneIndex >= static_cast<int> (zones.size())) return;
    auto& z = zones[static_cast<size_t> (selectedZoneIndex)];
    z.startNorm = 0.0f;
    z.endNorm = 1.0f;
    z.loopStartNorm = 0.15f;
    z.loopEndNorm = 0.85f;
    z.pitchSemi = 0.0f;
    z.pan = 0.0f;
    z.volume = 1.0f;
    z.attackSec = 0.005f;
    z.decaySec = 0.30f;
    z.sustainLvl = 0.85f;
    z.releaseSec = 0.25f;
    z.cutoffHz = 18000.0f;
    z.resQ = 0.707f;
}

void RythmeAudioProcessor::clearSelectedZoneRhythm()
{
    const juce::ScopedLock sl (zoneLock);
    if (selectedZoneIndex >= 0 && selectedZoneIndex < static_cast<int> (zones.size()))
        zones[static_cast<size_t> (selectedZoneIndex)].seqEvents.clear();
}

float RythmeAudioProcessor::snapNormToZeroCrossing (int zoneIndex, float rawNorm) const
{
    rawNorm = juce::jlimit (0.0f, 1.0f, rawNorm);
    if (zoneIndex < 0 || zoneIndex >= static_cast<int> (zones.size()))
        return rawNorm;
    const auto& z = zones[static_cast<size_t> (zoneIndex)];
    if (! z.autoZeroCross || z.layers.empty())
        return rawNorm;

    int lIdx = juce::jlimit (0, static_cast<int> (z.layers.size()) - 1, z.selectedLayerIndex);
    const auto& buf = z.layers[static_cast<size_t> (lIdx)].buffer;
    int total = buf.getNumSamples();
    if (total < 16) return rawNorm;

    const float* d = buf.getReadPointer (0);
    int target = juce::jlimit (1, total - 2, static_cast<int> (rawNorm * total));
    int searchRadius = std::min (512, total / 8);

    for (int off = 0; off < searchRadius; ++off)
    {
        if (target + off < total - 1 && (d[target + off] * d[target + off + 1] <= 0.0f))
            return static_cast<float> (target + off) / static_cast<float> (total);
        if (target - off > 1 && (d[target - off] * d[target - off - 1] <= 0.0f))
            return static_cast<float> (target - off) / static_cast<float> (total);
    }
    return rawNorm;
}

bool RythmeAudioProcessor::isGroupActiveForNote (int groupIdx) const
{
    if (groupIdx < 0 || groupIdx >= static_cast<int> (groups.size()))
        return false;
    const auto& g = groups[static_cast<size_t> (groupIdx)];
    if (g.muted)
        return false;

    if (g.keyswitchNote >= 0)
    {
        int activeKs = activeKeyswitchNote.load();
        if (activeKs >= 0 && g.keyswitchNote != activeKs)
            return false;
        if (activeKs < 0)
        {
            for (int i = 0; i < static_cast<int> (groups.size()); ++i)
                if (groups[static_cast<size_t> (i)].keyswitchNote >= 0)
                    return groups[static_cast<size_t> (i)].keyswitchNote == g.keyswitchNote;
        }
    }
    return true;
}

void RythmeAudioProcessor::startVoice (int zoneIdx, int midiNote, float velocity)
{
    auto& z = zones[static_cast<size_t> (zoneIdx)];
    if (! isGroupActiveForNote (z.groupIndex))
        return;
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

    bool frrOn = (apvts.getRawParameterValue ("frr")->load() > 0.5f) && (z.layers.size() == 1);
    bool pitchTrackOn = apvts.getRawParameterValue ("pitchTrack")->load() > 0.5f;
    float velRandAmt = apvts.getRawParameterValue ("velRand")->load();

    if (velRandAmt > 0.001f)
    {
        float delta = (rng.nextFloat() * 2.0f - 1.0f) * 0.30f * velRandAmt;
        v.velocity = juce::jlimit (0.04f, 1.0f, v.velocity + delta);
    }

    float rrPitchCents = 0.0f, rrStartOffsetSamples = 0.0f, rrFilterMult = 1.0f, rrGainMult = 1.0f, rrExtraAttackSec = 0.0f;
    if (frrOn)
    {
        ++rrSeedCounter;
        double n = static_cast<double> (rrSeedCounter);
        float u1 = static_cast<float> (std::fmod (n * 0.61803398875 + rng.nextFloat() * 0.13, 1.0));
        float u3 = static_cast<float> (std::fmod (n * 0.56984029099 + rng.nextFloat() * 0.19, 1.0));
        float u4 = static_cast<float> (std::fmod (n * 0.43015970900 + rng.nextFloat() * 0.11, 1.0));
        float u5 = static_cast<float> (std::fmod (n * 0.32471795724 + rng.nextFloat() * 0.23, 1.0));

        rrPitchCents = (u1 * 2.0f - 1.0f) * 18.0f;
        if (std::abs (rrPitchCents - lastRrPitch) < 4.5f) rrPitchCents = -rrPitchCents;
        lastRrPitch = rrPitchCents;

        rrStartOffsetSamples = u3 * static_cast<float> (currentSampleRate * 0.0042);
        rrFilterMult = 0.68f + u4 * 0.50f;
        rrGainMult = 0.88f + u5 * 0.24f;
        rrExtraAttackSec = u3 * 0.0045f;
    }

    // True Velocity Response (natural velocity scaling for recorded velocity layers)
    float gain = std::pow (v.velocity, 1.15f);

    float totalAttackSec = std::max (0.0005f, z.attackSec + rrExtraAttackSec);
    v.attackInc = 1.0f / std::max (1.0f, static_cast<float> (totalAttackSec * currentSampleRate));
    v.decayDec = 1.0f / std::max (1.0f, static_cast<float> (z.decaySec * currentSampleRate));
    v.sustainLvl = z.sustainLvl;
    v.releaseDec = 1.0f / std::max (1.0f, static_cast<float> (z.releaseSec * currentSampleRate));

    double noteSemiOffset = pitchTrackOn ? static_cast<double> (midiNote - z.rootKey) : 0.0;
    double semitones = noteSemiOffset + static_cast<double> (z.pitchSemi) + (rrPitchCents / 100.0);
    v.pitchRatio = (layer.sampleRate / currentSampleRate) * std::pow (2.0, semitones / 12.0);

    int startSamp = static_cast<int> (z.startNorm * totalSamps);
    int endSamp = std::max (startSamp + 2, static_cast<int> (z.endNorm * totalSamps));
    v.currentPos = z.reverse ? static_cast<double> (std::max (startSamp, endSamp - 2))
                             : (static_cast<double> (startSamp) + rrStartOffsetSamples);

    float finalPan = juce::jlimit (-1.0f, 1.0f, z.pan);
    float panAngle = (finalPan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    float finalGain = gain * rrGainMult * z.volume;
    v.gainL = finalGain * std::cos (panAngle) * 1.41421356f;
    v.gainR = finalGain * std::sin (panAngle) * 1.41421356f;

    v.filterL.reset();
    v.filterR.reset();
    float finalCut = juce::jlimit (80.0f, 20000.0f, z.cutoffHz * rrFilterMult);
    v.filterL.setParams (finalCut, z.resQ, currentSampleRate);
    v.filterR.setParams (finalCut, z.resQ, currentSampleRate);
}

void RythmeAudioProcessor::triggerNote (int midiNote, float velocity)
{
    lastPlayedNote.store (midiNote);
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

    int velInt = juce::jlimit (0, 127, static_cast<int> (std::round (velocity * 127.0f)));

    for (int i = 0; i < static_cast<int> (zones.size()); ++i)
    {
        const auto& z = zones[static_cast<size_t> (i)];
        if (midiNote >= z.lowKey && midiNote <= z.highKey && velInt >= z.lowVel && velInt <= z.highVel)
            startVoice (i, midiNote, velocity);
    }
}

void RythmeAudioProcessor::releaseNote (int midiNote)
{
    if (lastPlayedNote.load() == midiNote)
        lastPlayedNote.store (-1);
    for (auto& v : voices)
        if (v.active && v.midiNote == midiNote)
            v.envStage = 3;
}

bool RythmeAudioProcessor::hasAnyHeldNote() const
{
    for (float v : heldNotesVel)
        if (v > 0.0f) return true;
    return false;
}

void RythmeAudioProcessor::uiNoteOn (int midiNote, float velocity)
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
                for (const auto& ev : z.seqEvents)
                    if (ev.position < 0.004f)
                        startVoice (i, midiNote, velocity * ev.velocity);
        }
    }
}

void RythmeAudioProcessor::uiNoteOff (int midiNote)
{
    if (midiNote >= 0 && midiNote < 128)
        heldNotesVel[static_cast<size_t> (midiNote)] = 0.0f;
    releaseNote (midiNote);
}

void RythmeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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
        if (msg.isNoteOn()) uiNoteOn (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff()) uiNoteOff (msg.getNoteNumber());
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
    if (groupScratchBuf.getNumSamples() < numSamples) groupScratchBuf.setSize (2, numSamples, false, false, true);
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getWritePointer (1);

    for (int gIdx = 0; gIdx < static_cast<int> (groups.size()); ++gIdx)
    {
        auto& grp = groups[static_cast<size_t> (gIdx)];
        if (grp.muted) continue;

        bool hasActiveVoice = false;
        for (const auto& v : voices)
            if (v.active && v.zoneIndex >= 0 && v.zoneIndex < static_cast<int> (zones.size())
                && zones[static_cast<size_t> (v.zoneIndex)].groupIndex == gIdx)
            { hasActiveVoice = true; break; }

        bool hasTailFx = false;
        for (const auto& slot : grp.fxSlots)
            if (slot.enabled && (slot.type == SimpleFxType::Delay || slot.type == SimpleFxType::Reverb)) { hasTailFx = true; break; }

        if (! hasActiveVoice && ! hasTailFx)
            continue;

        groupScratchBuf.clear (0, numSamples);
        float* gL = groupScratchBuf.getWritePointer (0);
        float* gR = groupScratchBuf.getWritePointer (1);

        if (hasActiveVoice)
        {
            for (auto& v : voices)
            {
                if (! v.active || v.zoneIndex < 0 || v.zoneIndex >= static_cast<int> (zones.size())) continue;
                const auto& z = zones[static_cast<size_t> (v.zoneIndex)];
                if (z.groupIndex != gIdx) continue;
                if (v.layerIndex < 0 || v.layerIndex >= static_cast<int> (z.layers.size())) continue;

                const auto& layer = z.layers[static_cast<size_t> (v.layerIndex)];
                int totalSamps = layer.buffer.getNumSamples();
                if (totalSamps < 2) continue;

                int startSample = static_cast<int> (z.startNorm * totalSamps);
                int endSample = std::max (startSample + 2, static_cast<int> (z.endNorm * totalSamps));
                int lStartSample = juce::jlimit (startSample, endSample - 2, static_cast<int> (z.loopStartNorm * totalSamps));
                int lEndSample = juce::jlimit (lStartSample + 2, endSample, static_cast<int> (z.loopEndNorm * totalSamps));

                const float* srcL = layer.buffer.getReadPointer (0);
                const float* srcR = layer.buffer.getNumChannels() > 1 ? layer.buffer.getReadPointer (1) : srcL;

                for (int i = 0; i < numSamples; ++i)
                {
                    int posInt = static_cast<int> (v.currentPos);
                    if (z.sampleLoop && ! z.reverse && posInt >= lEndSample - 1)
                    {
                        v.currentPos = static_cast<double> (lStartSample);
                        posInt = lStartSample;
                    }
                    if (posInt < startSample || posInt >= endSample - 1 || posInt >= totalSamps - 1)
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

                    gL[i] += sL * v.envVal * v.gainL;
                    gR[i] += sR * v.envVal * v.gainR;
                    v.currentPos += (v.playingReverse ? -v.pitchRatio : v.pitchRatio);
                }
            }
        }

        for (auto& slot : grp.fxSlots)
            slot.processStereo (gL, gR, numSamples, currentSampleRate);

        float panAngle = (juce::jlimit (-1.0f, 1.0f, grp.pan) + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        float gGL = grp.volume * std::cos (panAngle) * 1.41421356f;
        float gGR = grp.volume * std::sin (panAngle) * 1.41421356f;

        for (int i = 0; i < numSamples; ++i)
        {
            outL[i] += gL[i] * gGL;
            outR[i] += gR[i] * gGR;
        }
    }
}

bool RythmeAudioProcessor::savePresetMn6x (const juce::File& file)
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
        gx->setAttribute ("ksNote", g.keyswitchNote);
        gx->setAttribute ("vol", g.volume);
        gx->setAttribute ("pan", g.pan);
        for (int s = 0; s < SampleGroup::numFxSlots; ++s)
        {
            const auto& slot = g.fxSlots[static_cast<size_t> (s)];
            auto* sx = gx->createNewChildElement ("FX");
            sx->setAttribute ("idx", s);
            sx->setAttribute ("type", static_cast<int> (slot.type));
            sx->setAttribute ("en", slot.enabled ? 1 : 0);
            sx->setAttribute ("p1", slot.p1); sx->setAttribute ("p2", slot.p2); sx->setAttribute ("p3", slot.p3);
        }
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
        zx->setAttribute ("zCross", z.autoZeroCross ? 1 : 0);
        zx->setAttribute ("rev", z.reverse ? 1 : 0);
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

bool RythmeAudioProcessor::loadPresetMn6x (const juce::File& file)
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
            g.keyswitchNote = gx->getIntAttribute ("ksNote", -1);
            g.volume = static_cast<float> (gx->getDoubleAttribute ("vol", 1.0));
            g.pan = static_cast<float> (gx->getDoubleAttribute ("pan", 0.0));
            for (auto* sx : gx->getChildIterator())
            {
                if (sx->hasTagName ("FX"))
                {
                    int sIdx = juce::jlimit (0, SampleGroup::numFxSlots - 1, sx->getIntAttribute ("idx", 0));
                    auto& slot = g.fxSlots[static_cast<size_t> (sIdx)];
                    slot.type = static_cast<SimpleFxType> (sx->getIntAttribute ("type", 0));
                    slot.enabled = sx->getIntAttribute ("en", 1) != 0;
                    slot.p1 = static_cast<float> (sx->getDoubleAttribute ("p1", 0.5));
                    slot.p2 = static_cast<float> (sx->getDoubleAttribute ("p2", 0.5));
                    slot.p3 = static_cast<float> (sx->getDoubleAttribute ("p3", 0.5));
                    slot.prepare (currentSampleRate);
                }
            }
            groups.push_back (std::move (g));
        }
        if (groups.empty()) groups.emplace_back();
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
                z.autoZeroCross = zx->getIntAttribute ("zCross", 1) != 0;
                z.reverse = zx->getIntAttribute ("rev", 0) != 0;
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
                    if (f.existsAsFile()) addSampleToZone (idx, f);
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

void RythmeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void RythmeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorEditor* RythmeAudioProcessor::createEditor()
{
    return new RythmeAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RythmeAudioProcessor();
}
