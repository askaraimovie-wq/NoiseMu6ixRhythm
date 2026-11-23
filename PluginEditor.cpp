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
    label.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, juce::Colour (0xffc9d4e8));
    addAndMakeVisible (label);
}

void LabeledKnob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (13));
    slider.setBounds (r);
}

// ---------------- ColorPianoStrip (Bottom Keyboard with Black ALL Key) ----------------
ColorPianoStrip::ColorPianoStrip (NoiseMu6ixRhythmAudioProcessor& p) : processor (p) {}

void ColorPianoStrip::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll (juce::Colour (0xff12161e));
    float keyW = area.getWidth() / static_cast<float> (numKeys);
    int activeNote = processor.lastPlayedNote.load();
    bool allOn = processor.masterAllHeld.load();

    const juce::ScopedLock sl (processor.zoneLock);

    for (int i = 0; i < numKeys; ++i)
    {
        int note = startNote + i;
        juce::Rectangle<float> kr (area.getX() + i * keyW, static_cast<float> (area.getY()), keyW, static_cast<float> (area.getHeight()));

        // Master ALL Trigger Key (Black key at index 0, MIDI 35)
        if (note == NoiseMu6ixRhythmAudioProcessor::masterAllKey)
        {
            g.setColour ((pressedNote == note || allOn) ? juce::Colour (0xffe8a138) : juce::Colour (0xff0a0c10));
            g.fillRect (kr);
            g.setColour (juce::Colour (0xffe8a138));
            g.drawRect (kr, 1.2f);
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
            g.drawText ("ALL", kr, juce::Justification::centred, false);
            continue;
        }

        int n = note % 12;
        bool isBlack = (n == 1 || n == 3 || n == 6 || n == 8 || n == 10);

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
        g.drawRect (kr, 0.8f);

        if (n == 0)
        {
            int oct = (note / 12) - 2;
            g.setColour (isMapped ? juce::Colours::white : juce::Colours::darkgrey);
            g.setFont (8.5f);
            g.drawText ("C" + juce::String (oct), kr.reduced (1.0f), juce::Justification::centredBottom, false);
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

// ---------------- KeyMapperComponent ----------------
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
    return { x1, y1, std::max (10.0f, x2 - x1), std::max (10.0f, y2 - y1) };
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
        g.setFont (11.5f);
        g.drawText ("Double-click or Drop WAV on any key | Drag edges to resize | Right-click to delete",
                    gridArea, juce::Justification::centred);
    }

    for (int i = 0; i < static_cast<int> (processor.zones.size()); ++i)
    {
        const auto& z = processor.zones[static_cast<size_t> (i)];
        auto r = getZoneRect (z, gridArea);
        g.setColour (z.colour.withAlpha (0.65f));
        g.fillRect (r);
        g.setColour (i == processor.selectedZoneIndex ? juce::Colours::white : z.colour.brighter (0.3f));
        g.drawRect (r, i == processor.selectedZoneIndex ? 2.0f : 1.0f);

        if (i == processor.selectedZoneIndex)
        {
            g.setColour (juce::Colours::white);
            g.fillRect (r.getX(), r.getCentreY() - 4.0f, 3.0f, 8.0f);
            g.fillRect (r.getRight() - 3.0f, r.getCentreY() - 4.0f, 3.0f, 8.0f);
        }

        g.setColour (juce::Colours::white);
        g.setFont (9.5f);
        g.drawText (z.name, r.reduced (3.0f, 1.0f), juce::Justification::topLeft, true);
    }

    // Color-coded horizontal piano strip + Black ALL key at far left
    int activeNote = processor.lastPlayedNote.load();
    bool allOn = processor.masterAllHeld.load();
    for (int i = 0; i < numKeys; ++i)
    {
        int note = startNote + i;
        juce::Rectangle<float> kr (keyArea.getX() + i * keyW, static_cast<float> (keyArea.getY()), keyW, static_cast<float> (keyArea.getHeight()));

        if (note == NoiseMu6ixRhythmAudioProcessor::masterAllKey)
        {
            g.setColour ((pressedKey == note || allOn) ? juce::Colour (0xffe8a138) : juce::Colour (0xff0a0c10));
            g.fillRect (kr);
            g.setColour (juce::Colour (0xffe8a138));
            g.drawRect (kr, 1.2f);
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions (8.0f, juce::Font::bold));
            g.drawText ("ALL", kr, juce::Justification::centred, false);
            continue;
        }

        int n = note % 12;
        bool isBlack = (n == 1 || n == 3 || n == 6 || n == 8 || n == 10);

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
        g.drawRect (kr, 0.8f);

        if (n == 0)
        {
            int oct = (note / 12) - 2;
            g.setColour (isMapped ? juce::Colours::white : juce::Colours::darkgrey);
            g.setFont (8.5f);
            g.drawText ("C" + juce::String (oct), kr.reduced (1.0f), juce::Justification::centredBottom, false);
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
        repaint();
        return;
    }

    const juce::ScopedLock sl (processor.zoneLock);
    dragMode = DragMode::None;
    dragZoneIdx = -1;

    for (int i = static_cast<int> (processor.zones.size()) - 1; i >= 0; --i)
    {
        auto r = getZoneRect (processor.zones[static_cast<size_t> (i)], gridArea);
        if (r.expanded (3.0f).contains (e.position))
        {
            if (e.mods.isRightButtonDown())
            {
                processor.deleteZone (i);
                repaint();
                return;
            }

            processor.selectedZoneIndex = i;
            dragZoneIdx = i;
            origLowKey = processor.zones[static_cast<size_t> (i)].lowKey;
            origHighKey = processor.zones[static_cast<size_t> (i)].highKey;
            dragStartNoteOffset = noteAtX (e.x) - origLowKey;

            if (std::abs (e.position.x - r.getX()) <= 6.0f)
                dragMode = DragMode::LeftEdge;
            else if (std::abs (e.position.x - r.getRight()) <= 6.0f)
                dragMode = DragMode::RightEdge;
            else if (std::abs (e.position.y - r.getY()) <= 6.0f)
                dragMode = DragMode::TopEdge;
            else if (std::abs (e.position.y - r.getBottom()) <= 6.0f)
                dragMode = DragMode::BottomEdge;
            else
                dragMode = DragMode::Move;

            repaint();
            return;
        }
    }
}

void KeyMapperComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    int note = noteAtX (e.x);
    if (note == NoiseMu6ixRhythmAudioProcessor::masterAllKey)
        return;
    if (onDoubleClickKey)
        onDoubleClickKey (note);
}

void KeyMapperComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode == DragMode::None || dragZoneIdx < 0)
        return;

    auto gridArea = getGridArea();
    const juce::ScopedLock sl (processor.zoneLock);
    if (dragZoneIdx >= static_cast<int> (processor.zones.size()))
        return;

    auto& z = processor.zones[static_cast<size_t> (dragZoneIdx)];
    int curNote = juce::jlimit (36, 84, noteAtX (e.x));
    int curVel = juce::jlimit (0, 127, static_cast<int> ((1.0f - (e.y - gridArea.getY()) / static_cast<float> (std::max (1, gridArea.getHeight()))) * 127.0f));

    if (dragMode == DragMode::LeftEdge)
        z.lowKey = juce::jlimit (36, z.highKey, curNote);
    else if (dragMode == DragMode::RightEdge)
        z.highKey = juce::jlimit (z.lowKey, 84, curNote);
    else if (dragMode == DragMode::TopEdge)
        z.highVel = juce::jlimit (z.lowVel + 5, 127, curVel);
    else if (dragMode == DragMode::BottomEdge)
        z.lowVel = juce::jlimit (0, z.highVel - 5, curVel);
    else if (dragMode == DragMode::Move)
    {
        int span = origHighKey - origLowKey;
        int newLow = juce::jlimit (36, 84 - span, curNote - dragStartNoteOffset);
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
        g.drawText ("Drop WAV/AIFF Sample Here, Double-Click Key Mapper, or Double-Click Library", area, juce::Justification::centred);
        return;
    }

    const auto& z = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)];
    const float* data = z.buffer.getReadPointer (0);
    int numSamples = z.buffer.getNumSamples();
    if (numSamples <= 0) return;

    float startNorm = processor.apvts.getRawParameterValue ("start")->load();
    float endNorm = processor.apvts.getRawParameterValue ("end")->load();
    float lStartNorm = processor.apvts.getRawParameterValue ("loopStart")->load();
    float lEndNorm = processor.apvts.getRawParameterValue ("loopEnd")->load();
    bool loopOn = processor.apvts.getRawParameterValue ("sampleLoop")->load() > 0.5f;

    float startX = area.getX() + startNorm * area.getWidth();
    float endX = area.getX() + endNorm * area.getWidth();
    float lStartX = area.getX() + lStartNorm * area.getWidth();
    float lEndX = area.getX() + lEndNorm * area.getWidth();

    g.setColour (juce::Colour (0x55000000));
    g.fillRect (static_cast<float> (area.getX()), static_cast<float> (area.getY()), startX - area.getX(), static_cast<float> (area.getHeight()));
    g.fillRect (endX, static_cast<float> (area.getY()), area.getRight() - endX, static_cast<float> (area.getHeight()));

    if (loopOn && lEndX > lStartX)
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
        float v = data[idx];
        p.lineTo (static_cast<float> (area.getX() + x), midY - v * halfH);
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

    if (loopOn)
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
    g.drawText (z.name + "  [" + juce::MidiMessage::getMidiNoteName (z.lowKey, true, true, 3) + "-"
                + juce::MidiMessage::getMidiNoteName (z.highKey, true, true, 3) + "]",
                area.reduced (16, 3), juce::Justification::topRight, true);
}

void WaveEditorComponent::mouseDown (const juce::MouseEvent& e)
{
    float normX = juce::jlimit (0.0f, 1.0f, e.x / static_cast<float> (std::max (1, getWidth())));
    float s = processor.apvts.getRawParameterValue ("start")->load();
    float en = processor.apvts.getRawParameterValue ("end")->load();
    float ls = processor.apvts.getRawParameterValue ("loopStart")->load();
    float le = processor.apvts.getRawParameterValue ("loopEnd")->load();
    bool loopOn = processor.apvts.getRawParameterValue ("sampleLoop")->load() > 0.5f;

    float dS = std::abs (normX - s), dE = std::abs (normX - en);
    float dLS = loopOn ? std::abs (normX - ls) : 99.0f;
    float dLE = loopOn ? std::abs (normX - le) : 99.0f;
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
    float normX = juce::jlimit (0.0f, 1.0f, e.x / static_cast<float> (std::max (1, getWidth())));
    const char* paramId = (activeDrag == MarkerDrag::Start) ? "start"
                        : (activeDrag == MarkerDrag::End) ? "end"
                        : (activeDrag == MarkerDrag::LoopStart) ? "loopStart" : "loopEnd";
    if (auto* p = processor.apvts.getParameter (paramId))
        p->setValueNotifyingHost (p->convertTo0to1 (normX));
    repaint();
}

void WaveEditorComponent::mouseUp (const juce::MouseEvent&) { activeDrag = MarkerDrag::None; }

// ---------------- RhythmSequencerComponent (Per-Zone Independent Rhythm!) ----------------
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
        g.drawText ("Load & select a key zone above to edit its individual rhythm pattern", area, juce::Justification::centred);
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
    g.drawText ("Active Pattern: " + z.name + "  |  Click=Add  Drag=Move/Vel  RightClick=Del",
                area.reduced (6, 2), juce::Justification::topRight, true);
}

void RhythmSequencerComponent::mouseDown (const juce::MouseEvent& e)
{
    auto area = getLocalBounds();
    const juce::ScopedLock sl (processor.zoneLock);
    if (processor.zones.empty() || processor.selectedZoneIndex < 0 || processor.selectedZoneIndex >= static_cast<int> (processor.zones.size()))
        return;

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
    if (processor.zones.empty() || processor.selectedZoneIndex < 0 || processor.selectedZoneIndex >= static_cast<int> (processor.zones.size()))
        return;

    auto& events = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)].seqEvents;
    if (draggedEventIdx >= static_cast<int> (events.size())) return;

    float normX = juce::jlimit (0.0f, 0.99f, e.position.x / static_cast<float> (std::max (1, area.getWidth())));
    if (! e.mods.isShiftDown())
        normX = snapToGrid (normX);

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

// ---------------- MainPluginView (Scalable Inner View) ----------------
MainPluginView::MainPluginView (NoiseMu6ixRhythmAudioProcessor& p, std::function<void(float)> onScaleSelected)
    : audioProcessor (p), scaleCallback (std::move (onScaleSelected)),
      keyMapper (p), waveEditor (p), seqComp (p), bottomPiano (p)
{
    // Clickable Title for UI Scaling (50%, 75%, 100%, 125%)
    addAndMakeVisible (titleScaleBtn);
    titleScaleBtn.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    titleScaleBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff273040));
    titleScaleBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    titleScaleBtn.setTooltip ("Click to change plugin size (50% / 75% / 100% / 125%)");
    titleScaleBtn.onClick = [this]() { showScaleMenu(); };

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
                if (dir.isDirectory())
                    refreshLibraryFolder (dir);
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

    // Editable Preset Name + SAVE / SAVE AS
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

    // Key Mapper + FRR / FVL / PITCH buttons
    addAndMakeVisible (keyMapper);
    keyMapper.onDoubleClickKey = [this] (int key) { openSampleChooserForKey (key); };

    addAndMakeVisible (frrBtn);
    addAndMakeVisible (fvlBtn);
    addAndMakeVisible (pitchTrackBtn);
    frrBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff5599));
    fvlBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff4ca3ff));
    pitchTrackBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffe8a138));
    frrBtn.setTooltip ("1000+ Generative Fake Round Robin");
    fvlBtn.setTooltip ("Continuous Fake Velocity Layer");
    pitchTrackBtn.setTooltip ("ON = Pitch changes across keys (Strings) | OFF = Fixed Pitch (Drums)");

    frrAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "frr", frrBtn);
    fvlAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "fvl", fvlBtn);
    pitchTrackAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "pitchTrack", pitchTrackBtn);

    // Wave Editor + Labeled Knobs (including Pan)
    addAndMakeVisible (waveEditor);
    addAndMakeVisible (sampleLoopBtn);
    sampleLoopBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff4cc98f));
    sampleLoopAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "sampleLoop", sampleLoopBtn);

    LabeledKnob* waveKnobs[] = { &startKnob, &endKnob, &loopStartKnob, &loopEndKnob, &pitchKnob, &panKnob, &volKnob,
                                 &attackKnob, &decayKnob, &sustainKnob, &releaseKnob, &cutoffKnob, &resKnob };
    for (auto* k : waveKnobs)
        addAndMakeVisible (*k);

    startAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "start", startKnob.slider);
    endAtt       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "end", endKnob.slider);
    loopStartAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "loopStart", loopStartKnob.slider);
    loopEndAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "loopEnd", loopEndKnob.slider);
    pitchAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "pitch", pitchKnob.slider);
    panAtt       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "pan", panKnob.slider);
    volAtt       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "volume", volKnob.slider);
    attackAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "attack", attackKnob.slider);
    decayAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "decay", decayKnob.slider);
    sustainAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "sustain", sustainKnob.slider);
    releaseAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "release", releaseKnob.slider);
    cutoffAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "cutoff", cutoffKnob.slider);
    resAtt       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "res", resKnob.slider);

    // Rhythm Sequencer + Bottom Controls
    addAndMakeVisible (seqComp);
    addAndMakeVisible (seqLoopBtn);
    seqLoopBtn.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffe84ea2));
    seqLoopAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "seqLoop", seqLoopBtn);

    addAndMakeVisible (shuffleKnob);
    addAndMakeVisible (timeRandKnob);
    addAndMakeVisible (velRandKnob);
    shuffleAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "shuffle", shuffleKnob.slider);
    timeRandAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "timeRand", timeRandKnob.slider);
    velRandAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "velRand", velRandKnob.slider);

    addAndMakeVisible (gridLabel);
    gridLabel.setText ("Grid", juce::dontSendNotification);
    gridLabel.setJustificationType (juce::Justification::centred);
    gridLabel.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    gridLabel.setColour (juce::Label::textColourId, juce::Colour (0xffc9d4e8));

    addAndMakeVisible (gridBox);
    gridBox.addItemList ({ "1/4", "1/8", "1/12 (T)", "1/16", "1/24 (T)" }, 1);
    gridAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (audioProcessor.apvts, "grid", gridBox);
    gridBox.onChange = [this]() { seqComp.repaint(); };

    addAndMakeVisible (bottomPiano);
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

void MainPluginView::openSampleChooserForKey (int targetKey)
{
    fileChooser = std::make_unique<juce::FileChooser> ("Select Sample for Key",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.wav;*.aiff;*.flac");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, targetKey] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f.existsAsFile())
            {
                audioProcessor.loadSampleIntoZone (f, targetKey, targetKey, targetKey);
                repaintDynamicAreas();
            }
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
                if (! f.hasFileExtension (".mn6x"))
                    f = f.withFileExtension (".mn6x");
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
    if (rowIsSelected)
        g.fillAll (juce::Colour (0xff3a2b4a));
    g.setColour (juce::Colours::lightgrey);
    g.setFont (11.5f);
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
        int nextKey = 60 + static_cast<int> (audioProcessor.zones.size());
        nextKey = juce::jlimit (36, 84, nextKey);
        audioProcessor.loadSampleIntoZone (f, nextKey, nextKey, nextKey);
    }
    repaintDynamicAreas();
}

void MainPluginView::handleFilesDropped (const juce::StringArray& files, int localX, int localY)
{
    int targetKey = 60 + static_cast<int> (audioProcessor.zones.size());
    if (keyMapper.getBounds().contains (localX, localY))
        targetKey = juce::jlimit (36, 84, keyMapper.noteAtX (localX - keyMapper.getX()));
    targetKey = juce::jlimit (36, 84, targetKey);

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
            audioProcessor.loadSampleIntoZone (f, targetKey, targetKey, targetKey);
            targetKey = juce::jlimit (36, 84, targetKey + 1);
        }
    }
    repaintDynamicAreas();
}

void MainPluginView::repaintDynamicAreas()
{
    keyMapper.repaint();
    waveEditor.repaint();
    seqComp.repaint();
    bottomPiano.repaint();
}

void MainPluginView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c222d));

    // Version v1.0.2 in bottom-right corner
    g.setColour (juce::Colour (0xff6e7b91));
    g.setFont (10.0f);
    g.drawText ("v1.0.2", getWidth() - 56, getHeight() - 16, 48, 13, juce::Justification::centredRight);

    // Section Labels
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.setColour (juce::Colour (0xff9aa8bf));
    g.drawText ("LIBRARY", 10, 8, 100, 16, juce::Justification::centredLeft);
    g.drawText ("KEY MAPPER", 184, 44, 140, 14, juce::Justification::centredLeft);
    g.drawText ("SAMPLE WAVE EDITOR", 184, 190, 200, 14, juce::Justification::centredLeft);
    g.drawText ("RHYTHM SEQUENCER", 184, 376, 200, 14, juce::Justification::centredLeft);
}

void MainPluginView::resized()
{
    auto area = getLocalBounds().reduced (8);

    // Left Library Panel (168px compact)
    auto libArea = area.removeFromLeft (168);
    libArea.removeFromTop (18);
    auto libBtnRow = libArea.removeFromTop (24);
    addLibBtn.setBounds (libBtnRow.removeFromLeft (98).reduced (1));
    sortAzBtn.setBounds (libBtnRow.removeFromLeft (35).reduced (1));
    delLibBtn.setBounds (libBtnRow.reduced (1));
    libraryList.setBounds (libArea.reduced (1));

    area.removeFromLeft (8);

    // Top Header Row
    auto topBar = area.removeFromTop (32);
    titleScaleBtn.setBounds (topBar.removeFromLeft (230).reduced (2));
    presetNameBox.setBounds (topBar.removeFromLeft (290).reduced (4, 3));
    saveBtn.setBounds (topBar.removeFromLeft (72).reduced (3, 3));
    saveAsBtn.setBounds (topBar.removeFromLeft (78).reduced (3, 3));

    // Key Mapper + FRR / FVL / PITCH
    area.removeFromTop (18);
    auto kmRow = area.removeFromTop (126);
    auto kmSide = kmRow.removeFromRight (72);
    frrBtn.setBounds (kmSide.removeFromTop (30).reduced (2));
    fvlBtn.setBounds (kmSide.removeFromTop (30).reduced (2));
    pitchTrackBtn.setBounds (kmSide.removeFromTop (30).reduced (2));
    keyMapper.setBounds (kmRow);

    // Sample Wave Editor
    area.removeFromTop (18);
    auto waveHeader = area.removeFromTop (20);
    sampleLoopBtn.setBounds (waveHeader.removeFromRight (68));
    waveEditor.setBounds (area.removeFromTop (84));

    // Wave Editor Labeled Knobs Row (13 knobs including Pan)
    area.removeFromTop (3);
    auto knobRow = area.removeFromTop (64);
    LabeledKnob* waveKnobs[] = { &startKnob, &endKnob, &loopStartKnob, &loopEndKnob, &pitchKnob, &panKnob, &volKnob,
                                 &attackKnob, &decayKnob, &sustainKnob, &releaseKnob, &cutoffKnob, &resKnob };
    int kw = knobRow.getWidth() / 13;
    for (auto* k : waveKnobs)
        k->setBounds (knobRow.removeFromLeft (kw).reduced (1));

    // Rhythm Sequencer
    area.removeFromTop (16);
    auto seqRow = area.removeFromTop (86);
    auto seqSide = seqRow.removeFromRight (72);
    seqLoopBtn.setBounds (seqSide.removeFromTop (30).reduced (2));
    seqComp.setBounds (seqRow);

    // Bottom Labeled Controls Row
    area.removeFromTop (4);
    auto bottomCtrl = area.removeFromTop (58);
    shuffleKnob.setBounds (bottomCtrl.removeFromLeft (68).reduced (1));
    timeRandKnob.setBounds (bottomCtrl.removeFromLeft (68).reduced (1));
    velRandKnob.setBounds (bottomCtrl.removeFromLeft (68).reduced (1));
    auto gridArea = bottomCtrl.removeFromLeft (104).reduced (4, 1);
    gridLabel.setBounds (gridArea.removeFromTop (14));
    gridBox.setBounds (gridArea.removeFromTop (24).reduced (0, 1));

    // Bottom Playable Color-Coded Keyboard
    area.removeFromTop (4);
    bottomPiano.setBounds (area.removeFromTop (34));
}

// ---------------- Outer Scalable Editor ----------------
NoiseMu6ixRhythmAudioProcessorEditor::NoiseMu6ixRhythmAudioProcessorEditor (NoiseMu6ixRhythmAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      mainView (p, [this] (float s) { applyUiScale (s); })
{
    addAndMakeVisible (mainView);
    mainView.setBounds (0, 0, baseWidth, baseHeight);
    setSize (baseWidth, baseHeight);
    startTimerHz (24);
}

NoiseMu6ixRhythmAudioProcessorEditor::~NoiseMu6ixRhythmAudioProcessorEditor() { stopTimer(); }

void NoiseMu6ixRhythmAudioProcessorEditor::applyUiScale (float newScale)
{
    currentScale = newScale;
    mainView.setTransform (juce::AffineTransform::scale (currentScale));
    setSize (static_cast<int> (std::round (baseWidth * currentScale)),
             static_cast<int> (std::round (baseHeight * currentScale)));
}

void NoiseMu6ixRhythmAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c222d));
}

void NoiseMu6ixRhythmAudioProcessorEditor::resized()
{
    mainView.setBounds (0, 0, baseWidth, baseHeight);
    mainView.setTransform (juce::AffineTransform::scale (currentScale));
}

void NoiseMu6ixRhythmAudioProcessorEditor::timerCallback()
{
    mainView.repaintDynamicAreas();
}

bool NoiseMu6ixRhythmAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray&) { return true; }

void NoiseMu6ixRhythmAudioProcessorEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    int localX = static_cast<int> (x / currentScale);
    int localY = static_cast<int> (y / currentScale);
    mainView.handleFilesDropped (files, localX, localY);
}
