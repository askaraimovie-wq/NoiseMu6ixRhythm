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

// ---------------- 10-Octave KeyMapperComponent ----------------
KeyMapperComponent::KeyMapperComponent (NoiseMu6ixRhythmAudioProcessor& p) : processor (p) {}

juce::Rectangle<int> KeyMapperComponent::getGridArea() const
{
    auto r = getLocalBounds();
    return r.removeFromTop (r.getHeight() - 28);
}

juce::Rectangle<int> KeyMapperComponent::getKeyboardArea() const
{
    auto r = getLocalBounds();
    return r.removeFromBottom (28);
}

int KeyMapperComponent::noteAtX (int x) const
{
    int note = startNote + static_cast<int> ((x / static_cast<float> (std::max (1, getWidth()))) * numKeys);
    return juce::jlimit (startNote, startNote + numKeys - 1, note);
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

    g.setColour (juce::Colour (0xff222936));
    for (int i = 1; i < 4; ++i)
    {
        int y = gridArea.getY() + (gridArea.getHeight() * i) / 4;
        g.drawHorizontalLine (y, static_cast<float> (gridArea.getX()), static_cast<float> (gridArea.getRight()));
    }
    float keyW = gridArea.getWidth() / static_cast<float> (numKeys);
    for (int i = 0; i < numKeys; ++i)
    {
        if ((startNote + i) % 12 == 0)
            g.drawVerticalLine (static_cast<int> (gridArea.getX() + i * keyW), static_cast<float> (gridArea.getY()), static_cast<float> (gridArea.getBottom()));
    }

    const juce::ScopedLock sl (processor.zoneLock);

    if (processor.zones.empty())
    {
        g.setColour (juce::Colour (0xff78849b));
        g.setFont (11.0f);
        if (readOnly)
            g.drawText ("Key Map Overview (Switch to EDIT tab to add or edit samples)", gridArea, juce::Justification::centred);
        else
            g.drawText ("Double-click any key for Empty Track | Drag edges to resize | Right-click piano key (or toggle ROOT + Left-click) to set Root Key",
                        gridArea, juce::Justification::centred);
    }

    int selectedRootKey = -1;
    if (processor.selectedZoneIndex >= 0 && processor.selectedZoneIndex < static_cast<int> (processor.zones.size()))
        selectedRootKey = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)].rootKey;

    for (int i = 0; i < static_cast<int> (processor.zones.size()); ++i)
    {
        const auto& z = processor.zones[static_cast<size_t> (i)];
        auto r = getZoneRect (z, gridArea);
        g.setColour (z.colour.withAlpha (z.layers.empty() ? 0.35f : 0.72f));
        g.fillRect (r);
        g.setColour (i == processor.selectedZoneIndex ? juce::Colours::white : z.colour.brighter (0.3f));
        g.drawRect (r, i == processor.selectedZoneIndex ? 2.0f : 1.0f);

        juce::String label = z.name;
        if (z.layers.size() > 1) label += " (" + juce::String (z.layers.size()) + "RR)";
        if (! readOnly && i == processor.selectedZoneIndex)
            label += " [R:" + juce::MidiMessage::getMidiNoteName (z.rootKey, true, true, 5) + "]";

        g.setColour (juce::Colours::white);
        g.setFont (9.0f);
        g.drawText (label, r.reduced (2.0f, 1.0f), juce::Justification::topLeft, true);
    }

    // Draw 10-octave horizontal keyboard
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
            if (note >= z.lowKey && note <= z.highKey)
            {
                mappedCol = z.colour;
                isMapped = true;
            }
        }

        if (note == pressedKey || note == activeNote)
            g.setColour (juce::Colours::white);
        else if (! readOnly && note == selectedRootKey)
            g.setColour (juce::Colour (0xffffd32a)); // Bright gold highlight for selected zone's Root Key
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
            g.setFont (juce::FontOptions (8.0f, juce::Font::bold));
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

    // Piano strip interaction
    if (keyArea.contains (e.getPosition()))
    {
        int note = noteAtX (e.x);

        // In EDIT tab: Right-click OR (ROOT mode ON + Left-click) sets the Root Key of the selected zone!
        if (! readOnly && (e.mods.isRightButtonDown() || rootKeyMode))
        {
            const juce::ScopedLock sl (processor.zoneLock);
            if (processor.selectedZoneIndex >= 0 && processor.selectedZoneIndex < static_cast<int> (processor.zones.size()))
            {
                processor.zones[static_cast<size_t> (processor.selectedZoneIndex)].rootKey = note;
                if (onSelectionChanged) onSelectionChanged();
                repaint();
                return;
            }
        }

        pressedKey = note;
        processor.uiNoteOn (note, 0.9f);
        if (onSelectionChanged) onSelectionChanged();
        repaint();
        return;
    }

    // Clicking inside the zone grid
    const juce::ScopedLock sl (processor.zoneLock);
    dragMode = DragMode::None;
    dragZoneIdx = -1;

    for (int i = static_cast<int> (processor.zones.size()) - 1; i >= 0; --i)
    {
        auto r = getZoneRect (processor.zones[static_cast<size_t> (i)], gridArea);
        if (r.expanded (4.0f).contains (e.position))
        {
            // In MAIN tab (readOnly): allow selecting a zone to view its rhythm, NO editing/dragging/deleting!
            if (readOnly)
            {
                processor.selectedZoneIndex = i;
                if (onSelectionChanged) onSelectionChanged();
                repaint();
                return;
            }

            if (e.mods.isRightButtonDown())
            {
                processor.deleteZone (i);
                if (onSelectionChanged) onSelectionChanged();
                repaint();
                return;
            }

            processor.selectedZoneIndex = i;
            dragZoneIdx = i;
            origLowKey = processor.zones[static_cast<size_t> (i)].lowKey;
            origHighKey = processor.zones[static_cast<size_t> (i)].highKey;
            dragStartNoteOffset = noteAtX (e.x) - origLowKey;

            if (std::abs (e.position.x - r.getX()) <= 6.0f) dragMode = DragMode::LeftEdge;
            else if (std::abs (e.position.x - r.getRight()) <= 6.0f) dragMode = DragMode::RightEdge;
            else if (std::abs (e.position.y - r.getY()) <= 6.0f) dragMode = DragMode::TopEdge;
            else if (std::abs (e.position.y - r.getBottom()) <= 6.0f) dragMode = DragMode::BottomEdge;
            else dragMode = DragMode::Move;

            if (onSelectionChanged) onSelectionChanged();
            repaint();
            return;
        }
    }
}

void KeyMapperComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (readOnly) return;
    int note = noteAtX (e.x);
    if (onDoubleClickKey)
        onDoubleClickKey (note);
}

void KeyMapperComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (readOnly || dragMode == DragMode::None || dragZoneIdx < 0) return;

    auto gridArea = getGridArea();
    const juce::ScopedLock sl (processor.zoneLock);
    if (dragZoneIdx >= static_cast<int> (processor.zones.size())) return;

    auto& z = processor.zones[static_cast<size_t> (dragZoneIdx)];
    int curNote = juce::jlimit (0, 120, noteAtX (e.x));
    int curVel = juce::jlimit (0, 127, static_cast<int> ((1.0f - (e.y - gridArea.getY()) / static_cast<float> (std::max (1, gridArea.getHeight()))) * 127.0f));

    if (dragMode == DragMode::LeftEdge) z.lowKey = juce::jlimit (0, z.highKey, curNote);
    else if (dragMode == DragMode::RightEdge) z.highKey = juce::jlimit (z.lowKey, 120, curNote);
    else if (dragMode == DragMode::TopEdge) z.highVel = juce::jlimit (z.lowVel + 5, 127, curVel);
    else if (dragMode == DragMode::BottomEdge) z.lowVel = juce::jlimit (0, z.highVel - 5, curVel);
    else if (dragMode == DragMode::Move)
    {
        int span = origHighKey - origLowKey;
        int newLow = juce::jlimit (0, 120 - span, curNote - dragStartNoteOffset);
        z.lowKey = newLow;
        z.highKey = newLow + span;
    }
    repaint();
}

void KeyMapperComponent::mouseUp (const juce::MouseEvent&)
{
    dragMode = DragMode::None;
    dragZoneIdx = -1;
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
        g.setFont (11.5f);
        g.drawText ("Double-click a key above to create Empty Track, then Drag & Drop WAV(s) here", area, juce::Justification::centred);
        return;
    }

    const auto& z = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)];
    if (z.layers.empty())
    {
        g.setColour (z.colour.brighter (0.3f));
        g.setFont (11.5f);
        g.drawText ("Empty Track — Drop 1 or multiple Round Robin WAV files here!", area, juce::Justification::centred);
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
    float halfH = area.getHeight() * 0.42f;
    p.startNewSubPath (static_cast<float> (area.getX()), midY);

    int step = std::max (1, numSamples / std::max (1, area.getWidth()));
    for (int x = 0; x < area.getWidth(); ++x)
    {
        int idx = std::min (numSamples - 1, x * step);
        p.lineTo (static_cast<float> (area.getX() + x), midY - data[idx] * halfH);
    }

    g.setColour (z.colour.brighter (0.25f));
    g.strokePath (p, juce::PathStrokeType (1.2f));

    g.setColour (juce::Colour (0xffff5599));
    g.drawVerticalLine (static_cast<int> (startX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
    g.drawVerticalLine (static_cast<int> (endX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
    g.fillRect (startX, static_cast<float> (area.getY()), 13.0f, 12.0f);
    g.fillRect (endX - 13.0f, static_cast<float> (area.getY()), 13.0f, 12.0f);
    g.setColour (juce::Colours::white);
    g.setFont (9.0f);
    g.drawText ("S", static_cast<int> (startX), area.getY(), 13, 12, juce::Justification::centred);
    g.drawText ("E", static_cast<int> (endX - 13.0f), area.getY(), 13, 12, juce::Justification::centred);

    if (z.sampleLoop)
    {
        g.setColour (juce::Colour (0xff4cc98f));
        g.drawVerticalLine (static_cast<int> (lStartX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
        g.drawVerticalLine (static_cast<int> (lEndX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
        g.fillRect (lStartX, static_cast<float> (area.getBottom() - 12), 16.0f, 12.0f);
        g.fillRect (lEndX - 16.0f, static_cast<float> (area.getBottom() - 12), 16.0f, 12.0f);
        g.setColour (juce::Colours::black);
        g.drawText ("LS", static_cast<int> (lStartX), area.getBottom() - 12, 16, 12, juce::Justification::centred);
        g.drawText ("LE", static_cast<int> (lEndX - 16.0f), area.getBottom() - 12, 16, 12, juce::Justification::centred);
    }

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (10.5f);
    g.drawText ("Track " + juce::String (lIdx + 1) + "/" + juce::String (z.layers.size()) + ": " + layer.name
                + "  |  Root: " + juce::MidiMessage::getMidiNoteName (z.rootKey, true, true, 5),
                area.reduced (16, 3), juce::Justification::topRight, true);
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
    for (int i = 0; i < divs; ++i)
    {
        float x = area.getX() + (i / static_cast<float> (divs)) * area.getWidth();
        bool isBeat = (i % std::max (1, divs / 4) == 0);
        g.setColour (isBeat ? juce::Colour (0xff493959) : juce::Colour (0xff2b2135));
        g.drawVerticalLine (static_cast<int> (x), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
    }

    if (processor.apvts.getRawParameterValue ("seqLoop")->load() > 0.5f)
    {
        float px = area.getX() + processor.seqPlayheadNorm.load() * area.getWidth();
        g.setColour (juce::Colour (0x88ffffff));
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
      keyMapper (p), waveEditor (p), seqComp (p), bottomPiano (p)
{
    addAndMakeVisible (titleScaleBtn);
    titleScaleBtn.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    titleScaleBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff273040));
    titleScaleBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    titleScaleBtn.onClick = [this]() { showScaleMenu(); };

    // ONLY TWO Navigation Tabs: MAIN & EDIT
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

    // Empty Editable Preset Name Box (subtle dash placeholder when empty)
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

    // Key Mapper + Edit-Only Side Buttons (FRR / FVL / PITCH / ROOT)
    addAndMakeVisible (keyMapper);
    keyMapper.onDoubleClickKey = [this] (int key)
    {
        audioProcessor.createEmptyZone (key);
        syncPerZoneWaveControls();
        repaintDynamicAreas();
    };
    keyMapper.onSelectionChanged = [this]() { syncPerZoneWaveControls(); };

    addAndMakeVisible (frrBtn);
    addAndMakeVisible (fvlBtn);
    addAndMakeVisible (pitchTrackBtn);
    addAndMakeVisible (rootKeyBtn);
    frrBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff5599));
    fvlBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff4ca3ff));
    pitchTrackBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffe8a138));
    rootKeyBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffffd32a));
    rootKeyBtn.setTooltip ("Turn ON and Left-Click any piano key in Key Mapper (or Right-Click any key) to set Root Key");
    rootKeyBtn.onClick = [this]() { keyMapper.setRootKeyMode (rootKeyBtn.getToggleState()); };

    frrAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "frr", frrBtn);
    fvlAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "fvl", fvlBtn);
    pitchTrackAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "pitchTrack", pitchTrackBtn);

    // EDIT Tab Only: Per-Zone Sample Wave Editor
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

    addAndMakeVisible (sampleLoopBtn);
    sampleLoopBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff4cc98f));
    sampleLoopBtn.onClick = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].sampleLoop = sampleLoopBtn.getToggleState();
        waveEditor.repaint();
    };

    addAndMakeVisible (waveEditor);
    waveEditor.onMarkersChanged = [this]() { syncPerZoneWaveControls(); };

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

    // MAIN Tab Only: Rhythm Sequencer + Transport
    addAndMakeVisible (seqComp);
    addAndMakeVisible (seqLoopBtn);
    seqLoopBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffe84ea2));
    seqLoopAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "seqLoop", seqLoopBtn);

    addAndMakeVisible (playPauseBtn);
    playPauseBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff273549));
    playPauseBtn.onClick = [this]() { togglePlayPause(); };

    addAndMakeVisible (hostSyncBtn);
    hostSyncBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff4cc98f));
    hostSyncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "hostSync", hostSyncBtn);

    addAndMakeVisible (bpmKnob);
    bpmAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "internalBpm", bpmKnob.slider);

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
    gridBox.addItemList ({ "1/4", "1/8", "1/12 (T)", "1/16", "1/24 (T)" }, 1);
    gridAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (audioProcessor.apvts, "grid", gridBox);
    gridBox.onChange = [this]() { seqComp.repaint(); };

    addAndMakeVisible (bottomPiano);

    setActiveTab (ActiveTab::Main);
    syncPerZoneWaveControls();
}

void MainPluginView::setActiveTab (ActiveTab tab)
{
    activeTab = tab;
    bool isMain = (tab == ActiveTab::Main);
    bool isEdit = (tab == ActiveTab::Edit);

    tabMainBtn.setColour (juce::TextButton::buttonColourId, isMain ? juce::Colour (0xffc93b7b) : juce::Colour (0xff242c3a));
    tabEditBtn.setColour (juce::TextButton::buttonColourId, isEdit ? juce::Colour (0xffc93b7b) : juce::Colour (0xff242c3a));

    // Key Mapper is visible on both, but Read-Only on MAIN and Full-Edit on EDIT
    keyMapper.setReadOnly (isMain);

    // Side buttons ONLY on EDIT tab
    frrBtn.setVisible (isEdit);
    fvlBtn.setVisible (isEdit);
    pitchTrackBtn.setVisible (isEdit);
    rootKeyBtn.setVisible (isEdit);

    // Wave Editor ONLY on EDIT tab
    trackSelectBox.setVisible (isEdit);
    realRRBtn.setVisible (isEdit);
    sampleLoopBtn.setVisible (isEdit);
    waveEditor.setVisible (isEdit);
    LabeledKnob* waveKnobs[] = { &startKnob, &endKnob, &loopStartKnob, &loopEndKnob, &pitchKnob, &panKnob, &volKnob,
                                 &attackKnob, &decayKnob, &sustainKnob, &releaseKnob, &cutoffKnob, &resKnob };
    for (auto* k : waveKnobs) k->setVisible (isEdit);

    // Rhythm Sequencer ONLY on MAIN tab
    seqComp.setVisible (isMain);
    seqLoopBtn.setVisible (isMain);
    playPauseBtn.setVisible (isMain);
    hostSyncBtn.setVisible (isMain);
    bpmKnob.setVisible (isMain);
    shuffleKnob.setVisible (isMain);
    timeRandKnob.setVisible (isMain);
    velRandKnob.setVisible (isMain);
    gridLabel.setVisible (isMain);
    gridBox.setVisible (isMain);

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

void MainPluginView::syncPerZoneWaveControls()
{
    trackSelectBox.clear (juce::dontSendNotification);
    const juce::ScopedLock sl (audioProcessor.zoneLock);
    if (audioProcessor.selectedZoneIndex < 0 || audioProcessor.selectedZoneIndex >= static_cast<int> (audioProcessor.zones.size()))
    {
        realRRBtn.setToggleState (false, juce::dontSendNotification);
        sampleLoopBtn.setToggleState (false, juce::dontSendNotification);
        return;
    }

    const auto& z = audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)];
    for (int i = 0; i < static_cast<int> (z.layers.size()); ++i)
        trackSelectBox.addItem ("Track " + juce::String (i + 1) + ": " + z.layers[static_cast<size_t> (i)].name, i + 1);

    if (! z.layers.empty())
        trackSelectBox.setSelectedId (juce::jlimit (0, static_cast<int> (z.layers.size()) - 1, z.selectedLayerIndex) + 1, juce::dontSendNotification);

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
    }
    else
    {
        bool addedToSelected = false;
        {
            const juce::ScopedLock sl (audioProcessor.zoneLock);
            if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
                if (audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].layers.empty())
                    addedToSelected = true;
        }
        if (addedToSelected)
            audioProcessor.addSampleToZone (audioProcessor.selectedZoneIndex, f);
        else
        {
            int nextKey = juce::jlimit (0, 120, 60 + static_cast<int> (audioProcessor.zones.size()));
            audioProcessor.loadSampleIntoNewZone (f, nextKey, nextKey, nextKey);
        }
    }
    syncPerZoneWaveControls();
    repaintDynamicAreas();
}

void MainPluginView::handleFilesDropped (const juce::StringArray& files, int localX, int localY)
{
    // Always allow dropping .mn6x presets anywhere; WAV drops only edit when in EDIT tab
    bool hasPreset = false;
    for (const auto& path : files)
        if (juce::File (path).hasFileExtension (".mn6x")) hasPreset = true;

    if (activeTab == ActiveTab::Main && ! hasPreset)
        return;

    if (waveEditor.isVisible() && waveEditor.getBounds().contains (localX, localY))
    {
        int targetZone = audioProcessor.selectedZoneIndex;
        if (targetZone < 0 || targetZone >= static_cast<int> (audioProcessor.zones.size()))
            targetZone = audioProcessor.createEmptyZone (60);

        for (const auto& path : files)
        {
            juce::File f (path);
            if (f.hasFileExtension (".mn6x"))
            {
                audioProcessor.loadPresetMn6x (f);
                presetNameBox.setText (audioProcessor.currentPresetName, juce::dontSendNotification);
            }
            else audioProcessor.addSampleToZone (targetZone, f);
        }
        syncPerZoneWaveControls();
        repaintDynamicAreas();
        return;
    }

    int targetKey = 60 + static_cast<int> (audioProcessor.zones.size());
    if (keyMapper.isVisible() && keyMapper.getBounds().contains (localX, localY))
        targetKey = juce::jlimit (0, 120, keyMapper.noteAtX (localX - keyMapper.getX()));
    targetKey = juce::jlimit (0, 120, targetKey);

    for (const auto& path : files)
    {
        juce::File f (path);
        if (f.hasFileExtension (".mn6x"))
        {
            audioProcessor.loadPresetMn6x (f);
            presetNameBox.setText (audioProcessor.currentPresetName, juce::dontSendNotification);
        }
        else if (activeTab == ActiveTab::Edit)
        {
            audioProcessor.loadSampleIntoNewZone (f, targetKey, targetKey, targetKey);
            targetKey = juce::jlimit (0, 120, targetKey + 1);
        }
    }
    syncPerZoneWaveControls();
    repaintDynamicAreas();
}

void MainPluginView::repaintDynamicAreas()
{
    if (keyMapper.isVisible()) keyMapper.repaint();
    if (waveEditor.isVisible()) waveEditor.repaint();
    if (seqComp.isVisible()) seqComp.repaint();
    bottomPiano.repaint();
}

void MainPluginView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c222d));

    // Version v1.0.5 in bottom-right corner
    g.setColour (juce::Colour (0xff6e7b91));
    g.setFont (10.0f);
    g.drawText ("v1.0.5", getWidth() - 56, getHeight() - 15, 48, 13, juce::Justification::centredRight);

    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.setColour (juce::Colour (0xff9aa8bf));
    g.drawText ("LIBRARY", 10, 8, 100, 14, juce::Justification::centredLeft);

    if (activeTab == ActiveTab::Main)
    {
        g.drawText ("KEY MAP (VIEW ONLY)", 182, 68, 180, 14, juce::Justification::centredLeft);
        g.drawText ("RHYTHM SEQUENCER", 182, 260, 180, 14, juce::Justification::centredLeft);
    }
    else
    {
        g.drawText ("KEY MAP EDITOR", 182, 68, 180, 14, juce::Justification::centredLeft);
        g.drawText ("SAMPLE WAVE EDITOR", 182, 278, 180, 14, juce::Justification::centredLeft);
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

    // ONLY TWO Tab Buttons: MAIN & EDIT
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
        // MAIN TAB: Read-Only Key Mapper (no side buttons) + Spacious Rhythm Sequencer below
        area.removeFromTop (18);
        keyMapper.setBounds (area.removeFromTop (168));

        area.removeFromTop (22);
        auto seqRow = area.removeFromTop (190);
        auto seqSide = seqRow.removeFromRight (74);
        seqLoopBtn.setBounds (seqSide.removeFromTop (32).reduced (2));
        seqComp.setBounds (seqRow);

        area.removeFromTop (6);
        auto bottomCtrl = area.removeFromTop (60);
        playPauseBtn.setBounds (bottomCtrl.removeFromLeft (68).reduced (2, 10));
        hostSyncBtn.setBounds (bottomCtrl.removeFromLeft (64).reduced (2, 12));
        bpmKnob.setBounds (bottomCtrl.removeFromLeft (64).reduced (1));
        shuffleKnob.setBounds (bottomCtrl.removeFromLeft (66).reduced (1));
        timeRandKnob.setBounds (bottomCtrl.removeFromLeft (66).reduced (1));
        velRandKnob.setBounds (bottomCtrl.removeFromLeft (66).reduced (1));
        auto gridArea = bottomCtrl.removeFromLeft (110).reduced (4, 1);
        gridLabel.setBounds (gridArea.removeFromTop (13));
        gridBox.setBounds (gridArea.removeFromTop (26).reduced (0, 1));
    }
    else
    {
        // EDIT TAB: Full Key Mapper + Side Buttons (FRR, FVL, PITCH, ROOT) + Spacious Sample Wave Editor below
        area.removeFromTop (18);
        auto kmRow = area.removeFromTop (184);
        auto kmSide = kmRow.removeFromRight (76);
        frrBtn.setBounds (kmSide.removeFromTop (32).reduced (2));
        fvlBtn.setBounds (kmSide.removeFromTop (32).reduced (2));
        pitchTrackBtn.setBounds (kmSide.removeFromTop (32).reduced (2));
        rootKeyBtn.setBounds (kmSide.removeFromTop (32).reduced (2));
        keyMapper.setBounds (kmRow);

        area.removeFromTop (10);
        auto waveHeader = area.removeFromTop (24);
        sampleLoopBtn.setBounds (waveHeader.removeFromRight (66));
        realRRBtn.setBounds (waveHeader.removeFromRight (52));
        trackSelectBox.setBounds (waveHeader.removeFromRight (210).reduced (4, 1));
        waveEditor.setBounds (area.removeFromTop (132));

        area.removeFromTop (6);
        auto knobRow = area.removeFromTop (72);
        LabeledKnob* waveKnobs[] = { &startKnob, &endKnob, &loopStartKnob, &loopEndKnob, &pitchKnob, &panKnob, &volKnob,
                                     &attackKnob, &decayKnob, &sustainKnob, &releaseKnob, &cutoffKnob, &resKnob };
        int kw = knobRow.getWidth() / 13;
        for (auto* k : waveKnobs) k->setBounds (knobRow.removeFromLeft (kw).reduced (1));
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
