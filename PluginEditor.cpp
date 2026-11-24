#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

static void drawSketchRect (juce::Graphics& g, juce::Rectangle<float> r, float corner, float lineW, juce::Colour col)
{
    g.setColour (col);
    g.drawRoundedRectangle (r, corner, lineW);
    g.setColour (col.withAlpha (0.45f));
    g.drawLine (r.getX() + 4.0f, r.getY() + 0.6f, r.getRight() - 4.0f, r.getY() + 0.6f, lineW * 0.7f);
}

// ---------------- StyledHorizontalFader ----------------
StyledHorizontalFader::StyledHorizontalFader (const juce::String& labelText, double minV, double maxV, double defaultV, int decimals)
    : title (labelText), minVal (minV), maxVal (maxV), defaultVal (defaultV), numDecimals (decimals), currentValue (defaultV)
{
    editor.setMultiLine (false);
    editor.setJustification (juce::Justification::centredRight);
    editor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff07080a));
    editor.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xfff2f4f8));
    editor.setColour (juce::TextEditor::textColourId, juce::Colour (0xfff2f4f8));
    editor.onReturnKey = [this]() { commitText(); };
    editor.onFocusLost = [this]() { commitText(); };
    addChildComponent (editor);
}

void StyledHorizontalFader::setValue (double v, bool notify)
{
    currentValue = juce::jlimit (minVal, maxVal, v);
    repaint();
    if (notify && onValueChanged) onValueChanged (currentValue);
}

void StyledHorizontalFader::setStyleAndColour (int id, juce::Colour col)
{
    styleId = juce::jlimit (1, 10, id);
    accentCol = col;
    repaint();
}

void StyledHorizontalFader::openTypeEditor()
{
    isEditingText = true;
    editor.setText (numDecimals == 0 ? juce::String (static_cast<int> (std::round (currentValue)))
                                     : juce::String (currentValue, numDecimals), false);
    editor.setBounds (getLocalBounds().removeFromTop (13).removeFromRight (46));
    editor.setVisible (true);
    editor.grabKeyboardFocus();
    editor.selectAll();
}

void StyledHorizontalFader::commitText()
{
    if (! isEditingText) return;
    isEditingText = false;
    editor.setVisible (false);
    setValue (editor.getText().getDoubleValue(), true);
}

void StyledHorizontalFader::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (13);
    auto valArea = top.removeFromRight (34);
    g.setColour (juce::Colour (0xfff2f4f8));
    g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
    g.drawText (title, top.reduced (1, 0), juce::Justification::centredLeft, true);

    if (! isEditingText)
    {
        juce::String vStr = (numDecimals == 0) ? juce::String (static_cast<int> (std::round (currentValue)))
                                               : juce::String (currentValue, numDecimals);
        g.setColour (juce::Colour (0xffd5dbe5));
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.drawText (vStr, valArea, juce::Justification::centredRight, true);
    }

    auto tr = r.reduced (3, 4).toFloat();
    float norm = static_cast<float> ((currentValue - minVal) / std::max (1e-6, maxVal - minVal));
    float fillW = norm * tr.getWidth();
    float thumbX = tr.getX() + fillW;
    auto acc = accentCol;

    if (styleId == 1)
    {
        g.setColour (juce::Colour (0xff07080a)); g.fillRoundedRectangle (tr, 3.0f);
        g.setColour (acc); g.fillRoundedRectangle (tr.withWidth (fillW), 3.0f);
        g.setColour (juce::Colours::white); g.fillEllipse (thumbX - 5.0f, tr.getCentreY() - 5.0f, 10.0f, 10.0f);
    }
    else if (styleId == 2)
    {
        auto line = tr.withSizeKeepingCentre (tr.getWidth(), 3.0f);
        g.setColour (juce::Colour (0xff07080a)); g.fillRect (line);
        g.setColour (acc); g.fillRect (line.withWidth (fillW));
        g.setColour (juce::Colours::white); g.fillRoundedRectangle (thumbX - 3.5f, tr.getY() - 1.0f, 7.0f, tr.getHeight() + 2.0f, 2.0f);
    }
    else if (styleId == 3)
    {
        int segs = 14;
        float sw = tr.getWidth() / segs;
        for (int i = 0; i < segs; ++i)
        {
            bool lit = (i / static_cast<float> (segs)) < norm;
            g.setColour (lit ? acc : juce::Colour (0xff121418));
            g.fillRect (tr.getX() + i * sw + 1.0f, tr.getY() + 1.0f, sw - 2.0f, tr.getHeight() - 2.0f);
        }
    }
    else if (styleId == 4)
    {
        g.setColour (juce::Colour (0xff07080a)); g.fillRect (tr);
        g.setColour (acc.withAlpha (0.85f)); g.fillRect (tr.withWidth (fillW));
        g.setColour (juce::Colours::white); g.fillRect (thumbX - 1.5f, tr.getY(), 3.0f, tr.getHeight());
    }
    else if (styleId == 5)
    {
        g.setColour (juce::Colour (0xff0c0e12)); g.fillRoundedRectangle (tr, tr.getHeight() * 0.5f);
        g.setColour (acc); g.fillRoundedRectangle (tr.withWidth (fillW), tr.getHeight() * 0.5f);
        juce::Path d;
        d.startNewSubPath (thumbX, tr.getY() - 1.0f); d.lineTo (thumbX + 6.0f, tr.getCentreY());
        d.lineTo (thumbX, tr.getBottom() + 1.0f); d.lineTo (thumbX - 6.0f, tr.getCentreY()); d.closeSubPath();
        g.setColour (juce::Colours::white); g.fillPath (d);
    }
    else if (styleId == 6)
    {
        g.setColour (juce::Colour (0xfff2f4f8)); g.drawRect (tr, 1.2f);
        g.setColour (acc); g.fillRect (tr.reduced (2.0f).withWidth (std::max (0.0f, fillW - 4.0f)));
        g.setColour (juce::Colours::white); g.fillRect (thumbX - 5.0f, tr.getY() - 1.0f, 10.0f, tr.getHeight() + 2.0f);
        g.setColour (juce::Colours::black); g.drawVerticalLine (static_cast<int> (thumbX), tr.getY(), tr.getBottom());
    }
    else if (styleId == 7)
    {
        auto line = tr.withSizeKeepingCentre (tr.getWidth(), 4.0f);
        g.setColour (juce::Colour (0xff07080a)); g.fillRoundedRectangle (line, 2.0f);
        g.setColour (acc.withAlpha (0.35f)); g.fillRoundedRectangle (line.withWidth (fillW).expanded (0.0f, 2.0f), 3.0f);
        g.setColour (acc); g.fillRoundedRectangle (line.withWidth (fillW), 2.0f);
        g.setColour (juce::Colour (0xff07080a)); g.fillEllipse (thumbX - 5.5f, tr.getCentreY() - 5.5f, 11.0f, 11.0f);
        g.setColour (acc); g.drawEllipse (thumbX - 5.5f, tr.getCentreY() - 5.5f, 11.0f, 11.0f, 2.0f);
    }
    else if (styleId == 8)
    {
        g.setColour (juce::Colour (0xff0d0f13)); g.fillRect (tr);
        g.setColour (juce::Colour (0xff9aa4b5));
        for (int t = 0; t <= 10; ++t)
            g.drawVerticalLine (static_cast<int> (tr.getX() + (tr.getWidth() * t) / 10.0f), tr.getBottom() - 4.0f, tr.getBottom());
        g.setColour (acc); g.fillRect (tr.getX(), tr.getY(), fillW, 4.0f);
        juce::Path tri;
        tri.startNewSubPath (thumbX, tr.getY() + 2.0f); tri.lineTo (thumbX + 5.0f, tr.getBottom()); tri.lineTo (thumbX - 5.0f, tr.getBottom()); tri.closeSubPath();
        g.setColour (juce::Colours::white); g.fillPath (tri);
    }
    else if (styleId == 9)
    {
        g.setColour (juce::Colour (0xff9aa4b5)); g.drawHorizontalLine (static_cast<int> (tr.getBottom() - 2.0f), tr.getX(), tr.getRight());
        g.setColour (acc); g.fillRect (tr.getX(), tr.getBottom() - 3.5f, fillW, 3.0f);
        g.setColour (acc); g.fillRect (thumbX - 4.0f, tr.getY() + 1.0f, 8.0f, tr.getHeight() - 3.0f);
    }
    else
    {
        g.setColour (juce::Colour (0xff07080a)); g.fillRoundedRectangle (tr, 2.0f);
        g.setColour (acc); g.fillRect (tr.getX() + 1.0f, tr.getCentreY() - 2.0f, std::max (0.0f, fillW - 2.0f), 4.0f);
        g.setColour (juce::Colour (0xff2b303a)); g.fillRoundedRectangle (thumbX - 6.0f, tr.getY(), 12.0f, tr.getHeight(), 2.0f);
        g.setColour (acc); g.fillRect (thumbX - 1.0f, tr.getY() + 1.0f, 2.0f, tr.getHeight() - 2.0f);
    }
}

void StyledHorizontalFader::resized()
{
    if (isEditingText)
        editor.setBounds (getLocalBounds().removeFromTop (13).removeFromRight (46));
}

void StyledHorizontalFader::updateFromX (int mx)
{
    auto tr = getLocalBounds().withTrimmedTop (13).reduced (3, 0);
    double norm = juce::jlimit (0.0, 1.0, (mx - tr.getX()) / static_cast<double> (std::max (1, tr.getWidth())));
    setValue (minVal + norm * (maxVal - minVal), true);
}

void StyledHorizontalFader::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown()) { openTypeEditor(); return; }
    updateFromX (e.x);
}

void StyledHorizontalFader::mouseDrag (const juce::MouseEvent& e)
{
    if (! e.mods.isRightButtonDown()) updateFromX (e.x);
}

void StyledHorizontalFader::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! e.mods.isRightButtonDown()) setValue (defaultVal, true);
}

// ---------------- CenterZeroPanFader ----------------
CenterZeroPanFader::CenterZeroPanFader (const juce::String& labelText) : title (labelText)
{
    editor.setMultiLine (false);
    editor.setJustification (juce::Justification::centredRight);
    editor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff07080a));
    editor.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xfff2f4f8));
    editor.setColour (juce::TextEditor::textColourId, juce::Colour (0xfff2f4f8));
    editor.onReturnKey = [this]() { commitText(); };
    editor.onFocusLost = [this]() { commitText(); };
    addChildComponent (editor);
}

void CenterZeroPanFader::setValue (float v, bool notify)
{
    value = juce::jlimit (-50.0f, 50.0f, v);
    repaint();
    if (notify && onValueChanged) onValueChanged (value);
}

void CenterZeroPanFader::setStyleAndColour (int id, juce::Colour col)
{
    styleId = juce::jlimit (1, 10, id);
    accentCol = col;
    repaint();
}

void CenterZeroPanFader::openTypeEditor()
{
    isEditingText = true;
    editor.setText (juce::String (static_cast<int> (std::round (value))), false);
    editor.setBounds (getLocalBounds().removeFromTop (13).removeFromRight (44));
    editor.setVisible (true);
    editor.grabKeyboardFocus();
    editor.selectAll();
}

void CenterZeroPanFader::commitText()
{
    if (! isEditingText) return;
    isEditingText = false;
    editor.setVisible (false);
    setValue (static_cast<float> (editor.getText().getDoubleValue()), true);
}

void CenterZeroPanFader::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (13);
    g.setColour (juce::Colour (0xfff2f4f8));
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText (title, top.removeFromLeft (top.getWidth() - 36), juce::Justification::centredLeft, false);

    if (! isEditingText)
    {
        juce::String valStr = (std::abs (value) < 0.5f) ? "0" : ((value > 0.0f ? "+" : "") + juce::String (static_cast<int> (std::round (value))));
        g.setColour (juce::Colour (0xffd5dbe5));
        g.drawText (valStr, top, juce::Justification::centredRight, false);
    }

    auto track = r.reduced (4, 4).toFloat();
    auto acc = accentCol;
    g.setColour (juce::Colour (0xff07080a));
    g.fillRoundedRectangle (track, (styleId % 2 == 0) ? 1.5f : 4.0f);
    drawSketchRect (g, track, (styleId % 2 == 0) ? 1.5f : 4.0f, 1.1f, juce::Colour (0xfff2f4f8));

    float cx = track.getCentreX();
    float norm = value / 50.0f;
    float thumbX = cx + norm * (track.getWidth() * 0.5f);

    if (value < -0.5f)
    {
        g.setColour (acc);
        g.fillRect (thumbX, track.getY() + 1.0f, cx - thumbX, track.getHeight() - 2.0f);
    }
    else if (value > 0.5f)
    {
        g.setColour (acc);
        g.fillRect (cx, track.getY() + 1.0f, thumbX - cx, track.getHeight() - 2.0f);
    }

    g.setColour (juce::Colour (0xffcfd5df));
    g.fillEllipse (cx - 2.0f, track.getCentreY() - 2.0f, 4.0f, 4.0f);

    g.setColour (juce::Colours::white);
    if (styleId % 3 == 0)
        g.fillRect (thumbX - 3.0f, track.getY() - 1.0f, 6.0f, track.getHeight() + 2.0f);
    else
        g.fillEllipse (thumbX - 5.0f, track.getCentreY() - 5.0f, 10.0f, 10.0f);
}

void CenterZeroPanFader::resized()
{
    if (isEditingText)
        editor.setBounds (getLocalBounds().removeFromTop (13).removeFromRight (44));
}

void CenterZeroPanFader::updateFromX (int mx)
{
    auto track = getLocalBounds().withTrimmedTop (13).reduced (4, 0);
    float norm = ((mx - track.getX()) / static_cast<float> (std::max (1, track.getWidth()))) * 2.0f - 1.0f;
    float v = juce::jlimit (-50.0f, 50.0f, norm * 50.0f);
    if (std::abs (v) < 1.8f) v = 0.0f;
    setValue (v, true);
}

void CenterZeroPanFader::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown()) { openTypeEditor(); return; }
    updateFromX (e.x);
}

void CenterZeroPanFader::mouseDrag (const juce::MouseEvent& e)
{
    if (! e.mods.isRightButtonDown()) updateFromX (e.x);
}

void CenterZeroPanFader::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! e.mods.isRightButtonDown()) setValue (0.0f, true);
}

// ---------------- DragTypeNumberBox ----------------
DragTypeNumberBox::DragTypeNumberBox (const juce::String& labelText, const juce::String& unitSuffix,
                                      double minV, double maxV, double defaultV, int decimals, bool showTopLabel)
    : title (labelText), suffix (unitSuffix), minVal (minV), maxVal (maxV), defaultVal (defaultV),
      numDecimals (decimals), hasLabel (showTopLabel), currentValue (defaultV)
{
    editor.setMultiLine (false);
    editor.setJustification (juce::Justification::centred);
    editor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff07080a));
    editor.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xfff2f4f8));
    editor.setColour (juce::TextEditor::textColourId, juce::Colour (0xfff2f4f8));
    editor.onReturnKey = [this]() { commitText(); };
    editor.onFocusLost = [this]() { commitText(); };
    addChildComponent (editor);
}

juce::String DragTypeNumberBox::formatDisplay() const
{
    if (numDecimals == 0)
        return juce::String (static_cast<int> (std::round (currentValue))) + suffix;
    return juce::String (currentValue, numDecimals) + suffix;
}

void DragTypeNumberBox::setValue (double v, bool notify)
{
    currentValue = juce::jlimit (minVal, maxVal, v);
    repaint();
    if (notify && onValueChanged) onValueChanged (currentValue);
}

void DragTypeNumberBox::setStyleAndColour (int id, juce::Colour col)
{
    styleId = juce::jlimit (1, 10, id);
    accentCol = col;
    repaint();
}

void DragTypeNumberBox::openTypeEditor()
{
    isEditingText = true;
    editor.setText (numDecimals == 0 ? juce::String (static_cast<int> (std::round (currentValue)))
                                     : juce::String (currentValue, numDecimals), false);
    editor.setVisible (true);
    editor.grabKeyboardFocus();
    editor.selectAll();
}

void DragTypeNumberBox::commitText()
{
    if (! isEditingText) return;
    isEditingText = false;
    editor.setVisible (false);
    setValue (editor.getText().getDoubleValue(), true);
}

void DragTypeNumberBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    if (hasLabel)
    {
        auto top = r.removeFromTop (13);
        g.setColour (juce::Colour (0xfff2f4f8));
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        g.drawText (title, top, juce::Justification::centred, false);
    }

    if (! isEditingText)
    {
        auto box = r.reduced (1, 1).toFloat();
        float corner = (styleId == 2 || styleId == 5 || styleId == 7) ? box.getHeight() * 0.5f
                     : ((styleId % 2 == 0) ? 2.0f : 4.0f);
        g.setColour (juce::Colour (0xff07080a));
        g.fillRoundedRectangle (box, corner);
        drawSketchRect (g, box, corner, (styleId == 4 || styleId == 8) ? 1.8f : 1.25f, accentCol);

        float norm = static_cast<float> ((currentValue - minVal) / std::max (1e-6, maxVal - minVal));
        if (styleId == 3 || styleId == 7)
        {
            g.setColour (accentCol);
            g.fillEllipse (box.getX() + 5.0f, box.getCentreY() - 2.5f, 5.0f, 5.0f);
        }
        else if (styleId == 4 || styleId == 9)
        {
            g.setColour (accentCol);
            g.fillRect (box.getX() + 3.0f, box.getBottom() - 2.5f, std::max (2.0f, (box.getWidth() - 6.0f) * norm), 2.0f);
        }

        g.setColour (juce::Colour (0xfff2f4f8));
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText (formatDisplay(), box, juce::Justification::centred, true);
    }
}

void DragTypeNumberBox::resized()
{
    auto r = getLocalBounds();
    if (hasLabel) r.removeFromTop (13);
    editor.setBounds (r.reduced (1, 1));
}

void DragTypeNumberBox::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown()) { openTypeEditor(); return; }
    dragStartY = e.y;
    dragStartValue = currentValue;
}

void DragTypeNumberBox::mouseDrag (const juce::MouseEvent& e)
{
    if (isEditingText || e.mods.isRightButtonDown()) return;
    int dy = dragStartY - e.y;
    if (std::abs (dy) < 2) return;
    double span = maxVal - minVal;
    double sensitivity = e.mods.isShiftDown() ? 0.0015 : 0.006;
    setValue (dragStartValue + dy * span * sensitivity, true);
}

void DragTypeNumberBox::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! e.mods.isRightButtonDown()) setValue (defaultVal, true);
}

// ---------------- GearIconButton & EditModeIconButton ----------------
GearIconButton::GearIconButton() : juce::Button ("Settings") {}

void GearIconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (down ? juce::Colour (0xff2a2f38) : (over ? juce::Colour (0xff1b1e24) : juce::Colour (0xff07080a)));
    g.fillRoundedRectangle (r, 4.0f);
    drawSketchRect (g, r.reduced (0.5f), 4.0f, 1.25f, juce::Colour (0xfff2f4f8));

    float cx = r.getCentreX();
    float cy = r.getCentreY();
    float outerR = std::min (r.getWidth(), r.getHeight()) * 0.34f;
    float innerR = outerR * 0.68f;
    float holeR  = outerR * 0.32f;

    juce::Path gear;
    int teeth = 8;
    for (int i = 0; i < teeth * 2; ++i)
    {
        float ang = (juce::MathConstants<float>::twoPi * i) / (teeth * 2);
        float rad = (i % 2 == 0) ? outerR : innerR;
        float px = cx + rad * std::cos (ang);
        float py = cy + rad * std::sin (ang);
        if (i == 0) gear.startNewSubPath (px, py); else gear.lineTo (px, py);
    }
    gear.closeSubPath();

    g.setColour (juce::Colour (0xfff2f4f8));
    g.fillPath (gear);
    g.setColour (juce::Colour (0xff07080a));
    g.fillEllipse (cx - holeR, cy - holeR, holeR * 2.0f, holeR * 2.0f);
}

EditModeIconButton::EditModeIconButton() : juce::Button ("EditMode") { setClickingTogglesState (true); }

void EditModeIconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat();
    bool on = getToggleState();
    g.setColour (on ? juce::Colour (0xfff2f4f8) : (down || over ? juce::Colour (0xff1b1e24) : juce::Colour (0xff07080a)));
    g.fillRoundedRectangle (r, 4.0f);
    drawSketchRect (g, r.reduced (0.5f), 4.0f, 1.25f, juce::Colour (0xfff2f4f8));

    juce::Colour ink = on ? juce::Colour (0xff07080a) : juce::Colour (0xfff2f4f8);
    g.setColour (ink);
    float left = r.getX() + 6.0f;
    float right = r.getRight() - 6.0f;
    float ys[3] = { r.getY() + r.getHeight() * 0.28f, r.getY() + r.getHeight() * 0.50f, r.getY() + r.getHeight() * 0.72f };
    float xs[3] = { left + (right - left) * 0.35f, left + (right - left) * 0.68f, left + (right - left) * 0.45f };
    for (int i = 0; i < 3; ++i)
    {
        g.drawLine (left, ys[i], right, ys[i], 1.5f);
        g.fillEllipse (xs[i] - 3.0f, ys[i] - 3.0f, 6.0f, 6.0f);
    }
}

// ---------------- StyledSwitchButton ----------------
StyledSwitchButton::StyledSwitchButton (const juce::String& text, bool isToggleable) : juce::TextButton (text)
{
    setClickingTogglesState (isToggleable);
}

void StyledSwitchButton::setStyleAndColour (int id, juce::Colour activeC)
{
    styleId = juce::jlimit (1, 10, id);
    activeCol = activeC;
    repaint();
}

void StyledSwitchButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat();
    bool on = getToggleState() || (down && ! getClickingTogglesState());
    float corner = (styleId == 2 || styleId == 5 || styleId == 7) ? r.getHeight() * 0.5f
                 : ((styleId % 2 == 0) ? 2.0f : 4.0f);

    juce::Colour bg = on ? activeCol : (over ? juce::Colour (0xff171a20) : juce::Colour (0xff07080a));
    g.setColour (bg);
    g.fillRoundedRectangle (r, corner);

    drawSketchRect (g, r.reduced (0.5f), corner, (styleId == 4 || styleId == 8) ? 1.8f : 1.25f,
                    on ? juce::Colours::white : juce::Colour (0xfff2f4f8));

    if (styleId == 3 || styleId == 7)
    {
        g.setColour (on ? juce::Colour (0xff07080a) : juce::Colour (0xff9aa4b5));
        g.fillEllipse (r.getX() + 4.0f, r.getCentreY() - 2.5f, 5.0f, 5.0f);
    }
    else if (styleId == 4 || styleId == 9)
    {
        g.setColour (on ? juce::Colour (0xff07080a) : juce::Colour (0xff9aa4b5));
        g.fillRect (r.getX() + 4.0f, r.getBottom() - 3.0f, r.getWidth() - 8.0f, 2.0f);
    }
    else if (styleId == 6 || styleId == 10)
    {
        g.setColour (on ? juce::Colour (0xff07080a) : juce::Colour (0xff9aa4b5));
        g.fillRect (r.getX() + 4.0f, r.getY() + 2.0f, r.getWidth() - 8.0f, 2.0f);
    }

    bool brightFill = on && (activeCol.getBrightness() > 0.45f);
    g.setColour (brightFill ? juce::Colour (0xff050608) : juce::Colour (0xfff2f4f8));
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    auto textRect = (styleId == 3 || styleId == 7) ? r.withTrimmedLeft (8.0f) : r;
    g.drawText (getButtonText(), textRect, juce::Justification::centred, true);
}

// ---------------- RightKinetikScaleButton ----------------
RightKinetikScaleButton::RightKinetikScaleButton (RythmeAudioProcessor& p, std::function<void(float)> onScaleSelected)
    : processor (p), onScale (std::move (onScaleSelected)) {}

void RightKinetikScaleButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff07080a));
    g.fillRoundedRectangle (r, 4.0f);
    drawSketchRect (g, r.reduced (0.5f), 4.0f, 1.3f, juce::Colour (0xfff2f4f8));

    g.setColour (juce::Colour (0xfff2f4f8));
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    juce::String txt = processor.pluginTitleText.isEmpty() ? "KINETIK" : processor.pluginTitleText;
    g.drawText (txt, r.reduced (6.0f, 0.0f), juce::Justification::centred, true);
}

void RightKinetikScaleButton::mouseDown (const juce::MouseEvent&)
{
    juce::PopupMenu menu;
    menu.addItem (1, "50%");
    menu.addItem (2, "75%");
    menu.addItem (3, "100% (Default)");
    menu.addItem (4, "120%");
    menu.addItem (5, "150%");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
        [this] (int res)
        {
            if (! onScale) return;
            if (res == 1) onScale (0.50f);
            else if (res == 2) onScale (0.75f);
            else if (res == 3) onScale (1.00f);
            else if (res == 4) onScale (1.20f);
            else if (res == 5) onScale (1.50f);
        });
}

// ---------------- PresetBarWidget ----------------
PresetBarWidget::PresetBarWidget (RythmeAudioProcessor& p,
                                std::function<void()> onSaveClicked,
                                std::function<void(const juce::File&)> onLoadFile,
                                std::function<void()> onSetRootFolder)
    : processor (p), saveCb (std::move (onSaveClicked)), loadFileCb (std::move (onLoadFile)), setRootCb (std::move (onSetRootFolder))
{
    dropdownBtn.setButtonText ("v");
    dropdownBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1014));
    dropdownBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xfff2f4f8));
    dropdownBtn.onClick = [this]() { openCascadingPresetMenu(); };
    addAndMakeVisible (dropdownBtn);

    libNameEditor.setMultiLine (false);
    libNameEditor.setJustification (juce::Justification::centred);
    libNameEditor.setText (processor.libraryNameText.isEmpty() ? "KINETIK" : processor.libraryNameText, juce::dontSendNotification);
    libNameEditor.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    libNameEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff07080a));
    libNameEditor.setColour (juce::TextEditor::textColourId, juce::Colour (0xfff2f4f8));
    libNameEditor.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    libNameEditor.onTextChange = [this]()
    {
        processor.libraryNameText = libNameEditor.getText().trim();
        processor.currentPresetName = processor.libraryNameText;
    };
    addAndMakeVisible (libNameEditor);

    saveIconBtn.setButtonText ("[S]");
    saveIconBtn.setTooltip ("Save Preset");
    saveIconBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff0e1014));
    saveIconBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xfff2f4f8));
    saveIconBtn.onClick = [this]() { if (saveCb) saveCb(); };
    addAndMakeVisible (saveIconBtn);
}

void PresetBarWidget::refreshTextFromProcessor()
{
    libNameEditor.setText (processor.libraryNameText.isEmpty() ? "KINETIK" : processor.libraryNameText, juce::dontSendNotification);
}

void PresetBarWidget::buildFolderSubMenu (juce::PopupMenu& menu, const juce::File& dir, int& nextId, std::map<int, juce::File>& idMap)
{
    if (! dir.isDirectory()) return;

    juce::Array<juce::File> subDirs;
    dir.findChildFiles (subDirs, juce::File::findDirectories, false);
    subDirs.sort();

    for (const auto& sd : subDirs)
    {
        juce::PopupMenu sub;
        buildFolderSubMenu (sub, sd, nextId, idMap);
        menu.addSubMenu (sd.getFileName(), sub);
    }

    juce::Array<juce::File> files;
    dir.findChildFiles (files, juce::File::findFiles, false, "*.mn6x;*.wav;*.aiff;*.flac");
    files.sort();
    for (const auto& f : files)
    {
        int id = nextId++;
        idMap[id] = f;
        menu.addItem (id, f.getFileNameWithoutExtension());
    }
}

void PresetBarWidget::openCascadingPresetMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Set Root Library Folder...");
    menu.addSeparator();

    int nextId = 100;
    std::map<int, juce::File> idMap;

    if (processor.rootLibraryFolder.isDirectory())
    {
        buildFolderSubMenu (menu, processor.rootLibraryFolder, nextId, idMap);
    }
    else
    {
        const juce::ScopedLock sl (processor.zoneLock);
        for (const auto& cat : processor.libraryCategories)
        {
            juce::PopupMenu sub;
            if (cat.folder.isDirectory())
                buildFolderSubMenu (sub, cat.folder, nextId, idMap);
            else
                for (const auto& item : cat.items)
                {
                    int id = nextId++;
                    idMap[id] = item;
                    sub.addItem (id, item.getFileNameWithoutExtension());
                }
            menu.addSubMenu (cat.name, sub);
        }
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&dropdownBtn),
        [this, idMap] (int res)
        {
            if (res == 1)
            {
                if (setRootCb) setRootCb();
                return;
            }
            auto it = idMap.find (res);
            if (it != idMap.end() && loadFileCb)
                loadFileCb (it->second);
        });
}

void PresetBarWidget::resized()
{
    auto r = getLocalBounds().reduced (1);
    dropdownBtn.setBounds (r.removeFromLeft (26));
    saveIconBtn.setBounds (r.removeFromRight (34));
    libNameEditor.setBounds (r.reduced (2, 0));
}

void PresetBarWidget::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff07080a));
    g.fillRoundedRectangle (r, 4.0f);
    drawSketchRect (g, r.reduced (0.5f), 4.0f, 1.3f, juce::Colour (0xfff2f4f8));
}

// ---------------- AdsrGraphComponent ----------------
AdsrGraphComponent::AdsrGraphComponent (RythmeAudioProcessor& p) : processor (p) {}

void AdsrGraphComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff07080a));
    g.fillRoundedRectangle (area, 4.0f);
    drawSketchRect (g, area.reduced (0.5f), 4.0f, 1.2f, juce::Colour (0xfff2f4f8));

    float atk = 0.10f, dec = 300.0f, sus = 85.0f, rel = 250.0f;
    {
        const juce::ScopedLock sl (processor.zoneLock);
        if (processor.selectedZoneIndex >= 0 && processor.selectedZoneIndex < static_cast<int> (processor.zones.size()))
        {
            const auto& z = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)];
            atk = z.attackMs; dec = z.decayMs; sus = z.sustainPct; rel = z.releaseMs;
        }
    }

    auto inner = area.reduced (8.0f, 6.0f);
    float w = inner.getWidth();
    float h = inner.getHeight();
    float x0 = inner.getX();
    float yBot = inner.getBottom();
    float yTop = inner.getY();

    float aW = juce::jlimit (6.0f, w * 0.28f, (atk / 2000.0f) * (w * 0.28f) + 6.0f);
    float dW = juce::jlimit (10.0f, w * 0.28f, (dec / 3000.0f) * (w * 0.28f) + 10.0f);
    float rW = juce::jlimit (10.0f, w * 0.28f, (rel / 3000.0f) * (w * 0.28f) + 10.0f);
    float sW = std::max (12.0f, w - (aW + dW + rW));

    float xA = x0 + aW;
    float xD = xA + dW;
    float xS = xD + sW;
    float xR = xS + rW;
    float ySus = yBot - (juce::jlimit (0.0f, 100.0f, sus) / 100.0f) * h;

    juce::Path envPath;
    envPath.startNewSubPath (x0, yBot);
    envPath.lineTo (xA, yTop);
    envPath.lineTo (xD, ySus);
    envPath.lineTo (xS, ySus);
    envPath.lineTo (xR, yBot);

    juce::Path fillPath (envPath);
    fillPath.lineTo (x0, yBot);
    fillPath.closeSubPath();

    g.setColour (juce::Colour (0x24ffffff));
    g.fillPath (fillPath);
    g.setColour (juce::Colour (0xfff2f4f8));
    g.strokePath (envPath, juce::PathStrokeType (1.8f));

    g.setColour (juce::Colours::white);
    g.fillEllipse (xA - 3.0f, yTop - 3.0f, 6.0f, 6.0f);
    g.fillEllipse (xD - 3.0f, ySus - 3.0f, 6.0f, 6.0f);
    g.fillEllipse (xS - 3.0f, ySus - 3.0f, 6.0f, 6.0f);
}

// ---------------- FabStyleFxDisplay ----------------
FabStyleFxDisplay::FabStyleFxDisplay (RythmeAudioProcessor& p) : processor (p) {}

void FabStyleFxDisplay::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff07080a));
    g.fillRoundedRectangle (area, 5.0f);
    drawSketchRect (g, area.reduced (0.5f), 5.0f, 1.2f, juce::Colour (0xfff2f4f8));

    SimpleFxSlot slotCopy;
    {
        const juce::ScopedLock sl (processor.zoneLock);
        if (processor.selectedGroupIndex >= 0 && processor.selectedGroupIndex < static_cast<int> (processor.groups.size()))
            slotCopy = processor.groups[static_cast<size_t> (processor.selectedGroupIndex)].fxSlots[static_cast<size_t> (juce::jlimit (0, 3, slotIdx))];
    }

    auto r = area.reduced (8.0f, 6.0f);
    g.setColour (juce::Colour (0xff1d2027));
    for (int i = 1; i <= 4; ++i)
        g.drawVerticalLine (static_cast<int> (r.getX() + (r.getWidth() * i) / 5.0f), r.getY(), r.getBottom());
    g.drawHorizontalLine (static_cast<int> (r.getCentreY()), r.getX(), r.getRight());

    std::array<float, RythmeAudioProcessor::scopeHistorySize> hist {};
    bool sounding = false;
    processor.copyScopeHistory (hist, sounding);
    if (sounding)
    {
        juce::Path wave;
        float midY = r.getCentreY();
        wave.startNewSubPath (r.getX(), midY);
        for (int i = 0; i < RythmeAudioProcessor::scopeHistorySize; ++i)
        {
            float px = r.getX() + (i / static_cast<float> (RythmeAudioProcessor::scopeHistorySize - 1)) * r.getWidth();
            float py = midY - hist[static_cast<size_t> (i)] * (r.getHeight() * 0.42f);
            wave.lineTo (px, py);
        }
        g.setColour (juce::Colour (0x55ffffff));
        g.strokePath (wave, juce::PathStrokeType (1.0f));
    }

    if (slotCopy.type == SimpleFxType::Off)
        return;

    juce::Path curve;
    juce::Colour curveCol = slotCopy.enabled ? juce::Colour (0xfff2f4f8) : juce::Colour (0xff78849b);

    int steps = 64;
    for (int i = 0; i <= steps; ++i)
    {
        float t = i / static_cast<float> (steps);
        float px = r.getX() + t * r.getWidth();
        float py = r.getCentreY();

        if (slotCopy.type == SimpleFxType::EQ)
        {
            float lowDrop  = (t < slotCopy.p1 * 0.35f) ? (1.0f - t / std::max (0.01f, slotCopy.p1 * 0.35f)) * 22.0f : 0.0f;
            float midBump  = std::exp (-std::pow ((t - slotCopy.p2) * 5.0f, 2.0f)) * ((slotCopy.p3 - 0.5f) * 34.0f);
            float highDrop = (t > 0.3f + slotCopy.p4 * 0.7f) ? ((t - (0.3f + slotCopy.p4 * 0.7f)) * 45.0f) : 0.0f;
            py = r.getCentreY() + lowDrop - midBump + highDrop;
        }
        else if (slotCopy.type == SimpleFxType::Compressor)
        {
            float thresh = 1.0f - slotCopy.p1;
            float outT = (t <= thresh) ? t : (thresh + (t - thresh) / (1.0f + slotCopy.p2 * 6.0f));
            py = r.getBottom() - outT * r.getHeight();
        }
        else if (slotCopy.type == SimpleFxType::Saturation)
        {
            float x = (t * 2.0f - 1.0f) * (1.0f + slotCopy.p1 * 4.0f);
            float y = std::tanh (x) / std::tanh (1.0f + slotCopy.p1 * 4.0f);
            py = r.getCentreY() - y * (r.getHeight() * 0.44f);
        }
        else
        {
            float decay = std::exp (-t * (1.5f + (1.0f - slotCopy.p2) * 4.0f)) * std::cos (t * 22.0f * (0.4f + slotCopy.p1));
            py = r.getCentreY() - decay * (r.getHeight() * 0.42f) * slotCopy.p5;
        }

        py = juce::jlimit (r.getY() + 2.0f, r.getBottom() - 2.0f, py);
        if (i == 0) curve.startNewSubPath (px, py); else curve.lineTo (px, py);
    }

    g.setColour (curveCol.withAlpha (0.20f));
    juce::Path fillC (curve);
    fillC.lineTo (r.getRight(), r.getBottom());
    fillC.lineTo (r.getX(), r.getBottom());
    fillC.closeSubPath();
    g.fillPath (fillC);

    g.setColour (curveCol);
    g.strokePath (curve, juce::PathStrokeType (2.0f));
}

// ---------------- LiveMainWaveDisplay (Clean sketch scope, no overlay text) ----------------
LiveMainWaveDisplay::LiveMainWaveDisplay (RythmeAudioProcessor& p) : processor (p) {}

void LiveMainWaveDisplay::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff07080a));
    g.fillRoundedRectangle (area, 5.0f);
    drawSketchRect (g, area.reduced (0.5f), 5.0f, 1.4f, juce::Colour (0xfff2f4f8));

    auto screen = area.reduced (7.0f, 6.0f);
    g.setColour (juce::Colour (0xff1d2027));
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine (static_cast<int> (screen.getX() + (screen.getWidth() * i) / 8.0f), screen.getY(), screen.getBottom());
    g.drawHorizontalLine (static_cast<int> (screen.getCentreY()), screen.getX(), screen.getRight());

    std::array<float, RythmeAudioProcessor::scopeHistorySize> hist {};
    bool isSounding = false;
    processor.copyScopeHistory (hist, isSounding);

    if (! isSounding)
        return;

    float midY = screen.getCentreY();
    float halfH = screen.getHeight() * 0.44f;
    juce::Path wave;
    wave.startNewSubPath (screen.getX(), midY);

    for (int i = 0; i < RythmeAudioProcessor::scopeHistorySize; ++i)
    {
        float x = screen.getX() + (i / static_cast<float> (RythmeAudioProcessor::scopeHistorySize - 1)) * screen.getWidth();
        float y = midY - hist[static_cast<size_t> (i)] * halfH;
        wave.lineTo (x, y);
    }

    g.setColour (juce::Colour (0x38ffffff));
    g.strokePath (wave, juce::PathStrokeType (3.2f));
    g.setColour (juce::Colour (0xfff2f4f8));
    g.strokePath (wave, juce::PathStrokeType (1.5f));
}

// ---------------- ColorPianoStrip ----------------
ColorPianoStrip::ColorPianoStrip (RythmeAudioProcessor& p) : processor (p) {}

void ColorPianoStrip::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll (juce::Colour (0xff07080a));
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
        bool isMapped = false;
        for (const auto& z : processor.zones)
            if (note >= z.lowKey && note <= z.highKey)
                isMapped = true;

        if (note == pressedNote || note == activeNote) g.setColour (juce::Colours::white);
        else if (isKs) g.setColour (note == activeKs ? juce::Colour (0xfff2f4f8) : juce::Colour (0xff8c95a6));
        else if (isMapped) g.setColour (isBlack ? juce::Colour (0xff8c95a6) : juce::Colour (0xfff2f4f8));
        else g.setColour (isBlack ? juce::Colour (0xff111318) : juce::Colour (0xfff2f4f8));

        g.fillRect (kr);
        g.setColour (juce::Colour (0xff07080a));
        g.drawRect (kr, 0.8f);

        if (n == 0)
        {
            int oct = note / 12;
            g.setColour (isBlack ? juce::Colour (0xffcfd5df) : juce::Colour (0xff090a0c));
            g.setFont (7.5f);
            g.drawText ("C" + juce::String (oct), kr.expanded (5.0f, 0.0f), juce::Justification::centredBottom, false);
        }
    }
    g.setColour (juce::Colour (0xfff2f4f8));
    g.drawRect (area.toFloat(), 1.3f);
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
    g.fillAll (juce::Colour (0xff07080a));
    drawSketchRect (g, area.toFloat().reduced (0.5f), 4.0f, 1.3f, juce::Colour (0xfff2f4f8));

    auto gridArea = getGridArea();
    auto keyArea = getKeyboardArea();

    int divs = readOnly ? 4 : (velGridDivs > 0 ? velGridDivs : 4);
    for (int i = 0; i <= divs; ++i)
    {
        float norm = i / static_cast<float> (divs);
        int y = gridArea.getBottom() - static_cast<int> (norm * gridArea.getHeight());
        g.setColour (juce::Colour (0xff232730));
        g.drawHorizontalLine (y, static_cast<float> (gridArea.getX()), static_cast<float> (gridArea.getRight()));
        if (! readOnly && (divs <= 8 || i % 2 == 0))
        {
            g.setColour (juce::Colour (0xffcfd5df));
            g.setFont (8.5f);
            g.drawText (juce::String (static_cast<int> (std::round (norm * 127.0f))), 2, y - 6, 22, 12, juce::Justification::centredRight, false);
        }
    }

    float keyW = gridArea.getWidth() / static_cast<float> (numKeys);
    g.setColour (juce::Colour (0xff262b34));
    for (int i = 0; i < numKeys; ++i)
        if ((startNote + i) % 12 == 0)
            g.drawVerticalLine (static_cast<int> (gridArea.getX() + i * keyW), static_cast<float> (gridArea.getY()), static_cast<float> (gridArea.getBottom()));

    const juce::ScopedLock sl (processor.zoneLock);
    sanitizeSelection();

    int selectedRootKey = -1;
    if (processor.selectedZoneIndex >= 0 && processor.selectedZoneIndex < static_cast<int> (processor.zones.size()))
        selectedRootKey = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)].rootKey;

    for (int i = 0; i < static_cast<int> (processor.zones.size()); ++i)
    {
        const auto& z = processor.zones[static_cast<size_t> (i)];
        if (! isZoneVisibleInMapper (z)) continue;

        auto r = getZoneRect (z, gridArea);
        bool isSelected = (! readOnly && selectedZoneIndices.count (i) > 0) || (readOnly && i == processor.selectedZoneIndex);

        g.setColour (isSelected ? juce::Colour (0x66ffffff) : juce::Colour (0x2effffff));
        g.fillRect (r);
        g.setColour (isSelected ? juce::Colours::white : juce::Colour (0xfff2f4f8));
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
        g.setColour (juce::Colour (0x30ffffff)); g.fillRect (mr);
        g.setColour (juce::Colours::white); g.drawRect (mr, 1.2f);
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
        bool isMapped = false;
        for (const auto& z : processor.zones)
            if (isZoneVisibleInMapper (z) && note >= z.lowKey && note <= z.highKey) isMapped = true;

        if (note == pressedKey || note == activeNote) g.setColour (juce::Colours::white);
        else if (isKs) g.setColour (note == activeKs ? juce::Colour (0xfff2f4f8) : juce::Colour (0xff8c95a6));
        else if (! readOnly && note == selectedRootKey) g.setColour (juce::Colour (0xffffffff));
        else if (isMapped) g.setColour (isBlack ? juce::Colour (0xff8c95a6) : juce::Colour (0xfff2f4f8));
        else g.setColour (isBlack ? juce::Colour (0xff111318) : juce::Colour (0xfff2f4f8));

        g.fillRect (kr);
        g.setColour (juce::Colour (0xff07080a));
        g.drawRect (kr, 0.6f);

        if (! readOnly && note == selectedRootKey)
        {
            g.setColour (juce::Colours::black);
            g.setFont (juce::FontOptions (8.0f, juce::Font::bold));
            g.drawText ("R", kr.expanded (3.0f, 0.0f), juce::Justification::centredTop, false);
        }
        if (n == 0)
        {
            g.setColour (isBlack ? juce::Colour (0xffcfd5df) : juce::Colour (0xff090a0c));
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
    g.fillAll (juce::Colour (0xff07080a));
    drawSketchRect (g, area.toFloat().reduced (0.5f), 4.0f, 1.3f, juce::Colour (0xfff2f4f8));

    const juce::ScopedLock sl (processor.zoneLock);
    if (processor.zones.empty() || processor.selectedZoneIndex < 0 || processor.selectedZoneIndex >= static_cast<int> (processor.zones.size()))
        return;

    const auto& z = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)];
    if (z.layers.empty())
        return;

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

    g.setColour (juce::Colour (0x66000000));
    g.fillRect (static_cast<float> (area.getX()), static_cast<float> (area.getY()), startX - area.getX(), static_cast<float> (area.getHeight()));
    g.fillRect (endX, static_cast<float> (area.getY()), area.getRight() - endX, static_cast<float> (area.getHeight()));

    if (z.sampleLoop && lEndX > lStartX)
    {
        g.setColour (juce::Colour (0x28ffffff));
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
    g.setColour (juce::Colour (0xfff2f4f8));
    g.strokePath (p, juce::PathStrokeType (1.3f));

    float topLineY = midY - halfH;
    if (z.fadeInNorm > 0.001f)
        g.drawLine (startX, midY, fadeInX, topLineY, 1.5f);
    if (z.fadeOutNorm > 0.001f)
        g.drawLine (fadeOutX, topLineY, endX, midY, 1.5f);

    g.setColour (juce::Colours::white);
    g.fillEllipse (fadeInX - 5.0f, topLineY - 5.0f, 10.0f, 10.0f);
    g.fillEllipse (fadeOutX - 5.0f, topLineY - 5.0f, 10.0f, 10.0f);

    g.drawVerticalLine (static_cast<int> (startX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
    g.drawVerticalLine (static_cast<int> (endX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
    g.fillRect (startX, static_cast<float> (area.getY()), 14.0f, 13.0f);
    g.fillRect (endX - 14.0f, static_cast<float> (area.getY()), 14.0f, 13.0f);
    g.setColour (juce::Colours::black); g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
    g.drawText ("S", static_cast<int> (startX), area.getY(), 14, 13, juce::Justification::centred);
    g.drawText ("E", static_cast<int> (endX - 14.0f), area.getY(), 14, 13, juce::Justification::centred);

    if (z.sampleLoop)
    {
        g.setColour (juce::Colour (0xffcfd5df));
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
    g.fillAll (juce::Colour (0xff07080a));
    drawSketchRect (g, area.toFloat().reduced (0.5f), 4.0f, 1.4f, juce::Colour (0xfff2f4f8));

    int divs = processor.getGridDivisions();
    int beatStep = std::max (1, divs / 4);
    for (int i = 0; i < divs; ++i)
    {
        float x = area.getX() + (i / static_cast<float> (divs)) * area.getWidth();
        g.setColour (i % beatStep == 0 ? juce::Colour (0xff474e5c) : juce::Colour (0xff1d2026));
        g.drawVerticalLine (static_cast<int> (x), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
    }

    if (processor.apvts.getRawParameterValue ("seqLoop")->load() > 0.5f)
    {
        float px = area.getX() + processor.seqPlayheadNorm.load() * area.getWidth();
        g.setColour (juce::Colour (0xb3ffffff));
        g.drawVerticalLine (static_cast<int> (px), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
    }

    const juce::ScopedLock sl (processor.zoneLock);
    if (processor.zones.empty() || processor.selectedZoneIndex < 0 || processor.selectedZoneIndex >= static_cast<int> (processor.zones.size()))
        return;

    const auto& z = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)];
    for (int i = 0; i < static_cast<int> (z.seqEvents.size()); ++i)
    {
        const auto& ev = z.seqEvents[static_cast<size_t> (i)];
        float cx = area.getX() + ev.position * area.getWidth();
        float barH = ev.velocity * (area.getHeight() - 18.0f);
        float topY = area.getBottom() - 9.0f - barH;
        g.setColour (i == draggedEventIdx ? juce::Colours::white : juce::Colour (0xfff2f4f8));
        g.drawLine (cx, static_cast<float> (area.getBottom() - 9), cx, topY, 3.2f);
        g.fillEllipse (cx - 4.5f, static_cast<float> (area.getBottom() - 13.5f), 9.0f, 9.0f);
        g.fillEllipse (cx - 3.5f, topY - 3.5f, 7.0f, 7.0f);
    }
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

// ---------------- LayoutCustomizeOverlay ----------------
LayoutCustomizeOverlay::LayoutCustomizeOverlay (std::function<void(const juce::String&)> onDeleteKey)
    : onDeleteCallback (std::move (onDeleteKey)) {}

void LayoutCustomizeOverlay::setTargets (std::vector<TargetEntry> items)
{
    targets = std::move (items);
    repaint();
}

void LayoutCustomizeOverlay::deleteSelectedOrMarked()
{
    if (activeIdx >= 0 && activeIdx < static_cast<int> (targets.size()))
    {
        const auto& t = targets[static_cast<size_t> (activeIdx)];
        pendingDeleteKeys.insert (t.key);
        if (t.comp != nullptr) t.comp->setVisible (false);
        if (onDeleteCallback) onDeleteCallback (t.key);
        activeIdx = -1;
        repaint();
    }
}

void LayoutCustomizeOverlay::clearPendingDeletes()
{
    pendingDeleteKeys.clear();
    repaint();
}

int LayoutCustomizeOverlay::snap (int v) const
{
    if (gridStepPx <= 1) return v;
    return static_cast<int> (std::round (v / static_cast<float> (gridStepPx)) * gridStepPx);
}

void LayoutCustomizeOverlay::paint (juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState sss (g);
    g.reduceClipRegion (getLocalBounds());
    g.fillAll (juce::Colour (0x24000000));

    if (gridStepPx > 1)
    {
        g.setColour (juce::Colour (0x34ffffff));
        for (int x = gridStepPx; x < getWidth(); x += gridStepPx)
            g.drawVerticalLine (x, 0.0f, static_cast<float> (getHeight()));
        for (int y = gridStepPx; y < getHeight(); y += gridStepPx)
            g.drawHorizontalLine (y, 0.0f, static_cast<float> (getWidth()));
    }

    for (size_t i = 0; i < targets.size(); ++i)
    {
        const auto& t = targets[i];
        auto* c = t.comp;
        if (c == nullptr || ! c->isVisible() || c->getParentComponent() == nullptr) continue;
        if (pendingDeleteKeys.count (t.key) > 0) continue;

        auto b = getLocalArea (c->getParentComponent(), c->getBounds()).toFloat();
        auto clipped = b.getIntersection (getLocalBounds().toFloat());
        if (clipped.isEmpty()) continue;

        bool selected = (static_cast<int> (i) == activeIdx);
        g.setColour (selected ? juce::Colour (0x34ffffff) : juce::Colour (0x14ffffff));
        g.fillRect (clipped);
        g.setColour (selected ? juce::Colours::white : juce::Colour (0xffcfd5df));
        g.drawRect (clipped, 1.3f);

        juce::Path corner;
        corner.startNewSubPath (clipped.getRight() - 8.0f, clipped.getBottom());
        corner.lineTo (clipped.getRight(), clipped.getBottom() - 8.0f);
        corner.lineTo (clipped.getRight(), clipped.getBottom());
        corner.closeSubPath();
        g.setColour (juce::Colours::white);
        g.fillPath (corner);
    }
}

void LayoutCustomizeOverlay::mouseDown (const juce::MouseEvent& e)
{
    activeIdx = -1;
    isResizing = false;
    for (int i = static_cast<int> (targets.size()) - 1; i >= 0; --i)
    {
        const auto& t = targets[static_cast<size_t> (i)];
        auto* c = t.comp;
        if (c == nullptr || ! c->isVisible() || c->getParentComponent() == nullptr) continue;
        if (pendingDeleteKeys.count (t.key) > 0) continue;

        auto b = getLocalArea (c->getParentComponent(), c->getBounds());
        if (b.contains (e.getPosition()))
        {
            activeIdx = i;
            dragStart = e.getPosition();
            startLocalBounds = b;
            juce::Rectangle<int> brCorner (b.getRight() - 12, b.getBottom() - 12, 14, 14);
            isResizing = brCorner.contains (e.getPosition());
            setMouseCursor (isResizing ? juce::MouseCursor::BottomRightCornerResizeCursor : juce::MouseCursor::DraggingHandCursor);
            repaint();
            return;
        }
    }
}

void LayoutCustomizeOverlay::mouseDrag (const juce::MouseEvent& e)
{
    if (activeIdx < 0 || activeIdx >= static_cast<int> (targets.size())) return;
    auto* c = targets[static_cast<size_t> (activeIdx)].comp;
    if (c == nullptr || c->getParentComponent() == nullptr) return;

    int dx = e.x - dragStart.x;
    int dy = e.y - dragStart.y;
    auto nb = startLocalBounds;

    if (isResizing)
    {
        nb.setWidth  (juce::jlimit (22, getWidth() - nb.getX(), std::max (22, snap (startLocalBounds.getWidth() + dx))));
        nb.setHeight (juce::jlimit (16, getHeight() - nb.getY(), std::max (16, snap (startLocalBounds.getHeight() + dy))));
    }
    else
    {
        nb.setX (juce::jlimit (0, std::max (0, getWidth() - nb.getWidth()), snap (startLocalBounds.getX() + dx)));
        nb.setY (juce::jlimit (0, std::max (0, getHeight() - nb.getHeight()), snap (startLocalBounds.getY() + dy)));
    }

    auto parentBounds = c->getParentComponent()->getLocalArea (this, nb);
    c->setBounds (parentBounds);
    repaint();
}

void LayoutCustomizeOverlay::mouseUp (const juce::MouseEvent&)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

// ---------------- MainPluginView ----------------
void MainPluginView::applyAllStyles()
{
    int fStyle = juce::jlimit (1, 10, audioProcessor.faderStyleId);
    auto fCol = RythmeAudioProcessor::getPaletteColour (audioProcessor.faderColourIdx);
    StyledHorizontalFader* allFaders[] = {
        &shuffleFader, &timeRandFader, &velRandFader,
        &grpVolFader, &crossfadeFader,
        &fxP1, &fxP2, &fxP3, &fxP4, &fxP5
    };
    for (auto* hf : allFaders)
        hf->setStyleAndColour (fStyle, fCol);

    grpPanFader.setStyleAndColour (fStyle, fCol);
    panFader.setStyleAndColour (fStyle, fCol);

    int kStyle = juce::jlimit (1, 10, audioProcessor.knobStyleId);
    auto kCol = RythmeAudioProcessor::getPaletteColour (audioProcessor.knobColourIdx);
    DragTypeNumberBox* allBoxes[] = {
        &bpmBox, &pitchBox, &volBox,
        &attackBox, &decayBox, &sustainBox, &releaseBox
    };
    for (auto* bx : allBoxes)
        bx->setStyleAndColour (kStyle, kCol);

    int sStyle = juce::jlimit (1, 10, audioProcessor.switchStyleId);
    auto sCol = RythmeAudioProcessor::getPaletteColour (audioProcessor.switchColourIdx);
    StyledSwitchButton* allSwitches[] = {
        &playPauseBtn, &hostSyncBtn, &seqLoopBtn, &seqClearBtn,
        &addGroupBtn, &delGroupBtn, &groupMuteBtn, &groupSoloBtn, &selGroupOnlyBtn,
        &frrBtn, &fvlBtn, &pitchTrackBtn, &selectAllZonesBtn, &delZoneBtn, &autoGridBtn,
        &wavePlayBtn, &realRRBtn, &sampleLoopBtn, &autoLoopZeroBtn, &reverseBtn, &normalizeBtn, &resetZoneBtn,
        &fxBypassBtn, &fxEnableBtn,
        &gearDeleteBtn, &gearResetBtn, &gearDoneBtn
    };
    for (auto* sw : allSwitches)
        sw->setStyleAndColour (sStyle, sCol);

    presetBar.repaint();
    rightScaleBtn.repaint();
    repaint();
}

MainPluginView::MainPluginView (RythmeAudioProcessor& p, std::function<void(float)> onScaleSelected)
    : audioProcessor (p), scaleCallback (std::move (onScaleSelected)),
      presetBar (p,
                 [this]() { savePresetAction(); },
                 [this] (const juce::File& f) { loadLibraryItem (f); },
                 [this]() { chooseRootLibraryFolder(); }),
      rightScaleBtn (p, onScaleSelected),
      mainWaveDisplay (p), seqComp (p), editKeyMapper (p), waveEditor (p),
      adsrGraph (p), fxDisplay (p),
      customizeOverlay ([this] (const juce::String& deletedKey)
      {
          if (activeTab == ActiveTab::Main) mainHiddenKeys.insert (deletedKey);
          else editHiddenKeys.insert (deletedKey);
      }),
      bottomPiano (p)
{
    addAndMakeVisible (presetBar);
    addAndMakeVisible (rightScaleBtn);

    addAndMakeVisible (editModeBtn);
    editModeBtn.setTooltip ("Toggle EDIT Page");
    editModeBtn.onClick = [this]()
    {
        setActiveTab (editModeBtn.getToggleState() ? ActiveTab::Edit : ActiveTab::Main);
    };

    addAndMakeVisible (topGearBtn);
    topGearBtn.setTooltip ("Customize Layout, Styles & Colours");
    topGearBtn.onClick = [this]() { enterLayoutCustomizeMode (! layoutEditMode); };

    // ---------------- MAIN TAB ----------------
    addAndMakeVisible (mainWaveDisplay);

    rhythmSeqHeaderLbl.setText ("RHYTHM SEQUENCER", juce::dontSendNotification);
    rhythmSeqHeaderLbl.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    rhythmSeqHeaderLbl.setColour (juce::Label::textColourId, juce::Colour (0xfff2f4f8));
    addAndMakeVisible (rhythmSeqHeaderLbl);

    addAndMakeVisible (seqComp);

    addAndMakeVisible (playPauseBtn);
    playPauseBtn.onClick = [this]() { togglePlayPause(); };

    addAndMakeVisible (hostSyncBtn);
    hostSyncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "hostSync", hostSyncBtn);

    addAndMakeVisible (bpmBox);
    bpmBox.setValue (audioProcessor.apvts.getRawParameterValue ("internalBpm")->load(), false);
    bpmBox.onValueChanged = [this] (double v)
    {
        if (auto* p = audioProcessor.apvts.getParameter ("internalBpm"))
            p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (v)));
    };

    addAndMakeVisible (shuffleFader);
    shuffleFader.setValue (audioProcessor.apvts.getRawParameterValue ("shuffle")->load(), false);
    shuffleFader.onValueChanged = [this] (double v)
    {
        if (auto* p = audioProcessor.apvts.getParameter ("shuffle")) p->setValueNotifyingHost (static_cast<float> (v));
    };

    addAndMakeVisible (timeRandFader);
    timeRandFader.setValue (audioProcessor.apvts.getRawParameterValue ("timeRand")->load(), false);
    timeRandFader.onValueChanged = [this] (double v)
    {
        if (auto* p = audioProcessor.apvts.getParameter ("timeRand")) p->setValueNotifyingHost (static_cast<float> (v));
    };

    addAndMakeVisible (velRandFader);
    velRandFader.setValue (audioProcessor.apvts.getRawParameterValue ("velRand")->load(), false);
    velRandFader.onValueChanged = [this] (double v)
    {
        if (auto* p = audioProcessor.apvts.getParameter ("velRand")) p->setValueNotifyingHost (static_cast<float> (v));
    };

    gridLabel.setText ("Grid", juce::dontSendNotification);
    gridLabel.setJustificationType (juce::Justification::centred);
    gridLabel.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    gridLabel.setColour (juce::Label::textColourId, juce::Colour (0xfff2f4f8));
    addAndMakeVisible (gridLabel);

    addAndMakeVisible (gridBox);
    gridBox.addItemList ({ "1/4", "1/8", "1/12 (T)", "1/16", "1/24 (T)", "1/32", "1/48 (T)", "1/64" }, 1);
    gridAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (audioProcessor.apvts, "grid", gridBox);
    gridBox.onChange = [this]() { seqComp.repaint(); };

    addAndMakeVisible (seqLoopBtn);
    seqLoopAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "seqLoop", seqLoopBtn);

    addAndMakeVisible (seqClearBtn);
    seqClearBtn.onClick = [this]() { audioProcessor.clearSelectedZoneRhythm(); seqComp.repaint(); };

    // ---------------- EDIT TAB ----------------
    addAndMakeVisible (editViewport);
    editViewport.setViewedComponent (&editContent, false);
    editViewport.setScrollBarsShown (true, false);

    editContent.addAndMakeVisible (addGroupBtn);
    addGroupBtn.onClick = [this]()
    {
        audioProcessor.createEmptyGroup();
        refreshGroupBoxes(); syncGroupControls(); syncFxControls(); bottomPiano.repaint();
    };

    editContent.addAndMakeVisible (delGroupBtn);
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
    groupNameEdit.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff07080a));
    groupNameEdit.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xfff2f4f8));
    groupNameEdit.setColour (juce::TextEditor::textColourId, juce::Colour (0xfff2f4f8));
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

    editContent.addAndMakeVisible (grpVolFader);
    grpVolFader.onValueChanged = [this] (double v)
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedGroupIndex >= 0 && audioProcessor.selectedGroupIndex < static_cast<int> (audioProcessor.groups.size()))
            audioProcessor.groups[static_cast<size_t> (audioProcessor.selectedGroupIndex)].volumeDb100 = static_cast<float> (v);
    };

    editContent.addAndMakeVisible (grpPanFader);
    grpPanFader.onValueChanged = [this] (float v)
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedGroupIndex >= 0 && audioProcessor.selectedGroupIndex < static_cast<int> (audioProcessor.groups.size()))
            audioProcessor.groups[static_cast<size_t> (audioProcessor.selectedGroupIndex)].pan50 = v;
    };

    editContent.addAndMakeVisible (frrBtn);
    frrAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "frr", frrBtn);

    editContent.addAndMakeVisible (fvlBtn);
    fvlAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "fvl", fvlBtn);

    editContent.addAndMakeVisible (pitchTrackBtn);
    pitchTrackAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "pitchTrack", pitchTrackBtn);

    editContent.addAndMakeVisible (selectAllZonesBtn);
    selectAllZonesBtn.onClick = [this]() { editKeyMapper.selectAllZones(); };

    editContent.addAndMakeVisible (delZoneBtn);
    delZoneBtn.onClick = [this]() { deleteSelectedInEditTab(); };

    editContent.addAndMakeVisible (autoGridBtn);
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
    zoneColourBox.setSelectedId (12, juce::dontSendNotification);
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

    editContent.addAndMakeVisible (crossfadeFader);
    crossfadeFader.onValueChanged = [this] (double v)
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].loopCrossfade = static_cast<float> (v);
    };

    editContent.addAndMakeVisible (reverseBtn);
    reverseBtn.onClick = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].reverse = reverseBtn.getToggleState();
    };

    editContent.addAndMakeVisible (normalizeBtn);
    normalizeBtn.onClick = [this]() { audioProcessor.normalizeSelectedSample(); waveEditor.repaint(); };

    editContent.addAndMakeVisible (resetZoneBtn);
    resetZoneBtn.onClick = [this]()
    {
        audioProcessor.resetSelectedZoneDefaults(); syncPerZoneControls(); waveEditor.repaint(); adsrGraph.repaint();
    };

    editContent.addAndMakeVisible (waveEditor);

    editContent.addAndMakeVisible (pitchBox);
    pitchBox.onValueChanged = [this] (double v)
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].pitchSemi = static_cast<float> (v);
    };

    editContent.addAndMakeVisible (panFader);
    panFader.onValueChanged = [this] (float v)
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].pan50 = v;
    };

    editContent.addAndMakeVisible (volBox);
    volBox.onValueChanged = [this] (double v)
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].volumeDb100 = static_cast<float> (v);
    };

    editContent.addAndMakeVisible (attackBox);
    attackBox.onValueChanged = [this] (double v)
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].attackMs = static_cast<float> (v);
        adsrGraph.repaint();
    };

    editContent.addAndMakeVisible (decayBox);
    decayBox.onValueChanged = [this] (double v)
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].decayMs = static_cast<float> (v);
        adsrGraph.repaint();
    };

    editContent.addAndMakeVisible (sustainBox);
    sustainBox.onValueChanged = [this] (double v)
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].sustainPct = static_cast<float> (v);
        adsrGraph.repaint();
    };

    editContent.addAndMakeVisible (releaseBox);
    releaseBox.onValueChanged = [this] (double v)
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].releaseMs = static_cast<float> (v);
        adsrGraph.repaint();
    };

    editContent.addAndMakeVisible (adsrGraph);

    editContent.addAndMakeVisible (fxBypassBtn);
    fxBypassBtn.onClick = [this]() { audioProcessor.fxBypass.store (fxBypassBtn.getToggleState()); fxDisplay.repaint(); };

    editContent.addAndMakeVisible (fxSlotSelectBox);
    for (int s = 0; s < SampleGroup::numFxSlots; ++s) fxSlotSelectBox.addItem ("FX " + juce::String (s + 1), s + 1);
    fxSlotSelectBox.setSelectedId (1, juce::dontSendNotification);
    fxSlotSelectBox.onChange = [this]()
    {
        fxDisplay.setSlotIndex (juce::jlimit (0, 3, fxSlotSelectBox.getSelectedId() - 1));
        syncFxControls();
    };

    editContent.addAndMakeVisible (fxTypeBox);
    fxTypeBox.addItemList ({ "FX Off", "Pro-EQ", "Pro-Comp", "Saturn-Sat", "Timeless-Dly", "Pro-Rev" }, 1);
    fxTypeBox.onChange = [this]()
    {
        int sIdx = juce::jlimit (0, SampleGroup::numFxSlots - 1, fxSlotSelectBox.getSelectedId() - 1);
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedGroupIndex >= 0 && audioProcessor.selectedGroupIndex < static_cast<int> (audioProcessor.groups.size()))
        {
            auto& slot = audioProcessor.groups[static_cast<size_t> (audioProcessor.selectedGroupIndex)].fxSlots[static_cast<size_t> (sIdx)];
            slot.type = static_cast<SimpleFxType> (fxTypeBox.getSelectedId() - 1);
            if (slot.type == SimpleFxType::Saturation)
                slot.p1 = 1.0f;
            slot.prepare (audioProcessor.getCurrentSampleRate());
        }
        syncFxControls();
        fxDisplay.repaint();
    };

    editContent.addAndMakeVisible (fxEnableBtn);
    fxEnableBtn.onClick = [this]()
    {
        int sIdx = juce::jlimit (0, SampleGroup::numFxSlots - 1, fxSlotSelectBox.getSelectedId() - 1);
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedGroupIndex >= 0 && audioProcessor.selectedGroupIndex < static_cast<int> (audioProcessor.groups.size()))
            audioProcessor.groups[static_cast<size_t> (audioProcessor.selectedGroupIndex)].fxSlots[static_cast<size_t> (sIdx)].enabled = fxEnableBtn.getToggleState();
        fxDisplay.repaint();
    };

    editContent.addAndMakeVisible (fxDisplay);

    StyledHorizontalFader* fxFaders[5] = { &fxP1, &fxP2, &fxP3, &fxP4, &fxP5 };
    for (int k = 0; k < 5; ++k)
    {
        editContent.addAndMakeVisible (*fxFaders[k]);
        fxFaders[k]->onValueChanged = [this, k] (double v)
        {
            int sIdx = juce::jlimit (0, SampleGroup::numFxSlots - 1, fxSlotSelectBox.getSelectedId() - 1);
            const juce::ScopedLock sl (audioProcessor.zoneLock);
            if (audioProcessor.selectedGroupIndex >= 0 && audioProcessor.selectedGroupIndex < static_cast<int> (audioProcessor.groups.size()))
            {
                auto& slot = audioProcessor.groups[static_cast<size_t> (audioProcessor.selectedGroupIndex)].fxSlots[static_cast<size_t> (sIdx)];
                float fv = static_cast<float> (v);
                if (k == 0) slot.p1 = fv;
                else if (k == 1) slot.p2 = fv;
                else if (k == 2) slot.p3 = fv;
                else if (k == 3) slot.p4 = fv;
                else slot.p5 = fv;
            }
            fxDisplay.repaint();
        };
    }

    // ---------------- GEAR CUSTOMIZE CONTROLS ----------------
    addChildComponent (gearGridBox);
    gearGridBox.addItemList ({ "Grid: Free", "Grid: 10x", "Grid: 20x", "Grid: 30x", "Grid: 40x", "Grid: 50x" }, 1);
    gearGridBox.setSelectedId (3, juce::dontSendNotification);
    customizeOverlay.setGridStep (20);
    gearGridBox.onChange = [this]()
    {
        int id = gearGridBox.getSelectedId();
        customizeOverlay.setGridStep (id <= 1 ? 0 : (id - 1) * 10);
    };

    addChildComponent (gearFaderStyleBox);
    for (int i = 1; i <= 10; ++i) gearFaderStyleBox.addItem ("Fdr St " + juce::String (i), i);
    gearFaderStyleBox.setSelectedId (audioProcessor.faderStyleId, juce::dontSendNotification);
    gearFaderStyleBox.onChange = [this]()
    {
        audioProcessor.faderStyleId = juce::jlimit (1, 10, gearFaderStyleBox.getSelectedId());
        applyAllStyles();
    };

    addChildComponent (gearFaderColBox);
    for (int c = 0; c < RythmeAudioProcessor::numPaletteColours; ++c)
        gearFaderColBox.addItem ("Fdr: " + RythmeAudioProcessor::getPaletteColourName (c), c + 1);
    gearFaderColBox.setSelectedId (audioProcessor.faderColourIdx + 1, juce::dontSendNotification);
    gearFaderColBox.onChange = [this]()
    {
        audioProcessor.faderColourIdx = juce::jlimit (0, RythmeAudioProcessor::numPaletteColours - 1, gearFaderColBox.getSelectedId() - 1);
        applyAllStyles();
    };

    addChildComponent (gearKnobStyleBox);
    for (int i = 1; i <= 10; ++i) gearKnobStyleBox.addItem ("Knb St " + juce::String (i), i);
    gearKnobStyleBox.setSelectedId (audioProcessor.knobStyleId, juce::dontSendNotification);
    gearKnobStyleBox.onChange = [this]()
    {
        audioProcessor.knobStyleId = juce::jlimit (1, 10, gearKnobStyleBox.getSelectedId());
        applyAllStyles();
    };

    addChildComponent (gearKnobColBox);
    for (int c = 0; c < RythmeAudioProcessor::numPaletteColours; ++c)
        gearKnobColBox.addItem ("Knb: " + RythmeAudioProcessor::getPaletteColourName (c), c + 1);
    gearKnobColBox.setSelectedId (audioProcessor.knobColourIdx + 1, juce::dontSendNotification);
    gearKnobColBox.onChange = [this]()
    {
        audioProcessor.knobColourIdx = juce::jlimit (0, RythmeAudioProcessor::numPaletteColours - 1, gearKnobColBox.getSelectedId() - 1);
        applyAllStyles();
    };

    addChildComponent (gearSwitchStyleBox);
    for (int i = 1; i <= 10; ++i) gearSwitchStyleBox.addItem ("Sw St " + juce::String (i), i);
    gearSwitchStyleBox.setSelectedId (audioProcessor.switchStyleId, juce::dontSendNotification);
    gearSwitchStyleBox.onChange = [this]()
    {
        audioProcessor.switchStyleId = juce::jlimit (1, 10, gearSwitchStyleBox.getSelectedId());
        applyAllStyles();
    };

    addChildComponent (gearSwitchColBox);
    for (int c = 0; c < RythmeAudioProcessor::numPaletteColours; ++c)
        gearSwitchColBox.addItem ("Sw: " + RythmeAudioProcessor::getPaletteColourName (c), c + 1);
    gearSwitchColBox.setSelectedId (audioProcessor.switchColourIdx + 1, juce::dontSendNotification);
    gearSwitchColBox.onChange = [this]()
    {
        audioProcessor.switchColourIdx = juce::jlimit (0, RythmeAudioProcessor::numPaletteColours - 1, gearSwitchColBox.getSelectedId() - 1);
        applyAllStyles();
    };

    addChildComponent (gearDeleteBtn);
    gearDeleteBtn.onClick = [this]() { customizeOverlay.deleteSelectedOrMarked(); };

    addChildComponent (gearResetBtn);
    gearResetBtn.onClick = [this]()
    {
        if (activeTab == ActiveTab::Main)
        {
            mainCustomBounds.clear();
            mainHiddenKeys.clear();
        }
        else
        {
            editCustomBounds.clear();
            editHiddenKeys.clear();
        }
        customizeOverlay.clearPendingDeletes();
        resized();
        if (layoutEditMode) enterLayoutCustomizeMode (true);
        customizeOverlay.repaint();
    };

    addChildComponent (gearDoneBtn);
    gearDoneBtn.onClick = [this]() { enterLayoutCustomizeMode (false); };

    addChildComponent (customizeOverlay);

    addAndMakeVisible (bottomPiano);

    refreshGroupBoxes();
    syncGroupControls();
    syncFxControls();
    applyAllStyles();
    setActiveTab (ActiveTab::Main);
    syncPerZoneControls();
}

void MainPluginView::chooseRootLibraryFolder()
{
    auto startDir = audioProcessor.rootLibraryFolder.isDirectory()
        ? audioProcessor.rootLibraryFolder
        : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    fileChooser = std::make_unique<juce::FileChooser> ("Select Root Library Folder", startDir);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult().isDirectory())
                audioProcessor.setRootLibraryFolder (fc.getResult());
        });
}

void MainPluginView::savePresetAction()
{
    if (audioProcessor.libraryNameText.trim().isEmpty())
        audioProcessor.libraryNameText = "KINETIK";
    audioProcessor.currentPresetName = audioProcessor.libraryNameText.trim();

    if (audioProcessor.currentPresetFile != juce::File())
    {
        audioProcessor.savePresetMn6x (audioProcessor.currentPresetFile);
        return;
    }

    auto baseDir = audioProcessor.rootLibraryFolder.isDirectory()
        ? audioProcessor.rootLibraryFolder
        : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    auto defaultFile = baseDir.getChildFile (audioProcessor.currentPresetName + ".mn6x");

    fileChooser = std::make_unique<juce::FileChooser> ("Save Preset (.mn6x)", defaultFile, "*.mn6x");
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f != juce::File())
            {
                if (! f.hasFileExtension (".mn6x")) f = f.withFileExtension (".mn6x");
                audioProcessor.libraryNameText = f.getFileNameWithoutExtension();
                audioProcessor.currentPresetName = audioProcessor.libraryNameText;
                presetBar.refreshTextFromProcessor();
                audioProcessor.savePresetMn6x (f);
            }
        });
}

void MainPluginView::saveCustomizedBoundsFromOverlay()
{
    if (activeTab == ActiveTab::Main)
    {
        for (const auto& k : customizeOverlay.getPendingDeleteKeys())
            mainHiddenKeys.insert (k);
        customizeOverlay.clearPendingDeletes();

        auto saveIfVis = [this] (const juce::String& key, juce::Component& c)
        {
            if (mainHiddenKeys.count (key) == 0 && c.isVisible())
                mainCustomBounds[key] = c.getBounds();
        };
        saveIfVis ("wave",   mainWaveDisplay);
        saveIfVis ("seqHdr", rhythmSeqHeaderLbl);
        saveIfVis ("seq",    seqComp);
        saveIfVis ("play",   playPauseBtn);
        saveIfVis ("sync",   hostSyncBtn);
        saveIfVis ("bpm",    bpmBox);
        saveIfVis ("shuf",   shuffleFader);
        saveIfVis ("trnd",   timeRandFader);
        saveIfVis ("vrnd",   velRandFader);
        saveIfVis ("grid",   gridBox);
        saveIfVis ("loop",   seqLoopBtn);
        saveIfVis ("clr",    seqClearBtn);
    }
    else
    {
        for (const auto& k : customizeOverlay.getPendingDeleteKeys())
            editHiddenKeys.insert (k);
        customizeOverlay.clearPendingDeletes();

        auto saveIfVis = [this] (const juce::String& key, juce::Component& c)
        {
            if (editHiddenKeys.count (key) == 0 && c.isVisible())
                editCustomBounds[key] = c.getBounds();
        };
        saveIfVis ("addGrp",  addGroupBtn);
        saveIfVis ("delGrp",  delGroupBtn);
        saveIfVis ("grpSel",  groupSelectBox);
        saveIfVis ("grpName", groupNameEdit);
        saveIfVis ("grpM",    groupMuteBtn);
        saveIfVis ("grpS",    groupSoloBtn);
        saveIfVis ("selGrp",  selGroupOnlyBtn);
        saveIfVis ("grpKs",   groupKsBox);
        saveIfVis ("grpVol",  grpVolFader);
        saveIfVis ("grpPan",  grpPanFader);

        saveIfVis ("frr",     frrBtn);
        saveIfVis ("fvl",     fvlBtn);
        saveIfVis ("pitchT",  pitchTrackBtn);
        saveIfVis ("allZ",    selectAllZonesBtn);
        saveIfVis ("delZ",    delZoneBtn);
        saveIfVis ("autoG",   autoGridBtn);
        saveIfVis ("velG",    velGridBox);
        saveIfVis ("colZ",    zoneColourBox);
        saveIfVis ("mapper",  editKeyMapper);

        saveIfVis ("wPlay",   wavePlayBtn);
        saveIfVis ("trk",     trackSelectBox);
        saveIfVis ("rr",      realRRBtn);
        saveIfVis ("sLoop",   sampleLoopBtn);
        saveIfVis ("zCross",  autoLoopZeroBtn);
        saveIfVis ("rev",     reverseBtn);
        saveIfVis ("norm",    normalizeBtn);
        saveIfVis ("rstZ",    resetZoneBtn);
        saveIfVis ("waveEd",  waveEditor);
        saveIfVis ("xfade",   crossfadeFader);

        saveIfVis ("pitch",   pitchBox);
        saveIfVis ("pan",     panFader);
        saveIfVis ("vol",     volBox);
        saveIfVis ("atk",     attackBox);
        saveIfVis ("dec",     decayBox);
        saveIfVis ("sus",     sustainBox);
        saveIfVis ("rel",     releaseBox);
        saveIfVis ("adsrG",   adsrGraph);

        saveIfVis ("fxSlot",  fxSlotSelectBox);
        saveIfVis ("fxType",  fxTypeBox);
        saveIfVis ("fxEn",    fxEnableBtn);
        saveIfVis ("fxByp",   fxBypassBtn);
        saveIfVis ("fxDisp",  fxDisplay);
        saveIfVis ("fxP1",    fxP1);
        saveIfVis ("fxP2",    fxP2);
        saveIfVis ("fxP3",    fxP3);
        saveIfVis ("fxP4",    fxP4);
        saveIfVis ("fxP5",    fxP5);
    }
}

void MainPluginView::enterLayoutCustomizeMode (bool enable)
{
    if (! enable && layoutEditMode)
        saveCustomizedBoundsFromOverlay();

    layoutEditMode = enable;
    gearGridBox.setVisible (enable);
    gearFaderStyleBox.setVisible (enable);
    gearFaderColBox.setVisible (enable);
    gearKnobStyleBox.setVisible (enable);
    gearKnobColBox.setVisible (enable);
    gearSwitchStyleBox.setVisible (enable);
    gearSwitchColBox.setVisible (enable);
    gearDeleteBtn.setVisible (enable);
    gearResetBtn.setVisible (enable);
    gearDoneBtn.setVisible (enable);
    customizeOverlay.setVisible (enable);

    resized();

    if (enable)
    {
        customizeOverlay.clearPendingDeletes();
        std::vector<LayoutCustomizeOverlay::TargetEntry> list;
        if (activeTab == ActiveTab::Main)
        {
            list = {
                { "wave",   &mainWaveDisplay },
                { "seqHdr", &rhythmSeqHeaderLbl },
                { "seq",    &seqComp },
                { "play",   &playPauseBtn },
                { "sync",   &hostSyncBtn },
                { "bpm",    &bpmBox },
                { "shuf",   &shuffleFader },
                { "trnd",   &timeRandFader },
                { "vrnd",   &velRandFader },
                { "grid",   &gridBox },
                { "loop",   &seqLoopBtn },
                { "clr",    &seqClearBtn }
            };
        }
        else
        {
            list = {
                { "addGrp",  &addGroupBtn },
                { "delGrp",  &delGroupBtn },
                { "grpSel",  &groupSelectBox },
                { "grpName", &groupNameEdit },
                { "grpM",    &groupMuteBtn },
                { "grpS",    &groupSoloBtn },
                { "selGrp",  &selGroupOnlyBtn },
                { "grpKs",   &groupKsBox },
                { "grpVol",  &grpVolFader },
                { "grpPan",  &grpPanFader },

                { "frr",     &frrBtn },
                { "fvl",     &fvlBtn },
                { "pitchT",  &pitchTrackBtn },
                { "allZ",    &selectAllZonesBtn },
                { "delZ",    &delZoneBtn },
                { "autoG",   &autoGridBtn },
                { "velG",    &velGridBox },
                { "colZ",    &zoneColourBox },
                { "mapper",  &editKeyMapper },

                { "wPlay",   &wavePlayBtn },
                { "trk",     &trackSelectBox },
                { "rr",      &realRRBtn },
                { "sLoop",   &sampleLoopBtn },
                { "zCross",  &autoLoopZeroBtn },
                { "rev",     &reverseBtn },
                { "norm",    &normalizeBtn },
                { "rstZ",    &resetZoneBtn },
                { "waveEd",  &waveEditor },
                { "xfade",   &crossfadeFader },

                { "pitch",   &pitchBox },
                { "pan",     &panFader },
                { "vol",     &volBox },
                { "atk",     &attackBox },
                { "dec",     &decayBox },
                { "sus",     &sustainBox },
                { "rel",     &releaseBox },
                { "adsrG",   &adsrGraph },

                { "fxSlot",  &fxSlotSelectBox },
                { "fxType",  &fxTypeBox },
                { "fxEn",    &fxEnableBtn },
                { "fxByp",   &fxBypassBtn },
                { "fxDisp",  &fxDisplay },
                { "fxP1",    &fxP1 },
                { "fxP2",    &fxP2 },
                { "fxP3",    &fxP3 },
                { "fxP4",    &fxP4 },
                { "fxP5",    &fxP5 }
            };
        }
        customizeOverlay.setTargets (std::move (list));
        customizeOverlay.toFront (false);
    }
    repaint();
}

void MainPluginView::loadLibraryItem (const juce::File& file)
{
    if (! file.existsAsFile()) return;
    if (file.hasFileExtension (".mn6x"))
    {
        audioProcessor.loadPresetMn6x (file);
        presetBar.refreshTextFromProcessor();
        gearFaderStyleBox.setSelectedId (audioProcessor.faderStyleId, juce::dontSendNotification);
        gearFaderColBox.setSelectedId (audioProcessor.faderColourIdx + 1, juce::dontSendNotification);
        gearKnobStyleBox.setSelectedId (audioProcessor.knobStyleId, juce::dontSendNotification);
        gearKnobColBox.setSelectedId (audioProcessor.knobColourIdx + 1, juce::dontSendNotification);
        gearSwitchStyleBox.setSelectedId (audioProcessor.switchStyleId, juce::dontSendNotification);
        gearSwitchColBox.setSelectedId (audioProcessor.switchColourIdx + 1, juce::dontSendNotification);
        audioProcessor.setLoadedItemInPanel (file.getFileNameWithoutExtension(), file, -1);
        refreshGroupBoxes(); syncGroupControls(); syncFxControls(); applyAllStyles();
    }
    else
    {
        int nextKey = juce::jlimit (0, 120, 60 + static_cast<int> (audioProcessor.zones.size()));
        audioProcessor.loadSampleIntoNewZone (file, nextKey, nextKey, nextKey);
        audioProcessor.libraryNameText = file.getFileNameWithoutExtension();
        presetBar.refreshTextFromProcessor();
        audioProcessor.setLoadedItemInPanel (file.getFileNameWithoutExtension(), file, nextKey);
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
    grpVolFader.setValue (g.volumeDb100, false);
    grpPanFader.setValue (g.pan50, false);
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

    StyledHorizontalFader* fxFaders[5] = { &fxP1, &fxP2, &fxP3, &fxP4, &fxP5 };
    float vals[5] = { slot.p1, slot.p2, slot.p3, slot.p4, slot.p5 };
    for (int k = 0; k < 5; ++k)
    {
        fxFaders[k]->setLabelText (SimpleFxSlot::getParamLabel (slot.type, k));
        fxFaders[k]->setValue (vals[k], false);
    }
    fxDisplay.repaint();
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

void MainPluginView::setActiveTab (ActiveTab tab)
{
    if (layoutEditMode) enterLayoutCustomizeMode (false);
    activeTab = tab;
    bool isMain = (tab == ActiveTab::Main);
    bool isEdit = (tab == ActiveTab::Edit);

    editModeBtn.setToggleState (isEdit, juce::dontSendNotification);

    mainWaveDisplay.setVisible (isMain && mainHiddenKeys.count ("wave") == 0);
    rhythmSeqHeaderLbl.setVisible (isMain && mainHiddenKeys.count ("seqHdr") == 0);
    seqComp.setVisible (isMain && mainHiddenKeys.count ("seq") == 0);
    playPauseBtn.setVisible (isMain && mainHiddenKeys.count ("play") == 0);
    hostSyncBtn.setVisible (isMain && mainHiddenKeys.count ("sync") == 0);
    bpmBox.setVisible (isMain && mainHiddenKeys.count ("bpm") == 0);
    shuffleFader.setVisible (isMain && mainHiddenKeys.count ("shuf") == 0);
    timeRandFader.setVisible (isMain && mainHiddenKeys.count ("trnd") == 0);
    velRandFader.setVisible (isMain && mainHiddenKeys.count ("vrnd") == 0);
    gridLabel.setVisible (isMain);
    gridBox.setVisible (isMain && mainHiddenKeys.count ("grid") == 0);
    seqLoopBtn.setVisible (isMain && mainHiddenKeys.count ("loop") == 0);
    seqClearBtn.setVisible (isMain && mainHiddenKeys.count ("clr") == 0);

    editViewport.setVisible (isEdit);

    resized();
    repaint();
}

void MainPluginView::togglePlayPause()
{
    bool next = ! audioProcessor.internalPlaying.load();
    audioProcessor.internalPlaying.store (next);
    playPauseBtn.setButtonText (next ? "PAUSE" : "PLAY");
    playPauseBtn.setToggleState (next, juce::dontSendNotification);
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
        zoneColourBox.setSelectedId (12, juce::dontSendNotification);
        adsrGraph.repaint();
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
    crossfadeFader.setValue (z.loopCrossfade, false);
    reverseBtn.setToggleState (z.reverse, juce::dontSendNotification);

    pitchBox.setValue (z.pitchSemi, false);
    panFader.setValue (z.pan50, false);
    volBox.setValue (z.volumeDb100, false);
    attackBox.setValue (z.attackMs, false);
    decayBox.setValue (z.decayMs, false);
    sustainBox.setValue (z.sustainPct, false);
    releaseBox.setValue (z.releaseMs, false);
    adsrGraph.repaint();
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
                presetBar.refreshTextFromProcessor();
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
            presetBar.refreshTextFromProcessor();
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
    if (fxDisplay.isVisible()) fxDisplay.repaint();
    bottomPiano.repaint();
}

static void applySavedOrDefault (juce::Component& c, const std::map<juce::String, juce::Rectangle<int>>& map,
                                 const std::set<juce::String>& hiddenSet, const juce::String& key, juce::Rectangle<int> defB)
{
    if (hiddenSet.count (key) > 0)
    {
        c.setVisible (false);
        return;
    }
    auto it = map.find (key);
    c.setBounds (it != map.end() ? it->second : defB);
}

void MainPluginView::layoutEditContent (int viewportW)
{
    int contentW = std::max (660, viewportW - 14);
    int y = 4;

    auto grpTop = juce::Rectangle<int> (4, y, contentW - 8, 28);
    applySavedOrDefault (addGroupBtn,     editCustomBounds, editHiddenKeys, "addGrp",  grpTop.removeFromLeft (52).reduced (1));
    applySavedOrDefault (delGroupBtn,     editCustomBounds, editHiddenKeys, "delGrp",  grpTop.removeFromLeft (56).reduced (1));
    applySavedOrDefault (groupSelectBox,  editCustomBounds, editHiddenKeys, "grpSel",  grpTop.removeFromLeft (96).reduced (1));
    applySavedOrDefault (groupNameEdit,   editCustomBounds, editHiddenKeys, "grpName", grpTop.removeFromLeft (100).reduced (2, 1));
    applySavedOrDefault (groupMuteBtn,    editCustomBounds, editHiddenKeys, "grpM",    grpTop.removeFromLeft (30).reduced (1));
    applySavedOrDefault (groupSoloBtn,    editCustomBounds, editHiddenKeys, "grpS",    grpTop.removeFromLeft (30).reduced (1));
    applySavedOrDefault (selGroupOnlyBtn, editCustomBounds, editHiddenKeys, "selGrp",  grpTop.removeFromLeft (58).reduced (1));
    applySavedOrDefault (groupKsBox,      editCustomBounds, editHiddenKeys, "grpKs",   grpTop.removeFromLeft (84).reduced (1));
    y += 32;

    auto grpCtrlRow = juce::Rectangle<int> (4, y, contentW - 8, 36);
    int halfGW = grpCtrlRow.getWidth() / 2;
    applySavedOrDefault (grpVolFader, editCustomBounds, editHiddenKeys, "grpVol", grpCtrlRow.removeFromLeft (halfGW).reduced (3, 0));
    applySavedOrDefault (grpPanFader, editCustomBounds, editHiddenKeys, "grpPan", grpCtrlRow.reduced (3, 0));
    y += 40;

    auto mapHdr = juce::Rectangle<int> (4, y, contentW - 8, 26);
    applySavedOrDefault (frrBtn,            editCustomBounds, editHiddenKeys, "frr",    mapHdr.removeFromLeft (46).reduced (1));
    applySavedOrDefault (fvlBtn,            editCustomBounds, editHiddenKeys, "fvl",    mapHdr.removeFromLeft (46).reduced (1));
    applySavedOrDefault (pitchTrackBtn,     editCustomBounds, editHiddenKeys, "pitchT", mapHdr.removeFromLeft (54).reduced (1));
    applySavedOrDefault (selectAllZonesBtn, editCustomBounds, editHiddenKeys, "allZ",   mapHdr.removeFromLeft (40).reduced (1));
    applySavedOrDefault (delZoneBtn,        editCustomBounds, editHiddenKeys, "delZ",   mapHdr.removeFromLeft (40).reduced (1));
    applySavedOrDefault (autoGridBtn,       editCustomBounds, editHiddenKeys, "autoG",  mapHdr.removeFromLeft (70).reduced (1));
    applySavedOrDefault (zoneColourBox,     editCustomBounds, editHiddenKeys, "colZ",   mapHdr.removeFromRight (108).reduced (1));
    applySavedOrDefault (velGridBox,        editCustomBounds, editHiddenKeys, "velG",   mapHdr.removeFromRight (82).reduced (1));
    y += 28;

    applySavedOrDefault (editKeyMapper, editCustomBounds, editHiddenKeys, "mapper", { 4, y, contentW - 8, 225 });
    y += 233;

    auto wavHdr = juce::Rectangle<int> (4, y, contentW - 8, 26);
    applySavedOrDefault (wavePlayBtn,     editCustomBounds, editHiddenKeys, "wPlay",  wavHdr.removeFromLeft (44).reduced (1));
    applySavedOrDefault (trackSelectBox,  editCustomBounds, editHiddenKeys, "trk",    wavHdr.removeFromLeft (146).reduced (1));
    applySavedOrDefault (realRRBtn,       editCustomBounds, editHiddenKeys, "rr",     wavHdr.removeFromLeft (36).reduced (1));
    applySavedOrDefault (sampleLoopBtn,   editCustomBounds, editHiddenKeys, "sLoop",  wavHdr.removeFromLeft (48).reduced (1));
    applySavedOrDefault (autoLoopZeroBtn, editCustomBounds, editHiddenKeys, "zCross", wavHdr.removeFromLeft (112).reduced (1));
    applySavedOrDefault (reverseBtn,      editCustomBounds, editHiddenKeys, "rev",    wavHdr.removeFromLeft (44).reduced (1));
    applySavedOrDefault (normalizeBtn,    editCustomBounds, editHiddenKeys, "norm",   wavHdr.removeFromLeft (52).reduced (1));
    applySavedOrDefault (resetZoneBtn,    editCustomBounds, editHiddenKeys, "rstZ",   wavHdr.removeFromRight (52).reduced (1));
    y += 28;

    applySavedOrDefault (waveEditor,     editCustomBounds, editHiddenKeys, "waveEd", { 4, y, contentW - 8, 165 });
    y += 171;

    applySavedOrDefault (crossfadeFader, editCustomBounds, editHiddenKeys, "xfade",  { 4, y, contentW - 8, 34 });
    y += 38;

    auto ppvRow = juce::Rectangle<int> (4, y, contentW - 8, 38);
    applySavedOrDefault (pitchBox, editCustomBounds, editHiddenKeys, "pitch", ppvRow.removeFromLeft (130).reduced (3, 0));
    applySavedOrDefault (volBox,   editCustomBounds, editHiddenKeys, "vol",   ppvRow.removeFromRight (130).reduced (3, 0));
    applySavedOrDefault (panFader, editCustomBounds, editHiddenKeys, "pan",   ppvRow.reduced (4, 0));
    y += 42;

    auto adsrRow = juce::Rectangle<int> (4, y, contentW - 8, 42);
    applySavedOrDefault (attackBox,  editCustomBounds, editHiddenKeys, "atk",   adsrRow.removeFromLeft (96).reduced (2, 0));
    applySavedOrDefault (decayBox,   editCustomBounds, editHiddenKeys, "dec",   adsrRow.removeFromLeft (96).reduced (2, 0));
    applySavedOrDefault (sustainBox, editCustomBounds, editHiddenKeys, "sus",   adsrRow.removeFromLeft (96).reduced (2, 0));
    applySavedOrDefault (releaseBox, editCustomBounds, editHiddenKeys, "rel",   adsrRow.removeFromLeft (96).reduced (2, 0));
    applySavedOrDefault (adsrGraph,  editCustomBounds, editHiddenKeys, "adsrG", adsrRow.reduced (4, 1));
    y += 52;

    auto fxHdr = juce::Rectangle<int> (4, y, contentW - 8, 28);
    applySavedOrDefault (fxSlotSelectBox, editCustomBounds, editHiddenKeys, "fxSlot", fxHdr.removeFromLeft (78).reduced (1));
    applySavedOrDefault (fxTypeBox,       editCustomBounds, editHiddenKeys, "fxType", fxHdr.removeFromLeft (112).reduced (1));
    applySavedOrDefault (fxEnableBtn,     editCustomBounds, editHiddenKeys, "fxEn",   fxHdr.removeFromLeft (48).reduced (1));
    applySavedOrDefault (fxBypassBtn,     editCustomBounds, editHiddenKeys, "fxByp",  fxHdr.removeFromLeft (58).reduced (1));
    y += 32;

    applySavedOrDefault (fxDisplay, editCustomBounds, editHiddenKeys, "fxDisp", { 4, y, contentW - 8, 86 });
    y += 90;

    auto fxRow1 = juce::Rectangle<int> (4, y, contentW - 8, 34);
    int fxW3 = fxRow1.getWidth() / 3;
    applySavedOrDefault (fxP1, editCustomBounds, editHiddenKeys, "fxP1", fxRow1.removeFromLeft (fxW3).reduced (3, 0));
    applySavedOrDefault (fxP2, editCustomBounds, editHiddenKeys, "fxP2", fxRow1.removeFromLeft (fxW3).reduced (3, 0));
    applySavedOrDefault (fxP3, editCustomBounds, editHiddenKeys, "fxP3", fxRow1.reduced (3, 0));
    y += 38;

    auto fxRow2 = juce::Rectangle<int> (4, y, contentW - 8, 34);
    int fxW2 = fxRow2.getWidth() / 2;
    applySavedOrDefault (fxP4, editCustomBounds, editHiddenKeys, "fxP4", fxRow2.removeFromLeft (fxW2).reduced (3, 0));
    applySavedOrDefault (fxP5, editCustomBounds, editHiddenKeys, "fxP5", fxRow2.reduced (3, 0));
    y += 44;

    int maxBottom = y + 8;
    for (int i = 0; i < editContent.getNumChildComponents(); ++i)
        maxBottom = std::max (maxBottom, editContent.getChildComponent (i)->getBottom() + 12);
    editContent.setSize (contentW, maxBottom);
}

void MainPluginView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff07080a));
    drawSketchRect (g, getLocalBounds().toFloat().reduced (3.0f), 6.0f, 1.4f, juce::Colour (0xfff2f4f8));

    g.setColour (juce::Colour (0xffd5dbe5));
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText (audioProcessor.getBuildVersionString(), getWidth() - 78, getHeight() - 13, 70, 12, juce::Justification::centredRight);

    if (activeTab == ActiveTab::Edit && editViewport.isVisible())
    {
        int divY = editViewport.getY() + 676 - editViewport.getViewPositionY();
        if (divY > editViewport.getY() && divY < editViewport.getBottom())
        {
            g.setColour (juce::Colour (0xfff2f4f8));
            g.drawHorizontalLine (divY, static_cast<float> (editViewport.getX() + 10), static_cast<float> (editViewport.getRight() - 20));
            g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
            g.drawText ("EFFECTS", editViewport.getX() + 14, divY + 3, 90, 14, juce::Justification::centredLeft);
        }
    }
}

void MainPluginView::resized()
{
    auto area = getLocalBounds().reduced (10, 8);
    auto bottomStrip = area.removeFromBottom (46);
    bottomPiano.setBounds (bottomStrip.removeFromTop (32).reduced (0, 1));

    // Top bar: Enlarged PresetBar (v + KINETIK + [S]) on the left/center,
    // and on the right: KINETIK scale button + Edit icon + Gear icon
    auto topBar = area.removeFromTop (34);
    topGearBtn.setBounds     (topBar.removeFromRight (32).reduced (1));
    editModeBtn.setBounds    (topBar.removeFromRight (32).reduced (1));
    topBar.removeFromRight (4);
    rightScaleBtn.setBounds  (topBar.removeFromRight (96).reduced (1));
    topBar.removeFromRight (8);
    presetBar.setBounds      (topBar.reduced (1));

    auto workArea = area;

    if (activeTab == ActiveTab::Main)
    {
        auto mainBody = area;
        mainBody.removeFromTop (6);
        auto defWave = mainBody.removeFromTop (168);
        mainBody.removeFromTop (6);
        auto seqHdr = mainBody.removeFromTop (20);
        auto defSeqHdr = seqHdr.removeFromLeft (200);
        mainBody.removeFromTop (3);
        auto defSeq = mainBody.removeFromTop (224);

        mainBody.removeFromTop (8);
        auto bottomCtrl = mainBody.removeFromTop (48);
        // Uniform rectangular buttons for PLAY, SYNC, LOOP, Clr with clean spacing
        auto defPlay = bottomCtrl.removeFromLeft (60).reduced (2, 4);
        auto defSync = bottomCtrl.removeFromLeft (58).reduced (2, 4);
        auto defBpm  = bottomCtrl.removeFromLeft (64).reduced (2, 2);
        auto defShuf = bottomCtrl.removeFromLeft (96).reduced (2, 2);
        auto defTrnd = bottomCtrl.removeFromLeft (96).reduced (2, 2);
        auto defVrnd = bottomCtrl.removeFromLeft (96).reduced (2, 2);

        auto gridArea = bottomCtrl.removeFromLeft (86).reduced (2, 2);
        auto defGridLbl = gridArea.removeFromTop (13);
        auto defGridBox = gridArea.removeFromTop (28).reduced (0, 1);
        gridLabel.setBounds (defGridLbl);

        auto defLoop = bottomCtrl.removeFromLeft (58).reduced (2, 4);
        auto defClr  = bottomCtrl.removeFromLeft (44).reduced (2, 4);

        applySavedOrDefault (mainWaveDisplay,    mainCustomBounds, mainHiddenKeys, "wave",   defWave);
        applySavedOrDefault (rhythmSeqHeaderLbl, mainCustomBounds, mainHiddenKeys, "seqHdr", defSeqHdr);
        applySavedOrDefault (seqComp,            mainCustomBounds, mainHiddenKeys, "seq",    defSeq);
        applySavedOrDefault (playPauseBtn,       mainCustomBounds, mainHiddenKeys, "play",   defPlay);
        applySavedOrDefault (hostSyncBtn,        mainCustomBounds, mainHiddenKeys, "sync",   defSync);
        applySavedOrDefault (bpmBox,             mainCustomBounds, mainHiddenKeys, "bpm",    defBpm);
        applySavedOrDefault (shuffleFader,       mainCustomBounds, mainHiddenKeys, "shuf",   defShuf);
        applySavedOrDefault (timeRandFader,      mainCustomBounds, mainHiddenKeys, "trnd",   defTrnd);
        applySavedOrDefault (velRandFader,       mainCustomBounds, mainHiddenKeys, "vrnd",   defVrnd);
        applySavedOrDefault (gridBox,            mainCustomBounds, mainHiddenKeys, "grid",   defGridBox);
        applySavedOrDefault (seqLoopBtn,         mainCustomBounds, mainHiddenKeys, "loop",   defLoop);
        applySavedOrDefault (seqClearBtn,        mainCustomBounds, mainHiddenKeys, "clr",    defClr);

        if (layoutEditMode)
        {
            auto barArea = workArea.removeFromTop (58);
            auto gRow1 = barArea.removeFromTop (27);
            gearGridBox.setBounds        (gRow1.removeFromLeft (92).reduced (1));
            gearFaderStyleBox.setBounds  (gRow1.removeFromLeft (92).reduced (1));
            gearFaderColBox.setBounds  (gRow1.removeFromLeft (120).reduced (1));
            gearKnobStyleBox.setBounds   (gRow1.removeFromLeft (92).reduced (1));
            gearKnobColBox.setBounds   (gRow1.removeFromLeft (120).reduced (1));

            barArea.removeFromTop (2);
            auto gRow2 = barArea.removeFromTop (27);
            gearSwitchStyleBox.setBounds (gRow2.removeFromLeft (92).reduced (1));
            gearSwitchColBox.setBounds (gRow2.removeFromLeft (120).reduced (1));
            gearDoneBtn.setBounds        (gRow2.removeFromRight (64).reduced (1));
            gearResetBtn.setBounds       (gRow2.removeFromRight (60).reduced (1));
            gearDeleteBtn.setBounds      (gRow2.removeFromRight (64).reduced (1));

            customizeOverlay.setBounds (workArea);
            customizeOverlay.toFront (false);
            gearGridBox.toFront (false);
            gearFaderStyleBox.toFront (false);
            gearFaderColBox.toFront (false);
            gearKnobStyleBox.toFront (false);
            gearKnobColBox.toFront (false);
            gearSwitchStyleBox.toFront (false);
            gearSwitchColBox.toFront (false);
            gearDeleteBtn.toFront (false);
            gearResetBtn.toFront (false);
            gearDoneBtn.toFront (false);
        }
    }
    else
    {
        auto editArea = area;
        if (layoutEditMode)
        {
            auto barArea = editArea.removeFromTop (58);
            auto gRow1 = barArea.removeFromTop (27);
            gearGridBox.setBounds        (gRow1.removeFromLeft (92).reduced (1));
            gearFaderStyleBox.setBounds  (gRow1.removeFromLeft (92).reduced (1));
            gearFaderColBox.setBounds  (gRow1.removeFromLeft (120).reduced (1));
            gearKnobStyleBox.setBounds   (gRow1.removeFromLeft (92).reduced (1));
            gearKnobColBox.setBounds   (gRow1.removeFromLeft (120).reduced (1));

            barArea.removeFromTop (2);
            auto gRow2 = barArea.removeFromTop (27);
            gearSwitchStyleBox.setBounds (gRow2.removeFromLeft (92).reduced (1));
            gearSwitchColBox.setBounds (gRow2.removeFromLeft (120).reduced (1));
            gearDoneBtn.setBounds        (gRow2.removeFromRight (64).reduced (1));
            gearResetBtn.setBounds       (gRow2.removeFromRight (60).reduced (1));
            gearDeleteBtn.setBounds      (gRow2.removeFromRight (64).reduced (1));
        }

        editArea.removeFromTop (6);
        editViewport.setBounds (editArea);
        layoutEditContent (editArea.getWidth());

        if (layoutEditMode)
        {
            customizeOverlay.setBounds (editArea);
            customizeOverlay.toFront (false);
            gearGridBox.toFront (false);
            gearFaderStyleBox.toFront (false);
            gearFaderColBox.toFront (false);
            gearKnobStyleBox.toFront (false);
            gearKnobColBox.toFront (false);
            gearSwitchStyleBox.toFront (false);
            gearSwitchColBox.toFront (false);
            gearDeleteBtn.toFront (false);
            gearResetBtn.toFront (false);
            gearDoneBtn.toFront (false);
        }
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
    startTimerHz (24);
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

void RythmeAudioProcessorEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff07080a)); }

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
