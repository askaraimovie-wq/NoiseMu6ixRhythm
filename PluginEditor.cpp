#include "PluginEditor.h"

// ---------------- KeyMapperComponent ----------------
KeyMapperComponent::KeyMapperComponent (NoiseMu6ixRhythmAudioProcessor& p) : processor (p) {}

void KeyMapperComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll (juce::Colour (0xff181d26));
    g.setColour (juce::Colour (0xff2c3545));
    g.drawRect (area, 1);

    auto gridArea = area.removeFromTop (area.getHeight() - 36);
    auto keyArea = area;

    // Velocity grid lines
    g.setColour (juce::Colour (0xff232a36));
    for (int i = 1; i < 4; ++i)
    {
        int y = gridArea.getY() + (gridArea.getHeight() * i) / 4;
        g.drawHorizontalLine (y, static_cast<float> (gridArea.getX()), static_cast<float> (gridArea.getRight()));
    }

    // Draw Sample Zones
    {
        const juce::ScopedLock sl (processor.zoneLock);
        const juce::Colour palette[4] = {
            juce::Colour (0x99d9538f), juce::Colour (0x994da6ff),
            juce::Colour (0x99e6a139), juce::Colour (0x995bc995)
        };
        for (int i = 0; i < static_cast<int> (processor.zones.size()); ++i)
        {
            const auto& z = processor.zones[static_cast<size_t> (i)];
            float x1 = gridArea.getX() + ((z.lowKey - startNote) / static_cast<float> (numKeys)) * gridArea.getWidth();
            float x2 = gridArea.getX() + ((z.highKey - startNote + 1) / static_cast<float> (numKeys)) * gridArea.getWidth();
            float y1 = gridArea.getBottom() - (z.highVel / 127.0f) * gridArea.getHeight();
            float y2 = gridArea.getBottom() - (z.lowVel / 127.0f) * gridArea.getHeight();
            juce::Rectangle<float> r (x1, y1, std::max (8.0f, x2 - x1), std::max (8.0f, y2 - y1));
            g.setColour (palette[i % 4]);
            g.fillRect (r);
            g.setColour (i == processor.selectedZoneIndex ? juce::Colours::white : juce::Colours::lightgrey.withAlpha (0.5f));
            g.drawRect (r, i == processor.selectedZoneIndex ? 2.0f : 1.0f);
            g.setFont (11.0f);
            g.drawText (z.name, r.reduced (3.0f), juce::Justification::topLeft, true);
        }
    }

    // Draw Horizontal Piano Keyboard
    float keyW = keyArea.getWidth() / static_cast<float> (numKeys);
    for (int i = 0; i < numKeys; ++i)
    {
        int note = startNote + i;
        int n = note % 12;
        bool isBlack = (n == 1 || n == 3 || n == 6 || n == 8 || n == 10);
        juce::Rectangle<float> kr (keyArea.getX() + i * keyW, static_cast<float> (keyArea.getY()), keyW, static_cast<float> (keyArea.getHeight()));
        g.setColour (isBlack ? juce::Colour (0xff15181f) : juce::Colour (0xffdce2ec));
        g.fillRect (kr);
        g.setColour (juce::Colour (0xff2b313d));
        g.drawRect (kr, 0.8f);
    }
}

void KeyMapperComponent::mouseDown (const juce::MouseEvent& e)
{
    auto area = getLocalBounds();
    int note = startNote + static_cast<int> ((e.x / static_cast<float> (area.getWidth())) * numKeys);
    note = juce::jlimit (startNote, startNote + numKeys - 1, note);
    processor.triggerNote (note, 0.85f);
    repaint();
}

// ---------------- WaveEditorComponent ----------------
WaveEditorComponent::WaveEditorComponent (NoiseMu6ixRhythmAudioProcessor& p) : processor (p) {}

void WaveEditorComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll (juce::Colour (0xff131820));
    g.setColour (juce::Colour (0xff2c3545));
    g.drawRect (area, 1);

    const juce::ScopedLock sl (processor.zoneLock);
    if (processor.zones.empty() || processor.selectedZoneIndex < 0 || processor.selectedZoneIndex >= static_cast<int> (processor.zones.size()))
    {
        g.setColour (juce::Colours::grey);
        g.setFont (14.0f);
        g.drawText ("Drag & Drop WAV/AIFF Sample Here (or Double-Click from Library)", area, juce::Justification::centred);
        return;
    }

    const auto& z = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)];
    const float* data = z.buffer.getReadPointer (0);
    int numSamples = z.buffer.getNumSamples();
    if (numSamples <= 0) return;

    juce::Path p;
    float midY = area.getCentreY();
    float halfH = area.getHeight() * 0.44f;
    p.startNewSubPath (static_cast<float> (area.getX()), midY);

    int step = std::max (1, numSamples / std::max (1, area.getWidth()));
    for (int x = 0; x < area.getWidth(); ++x)
    {
        int idx = std::min (numSamples - 1, x * step);
        float v = data[idx];
        p.lineTo (static_cast<float> (area.getX() + x), midY - v * halfH);
    }

    g.setColour (juce::Colour (0xff78b7ff));
    g.strokePath (p, juce::PathStrokeType (1.2f));

    // Start & End markers
    float startX = area.getX() + (*processor.apvts.getRawParameterValue ("start")) * area.getWidth();
    float endX = area.getX() + (*processor.apvts.getRawParameterValue ("end")) * area.getWidth();
    g.setColour (juce::Colour (0xffff66aa));
    g.drawVerticalLine (static_cast<int> (startX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
    g.drawVerticalLine (static_cast<int> (endX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
}

// ---------------- RhythmSequencerComponent ----------------
RhythmSequencerComponent::RhythmSequencerComponent (NoiseMu6ixRhythmAudioProcessor& p) : processor (p) {}

void RhythmSequencerComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll (juce::Colour (0xff1a1520));
    g.setColour (juce::Colour (0xff352940));
    g.drawRect (area, 1);

    float stepW = area.getWidth() / 16.0f;
    int activeStep = processor.currentSeqStep.load();

    for (int i = 0; i < 16; ++i)
    {
        float x = area.getX() + i * stepW;
        if (i == activeStep)
        {
            g.setColour (juce::Colour (0x25ff4da6));
            g.fillRect (x, static_cast<float> (area.getY()), stepW, static_cast<float> (area.getHeight()));
        }
        g.setColour (juce::Colour (0xff2b2233));
        g.drawVerticalLine (static_cast<int> (x), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));

        float vel = processor.stepVelocities[static_cast<size_t> (i)];
        if (vel > 0.01f)
        {
            float barH = vel * (area.getHeight() - 16.0f);
            float cx = x + stepW * 0.5f;
            g.setColour (juce::Colour (0xffe0489e));
            g.drawLine (cx, static_cast<float> (area.getBottom() - 8), cx, area.getBottom() - 8.0f - barH, 3.5f);
            g.fillEllipse (cx - 4.0f, static_cast<float> (area.getBottom() - 12), 8.0f, 8.0f);
        }
    }
}

void RhythmSequencerComponent::updateStepFromMouse (const juce::MouseEvent& e)
{
    int step = juce::jlimit (0, 15, static_cast<int> ((e.x / static_cast<float> (getWidth())) * 16.0f));
    float vel = juce::jlimit (0.0f, 1.0f, 1.0f - (e.y / static_cast<float> (getHeight())));
    if (vel < 0.08f) vel = 0.0f;
    processor.stepVelocities[static_cast<size_t> (step)] = vel;
    repaint();
}

void RhythmSequencerComponent::mouseDown (const juce::MouseEvent& e) { updateStepFromMouse (e); }
void RhythmSequencerComponent::mouseDrag (const juce::MouseEvent& e) { updateStepFromMouse (e); }

// ---------------- Main Editor ----------------
NoiseMu6ixRhythmAudioProcessorEditor::NoiseMu6ixRhythmAudioProcessorEditor (NoiseMu6ixRhythmAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), keyMapper (p), waveEditor (p), seqComp (p)
{
    setSize (1080, 680);

    // Library Panel
    addAndMakeVisible (addLibBtn);
    addAndMakeVisible (sortAzBtn);
    addAndMakeVisible (delLibBtn);
    addAndMakeVisible (libraryList);
    libraryList.setModel (this);
    libraryList.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff151921));

    addLibBtn.onClick = [this]()
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Select Sample/Library Folder", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));
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

    // Top Bar
    addAndMakeVisible (presetLabel);
    presetLabel.setText (audioProcessor.currentPresetName, juce::dontSendNotification);
    presetLabel.setJustificationType (juce::Justification::centred);
    presetLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff19202b));
    presetLabel.setColour (juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible (saveBtn);
    addAndMakeVisible (saveAsBtn);
    auto doSave = [this]()
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Save Preset (.mn6x)", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.mn6x");
        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f != juce::File())
                {
                    if (! f.hasFileExtension (".mn6x"))
                        f = f.withFileExtension (".mn6x");
                    audioProcessor.savePresetMn6x (f);
                    presetLabel.setText (f.getFileName(), juce::dontSendNotification);
                }
            });
    };
    saveBtn.onClick = doSave;
    saveAsBtn.onClick = doSave;

    // Key Mapper + FRR / FVL
    addAndMakeVisible (keyMapper);
    addAndMakeVisible (frrBtn);
    addAndMakeVisible (fvlBtn);
    frrBtn.setTooltip ("Fake Round Robin: Auto micro-pitch, pan & EQ variation on single hit");
    fvlBtn.setTooltip ("Fake Velocity Layer: Dynamic filter & tone shaping by velocity");
    frrAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "frr", frrBtn);
    fvlAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "fvl", fvlBtn);

    // Wave Editor + Controls
    addAndMakeVisible (waveEditor);
    addAndMakeVisible (sampleLoopBtn);
    sampleLoopAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "sampleLoop", sampleLoopBtn);

    setupKnob (startSlider, "Start");
    setupKnob (endSlider, "End");
    setupKnob (pitchSlider, "Pitch");
    setupKnob (volSlider, "Vol");
    setupKnob (attackSlider, "Atk");
    setupKnob (decaySlider, "Dec");
    setupKnob (sustainSlider, "Sus");
    setupKnob (releaseSlider, "Rel");
    setupKnob (cutoffSlider, "Cut");
    setupKnob (resSlider, "Res");

    startAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "start", startSlider);
    endAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "end", endSlider);
    pitchAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "pitch", pitchSlider);
    volAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "volume", volSlider);
    attackAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "attack", attackSlider);
    decayAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "decay", decaySlider);
    sustainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "sustain", sustainSlider);
    releaseAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "release", releaseSlider);
    cutoffAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "cutoff", cutoffSlider);
    resAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "res", resSlider);

    // Rhythm Sequencer + Bottom Bar
    addAndMakeVisible (seqComp);
    addAndMakeVisible (seqLoopBtn);
    seqLoopAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "seqLoop", seqLoopBtn);

    setupKnob (shuffleSlider, "Shuf");
    setupKnob (timeRandSlider, "T-Rnd");
    setupKnob (velRandSlider, "V-Rnd");
    shuffleAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "shuffle", shuffleSlider);
    timeRandAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "timeRand", timeRandSlider);
    velRandAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "velRand", velRandSlider);

    addAndMakeVisible (gridBox);
    gridBox.addItemList ({ "1/4", "1/8", "1/16" }, 1);
    gridAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (audioProcessor.apvts, "grid", gridBox);

    startTimerHz (24);
}

NoiseMu6ixRhythmAudioProcessorEditor::~NoiseMu6ixRhythmAudioProcessorEditor() { stopTimer(); }

void NoiseMu6ixRhythmAudioProcessorEditor::setupKnob (juce::Slider& s, const juce::String& name)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 16);
    s.setName (name);
    addAndMakeVisible (s);
}

void NoiseMu6ixRhythmAudioProcessorEditor::refreshLibraryFolder (const juce::File& folder)
{
    libraryFiles.clear();
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, true, "*.wav;*.aiff;*.flac;*.mn6x"))
        libraryFiles.add (f);
    libraryList.updateContent();
}

int NoiseMu6ixRhythmAudioProcessorEditor::getNumRows() { return libraryFiles.size(); }

void NoiseMu6ixRhythmAudioProcessorEditor::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll (juce::Colour (0xff3a2b4a));
    g.setColour (juce::Colours::lightgrey);
    g.setFont (13.0f);
    if (rowNumber >= 0 && rowNumber < libraryFiles.size())
        g.drawText (libraryFiles[rowNumber].getFileName(), 6, 0, width - 10, height, juce::Justification::centredLeft, true);
}

void NoiseMu6ixRhythmAudioProcessorEditor::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= libraryFiles.size()) return;
    auto f = libraryFiles[row];
    if (f.hasFileExtension (".mn6x"))
    {
        audioProcessor.loadPresetMn6x (f);
        presetLabel.setText (f.getFileName(), juce::dontSendNotification);
    }
    else
    {
        audioProcessor.loadSampleIntoZone (f, 36, 84, 60);
    }
    keyMapper.repaint();
    waveEditor.repaint();
}

bool NoiseMu6ixRhythmAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray&) { return true; }

void NoiseMu6ixRhythmAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& path : files)
    {
        juce::File f (path);
        if (f.hasFileExtension (".mn6x"))
            audioProcessor.loadPresetMn6x (f);
        else
            audioProcessor.loadSampleIntoZone (f, 36, 84, 60);
    }
    keyMapper.repaint();
    waveEditor.repaint();
}

void NoiseMu6ixRhythmAudioProcessorEditor::timerCallback()
{
    seqComp.repaint();
    waveEditor.repaint();
}

void NoiseMu6ixRhythmAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2430));

    // Title
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("NOISE MU6IX RHYTHM v1.0.1", 232, 12, 360, 36, juce::Justification::centredLeft);

    // Section Labels
    g.setFont (12.0f);
    g.setColour (juce::Colour (0xff9aa8bf));
    g.drawText ("LIBRARY", 12, 10, 120, 20, juce::Justification::centredLeft);
    g.drawText ("KEY MAPPER", 232, 56, 140, 18, juce::Justification::centredLeft);
    g.drawText ("SAMPLE WAVE EDITOR", 232, 240, 200, 18, juce::Justification::centredLeft);
    g.drawText ("RHYTHM SEQUENCER", 232, 464, 200, 18, juce::Justification::centredLeft);
}

void NoiseMu6ixRhythmAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);

    // Left External Library Panel (210px)
    auto libArea = area.removeFromLeft (210);
    libArea.removeFromTop (24);
    auto libBtnRow = libArea.removeFromTop (28);
    addLibBtn.setBounds (libBtnRow.removeFromLeft (120).reduced (2));
    sortAzBtn.setBounds (libBtnRow.removeFromLeft (44).reduced (2));
    delLibBtn.setBounds (libBtnRow.reduced (2));
    libraryList.setBounds (libArea.reduced (2));

    area.removeFromLeft (12);

    // Top Header Row
    auto topBar = area.removeFromTop (44);
    topBar.removeFromLeft (370);
    presetLabel.setBounds (topBar.removeFromLeft (250).reduced (4));
    saveBtn.setBounds (topBar.removeFromLeft (105).reduced (4));
    saveAsBtn.setBounds (topBar.removeFromLeft (95).reduced (4));

    // Key Mapper + FRR/FVL side buttons
    area.removeFromTop (22);
    auto kmRow = area.removeFromTop (160);
    auto kmSide = kmRow.removeFromRight (84);
    frrBtn.setBounds (kmSide.removeFromTop (42).reduced (4));
    fvlBtn.setBounds (kmSide.removeFromTop (42).reduced (4));
    keyMapper.setBounds (kmRow);

    // Sample Wave Editor
    area.removeFromTop (24);
    auto waveHeader = area.removeFromTop (24);
    sampleLoopBtn.setBounds (waveHeader.removeFromRight (80));
    waveEditor.setBounds (area.removeFromTop (105));

    // Wave Editor Knobs Row
    auto knobRow = area.removeFromTop (72);
    juce::Slider* knobs[] = { &startSlider, &endSlider, &pitchSlider, &volSlider,
                              &attackSlider, &decaySlider, &sustainSlider, &releaseSlider,
                              &cutoffSlider, &resSlider };
    int kw = knobRow.getWidth() / 10;
    for (auto* k : knobs)
        k->setBounds (knobRow.removeFromLeft (kw).reduced (2));

    // Rhythm Sequencer
    area.removeFromTop (20);
    auto seqRow = area.removeFromTop (110);
    auto seqSide = seqRow.removeFromRight (84);
    seqLoopBtn.setBounds (seqSide.removeFromTop (36).reduced (4));
    seqComp.setBounds (seqRow);

    // Bottom Global Controls
    area.removeFromTop (6);
    shuffleSlider.setBounds (area.removeFromLeft (80).reduced (2));
    timeRandSlider.setBounds (area.removeFromLeft (80).reduced (2));
    velRandSlider.setBounds (area.removeFromLeft (80).reduced (2));
    gridBox.setBounds (area.removeFromLeft (100).reduced (6, 18));
}
