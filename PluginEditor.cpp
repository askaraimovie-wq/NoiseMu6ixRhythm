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

// ---------------- 10-Octave ColorPianoStrip (Red Keyswitch Keys!) ----------------
ColorPianoStrip::ColorPianoStrip (NoiseMu6ixRhythmAudioProcessor& p) : processor (p) {}

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
    if (pressedNote >= 0)
    {
        processor.uiNoteOff (pressedNote);
        pressedNote = -1;
        repaint();
    }
}

// ---------------- GroupEditorPanel (5x5 Vertical Grid + Rename + Mute + RR Set + Keyswitch) ----------------
GroupEditorPanel::GroupTile::GroupTile (NoiseMu6ixRhythmAudioProcessor& p, int idx,
                                        std::function<void(int)> onSel, std::function<void()> onMut)
    : processor (p), groupIndex (idx), onSelect (std::move (onSel))
{
    const juce::ScopedLock sl (processor.zoneLock);
    const auto& grp = processor.groups[static_cast<size_t> (idx)];

    addAndMakeVisible (nameLbl);
    nameLbl.setText (grp.name, juce::dontSendNotification);
    nameLbl.setEditable (false, true, false); // Double-click to rename directly on tile!
    nameLbl.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    nameLbl.setColour (juce::Label::textColourId, juce::Colours::white);
    nameLbl.setInterceptsMouseClicks (true, false);
    nameLbl.onTextChange = [this, onMut]()
    {
        const juce::ScopedLock sl2 (processor.zoneLock);
        if (groupIndex >= 0 && groupIndex < static_cast<int> (processor.groups.size()))
            processor.groups[static_cast<size_t> (groupIndex)].name = nameLbl.getText().trim();
        if (onMut) onMut();
    };

    addAndMakeVisible (muteBtn);
    muteBtn.setToggleState (grp.muted, juce::dontSendNotification);
    muteBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffee5253));
    muteBtn.onClick = [this, onMut]()
    {
        const juce::ScopedLock sl2 (processor.zoneLock);
        if (groupIndex >= 0 && groupIndex < static_cast<int> (processor.groups.size()))
            processor.groups[static_cast<size_t> (groupIndex)].muted = muteBtn.getToggleState();
        if (onMut) onMut();
    };
}

void GroupEditorPanel::GroupTile::paint (juce::Graphics& g)
{
    const juce::ScopedLock sl (processor.zoneLock);
    if (groupIndex < 0 || groupIndex >= static_cast<int> (processor.groups.size())) return;
    const auto& grp = processor.groups[static_cast<size_t> (groupIndex)];
    bool isSel = (groupIndex == processor.selectedGroupIndex);

    g.fillAll (isSel ? juce::Colour (0xff2b476b) : juce::Colour (0xff1f2633));
    g.setColour (isSel ? juce::Colour (0xff66b3ff) : juce::Colour (0xff313c4f));
    g.drawRect (getLocalBounds(), isSel ? 1.8f : 1.0f);

    // Small badge for RR Set & Keyswitch
    juce::String badge;
    if (grp.rrSetId > 0) badge += "RR" + juce::String (grp.rrSetId) + " ";
    if (grp.keyswitchNote >= 0) badge += "KS:" + juce::MidiMessage::getMidiNoteName (grp.keyswitchNote, true, true, 5);
    if (badge.isNotEmpty())
    {
        g.setColour (grp.keyswitchNote >= 0 ? juce::Colour (0xffff7675) : juce::Colour (0xff74b9ff));
        g.setFont (9.0f);
        g.drawText (badge, getLocalBounds().reduced (4, 2), juce::Justification::bottomLeft, true);
    }
}

void GroupEditorPanel::GroupTile::resized()
{
    auto r = getLocalBounds().reduced (3);
    muteBtn.setBounds (r.removeFromRight (34).removeFromTop (20));
    nameLbl.setBounds (r.removeFromTop (20));
}

void GroupEditorPanel::GroupTile::mouseDown (const juce::MouseEvent&)
{
    if (onSelect) onSelect (groupIndex);
}

GroupEditorPanel::GroupEditorPanel (NoiseMu6ixRhythmAudioProcessor& p) : processor (p)
{
    titleLabel.setText ("GROUP EDITOR (5x5 Grid)", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (10.5f, juce::Font::bold));
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
        rebuildGroupGrid();
        if (onGroupChanged) onGroupChanged();
    };

    addAndMakeVisible (deleteGroupBtn);
    deleteGroupBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff592836));
    deleteGroupBtn.onClick = [this]()
    {
        processor.deleteGroup (processor.selectedGroupIndex);
        rebuildGroupGrid();
        if (onGroupChanged) onGroupChanged();
    };

    addAndMakeVisible (gridViewport);
    gridViewport.setViewedComponent (&gridContent, false);
    gridViewport.setScrollBarsShown (true, false); // Vertical scrollbar appears from Row 6 (26th group+)

    // Group Start Options Bar
    startBarLabel.setText ("GROUP START OPTIONS:", juce::dontSendNotification);
    startBarLabel.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    startBarLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa8bf));
    addAndMakeVisible (startBarLabel);

    nameFieldLabel.setText ("Name:", juce::dontSendNotification);
    nameFieldLabel.setFont (10.5f);
    addAndMakeVisible (nameFieldLabel);

    addAndMakeVisible (groupNameEdit);
    groupNameEdit.setMultiLine (false);
    groupNameEdit.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff141922));
    groupNameEdit.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff3c4960));
    groupNameEdit.setColour (juce::TextEditor::textColourId, juce::Colours::white);
    auto applyName = [this]()
    {
        const juce::ScopedLock sl (processor.zoneLock);
        if (processor.selectedGroupIndex >= 0 && processor.selectedGroupIndex < static_cast<int> (processor.groups.size()))
            processor.groups[static_cast<size_t> (processor.selectedGroupIndex)].name = groupNameEdit.getText().trim();
        rebuildGroupGrid();
        if (onGroupChanged) onGroupChanged();
    };
    groupNameEdit.onReturnKey = applyName;
    groupNameEdit.onFocusLost = applyName;

    rrSetLabel.setText ("RR Cycle:", juce::dontSendNotification);
    rrSetLabel.setFont (10.5f);
    addAndMakeVisible (rrSetLabel);

    addAndMakeVisible (rrSetBox);
    rrSetBox.addItem ("Off", 1);
    for (int r = 1; r <= 8; ++r)
        rrSetBox.addItem ("RR Set " + juce::String (r), r + 1);
    rrSetBox.onChange = [this]()
    {
        const juce::ScopedLock sl (processor.zoneLock);
        if (processor.selectedGroupIndex >= 0 && processor.selectedGroupIndex < static_cast<int> (processor.groups.size()))
            processor.groups[static_cast<size_t> (processor.selectedGroupIndex)].rrSetId = rrSetBox.getSelectedId() - 1;
        repaint();
        if (onGroupChanged) onGroupChanged();
    };

    ksLabel.setText ("Keyswitch:", juce::dontSendNotification);
    ksLabel.setFont (10.5f);
    addAndMakeVisible (ksLabel);

    addAndMakeVisible (keyswitchBox);
    keyswitchBox.addItem ("None", 1);
    for (int n = 0; n < 36; ++n) // C0 .. B2
        keyswitchBox.addItem (juce::MidiMessage::getMidiNoteName (n, true, true, 5), n + 2);
    keyswitchBox.onChange = [this]()
    {
        const juce::ScopedLock sl (processor.zoneLock);
        if (processor.selectedGroupIndex >= 0 && processor.selectedGroupIndex < static_cast<int> (processor.groups.size()))
        {
            int id = keyswitchBox.getSelectedId();
            processor.groups[static_cast<size_t> (processor.selectedGroupIndex)].keyswitchNote = (id <= 1) ? -1 : (id - 2);
        }
        repaint();
        if (onGroupChanged) onGroupChanged();
    };

    rebuildGroupGrid();
}

void GroupEditorPanel::syncStartBar()
{
    const juce::ScopedLock sl (processor.zoneLock);
    if (processor.selectedGroupIndex < 0 || processor.selectedGroupIndex >= static_cast<int> (processor.groups.size()))
        return;
    const auto& g = processor.groups[static_cast<size_t> (processor.selectedGroupIndex)];
    groupNameEdit.setText (g.name, juce::dontSendNotification);
    rrSetBox.setSelectedId (g.rrSetId + 1, juce::dontSendNotification);
    keyswitchBox.setSelectedId (g.keyswitchNote < 0 ? 1 : (g.keyswitchNote + 2), juce::dontSendNotification);
}

void GroupEditorPanel::rebuildGroupGrid()
{
    groupTiles.clear (true);
    int numGroups = 0;
    {
        const juce::ScopedLock sl (processor.zoneLock);
        numGroups = static_cast<int> (processor.groups.size());
    }

    int viewW = std::max (500, gridViewport.getWidth() - 14);
    int cols = 5;
    int gap = 4;
    int cellW = (viewW - (cols + 1) * gap) / cols;
    int cellH = 44; // 5 rows * 48px = 240px visible height; Row 6+ scrolls cleanly!

    int numRows = std::max (5, (numGroups + cols - 1) / cols);
    gridContent.setSize (viewW, numRows * (cellH + gap) + gap);

    for (int i = 0; i < numGroups; ++i)
    {
        int row = i / cols;
        int col = i % cols;
        auto* tile = new GroupTile (processor, i,
            [this] (int selIdx)
            {
                {
                    const juce::ScopedLock sl (processor.zoneLock);
                    processor.selectedGroupIndex = selIdx;
                }
                syncStartBar();
                gridContent.repaint();
                if (onGroupChanged) onGroupChanged();
            },
            [this]()
            {
                syncStartBar();
                if (onGroupChanged) onGroupChanged();
            });
        gridContent.addAndMakeVisible (tile);
        tile->setBounds (gap + col * (cellW + gap), gap + row * (cellH + gap), cellW, cellH);
        groupTiles.add (tile);
    }

    syncStartBar();
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
    auto r = getLocalBounds().reduced (6);
    auto topBar = r.removeFromTop (26);
    titleLabel.setBounds (topBar.removeFromLeft (140));
    editAllGroupsBtn.setBounds (topBar.removeFromLeft (110).reduced (1));
    groupSoloBtn.setBounds (topBar.removeFromLeft (88).reduced (1));
    selectByMidiBtn.setBounds (topBar.removeFromLeft (106).reduced (1));
    deleteGroupBtn.setBounds (topBar.removeFromRight (64).reduced (2, 1));
    createGroupBtn.setBounds (topBar.removeFromRight (108).reduced (2, 1));

    // Bottom Group Start Options Bar
    auto startBar = r.removeFromBottom (54);
    startBarLabel.setBounds (startBar.removeFromTop (18));
    auto ctrlRow = startBar.removeFromTop (28);
    nameFieldLabel.setBounds (ctrlRow.removeFromLeft (44));
    groupNameEdit.setBounds (ctrlRow.removeFromLeft (160).reduced (2, 2));
    ctrlRow.removeFromLeft (12);
    rrSetLabel.setBounds (ctrlRow.removeFromLeft (60));
    rrSetBox.setBounds (ctrlRow.removeFromLeft (110).reduced (2, 2));
    ctrlRow.removeFromLeft (12);
    ksLabel.setBounds (ctrlRow.removeFromLeft (68));
    keyswitchBox.setBounds (ctrlRow.removeFromLeft (96).reduced (2, 2));

    r.removeFromTop (4);
    r.removeFromBottom (6);
    gridViewport.setBounds (r);
    rebuildGroupGrid();
}

// ---------------- 10-Octave KeyMapperComponent ----------------
KeyMapperComponent::KeyMapperComponent (NoiseMu6ixRhythmAudioProcessor& p) : processor (p) {}

bool KeyMapperComponent::isZoneVisible (const SampleZone& z) const
{
    if (readOnly) return true;
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
        if (isZoneVisible (processor.zones[static_cast<size_t> (processor.selectedZoneIndex)]))
            selectedZoneIndices.insert (processor.selectedZoneIndex);
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

    int count = static_cast<int> (selectedZoneIndices.size());
    if (count == 1)
    {
        auto& z = processor.zones[static_cast<size_t> (*selectedZoneIndices.begin())];
        z.lowVel = 0; z.highVel = 127;
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

void KeyMapperComponent::autoMapSelectedByFilename()
{
    const juce::ScopedLock sl (processor.zoneLock);
    sanitizeSelection();
    for (int idx : selectedZoneIndices)
    {
        auto& z = processor.zones[static_cast<size_t> (idx)];
        int parsed = NoiseMu6ixRhythmAudioProcessor::parseMidiNoteFromFilename (z.name);
        if (parsed >= 0 && parsed <= 120)
        {
            z.lowKey = parsed;
            z.highKey = parsed;
            z.rootKey = parsed;
        }
    }
    if (onSelectionChanged) onSelectionChanged();
    repaint();
}

void KeyMapperComponent::autoSpreadKeyRanges()
{
    const juce::ScopedLock sl (processor.zoneLock);
    sanitizeSelection();
    std::vector<int> list (selectedZoneIndices.begin(), selectedZoneIndices.end());
    if (list.size() < 2) return;

    std::sort (list.begin(), list.end(), [this] (int a, int b)
    {
        return processor.zones[static_cast<size_t> (a)].rootKey < processor.zones[static_cast<size_t> (b)].rootKey;
    });

    for (size_t i = 0; i < list.size(); ++i)
    {
        auto& z = processor.zones[static_cast<size_t> (list[i])];
        if (i == 0)
            z.lowKey = std::min (z.lowKey, z.rootKey);
        else
        {
            int prevRoot = processor.zones[static_cast<size_t> (list[i - 1])].rootKey;
            int mid = (prevRoot + z.rootKey) / 2;
            z.lowKey = juce::jlimit (0, z.rootKey, mid + 1);
            processor.zones[static_cast<size_t> (list[i - 1])].highKey = juce::jlimit (prevRoot, 120, mid);
        }
        if (i == list.size() - 1)
            z.highKey = std::max (z.highKey, z.rootKey);
    }
    if (onSelectionChanged) onSelectionChanged();
    repaint();
}

void KeyMapperComponent::moveSelectedZonesToGroup (int newGroupIdx)
{
    const juce::ScopedLock sl (processor.zoneLock);
    if (newGroupIdx < 0 || newGroupIdx >= static_cast<int> (processor.groups.size())) return;
    sanitizeSelection();
    for (int idx : selectedZoneIndices)
        processor.zones[static_cast<size_t> (idx)].groupIndex = newGroupIdx;
    if (onSelectionChanged) onSelectionChanged();
    repaint();
}

juce::Rectangle<int> KeyMapperComponent::getGridArea() const
{
    auto r = getLocalBounds();
    int leftPad = readOnly ? 0 : 28;
    r.removeFromLeft (leftPad);
    return r.removeFromTop (r.getHeight() - (readOnly ? 26 : 34));
}

juce::Rectangle<int> KeyMapperComponent::getKeyboardArea() const
{
    auto r = getLocalBounds();
    int leftPad = readOnly ? 0 : 28;
    r.removeFromLeft (leftPad);
    return r.removeFromBottom (readOnly ? 26 : 34);
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
        if ((startNote + i) % 12 == 0)
            g.drawVerticalLine (static_cast<int> (gridArea.getX() + i * keyW), static_cast<float> (gridArea.getY()), static_cast<float> (gridArea.getBottom()));

    const juce::ScopedLock sl (processor.zoneLock);
    sanitizeSelection();

    int visibleCount = 0;
    for (const auto& z : processor.zones)
        if (isZoneVisible (z)) ++visibleCount;

    if (visibleCount == 0)
    {
        g.setColour (juce::Colour (0xff78849b));
        g.setFont (11.5f);
        g.drawText (readOnly ? "Key Map Overview (Switch to EDIT tab to add or edit samples)"
                             : "Drag & Drop WAV/AIFF onto any key | Box-drag empty space to multi-select | Right-Click piano key for Root Key",
                    gridArea, juce::Justification::centred);
    }

    int selectedRootKey = -1;
    if (processor.selectedZoneIndex >= 0 && processor.selectedZoneIndex < static_cast<int> (processor.zones.size()))
        if (isZoneVisible (processor.zones[static_cast<size_t> (processor.selectedZoneIndex)]))
            selectedRootKey = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)].rootKey;

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
        {
            if (isZoneVisible (z) && note >= z.lowKey && note <= z.highKey)
            {
                mappedCol = z.colour;
                isMapped = true;
            }
        }

        if (note == pressedKey || note == activeNote)
            g.setColour (juce::Colours::white);
        else if (isKs)
            g.setColour (note == activeKs ? juce::Colour (0xffff3b3b) : juce::Colour (0xffb82525));
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
            g.setColour ((isMapped || isKs) ? juce::Colours::white : juce::Colours::darkgrey);
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
                }
                else processor.deleteZone (i);
                selectedZoneIndices.clear();
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
        int minDelta = -120, maxDelta = 120;
        for (const auto& s : dragSnapshots)
        {
            minDelta = std::max (minDelta, -s.lowKey);
            maxDelta = std::min (maxDelta, 120 - s.highKey);
        }
        int clampedDelta = juce::jlimit (minDelta, maxDelta, deltaKey);
        for (const auto& s : dragSnapshots)
        {
            auto& z = processor.zones[static_cast<size_t> (s.index)];
            z.lowKey = s.lowKey + clampedDelta;
            z.highKey = s.highKey + clampedDelta;
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
    dragMode = DragMode::None;
    dragZoneIdx = -1;
    dragSnapshots.clear();
    if (isMarqueeDragging) { isMarqueeDragging = false; repaint(); }
    if (pressedKey >= 0) { processor.uiNoteOff (pressedKey); pressedKey = -1; repaint(); }
}

// ---------------- WaveEditorComponent (Grid + Snap + Zoomable) ----------------
WaveEditorComponent::WaveEditorComponent (NoiseMu6ixRhythmAudioProcessor& p) : processor (p) {}

float WaveEditorComponent::applySnap (float normX, const SampleZone& z) const
{
    normX = juce::jlimit (0.0f, 1.0f, normX);
    if (snapMode == 1 && z.waveGridOn) // Grid Snap
    {
        int divs = (z.waveGridWidthIdx == 0) ? 4 : (z.waveGridWidthIdx == 1 ? 8 : (z.waveGridWidthIdx == 2 ? 16 : 32));
        float step = 1.0f / static_cast<float> (divs);
        return juce::jlimit (0.0f, 1.0f, std::round (normX / step) * step);
    }
    if (snapMode == 2 && ! z.layers.empty()) // Zero-Crossing Snap
    {
        int lIdx = juce::jlimit (0, static_cast<int> (z.layers.size()) - 1, z.selectedLayerIndex);
        const auto& buf = z.layers[static_cast<size_t> (lIdx)].buffer;
        int total = buf.getNumSamples();
        if (total > 16)
        {
            const float* d = buf.getReadPointer (0);
            int target = juce::jlimit (1, total - 2, static_cast<int> (normX * total));
            int best = target;
            for (int off = 0; off < 400; ++off)
            {
                if (target + off < total - 1 && (d[target + off] * d[target + off + 1] <= 0.0f)) { best = target + off; break; }
                if (target - off > 1 && (d[target - off] * d[target - off - 1] <= 0.0f)) { best = target - off; break; }
            }
            return static_cast<float> (best) / static_cast<float> (total);
        }
    }
    return normX;
}

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
        g.drawText ("Drop WAV/AIFF onto Mapping Editor, or drop here to add Round Robin layers", area, juce::Justification::centred);
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

    // Optional Kontakt-style Wave Grid overlay
    if (z.waveGridOn)
    {
        int divs = (z.waveGridWidthIdx == 0) ? 4 : (z.waveGridWidthIdx == 1 ? 8 : (z.waveGridWidthIdx == 2 ? 16 : 32));
        for (int i = 1; i < divs; ++i)
        {
            float gx = area.getX() + (i / static_cast<float> (divs)) * area.getWidth();
            g.setColour (i % 4 == 0 ? juce::Colour (0xff3a4a63) : juce::Colour (0xff222b38));
            g.drawVerticalLine (static_cast<int> (gx), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
        }
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

    float rawX = juce::jlimit (0.0f, 1.0f, e.x / static_cast<float> (std::max (1, getWidth())));
    float normX = applySnap (rawX, z);
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

void RhythmSequencerComponent::mouseUp (const juce::MouseEvent&) { draggedEventIdx = -1; repaint(); }

// ---------------- ZoneListModel (Kontakt List View) ----------------
void MainPluginView::ZoneListModel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    const juce::ScopedLock sl (proc.zoneLock);
    if (row < 0 || row >= static_cast<int> (proc.zones.size())) return;
    const auto& z = proc.zones[static_cast<size_t> (row)];
    g.fillAll (selected ? juce::Colour (0xff2b476b) : (row % 2 == 0 ? juce::Colour (0xff151a24) : juce::Colour (0xff1a202c)));
    g.setColour (z.colour);
    g.fillRect (2, 3, 5, h - 6);
    g.setColour (juce::Colours::white);
    g.setFont (11.0f);

    juce::String grpName = (z.groupIndex >= 0 && z.groupIndex < static_cast<int> (proc.groups.size()))
                         ? proc.groups[static_cast<size_t> (z.groupIndex)].name : "Group 1";
    juce::String rowText = z.name + "   |   " + grpName
        + "   |   Key: " + juce::MidiMessage::getMidiNoteName (z.lowKey, true, true, 5)
        + "-" + juce::MidiMessage::getMidiNoteName (z.highKey, true, true, 5)
        + "   |   Vel: " + juce::String (z.lowVel) + "-" + juce::String (z.highVel)
        + "   |   Root: " + juce::MidiMessage::getMidiNoteName (z.rootKey, true, true, 5);
    g.drawText (rowText, 12, 0, w - 16, h, juce::Justification::centredLeft, true);
}

// ---------------- MainPluginView ----------------
MainPluginView::MainPluginView (NoiseMu6ixRhythmAudioProcessor& p, std::function<void(float)> onScaleSelected)
    : audioProcessor (p), scaleCallback (std::move (onScaleSelected)),
      mainKeyMapper (p), seqComp (p), groupPanel (p), editKeyMapper (p),
      zoneListModel (p, [this] (int r)
      {
          {
              const juce::ScopedLock sl (audioProcessor.zoneLock);
              audioProcessor.selectedZoneIndex = r;
          }
          editKeyMapper.selectSingleZone (r);
          syncPerZoneControls();
      }),
      waveEditor (p), bottomPiano (p)
{
    addAndMakeVisible (titleScaleBtn);
    titleScaleBtn.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    titleScaleBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff273040));
    titleScaleBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    titleScaleBtn.onClick = [this]() { showScaleMenu(); };

    // Top-level Tabs: MAIN & EDIT
    addAndMakeVisible (tabMainBtn);
    addAndMakeVisible (tabEditBtn);
    tabMainBtn.onClick = [this]() { setActiveTab (ActiveTab::Main); };
    tabEditBtn.onClick = [this]() { setActiveTab (ActiveTab::Edit); };

    // EDIT Sub-Tabs: Group Editor | Mapping Editor | Wave Editor
    addAndMakeVisible (subGroupBtn);
    addAndMakeVisible (subMapBtn);
    addAndMakeVisible (subWaveBtn);
    subGroupBtn.onClick = [this]() { setEditSubTab (EditSubTab::Group); };
    subMapBtn.onClick   = [this]() { setEditSubTab (EditSubTab::Mapping); };
    subWaveBtn.onClick  = [this]() { setEditSubTab (EditSubTab::Wave); };

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
        if (row >= 0 && row < libraryFiles.size()) { libraryFiles.remove (row); libraryList.updateContent(); }
    };

    // Preset Name Box + SAVE / SAVE AS
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

    // ---------------- EDIT SUB-TAB 1: GROUP EDITOR ----------------
    addAndMakeVisible (groupPanel);
    groupPanel.onGroupChanged = [this]()
    {
        refreshMoveToGroupBox();
        editKeyMapper.repaint();
        bottomPiano.repaint();
        syncPerZoneControls();
    };

    // ---------------- EDIT SUB-TAB 2: MAPPING EDITOR ----------------
    addAndMakeVisible (frrBtn);
    addAndMakeVisible (fvlBtn);
    addAndMakeVisible (pitchTrackBtn);
    frrBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff5599));
    fvlBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff4ca3ff));
    pitchTrackBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffe8a138));
    frrAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "frr", frrBtn);
    fvlAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "fvl", fvlBtn);
    pitchTrackAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "pitchTrack", pitchTrackBtn);

    addAndMakeVisible (selectAllZonesBtn);
    selectAllZonesBtn.onClick = [this]() { editKeyMapper.selectAllZones(); };

    addAndMakeVisible (zoneSoloBtn);
    zoneSoloBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffe8a138));
    zoneSoloBtn.onClick = [this]() { audioProcessor.zoneSoloEnabled.store (zoneSoloBtn.getToggleState()); };

    addAndMakeVisible (selGroupsOnlyBtn);
    selGroupsOnlyBtn.setToggleState (true, juce::dontSendNotification);
    selGroupsOnlyBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff4cc98f));
    selGroupsOnlyBtn.onClick = [this]() { editKeyMapper.setSelectedGroupsOnly (selGroupsOnlyBtn.getToggleState()); };

    addAndMakeVisible (listViewBtn);
    listViewBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff5aa9ff));
    listViewBtn.onClick = [this]() { setEditSubTab (editSubTab); };

    addAndMakeVisible (autoGridBtn);
    autoGridBtn.onClick = [this]() { editKeyMapper.autoGridSelectedZones(); };

    addAndMakeVisible (autoMapBtn);
    autoMapBtn.setTooltip ("Auto-Map selected zones by note name in filename (e.g. Violin_C3.wav)");
    autoMapBtn.onClick = [this]() { editKeyMapper.autoMapSelectedByFilename(); };

    addAndMakeVisible (spreadBtn);
    spreadBtn.setTooltip ("Auto-Spread key ranges to fill gaps between selected zones");
    spreadBtn.onClick = [this]() { editKeyMapper.autoSpreadKeyRanges(); };

    addAndMakeVisible (dupZoneBtn);
    dupZoneBtn.setTooltip ("Duplicate selected zone");
    dupZoneBtn.onClick = [this]()
    {
        int newIdx = audioProcessor.duplicateZone (audioProcessor.selectedZoneIndex);
        if (newIdx >= 0) editKeyMapper.selectSingleZone (newIdx);
        syncPerZoneControls();
        repaintDynamicAreas();
    };

    addAndMakeVisible (moveToGroupBox);
    moveToGroupBox.setTextWhenNothingSelected ("Move->Grp");
    moveToGroupBox.onChange = [this]()
    {
        int id = moveToGroupBox.getSelectedId();
        if (id > 0)
        {
            editKeyMapper.moveSelectedZonesToGroup (id - 1);
            moveToGroupBox.setSelectedId (0, juce::dontSendNotification);
        }
    };

    addAndMakeVisible (mapZoomBox);
    mapZoomBox.addItem ("Zoom 100%", 1);
    mapZoomBox.addItem ("Zoom 150%", 2);
    mapZoomBox.addItem ("Zoom 200%", 3);
    mapZoomBox.setSelectedId (1, juce::dontSendNotification);
    mapZoomBox.onChange = [this]()
    {
        int id = mapZoomBox.getSelectedId();
        mapZoomFactor = (id == 2) ? 1.5f : (id == 3 ? 2.0f : 1.0f);
        resized();
    };

    addAndMakeVisible (velGridBox);
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

    addAndMakeVisible (zoneColourBox);
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
                    auto& z = audioProcessor.zones[static_cast<size_t> (idx)];
                    z.colourIndex = cIdx;
                    z.colour = NoiseMu6ixRhythmAudioProcessor::getPaletteColour (cIdx);
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

    // Editable Numeric Info Bar
    auto setupNumBox = [this] (juce::Label& l, const juce::String& txt, juce::TextEditor& ed)
    {
        l.setText (txt, juce::dontSendNotification);
        l.setFont (9.5f);
        l.setColour (juce::Label::textColourId, juce::Colour (0xff9aa8bf));
        addAndMakeVisible (l);
        ed.setMultiLine (false);
        ed.setJustification (juce::Justification::centred);
        ed.setInputRestrictions (4, "0123456789");
        ed.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff131822));
        ed.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff313c4f));
        ed.setColour (juce::TextEditor::textColourId, juce::Colours::white);
        ed.onReturnKey = [this]() { applyNumericZoneEdits(); };
        ed.onFocusLost = [this]() { applyNumericZoneEdits(); };
        addAndMakeVisible (ed);
    };
    setupNumBox (lkLbl, "LowK:", lkEdit);
    setupNumBox (hkLbl, "HighK:", hkEdit);
    setupNumBox (lvLbl, "LowV:", lvEdit);
    setupNumBox (hvLbl, "HighV:", hvEdit);
    setupNumBox (rkLbl, "Root:", rkEdit);

    editKeyMapper.setReadOnly (false);
    editKeyMapper.setSelectedGroupsOnly (true);
    editKeyMapper.setVelocityGridDivisions (4);
    editKeyMapper.onSelectionChanged = [this]() { syncPerZoneControls(); };

    addAndMakeVisible (mapViewport);
    mapViewport.setViewedComponent (&editKeyMapper, false);
    mapViewport.setScrollBarsShown (false, true);

    addAndMakeVisible (zoneListBox);
    zoneListBox.setModel (&zoneListModel);
    zoneListBox.setRowHeight (22);
    zoneListBox.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff141922));

    // ---------------- EDIT SUB-TAB 3: WAVE EDITOR ----------------
    addAndMakeVisible (trackSelectBox);
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

    addAndMakeVisible (realRRBtn);
    realRRBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff5aa9ff));
    realRRBtn.onClick = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].realRREnabled = realRRBtn.getToggleState();
    };

    addAndMakeVisible (waveZoomBox);
    waveZoomBox.addItem ("100%", 1);
    waveZoomBox.addItem ("200%", 2);
    waveZoomBox.addItem ("400%", 3);
    waveZoomBox.setSelectedId (1, juce::dontSendNotification);
    waveZoomBox.onChange = [this]()
    {
        int id = waveZoomBox.getSelectedId();
        waveZoomFactor = (id == 2) ? 2.0f : (id == 3 ? 4.0f : 1.0f);
        resized();
    };

    addAndMakeVisible (waveSnapBox);
    waveSnapBox.addItem ("Snap: Off", 1);
    waveSnapBox.addItem ("Snap: Grid", 2);
    waveSnapBox.addItem ("Snap: Zero-X", 3);
    waveSnapBox.setSelectedId (1, juce::dontSendNotification);
    waveSnapBox.onChange = [this]() { waveEditor.setSnapMode (waveSnapBox.getSelectedId() - 1); };

    addAndMakeVisible (waveInfoLabel);
    waveInfoLabel.setFont (10.0f);
    waveInfoLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9aa8bf));

    addAndMakeVisible (waveViewport);
    waveViewport.setViewedComponent (&waveEditor, false);
    waveViewport.setScrollBarsShown (false, true);
    waveEditor.onMarkersChanged = [this]() { syncPerZoneControls(); };

    // Wave Sub-Tab Buttons
    addAndMakeVisible (waveSubLoopBtn);
    addAndMakeVisible (waveSubGridBtn);
    addAndMakeVisible (waveSubToneBtn);
    waveSubLoopBtn.onClick = [this]() { setWaveSubTab (WaveSubTab::Loop); };
    waveSubGridBtn.onClick = [this]() { setWaveSubTab (WaveSubTab::GridSync); };
    waveSubToneBtn.onClick = [this]() { setWaveSubTab (WaveSubTab::EditTone); };

    // Wave Sub-Tab 1: Loop
    addAndMakeVisible (sampleLoopBtn);
    sampleLoopBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff4cc98f));
    sampleLoopBtn.onClick = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].sampleLoop = sampleLoopBtn.getToggleState();
        waveEditor.repaint();
    };

    addAndMakeVisible (loopModeBox);
    loopModeBox.addItem ("Forward", 1);
    loopModeBox.addItem ("Ping-Pong", 2);
    loopModeBox.addItem ("Until Release", 3);
    loopModeBox.setSelectedId (1, juce::dontSendNotification);
    loopModeBox.onChange = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].loopMode = loopModeBox.getSelectedId() - 1;
    };

    loopCountLbl.setText ("Count:", juce::dontSendNotification);
    loopCountLbl.setFont (10.0f);
    addAndMakeVisible (loopCountLbl);
    addAndMakeVisible (loopCountBox);
    loopCountBox.addItem ("Inf", 1);
    for (int c = 1; c <= 16; ++c) loopCountBox.addItem (juce::String (c), c + 1);
    loopCountBox.setSelectedId (1, juce::dontSendNotification);
    loopCountBox.onChange = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].loopCount = loopCountBox.getSelectedId() - 1;
    };

    // Wave Sub-Tab 2: Grid / Sync
    addAndMakeVisible (waveGridOnBtn);
    waveGridOnBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff5aa9ff));
    waveGridOnBtn.onClick = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].waveGridOn = waveGridOnBtn.getToggleState();
        waveEditor.repaint();
    };

    addAndMakeVisible (waveGridModeBox);
    waveGridModeBox.addItem ("Auto", 1);
    waveGridModeBox.addItem ("Fix", 2);
    waveGridModeBox.setSelectedId (1, juce::dontSendNotification);
    waveGridModeBox.onChange = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].waveGridAuto = (waveGridModeBox.getSelectedId() == 1);
    };

    addAndMakeVisible (waveGridWidthBox);
    waveGridWidthBox.addItem ("Width: 1/4", 1);
    waveGridWidthBox.addItem ("Width: 1/8", 2);
    waveGridWidthBox.addItem ("Width: 1/16", 3);
    waveGridWidthBox.addItem ("Width: 1/32", 4);
    waveGridWidthBox.setSelectedId (2, juce::dontSendNotification);
    waveGridWidthBox.onChange = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].waveGridWidthIdx = waveGridWidthBox.getSelectedId() - 1;
        waveEditor.repaint();
    };

    waveBpmLbl.setText ("Grid BPM:", juce::dontSendNotification);
    waveBpmLbl.setFont (10.0f);
    addAndMakeVisible (waveBpmLbl);
    addAndMakeVisible (waveBpmEdit);
    waveBpmEdit.setMultiLine (false);
    waveBpmEdit.setJustification (juce::Justification::centred);
    waveBpmEdit.setInputRestrictions (6, "0123456789.");
    waveBpmEdit.setText ("120.0", juce::dontSendNotification);
    waveBpmEdit.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff141922));
    waveBpmEdit.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff3c4960));
    waveBpmEdit.setColour (juce::TextEditor::textColourId, juce::Colours::white);

    // Wave Sub-Tab 3: Edit / Tone
    addAndMakeVisible (reverseBtn);
    reverseBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffe04f8a));
    reverseBtn.onClick = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].reverse = reverseBtn.getToggleState();
    };

    addAndMakeVisible (normalizeBtn);
    normalizeBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b3c54));
    normalizeBtn.onClick = [this]()
    {
        audioProcessor.normalizeSelectedSample();
        waveEditor.repaint();
    };

    auto bindZoneKnob = [this] (LabeledKnob& k, double minV, double maxV, double stepV, std::function<void(SampleZone&, float)> setter)
    {
        k.slider.setRange (minV, maxV, stepV);
        addAndMakeVisible (k);
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

    bindZoneKnob (loopXfadeKnob, 0.0, 0.4, 0.01,   [] (SampleZone& z, float v) { z.loopCrossfade = v; });
    bindZoneKnob (loopTuneKnob, -100.0, 100.0, 1.0,[] (SampleZone& z, float v) { z.loopTuneCents = v; });
    bindZoneKnob (fadeInKnob,    0.0, 0.25, 0.005, [] (SampleZone& z, float v) { z.fadeInNorm = v; });
    bindZoneKnob (fadeOutKnob,   0.0, 0.25, 0.005, [] (SampleZone& z, float v) { z.fadeOutNorm = v; });

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

    refreshMoveToGroupBox();
    setActiveTab (ActiveTab::Main);
    syncPerZoneControls();
}

void MainPluginView::refreshMoveToGroupBox()
{
    moveToGroupBox.clear (juce::dontSendNotification);
    const juce::ScopedLock sl (audioProcessor.zoneLock);
    for (int i = 0; i < static_cast<int> (audioProcessor.groups.size()); ++i)
        moveToGroupBox.addItem ("-> " + audioProcessor.groups[static_cast<size_t> (i)].name, i + 1);
}

void MainPluginView::applyNumericZoneEdits()
{
    const juce::ScopedLock sl (audioProcessor.zoneLock);
    if (audioProcessor.selectedZoneIndex < 0 || audioProcessor.selectedZoneIndex >= static_cast<int> (audioProcessor.zones.size()))
        return;
    auto& z = audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)];
    int lk = juce::jlimit (0, 120, lkEdit.getText().getIntValue());
    int hk = juce::jlimit (lk, 120, hkEdit.getText().getIntValue());
    int lv = juce::jlimit (0, 127, lvEdit.getText().getIntValue());
    int hv = juce::jlimit (lv, 127, hvEdit.getText().getIntValue());
    int rk = juce::jlimit (0, 120, rkEdit.getText().getIntValue());
    z.lowKey = lk; z.highKey = hk; z.lowVel = lv; z.highVel = hv; z.rootKey = rk;
    syncPerZoneControls();
    repaintDynamicAreas();
}

void MainPluginView::selectAllInEditTab()
{
    if (activeTab == ActiveTab::Edit && editSubTab == EditSubTab::Mapping)
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

    subGroupBtn.setVisible (isEdit);
    subMapBtn.setVisible (isEdit);
    subWaveBtn.setVisible (isEdit);

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

    setEditSubTab (editSubTab);
}

void MainPluginView::setEditSubTab (EditSubTab sub)
{
    editSubTab = sub;
    bool isEdit  = (activeTab == ActiveTab::Edit);
    bool showGrp = isEdit && (sub == EditSubTab::Group);
    bool showMap = isEdit && (sub == EditSubTab::Mapping);
    bool showWav = isEdit && (sub == EditSubTab::Wave);

    subGroupBtn.setColour (juce::TextButton::buttonColourId, sub == EditSubTab::Group   ? juce::Colour (0xff4ca3ff) : juce::Colour (0xff232b38));
    subMapBtn.setColour   (juce::TextButton::buttonColourId, sub == EditSubTab::Mapping ? juce::Colour (0xff4ca3ff) : juce::Colour (0xff232b38));
    subWaveBtn.setColour  (juce::TextButton::buttonColourId, sub == EditSubTab::Wave    ? juce::Colour (0xff4ca3ff) : juce::Colour (0xff232b38));

    groupPanel.setVisible (showGrp);
    if (showGrp) groupPanel.rebuildGroupGrid();

    bool isListMode = listViewBtn.getToggleState();
    frrBtn.setVisible (showMap);
    fvlBtn.setVisible (showMap);
    pitchTrackBtn.setVisible (showMap);
    selectAllZonesBtn.setVisible (showMap);
    zoneSoloBtn.setVisible (showMap);
    selGroupsOnlyBtn.setVisible (showMap);
    listViewBtn.setVisible (showMap);
    autoGridBtn.setVisible (showMap);
    autoMapBtn.setVisible (showMap);
    spreadBtn.setVisible (showMap);
    dupZoneBtn.setVisible (showMap);
    moveToGroupBox.setVisible (showMap);
    mapZoomBox.setVisible (showMap);
    velGridBox.setVisible (showMap);
    zoneColourBox.setVisible (showMap);
    lkLbl.setVisible (showMap); lkEdit.setVisible (showMap);
    hkLbl.setVisible (showMap); hkEdit.setVisible (showMap);
    lvLbl.setVisible (showMap); lvEdit.setVisible (showMap);
    hvLbl.setVisible (showMap); hvEdit.setVisible (showMap);
    rkLbl.setVisible (showMap); rkEdit.setVisible (showMap);
    mapViewport.setVisible (showMap && ! isListMode);
    zoneListBox.setVisible (showMap && isListMode);
    if (showMap && isListMode) zoneListBox.updateContent();

    trackSelectBox.setVisible (showWav);
    realRRBtn.setVisible (showWav);
    waveZoomBox.setVisible (showWav);
    waveSnapBox.setVisible (showWav);
    waveInfoLabel.setVisible (showWav);
    waveViewport.setVisible (showWav);
    waveSubLoopBtn.setVisible (showWav);
    waveSubGridBtn.setVisible (showWav);
    waveSubToneBtn.setVisible (showWav);

    setWaveSubTab (waveSubTab);
}

void MainPluginView::setWaveSubTab (WaveSubTab sub)
{
    waveSubTab = sub;
    bool showWav = (activeTab == ActiveTab::Edit && editSubTab == EditSubTab::Wave);
    bool sLoop = showWav && (sub == WaveSubTab::Loop);
    bool sGrid = showWav && (sub == WaveSubTab::GridSync);
    bool sTone = showWav && (sub == WaveSubTab::EditTone);

    waveSubLoopBtn.setColour (juce::TextButton::buttonColourId, sub == WaveSubTab::Loop     ? juce::Colour (0xff3c5878) : juce::Colour (0xff202733));
    waveSubGridBtn.setColour (juce::TextButton::buttonColourId, sub == WaveSubTab::GridSync ? juce::Colour (0xff3c5878) : juce::Colour (0xff202733));
    waveSubToneBtn.setColour (juce::TextButton::buttonColourId, sub == WaveSubTab::EditTone ? juce::Colour (0xff3c5878) : juce::Colour (0xff202733));

    sampleLoopBtn.setVisible (sLoop);
    loopModeBox.setVisible (sLoop);
    loopXfadeKnob.setVisible (sLoop);
    loopTuneKnob.setVisible (sLoop);
    loopCountLbl.setVisible (sLoop);
    loopCountBox.setVisible (sLoop);
    loopStartKnob.setVisible (sLoop);
    loopEndKnob.setVisible (sLoop);

    waveGridOnBtn.setVisible (sGrid);
    waveGridModeBox.setVisible (sGrid);
    waveGridWidthBox.setVisible (sGrid);
    waveBpmLbl.setVisible (sGrid);
    waveBpmEdit.setVisible (sGrid);

    reverseBtn.setVisible (sTone);
    normalizeBtn.setVisible (sTone);
    fadeInKnob.setVisible (sTone);
    fadeOutKnob.setVisible (sTone);
    LabeledKnob* toneKnobs[] = { &startKnob, &endKnob, &pitchKnob, &panKnob, &volKnob,
                                 &attackKnob, &decayKnob, &sustainKnob, &releaseKnob, &cutoffKnob, &resKnob };
    for (auto* k : toneKnobs) k->setVisible (sTone);

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
        lkEdit.setText ("-", juce::dontSendNotification);
        hkEdit.setText ("-", juce::dontSendNotification);
        lvEdit.setText ("-", juce::dontSendNotification);
        hvEdit.setText ("-", juce::dontSendNotification);
        rkEdit.setText ("-", juce::dontSendNotification);
        waveInfoLabel.setText ("Sample: (none)", juce::dontSendNotification);
        return;
    }

    const auto& z = audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)];
    for (int i = 0; i < static_cast<int> (z.layers.size()); ++i)
        trackSelectBox.addItem ("Track " + juce::String (i + 1) + ": " + z.layers[static_cast<size_t> (i)].name, i + 1);

    if (! z.layers.empty())
        trackSelectBox.setSelectedId (juce::jlimit (0, static_cast<int> (z.layers.size()) - 1, z.selectedLayerIndex) + 1, juce::dontSendNotification);

    lkEdit.setText (juce::String (z.lowKey), juce::dontSendNotification);
    hkEdit.setText (juce::String (z.highKey), juce::dontSendNotification);
    lvEdit.setText (juce::String (z.lowVel), juce::dontSendNotification);
    hvEdit.setText (juce::String (z.highVel), juce::dontSendNotification);
    rkEdit.setText (juce::String (z.rootKey), juce::dontSendNotification);

    int totalSamps = z.layers.empty() ? 0 : z.layers[static_cast<size_t> (juce::jlimit (0, static_cast<int> (z.layers.size()) - 1, z.selectedLayerIndex))].buffer.getNumSamples();
    waveInfoLabel.setText ("S.Start: " + juce::String (static_cast<int> (z.startNorm * totalSamps))
                         + "  |  S.End: " + juce::String (static_cast<int> (z.endNorm * totalSamps))
                         + "  |  Len: " + juce::String (totalSamps), juce::dontSendNotification);

    zoneColourBox.setSelectedId (z.colourIndex + 1, juce::dontSendNotification);
    realRRBtn.setToggleState (z.realRREnabled, juce::dontSendNotification);
    sampleLoopBtn.setToggleState (z.sampleLoop, juce::dontSendNotification);
    loopModeBox.setSelectedId (z.loopMode + 1, juce::dontSendNotification);
    loopXfadeKnob.slider.setValue (z.loopCrossfade, juce::dontSendNotification);
    loopTuneKnob.slider.setValue (z.loopTuneCents, juce::dontSendNotification);
    loopCountBox.setSelectedId (z.loopCount + 1, juce::dontSendNotification);

    waveGridOnBtn.setToggleState (z.waveGridOn, juce::dontSendNotification);
    waveGridModeBox.setSelectedId (z.waveGridAuto ? 1 : 2, juce::dontSendNotification);
    waveGridWidthBox.setSelectedId (z.waveGridWidthIdx + 1, juce::dontSendNotification);
    reverseBtn.setToggleState (z.reverse, juce::dontSendNotification);
    fadeInKnob.slider.setValue (z.fadeInNorm, juce::dontSendNotification);
    fadeOutKnob.slider.setValue (z.fadeOutNorm, juce::dontSendNotification);

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
        groupPanel.rebuildGroupGrid();
        refreshMoveToGroupBox();
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

    // Drop on Wave Editor -> adds Round Robin sample(s) to selected zone
    if (activeTab == ActiveTab::Edit && editSubTab == EditSubTab::Wave && waveViewport.getBounds().contains (localX, localY))
    {
        int targetZone = audioProcessor.selectedZoneIndex;
        for (const auto& path : files)
        {
            juce::File f (path);
            if (f.hasFileExtension (".mn6x"))
            {
                audioProcessor.loadPresetMn6x (f);
                presetNameBox.setText (audioProcessor.currentPresetName, juce::dontSendNotification);
                groupPanel.rebuildGroupGrid();
                refreshMoveToGroupBox();
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
        syncPerZoneControls();
        repaintDynamicAreas();
        return;
    }

    int targetKey = 60 + static_cast<int> (audioProcessor.zones.size());
    if (activeTab == ActiveTab::Edit && editSubTab == EditSubTab::Mapping && mapViewport.getBounds().contains (localX, localY))
    {
        auto mapLocal = editKeyMapper.getLocalPoint (this, juce::Point<int> (localX, localY));
        targetKey = juce::jlimit (0, 120, editKeyMapper.noteAtX (mapLocal.x));
    }
    targetKey = juce::jlimit (0, 120, targetKey);

    for (const auto& path : files)
    {
        juce::File f (path);
        if (f.hasFileExtension (".mn6x"))
        {
            audioProcessor.loadPresetMn6x (f);
            presetNameBox.setText (audioProcessor.currentPresetName, juce::dontSendNotification);
            groupPanel.rebuildGroupGrid();
            refreshMoveToGroupBox();
        }
        else if (activeTab == ActiveTab::Edit)
        {
            audioProcessor.loadSampleIntoNewZone (f, targetKey, targetKey, targetKey);
            targetKey = juce::jlimit (0, 120, targetKey + 1);
        }
    }
    editKeyMapper.selectSingleZone (audioProcessor.selectedZoneIndex);
    if (zoneListBox.isVisible()) zoneListBox.updateContent();
    syncPerZoneControls();
    repaintDynamicAreas();
}

void MainPluginView::repaintDynamicAreas()
{
    if (mainKeyMapper.isVisible()) mainKeyMapper.repaint();
    if (seqComp.isVisible()) seqComp.repaint();
    if (mapViewport.isVisible()) editKeyMapper.repaint();
    if (waveViewport.isVisible()) waveEditor.repaint();
    bottomPiano.repaint();
}

void MainPluginView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c222d));

    // Version v1.0.9 in bottom-right corner
    g.setColour (juce::Colour (0xff6e7b91));
    g.setFont (10.0f);
    g.drawText ("v1.0.9", getWidth() - 56, getHeight() - 15, 48, 13, juce::Justification::centredRight);

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
    presetNameBox.setBounds (topBar.removeFromLeft (320).reduced (4, 2));
    saveBtn.setBounds (topBar.removeFromLeft (72).reduced (2));
    saveAsBtn.setBounds (topBar.removeFromLeft (78).reduced (2));

    // Top Tab Bar: MAIN | EDIT  (and when EDIT active: Group Editor | Mapping Editor | Wave Editor)
    area.removeFromTop (4);
    auto tabBar = area.removeFromTop (24);
    tabMainBtn.setBounds (tabBar.removeFromLeft (96).reduced (2, 1));
    tabEditBtn.setBounds (tabBar.removeFromLeft (96).reduced (2, 1));
    tabBar.removeFromLeft (16);
    subGroupBtn.setBounds (tabBar.removeFromLeft (114).reduced (2, 1));
    subMapBtn.setBounds   (tabBar.removeFromLeft (118).reduced (2, 1));
    subWaveBtn.setBounds  (tabBar.removeFromLeft (110).reduced (2, 1));

    // Bottom 10-Octave Piano Strip
    auto pianoArea = area.removeFromBottom (34);
    bottomPiano.setBounds (pianoArea.reduced (0, 2));
    area.removeFromBottom (4);

    if (activeTab == ActiveTab::Main)
    {
        area.removeFromTop (18);
        mainKeyMapper.setBounds (area.removeFromTop (164));

        area.removeFromTop (22);
        seqComp.setBounds (area.removeFromTop (204));

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
    else if (editSubTab == EditSubTab::Group)
    {
        area.removeFromTop (6);
        groupPanel.setBounds (area);
    }
    else if (editSubTab == EditSubTab::Mapping)
    {
        area.removeFromTop (4);
        // Row 1: FRR, FVL, PITCH, ALL, Zone Solo, Sel Grp Only, List View
        auto r1 = area.removeFromTop (24);
        frrBtn.setBounds (r1.removeFromLeft (48).reduced (1));
        fvlBtn.setBounds (r1.removeFromLeft (48).reduced (1));
        pitchTrackBtn.setBounds (r1.removeFromLeft (60).reduced (1));
        selectAllZonesBtn.setBounds (r1.removeFromLeft (40).reduced (1));
        zoneSoloBtn.setBounds (r1.removeFromLeft (82).reduced (1));
        selGroupsOnlyBtn.setBounds (r1.removeFromLeft (96).reduced (1));
        listViewBtn.setBounds (r1.removeFromLeft (82).reduced (1));
        mapZoomBox.setBounds (r1.removeFromRight (94).reduced (1));
        zoneColourBox.setBounds (r1.removeFromRight (106).reduced (1));
        velGridBox.setBounds (r1.removeFromRight (84).reduced (1));

        // Row 2: Auto Grid, Auto-Map, Spread, Dup, Move->Grp + Editable Numeric Boxes
        area.removeFromTop (3);
        auto r2 = area.removeFromTop (24);
        autoGridBtn.setBounds (r2.removeFromLeft (66).reduced (1));
        autoMapBtn.setBounds  (r2.removeFromLeft (68).reduced (1));
        spreadBtn.setBounds   (r2.removeFromLeft (56).reduced (1));
        dupZoneBtn.setBounds  (r2.removeFromLeft (42).reduced (1));
        moveToGroupBox.setBounds (r2.removeFromLeft (94).reduced (1));
        r2.removeFromLeft (8);

        auto placeNum = [&r2] (juce::Label& l, juce::TextEditor& ed)
        {
            l.setBounds (r2.removeFromLeft (36));
            ed.setBounds (r2.removeFromLeft (34).reduced (1));
            r2.removeFromLeft (4);
        };
        placeNum (lkLbl, lkEdit);
        placeNum (hkLbl, hkEdit);
        placeNum (lvLbl, lvEdit);
        placeNum (hvLbl, hvEdit);
        placeNum (rkLbl, rkEdit);

        area.removeFromTop (4);
        mapViewport.setBounds (area);
        zoneListBox.setBounds (area);

        int kmW = static_cast<int> (std::round (std::max (100, area.getWidth()) * mapZoomFactor));
        int kmH = std::max (200, area.getHeight() - (mapZoomFactor > 1.01f ? 14 : 0));
        editKeyMapper.setSize (kmW, kmH);
    }
    else if (editSubTab == EditSubTab::Wave)
    {
        area.removeFromTop (4);
        // Top Wave Header Bar
        auto wTop = area.removeFromTop (24);
        trackSelectBox.setBounds (wTop.removeFromLeft (180).reduced (1));
        realRRBtn.setBounds (wTop.removeFromLeft (48).reduced (1));
        waveZoomBox.setBounds (wTop.removeFromLeft (72).reduced (1));
        waveSnapBox.setBounds (wTop.removeFromLeft (104).reduced (1));
        waveInfoLabel.setBounds (wTop.reduced (6, 0));

        // Large Zoomable Waveform Display
        area.removeFromTop (4);
        auto wDisp = area.removeFromTop (230);
        waveViewport.setBounds (wDisp);
        int wW = static_cast<int> (std::round (std::max (100, wDisp.getWidth()) * waveZoomFactor));
        int wH = std::max (160, wDisp.getHeight() - (waveZoomFactor > 1.01f ? 14 : 0));
        waveEditor.setSize (wW, wH);

        // Kontakt-style 3 Wave Sub-Tab Buttons below waveform
        area.removeFromTop (8);
        auto wTabs = area.removeFromTop (24);
        waveSubLoopBtn.setBounds (wTabs.removeFromLeft (110).reduced (2, 1));
        waveSubGridBtn.setBounds (wTabs.removeFromLeft (110).reduced (2, 1));
        waveSubToneBtn.setBounds (wTabs.removeFromLeft (110).reduced (2, 1));

        area.removeFromTop (6);
        auto subArea = area;
        if (waveSubTab == WaveSubTab::Loop)
        {
            auto topR = subArea.removeFromTop (28);
            sampleLoopBtn.setBounds (topR.removeFromLeft (86).reduced (2));
            loopModeBox.setBounds (topR.removeFromLeft (120).reduced (2));
            topR.removeFromLeft (10);
            loopCountLbl.setBounds (topR.removeFromLeft (46));
            loopCountBox.setBounds (topR.removeFromLeft (72).reduced (2));

            subArea.removeFromTop (6);
            auto kRow = subArea.removeFromTop (80);
            loopStartKnob.setBounds (kRow.removeFromLeft (86).reduced (2));
            loopEndKnob.setBounds   (kRow.removeFromLeft (86).reduced (2));
            loopXfadeKnob.setBounds (kRow.removeFromLeft (86).reduced (2));
            loopTuneKnob.setBounds  (kRow.removeFromLeft (86).reduced (2));
        }
        else if (waveSubTab == WaveSubTab::GridSync)
        {
            auto gRow = subArea.removeFromTop (30);
            waveGridOnBtn.setBounds (gRow.removeFromLeft (84).reduced (2));
            waveGridModeBox.setBounds (gRow.removeFromLeft (86).reduced (2));
            waveGridWidthBox.setBounds (gRow.removeFromLeft (114).reduced (2));
            gRow.removeFromLeft (12);
            waveBpmLbl.setBounds (gRow.removeFromLeft (64));
            waveBpmEdit.setBounds (gRow.removeFromLeft (68).reduced (2));
        }
        else if (waveSubTab == WaveSubTab::EditTone)
        {
            auto tRow = subArea.removeFromTop (26);
            reverseBtn.setBounds (tRow.removeFromLeft (64).reduced (2));
            normalizeBtn.setBounds (tRow.removeFromLeft (72).reduced (2));

            subArea.removeFromTop (4);
            auto kRow = subArea.removeFromTop (84);
            LabeledKnob* knobs[] = { &fadeInKnob, &fadeOutKnob, &startKnob, &endKnob, &pitchKnob, &panKnob, &volKnob,
                                     &attackKnob, &decayKnob, &sustainKnob, &releaseKnob, &cutoffKnob, &resKnob };
            int kw = kRow.getWidth() / 13;
            for (auto* k : knobs) k->setBounds (kRow.removeFromLeft (kw).reduced (1));
        }
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
