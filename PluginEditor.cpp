#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

// ---------------- LabeledKnob ----------------
LabeledKnob::LabeledKnob (const juce::String& labelText)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 13);
    slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff5aa9ff));
    slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xffff66aa));
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);

    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, juce::Colour (0xffc9d4e8));
    addAndMakeVisible (label);
}

void LabeledKnob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (12));
    slider.setBounds (r);
}

// ---------------- 10-Octave ColorPianoStrip ----------------
ColorPianoStrip::ColorPianoStrip (NoiseMu6ixRhythmAudioProcessor& p) : processor (p) {}

void ColorPianoStrip::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll (juce::Colour (0xff12161e));
    float keyW = area.getWidth() / static_cast<float> (numKeys);
    int activeNote = processor.lastPlayedNote.load();

    const juce::ScopedLock sl (processor.zoneLock);

    for (int i = 0; i < numKeys; ++i)
    {
        int note = startNote + i;
        int n = note % 12;
        bool isBlack = (n == 1 || n == 3 || n == 6 || n == 8 || n == 10);
        juce::Rectangle<float> kr (area.getX() + i * keyW, static_cast<float> (area.getY()), keyW, static_cast<float> (area.getHeight()));

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
            g.setColour (isMapped ? juce::Colours::white : juce::Colours::darkgrey);
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
    if (pressedNote >= 0)
    {
        processor.uiNoteOff (pressedNote);
        pressedNote = -1;
        repaint();
    }
}

// ---------------- Compact Kontakt-Style GroupEditorPanel ----------------
GroupEditorPanel::GroupEditorPanel (NoiseMu6ixRhythmAudioProcessor& p) : processor (p)
{
    titleLabel.setText ("GROUP EDITOR", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa8bf));
    addAndMakeVisible (titleLabel);

    addAndMakeVisible (editAllGroupsBtn);
    editAllGroupsBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffee6c4d));
    editAllGroupsBtn.onClick = [this]()
    {
        processor.editAllGroups.store (editAllGroupsBtn.getToggleState());
        if (onGroupChanged) onGroupChanged();
    };

    addAndMakeVisible (groupSoloBtn);
    groupSoloBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffe8a138));
    groupSoloBtn.onClick = [this]()
    {
        processor.groupSoloEnabled.store (groupSoloBtn.getToggleState());
        if (onGroupChanged) onGroupChanged();
    };

    addAndMakeVisible (selectByMidiBtn);
    selectByMidiBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff4ca3ff));
    selectByMidiBtn.onClick = [this]()
    {
        processor.selectByMidi.store (selectByMidiBtn.getToggleState());
    };

    addAndMakeVisible (createGroupBtn);
    createGroupBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b3c54));
    createGroupBtn.onClick = [this]()
    {
        processor.createEmptyGroup();
        rebuildGroupButtons();
        if (onGroupChanged) onGroupChanged();
    };

    addAndMakeVisible (deleteGroupBtn);
    deleteGroupBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff592836));
    deleteGroupBtn.onClick = [this]()
    {
        processor.deleteGroup (processor.selectedGroupIndex);
        rebuildGroupButtons();
        if (onGroupChanged) onGroupChanged();
    };

    addAndMakeVisible (groupListViewport);
    groupListViewport.setViewedComponent (&groupListContent, false);
    groupListViewport.setScrollBarsShown (false, true);

    rebuildGroupButtons();
}

void GroupEditorPanel::rebuildGroupButtons()
{
    groupBtns.clear (true);
    const juce::ScopedLock sl (processor.zoneLock);
    int x = 2;
    int btnW = 92;
    int btnH = 22;

    for (int i = 0; i < static_cast<int> (processor.groups.size()); ++i)
    {
        auto* btn = new juce::TextButton (processor.groups[static_cast<size_t> (i)].name);
        bool isSel = (i == processor.selectedGroupIndex);
        btn->setColour (juce::TextButton::buttonColourId, isSel ? juce::Colour (0xff4ca3ff) : juce::Colour (0xff232b38));
        btn->setColour (juce::TextButton::textColourOffId, isSel ? juce::Colours::black : juce::Colours::lightgrey);
        btn->onClick = [this, i]()
        {
            {
                const juce::ScopedLock sl2 (processor.zoneLock);
                processor.selectedGroupIndex = i;
            }
            rebuildGroupButtons();
            if (onGroupChanged) onGroupChanged();
        };
        groupListContent.addAndMakeVisible (btn);
        btn->setBounds (x, 2, btnW, btnH);
        groupBtns.add (btn);
        x += btnW + 4;
    }
    groupListContent.setSize (std::max (groupListViewport.getWidth(), x + 4), 26);
    repaint();
}

void GroupEditorPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff171d27));
    g.setColour (juce::Colour (0xff2e3748));
    g.drawRect (getLocalBounds(), 1);
}

void GroupEditorPanel::resized()
{
    auto r = getLocalBounds().reduced (4);
    auto topBar = r.removeFromTop (24);
    titleLabel.setBounds (topBar.removeFromLeft (92));
    editAllGroupsBtn.setBounds (topBar.removeFromLeft (108).reduced (1));
    groupSoloBtn.setBounds (topBar.removeFromLeft (86).reduced (1));
    selectByMidiBtn.setBounds (topBar.removeFromLeft (104).reduced (1));
    deleteGroupBtn.setBounds (topBar.removeFromRight (58).reduced (2, 1));
    createGroupBtn.setBounds (topBar.removeFromRight (102).reduced (2, 1));

    r.removeFromTop (3);
    groupListViewport.setBounds (r);
    rebuildGroupButtons();
}

// ---------------- 10-Octave KeyMapperComponent ----------------
KeyMapperComponent::KeyMapperComponent (NoiseMu6ixRhythmAudioProcessor& p) : processor (p) {}

bool KeyMapperComponent::isZoneVisible (const SampleZone& z) const
{
    if (readOnly)
        return true;
    if (selectedGroupsOnly && ! processor.editAllGroups.load())
        return z.groupIndex == processor.selectedGroupIndex;
    return true;
}

void KeyMapperComponent::sanitizeSelection()
{
    int n = static_cast<int> (processor.zones.size());
    for (auto it = selectedZoneIndices.begin(); it != selectedZoneIndices.end(); )
    {
        if (*it < 0 || *it >= n || ! isZoneVisible (processor.zones[static_cast<size_t> (*it)]))
            it = selectedZoneIndices.erase (it);
        else
            ++it;
    }
    if (selectedZoneIndices.empty() && processor.selectedZoneIndex >= 0 && processor.selectedZoneIndex < n)
    {
        if (isZoneVisible (processor.zones[static_cast<size_t> (processor.selectedZoneIndex)]))
            selectedZoneIndices.insert (processor.selectedZoneIndex);
    }
}

void KeyMapperComponent::selectAllZones()
{
    const juce::ScopedLock sl (processor.zoneLock);
    selectedZoneIndices.clear();
    int n = static_cast<int> (processor.zones.size());
    for (int i = 0; i < n; ++i)
    {
        if (isZoneVisible (processor.zones[static_cast<size_t> (i)]))
        {
            selectedZoneIndices.insert (i);
            processor.selectedZoneIndex = i;
        }
    }
    if (onSelectionChanged) onSelectionChanged();
    repaint();
}

void KeyMapperComponent::clearSelection()
{
    selectedZoneIndices.clear();
    repaint();
}

void KeyMapperComponent::selectSingleZone (int idx)
{
    selectedZoneIndices.clear();
    if (idx >= 0)
        selectedZoneIndices.insert (idx);
    repaint();
}

void KeyMapperComponent::autoGridSelectedZones()
{
    const juce::ScopedLock sl (processor.zoneLock);
    sanitizeSelection();
    if (selectedZoneIndices.empty()) return;

    // Evenly distribute selected zones across velocity layers (0..127)
    int count = static_cast<int> (selectedZoneIndices.size());
    if (count == 1)
    {
        auto& z = processor.zones[static_cast<size_t> (*selectedZoneIndices.begin())];
        z.lowVel = 0;
        z.highVel = 127;
    }
    else
    {
        int i = 0;
        for (int idx : selectedZoneIndices)
        {
            auto& z = processor.zones[static_cast<size_t> (idx)];
            z.lowVel = static_cast<int> (std::round ((i * 127.0f) / count));
            z.highVel = static_cast<int> (std::round (((i + 1) * 127.0f) / count));
            if (i > 0 && z.lowVel < 127) z.lowVel += 1;
            ++i;
        }
    }
    if (onSelectionChanged) onSelectionChanged();
    repaint();
}

juce::Rectangle<int> KeyMapperComponent::getGridArea() const
{
    auto r = getLocalBounds();
    int leftPad = readOnly ? 0 : 28;
    r.removeFromLeft (leftPad);
    return r.removeFromTop (r.getHeight() - (readOnly ? 26 : 36));
}

juce::Rectangle<int> KeyMapperComponent::getKeyboardArea() const
{
    auto r = getLocalBounds();
    int leftPad = readOnly ? 0 : 28;
    r.removeFromLeft (leftPad);
    return r.removeFromBottom (readOnly ? 26 : 36);
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
    if (velGridDivs <= 0)
        return rawVel;
    float step = 127.0f / static_cast<float> (velGridDivs);
    int snapped = static_cast<int> (std::round (rawVel / step) * step);
    return juce::jlimit (0, 127, snapped);
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
    int labelStep = (divs <= 8) ? 1 : (divs <= 16 ? 2 : (divs <= 32 ? 4 : 8));
    int majorStep = (divs <= 8) ? 1 : (divs / 8);

    for (int i = 0; i <= divs; ++i)
    {
        float norm = i / static_cast<float> (divs);
        int y = gridArea.getBottom() - static_cast<int> (norm * gridArea.getHeight());
        bool isMajor = (i % std::max (1, majorStep) == 0);
        g.setColour (readOnly ? juce::Colour (0xff222936)
                              : (isMajor ? juce::Colour (0xff2d384a) : juce::Colour (0xff1e2531)));
        g.drawHorizontalLine (y, static_cast<float> (gridArea.getX()), static_cast<float> (gridArea.getRight()));

        if (! readOnly && (i % labelStep == 0 || i == divs))
        {
            int velVal = static_cast<int> (std::round (norm * 127.0f));
            g.setColour (juce::Colour (0xff8a99b3));
            g.setFont (8.5f);
            g.drawText (juce::String (velVal), 2, y - 6, 24, 12, juce::Justification::centredRight, false);
        }
    }

    float keyW = gridArea.getWidth() / static_cast<float> (numKeys);
    g.setColour (juce::Colour (0xff232b38));
    for (int i = 0; i < numKeys; ++i)
    {
        if ((startNote + i) % 12 == 0)
            g.drawVerticalLine (static_cast<int> (gridArea.getX() + i * keyW), static_cast<float> (gridArea.getY()), static_cast<float> (gridArea.getBottom()));
    }

    const juce::ScopedLock sl (processor.zoneLock);
    sanitizeSelection();

    int visibleCount = 0;
    for (const auto& z : processor.zones)
        if (isZoneVisible (z)) ++visibleCount;

    if (visibleCount == 0)
    {
        g.setColour (juce::Colour (0xff78849b));
        g.setFont (11.5f);
        if (readOnly)
            g.drawText ("Key Map Overview (Switch to EDIT tab to add or edit samples)", gridArea, juce::Justification::centred);
        else
            g.drawText ("Drag & Drop WAV/AIFF onto any key | Drag empty space to box-select | Right-Click piano key to set Root Key",
                        gridArea, juce::Justification::centred);
    }

    int selectedRootKey = -1;
    if (processor.selectedZoneIndex >= 0 && processor.selectedZoneIndex < static_cast<int> (processor.zones.size()))
    {
        if (isZoneVisible (processor.zones[static_cast<size_t> (processor.selectedZoneIndex)]))
            selectedRootKey = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)].rootKey;
    }

    for (int i = 0; i < static_cast<int> (processor.zones.size()); ++i)
    {
        const auto& z = processor.zones[static_cast<size_t> (i)];
        if (! isZoneVisible (z)) continue;

        auto r = getZoneRect (z, gridArea);
        bool isSelected = (! readOnly && selectedZoneIndices.count (i) > 0)
                       || (readOnly && i == processor.selectedZoneIndex);

        g.setColour (z.colour.withAlpha (isSelected ? 0.82f : 0.60f));
        g.fillRect (r);
        g.setColour (isSelected ? juce::Colours::white : z.colour.brighter (0.25f));
        g.drawRect (r, isSelected ? 2.0f : 1.0f);

        juce::String label = z.name;
        if (z.layers.size() > 1) label += " (" + juce::String (z.layers.size()) + "RR)";
        if (! readOnly && i == processor.selectedZoneIndex)
            label += " [R:" + juce::MidiMessage::getMidiNoteName (z.rootKey, true, true, 5)
                   + " V:" + juce::String (z.lowVel) + "-" + juce::String (z.highVel) + "]";

        g.setColour (juce::Colours::white);
        g.setFont (9.5f);
        g.drawText (label, r.reduced (3.0f, 1.0f), juce::Justification::topLeft, true);
    }

    if (! readOnly && isMarqueeDragging)
    {
        auto mr = juce::Rectangle<float>::leftTopRightBottom (
            std::min (marqueeStart.x, marqueeCurrent.x),
            std::min (marqueeStart.y, marqueeCurrent.y),
            std::max (marqueeStart.x, marqueeCurrent.x),
            std::max (marqueeStart.y, marqueeCurrent.y));
        g.setColour (juce::Colour (0x305aa9ff));
        g.fillRect (mr);
        g.setColour (juce::Colour (0xff80c4ff));
        g.drawRect (mr, 1.2f);
    }

    int activeNote = processor.lastPlayedNote.load();
    for (int i = 0; i < numKeys; ++i)
    {
        int note = startNote + i;
        int n = note % 12;
        bool isBlack = (n == 1 || n == 3 || n == 6 || n == 8 || n == 10);
        juce::Rectangle<float> kr (keyArea.getX() + i * keyW, static_cast<float> (keyArea.getY()), keyW, static_cast<float> (keyArea.getHeight()));

        juce::Colour mappedCol = juce::Colours::transparentBlack;
        bool isMapped = false;
        for (const auto& z : processor.zones)
        {
            if (isZoneVisible (z) && note >= z.lowKey && note <= z.highKey)
            {
                mappedCol = z.colour;
                isMapped = true;
            }
        }

        if (note == pressedKey || note == activeNote)
            g.setColour (juce::Colours::white);
        else if (! readOnly && note == selectedRootKey)
            g.setColour (juce::Colour (0xffffd32a));
        else if (isMapped)
            g.setColour (isBlack ? mappedCol.darker (0.35f) : mappedCol);
        else
            g.setColour (isBlack ? juce::Colour (0xff151921) : juce::Colour (0xffd8e0eb));

        g.fillRect (kr);
        g.setColour (juce::Colour (0xff272e3a));
        g.drawRect (kr, 0.5f);

        if (! readOnly && note == selectedRootKey)
        {
            g.setColour (juce::Colours::black);
            g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
            g.drawText ("R", kr.expanded (3.0f, 0.0f), juce::Justification::centredTop, false);
        }

        if (n == 0)
        {
            int oct = note / 12;
            g.setColour (isMapped ? juce::Colours::white : juce::Colours::darkgrey);
            g.setFont (7.5f);
            g.drawText ("C" + juce::String (oct), kr.expanded (5.0f, 0.0f), juce::Justification::centredBottom, false);
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

        // STRICTLY Right-Click ONLY sets Root Key
        if (! readOnly && e.mods.isRightButtonDown())
        {
            const juce::ScopedLock sl (processor.zoneLock);
            sanitizeSelection();
            if (! selectedZoneIndices.empty())
            {
                for (int idx : selectedZoneIndices)
                    if (idx >= 0 && idx < static_cast<int> (processor.zones.size()))
                        processor.zones[static_cast<size_t> (idx)].rootKey = note;
            }
            else if (processor.selectedZoneIndex >= 0 && processor.selectedZoneIndex < static_cast<int> (processor.zones.size()))
            {
                processor.zones[static_cast<size_t> (processor.selectedZoneIndex)].rootKey = note;
            }
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
        if (! isZoneVisible (processor.zones[static_cast<size_t> (i)])) continue;

        auto r = getZoneRect (processor.zones[static_cast<size_t> (i)], gridArea);
        if (r.expanded (4.0f).contains (e.position))
        {
            if (readOnly)
            {
                processor.selectedZoneIndex = i;
                selectedZoneIndices.clear();
                selectedZoneIndices.insert (i);
                if (onSelectionChanged) onSelectionChanged();
                repaint();
                return;
            }

            if (e.mods.isRightButtonDown())
            {
                if (selectedZoneIndices.count (i) > 0 && selectedZoneIndices.size() > 1)
                {
                    std::vector<int> toDel (selectedZoneIndices.begin(), selectedZoneIndices.end());
                    processor.deleteZones (toDel);
                    selectedZoneIndices.clear();
                }
                else
                {
                    processor.deleteZone (i);
                    selectedZoneIndices.clear();
                }
                if (onSelectionChanged) onSelectionChanged();
                repaint();
                return;
            }

            if (e.mods.isShiftDown() || e.mods.isCommandDown())
            {
                if (selectedZoneIndices.count (i) > 0 && selectedZoneIndices.size() > 1)
                    selectedZoneIndices.erase (i);
                else
                    selectedZoneIndices.insert (i);
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
            {
                if (idx >= 0 && idx < static_cast<int> (processor.zones.size()))
                {
                    const auto& z = processor.zones[static_cast<size_t> (idx)];
                    dragSnapshots.push_back ({ idx, z.lowKey, z.highKey });
                }
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
        if (! e.mods.isShiftDown() && ! e.mods.isCommandDown())
            selectedZoneIndices.clear();
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
            std::min (marqueeStart.x, marqueeCurrent.x),
            std::min (marqueeStart.y, marqueeCurrent.y),
            std::max (marqueeStart.x, marqueeCurrent.x),
            std::max (marqueeStart.y, marqueeCurrent.y));

        const juce::ScopedLock sl (processor.zoneLock);
        selectedZoneIndices.clear();
        for (int i = 0; i < static_cast<int> (processor.zones.size()); ++i)
        {
            if (! isZoneVisible (processor.zones[static_cast<size_t> (i)])) continue;
            auto zr = getZoneRect (processor.zones[static_cast<size_t> (i)], gridArea);
            if (mr.intersects (zr))
            {
                selectedZoneIndices.insert (i);
                processor.selectedZoneIndex = i;
            }
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
        int minDelta = -120;
        int maxDelta = 120;
        for (const auto& s : dragSnapshots)
        {
            minDelta = std::max (minDelta, -s.lowKey);
            maxDelta = std::min (maxDelta, 120 - s.highKey);
        }
        int clampedDelta = juce::jlimit (minDelta, maxDelta, deltaKey);

        for (const auto& s : dragSnapshots)
        {
            if (s.index >= 0 && s.index < static_cast<int> (processor.zones.size()))
            {
                auto& z = processor.zones[static_cast<size_t> (s.index)];
                z.lowKey = s.lowKey + clampedDelta;
                z.highKey = s.highKey + clampedDelta;
            }
        }
    }
    else if (dragMode == DragMode::LeftEdge)
    {
        for (const auto& s : dragSnapshots)
        {
            if (s.index >= 0 && s.index < static_cast<int> (processor.zones.size()))
            {
                auto& z = processor.zones[static_cast<size_t> (s.index)];
                z.lowKey = juce::jlimit (0, z.highKey, s.lowKey + deltaKey);
            }
        }
    }
    else if (dragMode == DragMode::RightEdge)
    {
        for (const auto& s : dragSnapshots)
        {
            if (s.index >= 0 && s.index < static_cast<int> (processor.zones.size()))
            {
                auto& z = processor.zones[static_cast<size_t> (s.index)];
                z.highKey = juce::jlimit (z.lowKey, 120, s.highKey + deltaKey);
            }
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
    dragMode = DragMode::None;
    dragZoneIdx = -1;
    dragSnapshots.clear();
    if (isMarqueeDragging)
    {
        isMarqueeDragging = false;
        repaint();
    }
    if (pressedKey >= 0)
    {
        processor.uiNoteOff (pressedKey);
        pressedKey = -1;
        repaint();
    }
}

// ---------------- WaveEditorComponent ----------------
WaveEditorComponent::WaveEditorComponent (NoiseMu6ixRhythmAudioProcessor& p) : processor (p) {}

void WaveEditorComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll (juce::Colour (0xff121720));
    g.setColour (juce::Colour (0xff2e3748));
    g.drawRect (area, 1);

    const juce::ScopedLock sl (processor.zoneLock);
    if (processor.zones.empty() || processor.selectedZoneIndex < 0 || processor.selectedZoneIndex >= static_cast<int> (processor.zones.size()))
    {
        g.setColour (juce::Colour (0xff78849b));
        g.setFont (12.0f);
        g.drawText ("Drag & Drop WAV/AIFF onto Key Map above, or drop here to add Round Robin layers", area, juce::Justification::centred);
        return;
    }

    const auto& z = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)];
    if (z.layers.empty())
    {
        g.setColour (z.colour.brighter (0.3f));
        g.setFont (12.0f);
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

    g.setColour (juce::Colour (0x55000000));
    g.fillRect (static_cast<float> (area.getX()), static_cast<float> (area.getY()), startX - area.getX(), static_cast<float> (area.getHeight()));
    g.fillRect (endX, static_cast<float> (area.getY()), area.getRight() - endX, static_cast<float> (area.getHeight()));

    if (z.sampleLoop && lEndX > lStartX)
    {
        g.setColour (juce::Colour (0x284cc98f));
        g.fillRect (lStartX, static_cast<float> (area.getY()), lEndX - lStartX, static_cast<float> (area.getHeight()));
    }

    juce::Path p;
    float midY = area.getCentreY();
    float halfH = area.getHeight() * 0.43f;
    p.startNewSubPath (static_cast<float> (area.getX()), midY);

    int step = std::max (1, numSamples / std::max (1, area.getWidth()));
    for (int x = 0; x < area.getWidth(); ++x)
    {
        int idx = std::min (numSamples - 1, x * step);
        p.lineTo (static_cast<float> (area.getX() + x), midY - data[idx] * halfH);
    }

    g.setColour (z.colour.brighter (0.25f));
    g.strokePath (p, juce::PathStrokeType (1.25f));

    g.setColour (juce::Colour (0xffff5599));
    g.drawVerticalLine (static_cast<int> (startX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
    g.drawVerticalLine (static_cast<int> (endX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
    g.fillRect (startX, static_cast<float> (area.getY()), 14.0f, 13.0f);
    g.fillRect (endX - 14.0f, static_cast<float> (area.getY()), 14.0f, 13.0f);
    g.setColour (juce::Colours::white);
    g.setFont (9.5f);
    g.drawText ("S", static_cast<int> (startX), area.getY(), 14, 13, juce::Justification::centred);
    g.drawText ("E", static_cast<int> (endX - 14.0f), area.getY(), 14, 13, juce::Justification::centred);

    if (z.sampleLoop)
    {
        g.setColour (juce::Colour (0xff4cc98f));
        g.drawVerticalLine (static_cast<int> (lStartX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
        g.drawVerticalLine (static_cast<int> (lEndX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
        g.fillRect (lStartX, static_cast<float> (area.getBottom() - 13), 18.0f, 13.0f);
        g.fillRect (lEndX - 18.0f, static_cast<float> (area.getBottom() - 13), 18.0f, 13.0f);
        g.setColour (juce::Colours::black);
        g.drawText ("LS", static_cast<int> (lStartX), area.getBottom() - 13, 18, 13, juce::Justification::centred);
        g.drawText ("LE", static_cast<int> (lEndX - 18.0f), area.getBottom() - 13, 18, 13, juce::Justification::centred);
    }

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (11.0f);
    g.drawText ("Track " + juce::String (lIdx + 1) + "/" + juce::String (z.layers.size()) + ": " + layer.name
                + "  |  Root: " + juce::MidiMessage::getMidiNoteName (z.rootKey, true, true, 5),
                area.reduced (18, 4), juce::Justification::topRight, true);
}

void WaveEditorComponent::mouseDown (const juce::MouseEvent& e)
{
    const juce::ScopedLock sl (processor.zoneLock);
    if (processor.selectedZoneIndex < 0 || processor.selectedZoneIndex >= static_cast<int> (processor.zones.size())) return;
    const auto& z = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)];

    float normX = juce::jlimit (0.0f, 1.0f, e.x / static_cast<float> (std::max (1, getWidth())));
    float dS = std::abs (normX - z.startNorm), dE = std::abs (normX - z.endNorm);
    float dLS = z.sampleLoop ? std::abs (normX - z.loopStartNorm) : 99.0f;
    float dLE = z.sampleLoop ? std::abs (normX - z.loopEndNorm) : 99.0f;
    float minD = std::min ({ dS, dE, dLS, dLE });

    if (minD < 0.05f)
    {
        if (minD == dS) activeDrag = MarkerDrag::Start;
        else if (minD == dE) activeDrag = MarkerDrag::End;
        else if (minD == dLS) activeDrag = MarkerDrag::LoopStart;
        else activeDrag = MarkerDrag::LoopEnd;
    }
}

void WaveEditorComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (activeDrag == MarkerDrag::None) return;
    const juce::ScopedLock sl (processor.zoneLock);
    if (processor.selectedZoneIndex < 0 || processor.selectedZoneIndex >= static_cast<int> (processor.zones.size())) return;
    auto& z = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)];

    float normX = juce::jlimit (0.0f, 1.0f, e.x / static_cast<float> (std::max (1, getWidth())));
    if (activeDrag == MarkerDrag::Start) z.startNorm = juce::jlimit (0.0f, z.endNorm - 0.02f, normX);
    else if (activeDrag == MarkerDrag::End) z.endNorm = juce::jlimit (z.startNorm + 0.02f, 1.0f, normX);
    else if (activeDrag == MarkerDrag::LoopStart) z.loopStartNorm = juce::jlimit (z.startNorm, z.loopEndNorm - 0.02f, normX);
    else if (activeDrag == MarkerDrag::LoopEnd) z.loopEndNorm = juce::jlimit (z.loopStartNorm + 0.02f, z.endNorm, normX);

    if (onMarkersChanged) onMarkersChanged();
    repaint();
}

void WaveEditorComponent::mouseUp (const juce::MouseEvent&) { activeDrag = MarkerDrag::None; }

// ---------------- RhythmSequencerComponent ----------------
RhythmSequencerComponent::RhythmSequencerComponent (NoiseMu6ixRhythmAudioProcessor& p) : processor (p) {}

float RhythmSequencerComponent::snapToGrid (float normX) const
{
    int divs = processor.getGridDivisions();
    float step = 1.0f / static_cast<float> (divs);
    float snapped = std::round (normX / step) * step;
    return juce::jlimit (0.0f, 1.0f - step * 0.25f, snapped);
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
        bool isBeat = (i % beatStep == 0);
        g.setColour (isBeat ? juce::Colour (0xff524066) : juce::Colour (0xff2b2135));
        g.drawVerticalLine (static_cast<int> (x), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
    }

    if (processor.apvts.getRawParameterValue ("seqLoop")->load() > 0.5f)
    {
        float px = area.getX() + processor.seqPlayheadNorm.load() * area.getWidth();
        g.setColour (juce::Colour (0x99ffffff));
        g.drawVerticalLine (static_cast<int> (px), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
    }

    const juce::ScopedLock sl (processor.zoneLock);
    if (processor.zones.empty() || processor.selectedZoneIndex < 0 || processor.selectedZoneIndex >= static_cast<int> (processor.zones.size()))
    {
        g.setColour (juce::Colour (0xff8a7a99));
        g.setFont (11.5f);
        g.drawText ("Select a key above to edit its rhythm pattern | Turn ON LOOP & press PLAY or Spacebar", area, juce::Justification::centred);
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

    g.setColour (z.colour.brighter (0.4f));
    g.setFont (10.0f);
    g.drawText ("Pattern: " + z.name + "  |  Click=Add  Drag=Move/Vel  RightClick=Del",
                area.reduced (6, 3), juce::Justification::topRight, true);
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
            if (e.mods.isRightButtonDown())
            {
                events.erase (events.begin() + i);
                repaint();
                return;
            }
            draggedEventIdx = i;
            break;
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

void RhythmSequencerComponent::mouseUp (const juce::MouseEvent&)
{
    draggedEventIdx = -1;
    repaint();
}

// ---------------- MainPluginView ----------------
MainPluginView::MainPluginView (NoiseMu6ixRhythmAudioProcessor& p, std::function<void(float)> onScaleSelected)
    : audioProcessor (p), scaleCallback (std::move (onScaleSelected)),
      mainKeyMapper (p), seqComp (p), groupPanel (p), editKeyMapper (p), waveEditor (p), bottomPiano (p)
{
    addAndMakeVisible (titleScaleBtn);
    titleScaleBtn.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    titleScaleBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff273040));
    titleScaleBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    titleScaleBtn.onClick = [this]() { showScaleMenu(); };

    // TWO Navigation Tabs: MAIN & EDIT
    addAndMakeVisible (tabMainBtn);
    addAndMakeVisible (tabEditBtn);
    tabMainBtn.onClick = [this]() { setActiveTab (ActiveTab::Main); };
    tabEditBtn.onClick = [this]() { setActiveTab (ActiveTab::Edit); };

    // Library Panel
    addAndMakeVisible (addLibBtn);
    addAndMakeVisible (sortAzBtn);
    addAndMakeVisible (delLibBtn);
    addAndMakeVisible (libraryList);
    libraryList.setModel (this);
    libraryList.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff141820));

    addLibBtn.onClick = [this]()
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Select Sample/Library Folder",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this] (const juce::FileChooser& fc)
            {
                auto dir = fc.getResult();
                if (dir.isDirectory()) refreshLibraryFolder (dir);
            });
    };

    sortAzBtn.onClick = [this]()
    {
        std::sort (libraryFiles.begin(), libraryFiles.end(),
            [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareIgnoreCase (b.getFileName()) < 0; });
        libraryList.updateContent();
    };

    delLibBtn.onClick = [this]()
    {
        int row = libraryList.getSelectedRow();
        if (row >= 0 && row < libraryFiles.size())
        {
            libraryFiles.remove (row);
            libraryList.updateContent();
        }
    };

    // Empty Editable Preset Name Box
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
        if (audioProcessor.currentPresetFile != juce::File())
            audioProcessor.savePresetMn6x (audioProcessor.currentPresetFile);
        else
            saveAsDialog();
    };
    saveAsBtn.onClick = [this]() { saveAsDialog(); };

    // ---------------- MAIN TAB SETUP ----------------
    mainKeyMapper.setReadOnly (true);
    mainKeyMapper.onSelectionChanged = [this]() { syncPerZoneControls(); };
    addAndMakeVisible (mainKeyMapper);

    addAndMakeVisible (seqComp);

    addAndMakeVisible (playPauseBtn);
    playPauseBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff273549));
    playPauseBtn.onClick = [this]() { togglePlayPause(); };

    addAndMakeVisible (hostSyncBtn);
    hostSyncBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff4cc98f));
    hostSyncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "hostSync", hostSyncBtn);

    addAndMakeVisible (bpmLabel);
    bpmLabel.setText ("BPM", juce::dontSendNotification);
    bpmLabel.setJustificationType (juce::Justification::centred);
    bpmLabel.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    bpmLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc9d4e8));

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

    addAndMakeVisible (shuffleKnob);
    addAndMakeVisible (timeRandKnob);
    addAndMakeVisible (velRandKnob);
    shuffleAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "shuffle", shuffleKnob.slider);
    timeRandAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "timeRand", timeRandKnob.slider);
    velRandAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "velRand", velRandKnob.slider);

    addAndMakeVisible (gridLabel);
    gridLabel.setText ("Grid", juce::dontSendNotification);
    gridLabel.setJustificationType (juce::Justification::centred);
    gridLabel.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    gridLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc9d4e8));

    addAndMakeVisible (gridBox);
    gridBox.addItemList ({ "1/4", "1/8", "1/12 (T)", "1/16", "1/24 (T)", "1/32", "1/48 (T)", "1/64" }, 1);
    gridAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (audioProcessor.apvts, "grid", gridBox);
    gridBox.onChange = [this]() { seqComp.repaint(); };

    addAndMakeVisible (seqLoopBtn);
    seqLoopBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffe84ea2));
    seqLoopAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "seqLoop", seqLoopBtn);

    // ---------------- EDIT TAB SETUP (Scrollable Viewport) ----------------
    addAndMakeVisible (editViewport);
    editViewport.setViewedComponent (&editContent, false);
    editViewport.setScrollBarsShown (true, false);

    // Compact Kontakt-style Group Editor above Mapping Editor
    editContent.addAndMakeVisible (groupPanel);
    groupPanel.onGroupChanged = [this]()
    {
        editKeyMapper.repaint();
        syncPerZoneControls();
    };

    editKmLabel.setText ("MAPPING EDITOR", juce::dontSendNotification);
    editKmLabel.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    editKmLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa8bf));
    editContent.addAndMakeVisible (editKmLabel);

    editContent.addAndMakeVisible (frrBtn);
    editContent.addAndMakeVisible (fvlBtn);
    editContent.addAndMakeVisible (pitchTrackBtn);
    frrBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff5599));
    fvlBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff4ca3ff));
    pitchTrackBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffe8a138));
    frrAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "frr", frrBtn);
    fvlAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "fvl", fvlBtn);
    pitchTrackAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "pitchTrack", pitchTrackBtn);

    editContent.addAndMakeVisible (selectAllZonesBtn);
    selectAllZonesBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b384e));
    selectAllZonesBtn.onClick = [this]() { editKeyMapper.selectAllZones(); };

    // Kontakt-style Mapping Editor Header Controls: Zone Solo, Selected Groups Only, Auto Grid
    editContent.addAndMakeVisible (zoneSoloBtn);
    zoneSoloBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffe8a138));
    zoneSoloBtn.onClick = [this]()
    {
        audioProcessor.zoneSoloEnabled.store (zoneSoloBtn.getToggleState());
    };

    editContent.addAndMakeVisible (selGroupsOnlyBtn);
    selGroupsOnlyBtn.setToggleState (true, juce::dontSendNotification);
    selGroupsOnlyBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff4cc98f));
    selGroupsOnlyBtn.onClick = [this]()
    {
        editKeyMapper.setSelectedGroupsOnly (selGroupsOnlyBtn.getToggleState());
    };

    editContent.addAndMakeVisible (autoGridBtn);
    autoGridBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b3c54));
    autoGridBtn.setTooltip ("Auto-distribute selected zones evenly across velocity layers (0-127)");
    autoGridBtn.onClick = [this]() { editKeyMapper.autoGridSelectedZones(); };

    // Velocity Grid Selector
    editContent.addAndMakeVisible (velGridBox);
    velGridBox.addItem ("Vel: Free", 1);
    velGridBox.addItem ("Vel: 2", 2);
    velGridBox.addItem ("Vel: 4", 3);
    velGridBox.addItem ("Vel: 8", 4);
    velGridBox.addItem ("Vel: 16", 5);
    velGridBox.addItem ("Vel: 32", 6);
    velGridBox.addItem ("Vel: 64", 7);
    velGridBox.setSelectedId (3, juce::dontSendNotification);
    velGridBox.onChange = [this]()
    {
        int id = velGridBox.getSelectedId();
        int divs = (id == 1) ? 0 : (id == 2 ? 2 : (id == 3 ? 4 : (id == 4 ? 8 : (id == 5 ? 16 : (id == 6 ? 32 : 64)))));
        editKeyMapper.setVelocityGridDivisions (divs);
    };

    // Zone Color Selector
    editContent.addAndMakeVisible (zoneColourBox);
    for (int c = 0; c < NoiseMu6ixRhythmAudioProcessor::numPaletteColours; ++c)
        zoneColourBox.addItem (NoiseMu6ixRhythmAudioProcessor::getPaletteColourName (c), c + 1);
    zoneColourBox.setSelectedId (1, juce::dontSendNotification);
    zoneColourBox.onChange = [this]()
    {
        int cIdx = zoneColourBox.getSelectedId() - 1;
        if (cIdx >= 0)
        {
            const juce::ScopedLock sl (audioProcessor.zoneLock);
            const auto& sel = editKeyMapper.getSelectedZoneIndices();
            if (! sel.empty())
            {
                for (int idx : sel)
                {
                    if (idx >= 0 && idx < static_cast<int> (audioProcessor.zones.size()))
                    {
                        auto& z = audioProcessor.zones[static_cast<size_t> (idx)];
                        z.colourIndex = cIdx;
                        z.colour = NoiseMu6ixRhythmAudioProcessor::getPaletteColour (cIdx);
                    }
                }
            }
            else if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            {
                auto& z = audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)];
                z.colourIndex = cIdx;
                z.colour = NoiseMu6ixRhythmAudioProcessor::getPaletteColour (cIdx);
            }
            repaintDynamicAreas();
        }
    };

    // Kontakt-style Zone Info Strip (Key Range | Vel Range | Root Key | Sample)
    editContent.addAndMakeVisible (zoneInfoBar);
    zoneInfoBar.setFont (juce::FontOptions (10.0f));
    zoneInfoBar.setColour (juce::Label::backgroundColourId, juce::Colour (0xff131822));
    zoneInfoBar.setColour (juce::Label::textColourId, juce::Colour (0xffb8c5db));
    zoneInfoBar.setText ("Key Range: -   |   Vel Range: -   |   Root Key: -   |   Sample: (none selected)", juce::dontSendNotification);

    editKeyMapper.setReadOnly (false);
    editKeyMapper.setSelectedGroupsOnly (true);
    editKeyMapper.setVelocityGridDivisions (4);
    editKeyMapper.onSelectionChanged = [this]() { syncPerZoneControls(); };
    editContent.addAndMakeVisible (editKeyMapper);

    // Sample Wave Editor inside Scrollable Edit Tab
    editWaveLabel.setText ("SAMPLE WAVE EDITOR", juce::dontSendNotification);
    editWaveLabel.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    editWaveLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa8bf));
    editContent.addAndMakeVisible (editWaveLabel);

    editContent.addAndMakeVisible (trackSelectBox);
    trackSelectBox.setTextWhenNoChoicesAvailable ("No Tracks");
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
    realRRBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff5aa9ff));
    realRRBtn.onClick = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].realRREnabled = realRRBtn.getToggleState();
    };

    editContent.addAndMakeVisible (sampleLoopBtn);
    sampleLoopBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff4cc98f));
    sampleLoopBtn.onClick = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].sampleLoop = sampleLoopBtn.getToggleState();
        waveEditor.repaint();
    };

    editContent.addAndMakeVisible (waveEditor);
    waveEditor.onMarkersChanged = [this]() { syncPerZoneControls(); };

    auto bindZoneKnob = [this] (LabeledKnob& k, double minV, double maxV, double stepV, std::function<void(SampleZone&, float)> setter)
    {
        k.slider.setRange (minV, maxV, stepV);
        editContent.addAndMakeVisible (k);
        k.slider.onValueChange = [this, &k, setter]()
        {
            const juce::ScopedLock sl (audioProcessor.zoneLock);
            if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            {
                setter (audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)], static_cast<float> (k.slider.getValue()));
                waveEditor.repaint();
            }
        };
    };

    bindZoneKnob (startKnob,     0.0, 0.95, 0.005, [] (SampleZone& z, float v) { z.startNorm = std::min (v, z.endNorm - 0.02f); });
    bindZoneKnob (endKnob,       0.05, 1.0, 0.005, [] (SampleZone& z, float v) { z.endNorm = std::max (v, z.startNorm + 0.02f); });
    bindZoneKnob (loopStartKnob, 0.0, 0.95, 0.005, [] (SampleZone& z, float v) { z.loopStartNorm = std::min (v, z.loopEndNorm - 0.02f); });
    bindZoneKnob (loopEndKnob,   0.05, 1.0, 0.005, [] (SampleZone& z, float v) { z.loopEndNorm = std::max (v, z.loopStartNorm + 0.02f); });
    bindZoneKnob (pitchKnob,     -24.0, 24.0, 0.1, [] (SampleZone& z, float v) { z.pitchSemi = v; });
    bindZoneKnob (panKnob,       -1.0, 1.0, 0.01,  [] (SampleZone& z, float v) { z.pan = v; });
    bindZoneKnob (volKnob,       0.0, 2.0, 0.01,   [] (SampleZone& z, float v) { z.volume = v; });
    bindZoneKnob (attackKnob,    0.001, 2.0, 0.002,[] (SampleZone& z, float v) { z.attackSec = v; });
    bindZoneKnob (decayKnob,     0.01, 3.0, 0.01,  [] (SampleZone& z, float v) { z.decaySec = v; });
    bindZoneKnob (sustainKnob,   0.0, 1.0, 0.01,   [] (SampleZone& z, float v) { z.sustainLvl = v; });
    bindZoneKnob (releaseKnob,   0.01, 4.0, 0.01,  [] (SampleZone& z, float v) { z.releaseSec = v; });
    bindZoneKnob (cutoffKnob,    80.0, 20000.0, 10.0, [] (SampleZone& z, float v) { z.cutoffHz = v; });
    bindZoneKnob (resKnob,       0.3, 5.0, 0.05,   [] (SampleZone& z, float v) { z.resQ = v; });

    addAndMakeVisible (bottomPiano);

    setActiveTab (ActiveTab::Main);
    syncPerZoneControls();
}

void MainPluginView::selectAllInEditTab()
{
    if (activeTab == ActiveTab::Edit)
        editKeyMapper.selectAllZones();
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
    bool isMain = (tab == ActiveTab::Main);
    bool isEdit = (tab == ActiveTab::Edit);

    tabMainBtn.setColour (juce::TextButton::buttonColourId, isMain ? juce::Colour (0xffc93b7b) : juce::Colour (0xff242c3a));
    tabEditBtn.setColour (juce::TextButton::buttonColourId, isEdit ? juce::Colour (0xffc93b7b) : juce::Colour (0xff242c3a));

    mainKeyMapper.setVisible (isMain);
    seqComp.setVisible (isMain);
    playPauseBtn.setVisible (isMain);
    hostSyncBtn.setVisible (isMain);
    bpmLabel.setVisible (isMain);
    bpmInputBox.setVisible (isMain);
    shuffleKnob.setVisible (isMain);
    timeRandKnob.setVisible (isMain);
    velRandKnob.setVisible (isMain);
    gridLabel.setVisible (isMain);
    gridBox.setVisible (isMain);
    seqLoopBtn.setVisible (isMain);

    editViewport.setVisible (isEdit);
    if (isEdit) groupPanel.rebuildGroupButtons();

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
        zoneColourBox.setSelectedId (1, juce::dontSendNotification);
        zoneInfoBar.setText ("Key Range: -   |   Vel Range: -   |   Root Key: -   |   Sample: (none selected)", juce::dontSendNotification);
        return;
    }

    const auto& z = audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)];
    for (int i = 0; i < static_cast<int> (z.layers.size()); ++i)
        trackSelectBox.addItem ("Track " + juce::String (i + 1) + ": " + z.layers[static_cast<size_t> (i)].name, i + 1);

    if (! z.layers.empty())
        trackSelectBox.setSelectedId (juce::jlimit (0, static_cast<int> (z.layers.size()) - 1, z.selectedLayerIndex) + 1, juce::dontSendNotification);

    juce::String grpName = (z.groupIndex >= 0 && z.groupIndex < static_cast<int> (audioProcessor.groups.size()))
                         ? audioProcessor.groups[static_cast<size_t> (z.groupIndex)].name : "Group 1";
    zoneInfoBar.setText (
        "Group: " + grpName
        + "   |   Key: " + juce::MidiMessage::getMidiNoteName (z.lowKey, true, true, 5)
        + " - " + juce::MidiMessage::getMidiNoteName (z.highKey, true, true, 5)
        + "   |   Vel: " + juce::String (z.lowVel) + " - " + juce::String (z.highVel)
        + "   |   Root: " + juce::MidiMessage::getMidiNoteName (z.rootKey, true, true, 5)
        + "   |   Sample: " + z.name,
        juce::dontSendNotification);

    zoneColourBox.setSelectedId (z.colourIndex + 1, juce::dontSendNotification);
    realRRBtn.setToggleState (z.realRREnabled, juce::dontSendNotification);
    sampleLoopBtn.setToggleState (z.sampleLoop, juce::dontSendNotification);

    startKnob.slider.setValue (z.startNorm, juce::dontSendNotification);
    endKnob.slider.setValue (z.endNorm, juce::dontSendNotification);
    loopStartKnob.slider.setValue (z.loopStartNorm, juce::dontSendNotification);
    loopEndKnob.slider.setValue (z.loopEndNorm, juce::dontSendNotification);
    pitchKnob.slider.setValue (z.pitchSemi, juce::dontSendNotification);
    panKnob.slider.setValue (z.pan, juce::dontSendNotification);
    volKnob.slider.setValue (z.volume, juce::dontSendNotification);
    attackKnob.slider.setValue (z.attackSec, juce::dontSendNotification);
    decayKnob.slider.setValue (z.decaySec, juce::dontSendNotification);
    sustainKnob.slider.setValue (z.sustainLvl, juce::dontSendNotification);
    releaseKnob.slider.setValue (z.releaseSec, juce::dontSendNotification);
    cutoffKnob.slider.setValue (z.cutoffHz, juce::dontSendNotification);
    resKnob.slider.setValue (z.resQ, juce::dontSendNotification);
}

void MainPluginView::showScaleMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "50%");
    menu.addItem (2, "75%");
    menu.addItem (3, "100% (Default)");
    menu.addItem (4, "125%");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&titleScaleBtn),
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

void MainPluginView::refreshLibraryFolder (const juce::File& folder)
{
    libraryFiles.clear();
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, true, "*.wav;*.aiff;*.flac;*.mn6x"))
        libraryFiles.add (f);
    libraryList.updateContent();
}

int MainPluginView::getNumRows() { return libraryFiles.size(); }

void MainPluginView::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowIsSelected) g.fillAll (juce::Colour (0xff3a2b4a));
    g.setColour (juce::Colours::lightgrey);
    g.setFont (11.0f);
    if (rowNumber >= 0 && rowNumber < libraryFiles.size())
        g.drawText (libraryFiles[rowNumber].getFileName(), 5, 0, width - 8, height, juce::Justification::centredLeft, true);
}

void MainPluginView::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= libraryFiles.size()) return;
    auto f = libraryFiles[row];
    if (f.hasFileExtension (".mn6x"))
    {
        audioProcessor.loadPresetMn6x (f);
        presetNameBox.setText (audioProcessor.currentPresetName, juce::dontSendNotification);
        groupPanel.rebuildGroupButtons();
    }
    else
    {
        int nextKey = juce::jlimit (0, 120, 60 + static_cast<int> (audioProcessor.zones.size()));
        audioProcessor.loadSampleIntoNewZone (f, nextKey, nextKey, nextKey);
        editKeyMapper.selectSingleZone (audioProcessor.selectedZoneIndex);
    }
    syncPerZoneControls();
    repaintDynamicAreas();
}

void MainPluginView::handleFilesDropped (const juce::StringArray& files, int localX, int localY)
{
    bool hasPreset = false;
    for (const auto& path : files)
        if (juce::File (path).hasFileExtension (".mn6x")) hasPreset = true;

    if (activeTab == ActiveTab::Main && ! hasPreset)
        return;

    auto editPos = editContent.getLocalPoint (this, juce::Point<int> (localX, localY));

    if (activeTab == ActiveTab::Edit && waveEditor.getBounds().contains (editPos))
    {
        int targetZone = audioProcessor.selectedZoneIndex;
        for (const auto& path : files)
        {
            juce::File f (path);
            if (f.hasFileExtension (".mn6x"))
            {
                audioProcessor.loadPresetMn6x (f);
                presetNameBox.setText (audioProcessor.currentPresetName, juce::dontSendNotification);
                groupPanel.rebuildGroupButtons();
            }
            else
            {
                if (targetZone < 0 || targetZone >= static_cast<int> (audioProcessor.zones.size()))
                {
                    audioProcessor.loadSampleIntoNewZone (f, 60, 60, 60);
                    targetZone = audioProcessor.selectedZoneIndex;
                }
                else
                {
                    audioProcessor.addSampleToZone (targetZone, f);
                }
            }
        }
        editKeyMapper.selectSingleZone (audioProcessor.selectedZoneIndex);
        syncPerZoneControls();
        repaintDynamicAreas();
        return;
    }

    int targetKey = 60 + static_cast<int> (audioProcessor.zones.size());
    if (activeTab == ActiveTab::Edit && editKeyMapper.getBounds().contains (editPos))
        targetKey = juce::jlimit (0, 120, editKeyMapper.noteAtX (editPos.x - editKeyMapper.getX()));
    targetKey = juce::jlimit (0, 120, targetKey);

    for (const auto& path : files)
    {
        juce::File f (path);
        if (f.hasFileExtension (".mn6x"))
        {
            audioProcessor.loadPresetMn6x (f);
            presetNameBox.setText (audioProcessor.currentPresetName, juce::dontSendNotification);
            groupPanel.rebuildGroupButtons();
        }
        else if (activeTab == ActiveTab::Edit)
        {
            audioProcessor.loadSampleIntoNewZone (f, targetKey, targetKey, targetKey);
            targetKey = juce::jlimit (0, 120, targetKey + 1);
        }
    }
    editKeyMapper.selectSingleZone (audioProcessor.selectedZoneIndex);
    syncPerZoneControls();
    repaintDynamicAreas();
}

void MainPluginView::repaintDynamicAreas()
{
    if (mainKeyMapper.isVisible()) mainKeyMapper.repaint();
    if (seqComp.isVisible()) seqComp.repaint();
    if (editViewport.isVisible())
    {
        editKeyMapper.repaint();
        waveEditor.repaint();
    }
    bottomPiano.repaint();
}

void MainPluginView::layoutEditContent (int viewportWidth)
{
    int contentW = std::max (600, viewportWidth - 14);
    int totalH = 825; // Scrollable EDIT tab: Compact Group Editor + Big Mapping Editor + Big Wave Editor
    editContent.setSize (contentW, totalH);

    auto r = juce::Rectangle<int> (0, 0, contentW, totalH).reduced (4);

    // 1. Compact Kontakt-Style Group Editor (62px tall!)
    groupPanel.setBounds (r.removeFromTop (62));
    r.removeFromTop (6);

    // 2. Mapping Editor Header Row 1: Title + FRR/FVL/PITCH/ALL + Zone Solo / Sel Grp Only / Auto Grid
    auto kmRow1 = r.removeFromTop (24);
    editKmLabel.setBounds (kmRow1.removeFromLeft (102));
    frrBtn.setBounds (kmRow1.removeFromLeft (46).reduced (1));
    fvlBtn.setBounds (kmRow1.removeFromLeft (46).reduced (1));
    pitchTrackBtn.setBounds (kmRow1.removeFromLeft (58).reduced (1));
    selectAllZonesBtn.setBounds (kmRow1.removeFromLeft (38).reduced (1));

    autoGridBtn.setBounds (kmRow1.removeFromRight (68).reduced (2, 1));
    selGroupsOnlyBtn.setBounds (kmRow1.removeFromRight (94).reduced (1));
    zoneSoloBtn.setBounds (kmRow1.removeFromRight (80).reduced (1));

    // Mapping Editor Header Row 2: Zone Info Strip + Vel Grid + Color
    r.removeFromTop (2);
    auto kmRow2 = r.removeFromTop (22);
    zoneColourBox.setBounds (kmRow2.removeFromRight (108).reduced (2, 0));
    velGridBox.setBounds (kmRow2.removeFromRight (86).reduced (2, 0));
    zoneInfoBar.setBounds (kmRow2.reduced (2, 1));

    // 3. BIG Full-Width Key Map Editor (340px tall)
    r.removeFromTop (4);
    editKeyMapper.setBounds (r.removeFromTop (340));

    // 4. Sample Wave Editor Header + Display + 13 Knobs
    r.removeFromTop (14);
    auto waveHeader = r.removeFromTop (24);
    editWaveLabel.setBounds (waveHeader.removeFromLeft (150));
    sampleLoopBtn.setBounds (waveHeader.removeFromRight (66));
    realRRBtn.setBounds (waveHeader.removeFromRight (52));
    trackSelectBox.setBounds (waveHeader.removeFromRight (210).reduced (4, 1));

    r.removeFromTop (4);
    waveEditor.setBounds (r.removeFromTop (200));

    r.removeFromTop (8);
    auto knobRow = r.removeFromTop (76);
    LabeledKnob* waveKnobs[] = { &startKnob, &endKnob, &loopStartKnob, &loopEndKnob, &pitchKnob, &panKnob, &volKnob,
                                 &attackKnob, &decayKnob, &sustainKnob, &releaseKnob, &cutoffKnob, &resKnob };
    int kw = knobRow.getWidth() / 13;
    for (auto* k : waveKnobs) k->setBounds (knobRow.removeFromLeft (kw).reduced (1));
}

void MainPluginView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c222d));

    // Version v1.0.8 in bottom-right corner
    g.setColour (juce::Colour (0xff6e7b91));
    g.setFont (10.0f);
    g.drawText ("v1.0.8", getWidth() - 56, getHeight() - 15, 48, 13, juce::Justification::centredRight);

    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.setColour (juce::Colour (0xff9aa8bf));
    g.drawText ("LIBRARY", 10, 8, 100, 14, juce::Justification::centredLeft);

    if (activeTab == ActiveTab::Main)
    {
        g.drawText ("KEY MAP (VIEW ONLY)", 182, 68, 180, 14, juce::Justification::centredLeft);
        g.drawText ("RHYTHM SEQUENCER", 182, 256, 180, 14, juce::Justification::centredLeft);
    }
}

void MainPluginView::resized()
{
    auto area = getLocalBounds().reduced (8);

    // Left Library Panel
    auto libArea = area.removeFromLeft (164);
    libArea.removeFromTop (16);
    auto libBtnRow = libArea.removeFromTop (24);
    addLibBtn.setBounds (libBtnRow.removeFromLeft (96).reduced (1));
    sortAzBtn.setBounds (libBtnRow.removeFromLeft (34).reduced (1));
    delLibBtn.setBounds (libBtnRow.reduced (1));
    libraryList.setBounds (libArea.reduced (1));

    area.removeFromLeft (8);

    // Top Header Row
    auto topBar = area.removeFromTop (28);
    titleScaleBtn.setBounds (topBar.removeFromLeft (210).reduced (1));
    presetNameBox.setBounds (topBar.removeFromLeft (310).reduced (4, 2));
    saveBtn.setBounds (topBar.removeFromLeft (72).reduced (2));
    saveAsBtn.setBounds (topBar.removeFromLeft (78).reduced (2));

    // TWO Tab Buttons: MAIN & EDIT
    area.removeFromTop (4);
    auto tabBar = area.removeFromTop (24);
    tabMainBtn.setBounds (tabBar.removeFromLeft (110).reduced (2, 1));
    tabEditBtn.setBounds (tabBar.removeFromLeft (110).reduced (2, 1));

    // Bottom 10-Octave Piano Strip
    auto pianoArea = area.removeFromBottom (34);
    bottomPiano.setBounds (pianoArea.reduced (0, 2));
    area.removeFromBottom (4);

    if (activeTab == ActiveTab::Main)
    {
        area.removeFromTop (18);
        mainKeyMapper.setBounds (area.removeFromTop (164));

        area.removeFromTop (22);
        seqComp.setBounds (area.removeFromTop (196));

        area.removeFromTop (6);
        auto bottomCtrl = area.removeFromTop (60);
        playPauseBtn.setBounds (bottomCtrl.removeFromLeft (66).reduced (2, 11));
        hostSyncBtn.setBounds (bottomCtrl.removeFromLeft (62).reduced (2, 12));

        auto bpmArea = bottomCtrl.removeFromLeft (68).reduced (3, 4);
        bpmLabel.setBounds (bpmArea.removeFromTop (13));
        bpmInputBox.setBounds (bpmArea.removeFromTop (26).reduced (0, 1));

        shuffleKnob.setBounds (bottomCtrl.removeFromLeft (64).reduced (1));
        timeRandKnob.setBounds (bottomCtrl.removeFromLeft (64).reduced (1));
        velRandKnob.setBounds (bottomCtrl.removeFromLeft (64).reduced (1));

        auto gridArea = bottomCtrl.removeFromLeft (126).reduced (4, 4);
        gridLabel.setBounds (gridArea.removeFromTop (13));
        gridBox.setBounds (gridArea.removeFromTop (26).reduced (0, 1));

        seqLoopBtn.setBounds (bottomCtrl.removeFromLeft (72).reduced (4, 14));
    }
    else
    {
        area.removeFromTop (4);
        editViewport.setBounds (area);
        layoutEditContent (area.getWidth());
    }
}

// ---------------- Outer Scalable Editor ----------------
NoiseMu6ixRhythmAudioProcessorEditor::NoiseMu6ixRhythmAudioProcessorEditor (NoiseMu6ixRhythmAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      mainView (p, [this] (float s) { applyUiScale (s); })
{
    setWantsKeyboardFocus (true);
    addAndMakeVisible (mainView);
    mainView.setBounds (0, 0, baseWidth, baseHeight);
    setSize (baseWidth, baseHeight);
    startTimerHz (20);
}

NoiseMu6ixRhythmAudioProcessorEditor::~NoiseMu6ixRhythmAudioProcessorEditor() { stopTimer(); }

bool NoiseMu6ixRhythmAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        mainView.togglePlayPause();
        return true;
    }
    if (key == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0))
    {
        mainView.selectAllInEditTab();
        return true;
    }
    return false;
}

void NoiseMu6ixRhythmAudioProcessorEditor::applyUiScale (float newScale)
{
    currentScale = newScale;
    mainView.setTransform (juce::AffineTransform::scale (currentScale));
    setSize (static_cast<int> (std::round (baseWidth * currentScale)),
             static_cast<int> (std::round (baseHeight * currentScale)));
}

void NoiseMu6ixRhythmAudioProcessorEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff1c222d)); }

void NoiseMu6ixRhythmAudioProcessorEditor::resized()
{
    mainView.setBounds (0, 0, baseWidth, baseHeight);
    mainView.setTransform (juce::AffineTransform::scale (currentScale));
}

void NoiseMu6ixRhythmAudioProcessorEditor::timerCallback() { mainView.repaintDynamicAreas(); }

bool NoiseMu6ixRhythmAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray&) { return true; }

void NoiseMu6ixRhythmAudioProcessorEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    int localX = static_cast<int> (x / currentScale);
    int localY = static_cast<int> (y / currentScale);
    mainView.handleFilesDropped (files, localX, localY);
}
