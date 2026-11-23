#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

// ---------------- Lightweight CPU-Friendly InsertEffect ----------------
void InsertEffect::prepare (double sr)
{
    fL1.reset(); fR1.reset(); fL2.reset(); fR2.reset();
    envFollow = 0.0f;
    if (type == FxType::Delay || type == FxType::Reverb)
    {
        int maxLen = static_cast<int> (sr * 1.5) + 64;
        if (static_cast<int> (delayBufL.size()) < maxLen)
        {
            delayBufL.assign (static_cast<size_t> (maxLen), 0.0f);
            delayBufR.assign (static_cast<size_t> (maxLen), 0.0f);
        }
        delayWritePos = 0;
    }
}

void InsertEffect::reset()
{
    fL1.reset(); fR1.reset(); fL2.reset(); fR2.reset();
    envFollow = 0.0f;
    std::fill (delayBufL.begin(), delayBufL.end(), 0.0f);
    std::fill (delayBufR.begin(), delayBufR.end(), 0.0f);
    delayWritePos = 0;
}

juce::String InsertEffect::getTypeName (FxType t)
{
    switch (t)
    {
        case FxType::None:       return "-- Empty --";
        case FxType::EQ:         return "EQ (Low/High)";
        case FxType::Compressor: return "Compressor";
        case FxType::Drive:      return "Saturation";
        case FxType::Filter:     return "Filter";
        case FxType::Delay:      return "Stereo Delay";
        case FxType::Reverb:     return "Space Reverb";
        case FxType::Width:      return "Stereo Width";
        default:                 return "-- Empty --";
    }
}

juce::String InsertEffect::getParamName (FxType t, int idx)
{
    switch (t)
    {
        case FxType::EQ:         return idx == 0 ? "Low Cut" : (idx == 1 ? "High Cut" : "Mix");
        case FxType::Compressor: return idx == 0 ? "Thresh"  : (idx == 1 ? "Ratio"    : "Makeup");
        case FxType::Drive:      return idx == 0 ? "Drive"   : (idx == 1 ? "Tone"     : "Mix");
        case FxType::Filter:     return idx == 0 ? "Cutoff"  : (idx == 1 ? "Res"      : "Mix");
        case FxType::Delay:      return idx == 0 ? "Time"    : (idx == 1 ? "Feedbk"   : "Mix");
        case FxType::Reverb:     return idx == 0 ? "Size"    : (idx == 1 ? "Damp"     : "Mix");
        case FxType::Width:      return idx == 0 ? "Width"   : (idx == 1 ? "Gain"     : "Mix");
        default:                 return idx == 0 ? "P1"      : (idx == 1 ? "P2"       : "Mix");
    }
}

void InsertEffect::processStereo (float* L, float* R, int numSamples, double sr)
{
    if (! enabled || type == FxType::None)
        return;

    if (type == FxType::EQ)
    {
        float hpFreq = 25.0f * std::pow (20.0f, p1);      // 25 Hz .. 500 Hz
        float lpFreq = 1200.0f * std::pow (16.0f, p2);    // 1.2 kHz .. 19.2 kHz
        fL1.setParams (hpFreq, 0.707f, sr);
        fR1.setParams (hpFreq, 0.707f, sr);
        fL2.setParams (lpFreq, 0.707f, sr);
        fR2.setParams (lpFreq, 0.707f, sr);
        float mix = p3;
        for (int i = 0; i < numSamples; ++i)
        {
            float wL = fL2.processLP (fL1.processHP (L[i]));
            float wR = fR2.processLP (fR1.processHP (R[i]));
            L[i] = L[i] * (1.0f - mix) + wL * mix;
            R[i] = R[i] * (1.0f - mix) + wR * mix;
        }
    }
    else if (type == FxType::Compressor)
    {
        float thresh = 0.05f + (1.0f - p1) * 0.85f;
        float ratio = 1.0f + p2 * 7.0f;
        float makeup = 0.8f + p3 * 1.4f;
        for (int i = 0; i < numSamples; ++i)
        {
            float peak = std::max (std::abs (L[i]), std::abs (R[i]));
            float coeff = (peak > envFollow) ? 0.08f : 0.002f;
            envFollow += coeff * (peak - envFollow);
            float gr = 1.0f;
            if (envFollow > thresh && envFollow > 1e-5f)
            {
                float over = envFollow / thresh;
                gr = std::pow (over, (1.0f / ratio) - 1.0f);
            }
            L[i] *= gr * makeup;
            R[i] *= gr * makeup;
        }
    }
    else if (type == FxType::Drive)
    {
        float drive = 1.0f + p1 * 9.0f;
        float norm = 1.0f / std::tanh (drive);
        float toneHz = 1000.0f + p2 * 17000.0f;
        fL1.setParams (toneHz, 0.707f, sr);
        fR1.setParams (toneHz, 0.707f, sr);
        float mix = p3;
        for (int i = 0; i < numSamples; ++i)
        {
            float dL = fL1.processLP (std::tanh (L[i] * drive) * norm);
            float dR = fR1.processLP (std::tanh (R[i] * drive) * norm);
            L[i] = L[i] * (1.0f - mix) + dL * mix;
            R[i] = R[i] * (1.0f - mix) + dR * mix;
        }
    }
    else if (type == FxType::Filter)
    {
        float cutoff = 80.0f * std::pow (240.0f, p1);
        float q = 0.5f + p2 * 5.0f;
        fL1.setParams (cutoff, q, sr);
        fR1.setParams (cutoff, q, sr);
        float mix = p3;
        for (int i = 0; i < numSamples; ++i)
        {
            float wL = fL1.processLP (L[i]);
            float wR = fR1.processLP (R[i]);
            L[i] = L[i] * (1.0f - mix) + wL * mix;
            R[i] = R[i] * (1.0f - mix) + wR * mix;
        }
    }
    else if (type == FxType::Delay || type == FxType::Reverb)
    {
        if (delayBufL.empty())
            prepare (sr);
        int bufLen = static_cast<int> (delayBufL.size());
        if (bufLen < 64) return;

        int delaySampsL = (type == FxType::Delay)
            ? juce::jlimit (64, bufLen - 2, static_cast<int> ((0.05f + p1 * 0.75f) * sr))
            : juce::jlimit (64, bufLen - 2, static_cast<int> ((0.025f + p1 * 0.085f) * sr));
        int delaySampsR = juce::jlimit (64, bufLen - 2, static_cast<int> (delaySampsL * 1.13f));
        float fb = (type == FxType::Delay) ? (p2 * 0.78f) : (0.35f + p1 * 0.48f);
        float dampHz = (type == FxType::Reverb) ? (1200.0f + (1.0f - p2) * 12000.0f) : 12000.0f;
        fL1.setParams (dampHz, 0.707f, sr);
        fR1.setParams (dampHz, 0.707f, sr);
        float mix = p3;

        for (int i = 0; i < numSamples; ++i)
        {
            int rPosL = (delayWritePos - delaySampsL + bufLen) % bufLen;
            int rPosR = (delayWritePos - delaySampsR + bufLen) % bufLen;
            float tapL = fL1.processLP (delayBufL[static_cast<size_t> (rPosL)]);
            float tapR = fR1.processLP (delayBufR[static_cast<size_t> (rPosR)]);

            delayBufL[static_cast<size_t> (delayWritePos)] = L[i] + tapR * fb;
            delayBufR[static_cast<size_t> (delayWritePos)] = R[i] + tapL * fb;
            delayWritePos = (delayWritePos + 1) % bufLen;

            L[i] = L[i] * (1.0f - mix * 0.5f) + tapL * mix;
            R[i] = R[i] * (1.0f - mix * 0.5f) + tapR * mix;
        }
    }
    else if (type == FxType::Width)
    {
        float width = p1 * 2.0f;
        float gain = 0.5f + p2;
        for (int i = 0; i < numSamples; ++i)
        {
            float mid = 0.5f * (L[i] + R[i]);
            float side = 0.5f * (L[i] - R[i]) * width;
            L[i] = (mid + side) * gain;
            R[i] = (mid - side) * gain;
        }
    }
}

// ---------------- AudioProcessor ----------------
NoiseMu6ixRhythmAudioProcessor::NoiseMu6ixRhythmAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withOutput ("Out 1 (Main)", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Out 2", juce::AudioChannelSet::stereo(), false)
                        .withOutput ("Out 3", juce::AudioChannelSet::stereo(), false)
                        .withOutput ("Out 4", juce::AudioChannelSet::stereo(), false)
                        .withOutput ("Out 5", juce::AudioChannelSet::stereo(), false)
                        .withOutput ("Out 6", juce::AudioChannelSet::stereo(), false)
                        .withOutput ("Out 7", juce::AudioChannelSet::stereo(), false)
                        .withOutput ("Out 8", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    formatManager.registerBasicFormats();
    heldNotesVel.fill (0.0f);
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
        juce::Colour (0xffe04f8a), juce::Colour (0xff4ca3ff), juce::Colour (0xffe8a138), juce::Colour (0xff4cc98f),
        juce::Colour (0xffa66cff), juce::Colour (0xffee6c4d), juce::Colour (0xff38c4d9), juce::Colour (0xffd9c838)
    };
    return palette[idx % 8];
}

void NoiseMu6ixRhythmAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    zoneScratchBuffer.setSize (2, std::max (512, samplesPerBlock));
    for (auto& v : voices)
    {
        v.filterL.reset();
        v.filterR.reset();
        v.active = false;
    }
    for (auto& fx : globalFx)
        fx.prepare (sampleRate);

    const juce::ScopedLock sl (zoneLock);
    for (auto& z : zones)
        for (auto& fx : z.channelFx)
            fx.prepare (sampleRate);
}

void NoiseMu6ixRhythmAudioProcessor::releaseResources() {}

bool NoiseMu6ixRhythmAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    for (int i = 1; i < layouts.outputBuses.size(); ++i)
    {
        auto set = layouts.outputBuses[i];
        if (! set.isDisabled() && set != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

int NoiseMu6ixRhythmAudioProcessor::createEmptyZone (int key)
{
    const juce::ScopedLock sl (zoneLock);
    int k = juce::jlimit (0, 120, key);
    SampleZone z;
    z.name = "Key " + juce::MidiMessage::getMidiNoteName (k, true, true, 5);
    z.lowKey = k;
    z.highKey = k;
    z.rootKey = k;
    z.lowVel = 0;
    z.highVel = 127;
    z.realRREnabled = true;
    z.seqEvents = createDefaultEvents();
    z.colour = getNextZoneColour (static_cast<int> (zones.size()));
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

void NoiseMu6ixRhythmAudioProcessor::addChannelFxToZone (int zoneIndex, FxType type)
{
    const juce::ScopedLock sl (zoneLock);
    if (zoneIndex < 0 || zoneIndex >= static_cast<int> (zones.size())) return;
    InsertEffect fx;
    fx.type = type;
    fx.enabled = true;
    fx.prepare (currentSampleRate);
    zones[static_cast<size_t> (zoneIndex)].channelFx.push_back (std::move (fx));
}

void NoiseMu6ixRhythmAudioProcessor::removeChannelFxFromZone (int zoneIndex, int fxIndex)
{
    const juce::ScopedLock sl (zoneLock);
    if (zoneIndex < 0 || zoneIndex >= static_cast<int> (zones.size())) return;
    auto& list = zones[static_cast<size_t> (zoneIndex)].channelFx;
    if (fxIndex >= 0 && fxIndex < static_cast<int> (list.size()))
        list.erase (list.begin() + fxIndex);
}

void NoiseMu6ixRhythmAudioProcessor::startVoice (int zoneIdx, int midiNote, float velocity)
{
    if (zoneIdx < 0 || zoneIdx >= static_cast<int> (zones.size()))
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
    if (layer.buffer.getNumSamples() == 0)
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

    // Per-Zone Pitch & Start
    double noteSemiOffset = pitchTrackOn ? static_cast<double> (midiNote - z.rootKey) : 0.0;
    double semitones = noteSemiOffset + static_cast<double> (z.pitchSemi) + (rrPitchCents / 100.0);
    v.pitchRatio = (layer.sampleRate / currentSampleRate) * std::pow (2.0, semitones / 12.0);
    v.currentPos = static_cast<double> (z.startNorm * layer.buffer.getNumSamples()) + rrStartOffsetSamples;

    // Per-Zone Pan & Volume
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
        if (v.active && v.midiNote == midiNote)
            v.envStage = 3;
}

bool NoiseMu6ixRhythmAudioProcessor::hasAnyHeldNote() const
{
    for (float v : heldNotesVel)
        if (v > 0.0f) return true;
    return false;
}

void NoiseMu6ixRhythmAudioProcessor::uiNoteOn (int midiNote, float velocity)
{
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
                if (ev.position < 0.005f)
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
                    evPos += (rng.nextFloat() * 2.0f - 1.0f) * 0.012 * timeRandAmt;
                evPos = std::fmod (evPos + 1.0, 1.0);

                bool triggered = (! wrapped && evPos > prevPhase && evPos <= seqBarPhase)
                              || (wrapped && (evPos > prevPhase || evPos <= seqBarPhase));
                if (triggered)
                    startVoice (zIdx, triggerMidi, triggerVel * ev.velocity);
            }
        }
    }

    // CPU-Friendly Per-Zone Voice Render + Per-Channel Mixer FX + Multi-Out Routing
    const juce::ScopedLock sl (zoneLock);

    bool anySolo = false;
    for (const auto& z : zones)
        if (z.soloed) { anySolo = true; break; }

    if (zoneScratchBuffer.getNumSamples() < numSamples)
        zoneScratchBuffer.setSize (2, numSamples, false, false, true);

    for (int zIdx = 0; zIdx < static_cast<int> (zones.size()); ++zIdx)
    {
        auto& z = zones[static_cast<size_t> (zIdx)];
        if (z.muted || (anySolo && ! z.soloed))
            continue;

        bool hasActiveVoice = false;
        for (const auto& v : voices)
            if (v.active && v.zoneIndex == zIdx) { hasActiveVoice = true; break; }

        bool hasTailFx = false;
        for (const auto& fx : z.channelFx)
            if (fx.enabled && (fx.type == FxType::Delay || fx.type == FxType::Reverb)) { hasTailFx = true; break; }

        // Zero CPU cost for idle zones!
        if (! hasActiveVoice && ! hasTailFx)
            continue;

        zoneScratchBuffer.clear (0, numSamples);
        float* zL = zoneScratchBuffer.getWritePointer (0);
        float* zR = zoneScratchBuffer.getWritePointer (1);

        if (hasActiveVoice)
        {
            for (auto& v : voices)
            {
                if (! v.active || v.zoneIndex != zIdx) continue;
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
                    if (z.sampleLoop && posInt >= lEndSample - 1)
                    {
                        v.currentPos = static_cast<double> (lStartSample);
                        posInt = lStartSample;
                    }
                    else if (posInt >= endSample - 1 || posInt >= totalSamps - 1)
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

                    if (v.driveAmt > 1.01f)
                    {
                        sL = std::tanh (sL * v.driveAmt) / std::tanh (v.driveAmt);
                        sR = std::tanh (sR * v.driveAmt) / std::tanh (v.driveAmt);
                    }

                    zL[i] += sL * v.envVal * v.gainL;
                    zR[i] += sR * v.envVal * v.gainR;

                    v.currentPos += v.pitchRatio;
                }
            }
        }

        // Apply Per-Channel Mixer Insert FX
        for (auto& fx : z.channelFx)
            fx.processStereo (zL, zR, numSamples, currentSampleRate);

        // Route to selected Output Bus (Out 1 .. Out 8), falling back safely to Main Out 1 if aux bus is disabled
        int targetBus = juce::jlimit (0, numOutputBuses - 1, z.outputBusIndex);
        if (targetBus >= getBusCount (false) || ! getBus (false, targetBus)->isEnabled())
            targetBus = 0;

        auto busBuf = getBusBuffer (buffer, false, targetBus);
        if (busBuf.getNumChannels() >= 2)
        {
            busBuf.addFrom (0, 0, zL, numSamples);
            busBuf.addFrom (1, 0, zR, numSamples);
        }
    }

    // Apply Global Master FX Rack to Main Out 1 (bypassed/empty slots cost 0 CPU)
    auto mainBus = getBusBuffer (buffer, false, 0);
    if (mainBus.getNumChannels() >= 2)
    {
        float* mL = mainBus.getWritePointer (0);
        float* mR = mainBus.getWritePointer (1);
        for (auto& fx : globalFx)
            fx.processStereo (mL, mR, numSamples, currentSampleRate);
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
        zx->setAttribute ("name", z.name);
        zx->setAttribute ("lowKey", z.lowKey);
        zx->setAttribute ("highKey", z.highKey);
        zx->setAttribute ("rootKey", z.rootKey);
        zx->setAttribute ("lowVel", z.lowVel);
        zx->setAttribute ("highVel", z.highVel);
        zx->setAttribute ("realRR", z.realRREnabled ? 1 : 0);

        zx->setAttribute ("start", z.startNorm);
        zx->setAttribute ("end", z.endNorm);
        zx->setAttribute ("lStart", z.loopStartNorm);
        zx->setAttribute ("lEnd", z.loopEndNorm);
        zx->setAttribute ("sLoop", z.sampleLoop ? 1 : 0);
        zx->setAttribute ("pitch", z.pitchSemi);
        zx->setAttribute ("pan", z.pan);
        zx->setAttribute ("vol", z.volume);
        zx->setAttribute ("atk", z.attackSec);
        zx->setAttribute ("dec", z.decaySec);
        zx->setAttribute ("sus", z.sustainLvl);
        zx->setAttribute ("rel", z.releaseSec);
        zx->setAttribute ("cut", z.cutoffHz);
        zx->setAttribute ("res", z.resQ);
        zx->setAttribute ("outBus", z.outputBusIndex);

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
                z.lowKey = zx->getIntAttribute ("lowKey", 60);
                z.highKey = zx->getIntAttribute ("highKey", 60);
                z.rootKey = zx->getIntAttribute ("rootKey", 60);
                z.lowVel = zx->getIntAttribute ("lowVel", 0);
                z.highVel = zx->getIntAttribute ("highVel", 127);
                z.realRREnabled = zx->getIntAttribute ("realRR", 1) != 0;

                z.startNorm = static_cast<float> (zx->getDoubleAttribute ("start", 0.0));
                z.endNorm = static_cast<float> (zx->getDoubleAttribute ("end", 1.0));
                z.loopStartNorm = static_cast<float> (zx->getDoubleAttribute ("lStart", 0.15));
                z.loopEndNorm = static_cast<float> (zx->getDoubleAttribute ("lEnd", 0.85));
                z.sampleLoop = zx->getIntAttribute ("sLoop", 0) != 0;
                z.pitchSemi = static_cast<float> (zx->getDoubleAttribute ("pitch", 0.0));
                z.pan = static_cast<float> (zx->getDoubleAttribute ("pan", 0.0));
                z.volume = static_cast<float> (zx->getDoubleAttribute ("vol", 1.0));
                z.attackSec = static_cast<float> (zx->getDoubleAttribute ("atk", 0.005));
                z.decaySec = static_cast<float> (zx->getDoubleAttribute ("dec", 0.30));
                z.sustainLvl = static_cast<float> (zx->getDoubleAttribute ("sus", 0.85));
                z.releaseSec = static_cast<float> (zx->getDoubleAttribute ("rel", 0.25));
                z.cutoffHz = static_cast<float> (zx->getDoubleAttribute ("cut", 18000.0));
                z.resQ = static_cast<float> (zx->getDoubleAttribute ("res", 0.707));
                z.outputBusIndex = zx->getIntAttribute ("outBus", 0);
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
