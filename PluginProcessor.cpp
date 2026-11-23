#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>
#include <cctype>

// ---------------- Lightweight CPU-Friendly InsertEffect ----------------
void InsertEffect::prepare (double sr)
{
    fL1.reset(); fR1.reset(); fL2.reset(); fR2.reset(); fL3.reset(); fR3.reset();
    envFollow = 0.0f;
    envFast = 0.0f;
    lfoPhase = 0.0f;
    if (type == FxType::Delay || type == FxType::Reverb || type == FxType::Chorus)
    {
        int maxLen = static_cast<int> (sr * 1.5) + 64;
        if (static_cast<int> (bufL.size()) < maxLen)
        {
            bufL.assign (static_cast<size_t> (maxLen), 0.0f);
            bufR.assign (static_cast<size_t> (maxLen), 0.0f);
        }
        writePos = 0;
    }
}

void InsertEffect::reset()
{
    fL1.reset(); fR1.reset(); fL2.reset(); fR2.reset(); fL3.reset(); fR3.reset();
    envFollow = 0.0f;
    envFast = 0.0f;
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
    writePos = 0;
}

juce::String InsertEffect::getTypeName (FxType t)
{
    switch (t)
    {
        case FxType::None:       return "Add FX +";
        case FxType::EQ3:        return "EQ 3";
        case FxType::Compressor: return "Compressor";
        case FxType::Saturation: return "Saturation";
        case FxType::Filter:     return "Filter";
        case FxType::Transient:  return "Transient";
        case FxType::Delay:      return "Delay";
        case FxType::Reverb:     return "Reverb";
        case FxType::Chorus:     return "Chorus";
        case FxType::Width:      return "Width";
        case FxType::Limiter:    return "Limiter";
        default:                 return "Add FX +";
    }
}

juce::String InsertEffect::getParamLabel (FxType t, int idx)
{
    switch (t)
    {
        case FxType::Compressor: return idx == 0 ? "Thresh" : (idx == 1 ? "Ratio"  : (idx == 2 ? "Release" : "Makeup"));
        case FxType::Saturation: return idx == 0 ? "Drive"  : (idx == 1 ? "Warmth" : (idx == 2 ? "Output"  : "Mix"));
        case FxType::Filter:     return idx == 0 ? "Cutoff" : (idx == 1 ? "Res"    : (idx == 2 ? "Mode"    : "Mix"));
        case FxType::Transient:  return idx == 0 ? "Attack" : (idx == 1 ? "Sustain": (idx == 2 ? "Smooth"  : "Mix"));
        case FxType::Delay:      return idx == 0 ? "Time"   : (idx == 1 ? "Feedbk" : (idx == 2 ? "Damp"    : "Mix"));
        case FxType::Reverb:     return idx == 0 ? "Size"   : (idx == 1 ? "Damp"   : (idx == 2 ? "PreDly"  : "Mix"));
        case FxType::Chorus:     return idx == 0 ? "Rate"   : (idx == 1 ? "Depth"  : (idx == 2 ? "Tone"    : "Mix"));
        case FxType::Width:      return idx == 0 ? "Width"  : (idx == 1 ? "LowMono": (idx == 2 ? "Gain"    : "Mix"));
        case FxType::Limiter:    return idx == 0 ? "Ceiling": (idx == 1 ? "Release": (idx == 2 ? "Gain"    : "Mix"));
        default:                 return "P" + juce::String (idx + 1);
    }
}

void InsertEffect::processStereo (float* L, float* R, int numSamples, double sr)
{
    if (bypassed || type == FxType::None)
        return;

    if (type == FxType::EQ3)
    {
        float q1 = 1.0f / juce::jlimit (0.15f, 2.5f, eqBw[0]);
        float q2 = 1.0f / juce::jlimit (0.15f, 2.5f, eqBw[1]);
        float q3 = 1.0f / juce::jlimit (0.15f, 2.5f, eqBw[2]);
        fL1.setParams (eqFreq[0], q1, sr); fR1.setParams (eqFreq[0], q1, sr);
        fL2.setParams (eqFreq[1], q2, sr); fR2.setParams (eqFreq[1], q2, sr);
        fL3.setParams (eqFreq[2], q3, sr); fR3.setParams (eqFreq[2], q3, sr);

        float g1 = std::pow (10.0f, eqGain[0] / 20.0f) - 1.0f;
        float g2 = std::pow (10.0f, eqGain[1] / 20.0f) - 1.0f;
        float g3 = std::pow (10.0f, eqGain[2] / 20.0f) - 1.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float inL = L[i];
            float inR = R[i];
            float b1L = fL1.processBP (inL);
            float b1R = fR1.processBP (inR);
            float b2L = fL2.processBP (inL);
            float b2R = fR2.processBP (inR);
            float b3L = fL3.processBP (inL);
            float b3R = fR3.processBP (inR);
            L[i] = inL + b1L * g1 + b2L * g2 + b3L * g3;
            R[i] = inR + b1R * g1 + b2R * g2 + b3R * g3;
        }
    }
    else if (type == FxType::Compressor)
    {
        float thresh = 0.04f + (1.0f - p1) * 0.86f;
        float ratio = 1.0f + p2 * 9.0f;
        float relCoeff = 0.0005f + (1.0f - p3) * 0.01f;
        float makeup = 0.7f + p4 * 1.6f;
        for (int i = 0; i < numSamples; ++i)
        {
            float peak = std::max (std::abs (L[i]), std::abs (R[i]));
            float coeff = (peak > envFollow) ? 0.08f : relCoeff;
            envFollow += coeff * (peak - envFollow);
            float gr = 1.0f;
            if (envFollow > thresh && envFollow > 1e-5f)
                gr = std::pow (envFollow / thresh, (1.0f / ratio) - 1.0f);
            L[i] *= gr * makeup;
            R[i] *= gr * makeup;
        }
    }
    else if (type == FxType::Saturation)
    {
        float drive = 1.0f + p1 * 10.0f;
        float norm = 1.0f / std::tanh (drive);
        float toneHz = 1200.0f + (1.0f - p2) * 16800.0f;
        float outG = 0.5f + p3;
        float mix = p4;
        fL1.setParams (toneHz, 0.707f, sr);
        fR1.setParams (toneHz, 0.707f, sr);
        for (int i = 0; i < numSamples; ++i)
        {
            float dL = fL1.processLP (std::tanh (L[i] * drive) * norm) * outG;
            float dR = fR1.processLP (std::tanh (R[i] * drive) * norm) * outG;
            L[i] = L[i] * (1.0f - mix) + dL * mix;
            R[i] = R[i] * (1.0f - mix) + dR * mix;
        }
    }
    else if (type == FxType::Filter)
    {
        float cutoff = 60.0f * std::pow (300.0f, p1);
        float q = 0.5f + p2 * 6.0f;
        bool isHP = (p3 > 0.5f);
        float mix = p4;
        fL1.setParams (cutoff, q, sr);
        fR1.setParams (cutoff, q, sr);
        for (int i = 0; i < numSamples; ++i)
        {
            float wL = isHP ? fL1.processHP (L[i]) : fL1.processLP (L[i]);
            float wR = isHP ? fR1.processHP (R[i]) : fR1.processLP (R[i]);
            L[i] = L[i] * (1.0f - mix) + wL * mix;
            R[i] = R[i] * (1.0f - mix) + wR * mix;
        }
    }
    else if (type == FxType::Transient)
    {
        float atkBoost = (p1 - 0.5f) * 3.0f;
        float susBoost = (p2 - 0.5f) * 2.0f;
        float mix = p4;
        for (int i = 0; i < numSamples; ++i)
        {
            float pk = std::max (std::abs (L[i]), std::abs (R[i]));
            envFast += (pk > envFast ? 0.15f : 0.01f) * (pk - envFast);
            envFollow += (pk > envFollow ? 0.005f : 0.0008f) * (pk - envFollow);
            float diff = envFast - envFollow;
            float gain = juce::jlimit (0.1f, 3.0f, 1.0f + diff * atkBoost + envFollow * susBoost);
            L[i] = L[i] * (1.0f - mix) + (L[i] * gain) * mix;
            R[i] = R[i] * (1.0f - mix) + (R[i] * gain) * mix;
        }
    }
    else if (type == FxType::Delay || type == FxType::Reverb || type == FxType::Chorus)
    {
        if (bufL.empty()) prepare (sr);
        int bufLen = static_cast<int> (bufL.size());
        if (bufLen < 64) return;

        if (type == FxType::Chorus)
        {
            float rate = 0.2f + p1 * 5.0f;
            float depthSamps = (2.0f + p2 * 18.0f) * static_cast<float> (sr * 0.001);
            float mix = p4;
            float phaseInc = static_cast<float> (rate * juce::MathConstants<double>::twoPi / sr);
            for (int i = 0; i < numSamples; ++i)
            {
                lfoPhase += phaseInc;
                if (lfoPhase > juce::MathConstants<float>::twoPi) lfoPhase -= juce::MathConstants<float>::twoPi;
                int modL = juce::jlimit (1, bufLen - 2, static_cast<int> (depthSamps * (1.0f + std::sin (lfoPhase)) + 8));
                int modR = juce::jlimit (1, bufLen - 2, static_cast<int> (depthSamps * (1.0f + std::cos (lfoPhase)) + 8));
                bufL[static_cast<size_t> (writePos)] = L[i];
                bufR[static_cast<size_t> (writePos)] = R[i];
                float wL = bufL[static_cast<size_t> ((writePos - modL + bufLen) % bufLen)];
                float wR = bufR[static_cast<size_t> ((writePos - modR + bufLen) % bufLen)];
                writePos = (writePos + 1) % bufLen;
                L[i] = L[i] * (1.0f - mix * 0.5f) + wL * mix;
                R[i] = R[i] * (1.0f - mix * 0.5f) + wR * mix;
            }
            return;
        }

        int dSampsL = (type == FxType::Delay)
            ? juce::jlimit (64, bufLen - 2, static_cast<int> ((0.04f + p1 * 0.75f) * sr))
            : juce::jlimit (64, bufLen - 2, static_cast<int> ((0.022f + p1 * 0.085f + p3 * 0.03f) * sr));
        int dSampsR = juce::jlimit (64, bufLen - 2, static_cast<int> (dSampsL * 1.14f));
        float fb = (type == FxType::Delay) ? (p2 * 0.78f) : (0.38f + p1 * 0.48f);
        float dampHz = (type == FxType::Delay) ? (1500.0f + (1.0f - p3) * 14000.0f) : (1200.0f + (1.0f - p2) * 14000.0f);
        float mix = p4;
        fL1.setParams (dampHz, 0.707f, sr);
        fR1.setParams (dampHz, 0.707f, sr);

        for (int i = 0; i < numSamples; ++i)
        {
            int rPosL = (writePos - dSampsL + bufLen) % bufLen;
            int rPosR = (writePos - dSampsR + bufLen) % bufLen;
            float tapL = fL1.processLP (bufL[static_cast<size_t> (rPosL)]);
            float tapR = fR1.processLP (bufR[static_cast<size_t> (rPosR)]);
            bufL[static_cast<size_t> (writePos)] = L[i] + tapR * fb;
            bufR[static_cast<size_t> (writePos)] = R[i] + tapL * fb;
            writePos = (writePos + 1) % bufLen;
            L[i] = L[i] * (1.0f - mix * 0.5f) + tapL * mix;
            R[i] = R[i] * (1.0f - mix * 0.5f) + tapR * mix;
        }
    }
    else if (type == FxType::Width)
    {
        float width = p1 * 2.0f;
        float gain = 0.5f + p3;
        for (int i = 0; i < numSamples; ++i)
        {
            float mid = 0.5f * (L[i] + R[i]);
            float side = 0.5f * (L[i] - R[i]) * width;
            L[i] = (mid + side) * gain;
            R[i] = (mid - side) * gain;
        }
    }
    else if (type == FxType::Limiter)
    {
        float ceil = 0.1f + p1 * 0.9f;
        float inG = 0.8f + p3 * 1.4f;
        for (int i = 0; i < numSamples; ++i)
        {
            float xL = L[i] * inG;
            float xR = R[i] * inG;
            L[i] = juce::jlimit (-ceil, ceil, xL);
            R[i] = juce::jlimit (-ceil, ceil, xR);
        }
    }
}

// ---------------- Palette & Filename Helper ----------------
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
    groupRRCounters.fill (0);
    groups.emplace_back();
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

void NoiseMu6ixRhythmAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    int bufLen = std::max (512, samplesPerBlock);
    groupScratchBuf.setSize (2, bufLen);
    sendBusBuf.setSize (2, bufLen);

    for (auto& v : voices)
    {
        v.filterL.reset();
        v.filterR.reset();
        v.active = false;
    }
    for (auto& fx : sendFx) fx.prepare (sampleRate);
    for (auto& fx : mainFx) fx.prepare (sampleRate);

    const juce::ScopedLock sl (zoneLock);
    for (auto& g : groups)
        for (auto& fx : g.insertFx)
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

int NoiseMu6ixRhythmAudioProcessor::createEmptyGroup()
{
    const juce::ScopedLock sl (zoneLock);
    int nextNum = static_cast<int> (groups.size()) + 1;
    SampleGroup g;
    g.name = "Group " + juce::String (nextNum);
    groups.push_back (std::move (g));
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

void NoiseMu6ixRhythmAudioProcessor::trimSelectedSample()
{
    const juce::ScopedLock sl (zoneLock);
    if (selectedZoneIndex < 0 || selectedZoneIndex >= static_cast<int> (zones.size())) return;
    auto& z = zones[static_cast<size_t> (selectedZoneIndex)];
    if (z.layers.empty()) return;
    int lIdx = juce::jlimit (0, static_cast<int> (z.layers.size()) - 1, z.selectedLayerIndex);
    auto& buf = z.layers[static_cast<size_t> (lIdx)].buffer;
    int total = buf.getNumSamples();
    int s = juce::jlimit (0, total - 4, static_cast<int> (z.startNorm * total));
    int e = juce::jlimit (s + 4, total, static_cast<int> (z.endNorm * total));
    int newLen = e - s;
    juce::AudioBuffer<float> trimmed (buf.getNumChannels(), newLen);
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        trimmed.copyFrom (ch, 0, buf, ch, s, newLen);
    buf = std::move (trimmed);
    z.startNorm = 0.0f;
    z.endNorm = 1.0f;
}

void NoiseMu6ixRhythmAudioProcessor::sliceSelectedSampleToKeys()
{
    const juce::ScopedLock sl (zoneLock);
    if (selectedZoneIndex < 0 || selectedZoneIndex >= static_cast<int> (zones.size())) return;
    const auto srcZone = zones[static_cast<size_t> (selectedZoneIndex)];
    if (srcZone.layers.empty()) return;

    int slices = (srcZone.waveGridWidthIdx == 0) ? 4 : (srcZone.waveGridWidthIdx == 1 ? 8 : (srcZone.waveGridWidthIdx == 2 ? 16 : 32));
    int baseKey = srcZone.rootKey;
    float span = std::max (0.02f, srcZone.endNorm - srcZone.startNorm);

    for (int i = 0; i < slices; ++i)
    {
        int key = juce::jlimit (0, 120, baseKey + i);
        SampleZone sz = srcZone;
        sz.name = srcZone.name + " [" + juce::String (i + 1) + "]";
        sz.lowKey = key;
        sz.highKey = key;
        sz.rootKey = key;
        sz.startNorm = srcZone.startNorm + (span * i) / static_cast<float> (slices);
        sz.endNorm   = srcZone.startNorm + (span * (i + 1)) / static_cast<float> (slices);
        if (i == 0)
            zones[static_cast<size_t> (selectedZoneIndex)] = std::move (sz);
        else
            zones.push_back (std::move (sz));
    }
}

void NoiseMu6ixRhythmAudioProcessor::clearSelectedZoneRhythm()
{
    const juce::ScopedLock sl (zoneLock);
    if (selectedZoneIndex >= 0 && selectedZoneIndex < static_cast<int> (zones.size()))
        zones[static_cast<size_t> (selectedZoneIndex)].seqEvents.clear();
}

float NoiseMu6ixRhythmAudioProcessor::getSelectedZonePlayheadNorm() const
{
    for (const auto& v : voices)
    {
        if (v.active && v.zoneIndex == selectedZoneIndex)
        {
            const juce::ScopedLock sl (zoneLock);
            if (v.zoneIndex >= 0 && v.zoneIndex < static_cast<int> (zones.size()))
            {
                const auto& z = zones[static_cast<size_t> (v.zoneIndex)];
                if (! z.layers.empty())
                {
                    int total = z.layers[static_cast<size_t> (juce::jlimit (0, static_cast<int> (z.layers.size()) - 1, v.layerIndex))].buffer.getNumSamples();
                    if (total > 0)
                        return juce::jlimit (0.0f, 1.0f, static_cast<float> (v.currentPos / total));
                }
            }
        }
    }
    return -1.0f;
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

void NoiseMu6ixRhythmAudioProcessor::startVoice (int zoneIdx, int midiNote, float velocity, float velFadeGain)
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
    v.currentPos = z.reverse ? static_cast<double> (std::max (startSamp, endSamp - 2))
                             : (static_cast<double> (startSamp) + rrStartOffsetSamples);

    float finalPan = juce::jlimit (-1.0f, 1.0f, z.pan);
    float panAngle = (finalPan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    float finalGain = gain * rrGainMult * z.volume * velFadeGain;
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

    int velInt = juce::jlimit (0, 127, static_cast<int> (velocity * 127.0f));

    std::array<int, 9> chosenGroupForRRSet;
    chosenGroupForRRSet.fill (-1);
    for (int rrSet = 1; rrSet <= 8; ++rrSet)
    {
        std::vector<int> candidates;
        for (int gIdx = 0; gIdx < static_cast<int> (groups.size()); ++gIdx)
            if (groups[static_cast<size_t> (gIdx)].rrSetId == rrSet && isGroupActiveForNote (gIdx))
                candidates.push_back (gIdx);
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
        int effLowV = std::max (0, z.lowVel - z.velFade);
        int effHighV = std::min (127, z.highVel + z.velFade);
        if (midiNote >= z.lowKey && midiNote <= z.highKey && velInt >= effLowV && velInt <= effHighV)
        {
            if (z.groupIndex >= 0 && z.groupIndex < static_cast<int> (groups.size()))
            {
                int rrSet = groups[static_cast<size_t> (z.groupIndex)].rrSetId;
                if (rrSet >= 1 && rrSet <= 8 && chosenGroupForRRSet[static_cast<size_t> (rrSet)] != z.groupIndex)
                    continue;
            }

            // Smooth velocity crossfade gain if inside velFade margin
            float fadeGain = 1.0f;
            if (z.velFade > 0)
            {
                if (velInt < z.lowVel)
                    fadeGain = 1.0f - static_cast<float> (z.lowVel - velInt) / static_cast<float> (z.velFade + 1);
                else if (velInt > z.highVel)
                    fadeGain = 1.0f - static_cast<float> (velInt - z.highVel) / static_cast<float> (z.velFade + 1);
                fadeGain = juce::jlimit (0.05f, 1.0f, fadeGain);
            }

            startVoice (i, midiNote, velocity, fadeGain);
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

    // Per-Group Voice Render -> Group Insert FX -> Send FX Bus -> Multi-Out -> Main Master FX
    const juce::ScopedLock sl (zoneLock);
    if (groupScratchBuf.getNumSamples() < numSamples) groupScratchBuf.setSize (2, numSamples, false, false, true);
    if (sendBusBuf.getNumSamples() < numSamples) sendBusBuf.setSize (2, numSamples, false, false, true);
    sendBusBuf.clear (0, numSamples);

    bool anySendActive = false;

    for (int gIdx = 0; gIdx < static_cast<int> (groups.size()); ++gIdx)
    {
        auto& grp = groups[static_cast<size_t> (gIdx)];
        if (grp.muted || (groupSoloEnabled.load() && gIdx != selectedGroupIndex))
            continue;

        bool hasActiveVoice = false;
        for (const auto& v : voices)
            if (v.active && v.zoneIndex >= 0 && v.zoneIndex < static_cast<int> (zones.size())
                && zones[static_cast<size_t> (v.zoneIndex)].groupIndex == gIdx)
            { hasActiveVoice = true; break; }

        bool hasTailFx = false;
        for (const auto& fx : grp.insertFx)
            if (! fx.bypassed && (fx.type == FxType::Delay || fx.type == FxType::Reverb)) { hasTailFx = true; break; }

        // 0% CPU for idle groups!
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
                            if (z.loopMode == 1)
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

                    if (loopAllowed && z.loopMode != 1 && xfadeSamps > 8 && posInt >= lEndSample - xfadeSamps)
                    {
                        float alpha = static_cast<float> (posInt - (lEndSample - xfadeSamps)) / static_cast<float> (xfadeSamps);
                        int altPos = juce::jlimit (startSample, totalSamps - 2, lStartSample + (posInt - (lEndSample - xfadeSamps)));
                        sL = sL * (1.0f - alpha) + srcL[altPos] * alpha;
                        sR = sR * (1.0f - alpha) + srcR[altPos] * alpha;
                    }

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
                    gL[i] += sL * amp * v.gainL;
                    gR[i] += sR * amp * v.gainR;

                    double stepRatio = v.inLoopRegion ? v.loopPitchRatio : v.pitchRatio;
                    v.currentPos += (v.playingReverse ? -stepRatio : stepRatio);
                }
            }
        }

        // Apply Group Insert FX (8 slots per group)
        for (auto& fx : grp.insertFx)
            fx.processStereo (gL, gR, numSamples, currentSampleRate);

        // Apply Group Amplifier (Volume, Pan, Phase Invert, L/R Swap)
        float panAngle = (juce::jlimit (-1.0f, 1.0f, grp.pan) + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        float phaseSign = grp.phaseInvert ? -1.0f : 1.0f;
        float grpGL = grp.volume * std::cos (panAngle) * 1.41421356f * phaseSign;
        float grpGR = grp.volume * std::sin (panAngle) * 1.41421356f * phaseSign;

        for (int i = 0; i < numSamples; ++i)
        {
            float inL = grp.lrSwap ? gR[i] : gL[i];
            float inR = grp.lrSwap ? gL[i] : gR[i];
            gL[i] = inL * grpGL;
            gR[i] = inR * grpGR;
        }

        // Feed Send FX Bus
        if (grp.sendLevel > 0.001f)
        {
            sendBusBuf.addFrom (0, 0, gL, numSamples, grp.sendLevel);
            sendBusBuf.addFrom (1, 0, gR, numSamples, grp.sendLevel);
            anySendActive = true;
        }

        // Route to selected Output Bus (Out 1 .. Out 8)
        int targetBus = juce::jlimit (0, numOutputBuses - 1, grp.outputBusIndex);
        if (targetBus >= getBusCount (false) || ! getBus (false, targetBus)->isEnabled())
            targetBus = 0;

        auto busBuf = getBusBuffer (buffer, false, targetBus);
        if (busBuf.getNumChannels() >= 2)
        {
            busBuf.addFrom (0, 0, gL, numSamples);
            busBuf.addFrom (1, 0, gR, numSamples);
        }
    }

    // Process Send FX Rack (8 slots) and sum to Main Out 1
    auto mainBus = getBusBuffer (buffer, false, 0);
    if (mainBus.getNumChannels() >= 2)
    {
        float* mL = mainBus.getWritePointer (0);
        float* mR = mainBus.getWritePointer (1);

        bool hasSendTail = false;
        for (const auto& fx : sendFx)
            if (! fx.bypassed && (fx.type == FxType::Delay || fx.type == FxType::Reverb)) { hasSendTail = true; break; }

        if (anySendActive || hasSendTail)
        {
            float* sL = sendBusBuf.getWritePointer (0);
            float* sR = sendBusBuf.getWritePointer (1);
            for (auto& fx : sendFx)
                fx.processStereo (sL, sR, numSamples, currentSampleRate);
            mainBus.addFrom (0, 0, sL, numSamples);
            mainBus.addFrom (1, 0, sR, numSamples);
        }

        // Process Main Master FX Rack (8 slots)
        for (auto& fx : mainFx)
            fx.processStereo (mL, mR, numSamples, currentSampleRate);
    }
}

static void saveFxRackXml (juce::XmlElement* parent, const char* tag, const std::array<InsertEffect, NoiseMu6ixRhythmAudioProcessor::numFxSlots>& rack)
{
    auto* rx = parent->createNewChildElement (tag);
    for (int i = 0; i < NoiseMu6ixRhythmAudioProcessor::numFxSlots; ++i)
    {
        const auto& fx = rack[static_cast<size_t> (i)];
        auto* sx = rx->createNewChildElement ("SLOT");
        sx->setAttribute ("idx", i);
        sx->setAttribute ("type", static_cast<int> (fx.type));
        sx->setAttribute ("byp", fx.bypassed ? 1 : 0);
        sx->setAttribute ("p1", fx.p1); sx->setAttribute ("p2", fx.p2);
        sx->setAttribute ("p3", fx.p3); sx->setAttribute ("p4", fx.p4);
        for (int b = 0; b < 3; ++b)
        {
            sx->setAttribute ("f" + juce::String (b), fx.eqFreq[b]);
            sx->setAttribute ("bw" + juce::String (b), fx.eqBw[b]);
            sx->setAttribute ("g" + juce::String (b), fx.eqGain[b]);
        }
    }
}

static void loadFxRackXml (juce::XmlElement* parent, const char* tag, std::array<InsertEffect, NoiseMu6ixRhythmAudioProcessor::numFxSlots>& rack, double sr)
{
    if (auto* rx = parent->getChildByName (tag))
    {
        for (auto* sx : rx->getChildIterator())
        {
            int idx = juce::jlimit (0, NoiseMu6ixRhythmAudioProcessor::numFxSlots - 1, sx->getIntAttribute ("idx", 0));
            auto& fx = rack[static_cast<size_t> (idx)];
            fx.type = static_cast<FxType> (sx->getIntAttribute ("type", 0));
            fx.bypassed = sx->getIntAttribute ("byp", 0) != 0;
            fx.p1 = static_cast<float> (sx->getDoubleAttribute ("p1", 0.5));
            fx.p2 = static_cast<float> (sx->getDoubleAttribute ("p2", 0.5));
            fx.p3 = static_cast<float> (sx->getDoubleAttribute ("p3", 0.5));
            fx.p4 = static_cast<float> (sx->getDoubleAttribute ("p4", 0.5));
            for (int b = 0; b < 3; ++b)
            {
                fx.eqFreq[b] = static_cast<float> (sx->getDoubleAttribute ("f" + juce::String (b), fx.eqFreq[b]));
                fx.eqBw[b]   = static_cast<float> (sx->getDoubleAttribute ("bw" + juce::String (b), fx.eqBw[b]));
                fx.eqGain[b] = static_cast<float> (sx->getDoubleAttribute ("g" + juce::String (b), fx.eqGain[b]));
            }
            fx.prepare (sr);
        }
    }
}

bool NoiseMu6ixRhythmAudioProcessor::savePresetMn6x (const juce::File& file)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("presetName", currentPresetName);

    const juce::ScopedLock sl (zoneLock);

    saveFxRackXml (xml.get(), "SEND_FX", sendFx);
    saveFxRackXml (xml.get(), "MAIN_FX", mainFx);

    auto* groupsXml = xml->createNewChildElement ("GROUPS");
    for (const auto& g : groups)
    {
        auto* gx = groupsXml->createNewChildElement ("GROUP");
        gx->setAttribute ("name", g.name);
        gx->setAttribute ("muted", g.muted ? 1 : 0);
        gx->setAttribute ("rrSet", g.rrSetId);
        gx->setAttribute ("ksNote", g.keyswitchNote);
        gx->setAttribute ("vol", g.volume);
        gx->setAttribute ("pan", g.pan);
        gx->setAttribute ("send", g.sendLevel);
        gx->setAttribute ("phase", g.phaseInvert ? 1 : 0);
        gx->setAttribute ("swap", g.lrSwap ? 1 : 0);
        gx->setAttribute ("outBus", g.outputBusIndex);
        saveFxRackXml (gx, "INSERT_FX", g.insertFx);
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
        zx->setAttribute ("velFade", z.velFade);
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

    {
        const juce::ScopedLock sl (zoneLock);
        loadFxRackXml (xml.get(), "SEND_FX", sendFx, currentSampleRate);
        loadFxRackXml (xml.get(), "MAIN_FX", mainFx, currentSampleRate);
    }

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
            g.volume = static_cast<float> (gx->getDoubleAttribute ("vol", 1.0));
            g.pan = static_cast<float> (gx->getDoubleAttribute ("pan", 0.0));
            g.sendLevel = static_cast<float> (gx->getDoubleAttribute ("send", 0.0));
            g.phaseInvert = gx->getIntAttribute ("phase", 0) != 0;
            g.lrSwap = gx->getIntAttribute ("swap", 0) != 0;
            g.outputBusIndex = gx->getIntAttribute ("outBus", 0);
            loadFxRackXml (gx, "INSERT_FX", g.insertFx, currentSampleRate);
            groups.push_back (std::move (g));
        }
        if (groups.empty())
            groups.emplace_back();
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
                z.velFade = zx->getIntAttribute ("velFade", 0);
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
