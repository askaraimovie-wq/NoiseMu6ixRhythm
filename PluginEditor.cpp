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
            int oct = note / 12; // C0 .. C10 (10 octaves)
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
    return r.removeFromTop (r.getHeight() - 26);
}

juce::Rectangle<int> KeyMapperComponent::getKeyboardArea() const
{
    auto r = getLocalBounds();
    return r.removeFromBottom (26);
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
        g.drawText ("10-Octave Key Mapper (C0-C10) | Double-click any key for Empty Track | Drag edges to resize | Right-click to delete",
                    gridArea, juce::Justification::centred);
    }

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
        g.setColour (juce::Colours::white);
        g.setFont (9.0f);
        g.drawText (label, r.reduced (2.0f, 1.0f), juce::Justification::topLeft, true);
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
            if (note >= z.lowKey && note <= z.highKey)
            {
                mappedCol = z.colour;
                isMapped = true;
            }
        }

        if (note == pressedKey || note == activeNote)
            g.setColour (juce::Colours::white);
        else if (isMapped)
            g.setColour (isBlack ? mappedCol.darker (0.35f) : mappedCol);
        else
            g.setColour (isBlack ? juce::Colour (0xff151921) : juce::Colour (0xffd8e0eb));

        g.fillRect (kr);
        g.setColour (juce::Colour (0xff272e3a));
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

void KeyMapperComponent::mouseDown (const juce::MouseEvent& e)
{
    auto gridArea = getGridArea();
    auto keyArea = getKeyboardArea();

    if (keyArea.contains (e.getPosition()))
    {
        int note = noteAtX (e.x);
        pressedKey = note;
        processor.uiNoteOn (note, 0.9f);
        if (onSelectionChanged) onSelectionChanged();
        repaint();
        return;
    }

    const juce::ScopedLock sl (processor.zoneLock);
    dragMode = DragMode::None;
    dragZoneIdx = -1;

    for (int i = static_cast<int> (processor.zones.size()) - 1; i >= 0; --i)
    {
        auto r = getZoneRect (processor.zones[static_cast<size_t> (i)], gridArea);
        if (r.expanded (4.0f).contains (e.position))
        {
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
    int note = noteAtX (e.x);
    if (onDoubleClickKey)
        onDoubleClickKey (note);
}

void KeyMapperComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode == DragMode::None || dragZoneIdx < 0) return;

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
        z.rootKey = newLow;
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

// ---------------- WaveEditorComponent (Per-Zone Independent Markers!) ----------------
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
    g.drawText ("Track " + juce::String (lIdx + 1) + "/" + juce::String (z.layers.size()) + ": " + layer.name,
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
        g.drawText ("Select a key zone above to edit its rhythm pattern | Turn ON LOOP & press PLAY or Spacebar", area, juce::Justification::centred);
        return;
    }

    const auto& z = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)];
    for (int i = 0; i < static_cast<int> (z.seqEvents.size()); ++i)
    {
        const auto& ev = z.seqEvents[static_cast<size_t> (i)];
        float cx = area.getX() + ev.position * area.getWidth();
        float barH = ev.velocity * (area.getHeight() - 16.0f);
        float topY = area.getBottom() - 8.0f - barH;

        g.setColour (i == draggedEventIdx ? juce::Colours::white : z.colour);
        g.drawLine (cx, static_cast<float> (area.getBottom() - 8), cx, topY, 3.0f);
        g.fillEllipse (cx - 4.5f, static_cast<float> (area.getBottom() - 12.5f), 9.0f, 9.0f);
        g.fillEllipse (cx - 3.5f, topY - 3.5f, 7.0f, 7.0f);
    }

    g.setColour (z.colour.brighter (0.4f));
    g.setFont (10.0f);
    g.drawText ("Pattern: " + z.name + "  |  Click=Add  Drag=Move/Vel  RightClick=Del",
                area.reduced (6, 2), juce::Justification::topRight, true);
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

// ---------------- GlobalEffectsView (8 Insert Slots + Parameter Controls) ----------------
GlobalEffectsView::GlobalEffectsView (NoiseMu6ixRhythmAudioProcessor& p) : processor (p)
{
    for (int i = 0; i < NoiseMu6ixRhythmAudioProcessor::numGlobalFxSlots; ++i)
    {
        auto& cb = slotBoxes[static_cast<size_t> (i)];
        addAndMakeVisible (cb);
        for (int t = 0; t <= 7; ++t)
            cb.addItem (InsertEffect::getTypeName (static_cast<FxType> (t)), t + 1);
        cb.setSelectedId (1, juce::dontSendNotification);
        cb.onChange = [this, i]()
        {
            const juce::ScopedLock sl (processor.zoneLock);
            auto& fx = processor.globalFx[static_cast<size_t> (i)];
            fx.type = static_cast<FxType> (slotBoxes[static_cast<size_t> (i)].getSelectedId() - 1);
            fx.prepare (processor.getCurrentSampleRate());
            selectSlot (i);
        };

        auto& bp = slotBypass[static_cast<size_t> (i)];
        bp.setButtonText ("ON");
        bp.setToggleState (true, juce::dontSendNotification);
        bp.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff4cc98f));
        addAndMakeVisible (bp);
        bp.onClick = [this, i]()
        {
            const juce::ScopedLock sl (processor.zoneLock);
            processor.globalFx[static_cast<size_t> (i)].enabled = slotBypass[static_cast<size_t> (i)].getToggleState();
        };

        auto& eb = slotEditBtns[static_cast<size_t> (i)];
        eb.setButtonText ("Slot " + juce::String (i + 1));
        addAndMakeVisible (eb);
        eb.onClick = [this, i]() { selectSlot (i); };
    }

    addAndMakeVisible (activeSlotLabel);
    activeSlotLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    activeSlotLabel.setColour (juce::Label::textColourId, juce::Colours::white);

    auto setupFxKnob = [this] (LabeledKnob& k, int pIdx)
    {
        k.slider.setRange (0.0, 1.0, 0.01);
        addAndMakeVisible (k);
        k.slider.onValueChange = [this, &k, pIdx]()
        {
            const juce::ScopedLock sl (processor.zoneLock);
            auto& fx = processor.globalFx[static_cast<size_t> (selectedSlot)];
            float val = static_cast<float> (k.slider.getValue());
            if (pIdx == 0) fx.p1 = val;
            else if (pIdx == 1) fx.p2 = val;
            else fx.p3 = val;
        };
    };
    setupFxKnob (p1Knob, 0);
    setupFxKnob (p2Knob, 1);
    setupFxKnob (p3Knob, 2);

    selectSlot (0);
}

void GlobalEffectsView::selectSlot (int idx)
{
    selectedSlot = juce::jlimit (0, NoiseMu6ixRhythmAudioProcessor::numGlobalFxSlots - 1, idx);
    for (int i = 0; i < NoiseMu6ixRhythmAudioProcessor::numGlobalFxSlots; ++i)
        slotEditBtns[static_cast<size_t> (i)].setColour (juce::TextButton::buttonColourId,
            i == selectedSlot ? juce::Colour (0xff3f5273) : juce::Colour (0xff232a36));

    const juce::ScopedLock sl (processor.zoneLock);
    const auto& fx = processor.globalFx[static_cast<size_t> (selectedSlot)];
    activeSlotLabel.setText ("GLOBAL MASTER SLOT " + juce::String (selectedSlot + 1) + " : " + InsertEffect::getTypeName (fx.type),
                             juce::dontSendNotification);
    p1Knob.label.setText (InsertEffect::getParamName (fx.type, 0), juce::dontSendNotification);
    p2Knob.label.setText (InsertEffect::getParamName (fx.type, 1), juce::dontSendNotification);
    p3Knob.label.setText (InsertEffect::getParamName (fx.type, 2), juce::dontSendNotification);
    p1Knob.slider.setValue (fx.p1, juce::dontSendNotification);
    p2Knob.slider.setValue (fx.p2, juce::dontSendNotification);
    p3Knob.slider.setValue (fx.p3, juce::dontSendNotification);
}

void GlobalEffectsView::refreshFromProcessor()
{
    const juce::ScopedLock sl (processor.zoneLock);
    for (int i = 0; i < NoiseMu6ixRhythmAudioProcessor::numGlobalFxSlots; ++i)
    {
        slotBoxes[static_cast<size_t> (i)].setSelectedId (static_cast<int> (processor.globalFx[static_cast<size_t> (i)].type) + 1, juce::dontSendNotification);
        slotBypass[static_cast<size_t> (i)].setToggleState (processor.globalFx[static_cast<size_t> (i)].enabled, juce::dontSendNotification);
    }
    selectSlot (selectedSlot);
}

void GlobalEffectsView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff171c26));
    g.setColour (juce::Colour (0xff2e3748));
    g.drawRect (getLocalBounds(), 1);
}

void GlobalEffectsView::resized()
{
    auto area = getLocalBounds().reduced (10);
    auto slotsArea = area.removeFromTop (240);
    int colW = slotsArea.getWidth() / 4;
    int rowH = slotsArea.getHeight() / 2;

    for (int i = 0; i < 8; ++i)
    {
        int col = i % 4;
        int row = i / 4;
        auto cell = juce::Rectangle<int> (slotsArea.getX() + col * colW, slotsArea.getY() + row * rowH, colW, rowH).reduced (5);
        auto topR = cell.removeFromTop (26);
        slotEditBtns[static_cast<size_t> (i)].setBounds (topR.removeFromLeft (cell.getWidth() - 50).reduced (2));
        slotBypass[static_cast<size_t> (i)].setBounds (topR.reduced (2));
        slotBoxes[static_cast<size_t> (i)].setBounds (cell.removeFromTop (30).reduced (2));
    }

    area.removeFromTop (8);
    activeSlotLabel.setBounds (area.removeFromTop (26));
    area.removeFromTop (6);
    auto knobRow = area.removeFromTop (96);
    p1Knob.setBounds (knobRow.removeFromLeft (110).reduced (4));
    p2Knob.setBounds (knobRow.removeFromLeft (110).reduced (4));
    p3Knob.setBounds (knobRow.removeFromLeft (110).reduced (4));
}

// ---------------- MixerView (Color Strips + Out 1..8 + Unlimited Per-Channel Insert FX) ----------------
MixerView::ChannelStrip::ChannelStrip (NoiseMu6ixRhythmAudioProcessor& p, int zIdx,
                                       std::function<void(int, int)> onEditFx,
                                       std::function<void()> onRebuild)
    : processor (p), zoneIndex (zIdx)
{
    const juce::ScopedLock sl (processor.zoneLock);
    const auto& z = processor.zones[static_cast<size_t> (zIdx)];
    stripColour = z.colour;

    addAndMakeVisible (nameLabel);
    nameLabel.setText (z.name, juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    nameLabel.setColour (juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible (outBusBox);
    for (int b = 0; b < NoiseMu6ixRhythmAudioProcessor::numOutputBuses; ++b)
        outBusBox.addItem ("Out " + juce::String (b + 1), b + 1);
    outBusBox.setSelectedId (z.outputBusIndex + 1, juce::dontSendNotification);
    outBusBox.onChange = [this]()
    {
        const juce::ScopedLock sl2 (processor.zoneLock);
        if (zoneIndex >= 0 && zoneIndex < static_cast<int> (processor.zones.size()))
            processor.zones[static_cast<size_t> (zoneIndex)].outputBusIndex = outBusBox.getSelectedId() - 1;
    };

    // Unlimited Insert FX buttons + "+ FX" button
    for (int f = 0; f < static_cast<int> (z.channelFx.size()); ++f)
    {
        auto* btn = new juce::TextButton (InsertEffect::getTypeName (z.channelFx[static_cast<size_t> (f)].type));
        btn->setColour (juce::TextButton::buttonColourId,
            z.channelFx[static_cast<size_t> (f)].enabled ? juce::Colour (0xff2c3c54) : juce::Colour (0xff1d222b));
        btn->onClick = [onEditFx, zIdx, f]() { if (onEditFx) onEditFx (zIdx, f); };
        addAndMakeVisible (btn);
        fxBtns.add (btn);
    }

    addAndMakeVisible (addFxBtn);
    addFxBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff354663));
    addFxBtn.onClick = [this, onEditFx, onRebuild]()
    {
        juce::PopupMenu m;
        for (int t = 1; t <= 7; ++t)
            m.addItem (t, InsertEffect::getTypeName (static_cast<FxType> (t)));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addFxBtn),
            [this, onEditFx, onRebuild] (int res)
            {
                if (res > 0)
                {
                    processor.addChannelFxToZone (zoneIndex, static_cast<FxType> (res));
                    int newSlot = 0;
                    {
                        const juce::ScopedLock sl2 (processor.zoneLock);
                        if (zoneIndex < static_cast<int> (processor.zones.size()))
                            newSlot = static_cast<int> (processor.zones[static_cast<size_t> (zoneIndex)].channelFx.size()) - 1;
                    }
                    if (onRebuild) onRebuild();
                    if (onEditFx) onEditFx (zoneIndex, newSlot);
                }
            });
    };

    // Pan knob
    panSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    panSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    panSlider.setRange (-1.0, 1.0, 0.01);
    panSlider.setValue (z.pan, juce::dontSendNotification);
    panSlider.setTooltip ("Channel Pan");
    addAndMakeVisible (panSlider);
    panSlider.onValueChange = [this]()
    {
        const juce::ScopedLock sl2 (processor.zoneLock);
        if (zoneIndex >= 0 && zoneIndex < static_cast<int> (processor.zones.size()))
            processor.zones[static_cast<size_t> (zoneIndex)].pan = static_cast<float> (panSlider.getValue());
    };

    // Volume fader
    volSlider.setSliderStyle (juce::Slider::LinearVertical);
    volSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 44, 14);
    volSlider.setRange (0.0, 2.0, 0.01);
    volSlider.setValue (z.volume, juce::dontSendNotification);
    addAndMakeVisible (volSlider);
    volSlider.onValueChange = [this]()
    {
        const juce::ScopedLock sl2 (processor.zoneLock);
        if (zoneIndex >= 0 && zoneIndex < static_cast<int> (processor.zones.size()))
            processor.zones[static_cast<size_t> (zoneIndex)].volume = static_cast<float> (volSlider.getValue());
    };

    // Mute & Solo
    addAndMakeVisible (muteBtn);
    addAndMakeVisible (soloBtn);
    muteBtn.setToggleState (z.muted, juce::dontSendNotification);
    soloBtn.setToggleState (z.soloed, juce::dontSendNotification);
    muteBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffee5253));
    soloBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffe8a138));
    muteBtn.onClick = [this]()
    {
        const juce::ScopedLock sl2 (processor.zoneLock);
        if (zoneIndex >= 0 && zoneIndex < static_cast<int> (processor.zones.size()))
            processor.zones[static_cast<size_t> (zoneIndex)].muted = muteBtn.getToggleState();
    };
    soloBtn.onClick = [this]()
    {
        const juce::ScopedLock sl2 (processor.zoneLock);
        if (zoneIndex >= 0 && zoneIndex < static_cast<int> (processor.zones.size()))
            processor.zones[static_cast<size_t> (zoneIndex)].soloed = soloBtn.getToggleState();
    };
}

void MixerView::ChannelStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.fillAll (juce::Colour (0xff191f2a));
    g.setColour (stripColour);
    g.fillRect (r.removeFromTop (5));
    g.setColour (juce::Colour (0xff2e3748));
    g.drawRect (getLocalBounds(), 1);
}

void MixerView::ChannelStrip::resized()
{
    auto r = getLocalBounds().reduced (4);
    r.removeFromTop (5);
    nameLabel.setBounds (r.removeFromTop (18));
    outBusBox.setBounds (r.removeFromTop (22).reduced (1));
    r.removeFromTop (3);

    int maxVisibleFx = std::min (4, fxBtns.size());
    for (int i = 0; i < fxBtns.size(); ++i)
    {
        if (i < maxVisibleFx)
            fxBtns[i]->setBounds (r.removeFromTop (20).reduced (1));
        else
            fxBtns[i]->setBounds (0, 0, 0, 0);
    }
    addFxBtn.setBounds (r.removeFromTop (20).reduced (1));

    auto msRow = r.removeFromBottom (22);
    muteBtn.setBounds (msRow.removeFromLeft (msRow.getWidth() / 2));
    soloBtn.setBounds (msRow);

    panSlider.setBounds (r.removeFromBottom (36).reduced (16, 0));
    volSlider.setBounds (r.reduced (6, 2));
}

MixerView::MixerView (NoiseMu6ixRhythmAudioProcessor& p) : processor (p)
{
    addAndMakeVisible (stripViewport);
    stripViewport.setViewedComponent (&stripContainer, false);
    stripViewport.setScrollBarsShown (false, true);

    addAndMakeVisible (fxInspectorLabel);
    fxInspectorLabel.setText ("Select a Channel FX Insert above to edit its parameters", juce::dontSendNotification);
    fxInspectorLabel.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    fxInspectorLabel.setColour (juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible (fxEnableBtn);
    fxEnableBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff4cc98f));
    fxEnableBtn.onClick = [this]()
    {
        const juce::ScopedLock sl (processor.zoneLock);
        if (selectedFxZone >= 0 && selectedFxZone < static_cast<int> (processor.zones.size()))
        {
            auto& list = processor.zones[static_cast<size_t> (selectedFxZone)].channelFx;
            if (selectedFxSlot >= 0 && selectedFxSlot < static_cast<int> (list.size()))
                list[static_cast<size_t> (selectedFxSlot)].enabled = fxEnableBtn.getToggleState();
        }
        rebuildStrips();
    };

    addAndMakeVisible (fxDeleteBtn);
    fxDeleteBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff782838));
    fxDeleteBtn.onClick = [this]()
    {
        processor.removeChannelFxFromZone (selectedFxZone, selectedFxSlot);
        selectedFxSlot = -1;
        fxInspectorLabel.setText ("FX Removed", juce::dontSendNotification);
        rebuildStrips();
    };

    auto setupInspKnob = [this] (LabeledKnob& k, int pIdx)
    {
        k.slider.setRange (0.0, 1.0, 0.01);
        addAndMakeVisible (k);
        k.slider.onValueChange = [this, &k, pIdx]()
        {
            const juce::ScopedLock sl (processor.zoneLock);
            if (selectedFxZone >= 0 && selectedFxZone < static_cast<int> (processor.zones.size()))
            {
                auto& list = processor.zones[static_cast<size_t> (selectedFxZone)].channelFx;
                if (selectedFxSlot >= 0 && selectedFxSlot < static_cast<int> (list.size()))
                {
                    float val = static_cast<float> (k.slider.getValue());
                    if (pIdx == 0) list[static_cast<size_t> (selectedFxSlot)].p1 = val;
                    else if (pIdx == 1) list[static_cast<size_t> (selectedFxSlot)].p2 = val;
                    else list[static_cast<size_t> (selectedFxSlot)].p3 = val;
                }
            }
        };
    };
    setupInspKnob (fxP1, 0);
    setupInspKnob (fxP2, 1);
    setupInspKnob (fxP3, 2);
}

void MixerView::selectChannelFx (int zIdx, int fxIdx)
{
    selectedFxZone = zIdx;
    selectedFxSlot = fxIdx;
    const juce::ScopedLock sl (processor.zoneLock);
    if (zIdx < 0 || zIdx >= static_cast<int> (processor.zones.size())) return;
    const auto& z = processor.zones[static_cast<size_t> (zIdx)];
    if (fxIdx < 0 || fxIdx >= static_cast<int> (z.channelFx.size())) return;

    const auto& fx = z.channelFx[static_cast<size_t> (fxIdx)];
    fxInspectorLabel.setText (z.name + "  ->  Insert " + juce::String (fxIdx + 1) + ": " + InsertEffect::getTypeName (fx.type),
                              juce::dontSendNotification);
    fxEnableBtn.setToggleState (fx.enabled, juce::dontSendNotification);
    fxP1.label.setText (InsertEffect::getParamName (fx.type, 0), juce::dontSendNotification);
    fxP2.label.setText (InsertEffect::getParamName (fx.type, 1), juce::dontSendNotification);
    fxP3.label.setText (InsertEffect::getParamName (fx.type, 2), juce::dontSendNotification);
    fxP1.slider.setValue (fx.p1, juce::dontSendNotification);
    fxP2.slider.setValue (fx.p2, juce::dontSendNotification);
    fxP3.slider.setValue (fx.p3, juce::dontSendNotification);
}

void MixerView::rebuildStrips()
{
    strips.clear (true);
    int numZones = 0;
    {
        const juce::ScopedLock sl (processor.zoneLock);
        numZones = static_cast<int> (processor.zones.size());
    }

    int stripW = 104;
    int h = std::max (260, stripViewport.getHeight() - 14);
    for (int i = 0; i < numZones; ++i)
    {
        auto* s = new ChannelStrip (processor, i,
            [this] (int z, int f) { selectChannelFx (z, f); },
            [this]() { rebuildStrips(); });
        stripContainer.addAndMakeVisible (s);
        s->setBounds (i * (stripW + 4), 0, stripW, h);
        strips.add (s);
    }
    stripContainer.setSize (std::max (stripViewport.getWidth(), numZones * (stripW + 4)), h);
    repaint();
}

void MixerView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff151a22));
    g.setColour (juce::Colour (0xff2e3748));
    g.drawRect (getLocalBounds(), 1);

    if (strips.isEmpty())
    {
        g.setColour (juce::Colour (0xff78849b));
        g.setFont (12.5f);
        g.drawText ("No Instruments Mapped Yet — Add samples in KEY MAP to see color-coded Mixer channels here!",
                    getLocalBounds().reduced (20), juce::Justification::centred);
    }
}

void MixerView::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto inspArea = r.removeFromBottom (88);
    stripViewport.setBounds (r);

    auto topInsp = inspArea.removeFromTop (22);
    fxDeleteBtn.setBounds (topInsp.removeFromRight (72).reduced (2));
    fxEnableBtn.setBounds (topInsp.removeFromRight (54).reduced (2));
    fxInspectorLabel.setBounds (topInsp);

    fxP1.setBounds (inspArea.removeFromLeft (90).reduced (2));
    fxP2.setBounds (inspArea.removeFromLeft (90).reduced (2));
    fxP3.setBounds (inspArea.removeFromLeft (90).reduced (2));
    rebuildStrips();
}

// ---------------- MainPluginView ----------------
MainPluginView::MainPluginView (NoiseMu6ixRhythmAudioProcessor& p, std::function<void(float)> onScaleSelected)
    : audioProcessor (p), scaleCallback (std::move (onScaleSelected)),
      keyMapper (p), waveEditor (p), seqComp (p), effectsView (p), mixerView (p), bottomPiano (p)
{
    addAndMakeVisible (titleScaleBtn);
    titleScaleBtn.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    titleScaleBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff273040));
    titleScaleBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    titleScaleBtn.onClick = [this]() { showScaleMenu(); };

    // Top Navigation Tabs
    auto setupTab = [this] (juce::TextButton& b, ActiveTab t)
    {
        addAndMakeVisible (b);
        b.onClick = [this, t]() { setActiveTab (t); };
    };
    setupTab (tabMainBtn, ActiveTab::Main);
    setupTab (tabKeyMapBtn, ActiveTab::KeyMap);
    setupTab (tabWaveBtn, ActiveTab::Wave);
    setupTab (tabRhythmBtn, ActiveTab::Rhythm);
    setupTab (tabEffectsBtn, ActiveTab::Effects);
    setupTab (tabMixerBtn, ActiveTab::Mixer);

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

    // Preset Name + SAVE / SAVE AS
    addAndMakeVisible (presetNameBox);
    presetNameBox.setText (audioProcessor.currentPresetName, juce::dontSendNotification);
    presetNameBox.setEditable (true, true, false);
    presetNameBox.setJustificationType (juce::Justification::centred);
    presetNameBox.setColour (juce::Label::backgroundColourId, juce::Colour (0xff161c26));
    presetNameBox.setColour (juce::Label::outlineColourId, juce::Colour (0xff3c4960));
    presetNameBox.setColour (juce::Label::textColourId, juce::Colours::white);
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

    // Key Mapper + FRR / FVL / PITCH
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
    frrBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff5599));
    fvlBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff4ca3ff));
    pitchTrackBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffe8a138));
    frrAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "frr", frrBtn);
    fvlAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "fvl", fvlBtn);
    pitchTrackAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "pitchTrack", pitchTrackBtn);

    // Per-Zone Sample Wave Editor Header & Knobs
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

    // Bind each Wave Editor knob to the SELECTED ZONE ONLY!
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

    // Rhythm Sequencer + Transport
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

    // Dedicated Effects & Mixer Tabs
    addAndMakeVisible (effectsView);
    addAndMakeVisible (mixerView);
    addAndMakeVisible (bottomPiano);

    setActiveTab (ActiveTab::Main);
    syncPerZoneWaveControls();
}

void MainPluginView::setActiveTab (ActiveTab tab)
{
    activeTab = tab;
    auto styleTab = [tab] (juce::TextButton& b, ActiveTab t)
    {
        b.setColour (juce::TextButton::buttonColourId, tab == t ? juce::Colour (0xffc93b7b) : juce::Colour (0xff242c3a));
    };
    styleTab (tabMainBtn, ActiveTab::Main);
    styleTab (tabKeyMapBtn, ActiveTab::KeyMap);
    styleTab (tabWaveBtn, ActiveTab::Wave);
    styleTab (tabRhythmBtn, ActiveTab::Rhythm);
    styleTab (tabEffectsBtn, ActiveTab::Effects);
    styleTab (tabMixerBtn, ActiveTab::Mixer);

    bool showKM   = (tab == ActiveTab::Main || tab == ActiveTab::KeyMap);
    bool showWave = (tab == ActiveTab::Main || tab == ActiveTab::Wave);
    bool showSeq  = (tab == ActiveTab::Main || tab == ActiveTab::Rhythm);
    bool showFx   = (tab == ActiveTab::Effects);
    bool showMix  = (tab == ActiveTab::Mixer);

    keyMapper.setVisible (showKM);
    frrBtn.setVisible (showKM);
    fvlBtn.setVisible (showKM);
    pitchTrackBtn.setVisible (showKM);

    trackSelectBox.setVisible (showWave);
    realRRBtn.setVisible (showWave);
    sampleLoopBtn.setVisible (showWave);
    waveEditor.setVisible (showWave);
    LabeledKnob* waveKnobs[] = { &startKnob, &endKnob, &loopStartKnob, &loopEndKnob, &pitchKnob, &panKnob, &volKnob,
                                 &attackKnob, &decayKnob, &sustainKnob, &releaseKnob, &cutoffKnob, &resKnob };
    for (auto* k : waveKnobs) k->setVisible (showWave);

    seqComp.setVisible (showSeq);
    seqLoopBtn.setVisible (showSeq);
    playPauseBtn.setVisible (showSeq);
    hostSyncBtn.setVisible (showSeq);
    bpmKnob.setVisible (showSeq);
    shuffleKnob.setVisible (showSeq);
    timeRandKnob.setVisible (showSeq);
    velRandKnob.setVisible (showSeq);
    gridLabel.setVisible (showSeq);
    gridBox.setVisible (showSeq);

    effectsView.setVisible (showFx);
    mixerView.setVisible (showMix);

    if (showFx) effectsView.refreshFromProcessor();
    if (showMix) mixerView.rebuildStrips();

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
    juce::String defaultName = audioProcessor.currentPresetName.isEmpty() ? "Rhythm Section 1" : audioProcessor.currentPresetName;
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
    if (activeTab == ActiveTab::Mixer) mixerView.rebuildStrips();
    repaintDynamicAreas();
}

void MainPluginView::handleFilesDropped (const juce::StringArray& files, int localX, int localY)
{
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
        else
        {
            audioProcessor.loadSampleIntoNewZone (f, targetKey, targetKey, targetKey);
            targetKey = juce::jlimit (0, 120, targetKey + 1);
        }
    }
    syncPerZoneWaveControls();
    if (activeTab == ActiveTab::Mixer) mixerView.rebuildStrips();
    repaintDynamicAreas();
}

void MainPluginView::repaintDynamicAreas()
{
    // CPU-Friendly: only repaint currently visible components!
    if (keyMapper.isVisible()) keyMapper.repaint();
    if (waveEditor.isVisible()) waveEditor.repaint();
    if (seqComp.isVisible()) seqComp.repaint();
    bottomPiano.repaint();
}

void MainPluginView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c222d));

    // Version v1.0.4 in bottom-right corner
    g.setColour (juce::Colour (0xff6e7b91));
    g.setFont (10.0f);
    g.drawText ("v1.0.4", getWidth() - 56, getHeight() - 15, 48, 13, juce::Justification::centredRight);

    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.setColour (juce::Colour (0xff9aa8bf));
    g.drawText ("LIBRARY", 10, 8, 100, 14, juce::Justification::centredLeft);
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

    // Top Header Row (Title + Preset + Save)
    auto topBar = area.removeFromTop (28);
    titleScaleBtn.setBounds (topBar.removeFromLeft (210).reduced (1));
    presetNameBox.setBounds (topBar.removeFromLeft (310).reduced (4, 2));
    saveBtn.setBounds (topBar.removeFromLeft (72).reduced (2));
    saveAsBtn.setBounds (topBar.removeFromLeft (78).reduced (2));

    // Top Navigation Tabs Row
    area.removeFromTop (4);
    auto tabBar = area.removeFromTop (24);
    int tw = 96;
    tabMainBtn.setBounds (tabBar.removeFromLeft (tw).reduced (2, 1));
    tabKeyMapBtn.setBounds (tabBar.removeFromLeft (tw).reduced (2, 1));
    tabWaveBtn.setBounds (tabBar.removeFromLeft (tw).reduced (2, 1));
    tabRhythmBtn.setBounds (tabBar.removeFromLeft (tw).reduced (2, 1));
    tabEffectsBtn.setBounds (tabBar.removeFromLeft (tw).reduced (2, 1));
    tabMixerBtn.setBounds (tabBar.removeFromLeft (tw).reduced (2, 1));

    // Bottom 10-Octave Piano Strip (always visible at bottom)
    auto pianoArea = area.removeFromBottom (34);
    bottomPiano.setBounds (pianoArea.reduced (0, 2));
    area.removeFromBottom (4);
    area.removeFromTop (6);

    LabeledKnob* waveKnobs[] = { &startKnob, &endKnob, &loopStartKnob, &loopEndKnob, &pitchKnob, &panKnob, &volKnob,
                                 &attackKnob, &decayKnob, &sustainKnob, &releaseKnob, &cutoffKnob, &resKnob };

    if (activeTab == ActiveTab::Main)
    {
        auto kmRow = area.removeFromTop (124);
        auto kmSide = kmRow.removeFromRight (70);
        frrBtn.setBounds (kmSide.removeFromTop (30).reduced (2));
        fvlBtn.setBounds (kmSide.removeFromTop (30).reduced (2));
        pitchTrackBtn.setBounds (kmSide.removeFromTop (30).reduced (2));
        keyMapper.setBounds (kmRow);

        area.removeFromTop (6);
        auto waveHeader = area.removeFromTop (22);
        sampleLoopBtn.setBounds (waveHeader.removeFromRight (64));
        realRRBtn.setBounds (waveHeader.removeFromRight (52));
        trackSelectBox.setBounds (waveHeader.removeFromRight (190).reduced (4, 1));
        waveEditor.setBounds (area.removeFromTop (84));

        area.removeFromTop (3);
        auto knobRow = area.removeFromTop (62);
        int kw = knobRow.getWidth() / 13;
        for (auto* k : waveKnobs) k->setBounds (knobRow.removeFromLeft (kw).reduced (1));

        area.removeFromTop (6);
        auto seqRow = area.removeFromTop (84);
        auto seqSide = seqRow.removeFromRight (70);
        seqLoopBtn.setBounds (seqSide.removeFromTop (30).reduced (2));
        seqComp.setBounds (seqRow);

        area.removeFromTop (4);
        auto bottomCtrl = area.removeFromTop (56);
        playPauseBtn.setBounds (bottomCtrl.removeFromLeft (64).reduced (2, 10));
        hostSyncBtn.setBounds (bottomCtrl.removeFromLeft (62).reduced (2, 12));
        bpmKnob.setBounds (bottomCtrl.removeFromLeft (60).reduced (1));
        shuffleKnob.setBounds (bottomCtrl.removeFromLeft (62).reduced (1));
        timeRandKnob.setBounds (bottomCtrl.removeFromLeft (62).reduced (1));
        velRandKnob.setBounds (bottomCtrl.removeFromLeft (62).reduced (1));
        auto gridArea = bottomCtrl.removeFromLeft (104).reduced (4, 1);
        gridLabel.setBounds (gridArea.removeFromTop (13));
        gridBox.setBounds (gridArea.removeFromTop (24).reduced (0, 1));
    }
    else if (activeTab == ActiveTab::KeyMap)
    {
        auto kmSide = area.removeFromRight (76);
        frrBtn.setBounds (kmSide.removeFromTop (34).reduced (2));
        fvlBtn.setBounds (kmSide.removeFromTop (34).reduced (2));
        pitchTrackBtn.setBounds (kmSide.removeFromTop (34).reduced (2));
        keyMapper.setBounds (area);
    }
    else if (activeTab == ActiveTab::Wave)
    {
        auto waveHeader = area.removeFromTop (26);
        sampleLoopBtn.setBounds (waveHeader.removeFromRight (70));
        realRRBtn.setBounds (waveHeader.removeFromRight (56));
        trackSelectBox.setBounds (waveHeader.removeFromRight (240).reduced (4, 1));
        auto knobRow = area.removeFromBottom (88);
        int kw = knobRow.getWidth() / 13;
        for (auto* k : waveKnobs) k->setBounds (knobRow.removeFromLeft (kw).reduced (1));
        area.removeFromBottom (6);
        waveEditor.setBounds (area);
    }
    else if (activeTab == ActiveTab::Rhythm)
    {
        auto bottomCtrl = area.removeFromBottom (68);
        playPauseBtn.setBounds (bottomCtrl.removeFromLeft (72).reduced (2, 14));
        hostSyncBtn.setBounds (bottomCtrl.removeFromLeft (68).reduced (2, 16));
        bpmKnob.setBounds (bottomCtrl.removeFromLeft (72).reduced (2));
        shuffleKnob.setBounds (bottomCtrl.removeFromLeft (72).reduced (2));
        timeRandKnob.setBounds (bottomCtrl.removeFromLeft (72).reduced (2));
        velRandKnob.setBounds (bottomCtrl.removeFromLeft (72).reduced (2));
        auto gridArea = bottomCtrl.removeFromLeft (115).reduced (4, 4);
        gridLabel.setBounds (gridArea.removeFromTop (14));
        gridBox.setBounds (gridArea.removeFromTop (26).reduced (0, 1));
        area.removeFromBottom (6);
        auto seqSide = area.removeFromRight (74);
        seqLoopBtn.setBounds (seqSide.removeFromTop (34).reduced (2));
        seqComp.setBounds (area);
    }
    else if (activeTab == ActiveTab::Effects)
    {
        effectsView.setBounds (area);
    }
    else if (activeTab == ActiveTab::Mixer)
    {
        mixerView.setBounds (area);
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
    startTimerHz (20); // CPU-friendly 20Hz refresh
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
