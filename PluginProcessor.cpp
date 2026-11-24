#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

void SimpleFxSlot::prepare (double sr)
{
    fL.reset(); fR.reset();
    midL.reset(); midR.reset();
    env = 0.0f;
    lastGrDb = 0.0f;
    if (type == SimpleFxType::Delay || type == SimpleFxType::Reverb)
    {
        int len = static_cast<int> (sr * 1.5) + 64;
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
        case SimpleFxType::EQ:         return "Pro-EQ";
        case SimpleFxType::Compressor: return "Pro-Comp";
        case SimpleFxType::Saturation: return "Saturn-Sat";
        case SimpleFxType::Delay:      return "Timeless-Dly";
        case SimpleFxType::Reverb:     return "Pro-Rev";
        default:                       return "FX Off";
    }
}

juce::String SimpleFxSlot::getParamLabel (SimpleFxType t, int idx)
{
    switch (t)
    {
        case SimpleFxType::EQ:
            return idx == 0 ? "Low Cut" : (idx == 1 ? "Mid Freq" : (idx == 2 ? "Mid Gain" : (idx == 3 ? "High Cut" : "Out Gain")));
        case SimpleFxType::Compressor:
            return idx == 0 ? "Thresh" : (idx == 1 ? "Ratio" : (idx == 2 ? "Attack" : (idx == 3 ? "Release" : "Makeup")));
        case SimpleFxType::Saturation:
            return idx == 0 ? "Drive" : (idx == 1 ? "Tone" : (idx == 2 ? "Dynamics" : (idx == 3 ? "Level" : "Mix")));
        case SimpleFxType::Delay:
            return idx == 0 ? "Time" : (idx == 1 ? "Feedback" : (idx == 2 ? "LP Cut" : (idx == 3 ? "Width" : "Mix")));
        case SimpleFxType::Reverb:
            return idx == 0 ? "Space" : (idx == 1 ? "Decay" : (idx == 2 ? "Damping" : (idx == 3 ? "PreDelay" : "Mix")));
        default:
            return "P" + juce::String (idx + 1);
    }
}

void SimpleFxSlot::processStereo (float* L, float* R, int numSamples, double sr)
{
    if (! enabled || type == SimpleFxType::Off)
        return;

    if (type == SimpleFxType::EQ)
    {
        float lowCutHz  = 20.0f + p1 * 500.0f;
        float midFreqHz = 250.0f + p2 * 5500.0f;
        float midGain   = 0.4f + p3 * 1.6f;
        float highCutHz = 1800.0f + p4 * 18200.0f;
        float outGain   = 0.5f + p5 * 1.5f;

        fL.setParams (highCutHz, 0.707f, sr);
        fR.setParams (highCutHz, 0.707f, sr);
        midL.setParams (midFreqHz, 1.2f, sr);
        midR.setParams (midFreqHz, 1.2f, sr);
        float lowAtt = juce::jlimit (0.0f, 0.45f, (lowCutHz - 20.0f) / 1100.0f);

        for (int i = 0; i < numSamples; ++i)
        {
            float xL = fL.processLP (L[i]) * (1.0f - lowAtt);
            float xR = fR.processLP (R[i]) * (1.0f - lowAtt);
            float mL = midL.processLP (xL);
            float mR = midR.processLP (xR);
            L[i] = (xL + mL * (midGain - 1.0f) * 0.45f) * outGain;
            R[i] = (xR + mR * (midGain - 1.0f) * 0.45f) * outGain;
        }
    }
    else if (type == SimpleFxType::Compressor)
    {
        float thresh  = 0.06f + (1.0f - p1) * 0.84f;
        float ratio   = 1.0f + p2 * 11.0f;
        float atkCoef = 0.005f + (1.0f - p3) * 0.14f;
        float relCoef = 0.0004f + (1.0f - p4) * 0.015f;
        float makeup  = 0.5f + p5 * 1.6f;

        float minGr = 1.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            float pk = std::max (std::abs (L[i]), std::abs (R[i]));
            env += (pk > env ? atkCoef : relCoef) * (pk - env);
            float gr = (env > thresh && env > 1e-5f) ? std::pow (env / thresh, (1.0f / ratio) - 1.0f) : 1.0f;
            minGr = std::min (minGr, gr);
            L[i] *= gr * makeup;
            R[i] *= gr * makeup;
        }
        lastGrDb = juce::Decibels::gainToDecibels (minGr, -24.0f);
    }
    else if (type == SimpleFxType::Saturation)
    {
        float drive = 1.0f + p1 * 7.5f;
        float norm  = 1.0f / std::tanh (drive);
        float cutHz = 1600.0f + p2 * 18400.0f;
        float level = 0.5f + p4 * 1.0f;
        float mix   = p5;

        fL.setParams (cutHz, 0.707f, sr);
        fR.setParams (cutHz, 0.707f, sr);
        for (int i = 0; i < numSamples; ++i)
        {
            float sL = fL.processLP (std::tanh (L[i] * drive) * norm) * level;
            float sR = fR.processLP (std::tanh (R[i] * drive) * norm) * level;
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
            ? juce::jlimit (64, bufLen - 2, static_cast<int> ((0.04f + p1 * 0.75f) * sr))
            : juce::jlimit (64, bufLen - 2, static_cast<int> ((0.02f + p1 * 0.09f + p4 * 0.05f) * sr));
        float widthScale = (type == SimpleFxType::Delay) ? (1.0f + p4 * 0.25f) : 1.18f;
        int dR = juce::jlimit (64, bufLen - 2, static_cast<int> (dL * widthScale));
        float fb = (type == SimpleFxType::Delay) ? (p2 * 0.78f) : (0.38f + p2 * 0.48f);
        float dampHz = 1200.0f + (1.0f - p3) * 16000.0f;
        float mix = p5;

        fL.setParams (dampHz, 0.707f, sr);
        fR.setParams (dampHz, 0.707f, sr);

        for (int i = 0; i < numSamples; ++i)
        {
            int rL = (writePos - dL + bufLen) % bufLen;
            int rR = (writePos - dR + bufLen) % bufLen;
            float tL = fL.processLP (bufL[static_cast<size_t> (rL)]);
            float tR = fR.processLP (bufR[static_cast<size_t> (rR)]);
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
        juce::Colour (0xffd8dde6), juce::Colour (0xffb8c0cc), juce::Colour (0xff98a2b3),
        juce::Colour (0xff78849b), juce::Colour (0xff4ca3ff), juce::Colour (0xffff5599),
        juce::Colour (0xff4cc98f), juce::Colour (0xffe8a138), juce::Colour (0xffa66cff),
        juce::Colour (0xff38c4d9), juce::Colour (0xffee5253), juce::Colour (0xfff2f4f8)
    };
    return palette[juce::jlimit (0, numPaletteColours - 1, idx)];
}

juce::String RythmeAudioProcessor::getPaletteColourName (int idx)
{
    static const char* names[numPaletteColours] = {
        "Silver", "Light Ash", "Mid Slate", "Dark Graphite",
        "Blue", "Pink", "Emerald", "Amber", "Purple", "Cyan", "Red", "White"
    };
    return names[juce::jlimit (0, numPaletteColours - 1, idx)];
}

float RythmeAudioProcessor::vol100ToLinearGain (float vol100)
{
    float v = juce::jlimit (0.0f, 100.0f, vol100);
    if (v <= 0.01f) return 0.0f;
    return std::pow (v / 80.0f, 1.35f);
}

RythmeAudioProcessor::RythmeAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    formatManager.registerBasicFormats();
    heldNotesVel.fill (0.0f);
    scopeRing.fill (0.0f);
    groups.emplace_back();
    selectedGroupIndex = 0;
    loadLibraryRegistry();
}

RythmeAudioProcessor::~RythmeAudioProcessor() = default;

void RythmeAudioProcessor::copyScopeHistory (std::array<float, scopeHistorySize>& dest, bool& isSounding) const
{
    const juce::SpinLock::ScopedLockType sl (scopeLock);
    int rIdx = scopeWriteIdx;
    for (int i = 0; i < scopeHistorySize; ++i)
    {
        dest[static_cast<size_t> (i)] = scopeRing[static_cast<size_t> (rIdx)];
        rIdx = (rIdx + 1) % scopeHistorySize;
    }
    isSounding = (silentSamplesHold < static_cast<int> (currentSampleRate * 0.14));
}

juce::File RythmeAudioProcessor::getRegistryFile() const
{
    auto appData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("NoiseMu6ixStudio")
                       .getChildFile ("KINETIK");
    appData.createDirectory();
    return appData.getChildFile ("library_registry.xml");
}

void RythmeAudioProcessor::scanFolderIntoCategories (const juce::File& folder)
{
    libraryCategories.clear();
    if (! folder.isDirectory()) return;

    juce::Array<juce::File> subDirs;
    folder.findChildFiles (subDirs, juce::File::findDirectories, false);
    subDirs.sort();

    for (const auto& sd : subDirs)
    {
        LibraryCategoryEntry cat;
        cat.name = sd.getFileName();
        cat.folder = sd;
        cat.expanded = false;
        for (const auto& f : sd.findChildFiles (juce::File::findFiles, true, "*.wav;*.aiff;*.flac;*.mn6x"))
            cat.items.push_back (f);
        std::sort (cat.items.begin(), cat.items.end(), [] (const juce::File& a, const juce::File& b)
        {
            return a.getFileNameWithoutExtension().compareIgnoreCase (b.getFileNameWithoutExtension()) < 0;
        });
        libraryCategories.push_back (std::move (cat));
    }

    std::vector<juce::File> rootItems;
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.wav;*.aiff;*.flac;*.mn6x"))
        rootItems.push_back (f);
    if (! rootItems.empty())
    {
        std::sort (rootItems.begin(), rootItems.end(), [] (const juce::File& a, const juce::File& b)
        {
            return a.getFileNameWithoutExtension().compareIgnoreCase (b.getFileNameWithoutExtension()) < 0;
        });
        LibraryCategoryEntry rootCat;
        rootCat.name = folder.getFileName();
        rootCat.folder = folder;
        rootCat.expanded = true;
        rootCat.items = std::move (rootItems);
        libraryCategories.insert (libraryCategories.begin(), std::move (rootCat));
    }
}

void RythmeAudioProcessor::setRootLibraryFolder (const juce::File& folder)
{
    if (! folder.isDirectory()) return;
    const juce::ScopedLock sl (zoneLock);
    rootLibraryFolder = folder;
    scanFolderIntoCategories (folder);
    saveLibraryRegistry();
}

void RythmeAudioProcessor::saveLibraryRegistry()
{
    juce::XmlElement root ("KINETIK_LIBRARY");
    root.setAttribute ("rootFolder", rootLibraryFolder.getFullPathName());
    for (const auto& cat : libraryCategories)
    {
        auto* cx = root.createNewChildElement ("CATEGORY");
        cx->setAttribute ("name", cat.name);
        cx->setAttribute ("folder", cat.folder.getFullPathName());
        cx->setAttribute ("expanded", cat.expanded ? 1 : 0);
        for (const auto& item : cat.items)
        {
            auto* ix = cx->createNewChildElement ("ITEM");
            ix->setAttribute ("file", item.getFullPathName());
        }
    }
    root.writeTo (getRegistryFile());
}

void RythmeAudioProcessor::loadLibraryRegistry()
{
    libraryCategories.clear();
    auto regFile = getRegistryFile();
    if (! regFile.existsAsFile()) return;
    auto xml = juce::XmlDocument::parse (regFile);
    if (xml == nullptr) return;

    rootLibraryFolder = juce::File (xml->getStringAttribute ("rootFolder"));
    if (rootLibraryFolder.isDirectory())
    {
        scanFolderIntoCategories (rootLibraryFolder);
        return;
    }

    for (auto* cx : xml->getChildIterator())
    {
        if (! cx->hasTagName ("CATEGORY")) continue;
        LibraryCategoryEntry cat;
        cat.name = cx->getStringAttribute ("name", "LIBRARY");
        cat.folder = juce::File (cx->getStringAttribute ("folder"));
        cat.expanded = cx->getIntAttribute ("expanded", 0) != 0;

        for (auto* ix : cx->getChildIterator())
        {
            if (ix->hasTagName ("ITEM"))
            {
                juce::File f (ix->getStringAttribute ("file"));
                if (f.existsAsFile()) cat.items.push_back (f);
            }
        }
        if (cat.items.empty() && cat.folder.isDirectory())
        {
            for (const auto& f : cat.folder.findChildFiles (juce::File::findFiles, true, "*.wav;*.aiff;*.flac;*.mn6x"))
                cat.items.push_back (f);
        }
        if (! cat.items.empty() || cat.folder.isDirectory())
            libraryCategories.push_back (std::move (cat));
    }
}

void RythmeAudioProcessor::addLibraryFolderPersistent (const juce::File& folder)
{
    if (! folder.isDirectory()) return;

    const juce::ScopedLock sl (zoneLock);
    for (auto& existing : libraryCategories)
    {
        if (existing.folder == folder)
        {
            existing.items.clear();
            for (const auto& f : folder.findChildFiles (juce::File::findFiles, true, "*.wav;*.aiff;*.flac;*.mn6x"))
                existing.items.push_back (f);
            existing.expanded = true;
            saveLibraryRegistry();
            return;
        }
    }

    LibraryCategoryEntry cat;
    cat.name = folder.getFileName();
    cat.folder = folder;
    cat.expanded = true;
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, true, "*.wav;*.aiff;*.flac;*.mn6x"))
        cat.items.push_back (f);
    libraryCategories.push_back (std::move (cat));
    saveLibraryRegistry();
}

void RythmeAudioProcessor::sortLibraryPersistent()
{
    const juce::ScopedLock sl (zoneLock);
    std::sort (libraryCategories.begin(), libraryCategories.end(),
        [] (const LibraryCategoryEntry& a, const LibraryCategoryEntry& b)
        {
            return a.name.compareIgnoreCase (b.name) < 0;
        });
    for (auto& cat : libraryCategories)
    {
        std::sort (cat.items.begin(), cat.items.end(),
            [] (const juce::File& a, const juce::File& b)
            {
                return a.getFileNameWithoutExtension().compareIgnoreCase (b.getFileNameWithoutExtension()) < 0;
            });
    }
    saveLibraryRegistry();
}

void RythmeAudioProcessor::removeLibraryEntryPersistent (int catIdx)
{
    const juce::ScopedLock sl (zoneLock);
    if (catIdx >= 0 && catIdx < static_cast<int> (libraryCategories.size()))
    {
        libraryCategories.erase (libraryCategories.begin() + catIdx);
        saveLibraryRegistry();
    }
}

void RythmeAudioProcessor::toggleLibraryCategoryExpanded (int catIdx)
{
    const juce::ScopedLock sl (zoneLock);
    if (catIdx >= 0 && catIdx < static_cast<int> (libraryCategories.size()))
    {
        libraryCategories[static_cast<size_t> (catIdx)].expanded = ! libraryCategories[static_cast<size_t> (catIdx)].expanded;
        saveLibraryRegistry();
    }
}

void RythmeAudioProcessor::setLoadedItemInPanel (const juce::String& displayName, const juce::File& file, int keyHint)
{
    loadedItemActive = true;
    loadedItemName = displayName;
    loadedItemFile = file;
    loadedItemKey = keyHint;
}

void RythmeAudioProcessor::clearLoadedItemPanel()
{
    const juce::ScopedLock sl (zoneLock);
    if (loadedItemKey >= 0)
    {
        for (int i = static_cast<int> (zones.size()) - 1; i >= 0; --i)
        {
            if (zones[static_cast<size_t> (i)].rootKey == loadedItemKey)
            {
                deleteZone (i);
                break;
            }
        }
    }
    loadedItemActive = false;
    loadedItemName.clear();
    loadedItemFile = juce::File();
    loadedItemKey = -1;
}

std::vector<RhythmEvent> RythmeAudioProcessor::createDefaultEvents() const
{
    return { { 0.000f, 0.95f }, { 0.250f, 0.85f }, { 0.500f, 0.95f }, { 0.750f, 0.85f } };
}

juce::AudioProcessorValueTreeState::ParameterLayout RythmeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterBool> ("frr", "FRR (Fake Round Robin)", false));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("fvl", "FVL (Fake Velocity Layer Cut-Only)", false));
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
    z.autoLoopZeroCross = true;
    z.seqEvents = createDefaultEvents();
    z.colourIndex = 11;
    z.colour = getPaletteColour (11);
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
    z.loopCrossfade = 0.04f;
    z.fadeInNorm = 0.0f;
    z.fadeOutNorm = 0.0f;
    z.pitchSemi = 0.0f;
    z.pan50 = 0.0f;
    z.volumeDb100 = 80.0f;
    z.attackMs = 0.10f;
    z.decayMs = 300.0f;
    z.sustainPct = 85.0f;
    z.releaseMs = 250.0f;
    z.cutoffHz = 20000.0f;
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
    if (! z.autoLoopZeroCross || z.layers.empty())
        return rawNorm;

    int lIdx = juce::jlimit (0, static_cast<int> (z.layers.size()) - 1, z.selectedLayerIndex);
    const auto& buf = z.layers[static_cast<size_t> (lIdx)].buffer;
    int total = buf.getNumSamples();
    if (total < 16) return rawNorm;

    const float* d = buf.getReadPointer (0);
    int target = juce::jlimit (1, total - 2, static_cast<int> (rawNorm * total));
    int searchRadius = std::min (600, total / 6);

    for (int off = 0; off < searchRadius; ++off)
    {
        if (target + off < total - 1 && (d[target + off] * d[target + off + 1] <= 0.0f))
            return static_cast<float> (target + off) / static_cast<float> (total);
        if (target - off > 1 && (d[target - off] * d[target - off - 1] <= 0.0f))
            return static_cast<float> (target - off) / static_cast<float> (total);
    }
    return rawNorm;
}

bool RythmeAudioProcessor::hasAnySoloedGroup() const
{
    for (const auto& g : groups)
        if (g.soloed) return true;
    return false;
}

bool RythmeAudioProcessor::isGroupActiveForNote (int groupIdx) const
{
    if (groupIdx < 0 || groupIdx >= static_cast<int> (groups.size()))
        return false;
    const auto& g = groups[static_cast<size_t> (groupIdx)];
    if (g.muted)
        return false;
    if (hasAnySoloedGroup() && ! g.soloed)
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
    bool fvlOn = apvts.getRawParameterValue ("fvl")->load() > 0.5f;
    bool pitchTrackOn = apvts.getRawParameterValue ("pitchTrack")->load() > 0.5f;
    float velRandAmt = apvts.getRawParameterValue ("velRand")->load();

    if (velRandAmt > 0.001f)
    {
        float delta = (rng.nextFloat() * 2.0f - 1.0f) * 0.25f * velRandAmt;
        v.velocity = juce::jlimit (0.04f, 1.0f, v.velocity + delta);
    }

    float rrPitchCents = 0.0f;
    float rrStartOffsetSamples = 0.0f;
    float rrGainMult = 1.0f;
    float rrExtraAttackMs = 0.0f;
    if (frrOn)
    {
        ++rrSeedCounter;
        double n = static_cast<double> (rrSeedCounter);
        float u1 = static_cast<float> (std::fmod (n * 0.61803398875 + rng.nextFloat() * 0.07, 1.0));
        float u2 = static_cast<float> (std::fmod (n * 0.56984029099 + rng.nextFloat() * 0.09, 1.0));
        float u3 = static_cast<float> (std::fmod (n * 0.43015970900 + rng.nextFloat() * 0.06, 1.0));

        rrPitchCents = (u1 * 2.0f - 1.0f) * 5.5f;
        if (std::abs (rrPitchCents - lastRrPitch) < 1.8f) rrPitchCents = -rrPitchCents;
        lastRrPitch = rrPitchCents;

        rrStartOffsetSamples = u2 * static_cast<float> (currentSampleRate * 0.0012);
        rrGainMult = 0.96f + u3 * 0.04f;
        rrExtraAttackMs = u2 * 0.8f;
    }

    float fvlCutHz = 20000.0f;
    float fvlGainScale = 1.0f;
    if (fvlOn)
    {
        float vNorm = juce::jlimit (0.0f, 1.0f, v.velocity);
        fvlCutHz = 1400.0f + std::pow (vNorm, 1.45f) * 18600.0f;
        fvlGainScale = 0.45f + 0.55f * vNorm;
    }

    float gain = std::pow (v.velocity, 1.10f) * fvlGainScale;

    float totalAttackSec = std::max (0.0001f, (z.attackMs + rrExtraAttackMs) * 0.001f);
    float decaySec = std::max (0.001f, z.decayMs * 0.001f);
    float releaseSec = std::max (0.001f, z.releaseMs * 0.001f);

    v.attackInc = 1.0f / std::max (1.0f, static_cast<float> (totalAttackSec * currentSampleRate));
    v.decayDec = 1.0f / std::max (1.0f, static_cast<float> (decaySec * currentSampleRate));
    v.sustainLvl = juce::jlimit (0.0f, 1.0f, z.sustainPct * 0.01f);
    v.releaseDec = 1.0f / std::max (1.0f, static_cast<float> (releaseSec * currentSampleRate));

    double noteSemiOffset = pitchTrackOn ? static_cast<double> (midiNote - z.rootKey) : 0.0;
    double semitones = noteSemiOffset + static_cast<double> (z.pitchSemi) + (rrPitchCents / 100.0);
    v.pitchRatio = (layer.sampleRate / currentSampleRate) * std::pow (2.0, semitones / 12.0);

    int startSamp = static_cast<int> (z.startNorm * totalSamps);
    int endSamp = std::max (startSamp + 2, static_cast<int> (z.endNorm * totalSamps));
    v.currentPos = z.reverse ? static_cast<double> (std::max (startSamp, endSamp - 2))
                             : (static_cast<double> (startSamp) + rrStartOffsetSamples);

    float panNorm = juce::jlimit (-1.0f, 1.0f, z.pan50 / 50.0f);
    float panAngle = (panNorm + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    float finalGain = gain * rrGainMult * vol100ToLinearGain (z.volumeDb100);
    v.gainL = finalGain * std::cos (panAngle) * 1.41421356f;
    v.gainR = finalGain * std::sin (panAngle) * 1.41421356f;

    v.filterL.reset();
    v.filterR.reset();
    float finalCut = juce::jlimit (80.0f, 20000.0f, std::min (z.cutoffHz, fvlCutHz));
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

    bool seqLoopOn = (apvts.getRawParameterValue ("seqLoop")->load() > 0.5f);
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

    bool seqLoopOn = (apvts.getRawParameterValue ("seqLoop")->load() > 0.5f);
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

    bool anySolo = hasAnySoloedGroup();
    bool bypassAllFx = fxBypass.load();

    for (int gIdx = 0; gIdx < static_cast<int> (groups.size()); ++gIdx)
    {
        auto& grp = groups[static_cast<size_t> (gIdx)];
        if (grp.muted || (anySolo && ! grp.soloed)) continue;

        bool hasActiveVoice = false;
        for (const auto& v : voices)
            if (v.active && v.zoneIndex >= 0 && v.zoneIndex < static_cast<int> (zones.size())
                && zones[static_cast<size_t> (v.zoneIndex)].groupIndex == gIdx)
            { hasActiveVoice = true; break; }

        bool hasTailFx = false;
        if (! bypassAllFx)
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
                int activeSpan = std::max (2, endSample - startSample);
                int fadeInSamps = static_cast<int> (z.fadeInNorm * activeSpan);
                int fadeOutSamps = static_cast<int> (z.fadeOutNorm * activeSpan);
                int loopLen = std::max (2, lEndSample - lStartSample);
                int xfadeSamps = static_cast<int> (z.loopCrossfade * loopLen);

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

                    if (z.sampleLoop && ! z.reverse && xfadeSamps > 6 && posInt >= lEndSample - xfadeSamps)
                    {
                        float alpha = static_cast<float> (posInt - (lEndSample - xfadeSamps)) / static_cast<float> (xfadeSamps);
                        int altPos = juce::jlimit (startSample, totalSamps - 2, lStartSample + (posInt - (lEndSample - xfadeSamps)));
                        sL = sL * (1.0f - alpha) + srcL[altPos] * alpha;
                        sR = sR * (1.0f - alpha) + srcR[altPos] * alpha;
                    }

                    float edgeGain = 1.0f;
                    if (fadeInSamps > 4 && (posInt - startSample) < fadeInSamps)
                        edgeGain *= static_cast<float> (posInt - startSample) / static_cast<float> (fadeInSamps);
                    if (fadeOutSamps > 4 && (endSample - posInt) < fadeOutSamps)
                        edgeGain *= static_cast<float> (endSample - posInt) / static_cast<float> (fadeOutSamps);

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

                    float totalAmp = v.envVal * edgeGain;
                    gL[i] += sL * totalAmp * v.gainL;
                    gR[i] += sR * totalAmp * v.gainR;
                    v.currentPos += (v.playingReverse ? -v.pitchRatio : v.pitchRatio);
                }
            }
        }

        if (! bypassAllFx)
            for (auto& slot : grp.fxSlots)
                slot.processStereo (gL, gR, numSamples, currentSampleRate);

        float gPanNorm = juce::jlimit (-1.0f, 1.0f, grp.pan50 / 50.0f);
        float panAngle = (gPanNorm + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        float gLin = vol100ToLinearGain (grp.volumeDb100);
        float gGL = gLin * std::cos (panAngle) * 1.41421356f;
        float gGR = gLin * std::sin (panAngle) * 1.41421356f;

        for (int i = 0; i < numSamples; ++i)
        {
            outL[i] += gL[i] * gGL;
            outR[i] += gR[i] * gGR;
        }
    }

    float blockPeak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        blockPeak = std::max (blockPeak, std::max (std::abs (outL[i]), std::abs (outR[i])));

    {
        const juce::SpinLock::ScopedLockType ssl (scopeLock);
        if (blockPeak > 0.0005f)
        {
            silentSamplesHold = 0;
            int step = std::max (1, numSamples / 4);
            for (int i = 0; i < numSamples; i += step)
            {
                float mono = 0.5f * (outL[i] + outR[i]);
                scopeRing[static_cast<size_t> (scopeWriteIdx)] = juce::jlimit (-1.0f, 1.0f, mono);
                scopeWriteIdx = (scopeWriteIdx + 1) % scopeHistorySize;
            }
        }
        else
        {
            silentSamplesHold += numSamples;
            if (silentSamplesHold >= static_cast<int> (currentSampleRate * 0.14))
                scopeRing.fill (0.0f);
        }
    }
}

bool RythmeAudioProcessor::savePresetMn6x (const juce::File& file)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("presetName", currentPresetName);
    xml->setAttribute ("libraryName", libraryNameText);
    xml->setAttribute ("brandLetters", brandLetters);
    xml->setAttribute ("pluginTitleText", pluginTitleText);
    xml->setAttribute ("logoStyleId", logoStyleId);
    xml->setAttribute ("logoColourIdx", logoColourIdx);
    xml->setAttribute ("knobStyleId", knobStyleId);
    xml->setAttribute ("knobColourIdx", knobColourIdx);
    xml->setAttribute ("faderStyleId", faderStyleId);
    xml->setAttribute ("faderColourIdx", faderColourIdx);
    xml->setAttribute ("switchStyleId", switchStyleId);
    xml->setAttribute ("switchColourIdx", switchColourIdx);

    const juce::ScopedLock sl (zoneLock);
    auto* groupsXml = xml->createNewChildElement ("GROUPS");
    for (const auto& g : groups)
    {
        auto* gx = groupsXml->createNewChildElement ("GROUP");
        gx->setAttribute ("name", g.name);
        gx->setAttribute ("muted", g.muted ? 1 : 0);
        gx->setAttribute ("soloed", g.soloed ? 1 : 0);
        gx->setAttribute ("ksNote", g.keyswitchNote);
        gx->setAttribute ("vol100", g.volumeDb100);
        gx->setAttribute ("pan50", g.pan50);
        for (int s = 0; s < SampleGroup::numFxSlots; ++s)
        {
            const auto& slot = g.fxSlots[static_cast<size_t> (s)];
            auto* sx = gx->createNewChildElement ("FX");
            sx->setAttribute ("idx", s);
            sx->setAttribute ("type", static_cast<int> (slot.type));
            sx->setAttribute ("en", slot.enabled ? 1 : 0);
            sx->setAttribute ("p1", slot.p1); sx->setAttribute ("p2", slot.p2);
            sx->setAttribute ("p3", slot.p3); sx->setAttribute ("p4", slot.p4); sx->setAttribute ("p5", slot.p5);
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
        zx->setAttribute ("autoZLoop", z.autoLoopZeroCross ? 1 : 0);
        zx->setAttribute ("lXfade", z.loopCrossfade);
        zx->setAttribute ("fIn", z.fadeInNorm);
        zx->setAttribute ("fOut", z.fadeOutNorm);
        zx->setAttribute ("rev", z.reverse ? 1 : 0);
        zx->setAttribute ("pitch", z.pitchSemi);
        zx->setAttribute ("pan50", z.pan50);
        zx->setAttribute ("vol100", z.volumeDb100);
        zx->setAttribute ("atkMs", z.attackMs);
        zx->setAttribute ("decMs", z.decayMs);
        zx->setAttribute ("susPct", z.sustainPct);
        zx->setAttribute ("relMs", z.releaseMs);
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
    libraryNameText = xml->getStringAttribute ("libraryName", "KINETIK");
    brandLetters = xml->getStringAttribute ("brandLetters", "N6");
    pluginTitleText = xml->getStringAttribute ("pluginTitleText", "KINETIK");
    logoStyleId = juce::jlimit (1, 4, xml->getIntAttribute ("logoStyleId", 1));
    logoColourIdx = juce::jlimit (0, numPaletteColours - 1, xml->getIntAttribute ("logoColourIdx", 11));
    knobStyleId = juce::jlimit (1, 10, xml->getIntAttribute ("knobStyleId", 1));
    knobColourIdx = juce::jlimit (0, numPaletteColours - 1, xml->getIntAttribute ("knobColourIdx", 11));
    faderStyleId = juce::jlimit (1, 10, xml->getIntAttribute ("faderStyleId", 1));
    faderColourIdx = juce::jlimit (0, numPaletteColours - 1, xml->getIntAttribute ("faderColourIdx", 11));
    switchStyleId = juce::jlimit (1, 10, xml->getIntAttribute ("switchStyleId", 1));
    switchColourIdx = juce::jlimit (0, numPaletteColours - 1, xml->getIntAttribute ("switchColourIdx", 11));
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
            g.soloed = gx->getIntAttribute ("soloed", 0) != 0;
            g.keyswitchNote = gx->getIntAttribute ("ksNote", -1);
            g.volumeDb100 = static_cast<float> (gx->getDoubleAttribute ("vol100", 80.0));
            g.pan50 = static_cast<float> (gx->getDoubleAttribute ("pan50", 0.0));
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
                    slot.p4 = static_cast<float> (sx->getDoubleAttribute ("p4", 0.5));
                    slot.p5 = static_cast<float> (sx->getDoubleAttribute ("p5", 0.5));
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
                z.colourIndex = zx->getIntAttribute ("colIdx", 11);
                z.colour = getPaletteColour (z.colourIndex);
                z.realRREnabled = zx->getIntAttribute ("realRR", 1) != 0;

                z.startNorm = static_cast<float> (zx->getDoubleAttribute ("start", 0.0));
                z.endNorm = static_cast<float> (zx->getDoubleAttribute ("end", 1.0));
                z.loopStartNorm = static_cast<float> (zx->getDoubleAttribute ("lStart", 0.15));
                z.loopEndNorm = static_cast<float> (zx->getDoubleAttribute ("lEnd", 0.85));
                z.sampleLoop = zx->getIntAttribute ("sLoop", 0) != 0;
                z.autoLoopZeroCross = zx->getIntAttribute ("autoZLoop", 1) != 0;
                z.loopCrossfade = static_cast<float> (zx->getDoubleAttribute ("lXfade", 0.04));
                z.fadeInNorm = static_cast<float> (zx->getDoubleAttribute ("fIn", 0.0));
                z.fadeOutNorm = static_cast<float> (zx->getDoubleAttribute ("fOut", 0.0));
                z.reverse = zx->getIntAttribute ("rev", 0) != 0;
                z.pitchSemi = static_cast<float> (zx->getDoubleAttribute ("pitch", 0.0));
                z.pan50 = static_cast<float> (zx->getDoubleAttribute ("pan50", 0.0));
                z.volumeDb100 = static_cast<float> (zx->getDoubleAttribute ("vol100", 80.0));
                z.attackMs = static_cast<float> (zx->getDoubleAttribute ("atkMs", 0.10));
                z.decayMs = static_cast<float> (zx->getDoubleAttribute ("decMs", 300.0));
                z.sustainPct = static_cast<float> (zx->getDoubleAttribute ("susPct", 85.0));
                z.releaseMs = static_cast<float> (zx->getDoubleAttribute ("relMs", 250.0));
                z.cutoffHz = static_cast<float> (zx->getDoubleAttribute ("cut", 20000.0));
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
