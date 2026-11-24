#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

static juce::Colour getFaderThemeColour (int styleId)
{
    static const juce::Colour cols[10] = {
        juce::Colour (0xff4ca3ff), juce::Colour (0xffff5599), juce::Colour (0xff4cc98f),
        juce::Colour (0xffe8a138), juce::Colour (0xffa66cff), juce::Colour (0xff38c4d9),
        juce::Colour (0xffee5253), juce::Colour (0xff2eb872), juce::Colour (0xffe04f8a),
        juce::Colour (0xffd9c838)
    };
    return cols[juce::jlimit (0, 9, styleId - 1)];
}

// ---------------- HorizontalFader ----------------
HorizontalFader::HorizontalFader (const juce::String& labelText)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 15);
    applyFaderStyle (1);
    addAndMakeVisible (slider);

    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, juce::Colour (0xffc9d4e8));
    addAndMakeVisible (label);
}

void HorizontalFader::applyFaderStyle (int styleId)
{
    auto c = getFaderThemeColour (styleId);
    slider.setColour (juce::Slider::trackColourId, c);
    slider.setColour (juce::Slider::thumbColourId, c.brighter (0.35f));
    slider.setColour (juce::Slider::backgroundColourId, (styleId % 2 == 0) ? juce::Colour (0xff191e2b) : juce::Colour (0xff121720));
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void HorizontalFader::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (13));
    slider.setBounds (r);
}

// ---------------- BrandLogoHeader ----------------
BrandLogoHeader::BrandLogoHeader (RythmeAudioProcessor& p, std::function<void()> onClickMenu)
    : processor (p), onClick (std::move (onClickMenu)) {}

void BrandLogoHeader::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff141923));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (juce::Colour (0xff364661));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    auto badge = r.removeFromLeft (46.0f).reduced (5.0f, 4.0f);
    float cx = badge.getCentreX();
    float cy = badge.getCentreY();
    float rx = badge.getWidth() * 0.46f;
    float ry = badge.getHeight() * 0.46f;

    int lStyle = juce::jlimit (1, 4, processor.logoStyleId);
    if (lStyle == 1)
    {
        juce::Path shield;
        shield.startNewSubPath (cx, cy - ry);
        shield.lineTo (cx + rx, cy - ry * 0.45f);
        shield.lineTo (cx + rx * 0.85f, cy + ry * 0.55f);
        shield.lineTo (cx, cy + ry);
        shield.lineTo (cx - rx * 0.85f, cy + ry * 0.55f);
        shield.lineTo (cx - rx, cy - ry * 0.45f);
        shield.closeSubPath();
        g.setColour (juce::Colour (0xff2b5876));
        g.fillPath (shield);
        g.setColour (juce::Colour (0xff66b3ff));
        g.strokePath (shield, juce::PathStrokeType (1.4f));
    }
    else if (lStyle == 2)
    {
        g.setColour (juce::Colour (0xff3b235a));
        g.fillRoundedRectangle (badge, 8.0f);
        g.setColour (juce::Colour (0xffff66aa));
        g.drawRoundedRectangle (badge.reduced (0.5f), 8.0f, 1.5f);
    }
    else if (lStyle == 3)
    {
        juce::Path hex;
        for (int i = 0; i < 6; ++i)
        {
            float ang = juce::MathConstants<float>::pi / 3.0f * i - juce::MathConstants<float>::pi * 0.5f;
            float px = cx + rx * std::cos (ang);
            float py = cy + ry * std::sin (ang);
            if (i == 0) hex.startNewSubPath (px, py); else hex.lineTo (px, py);
        }
        hex.closeSubPath();
        g.setColour (juce::Colour (0xff1d3d35));
        g.fillPath (hex);
        g.setColour (juce::Colour (0xff4cc98f));
        g.strokePath (hex, juce::PathStrokeType (1.4f));
    }
    else
    {
        g.setColour (juce::Colour (0xff472a18));
        g.fillEllipse (badge);
        g.setColour (juce::Colour (0xffe8a138));
        g.drawEllipse (badge.reduced (0.5f), 1.5f);
    }

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (13.5f, juce::Font::bold));
    g.drawText (processor.brandLetters.isEmpty() ? "N6" : processor.brandLetters, badge, juce::Justification::centred, false);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText (processor.pluginTitleText.isEmpty() ? "RYTHME" : processor.pluginTitleText,
                r.reduced (4.0f, 0.0f), juce::Justification::centredLeft, true);
}

// ---------------- MountedSoundCard ----------------
MountedSoundCard::MountedSoundCard (std::function<void()> onCloseClicked) : onClose (std::move (onCloseClicked))
{
    closeBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff5c2633));
    closeBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    closeBtn.onClick = [this]() { if (onClose) onClose(); };
    addAndMakeVisible (closeBtn);
}

void MountedSoundCard::setItem (const juce::String& name, bool isActive)
{
    itemName = name;
    active = isActive;
    closeBtn.setVisible (active);
    repaint();
}

void MountedSoundCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (active ? juce::Colour (0xff1d283a) : juce::Colour (0xff141821));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (active ? juce::Colour (0xff4ca3ff) : juce::Colour (0xff293242));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);

    if (active)
    {
        g.setColour (juce::Colour (0xff74b9ff));
        g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
        g.drawText ("LOADED", 8, 3, getWidth() - 34, 11, juce::Justification::centredLeft, true);

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText (itemName, 8, 15, getWidth() - 34, 18, juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour (juce::Colour (0xff64728a));
        g.setFont (juce::FontOptions (10.0f, juce::Font::plain));
        g.drawText ("Empty (Click Kick/Item)", r.reduced (6.0f), juce::Justification::centred, true);
    }
}

void MountedSoundCard::resized()
{
    auto r = getLocalBounds().reduced (4);
    closeBtn.setBounds (r.removeFromRight (22).withSizeKeepingCentre (22, 22));
}

// ---------------- LiveMainWaveDisplay ----------------
LiveMainWaveDisplay::LiveMainWaveDisplay (RythmeAudioProcessor& p) : processor (p) {}

void LiveMainWaveDisplay::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll (juce::Colour (0xff111620));
    g.setColour (juce::Colour (0xff2c374b));
    g.drawRect (area, 1);

    const juce::ScopedLock sl (processor.zoneLock);
    int zIdx = processor.selectedZoneIndex;
    if (zIdx < 0 || zIdx >= static_cast<int> (processor.zones.size()))
    {
        g.setColour (juce::Colour (0xff5d6b82));
        g.setFont (11.0f);
        g.drawText ("PLAYING WAVEFORM", area, juce::Justification::centred);
        return;
    }

    const auto& z = processor.zones[static_cast<size_t> (zIdx)];
    if (z.layers.empty()) return;

    int lIdx = juce::jlimit (0, static_cast<int> (z.layers.size()) - 1, z.selectedLayerIndex);
    const auto& buf = z.layers[static_cast<size_t> (lIdx)].buffer;
    int numSamples = buf.getNumSamples();
    if (numSamples <= 1) return;

    const float* data = buf.getReadPointer (0);
    float midY = area.getCentreY();
    float halfH = area.getHeight() * 0.42f;

    g.setColour (juce::Colour (0xff1d2433));
    g.drawHorizontalLine (static_cast<int> (midY), static_cast<float> (area.getX()), static_cast<float> (area.getRight()));

    juce::Path path;
    path.startNewSubPath (static_cast<float> (area.getX()), midY);
    int step = std::max (1, numSamples / std::max (1, area.getWidth()));
    for (int x = 0; x < area.getWidth(); ++x)
    {
        int idx = std::min (numSamples - 1, x * step);
        path.lineTo (static_cast<float> (area.getX() + x), midY - data[idx] * halfH);
    }
    g.setColour (z.colour.brighter (0.3f));
    g.strokePath (path, juce::PathStrokeType (1.4f));

    float phNorm = processor.getSelectedZonePlayheadNorm();
    if (phNorm >= 0.0f && phNorm <= 1.0f)
    {
        float px = area.getX() + phNorm * area.getWidth();
        g.setColour (juce::Colour (0x55ffffff));
        g.fillRect (static_cast<float> (area.getX()), static_cast<float> (area.getY()), px - area.getX(), static_cast<float> (area.getHeight()));
        g.setColour (juce::Colours::white);
        g.drawVerticalLine (static_cast<int> (px), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
    }
}

// ---------------- ColorPianoStrip ----------------
ColorPianoStrip::ColorPianoStrip (RythmeAudioProcessor& p) : processor (p) {}

void ColorPianoStrip::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll (juce::Colour (0xff12161e));
    float keyW = area.getWidth() / static_cast<float> (numKeys);
    int activeNote = processor.lastPlayedNote.load();
    int activeKs = processor.activeKeyswitchNote.load();

    const juce::ScopedLock sl (processor.zoneLock);
    for (int i = 0; i < numKeys; ++i)
    {
        int note = startNote + i;
        int n = note % 12;
        bool isBlack = (n == 1 || n == 3 || n == 6 || n == 8 || n == 10);
        juce::Rectangle<float> kr (area.getX() + i * keyW, static_cast<float> (area.getY()), keyW, static_cast<float> (area.getHeight()));

        bool isKs = processor.isKeyswitchNote (note);
        juce::Colour mappedCol = juce::Colours::transparentBlack;
        bool isMapped = false;
        for (const auto& z : processor.zones)
        {
            if (note >= z.lowKey && note <= z.highKey)
            {
                mappedCol = z.colour;
                isMapped = true;
            }
        }

        if (note == pressedNote || note == activeNote)
            g.setColour (juce::Colours::white);
        else if (isKs)
            g.setColour (note == activeKs ? juce::Colour (0xffff3b3b) : juce::Colour (0xffb82525));
        else if (isMapped)
            g.setColour (isBlack ? mappedCol.darker (0.35f) : mappedCol);
        else
            g.setColour (isBlack ? juce::Colour (0xff191e28) : juce::Colour (0xffcfd7e3));

        g.fillRect (kr);
        g.setColour (juce::Colour (0xff232a36));
        g.drawRect (kr, 0.5f);

        if (n == 0)
        {
            int oct = note / 12;
            g.setColour ((isMapped || isKs) ? juce::Colours::white : juce::Colours::darkgrey);
            g.setFont (7.5f);
            g.drawText ("C" + juce::String (oct), kr.expanded (5.0f, 0.0f), juce::Justification::centredBottom, false);
        }
    }
}

void ColorPianoStrip::mouseDown (const juce::MouseEvent& e)
{
    int note = startNote + static_cast<int> ((e.x / static_cast<float> (std::max (1, getWidth()))) * numKeys);
    note = juce::jlimit (startNote, startNote + numKeys - 1, note);
    pressedNote = note;
    processor.uiNoteOn (note, 0.9f);
    repaint();
}

void ColorPianoStrip::mouseUp (const juce::MouseEvent&)
{
    if (pressedNote >= 0) { processor.uiNoteOff (pressedNote); pressedNote = -1; repaint(); }
}

// ---------------- KeyMapperComponent ----------------
KeyMapperComponent::KeyMapperComponent (RythmeAudioProcessor& p) : processor (p) {}

bool KeyMapperComponent::isZoneVisibleInMapper (const SampleZone& z) const
{
    if (! readOnly && filterToSelectedGroup)
        return z.groupIndex == processor.selectedGroupIndex;
    return true;
}

void KeyMapperComponent::sanitizeSelection()
{
    int n = static_cast<int> (processor.zones.size());
    for (auto it = selectedZoneIndices.begin(); it != selectedZoneIndices.end(); )
    {
        if (*it < 0 || *it >= n || ! isZoneVisibleInMapper (processor.zones[static_cast<size_t> (*it)]))
            it = selectedZoneIndices.erase (it);
        else
            ++it;
    }
    if (selectedZoneIndices.empty() && processor.selectedZoneIndex >= 0 && processor.selectedZoneIndex < n)
        if (isZoneVisibleInMapper (processor.zones[static_cast<size_t> (processor.selectedZoneIndex)]))
            selectedZoneIndices.insert (processor.selectedZoneIndex);
}

void KeyMapperComponent::selectAllZones()
{
    const juce::ScopedLock sl (processor.zoneLock);
    selectedZoneIndices.clear();
    int n = static_cast<int> (processor.zones.size());
    for (int i = 0; i < n; ++i)
    {
        if (isZoneVisibleInMapper (processor.zones[static_cast<size_t> (i)]))
        {
            selectedZoneIndices.insert (i);
            processor.selectedZoneIndex = i;
        }
    }
    if (onSelectionChanged) onSelectionChanged();
    repaint();
}

void KeyMapperComponent::deleteSelectedZones()
{
    {
        const juce::ScopedLock sl (processor.zoneLock);
        sanitizeSelection();
        if (! selectedZoneIndices.empty())
        {
            std::vector<int> toDel (selectedZoneIndices.begin(), selectedZoneIndices.end());
            processor.deleteZones (toDel);
            selectedZoneIndices.clear();
        }
        else if (processor.selectedZoneIndex >= 0)
            processor.deleteZone (processor.selectedZoneIndex);
    }
    if (onSelectionChanged) onSelectionChanged();
    repaint();
}

void KeyMapperComponent::clearSelection() { selectedZoneIndices.clear(); repaint(); }

void KeyMapperComponent::selectSingleZone (int idx)
{
    selectedZoneIndices.clear();
    if (idx >= 0) selectedZoneIndices.insert (idx);
    repaint();
}

void KeyMapperComponent::autoGridSelectedZones()
{
    const juce::ScopedLock sl (processor.zoneLock);
    sanitizeSelection();
    if (selectedZoneIndices.empty()) return;

    std::vector<int> ordered (selectedZoneIndices.begin(), selectedZoneIndices.end());
    std::sort (ordered.begin(), ordered.end(), [this] (int a, int b)
    {
        const auto& za = processor.zones[static_cast<size_t> (a)];
        const auto& zb = processor.zones[static_cast<size_t> (b)];
        if (za.lowVel != zb.lowVel) return za.lowVel < zb.lowVel;
        return a < b;
    });

    int count = static_cast<int> (ordered.size());
    if (count == 1)
    {
        auto& z = processor.zones[static_cast<size_t> (ordered[0])];
        z.lowVel = 0; z.highVel = 127;
    }
    else
    {
        for (int i = 0; i < count; ++i)
        {
            auto& z = processor.zones[static_cast<size_t> (ordered[static_cast<size_t> (i)])];
            z.lowVel = static_cast<int> (std::round ((i * 127.0f) / count));
            z.highVel = static_cast<int> (std::round (((i + 1) * 127.0f) / count));
            if (i > 0 && z.lowVel < 127) z.lowVel += 1;
        }
    }
    if (onSelectionChanged) onSelectionChanged();
    repaint();
}

juce::Rectangle<int> KeyMapperComponent::getGridArea() const
{
    auto r = getLocalBounds();
    r.removeFromLeft (readOnly ? 0 : 26);
    return r.removeFromTop (r.getHeight() - (readOnly ? 24 : 28));
}

juce::Rectangle<int> KeyMapperComponent::getKeyboardArea() const
{
    auto r = getLocalBounds();
    r.removeFromLeft (readOnly ? 0 : 26);
    return r.removeFromBottom (readOnly ? 24 : 28);
}

int KeyMapperComponent::noteAtX (int x) const
{
    auto ka = getKeyboardArea();
    int relX = x - ka.getX();
    int note = startNote + static_cast<int> ((relX / static_cast<float> (std::max (1, ka.getWidth()))) * numKeys);
    return juce::jlimit (startNote, startNote + numKeys - 1, note);
}

int KeyMapperComponent::snapVelocity (int rawVel) const
{
    rawVel = juce::jlimit (0, 127, rawVel);
    if (velGridDivs <= 0) return rawVel;
    float step = 127.0f / static_cast<float> (velGridDivs);
    return juce::jlimit (0, 127, static_cast<int> (std::round (rawVel / step) * step));
}

juce::Rectangle<float> KeyMapperComponent::getZoneRect (const SampleZone& z, juce::Rectangle<int> gridArea) const
{
    float x1 = gridArea.getX() + ((z.lowKey - startNote) / static_cast<float> (numKeys)) * gridArea.getWidth();
    float x2 = gridArea.getX() + ((z.highKey - startNote + 1) / static_cast<float> (numKeys)) * gridArea.getWidth();
    float y1 = gridArea.getBottom() - (z.highVel / 127.0f) * gridArea.getHeight();
    float y2 = gridArea.getBottom() - (z.lowVel / 127.0f) * gridArea.getHeight();
    return { x1, y1, std::max (8.0f, x2 - x1), std::max (10.0f, y2 - y1) };
}

void KeyMapperComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll (juce::Colour (0xff161b24));
    g.setColour (juce::Colour (0xff2e3748));
    g.drawRect (area, 1);

    auto gridArea = getGridArea();
    auto keyArea = getKeyboardArea();

    int divs = readOnly ? 4 : (velGridDivs > 0 ? velGridDivs : 4);
    for (int i = 0; i <= divs; ++i)
    {
        float norm = i / static_cast<float> (divs);
        int y = gridArea.getBottom() - static_cast<int> (norm * gridArea.getHeight());
        g.setColour (juce::Colour (0xff222936));
        g.drawHorizontalLine (y, static_cast<float> (gridArea.getX()), static_cast<float> (gridArea.getRight()));
        if (! readOnly && (divs <= 8 || i % 2 == 0))
        {
            g.setColour (juce::Colour (0xff8a99b3));
            g.setFont (8.5f);
            g.drawText (juce::String (static_cast<int> (std::round (norm * 127.0f))), 2, y - 6, 22, 12, juce::Justification::centredRight, false);
        }
    }

    float keyW = gridArea.getWidth() / static_cast<float> (numKeys);
    g.setColour (juce::Colour (0xff232b38));
    for (int i = 0; i < numKeys; ++i)
        if ((startNote + i) % 12 == 0)
            g.drawVerticalLine (static_cast<int> (gridArea.getX() + i * keyW), static_cast<float> (gridArea.getY()), static_cast<float> (gridArea.getBottom()));

    const juce::ScopedLock sl (processor.zoneLock);
    sanitizeSelection();

    int visibleZones = 0;
    for (const auto& z : processor.zones)
        if (isZoneVisibleInMapper (z)) ++visibleZones;

    if (visibleZones == 0)
    {
        g.setColour (juce::Colour (0xff78849b));
        g.setFont (11.0f);
        g.drawText (readOnly ? "Switch to EDIT tab to map samples"
                             : "Drag & Drop WAV/AIFF onto any key | Box-drag to multi-select | Right-Click key = Root",
                    gridArea, juce::Justification::centred);
    }

    int selectedRootKey = -1;
    if (processor.selectedZoneIndex >= 0 && processor.selectedZoneIndex < static_cast<int> (processor.zones.size()))
        selectedRootKey = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)].rootKey;

    for (int i = 0; i < static_cast<int> (processor.zones.size()); ++i)
    {
        const auto& z = processor.zones[static_cast<size_t> (i)];
        if (! isZoneVisibleInMapper (z)) continue;

        auto r = getZoneRect (z, gridArea);
        bool isSelected = (! readOnly && selectedZoneIndices.count (i) > 0) || (readOnly && i == processor.selectedZoneIndex);

        g.setColour (z.colour.withAlpha (isSelected ? 0.82f : 0.60f));
        g.fillRect (r);
        g.setColour (isSelected ? juce::Colours::white : z.colour.brighter (0.25f));
        g.drawRect (r, isSelected ? 2.0f : 1.0f);

        juce::String label = z.name;
        if (z.layers.size() > 1) label += " (" + juce::String (z.layers.size()) + "RR)";
        g.setColour (juce::Colours::white);
        g.setFont (9.0f);
        g.drawText (label, r.reduced (3.0f, 1.0f), juce::Justification::topLeft, true);
    }

    if (! readOnly && isMarqueeDragging)
    {
        auto mr = juce::Rectangle<float>::leftTopRightBottom (
            std::min (marqueeStart.x, marqueeCurrent.x), std::min (marqueeStart.y, marqueeCurrent.y),
            std::max (marqueeStart.x, marqueeCurrent.x), std::max (marqueeStart.y, marqueeCurrent.y));
        g.setColour (juce::Colour (0x305aa9ff)); g.fillRect (mr);
        g.setColour (juce::Colour (0xff80c4ff)); g.drawRect (mr, 1.2f);
    }

    int activeNote = processor.lastPlayedNote.load();
    int activeKs = processor.activeKeyswitchNote.load();
    for (int i = 0; i < numKeys; ++i)
    {
        int note = startNote + i;
        int n = note % 12;
        bool isBlack = (n == 1 || n == 3 || n == 6 || n == 8 || n == 10);
        juce::Rectangle<float> kr (keyArea.getX() + i * keyW, static_cast<float> (keyArea.getY()), keyW, static_cast<float> (keyArea.getHeight()));

        bool isKs = processor.isKeyswitchNote (note);
        juce::Colour mappedCol = juce::Colours::transparentBlack;
        bool isMapped = false;
        for (const auto& z : processor.zones)
            if (isZoneVisibleInMapper (z) && note >= z.lowKey && note <= z.highKey) { mappedCol = z.colour; isMapped = true; }

        if (note == pressedKey || note == activeNote) g.setColour (juce::Colours::white);
        else if (isKs) g.setColour (note == activeKs ? juce::Colour (0xffff3b3b) : juce::Colour (0xffb82525));
        else if (! readOnly && note == selectedRootKey) g.setColour (juce::Colour (0xffffd32a));
        else if (isMapped) g.setColour (isBlack ? mappedCol.darker (0.35f) : mappedCol);
        else g.setColour (isBlack ? juce::Colour (0xff151921) : juce::Colour (0xffd8e0eb));

        g.fillRect (kr);
        g.setColour (juce::Colour (0xff272e3a));
        g.drawRect (kr, 0.5f);

        if (! readOnly && note == selectedRootKey)
        {
            g.setColour (juce::Colours::black);
            g.setFont (juce::FontOptions (8.0f, juce::Font::bold));
            g.drawText ("R", kr.expanded (3.0f, 0.0f), juce::Justification::centredTop, false);
        }
        if (n == 0)
        {
            g.setColour ((isMapped || isKs) ? juce::Colours::white : juce::Colours::darkgrey);
            g.setFont (7.5f);
            g.drawText ("C" + juce::String (note / 12), kr.expanded (5.0f, 0.0f), juce::Justification::centredBottom, false);
        }
    }
}

void KeyMapperComponent::mouseDown (const juce::MouseEvent& e)
{
    auto gridArea = getGridArea();
    auto keyArea = getKeyboardArea();

    if (keyArea.contains (e.getPosition()))
    {
        int note = noteAtX (e.x);
        if (! readOnly && e.mods.isRightButtonDown())
        {
            const juce::ScopedLock sl (processor.zoneLock);
            sanitizeSelection();
            for (int idx : selectedZoneIndices)
                if (idx >= 0 && idx < static_cast<int> (processor.zones.size()))
                    processor.zones[static_cast<size_t> (idx)].rootKey = note;
            if (onSelectionChanged) onSelectionChanged();
            repaint();
            return;
        }
        pressedKey = note;
        processor.uiNoteOn (note, 0.9f);
        if (onSelectionChanged) onSelectionChanged();
        repaint();
        return;
    }

    const juce::ScopedLock sl (processor.zoneLock);
    sanitizeSelection();
    dragMode = DragMode::None;
    dragZoneIdx = -1;
    dragSnapshots.clear();
    isMarqueeDragging = false;

    for (int i = static_cast<int> (processor.zones.size()) - 1; i >= 0; --i)
    {
        if (! isZoneVisibleInMapper (processor.zones[static_cast<size_t> (i)])) continue;

        auto r = getZoneRect (processor.zones[static_cast<size_t> (i)], gridArea);
        if (r.expanded (4.0f).contains (e.position))
        {
            if (readOnly || e.mods.isRightButtonDown())
            {
                processor.selectedZoneIndex = i;
                selectedZoneIndices.clear();
                selectedZoneIndices.insert (i);
                if (onSelectionChanged) onSelectionChanged();
                repaint();
                return;
            }

            if (e.mods.isShiftDown() || e.mods.isCommandDown())
            {
                if (selectedZoneIndices.count (i) > 0 && selectedZoneIndices.size() > 1) selectedZoneIndices.erase (i);
                else selectedZoneIndices.insert (i);
            }
            else if (selectedZoneIndices.count (i) == 0)
            {
                selectedZoneIndices.clear();
                selectedZoneIndices.insert (i);
            }

            processor.selectedZoneIndex = i;
            dragZoneIdx = i;
            dragStartNote = noteAtX (e.x);

            for (int idx : selectedZoneIndices)
                if (idx >= 0 && idx < static_cast<int> (processor.zones.size()))
                {
                    const auto& z = processor.zones[static_cast<size_t> (idx)];
                    dragSnapshots.push_back ({ idx, z.lowKey, z.highKey });
                }

            if (std::abs (e.position.x - r.getX()) <= 6.0f) dragMode = DragMode::LeftEdge;
            else if (std::abs (e.position.x - r.getRight()) <= 6.0f) dragMode = DragMode::RightEdge;
            else if (std::abs (e.position.y - r.getY()) <= 7.0f) dragMode = DragMode::TopEdge;
            else if (std::abs (e.position.y - r.getBottom()) <= 7.0f) dragMode = DragMode::BottomEdge;
            else dragMode = DragMode::Move;

            if (onSelectionChanged) onSelectionChanged();
            repaint();
            return;
        }
    }

    if (! readOnly && ! e.mods.isRightButtonDown() && gridArea.contains (e.getPosition()))
    {
        if (! e.mods.isShiftDown() && ! e.mods.isCommandDown()) selectedZoneIndices.clear();
        isMarqueeDragging = true;
        marqueeStart = e.position;
        marqueeCurrent = e.position;
        repaint();
    }
}

void KeyMapperComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (readOnly) return;
    auto gridArea = getGridArea();

    if (isMarqueeDragging)
    {
        marqueeCurrent = e.position;
        auto mr = juce::Rectangle<float>::leftTopRightBottom (
            std::min (marqueeStart.x, marqueeCurrent.x), std::min (marqueeStart.y, marqueeCurrent.y),
            std::max (marqueeStart.x, marqueeCurrent.x), std::max (marqueeStart.y, marqueeCurrent.y));
        const juce::ScopedLock sl (processor.zoneLock);
        selectedZoneIndices.clear();
        for (int i = 0; i < static_cast<int> (processor.zones.size()); ++i)
            if (isZoneVisibleInMapper (processor.zones[static_cast<size_t> (i)]) && mr.intersects (getZoneRect (processor.zones[static_cast<size_t> (i)], gridArea)))
            {
                selectedZoneIndices.insert (i);
                processor.selectedZoneIndex = i;
            }
        if (onSelectionChanged) onSelectionChanged();
        repaint();
        return;
    }

    if (dragMode == DragMode::None || dragZoneIdx < 0) return;
    const juce::ScopedLock sl (processor.zoneLock);
    if (dragZoneIdx >= static_cast<int> (processor.zones.size())) return;

    int curNote = juce::jlimit (0, 120, noteAtX (e.x));
    int deltaKey = curNote - dragStartNote;
    int rawVel = juce::jlimit (0, 127, static_cast<int> ((1.0f - (e.y - gridArea.getY()) / static_cast<float> (std::max (1, gridArea.getHeight()))) * 127.0f));
    int curVel = e.mods.isShiftDown() ? rawVel : snapVelocity (rawVel);

    if (dragMode == DragMode::Move)
    {
        int minDelta = -120, maxDelta = 120;
        for (const auto& s : dragSnapshots) { minDelta = std::max (minDelta, -s.lowKey); maxDelta = std::min (maxDelta, 120 - s.highKey); }
        int d = juce::jlimit (minDelta, maxDelta, deltaKey);
        for (const auto& s : dragSnapshots)
        {
            auto& z = processor.zones[static_cast<size_t> (s.index)];
            z.lowKey = s.lowKey + d; z.highKey = s.highKey + d;
        }
    }
    else if (dragMode == DragMode::LeftEdge)
    {
        for (const auto& s : dragSnapshots)
        {
            auto& z = processor.zones[static_cast<size_t> (s.index)];
            z.lowKey = juce::jlimit (0, z.highKey, s.lowKey + deltaKey);
        }
    }
    else if (dragMode == DragMode::RightEdge)
    {
        for (const auto& s : dragSnapshots)
        {
            auto& z = processor.zones[static_cast<size_t> (s.index)];
            z.highKey = juce::jlimit (z.lowKey, 120, s.highKey + deltaKey);
        }
    }
    else if (dragMode == DragMode::TopEdge)
    {
        auto& z = processor.zones[static_cast<size_t> (dragZoneIdx)];
        z.highVel = juce::jlimit (z.lowVel + 2, 127, curVel);
    }
    else if (dragMode == DragMode::BottomEdge)
    {
        auto& z = processor.zones[static_cast<size_t> (dragZoneIdx)];
        z.lowVel = juce::jlimit (0, z.highVel - 2, curVel);
    }
    if (onSelectionChanged) onSelectionChanged();
    repaint();
}

void KeyMapperComponent::mouseUp (const juce::MouseEvent&)
{
    dragMode = DragMode::None; dragZoneIdx = -1; dragSnapshots.clear();
    if (isMarqueeDragging) { isMarqueeDragging = false; repaint(); }
    if (pressedKey >= 0) { processor.uiNoteOff (pressedKey); pressedKey = -1; repaint(); }
}

// ---------------- WaveEditorComponent ----------------
WaveEditorComponent::WaveEditorComponent (RythmeAudioProcessor& p) : processor (p) {}

void WaveEditorComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll (juce::Colour (0xff121720));
    g.setColour (juce::Colour (0xff2e3748));
    g.drawRect (area, 1);

    const juce::ScopedLock sl (processor.zoneLock);
    if (processor.zones.empty() || processor.selectedZoneIndex < 0 || processor.selectedZoneIndex >= static_cast<int> (processor.zones.size()))
    {
        g.setColour (juce::Colour (0xff78849b)); g.setFont (11.0f);
        g.drawText ("Drop WAV/AIFF onto Key Map, or drop here to add Round Robin layers", area, juce::Justification::centred);
        return;
    }

    const auto& z = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)];
    if (z.layers.empty())
    {
        g.setColour (z.colour.brighter (0.3f)); g.setFont (11.0f);
        g.drawText ("Drop WAV/AIFF sample(s) here!", area, juce::Justification::centred);
        return;
    }

    int lIdx = juce::jlimit (0, static_cast<int> (z.layers.size()) - 1, z.selectedLayerIndex);
    const auto& layer = z.layers[static_cast<size_t> (lIdx)];
    const float* data = layer.buffer.getReadPointer (0);
    int numSamples = layer.buffer.getNumSamples();
    if (numSamples <= 0) return;

    float startX = area.getX() + z.startNorm * area.getWidth();
    float endX = area.getX() + z.endNorm * area.getWidth();
    float lStartX = area.getX() + z.loopStartNorm * area.getWidth();
    float lEndX = area.getX() + z.loopEndNorm * area.getWidth();
    float spanW = std::max (4.0f, endX - startX);
    float fadeInX = startX + z.fadeInNorm * spanW;
    float fadeOutX = endX - z.fadeOutNorm * spanW;

    g.setColour (juce::Colour (0x55000000));
    g.fillRect (static_cast<float> (area.getX()), static_cast<float> (area.getY()), startX - area.getX(), static_cast<float> (area.getHeight()));
    g.fillRect (endX, static_cast<float> (area.getY()), area.getRight() - endX, static_cast<float> (area.getHeight()));

    if (z.sampleLoop && lEndX > lStartX)
    {
        g.setColour (juce::Colour (0x334cc98f));
        g.fillRect (lStartX, static_cast<float> (area.getY()), lEndX - lStartX, static_cast<float> (area.getHeight()));
    }

    juce::Path p;
    float midY = area.getCentreY();
    float halfH = area.getHeight() * 0.40f;
    p.startNewSubPath (static_cast<float> (area.getX()), midY);
    int step = std::max (1, numSamples / std::max (1, area.getWidth()));
    for (int x = 0; x < area.getWidth(); ++x)
    {
        int idx = std::min (numSamples - 1, x * step);
        p.lineTo (static_cast<float> (area.getX() + x), midY - data[idx] * halfH);
    }
    g.setColour (z.colour.brighter (0.25f));
    g.strokePath (p, juce::PathStrokeType (1.2f));

    float topLineY = midY - halfH;
    if (z.fadeInNorm > 0.001f)
    {
        g.setColour (juce::Colour (0xffffd32a));
        g.drawLine (startX, midY, fadeInX, topLineY, 1.5f);
    }
    if (z.fadeOutNorm > 0.001f)
    {
        g.setColour (juce::Colour (0xffffd32a));
        g.drawLine (fadeOutX, topLineY, endX, midY, 1.5f);
    }

    g.setColour (juce::Colour (0xffffd32a));
    g.fillEllipse (fadeInX - 5.0f, topLineY - 5.0f, 10.0f, 10.0f);
    g.fillEllipse (fadeOutX - 5.0f, topLineY - 5.0f, 10.0f, 10.0f);

    g.setColour (juce::Colour (0xffff5599));
    g.drawVerticalLine (static_cast<int> (startX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
    g.drawVerticalLine (static_cast<int> (endX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
    g.fillRect (startX, static_cast<float> (area.getY()), 14.0f, 13.0f);
    g.fillRect (endX - 14.0f, static_cast<float> (area.getY()), 14.0f, 13.0f);
    g.setColour (juce::Colours::white); g.setFont (9.0f);
    g.drawText ("S", static_cast<int> (startX), area.getY(), 14, 13, juce::Justification::centred);
    g.drawText ("E", static_cast<int> (endX - 14.0f), area.getY(), 14, 13, juce::Justification::centred);

    if (z.sampleLoop)
    {
        g.setColour (juce::Colour (0xff4cc98f));
        g.drawVerticalLine (static_cast<int> (lStartX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
        g.drawVerticalLine (static_cast<int> (lEndX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
        g.fillRect (lStartX, static_cast<float> (area.getBottom() - 14), 20.0f, 14.0f);
        g.fillRect (lEndX - 20.0f, static_cast<float> (area.getBottom() - 14), 20.0f, 14.0f);
        g.setColour (juce::Colours::black);
        g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
        g.drawText ("LS", static_cast<int> (lStartX), area.getBottom() - 14, 20, 14, juce::Justification::centred);
        g.drawText ("LE", static_cast<int> (lEndX - 20.0f), area.getBottom() - 14, 20, 14, juce::Justification::centred);
    }
}

void WaveEditorComponent::mouseDown (const juce::MouseEvent& e)
{
    const juce::ScopedLock sl (processor.zoneLock);
    if (processor.selectedZoneIndex < 0 || processor.selectedZoneIndex >= static_cast<int> (processor.zones.size())) return;
    const auto& z = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)];

    float w = static_cast<float> (std::max (1, getWidth()));
    float normX = juce::jlimit (0.0f, 1.0f, e.x / w);
    float startX = z.startNorm * w;
    float endX = z.endNorm * w;
    float spanW = std::max (4.0f, endX - startX);
    float fadeInX = startX + z.fadeInNorm * spanW;
    float fadeOutX = endX - z.fadeOutNorm * spanW;
    float topLineY = getHeight() * 0.5f - getHeight() * 0.40f;

    if (std::abs (e.x - fadeInX) <= 8.0f && std::abs (e.y - topLineY) <= 14.0f) { activeDrag = DragTarget::FadeIn; return; }
    if (std::abs (e.x - fadeOutX) <= 8.0f && std::abs (e.y - topLineY) <= 14.0f) { activeDrag = DragTarget::FadeOut; return; }

    float dS = std::abs (normX - z.startNorm);
    float dE = std::abs (normX - z.endNorm);
    float dLS = z.sampleLoop ? std::abs (normX - z.loopStartNorm) : 99.0f;
    float dLE = z.sampleLoop ? std::abs (normX - z.loopEndNorm) : 99.0f;
    float minD = std::min ({ dS, dE, dLS, dLE });

    if (minD < 0.05f)
    {
        if (minD == dS) activeDrag = DragTarget::Start;
        else if (minD == dE) activeDrag = DragTarget::End;
        else if (minD == dLS) activeDrag = DragTarget::LoopStart;
        else activeDrag = DragTarget::LoopEnd;
    }
}

void WaveEditorComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (activeDrag == DragTarget::None) return;
    const juce::ScopedLock sl (processor.zoneLock);
    if (processor.selectedZoneIndex < 0 || processor.selectedZoneIndex >= static_cast<int> (processor.zones.size())) return;
    int zIdx = processor.selectedZoneIndex;
    auto& z = processor.zones[static_cast<size_t> (zIdx)];

    float w = static_cast<float> (std::max (1, getWidth()));
    float rawNormX = juce::jlimit (0.0f, 1.0f, e.x / w);

    if (activeDrag == DragTarget::Start)
        z.startNorm = juce::jlimit (0.0f, z.endNorm - 0.02f, rawNormX);
    else if (activeDrag == DragTarget::End)
        z.endNorm = juce::jlimit (z.startNorm + 0.02f, 1.0f, rawNormX);
    else if (activeDrag == DragTarget::LoopStart)
    {
        float sn = z.autoLoopZeroCross ? processor.snapNormToZeroCrossing (zIdx, rawNormX) : rawNormX;
        z.loopStartNorm = juce::jlimit (z.startNorm, z.loopEndNorm - 0.02f, sn);
    }
    else if (activeDrag == DragTarget::LoopEnd)
    {
        float sn = z.autoLoopZeroCross ? processor.snapNormToZeroCrossing (zIdx, rawNormX) : rawNormX;
        z.loopEndNorm = juce::jlimit (z.loopStartNorm + 0.02f, z.endNorm, sn);
    }
    else if (activeDrag == DragTarget::FadeIn)
    {
        float span = std::max (0.02f, z.endNorm - z.startNorm);
        z.fadeInNorm = juce::jlimit (0.0f, 0.45f, (rawNormX - z.startNorm) / span);
    }
    else if (activeDrag == DragTarget::FadeOut)
    {
        float span = std::max (0.02f, z.endNorm - z.startNorm);
        z.fadeOutNorm = juce::jlimit (0.0f, 0.45f, (z.endNorm - rawNormX) / span);
    }

    repaint();
}

void WaveEditorComponent::mouseUp (const juce::MouseEvent&) { activeDrag = DragTarget::None; }

// ---------------- RhythmSequencerComponent ----------------
RhythmSequencerComponent::RhythmSequencerComponent (RythmeAudioProcessor& p) : processor (p) {}

float RhythmSequencerComponent::snapToGrid (float normX) const
{
    int divs = processor.getGridDivisions();
    float step = 1.0f / static_cast<float> (divs);
    return juce::jlimit (0.0f, 1.0f - step * 0.25f, std::round (normX / step) * step);
}

void RhythmSequencerComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll (juce::Colour (0xff1a1521));
    g.setColour (juce::Colour (0xff3a2d47));
    g.drawRect (area, 1);

    int divs = processor.getGridDivisions();
    int beatStep = std::max (1, divs / 4);
    for (int i = 0; i < divs; ++i)
    {
        float x = area.getX() + (i / static_cast<float> (divs)) * area.getWidth();
        g.setColour (i % beatStep == 0 ? juce::Colour (0xff524066) : juce::Colour (0xff2b2135));
        g.drawVerticalLine (static_cast<int> (x), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
    }

    if (! processor.seqBypass.load() && processor.apvts.getRawParameterValue ("seqLoop")->load() > 0.5f)
    {
        float px = area.getX() + processor.seqPlayheadNorm.load() * area.getWidth();
        g.setColour (juce::Colour (0x99ffffff));
        g.drawVerticalLine (static_cast<int> (px), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
    }

    const juce::ScopedLock sl (processor.zoneLock);
    if (processor.zones.empty() || processor.selectedZoneIndex < 0 || processor.selectedZoneIndex >= static_cast<int> (processor.zones.size()))
    {
        g.setColour (juce::Colour (0xff8a7a99)); g.setFont (11.0f);
        g.drawText ("Select a key or item to edit rhythm | Turn ON LOOP & press PLAY or Spacebar", area, juce::Justification::centred);
        return;
    }

    const auto& z = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)];
    for (int i = 0; i < static_cast<int> (z.seqEvents.size()); ++i)
    {
        const auto& ev = z.seqEvents[static_cast<size_t> (i)];
        float cx = area.getX() + ev.position * area.getWidth();
        float barH = ev.velocity * (area.getHeight() - 18.0f);
        float topY = area.getBottom() - 9.0f - barH;
        g.setColour (i == draggedEventIdx ? juce::Colours::white : z.colour);
        g.drawLine (cx, static_cast<float> (area.getBottom() - 9), cx, topY, 3.2f);
        g.fillEllipse (cx - 4.5f, static_cast<float> (area.getBottom() - 13.5f), 9.0f, 9.0f);
        g.fillEllipse (cx - 3.5f, topY - 3.5f, 7.0f, 7.0f);
    }

    g.setColour (z.colour.brighter (0.4f)); g.setFont (10.0f);
    g.drawText ("Pattern: " + z.name + "  |  Click=Add  Drag=Move/Vel  RightClick=Del", area.reduced (6, 3), juce::Justification::topRight, true);
}

void RhythmSequencerComponent::mouseDown (const juce::MouseEvent& e)
{
    auto area = getLocalBounds();
    const juce::ScopedLock sl (processor.zoneLock);
    if (processor.zones.empty() || processor.selectedZoneIndex < 0 || processor.selectedZoneIndex >= static_cast<int> (processor.zones.size())) return;
    auto& events = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)].seqEvents;
    draggedEventIdx = -1;

    for (int i = 0; i < static_cast<int> (events.size()); ++i)
    {
        float cx = area.getX() + events[static_cast<size_t> (i)].position * area.getWidth();
        if (std::abs (e.position.x - cx) <= 9.0f)
        {
            if (e.mods.isRightButtonDown()) { events.erase (events.begin() + i); repaint(); return; }
            draggedEventIdx = i; break;
        }
    }

    if (draggedEventIdx < 0 && ! e.mods.isRightButtonDown())
    {
        float normX = snapToGrid (e.position.x / static_cast<float> (std::max (1, area.getWidth())));
        float vel = juce::jlimit (0.1f, 1.0f, 1.0f - (e.position.y / static_cast<float> (std::max (1, area.getHeight()))));
        events.push_back ({ normX, vel });
        draggedEventIdx = static_cast<int> (events.size()) - 1;
    }
    repaint();
}

void RhythmSequencerComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (draggedEventIdx < 0) return;
    auto area = getLocalBounds();
    const juce::ScopedLock sl (processor.zoneLock);
    if (processor.zones.empty() || processor.selectedZoneIndex < 0 || processor.selectedZoneIndex >= static_cast<int> (processor.zones.size())) return;
    auto& events = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)].seqEvents;
    if (draggedEventIdx >= static_cast<int> (events.size())) return;

    float normX = juce::jlimit (0.0f, 0.99f, e.position.x / static_cast<float> (std::max (1, area.getWidth())));
    if (! e.mods.isShiftDown()) normX = snapToGrid (normX);
    float vel = juce::jlimit (0.1f, 1.0f, 1.0f - (e.position.y / static_cast<float> (std::max (1, area.getHeight()))));
    events[static_cast<size_t> (draggedEventIdx)].position = normX;
    events[static_cast<size_t> (draggedEventIdx)].velocity = vel;
    repaint();
}

void RhythmSequencerComponent::mouseUp (const juce::MouseEvent&) { draggedEventIdx = -1; repaint(); }

// ---------------- PatcherRoutingCanvas ----------------
PatcherRoutingCanvas::PatcherRoutingCanvas (RythmeAudioProcessor& p, int gridPx)
    : processor (p), gridStepPx (gridPx) {}

float PatcherRoutingCanvas::snapVal (float v) const
{
    if (gridStepPx <= 1) return v;
    float step = static_cast<float> (gridStepPx);
    return std::round (v / step) * step;
}

void PatcherRoutingCanvas::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll (juce::Colour (0xff10151e));
    g.setColour (juce::Colour (0xff313f57));
    g.drawRect (area, 1);

    if (gridStepPx > 1)
    {
        g.setColour (juce::Colour (0xff1c2534));
        for (int x = gridStepPx; x < area.getWidth(); x += gridStepPx)
            g.drawVerticalLine (x, 0.0f, static_cast<float> (area.getHeight()));
        for (int y = gridStepPx; y < area.getHeight(); y += gridStepPx)
            g.drawHorizontalLine (y, 0.0f, static_cast<float> (area.getWidth()));
    }

    const juce::ScopedLock sl (processor.zoneLock);

    // Draw connecting wires (FL Studio Patcher style cables)
    for (size_t wIdx = 0; wIdx < processor.patchWires.size(); ++wIdx)
    {
        const auto& w = processor.patchWires[wIdx];
        const PatchNode* nFrom = nullptr;
        const PatchNode* nTo = nullptr;
        for (const auto& n : processor.patchNodes)
        {
            if (n.id == w.fromNodeId) nFrom = &n;
            if (n.id == w.toNodeId) nTo = &n;
        }
        if (nFrom != nullptr && nTo != nullptr)
        {
            float x1 = nFrom->x + nFrom->w;
            float y1 = nFrom->y + nFrom->h * 0.5f;
            float x2 = nTo->x;
            float y2 = nTo->y + nTo->h * 0.5f;

            juce::Path cable;
            cable.startNewSubPath (x1, y1);
            float ctrl = std::max (32.0f, std::abs (x2 - x1) * 0.5f);
            cable.cubicTo (x1 + ctrl, y1, x2 - ctrl, y2, x2, y2);

            g.setColour (w.enabled ? juce::Colour (0xff5aa9ff) : juce::Colour (0xff475266));
            g.strokePath (cable, juce::PathStrokeType (w.enabled ? 2.4f : 1.5f));
        }
    }

    // Draw nodes (Drag body to move, Drag bottom-right corner to resize, Click port/Right-click to connect)
    for (size_t i = 0; i < processor.patchNodes.size(); ++i)
    {
        const auto& n = processor.patchNodes[i];
        juce::Rectangle<float> nr (n.x, n.y, n.w, n.h);
        g.setColour (n.active ? juce::Colour (0xff1d2738) : juce::Colour (0xff161a22));
        g.fillRoundedRectangle (nr, 6.0f);
        g.setColour (static_cast<int> (i) == connectFromIdx ? juce::Colour (0xffffd32a)
                   : (n.active ? juce::Colour (0xff4cc98f) : juce::Colour (0xff3c475c)));
        g.drawRoundedRectangle (nr.reduced (0.5f), 6.0f, 1.5f);

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (n.title, nr.reduced (8.0f, 4.0f), juce::Justification::topLeft, true);

        g.setColour (juce::Colour (0xff8fa3c4));
        g.setFont (juce::FontOptions (8.5f, juce::Font::plain));
        g.drawText (n.category + (n.active ? " [ON]" : " [OFF]"), nr.reduced (8.0f, 18.0f), juce::Justification::topLeft, true);

        // Input & Output ports
        g.setColour (juce::Colour (0xffff66aa));
        g.fillEllipse (n.x - 5.0f, n.y + n.h * 0.5f - 5.0f, 10.0f, 10.0f);
        g.setColour (juce::Colour (0xff5aa9ff));
        g.fillEllipse (n.x + n.w - 5.0f, n.y + n.h * 0.5f - 5.0f, 10.0f, 10.0f);

        // Bottom-right resize grip triangle
        g.setColour (juce::Colour (0xff9fb0cc));
        juce::Path grip;
        grip.startNewSubPath (nr.getRight() - 9.0f, nr.getBottom());
        grip.lineTo (nr.getRight(), nr.getBottom() - 9.0f);
        grip.lineTo (nr.getRight(), nr.getBottom());
        grip.closeSubPath();
        g.fillPath (grip);
    }
}

void PatcherRoutingCanvas::mouseDown (const juce::MouseEvent& e)
{
    const juce::ScopedLock sl (processor.zoneLock);
    draggedNodeIdx = -1;
    resizingNode = false;

    for (int i = static_cast<int> (processor.patchNodes.size()) - 1; i >= 0; --i)
    {
        auto& n = processor.patchNodes[static_cast<size_t> (i)];
        juce::Rectangle<float> nr (n.x, n.y, n.w, n.h);
        juce::Rectangle<float> outPort (n.x + n.w - 8.0f, n.y + n.h * 0.5f - 8.0f, 16.0f, 16.0f);
        juce::Rectangle<float> resizeCorner (nr.getRight() - 12.0f, nr.getBottom() - 12.0f, 14.0f, 14.0f);

        if (outPort.contains (e.position))
        {
            connectFromIdx = i;
            repaint();
            return;
        }

        if (connectFromIdx >= 0 && connectFromIdx != i)
        {
            processor.addPatchWire (processor.patchNodes[static_cast<size_t> (connectFromIdx)].id, n.id);
            connectFromIdx = -1;
            repaint();
            return;
        }

        if (nr.contains (e.position))
        {
            if (e.mods.isRightButtonDown())
            {
                n.active = ! n.active;
                repaint();
                return;
            }
            draggedNodeIdx = i;
            dragStartPos = e.position;
            origX = n.x; origY = n.y; origW = n.w; origH = n.h;
            resizingNode = resizeCorner.contains (e.position);
            return;
        }
    }
    connectFromIdx = -1;
    repaint();
}

void PatcherRoutingCanvas::mouseDrag (const juce::MouseEvent& e)
{
    if (draggedNodeIdx < 0) return;
    const juce::ScopedLock sl (processor.zoneLock);
    if (draggedNodeIdx >= static_cast<int> (processor.patchNodes.size())) return;
    auto& n = processor.patchNodes[static_cast<size_t> (draggedNodeIdx)];

    float dx = e.position.x - dragStartPos.x;
    float dy = e.position.y - dragStartPos.y;

    if (resizingNode)
    {
        n.w = juce::jlimit (84.0f, 260.0f, snapVal (origW + dx));
        n.h = juce::jlimit (38.0f, 140.0f, snapVal (origH + dy));
    }
    else
    {
        n.x = juce::jlimit (4.0f, std::max (20.0f, static_cast<float> (getWidth()) - n.w - 4.0f), snapVal (origX + dx));
        n.y = juce::jlimit (4.0f, std::max (20.0f, static_cast<float> (getHeight()) - n.h - 4.0f), snapVal (origY + dy));
    }
    repaint();
}

void PatcherRoutingCanvas::mouseUp (const juce::MouseEvent&)
{
    draggedNodeIdx = -1;
    resizingNode = false;
}

// ---------------- MainPluginView ----------------
void MainPluginView::styleUniformSwitchButton (juce::TextButton& b, juce::Colour activeCol)
{
    b.setClickingTogglesState (true);
    int s = juce::jlimit (1, 10, audioProcessor.switchStyleId);
    juce::Colour offCol = (s % 2 == 0) ? juce::Colour (0xff2b2538) : juce::Colour (0xff232b38);
    b.setColour (juce::TextButton::buttonColourId, offCol);
    b.setColour (juce::TextButton::buttonOnColourId, activeCol);
    b.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffc9d4e8));
    b.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void MainPluginView::applyAllStyles()
{
    int fStyle = juce::jlimit (1, 10, audioProcessor.faderStyleId);
    HorizontalFader* allFaders[] = {
        &shuffleFader, &timeRandFader, &velRandFader,
        &grpVolFader, &grpPanFader, &crossfadeFader,
        &pitchFader, &panFader, &volFader,
        &attackFader, &decayFader, &sustainFader, &releaseFader,
        &cutoffFader, &resFader, &fxP1, &fxP2, &fxP3
    };
    for (auto* hf : allFaders)
        hf->applyFaderStyle (fStyle);

    styleUniformSwitchButton (groupMuteBtn, juce::Colour (0xffee5253));
    styleUniformSwitchButton (groupSoloBtn, juce::Colour (0xffe8a138));
    styleUniformSwitchButton (selGroupOnlyBtn, juce::Colour (0xff4cc98f));
    styleUniformSwitchButton (frrBtn, juce::Colour (0xffff5599));
    styleUniformSwitchButton (pitchTrackBtn, juce::Colour (0xffe8a138));
    styleUniformSwitchButton (realRRBtn, juce::Colour (0xff5aa9ff));
    styleUniformSwitchButton (sampleLoopBtn, juce::Colour (0xff4cc98f));
    styleUniformSwitchButton (autoLoopZeroBtn, juce::Colour (0xff38c4d9));
    styleUniformSwitchButton (reverseBtn, juce::Colour (0xffe04f8a));
    styleUniformSwitchButton (fxEnableBtn, juce::Colour (0xff4cc98f));
    styleUniformSwitchButton (seqBypassBtn, juce::Colour (0xffee5253));
    styleUniformSwitchButton (fxBypassBtn, juce::Colour (0xffee5253));

    brandHeader.repaint();
    repaint();
}

MainPluginView::MainPluginView (RythmeAudioProcessor& p, std::function<void(float)> onScaleSelected)
    : audioProcessor (p), scaleCallback (std::move (onScaleSelected)),
      brandHeader (p, [this]() { showScaleMenu(); }),
      mountedCard ([this]()
      {
          audioProcessor.clearLoadedItemPanel();
          mountedCard.setItem ("", false);
          syncPerZoneControls();
          repaintDynamicAreas();
      }),
      mainWaveDisplay (p), seqComp (p), editKeyMapper (p), waveEditor (p),
      patchCanvas (p, 0), bottomPiano (p)
{
    addAndMakeVisible (brandHeader);

    addAndMakeVisible (addLibBtn);
    addAndMakeVisible (sortAzBtn);
    addAndMakeVisible (delLibBtn);
    addAndMakeVisible (libraryList);
    libraryList.setModel (this);
    libraryList.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff131720));

    addLibBtn.onClick = [this]()
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Select Sample/Library Folder (e.g. DRUMS)",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this] (const juce::FileChooser& fc)
            {
                if (fc.getResult().isDirectory())
                {
                    audioProcessor.addLibraryFolderPersistent (fc.getResult());
                    rebuildLibraryRows();
                }
            });
    };

    sortAzBtn.onClick = [this]()
    {
        audioProcessor.sortLibraryPersistent();
        rebuildLibraryRows();
    };

    delLibBtn.onClick = [this]()
    {
        int row = libraryList.getSelectedRow();
        if (row >= 0 && row < static_cast<int> (visibleLibRows.size()))
        {
            const auto& vr = visibleLibRows[static_cast<size_t> (row)];
            if (vr.catIdx >= 0)
            {
                audioProcessor.removeLibraryEntryPersistent (vr.catIdx);
                rebuildLibraryRows();
            }
        }
    };

    // Top Bar: MAIN, EDIT, EDIT PANEL + Preset Name + SAVE / SAVE AS
    addAndMakeVisible (tabMainBtn);
    addAndMakeVisible (tabEditBtn);
    addAndMakeVisible (tabPanelBtn);
    tabMainBtn.onClick  = [this]() { setActiveTab (ActiveTab::Main); };
    tabEditBtn.onClick  = [this]() { setActiveTab (ActiveTab::Edit); };
    tabPanelBtn.onClick = [this]() { setActiveTab (ActiveTab::EditPanel); };

    addAndMakeVisible (presetNameBox);
    presetNameBox.setMultiLine (false);
    presetNameBox.setJustification (juce::Justification::centred);
    presetNameBox.setText (audioProcessor.currentPresetName, juce::dontSendNotification);
    presetNameBox.setTextToShowWhenEmpty ("---", juce::Colour (0xff5c677d));
    presetNameBox.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff161c26));
    presetNameBox.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff3c4960));
    presetNameBox.setColour (juce::TextEditor::textColourId, juce::Colours::white);
    presetNameBox.onTextChange = [this]() { audioProcessor.currentPresetName = presetNameBox.getText().trim(); };

    addAndMakeVisible (saveBtn);
    addAndMakeVisible (saveAsBtn);
    saveBtn.onClick = [this]()
    {
        audioProcessor.currentPresetName = presetNameBox.getText().trim();
        if (audioProcessor.currentPresetFile != juce::File()) audioProcessor.savePresetMn6x (audioProcessor.currentPresetFile);
        else saveAsDialog();
    };
    saveAsBtn.onClick = [this]() { saveAsDialog(); };

    // ---------------- MAIN TAB ----------------
    addAndMakeVisible (mountedCard);
    mountedCard.setItem (audioProcessor.loadedItemName, audioProcessor.loadedItemActive);

    addAndMakeVisible (mainWaveDisplay);

    rhythmSeqHeaderLbl.setText ("RHYTHM SEQUENCER", juce::dontSendNotification);
    rhythmSeqHeaderLbl.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    rhythmSeqHeaderLbl.setColour (juce::Label::textColourId, juce::Colour (0xffc9d4e8));
    addAndMakeVisible (rhythmSeqHeaderLbl);

    addAndMakeVisible (seqBypassBtn);
    seqBypassBtn.onClick = [this]() { audioProcessor.seqBypass.store (seqBypassBtn.getToggleState()); };

    addAndMakeVisible (seqComp);

    addAndMakeVisible (playPauseBtn);
    playPauseBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff273549));
    playPauseBtn.onClick = [this]() { togglePlayPause(); };

    addAndMakeVisible (hostSyncBtn);
    hostSyncBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff4cc98f));
    hostSyncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "hostSync", hostSyncBtn);

    bpmLabel.setText ("BPM", juce::dontSendNotification);
    bpmLabel.setJustificationType (juce::Justification::centred);
    bpmLabel.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    bpmLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc9d4e8));
    addAndMakeVisible (bpmLabel);

    addAndMakeVisible (bpmInputBox);
    bpmInputBox.setMultiLine (false);
    bpmInputBox.setJustification (juce::Justification::centred);
    bpmInputBox.setInputRestrictions (6, "0123456789.");
    bpmInputBox.setText (juce::String (audioProcessor.apvts.getRawParameterValue ("internalBpm")->load(), 1), juce::dontSendNotification);
    bpmInputBox.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff141922));
    bpmInputBox.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff3c4960));
    bpmInputBox.setColour (juce::TextEditor::textColourId, juce::Colours::white);
    bpmInputBox.onReturnKey = [this]() { applyBpmText(); };
    bpmInputBox.onFocusLost = [this]() { applyBpmText(); };

    addAndMakeVisible (shuffleFader);
    addAndMakeVisible (timeRandFader);
    addAndMakeVisible (velRandFader);
    shuffleAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "shuffle", shuffleFader.slider);
    timeRandAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "timeRand", timeRandFader.slider);
    velRandAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "velRand", velRandFader.slider);

    gridLabel.setText ("Grid", juce::dontSendNotification);
    gridLabel.setJustificationType (juce::Justification::centred);
    gridLabel.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    gridLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc9d4e8));
    addAndMakeVisible (gridLabel);

    addAndMakeVisible (gridBox);
    gridBox.addItemList ({ "1/4", "1/8", "1/12 (T)", "1/16", "1/24 (T)", "1/32", "1/48 (T)", "1/64" }, 1);
    gridAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (audioProcessor.apvts, "grid", gridBox);
    gridBox.onChange = [this]() { seqComp.repaint(); };

    addAndMakeVisible (seqLoopBtn);
    seqLoopBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffe84ea2));
    seqLoopAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "seqLoop", seqLoopBtn);

    addAndMakeVisible (seqClearBtn);
    seqClearBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff592836));
    seqClearBtn.onClick = [this]() { audioProcessor.clearSelectedZoneRhythm(); seqComp.repaint(); };

    // ---------------- EDIT TAB ----------------
    addAndMakeVisible (editViewport);
    editViewport.setViewedComponent (&editContent, false);
    editViewport.setScrollBarsShown (true, false);

    editContent.addAndMakeVisible (addGroupBtn);
    addGroupBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b3c54));
    addGroupBtn.onClick = [this]()
    {
        audioProcessor.createEmptyGroup();
        refreshGroupBoxes(); syncGroupControls(); syncFxControls(); bottomPiano.repaint();
    };

    editContent.addAndMakeVisible (delGroupBtn);
    delGroupBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff6b2938));
    delGroupBtn.onClick = [this]()
    {
        if (audioProcessor.deleteSelectedGroup())
        {
            refreshGroupBoxes(); syncGroupControls(); syncFxControls(); syncPerZoneControls(); repaintDynamicAreas();
        }
    };

    editContent.addAndMakeVisible (groupSelectBox);
    groupSelectBox.onChange = [this]()
    {
        int id = groupSelectBox.getSelectedId();
        if (id > 0)
        {
            const juce::ScopedLock sl (audioProcessor.zoneLock);
            audioProcessor.selectedGroupIndex = juce::jlimit (0, static_cast<int> (audioProcessor.groups.size()) - 1, id - 1);
        }
        syncGroupControls(); syncFxControls(); editKeyMapper.repaint();
    };

    editContent.addAndMakeVisible (groupNameEdit);
    groupNameEdit.setMultiLine (false);
    groupNameEdit.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff141922));
    groupNameEdit.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff3c4960));
    groupNameEdit.setColour (juce::TextEditor::textColourId, juce::Colours::white);
    auto applyGroupName = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedGroupIndex >= 0 && audioProcessor.selectedGroupIndex < static_cast<int> (audioProcessor.groups.size()))
            audioProcessor.groups[static_cast<size_t> (audioProcessor.selectedGroupIndex)].name = groupNameEdit.getText().trim();
        refreshGroupBoxes();
    };
    groupNameEdit.onReturnKey = applyGroupName;
    groupNameEdit.onFocusLost = applyGroupName;

    editContent.addAndMakeVisible (groupMuteBtn);
    groupMuteBtn.onClick = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedGroupIndex >= 0 && audioProcessor.selectedGroupIndex < static_cast<int> (audioProcessor.groups.size()))
            audioProcessor.groups[static_cast<size_t> (audioProcessor.selectedGroupIndex)].muted = groupMuteBtn.getToggleState();
    };

    editContent.addAndMakeVisible (groupSoloBtn);
    groupSoloBtn.onClick = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedGroupIndex >= 0 && audioProcessor.selectedGroupIndex < static_cast<int> (audioProcessor.groups.size()))
            audioProcessor.groups[static_cast<size_t> (audioProcessor.selectedGroupIndex)].soloed = groupSoloBtn.getToggleState();
    };

    editContent.addAndMakeVisible (selGroupOnlyBtn);
    selGroupOnlyBtn.setToggleState (audioProcessor.selectGroupOnly.load(), juce::dontSendNotification);
    editKeyMapper.setFilterToSelectedGroup (audioProcessor.selectGroupOnly.load());
    selGroupOnlyBtn.onClick = [this]()
    {
        bool st = selGroupOnlyBtn.getToggleState();
        audioProcessor.selectGroupOnly.store (st);
        editKeyMapper.setFilterToSelectedGroup (st);
    };

    editContent.addAndMakeVisible (groupKsBox);
    groupKsBox.addItem ("KS: None", 1);
    for (int n = 0; n < 36; ++n) groupKsBox.addItem ("KS " + juce::MidiMessage::getMidiNoteName (n, true, true, 5), n + 2);
    groupKsBox.onChange = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedGroupIndex >= 0 && audioProcessor.selectedGroupIndex < static_cast<int> (audioProcessor.groups.size()))
        {
            int id = groupKsBox.getSelectedId();
            audioProcessor.groups[static_cast<size_t> (audioProcessor.selectedGroupIndex)].keyswitchNote = (id <= 1) ? -1 : (id - 2);
        }
        bottomPiano.repaint();
    };

    grpVolFader.slider.setRange (0.0, 2.0, 0.01);
    editContent.addAndMakeVisible (grpVolFader);
    grpVolFader.slider.onValueChange = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedGroupIndex >= 0 && audioProcessor.selectedGroupIndex < static_cast<int> (audioProcessor.groups.size()))
            audioProcessor.groups[static_cast<size_t> (audioProcessor.selectedGroupIndex)].volume = static_cast<float> (grpVolFader.slider.getValue());
    };

    grpPanFader.slider.setRange (-1.0, 1.0, 0.01);
    editContent.addAndMakeVisible (grpPanFader);
    grpPanFader.slider.onValueChange = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedGroupIndex >= 0 && audioProcessor.selectedGroupIndex < static_cast<int> (audioProcessor.groups.size()))
            audioProcessor.groups[static_cast<size_t> (audioProcessor.selectedGroupIndex)].pan = static_cast<float> (grpPanFader.slider.getValue());
    };

    editContent.addAndMakeVisible (frrBtn);
    frrAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "frr", frrBtn);

    editContent.addAndMakeVisible (pitchTrackBtn);
    pitchTrackAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "pitchTrack", pitchTrackBtn);

    editContent.addAndMakeVisible (selectAllZonesBtn);
    selectAllZonesBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff232b38));
    selectAllZonesBtn.onClick = [this]() { editKeyMapper.selectAllZones(); };

    editContent.addAndMakeVisible (delZoneBtn);
    delZoneBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff6b2938));
    delZoneBtn.onClick = [this]() { deleteSelectedInEditTab(); };

    editContent.addAndMakeVisible (autoGridBtn);
    autoGridBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b3c54));
    autoGridBtn.onClick = [this]() { editKeyMapper.autoGridSelectedZones(); };

    editContent.addAndMakeVisible (velGridBox);
    velGridBox.addItemList ({ "Vel: Free", "Vel: 2", "Vel: 4", "Vel: 8", "Vel: 16", "Vel: 32", "Vel: 64" }, 1);
    velGridBox.setSelectedId (3, juce::dontSendNotification);
    velGridBox.onChange = [this]()
    {
        int id = velGridBox.getSelectedId();
        int divs = (id == 1) ? 0 : (id == 2 ? 2 : (id == 3 ? 4 : (id == 4 ? 8 : (id == 5 ? 16 : (id == 6 ? 32 : 64)))));
        editKeyMapper.setVelocityGridDivisions (divs);
    };

    editContent.addAndMakeVisible (zoneColourBox);
    for (int c = 0; c < RythmeAudioProcessor::numPaletteColours; ++c)
        zoneColourBox.addItem (RythmeAudioProcessor::getPaletteColourName (c), c + 1);
    zoneColourBox.setSelectedId (1, juce::dontSendNotification);
    zoneColourBox.onChange = [this]()
    {
        int cIdx = zoneColourBox.getSelectedId() - 1;
        if (cIdx >= 0)
        {
            const juce::ScopedLock sl (audioProcessor.zoneLock);
            const auto& sel = editKeyMapper.getSelectedZoneIndices();
            if (! sel.empty())
                for (int idx : sel) { auto& z = audioProcessor.zones[static_cast<size_t> (idx)]; z.colourIndex = cIdx; z.colour = RythmeAudioProcessor::getPaletteColour (cIdx); }
            else if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
                { auto& z = audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)]; z.colourIndex = cIdx; z.colour = RythmeAudioProcessor::getPaletteColour (cIdx); }
            repaintDynamicAreas();
        }
    };

    editKeyMapper.setReadOnly (false);
    editKeyMapper.onSelectionChanged = [this]() { syncPerZoneControls(); };
    editContent.addAndMakeVisible (editKeyMapper);

    editContent.addAndMakeVisible (wavePlayBtn);
    wavePlayBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff27473b));
    wavePlayBtn.onClick = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.triggerNote (audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].rootKey, 0.9f);
    };

    editContent.addAndMakeVisible (trackSelectBox);
    trackSelectBox.setTextWhenNoChoicesAvailable ("No Samples");
    trackSelectBox.onChange = [this]()
    {
        int id = trackSelectBox.getSelectedId();
        if (id > 0)
        {
            const juce::ScopedLock sl (audioProcessor.zoneLock);
            if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
                audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].selectedLayerIndex = id - 1;
            waveEditor.repaint();
        }
    };

    editContent.addAndMakeVisible (realRRBtn);
    realRRBtn.onClick = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].realRREnabled = realRRBtn.getToggleState();
    };

    editContent.addAndMakeVisible (sampleLoopBtn);
    sampleLoopBtn.onClick = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].sampleLoop = sampleLoopBtn.getToggleState();
        waveEditor.repaint();
    };

    editContent.addAndMakeVisible (autoLoopZeroBtn);
    autoLoopZeroBtn.setToggleState (true, juce::dontSendNotification);
    autoLoopZeroBtn.onClick = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
        {
            int zIdx = audioProcessor.selectedZoneIndex;
            auto& z = audioProcessor.zones[static_cast<size_t> (zIdx)];
            z.autoLoopZeroCross = autoLoopZeroBtn.getToggleState();
            if (z.autoLoopZeroCross)
            {
                z.loopStartNorm = juce::jlimit (z.startNorm, z.loopEndNorm - 0.02f, audioProcessor.snapNormToZeroCrossing (zIdx, z.loopStartNorm));
                z.loopEndNorm   = juce::jlimit (z.loopStartNorm + 0.02f, z.endNorm, audioProcessor.snapNormToZeroCrossing (zIdx, z.loopEndNorm));
                waveEditor.repaint();
            }
        }
    };

    crossfadeFader.slider.setRange (0.0, 0.35, 0.01);
    editContent.addAndMakeVisible (crossfadeFader);
    crossfadeFader.slider.onValueChange = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].loopCrossfade = static_cast<float> (crossfadeFader.slider.getValue());
    };

    editContent.addAndMakeVisible (reverseBtn);
    reverseBtn.onClick = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].reverse = reverseBtn.getToggleState();
    };

    editContent.addAndMakeVisible (normalizeBtn);
    normalizeBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b3c54));
    normalizeBtn.onClick = [this]() { audioProcessor.normalizeSelectedSample(); waveEditor.repaint(); };

    editContent.addAndMakeVisible (resetZoneBtn);
    resetZoneBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3b334a));
    resetZoneBtn.onClick = [this]()
    {
        audioProcessor.resetSelectedZoneDefaults(); syncPerZoneControls(); waveEditor.repaint();
    };

    editContent.addAndMakeVisible (waveEditor);

    auto bindZoneFader = [this] (HorizontalFader& f, double minV, double maxV, double stepV, std::function<void(SampleZone&, float)> setter)
    {
        f.slider.setRange (minV, maxV, stepV);
        editContent.addAndMakeVisible (f);
        f.slider.onValueChange = [this, &f, setter]()
        {
            const juce::ScopedLock sl (audioProcessor.zoneLock);
            if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
                setter (audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)], static_cast<float> (f.slider.getValue()));
        };
    };

    bindZoneFader (pitchFader,   -24.0, 24.0, 0.1,   [] (SampleZone& z, float v) { z.pitchSemi = v; });
    bindZoneFader (panFader,     -1.0, 1.0, 0.01,   [] (SampleZone& z, float v) { z.pan = v; });
    bindZoneFader (volFader,     0.0, 2.0, 0.01,    [] (SampleZone& z, float v) { z.volume = v; });
    bindZoneFader (attackFader,  0.001, 2.0, 0.002, [] (SampleZone& z, float v) { z.attackSec = v; });
    bindZoneFader (decayFader,   0.01, 3.0, 0.01,   [] (SampleZone& z, float v) { z.decaySec = v; });
    bindZoneFader (sustainFader, 0.0, 1.0, 0.01,    [] (SampleZone& z, float v) { z.sustainLvl = v; });
    bindZoneFader (releaseFader, 0.01, 4.0, 0.01,   [] (SampleZone& z, float v) { z.releaseSec = v; });
    bindZoneFader (cutoffFader,  80.0, 20000.0, 10.0, [] (SampleZone& z, float v) { z.cutoffHz = v; });
    bindZoneFader (resFader,     0.3, 5.0, 0.05,    [] (SampleZone& z, float v) { z.resQ = v; });

    editContent.addAndMakeVisible (fxBypassBtn);
    fxBypassBtn.onClick = [this]() { audioProcessor.fxBypass.store (fxBypassBtn.getToggleState()); };

    editContent.addAndMakeVisible (fxSlotSelectBox);
    for (int s = 0; s < SampleGroup::numFxSlots; ++s) fxSlotSelectBox.addItem ("FX " + juce::String (s + 1), s + 1);
    fxSlotSelectBox.setSelectedId (1, juce::dontSendNotification);
    fxSlotSelectBox.onChange = [this]() { syncFxControls(); };

    editContent.addAndMakeVisible (fxTypeBox);
    fxTypeBox.addItemList ({ "FX Off", "EQ", "Comp", "Sat", "Delay", "Reverb" }, 1);
    fxTypeBox.onChange = [this]()
    {
        int sIdx = juce::jlimit (0, SampleGroup::numFxSlots - 1, fxSlotSelectBox.getSelectedId() - 1);
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedGroupIndex >= 0 && audioProcessor.selectedGroupIndex < static_cast<int> (audioProcessor.groups.size()))
        {
            auto& slot = audioProcessor.groups[static_cast<size_t> (audioProcessor.selectedGroupIndex)].fxSlots[static_cast<size_t> (sIdx)];
            slot.type = static_cast<SimpleFxType> (fxTypeBox.getSelectedId() - 1);
            slot.prepare (audioProcessor.getCurrentSampleRate());
        }
        syncFxControls();
    };

    editContent.addAndMakeVisible (fxEnableBtn);
    fxEnableBtn.onClick = [this]()
    {
        int sIdx = juce::jlimit (0, SampleGroup::numFxSlots - 1, fxSlotSelectBox.getSelectedId() - 1);
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedGroupIndex >= 0 && audioProcessor.selectedGroupIndex < static_cast<int> (audioProcessor.groups.size()))
            audioProcessor.groups[static_cast<size_t> (audioProcessor.selectedGroupIndex)].fxSlots[static_cast<size_t> (sIdx)].enabled = fxEnableBtn.getToggleState();
    };

    HorizontalFader* fxFaders[3] = { &fxP1, &fxP2, &fxP3 };
    for (int k = 0; k < 3; ++k)
    {
        fxFaders[k]->slider.setRange (0.0, 1.0, 0.01);
        editContent.addAndMakeVisible (*fxFaders[k]);
        fxFaders[k]->slider.onValueChange = [this, k, fxFaders]()
        {
            int sIdx = juce::jlimit (0, SampleGroup::numFxSlots - 1, fxSlotSelectBox.getSelectedId() - 1);
            const juce::ScopedLock sl (audioProcessor.zoneLock);
            if (audioProcessor.selectedGroupIndex >= 0 && audioProcessor.selectedGroupIndex < static_cast<int> (audioProcessor.groups.size()))
            {
                auto& slot = audioProcessor.groups[static_cast<size_t> (audioProcessor.selectedGroupIndex)].fxSlots[static_cast<size_t> (sIdx)];
                float v = static_cast<float> (fxFaders[k]->slider.getValue());
                if (k == 0) slot.p1 = v; else if (k == 1) slot.p2 = v; else slot.p3 = v;
            }
        };
    }

    // ---------------- EDIT PANEL CONTROLS ----------------
    addAndMakeVisible (panelGridBox);
    panelGridBox.addItemList ({ "Grid: Free", "Grid: 10x", "Grid: 20x", "Grid: 30x", "Grid: 40x", "Grid: 50x" }, 1);
    panelGridBox.setSelectedId (1, juce::dontSendNotification);
    panelGridBox.onChange = [this]()
    {
        int id = panelGridBox.getSelectedId();
        int px = (id <= 1) ? 0 : ((id - 1) * 10);
        patchCanvas.setGridStepPx (px);
    };

    addAndMakeVisible (knobStyleBox);
    knobStyleBox.addItem ("Knob Style 1..10", 1);
    for (int i = 1; i <= 10; ++i) knobStyleBox.addItem ("Knob Style " + juce::String (i), i + 1);
    knobStyleBox.setSelectedId (audioProcessor.knobStyleId + 1, juce::dontSendNotification);
    knobStyleBox.onChange = [this]()
    {
        int id = knobStyleBox.getSelectedId();
        if (id >= 2) audioProcessor.knobStyleId = id - 1;
        applyAllStyles();
    };

    addAndMakeVisible (faderStyleBox);
    for (int i = 1; i <= 10; ++i) faderStyleBox.addItem ("Fader Style " + juce::String (i), i);
    faderStyleBox.setSelectedId (audioProcessor.faderStyleId, juce::dontSendNotification);
    faderStyleBox.onChange = [this]()
    {
        audioProcessor.faderStyleId = juce::jlimit (1, 10, faderStyleBox.getSelectedId());
        applyAllStyles();
    };

    addAndMakeVisible (switchStyleBox);
    for (int i = 1; i <= 10; ++i) switchStyleBox.addItem ("Switch Style " + juce::String (i), i);
    switchStyleBox.setSelectedId (audioProcessor.switchStyleId, juce::dontSendNotification);
    switchStyleBox.onChange = [this]()
    {
        audioProcessor.switchStyleId = juce::jlimit (1, 10, switchStyleBox.getSelectedId());
        applyAllStyles();
    };

    addAndMakeVisible (logoDesignBox);
    logoDesignBox.addItemList ({ "Logo: Shield", "Logo: Pill", "Logo: Hex", "Logo: Circle" }, 1);
    logoDesignBox.setSelectedId (audioProcessor.logoStyleId, juce::dontSendNotification);
    logoDesignBox.onChange = [this]()
    {
        audioProcessor.logoStyleId = juce::jlimit (1, 4, logoDesignBox.getSelectedId());
        brandHeader.repaint();
    };

    addAndMakeVisible (logoLettersEdit);
    logoLettersEdit.setMultiLine (false);
    logoLettersEdit.setText (audioProcessor.brandLetters, juce::dontSendNotification);
    logoLettersEdit.setTextToShowWhenEmpty ("Logo Letters", juce::Colour (0xff6d7b94));
    logoLettersEdit.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff141922));
    logoLettersEdit.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff3c4960));
    logoLettersEdit.setColour (juce::TextEditor::textColourId, juce::Colours::white);
    logoLettersEdit.onTextChange = [this]()
    {
        audioProcessor.brandLetters = logoLettersEdit.getText().trim().toUpperCase();
        brandHeader.repaint();
    };

    addAndMakeVisible (titleTextEdit);
    titleTextEdit.setMultiLine (false);
    titleTextEdit.setText (audioProcessor.pluginTitleText, juce::dontSendNotification);
    titleTextEdit.setTextToShowWhenEmpty ("Plugin Title", juce::Colour (0xff6d7b94));
    titleTextEdit.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff141922));
    titleTextEdit.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff3c4960));
    titleTextEdit.setColour (juce::TextEditor::textColourId, juce::Colours::white);
    titleTextEdit.onTextChange = [this]()
    {
        audioProcessor.pluginTitleText = titleTextEdit.getText().trim();
        brandHeader.repaint();
    };

    addAndMakeVisible (newNodeNameEdit);
    newNodeNameEdit.setMultiLine (false);
    newNodeNameEdit.setTextToShowWhenEmpty ("New Knob / Wave / Switch", juce::Colour (0xff6d7b94));
    newNodeNameEdit.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff141922));
    newNodeNameEdit.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff3c4960));
    newNodeNameEdit.setColour (juce::TextEditor::textColourId, juce::Colours::white);

    addAndMakeVisible (addPatchNodeBtn);
    addPatchNodeBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b3c54));
    addPatchNodeBtn.onClick = [this]()
    {
        audioProcessor.addCustomPatchNode (newNodeNameEdit.getText().trim(), "CUSTOM");
        newNodeNameEdit.clear();
        patchCanvas.repaint();
    };

    addAndMakeVisible (patchCanvas);

    addAndMakeVisible (panelTargetMainBtn);
    panelTargetMainBtn.onClick = [this]()
    {
        panelSubTarget = 0;
        panelTargetMainBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b5876));
        panelTargetEditBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff232b38));
    };

    addAndMakeVisible (panelTargetEditBtn);
    panelTargetEditBtn.onClick = [this]()
    {
        panelSubTarget = 1;
        panelTargetEditBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b5876));
        panelTargetMainBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff232b38));
    };

    addAndMakeVisible (panelDoneBtn);
    panelDoneBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2d7a56));
    panelDoneBtn.onClick = [this]()
    {
        applyAllStyles();
        setActiveTab (panelSubTarget == 0 ? ActiveTab::Main : ActiveTab::Edit);
    };

    addAndMakeVisible (bottomPiano);

    rebuildLibraryRows();
    refreshGroupBoxes();
    syncGroupControls();
    syncFxControls();
    applyAllStyles();
    setActiveTab (ActiveTab::Main);
    syncPerZoneControls();
}

void MainPluginView::rebuildLibraryRows()
{
    visibleLibRows.clear();
    const juce::ScopedLock sl (audioProcessor.zoneLock);
    for (int c = 0; c < static_cast<int> (audioProcessor.libraryCategories.size()); ++c)
    {
        const auto& cat = audioProcessor.libraryCategories[static_cast<size_t> (c)];
        VisibleLibRow hdr;
        hdr.isCategory = true;
        hdr.catIdx = c;
        hdr.label = cat.name;
        hdr.expanded = cat.expanded;
        visibleLibRows.push_back (std::move (hdr));

        if (cat.expanded)
        {
            for (const auto& item : cat.items)
            {
                VisibleLibRow sub;
                sub.isCategory = false;
                sub.catIdx = c;
                sub.label = item.getFileNameWithoutExtension();
                sub.expanded = false;
                sub.file = item;
                visibleLibRows.push_back (std::move (sub));
            }
        }
    }
    libraryList.updateContent();
    libraryList.repaint();
}

void MainPluginView::loadLibraryItem (const juce::File& file)
{
    if (! file.existsAsFile()) return;
    if (file.hasFileExtension (".mn6x"))
    {
        audioProcessor.loadPresetMn6x (file);
        presetNameBox.setText (audioProcessor.currentPresetName, juce::dontSendNotification);
        logoLettersEdit.setText (audioProcessor.brandLetters, juce::dontSendNotification);
        titleTextEdit.setText (audioProcessor.pluginTitleText, juce::dontSendNotification);
        audioProcessor.setLoadedItemInPanel (file.getFileNameWithoutExtension(), file, -1);
        mountedCard.setItem (file.getFileNameWithoutExtension(), true);
        refreshGroupBoxes(); syncGroupControls(); syncFxControls(); applyAllStyles();
    }
    else
    {
        int nextKey = juce::jlimit (0, 120, 60 + static_cast<int> (audioProcessor.zones.size()));
        audioProcessor.loadSampleIntoNewZone (file, nextKey, nextKey, nextKey);
        audioProcessor.setLoadedItemInPanel (file.getFileNameWithoutExtension(), file, nextKey);
        mountedCard.setItem (file.getFileNameWithoutExtension(), true);
        editKeyMapper.selectSingleZone (audioProcessor.selectedZoneIndex);
    }
    syncPerZoneControls();
    repaintDynamicAreas();
}

void MainPluginView::refreshGroupBoxes()
{
    groupSelectBox.clear (juce::dontSendNotification);
    const juce::ScopedLock sl (audioProcessor.zoneLock);
    for (int i = 0; i < static_cast<int> (audioProcessor.groups.size()); ++i)
        groupSelectBox.addItem (audioProcessor.groups[static_cast<size_t> (i)].name, i + 1);
    groupSelectBox.setSelectedId (juce::jlimit (0, std::max (0, static_cast<int> (audioProcessor.groups.size()) - 1), audioProcessor.selectedGroupIndex) + 1, juce::dontSendNotification);
}

void MainPluginView::syncGroupControls()
{
    const juce::ScopedLock sl (audioProcessor.zoneLock);
    if (audioProcessor.selectedGroupIndex < 0 || audioProcessor.selectedGroupIndex >= static_cast<int> (audioProcessor.groups.size())) return;
    const auto& g = audioProcessor.groups[static_cast<size_t> (audioProcessor.selectedGroupIndex)];
    groupNameEdit.setText (g.name, juce::dontSendNotification);
    groupMuteBtn.setToggleState (g.muted, juce::dontSendNotification);
    groupSoloBtn.setToggleState (g.soloed, juce::dontSendNotification);
    groupKsBox.setSelectedId (g.keyswitchNote < 0 ? 1 : (g.keyswitchNote + 2), juce::dontSendNotification);
    grpVolFader.slider.setValue (g.volume, juce::dontSendNotification);
    grpPanFader.slider.setValue (g.pan, juce::dontSendNotification);
    delGroupBtn.setEnabled (audioProcessor.groups.size() > 1);
}

void MainPluginView::syncFxControls()
{
    int sIdx = juce::jlimit (0, SampleGroup::numFxSlots - 1, fxSlotSelectBox.getSelectedId() - 1);
    const juce::ScopedLock sl (audioProcessor.zoneLock);
    if (audioProcessor.selectedGroupIndex < 0 || audioProcessor.selectedGroupIndex >= static_cast<int> (audioProcessor.groups.size())) return;
    const auto& slot = audioProcessor.groups[static_cast<size_t> (audioProcessor.selectedGroupIndex)].fxSlots[static_cast<size_t> (sIdx)];
    fxTypeBox.setSelectedId (static_cast<int> (slot.type) + 1, juce::dontSendNotification);
    fxEnableBtn.setToggleState (slot.enabled, juce::dontSendNotification);
    fxP1.label.setText (SimpleFxSlot::getParamLabel (slot.type, 0), juce::dontSendNotification);
    fxP2.label.setText (SimpleFxSlot::getParamLabel (slot.type, 1), juce::dontSendNotification);
    fxP3.label.setText (SimpleFxSlot::getParamLabel (slot.type, 2), juce::dontSendNotification);
    fxP1.slider.setValue (slot.p1, juce::dontSendNotification);
    fxP2.slider.setValue (slot.p2, juce::dontSendNotification);
    fxP3.slider.setValue (slot.p3, juce::dontSendNotification);
}

void MainPluginView::selectAllInEditTab()
{
    if (activeTab == ActiveTab::Edit) editKeyMapper.selectAllZones();
}

void MainPluginView::deleteSelectedInEditTab()
{
    if (activeTab == ActiveTab::Edit)
    {
        editKeyMapper.deleteSelectedZones();
        syncPerZoneControls();
        repaintDynamicAreas();
    }
}

void MainPluginView::applyBpmText()
{
    float val = juce::jlimit (40.0f, 240.0f, bpmInputBox.getText().getFloatValue());
    if (val < 40.0f) val = 120.0f;
    bpmInputBox.setText (juce::String (val, 1), juce::dontSendNotification);
    if (auto* p = audioProcessor.apvts.getParameter ("internalBpm"))
        p->setValueNotifyingHost (p->convertTo0to1 (val));
}

void MainPluginView::setActiveTab (ActiveTab tab)
{
    activeTab = tab;
    bool isMain  = (tab == ActiveTab::Main);
    bool isEdit  = (tab == ActiveTab::Edit);
    bool isPanel = (tab == ActiveTab::EditPanel);

    tabMainBtn.setColour  (juce::TextButton::buttonColourId, isMain  ? juce::Colour (0xffc93b7b) : juce::Colour (0xff242c3a));
    tabEditBtn.setColour  (juce::TextButton::buttonColourId, isEdit  ? juce::Colour (0xffc93b7b) : juce::Colour (0xff242c3a));
    tabPanelBtn.setColour (juce::TextButton::buttonColourId, isPanel ? juce::Colour (0xffc93b7b) : juce::Colour (0xff242c3a));

    mountedCard.setVisible (isMain);
    mainWaveDisplay.setVisible (isMain);
    rhythmSeqHeaderLbl.setVisible (isMain);
    seqBypassBtn.setVisible (isMain);
    seqComp.setVisible (isMain);
    playPauseBtn.setVisible (isMain);
    hostSyncBtn.setVisible (isMain);
    bpmLabel.setVisible (isMain);
    bpmInputBox.setVisible (isMain);
    shuffleFader.setVisible (isMain);
    timeRandFader.setVisible (isMain);
    velRandFader.setVisible (isMain);
    gridLabel.setVisible (isMain);
    gridBox.setVisible (isMain);
    seqLoopBtn.setVisible (isMain);
    seqClearBtn.setVisible (isMain);

    editViewport.setVisible (isEdit);

    panelGridBox.setVisible (isPanel);
    knobStyleBox.setVisible (isPanel);
    faderStyleBox.setVisible (isPanel);
    switchStyleBox.setVisible (isPanel);
    logoDesignBox.setVisible (isPanel);
    logoLettersEdit.setVisible (isPanel);
    titleTextEdit.setVisible (isPanel);
    newNodeNameEdit.setVisible (isPanel);
    addPatchNodeBtn.setVisible (isPanel);
    patchCanvas.setVisible (isPanel);
    panelTargetMainBtn.setVisible (isPanel);
    panelTargetEditBtn.setVisible (isPanel);
    panelDoneBtn.setVisible (isPanel);

    resized();
    repaint();
}

void MainPluginView::togglePlayPause()
{
    bool next = ! audioProcessor.internalPlaying.load();
    audioProcessor.internalPlaying.store (next);
    if (next)
        if (auto* p = audioProcessor.apvts.getParameter ("seqLoop"))
            p->setValueNotifyingHost (1.0f);
    playPauseBtn.setButtonText (next ? "PAUSE" : "PLAY");
    playPauseBtn.setColour (juce::TextButton::buttonColourId, next ? juce::Colour (0xffc93b7b) : juce::Colour (0xff273549));
}

void MainPluginView::syncPerZoneControls()
{
    trackSelectBox.clear (juce::dontSendNotification);
    const juce::ScopedLock sl (audioProcessor.zoneLock);
    if (audioProcessor.selectedZoneIndex < 0 || audioProcessor.selectedZoneIndex >= static_cast<int> (audioProcessor.zones.size()))
    {
        realRRBtn.setToggleState (false, juce::dontSendNotification);
        sampleLoopBtn.setToggleState (false, juce::dontSendNotification);
        autoLoopZeroBtn.setToggleState (true, juce::dontSendNotification);
        reverseBtn.setToggleState (false, juce::dontSendNotification);
        zoneColourBox.setSelectedId (1, juce::dontSendNotification);
        return;
    }

    const auto& z = audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)];
    for (int i = 0; i < static_cast<int> (z.layers.size()); ++i)
        trackSelectBox.addItem (z.layers[static_cast<size_t> (i)].name, i + 1);
    if (! z.layers.empty())
        trackSelectBox.setSelectedId (juce::jlimit (0, static_cast<int> (z.layers.size()) - 1, z.selectedLayerIndex) + 1, juce::dontSendNotification);

    zoneColourBox.setSelectedId (z.colourIndex + 1, juce::dontSendNotification);
    realRRBtn.setToggleState (z.realRREnabled, juce::dontSendNotification);
    sampleLoopBtn.setToggleState (z.sampleLoop, juce::dontSendNotification);
    autoLoopZeroBtn.setToggleState (z.autoLoopZeroCross, juce::dontSendNotification);
    crossfadeFader.slider.setValue (z.loopCrossfade, juce::dontSendNotification);
    reverseBtn.setToggleState (z.reverse, juce::dontSendNotification);

    pitchFader.slider.setValue (z.pitchSemi, juce::dontSendNotification);
    panFader.slider.setValue (z.pan, juce::dontSendNotification);
    volFader.slider.setValue (z.volume, juce::dontSendNotification);
    attackFader.slider.setValue (z.attackSec, juce::dontSendNotification);
    decayFader.slider.setValue (z.decaySec, juce::dontSendNotification);
    sustainFader.slider.setValue (z.sustainLvl, juce::dontSendNotification);
    releaseFader.slider.setValue (z.releaseSec, juce::dontSendNotification);
    cutoffFader.slider.setValue (z.cutoffHz, juce::dontSendNotification);
    resFader.slider.setValue (z.resQ, juce::dontSendNotification);
}

void MainPluginView::showScaleMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "50%"); menu.addItem (2, "75%"); menu.addItem (3, "100% (Default)"); menu.addItem (4, "125%");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&brandHeader),
        [this] (int result)
        {
            if (result == 1 && scaleCallback) scaleCallback (0.50f);
            else if (result == 2 && scaleCallback) scaleCallback (0.75f);
            else if (result == 3 && scaleCallback) scaleCallback (1.00f);
            else if (result == 4 && scaleCallback) scaleCallback (1.25f);
        });
}

void MainPluginView::saveAsDialog()
{
    audioProcessor.currentPresetName = presetNameBox.getText().trim();
    juce::String defaultName = audioProcessor.currentPresetName.isEmpty() ? "Untitled" : audioProcessor.currentPresetName;
    auto defaultFile = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (defaultName + ".mn6x");
    fileChooser = std::make_unique<juce::FileChooser> ("Save Preset (.mn6x)", defaultFile, "*.mn6x");
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f != juce::File())
            {
                if (! f.hasFileExtension (".mn6x")) f = f.withFileExtension (".mn6x");
                audioProcessor.currentPresetName = f.getFileNameWithoutExtension();
                presetNameBox.setText (audioProcessor.currentPresetName, juce::dontSendNotification);
                audioProcessor.savePresetMn6x (f);
            }
        });
}

int MainPluginView::getNumRows() { return static_cast<int> (visibleLibRows.size()); }

void MainPluginView::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int> (visibleLibRows.size())) return;
    const auto& vr = visibleLibRows[static_cast<size_t> (rowNumber)];

    if (vr.isCategory)
    {
        auto box = juce::Rectangle<float> (2.0f, 2.0f, static_cast<float> (width - 4), static_cast<float> (height - 4));
        g.setColour (vr.expanded ? juce::Colour (0xff25354f) : juce::Colour (0xff1c2433));
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (vr.expanded ? juce::Colour (0xff5aa9ff) : juce::Colour (0xff35445e));
        g.drawRoundedRectangle (box.reduced (0.5f), 5.0f, 1.2f);

        g.setColour (juce::Colour (0xff74b9ff));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (vr.expanded ? "v" : ">", 8, 0, 14, height, juce::Justification::centredLeft);

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        g.drawText (vr.label, 24, 0, width - 30, height, juce::Justification::centredLeft, true);
    }
    else
    {
        g.fillAll (rowIsSelected ? juce::Colour (0xff2e4a73) : juce::Colour (0xff121720));
        g.setColour (juce::Colour (0xff4cc98f));
        g.fillRect (14, 7, 3, height - 14);
        g.setColour (juce::Colours::lightgrey);
        g.setFont (juce::FontOptions (11.0f, juce::Font::plain));
        g.drawText (vr.label, 22, 0, width - 26, height, juce::Justification::centredLeft, true);
    }
}

void MainPluginView::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= static_cast<int> (visibleLibRows.size())) return;
    const auto& vr = visibleLibRows[static_cast<size_t> (row)];
    if (vr.isCategory)
    {
        audioProcessor.toggleLibraryCategoryExpanded (vr.catIdx);
        rebuildLibraryRows();
    }
    else
    {
        loadLibraryItem (vr.file);
    }
}

void MainPluginView::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= static_cast<int> (visibleLibRows.size())) return;
    const auto& vr = visibleLibRows[static_cast<size_t> (row)];
    if (! vr.isCategory)
        loadLibraryItem (vr.file);
}

void MainPluginView::handleFilesDropped (const juce::StringArray& files, int localX, int localY)
{
    bool hasPreset = false;
    for (const auto& path : files) if (juce::File (path).hasFileExtension (".mn6x")) hasPreset = true;
    if (activeTab == ActiveTab::Main && ! hasPreset) return;

    auto editLocal = editContent.getLocalPoint (this, juce::Point<int> (localX, localY));

    if (activeTab == ActiveTab::Edit && waveEditor.getBounds().contains (editLocal))
    {
        int targetZone = audioProcessor.selectedZoneIndex;
        for (const auto& path : files)
        {
            juce::File f (path);
            if (f.hasFileExtension (".mn6x"))
            {
                audioProcessor.loadPresetMn6x (f);
                presetNameBox.setText (audioProcessor.currentPresetName, juce::dontSendNotification);
                refreshGroupBoxes(); syncGroupControls(); syncFxControls();
            }
            else
            {
                if (targetZone < 0 || targetZone >= static_cast<int> (audioProcessor.zones.size()))
                {
                    audioProcessor.loadSampleIntoNewZone (f, 60, 60, 60);
                    targetZone = audioProcessor.selectedZoneIndex;
                }
                else audioProcessor.addSampleToZone (targetZone, f);
            }
        }
        editKeyMapper.selectSingleZone (audioProcessor.selectedZoneIndex);
        syncPerZoneControls(); repaintDynamicAreas();
        return;
    }

    int targetKey = 60 + static_cast<int> (audioProcessor.zones.size());
    if (activeTab == ActiveTab::Edit && editKeyMapper.getBounds().contains (editLocal))
    {
        auto mapLocal = editKeyMapper.getLocalPoint (this, juce::Point<int> (localX, localY));
        targetKey = juce::jlimit (0, 120, editKeyMapper.noteAtX (mapLocal.x));
    }
    targetKey = juce::jlimit (0, 120, targetKey);

    std::vector<juce::File> audioFiles;
    for (const auto& path : files)
    {
        juce::File f (path);
        if (f.hasFileExtension (".mn6x"))
        {
            audioProcessor.loadPresetMn6x (f);
            presetNameBox.setText (audioProcessor.currentPresetName, juce::dontSendNotification);
            refreshGroupBoxes(); syncGroupControls(); syncFxControls();
        }
        else if (activeTab == ActiveTab::Edit)
            audioFiles.push_back (f);
    }

    int count = static_cast<int> (audioFiles.size());
    for (int i = 0; i < count; ++i)
    {
        int lowV  = (count <= 1) ? 0   : static_cast<int> (std::round ((i * 127.0f) / count)) + (i > 0 ? 1 : 0);
        int highV = (count <= 1) ? 127 : static_cast<int> (std::round (((i + 1) * 127.0f) / count));
        audioProcessor.loadSampleIntoNewZone (audioFiles[static_cast<size_t> (i)], targetKey, targetKey, targetKey, lowV, highV);
    }

    editKeyMapper.selectSingleZone (audioProcessor.selectedZoneIndex);
    syncPerZoneControls();
    repaintDynamicAreas();
}

void MainPluginView::repaintDynamicAreas()
{
    if (mainWaveDisplay.isVisible()) mainWaveDisplay.repaint();
    if (seqComp.isVisible()) seqComp.repaint();
    if (editKeyMapper.isVisible()) editKeyMapper.repaint();
    if (waveEditor.isVisible()) waveEditor.repaint();
    bottomPiano.repaint();
}

void MainPluginView::layoutEditContent (int viewportW)
{
    int contentW = std::max (660, viewportW - 14);
    int y = 4;

    auto grpTop = juce::Rectangle<int> (4, y, contentW - 8, 28);
    addGroupBtn.setBounds     (grpTop.removeFromLeft (54).reduced (1));
    delGroupBtn.setBounds     (grpTop.removeFromLeft (58).reduced (1));
    groupSelectBox.setBounds  (grpTop.removeFromLeft (98).reduced (1));
    groupNameEdit.setBounds   (grpTop.removeFromLeft (104).reduced (2, 1));
    groupMuteBtn.setBounds    (grpTop.removeFromLeft (32).reduced (1));
    groupSoloBtn.setBounds    (grpTop.removeFromLeft (32).reduced (1));
    selGroupOnlyBtn.setBounds (grpTop.removeFromLeft (62).reduced (1));
    groupKsBox.setBounds      (grpTop.removeFromLeft (88).reduced (1));
    y += 32;

    auto grpFaders = juce::Rectangle<int> (4, y, contentW - 8, 34);
    int gfW = grpFaders.getWidth() / 2;
    grpVolFader.setBounds (grpFaders.removeFromLeft (gfW).reduced (3, 0));
    grpPanFader.setBounds (grpFaders.reduced (3, 0));
    y += 40;

    auto mapHdr = juce::Rectangle<int> (4, y, contentW - 8, 26);
    frrBtn.setBounds            (mapHdr.removeFromLeft (48).reduced (1));
    pitchTrackBtn.setBounds     (mapHdr.removeFromLeft (56).reduced (1));
    selectAllZonesBtn.setBounds (mapHdr.removeFromLeft (42).reduced (1));
    delZoneBtn.setBounds        (mapHdr.removeFromLeft (42).reduced (1));
    autoGridBtn.setBounds       (mapHdr.removeFromLeft (72).reduced (1));
    zoneColourBox.setBounds     (mapHdr.removeFromRight (110).reduced (1));
    velGridBox.setBounds        (mapHdr.removeFromRight (84).reduced (1));
    y += 28;

    editKeyMapper.setBounds (4, y, contentW - 8, 235);
    y += 243;

    auto wavHdr = juce::Rectangle<int> (4, y, contentW - 8, 26);
    wavePlayBtn.setBounds     (wavHdr.removeFromLeft (44).reduced (1));
    trackSelectBox.setBounds  (wavHdr.removeFromLeft (148).reduced (1));
    realRRBtn.setBounds       (wavHdr.removeFromLeft (38).reduced (1));
    sampleLoopBtn.setBounds   (wavHdr.removeFromLeft (50).reduced (1));
    autoLoopZeroBtn.setBounds (wavHdr.removeFromLeft (114).reduced (1));
    reverseBtn.setBounds      (wavHdr.removeFromLeft (44).reduced (1));
    normalizeBtn.setBounds    (wavHdr.removeFromLeft (52).reduced (1));
    resetZoneBtn.setBounds    (wavHdr.removeFromRight (52).reduced (1));
    y += 28;

    waveEditor.setBounds (4, y, contentW - 8, 175);
    y += 181;

    crossfadeFader.setBounds (4, y, contentW - 8, 34);
    y += 38;

    HorizontalFader* sFaders[9] = {
        &pitchFader, &panFader, &volFader,
        &attackFader, &decayFader, &sustainFader,
        &releaseFader, &cutoffFader, &resFader
    };
    for (int row = 0; row < 3; ++row)
    {
        auto sfRow = juce::Rectangle<int> (4, y, contentW - 8, 34);
        int sfW = sfRow.getWidth() / 3;
        for (int col = 0; col < 3; ++col)
            sFaders[row * 3 + col]->setBounds (sfRow.removeFromLeft (sfW).reduced (3, 0));
        y += 38;
    }

    y += 12;
    auto fxHdr = juce::Rectangle<int> (4, y, contentW - 8, 28);
    fxSlotSelectBox.setBounds (fxHdr.removeFromLeft (78).reduced (1));
    fxTypeBox.setBounds       (fxHdr.removeFromLeft (96).reduced (1));
    fxEnableBtn.setBounds     (fxHdr.removeFromLeft (48).reduced (1));
    fxBypassBtn.setBounds     (fxHdr.removeFromLeft (58).reduced (1));
    y += 34;

    auto fxFaderRow = juce::Rectangle<int> (4, y, contentW - 8, 36);
    int fxW = fxFaderRow.getWidth() / 3;
    fxP1.setBounds (fxFaderRow.removeFromLeft (fxW).reduced (3, 0));
    fxP2.setBounds (fxFaderRow.removeFromLeft (fxW).reduced (3, 0));
    fxP3.setBounds (fxFaderRow.reduced (3, 0));
    y += 46;

    editContent.setSize (contentW, y + 10);
}

void MainPluginView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c222d));
    g.setColour (juce::Colour (0xff6e7b91));
    g.setFont (10.0f);
    g.drawText (audioProcessor.getBuildVersionString(), getWidth() - 64, getHeight() - 15, 56, 13, juce::Justification::centredRight);

    if (activeTab == ActiveTab::Edit && editViewport.isVisible())
    {
        int divY = editViewport.getY() + 654 - editViewport.getViewPositionY();
        if (divY > editViewport.getY() && divY < editViewport.getBottom())
        {
            g.setColour (juce::Colour (0xff3c4c66));
            g.drawHorizontalLine (divY, static_cast<float> (editViewport.getX() + 10), static_cast<float> (editViewport.getRight() - 20));
            g.setColour (juce::Colour (0xff8fa3c4));
            g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
            g.drawText ("EFFECTS", editViewport.getX() + 14, divY + 3, 90, 14, juce::Justification::centredLeft);
        }
    }
}

void MainPluginView::resized()
{
    auto area = getLocalBounds().reduced (8);

    auto libArea = area.removeFromLeft (172);
    brandHeader.setBounds (libArea.removeFromTop (44).reduced (1));
    libArea.removeFromTop (6);
    auto libBtnRow = libArea.removeFromTop (24);
    addLibBtn.setBounds (libBtnRow.removeFromLeft (98).reduced (1));
    sortAzBtn.setBounds (libBtnRow.removeFromLeft (35).reduced (1));
    delLibBtn.setBounds (libBtnRow.reduced (1));
    libArea.removeFromTop (4);
    libraryList.setBounds (libArea.reduced (1, 0));

    area.removeFromLeft (8);

    auto topBar = area.removeFromTop (30);
    tabMainBtn.setBounds  (topBar.removeFromLeft (72).reduced (2, 1));
    tabEditBtn.setBounds  (topBar.removeFromLeft (72).reduced (2, 1));
    tabPanelBtn.setBounds (topBar.removeFromLeft (96).reduced (2, 1));
    topBar.removeFromLeft (6);
    presetNameBox.setBounds (topBar.removeFromLeft (240).reduced (2, 2));
    saveBtn.setBounds       (topBar.removeFromLeft (66).reduced (2));
    saveAsBtn.setBounds     (topBar.removeFromLeft (74).reduced (2));

    auto pianoArea = area.removeFromBottom (34);
    bottomPiano.setBounds (pianoArea.reduced (0, 2));
    area.removeFromBottom (4);

    if (activeTab == ActiveTab::Main)
    {
        area.removeFromTop (4);
        mountedCard.setBounds (area.removeFromTop (36));

        area.removeFromTop (4);
        mainWaveDisplay.setBounds (area.removeFromTop (136));

        area.removeFromTop (5);
        auto seqHdr = area.removeFromTop (18);
        rhythmSeqHeaderLbl.setBounds (seqHdr.removeFromLeft (165));
        seqBypassBtn.setBounds (seqHdr.removeFromRight (56).reduced (1, 0));
        area.removeFromTop (2);
        seqComp.setBounds (area.removeFromTop (216));

        area.removeFromTop (5);
        auto bottomCtrl = area.removeFromTop (62);
        playPauseBtn.setBounds (bottomCtrl.removeFromLeft (60).reduced (2, 12));
        hostSyncBtn.setBounds  (bottomCtrl.removeFromLeft (56).reduced (2, 13));

        auto bpmArea = bottomCtrl.removeFromLeft (58).reduced (2, 6);
        bpmLabel.setBounds (bpmArea.removeFromTop (13));
        bpmInputBox.setBounds (bpmArea.removeFromTop (26).reduced (0, 1));

        shuffleFader.setBounds  (bottomCtrl.removeFromLeft (96).reduced (2, 4));
        timeRandFader.setBounds (bottomCtrl.removeFromLeft (96).reduced (2, 4));
        velRandFader.setBounds  (bottomCtrl.removeFromLeft (96).reduced (2, 4));

        auto gridArea = bottomCtrl.removeFromLeft (94).reduced (2, 6);
        gridLabel.setBounds (gridArea.removeFromTop (13));
        gridBox.setBounds (gridArea.removeFromTop (26).reduced (0, 1));

        seqLoopBtn.setBounds  (bottomCtrl.removeFromLeft (58).reduced (2, 15));
        seqClearBtn.setBounds (bottomCtrl.removeFromLeft (40).reduced (2, 15));
    }
    else if (activeTab == ActiveTab::Edit)
    {
        area.removeFromTop (6);
        editViewport.setBounds (area);
        layoutEditContent (area.getWidth());
    }
    else
    {
        area.removeFromTop (4);
        auto row1 = area.removeFromTop (26);
        panelGridBox.setBounds   (row1.removeFromLeft (108).reduced (1));
        knobStyleBox.setBounds   (row1.removeFromLeft (138).reduced (1));
        faderStyleBox.setBounds  (row1.removeFromLeft (124).reduced (1));
        switchStyleBox.setBounds (row1.removeFromLeft (128).reduced (1));
        panelDoneBtn.setBounds   (row1.removeFromRight (72).reduced (1));

        area.removeFromTop (4);
        auto row2 = area.removeFromTop (26);
        logoDesignBox.setBounds   (row2.removeFromLeft (116).reduced (1));
        logoLettersEdit.setBounds (row2.removeFromLeft (86).reduced (2, 1));
        titleTextEdit.setBounds   (row2.removeFromLeft (128).reduced (2, 1));
        newNodeNameEdit.setBounds (row2.removeFromLeft (148).reduced (2, 1));
        addPatchNodeBtn.setBounds (row2.removeFromLeft (84).reduced (1));

        area.removeFromTop (4);
        auto bottomBar = area.removeFromBottom (28);
        panelTargetMainBtn.setBounds (bottomBar.removeFromLeft (96).reduced (2, 1));
        panelTargetEditBtn.setBounds (bottomBar.removeFromLeft (96).reduced (2, 1));

        area.removeFromBottom (4);
        patchCanvas.setBounds (area);
    }
}

// ---------------- Outer Scalable Editor ----------------
RythmeAudioProcessorEditor::RythmeAudioProcessorEditor (RythmeAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      mainView (p, [this] (float s) { applyUiScale (s); })
{
    setWantsKeyboardFocus (true);
    addAndMakeVisible (mainView);
    mainView.setBounds (0, 0, baseWidth, baseHeight);
    setSize (baseWidth, baseHeight);
    startTimerHz (20);
}

RythmeAudioProcessorEditor::~RythmeAudioProcessorEditor() { stopTimer(); }

bool RythmeAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey) { mainView.togglePlayPause(); return true; }
    if (key == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0)) { mainView.selectAllInEditTab(); return true; }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) { mainView.deleteSelectedInEditTab(); return true; }
    return false;
}

void RythmeAudioProcessorEditor::applyUiScale (float newScale)
{
    currentScale = newScale;
    mainView.setTransform (juce::AffineTransform::scale (currentScale));
    setSize (static_cast<int> (std::round (baseWidth * currentScale)),
             static_cast<int> (std::round (baseHeight * currentScale)));
}

void RythmeAudioProcessorEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff1c222d)); }

void RythmeAudioProcessorEditor::resized()
{
    mainView.setBounds (0, 0, baseWidth, baseHeight);
    mainView.setTransform (juce::AffineTransform::scale (currentScale));
}

void RythmeAudioProcessorEditor::timerCallback() { mainView.repaintDynamicAreas(); }

bool RythmeAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray&) { return true; }

void RythmeAudioProcessorEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    int localX = static_cast<int> (x / currentScale);
    int localY = static_cast<int> (y / currentScale);
    mainView.handleFilesDropped (files, localX, localY);
}
