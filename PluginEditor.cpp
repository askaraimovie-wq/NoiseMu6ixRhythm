#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

static void drawSketchRect (juce::Graphics& g, juce::Rectangle<float> r, float corner, float lineW, juce::Colour col)
{
    g.setColour (col);
    g.drawRoundedRectangle (r, corner, lineW);
    g.setColour (col.withAlpha (0.42f));
    g.drawLine (r.getX() + 4.0f, r.getY() + 0.6f, r.getRight() - 4.0f, r.getY() + 0.6f, lineW * 0.7f);
}

static inline juce::Colour contrastInkFor (juce::Colour bg)
{
    return (bg.getPerceivedBrightness() > 0.56f) ? juce::Colour (0xff07080a) : juce::Colour (0xfff2f4f8);
}

// ==============================================================================
// KinetikLookAndFeel (Unified Theme-Aware Styling for All Controls & Menus)
// ==============================================================================
KinetikLookAndFeel::KinetikLookAndFeel()
{
    applyTheme (RythmeAudioProcessor::getThemePalette (0));
}

void KinetikLookAndFeel::applyTheme (const KinetikThemePalette& pal)
{
    theme = pal;
    setColour (juce::ComboBox::backgroundColourId, pal.bg);
    setColour (juce::ComboBox::outlineColourId, pal.border);
    setColour (juce::ComboBox::textColourId, pal.text);
    setColour (juce::ComboBox::arrowColourId, pal.text);

    setColour (juce::PopupMenu::backgroundColourId, pal.panel);
    setColour (juce::PopupMenu::textColourId, pal.text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, pal.accent);
    setColour (juce::PopupMenu::highlightedTextColourId, contrastInkFor (pal.accent));

    setColour (juce::TextEditor::backgroundColourId, pal.bg);
    setColour (juce::TextEditor::outlineColourId, pal.border);
    setColour (juce::TextEditor::focusedOutlineColourId, pal.accent);
    setColour (juce::TextEditor::textColourId, pal.text);
    setColour (juce::TextEditor::highlightColourId, pal.accent.withAlpha (0.35f));

    setColour (juce::TextButton::buttonColourId, pal.panel);
    setColour (juce::TextButton::buttonOnColourId, pal.accent);
    setColour (juce::TextButton::textColourOffId, pal.text);
    setColour (juce::TextButton::textColourOnId, contrastInkFor (pal.accent));
}

void KinetikLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height));
    g.setColour (isButtonDown ? theme.surface : theme.bg);
    g.fillRoundedRectangle (r, 4.0f);
    drawSketchRect (g, r.reduced (0.5f), 4.0f, 1.2f, theme.border);

    juce::Path arrow;
    float cx = buttonX + buttonW * 0.5f;
    float cy = buttonY + buttonH * 0.5f;
    arrow.startNewSubPath (cx - 3.5f, cy - 1.8f);
    arrow.lineTo (cx, cy + 2.2f);
    arrow.lineTo (cx + 3.5f, cy - 1.8f);
    g.setColour (box.isEnabled() ? theme.text : theme.mutedText);
    g.strokePath (arrow, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font KinetikLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (10.5f, juce::Font::bold));
}

void KinetikLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 1, std::max (1, box.getWidth() - 18), std::max (1, box.getHeight() - 2));
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

void KinetikLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height));
    g.setColour (theme.panel);
    g.fillRect (r);
    g.setColour (theme.border);
    g.drawRect (r, 1.2f);
}

void KinetikLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator,
                                            bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                                            const juce::String& text, const juce::String& shortcutKeyText,
                                            const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour (theme.grid);
        g.drawHorizontalLine (area.getCentreY(), static_cast<float> (area.getX() + 6), static_cast<float> (area.getRight() - 6));
        return;
    }

    auto r = area.reduced (2, 1);
    if (isHighlighted && isActive)
    {
        g.setColour (theme.accent);
        g.fillRoundedRectangle (r.toFloat(), 3.0f);
        g.setColour (contrastInkFor (theme.accent));
    }
    else
    {
        g.setColour (isActive ? theme.text : theme.mutedText);
    }

    g.setFont (juce::FontOptions (11.0f, isTicked ? juce::Font::bold : juce::Font::plain));
    auto textArea = r.reduced (8, 0);
    if (isTicked)
    {
        g.drawText ("*", textArea.removeFromLeft (12), juce::Justification::centredLeft, false);
    }
    if (hasSubMenu)
    {
        g.drawText (">", textArea.removeFromRight (12), juce::Justification::centredRight, false);
    }
    if (shortcutKeyText.isNotEmpty())
    {
        g.drawText (shortcutKeyText, textArea.removeFromRight (44), juce::Justification::centredRight, false);
    }
    g.drawText (text, textArea, juce::Justification::centredLeft, true);
}

void KinetikLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                               bool over, bool down)
{
    auto r = button.getLocalBounds().toFloat();
    bool on = button.getToggleState() || down;
    g.setColour (on ? theme.accent : (over ? theme.surface : theme.bg));
    g.fillRoundedRectangle (r, 4.0f);
    drawSketchRect (g, r.reduced (0.5f), 4.0f, 1.2f, theme.border);
}

void KinetikLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                         bool, bool down)
{
    bool on = button.getToggleState() || down;
    g.setColour (on ? contrastInkFor (theme.accent) : theme.text);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (button.getButtonText(), button.getLocalBounds().reduced (2, 0), juce::Justification::centred, true);
}

void KinetikLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor&)
{
    g.setColour (theme.bg);
    g.fillRoundedRectangle (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height), 3.0f);
}

void KinetikLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& ed)
{
    if (ed.findColour (juce::TextEditor::outlineColourId).isTransparent())
        return;
    auto r = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height));
    drawSketchRect (g, r.reduced (0.5f), 3.0f, ed.hasKeyboardFocus (true) ? 1.5f : 1.1f, theme.border);
}

void KinetikLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height,
                                        bool isVertical, int thumbStart, int thumbSize, bool isOver, bool isDown)
{
    g.setColour (theme.panel);
    g.fillRect (x, y, width, height);
    g.setColour (isDown ? theme.accent : (isOver ? theme.text : theme.mutedText));
    if (isVertical)
        g.fillRoundedRectangle (static_cast<float> (x + 2), static_cast<float> (thumbStart),
                                static_cast<float> (std::max (2, width - 4)), static_cast<float> (thumbSize), 3.0f);
    else
        g.fillRoundedRectangle (static_cast<float> (thumbStart), static_cast<float> (y + 2),
                                static_cast<float> (thumbSize), static_cast<float> (std::max (2, height - 4)), 3.0f);
}

// ==============================================================================
// StyledHorizontalFader
// ==============================================================================
StyledHorizontalFader::StyledHorizontalFader (const juce::String& labelText, double minV, double maxV, double defaultV, int decimals)
    : title (labelText), minVal (minV), maxVal (maxV), defaultVal (defaultV), numDecimals (decimals), currentValue (defaultV)
{
    theme = RythmeAudioProcessor::getThemePalette (0);
    editor.setMultiLine (false);
    editor.setJustification (juce::Justification::centredRight);
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

void StyledHorizontalFader::setThemePalette (const KinetikThemePalette& pal)
{
    theme = pal;
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
    g.setColour (theme.text);
    g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
    g.drawText (title, top.reduced (1, 0), juce::Justification::centredLeft, true);

    if (! isEditingText)
    {
        juce::String vStr = (numDecimals == 0) ? juce::String (static_cast<int> (std::round (currentValue)))
                                               : juce::String (currentValue, numDecimals);
        g.setColour (theme.mutedText);
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
        g.setColour (theme.bg); g.fillRoundedRectangle (tr, 3.0f);
        g.setColour (acc); g.fillRoundedRectangle (tr.withWidth (fillW), 3.0f);
        g.setColour (theme.text); g.fillEllipse (thumbX - 5.0f, tr.getCentreY() - 5.0f, 10.0f, 10.0f);
    }
    else if (styleId == 2)
    {
        auto line = tr.withSizeKeepingCentre (tr.getWidth(), 3.0f);
        g.setColour (theme.bg); g.fillRect (line);
        g.setColour (acc); g.fillRect (line.withWidth (fillW));
        g.setColour (theme.text); g.fillRoundedRectangle (thumbX - 3.5f, tr.getY() - 1.0f, 7.0f, tr.getHeight() + 2.0f, 2.0f);
    }
    else if (styleId == 3)
    {
        int segs = 14;
        float sw = tr.getWidth() / segs;
        for (int i = 0; i < segs; ++i)
        {
            bool lit = (i / static_cast<float> (segs)) < norm;
            g.setColour (lit ? acc : theme.surface);
            g.fillRect (tr.getX() + i * sw + 1.0f, tr.getY() + 1.0f, sw - 2.0f, tr.getHeight() - 2.0f);
        }
    }
    else if (styleId == 4)
    {
        g.setColour (theme.bg); g.fillRect (tr);
        g.setColour (acc.withAlpha (0.85f)); g.fillRect (tr.withWidth (fillW));
        g.setColour (theme.text); g.fillRect (thumbX - 1.5f, tr.getY(), 3.0f, tr.getHeight());
    }
    else if (styleId == 5)
    {
        g.setColour (theme.surface); g.fillRoundedRectangle (tr, tr.getHeight() * 0.5f);
        g.setColour (acc); g.fillRoundedRectangle (tr.withWidth (fillW), tr.getHeight() * 0.5f);
        juce::Path d;
        d.startNewSubPath (thumbX, tr.getY() - 1.0f); d.lineTo (thumbX + 6.0f, tr.getCentreY());
        d.lineTo (thumbX, tr.getBottom() + 1.0f); d.lineTo (thumbX - 6.0f, tr.getCentreY()); d.closeSubPath();
        g.setColour (theme.text); g.fillPath (d);
    }
    else if (styleId == 6)
    {
        g.setColour (theme.border); g.drawRect (tr, 1.2f);
        g.setColour (acc); g.fillRect (tr.reduced (2.0f).withWidth (std::max (0.0f, fillW - 4.0f)));
        g.setColour (theme.text); g.fillRect (thumbX - 5.0f, tr.getY() - 1.0f, 10.0f, tr.getHeight() + 2.0f);
        g.setColour (theme.bg); g.drawVerticalLine (static_cast<int> (thumbX), tr.getY(), tr.getBottom());
    }
    else if (styleId == 7)
    {
        auto line = tr.withSizeKeepingCentre (tr.getWidth(), 4.0f);
        g.setColour (theme.bg); g.fillRoundedRectangle (line, 2.0f);
        g.setColour (acc.withAlpha (0.35f)); g.fillRoundedRectangle (line.withWidth (fillW).expanded (0.0f, 2.0f), 3.0f);
        g.setColour (acc); g.fillRoundedRectangle (line.withWidth (fillW), 2.0f);
        g.setColour (theme.bg); g.fillEllipse (thumbX - 5.5f, tr.getCentreY() - 5.5f, 11.0f, 11.0f);
        g.setColour (acc); g.drawEllipse (thumbX - 5.5f, tr.getCentreY() - 5.5f, 11.0f, 11.0f, 2.0f);
    }
    else if (styleId == 8)
    {
        g.setColour (theme.bg); g.fillRect (tr);
        g.setColour (acc); g.fillRect (tr.withWidth (fillW));
        g.setColour (theme.border);
        for (int i = 1; i < 10; ++i)
        {
            float tx = tr.getX() + (tr.getWidth() * i) / 10.0f;
            g.drawVerticalLine (static_cast<int> (tx), tr.getY(), tr.getBottom());
        }
        g.setColour (theme.text); g.fillRect (thumbX - 2.0f, tr.getY() - 1.0f, 4.0f, tr.getHeight() + 2.0f);
    }
    else if (styleId == 9)
    {
        auto line = tr.withSizeKeepingCentre (tr.getWidth(), 2.0f);
        g.setColour (theme.grid); g.fillRect (line);
        g.setColour (acc); g.fillRect (line.withWidth (fillW));
        g.setColour (acc); g.drawEllipse (thumbX - 5.0f, tr.getCentreY() - 5.0f, 10.0f, 10.0f, 2.0f);
    }
    else
    {
        g.setColour (theme.bg); g.fillRoundedRectangle (tr, 4.0f);
        g.setColour (acc); g.fillRoundedRectangle (tr.withWidth (fillW), 4.0f);
        g.setColour (theme.border); g.drawRoundedRectangle (tr, 4.0f, 1.1f);
        g.setColour (theme.text); g.fillEllipse (thumbX - 3.0f, tr.getCentreY() - 3.0f, 6.0f, 6.0f);
    }
}

void StyledHorizontalFader::resized()
{
    if (isEditingText)
        editor.setBounds (getLocalBounds().removeFromTop (13).removeFromRight (46));
}

void StyledHorizontalFader::updateFromX (int mx)
{
    auto tr = getLocalBounds().reduced (3, 4);
    float norm = juce::jlimit (0.0f, 1.0f, (mx - tr.getX()) / static_cast<float> (std::max (1, tr.getWidth())));
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
    if (e.y <= 15) openTypeEditor();
    else setValue (defaultVal, true);
}

// ==============================================================================
// CenterZeroPanFader
// ==============================================================================
CenterZeroPanFader::CenterZeroPanFader (const juce::String& labelText) : title (labelText)
{
    theme = RythmeAudioProcessor::getThemePalette (0);
    editor.setMultiLine (false);
    editor.setJustification (juce::Justification::centredRight);
    editor.onReturnKey = [this]() { commitText(); };
    editor.onFocusLost = [this]() { commitText(); };
    addChildComponent (editor);
}

void CenterZeroPanFader::setValue (float v, bool notify)
{
    value = juce::jlimit (-50.0f, 50.0f, v);
    if (std::abs (value) < 0.8f) value = 0.0f;
    repaint();
    if (notify && onValueChanged) onValueChanged (value);
}

void CenterZeroPanFader::setStyleAndColour (int id, juce::Colour col)
{
    styleId = juce::jlimit (1, 10, id);
    accentCol = col;
    repaint();
}

void CenterZeroPanFader::setThemePalette (const KinetikThemePalette& pal)
{
    theme = pal;
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
    auto valArea = top.removeFromRight (38);
    g.setColour (theme.text);
    g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
    g.drawText (title, top.reduced (1, 0), juce::Justification::centredLeft, true);

    if (! isEditingText)
    {
        int iv = static_cast<int> (std::round (value));
        juce::String s = (iv == 0) ? "C" : (iv < 0 ? juce::String (std::abs (iv)) + "L" : juce::String (iv) + "R");
        g.setColour (theme.mutedText);
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.drawText (s, valArea, juce::Justification::centredRight, true);
    }

    auto track = r.reduced (4, 5).toFloat();
    g.setColour (theme.bg);
    g.fillRoundedRectangle (track, (styleId % 2 == 0) ? 1.5f : 3.5f);
    g.setColour (theme.border);
    g.drawRoundedRectangle (track, (styleId % 2 == 0) ? 1.5f : 3.5f, 1.0f);

    float cx = track.getCentreX();
    float norm = value / 50.0f;
    float thumbX = cx + norm * (track.getWidth() * 0.5f);

    g.setColour (theme.mutedText);
    g.drawVerticalLine (static_cast<int> (cx), track.getY() - 2.0f, track.getBottom() + 2.0f);

    g.setColour (accentCol);
    float x1 = std::min (cx, thumbX);
    float x2 = std::max (cx, thumbX);
    g.fillRect (x1, track.getY() + 1.0f, std::max (2.0f, x2 - x1), track.getHeight() - 2.0f);

    g.setColour (theme.text);
    if (styleId % 2 == 0)
        g.fillRect (thumbX - 3.5f, track.getY() - 2.0f, 7.0f, track.getHeight() + 4.0f);
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
    auto track = getLocalBounds().reduced (4, 5);
    float norm = juce::jlimit (-1.0f, 1.0f, (mx - track.getCentreX()) / static_cast<float> (std::max (1, track.getWidth() / 2)));
    setValue (norm * 50.0f, true);
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
    if (e.y <= 15) openTypeEditor();
    else setValue (0.0f, true);
}

// ==============================================================================
// DragTypeNumberBox
// ==============================================================================
DragTypeNumberBox::DragTypeNumberBox (const juce::String& labelText, const juce::String& unitSuffix,
                                      double minV, double maxV, double defaultV, int decimals, bool showTopLabel)
    : title (labelText), suffix (unitSuffix), minVal (minV), maxVal (maxV), defaultVal (defaultV),
      numDecimals (decimals), hasLabel (showTopLabel), currentValue (defaultV)
{
    theme = RythmeAudioProcessor::getThemePalette (0);
    editor.setMultiLine (false);
    editor.setJustification (juce::Justification::centred);
    editor.onReturnKey = [this]() { commitText(); };
    editor.onFocusLost = [this]() { commitText(); };
    addChildComponent (editor);
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

void DragTypeNumberBox::setThemePalette (const KinetikThemePalette& pal)
{
    theme = pal;
    repaint();
}

juce::String DragTypeNumberBox::formatDisplay() const
{
    if (numDecimals == 0) return juce::String (static_cast<int> (std::round (currentValue))) + suffix;
    return juce::String (currentValue, numDecimals) + suffix;
}

void DragTypeNumberBox::openTypeEditor()
{
    isEditingText = true;
    editor.setText (numDecimals == 0 ? juce::String (static_cast<int> (std::round (currentValue)))
                                     : juce::String (currentValue, numDecimals), false);
    auto r = getLocalBounds();
    if (hasLabel) r.removeFromTop (13);
    editor.setBounds (r.reduced (1));
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
        g.setColour (theme.text);
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        g.drawText (title, top, juce::Justification::centred, false);
    }

    if (! isEditingText)
    {
        auto box = r.reduced (1, 1).toFloat();
        float corner = (styleId == 2 || styleId == 5 || styleId == 7) ? box.getHeight() * 0.5f
                     : ((styleId % 2 == 0) ? 2.0f : 4.0f);
        g.setColour (theme.bg);
        g.fillRoundedRectangle (box, corner);
        drawSketchRect (g, box, corner, (styleId == 4 || styleId == 8) ? 1.8f : 1.25f, accentCol);

        float norm = static_cast<float> ((currentValue - minVal) / std::max (1e-6, maxVal - minVal));
        if (styleId == 3 || styleId == 6 || styleId == 9)
        {
            g.setColour (accentCol.withAlpha (0.24f));
            g.fillRoundedRectangle (box.reduced (2.0f).withWidth ((box.getWidth() - 4.0f) * norm), corner);
        }
        else if (styleId == 5 || styleId == 7 || styleId == 10)
        {
            float cx = box.getX() + 10.0f;
            float cy = box.getCentreY();
            float rad = 6.5f;
            float ang = -2.35f + norm * 4.70f;
            g.setColour (accentCol);
            g.drawEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, 1.3f);
            g.drawLine (cx, cy, cx + rad * std::sin (ang), cy - rad * std::cos (ang), 1.6f);
        }

        g.setColour (theme.text);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        auto txtBox = (styleId == 5 || styleId == 7 || styleId == 10) ? box.withTrimmedLeft (16.0f) : box;
        g.drawText (formatDisplay(), txtBox, juce::Justification::centred, true);
    }
}

void DragTypeNumberBox::resized()
{
    if (isEditingText)
    {
        auto r = getLocalBounds();
        if (hasLabel) r.removeFromTop (13);
        editor.setBounds (r.reduced (1));
    }
}

void DragTypeNumberBox::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown()) { openTypeEditor(); return; }
    dragStartY = e.y;
    dragStartValue = currentValue;
}

void DragTypeNumberBox::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown()) return;
    double span = maxVal - minVal;
    double speed = e.mods.isShiftDown() ? 0.15 : 1.0;
    double delta = ((dragStartY - e.y) / 140.0) * span * speed;
    setValue (dragStartValue + delta, true);
}

void DragTypeNumberBox::mouseDoubleClick (const juce::MouseEvent&) { openTypeEditor(); }

// ==============================================================================
// GearIconButton & EditModeIconButton & StyledSwitchButton
// ==============================================================================
GearIconButton::GearIconButton() : juce::Button ("Gear")
{
    theme = RythmeAudioProcessor::getThemePalette (0);
}

void GearIconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (down ? theme.accent : (over ? theme.surface : theme.bg));
    g.fillRoundedRectangle (r, 4.0f);
    drawSketchRect (g, r.reduced (0.5f), 4.0f, 1.25f, theme.border);

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
        if (i == 0) gear.startNewSubPath (px, py);
        else gear.lineTo (px, py);
    }
    gear.closeSubPath();

    juce::Colour ink = down ? contrastInkFor (theme.accent) : theme.text;
    g.setColour (ink);
    g.fillPath (gear);
    g.setColour (down ? theme.accent : theme.bg);
    g.fillEllipse (cx - holeR, cy - holeR, holeR * 2.0f, holeR * 2.0f);
}

EditModeIconButton::EditModeIconButton() : juce::Button ("EditMode")
{
    theme = RythmeAudioProcessor::getThemePalette (0);
    setClickingTogglesState (true);
}

void EditModeIconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat();
    bool on = getToggleState();
    g.setColour (on ? theme.accent : (down || over ? theme.surface : theme.bg));
    g.fillRoundedRectangle (r, 4.0f);
    drawSketchRect (g, r.reduced (0.5f), 4.0f, 1.25f, theme.border);

    juce::Colour ink = on ? contrastInkFor (theme.accent) : theme.text;
    g.setColour (ink);
    float left = r.getX() + 6.0f;
    float right = r.getRight() - 6.0f;
    float ys[3] = { r.getY() + r.getHeight() * 0.28f, r.getY() + r.getHeight() * 0.50f, r.getY() + r.getHeight() * 0.72f };
    float xs[3] = { left + (right - left) * 0.35f, left + (right - left) * 0.68f, left + (right - left) * 0.45f };
    for (int i = 0; i < 3; ++i)
    {
        g.drawLine (left, ys[i], right, ys[i], 1.5f);
        g.fillEllipse (xs[i] - 2.4f, ys[i] - 2.4f, 4.8f, 4.8f);
    }
}

StyledSwitchButton::StyledSwitchButton (const juce::String& text, bool isToggleable) : juce::TextButton (text)
{
    theme = RythmeAudioProcessor::getThemePalette (0);
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

    juce::Colour bg = on ? activeCol : (over ? theme.surface : theme.bg);
    g.setColour (bg);
    g.fillRoundedRectangle (r, corner);

    drawSketchRect (g, r.reduced (0.5f), corner, (styleId == 4 || styleId == 8) ? 1.8f : 1.25f,
                    on ? activeCol.brighter (0.35f) : theme.border);

    if (styleId == 3 || styleId == 7)
    {
        g.setColour (on ? contrastInkFor (bg) : theme.mutedText);
        g.fillEllipse (r.getX() + 4.0f, r.getCentreY() - 2.5f, 5.0f, 5.0f);
    }
    else if (styleId == 6 || styleId == 9)
    {
        g.setColour (on ? contrastInkFor (bg) : activeCol);
        g.fillRect (r.getX() + 3.0f, r.getBottom() - 3.5f, r.getWidth() - 6.0f, 2.0f);
    }

    g.setColour (on ? contrastInkFor (bg) : theme.text);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText (getButtonText(), r.reduced (2.0f, 0.0f), juce::Justification::centred, true);
}

// ==============================================================================
// LeftKinetikScaleButton (Big KINETIK Title + Small Current Version + Scale Menu)
// ==============================================================================
LeftKinetikScaleButton::LeftKinetikScaleButton (RythmeAudioProcessor& p, std::function<void(float)> onScaleSelected)
    : processor (p), onScale (std::move (onScaleSelected))
{
    theme = RythmeAudioProcessor::getThemePalette (0);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Click to change UI Size (50% - 150%)");
}

void LeftKinetikScaleButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme.bg);
    g.fillRoundedRectangle (r, 4.0f);
    drawSketchRect (g, r.reduced (0.5f), 4.0f, 1.35f, theme.border);

    auto inner = r.reduced (7.0f, 0.0f);
    auto verRect = inner.removeFromRight (38.0f);

    g.setColour (theme.text);
    g.setFont (juce::FontOptions (16.5f, juce::Font::bold));
    juce::String txt = processor.pluginTitleText.isEmpty() ? "KINETIK" : processor.pluginTitleText;
    g.drawText (txt, inner, juce::Justification::centredLeft, true);

    g.setColour (theme.mutedText);
    g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
    g.drawText (processor.getBuildVersionString(), verRect.withTrimmedTop (6.0f), juce::Justification::centredRight, false);
}

void LeftKinetikScaleButton::mouseDown (const juce::MouseEvent&)
{
    float cur = processor.uiScale;
    juce::PopupMenu menu;
    menu.addItem (1, "50%",            true, std::abs (cur - 0.50f) < 0.03f);
    menu.addItem (2, "75%",            true, std::abs (cur - 0.75f) < 0.03f);
    menu.addItem (3, "100% (Default)", true, std::abs (cur - 1.00f) < 0.03f);
    menu.addItem (4, "125%",           true, std::abs (cur - 1.25f) < 0.03f);
    menu.addItem (5, "150%",           true, std::abs (cur - 1.50f) < 0.03f);

    juce::Component::SafePointer<LeftKinetikScaleButton> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
        [safeThis] (int res)
        {
            if (safeThis == nullptr || ! safeThis->onScale || res == 0) return;
            float target = 1.0f;
            if (res == 1) target = 0.50f;
            else if (res == 2) target = 0.75f;
            else if (res == 3) target = 1.00f;
            else if (res == 4) target = 1.25f;
            else if (res == 5) target = 1.50f;

            // 60ms delay ensures FL Studio VST3 host has released popup focus before resizing
            juce::Timer::callAfterDelay (60, [safeThis, target]()
            {
                if (safeThis != nullptr && safeThis->onScale)
                    safeThis->onScale (target);
            });
        });
}

// ==============================================================================
// CpuRamBadgeComponent (Live Audio CPU % & Sample RAM MB)
// ==============================================================================
CpuRamBadgeComponent::CpuRamBadgeComponent (RythmeAudioProcessor& p) : processor (p)
{
    theme = RythmeAudioProcessor::getThemePalette (0);
}

void CpuRamBadgeComponent::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme.bg);
    g.fillRoundedRectangle (r, 4.0f);
    drawSketchRect (g, r.reduced (0.5f), 4.0f, 1.15f, theme.border);

    auto inner = r.reduced (4.0f, 2.0f);
    auto topRow = inner.removeFromTop (inner.getHeight() * 0.5f);

    float cpu = juce::jlimit (0.0f, 99.9f, processor.cpuUsagePercent.load());
    float ram = std::max (0.0f, processor.sampleRamMb.load());

    g.setColour (cpu > 55.0f ? juce::Colour (0xffee5253) : theme.text);
    g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
    g.drawText ("CPU " + juce::String (cpu, 1) + "%", topRow, juce::Justification::centred, false);

    g.setColour (theme.mutedText);
    g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
    juce::String ramStr = (ram >= 100.0f) ? ("RAM " + juce::String (static_cast<int> (std::round (ram))) + "MB")
                                          : ("RAM " + juce::String (ram, 1) + "MB");
    g.drawText (ramStr, inner, juce::Justification::centred, false);
}

// ==============================================================================
// UnifiedGridBox
// ==============================================================================
UnifiedGridBox::UnifiedGridBox()
{
    theme = RythmeAudioProcessor::getThemePalette (0);
    addAndMakeVisible (combo);
}

void UnifiedGridBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (13);
    g.setColour (theme.text);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText ("Grid", top, juce::Justification::centred, false);
}

void UnifiedGridBox::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (13);
    combo.setBounds (r.reduced (1, 1));
}

// ==============================================================================
// TopIoMeterAndMasterKnob (2 Stacked Horizontal Output Bars + 12 o'clock 0 dB Knob)
// ==============================================================================
TopIoMeterAndMasterKnob::TopIoMeterAndMasterKnob (RythmeAudioProcessor& p) : processor (p)
{
    theme = RythmeAudioProcessor::getThemePalette (0);
}

juce::Rectangle<float> TopIoMeterAndMasterKnob::getKnobBounds() const
{
    auto r = getLocalBounds().toFloat();
    float sz = std::min (22.0f, r.getHeight() - 2.0f);
    return juce::Rectangle<float> (r.getRight() - sz - 4.0f, r.getCentreY() - sz * 0.5f, sz, sz);
}

void TopIoMeterAndMasterKnob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme.bg);
    g.fillRoundedRectangle (r, 4.0f);
    drawSketchRect (g, r.reduced (0.5f), 4.0f, 1.15f, theme.border);

    auto knobR = getKnobBounds();
    auto meterArea = r.reduced (6.0f, 3.0f).withRight (knobR.getX() - 8.0f);
    float rowH = (meterArea.getHeight() - 3.0f) * 0.5f;

    auto topLeftBar  = meterArea.removeFromTop (rowH);
    meterArea.removeFromTop (3.0f);
    auto botRightBar = meterArea;

    float lvlL = std::pow (juce::jlimit (0.0f, 1.0f, processor.outputMeterL.load()), 0.58f);
    float lvlR = std::pow (juce::jlimit (0.0f, 1.0f, processor.outputMeterR.load()), 0.58f);

    auto drawMeterLine = [this, &g] (juce::Rectangle<float> bar, float lvl)
    {
        g.setColour (theme.surface);
        g.fillRoundedRectangle (bar, 1.5f);
        g.setColour (theme.border);
        g.drawRoundedRectangle (bar, 1.5f, 0.85f);

        int segs = 44;
        float sw = (bar.getWidth() - 4.0f) / segs;
        for (int i = 0; i < segs; ++i)
        {
            float thresh = (i + 0.4f) / static_cast<float> (segs);
            bool lit = (lvl >= thresh);
            g.setColour (lit ? theme.accent : theme.grid);
            g.fillRect (bar.getX() + 2.0f + i * sw, bar.getY() + 1.5f, std::max (1.5f, sw - 1.5f), std::max (1.5f, bar.getHeight() - 3.0f));
        }
    };

    drawMeterLine (topLeftBar, lvlL);
    drawMeterLine (botRightBar, lvlR);

    float db = juce::jlimit (-24.0f, 12.0f, processor.masterGainDb.load());
    float norm = (db <= 0.0f) ? (0.5f + (db / 24.0f) * 0.5f)
                              : (0.5f + (db / 12.0f) * 0.5f);
    norm = juce::jlimit (0.0f, 1.0f, norm);

    g.setColour (theme.surface);
    g.fillEllipse (knobR);
    g.setColour (theme.border);
    g.drawEllipse (knobR, 1.4f);

    float startAng = -juce::MathConstants<float>::pi * 0.75f;
    float endAng   =  juce::MathConstants<float>::pi * 0.75f;
    float valAng   = startAng + norm * (endAng - startAng);

    float cx = knobR.getCentreX();
    float cy = knobR.getCentreY();
    float rad = knobR.getWidth() * 0.5f;

    g.setColour (theme.mutedText);
    g.drawLine (cx, cy - rad - 1.0f, cx, cy - rad - 3.2f, 1.4f);

    g.setColour (theme.text);
    g.drawLine (cx, cy, cx + (rad - 2.0f) * std::sin (valAng), cy - (rad - 2.0f) * std::cos (valAng), 1.9f);
}

void TopIoMeterAndMasterKnob::mouseDown (const juce::MouseEvent& e)
{
    isDraggingKnob = getKnobBounds().expanded (5.0f).contains (e.position);
    if (isDraggingKnob)
    {
        dragStartY = e.y;
        dragStartDb = processor.masterGainDb.load();
    }
}

void TopIoMeterAndMasterKnob::mouseDrag (const juce::MouseEvent& e)
{
    if (! isDraggingKnob) return;
    float dy = static_cast<float> (dragStartY - e.y);
    float nextDb = juce::jlimit (-24.0f, 12.0f, dragStartDb + dy * 0.22f);
    if (std::abs (nextDb) < 0.45f) nextDb = 0.0f;
    processor.masterGainDb.store (nextDb);
    repaint();
}

void TopIoMeterAndMasterKnob::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (getKnobBounds().expanded (5.0f).contains (e.position))
    {
        processor.masterGainDb.store (0.0f);
        repaint();
    }
}

// ==============================================================================
// PitchModWheelsComponent (Dedicated Bottom P / M Labels so Thumb Never Covers Text)
// ==============================================================================
PitchModWheelsComponent::PitchModWheelsComponent (RythmeAudioProcessor& p) : processor (p)
{
    theme = RythmeAudioProcessor::getThemePalette (0);
}

void PitchModWheelsComponent::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme.bg);
    g.fillRoundedRectangle (r, 3.0f);
    drawSketchRect (g, r.reduced (0.5f), 3.0f, 1.2f, theme.border);

    auto inner = r.reduced (3.0f, 2.0f);
    auto lblRow = inner.removeFromBottom (9.0f);
    float w = (inner.getWidth() - 4.0f) * 0.5f;

    auto pitchBox = inner.removeFromLeft (w);
    inner.removeFromLeft (4.0f);
    auto modBox = inner;

    auto pitchLbl = lblRow.removeFromLeft (w);
    lblRow.removeFromLeft (4.0f);
    auto modLbl = lblRow;

    auto drawWheel = [this, &g] (juce::Rectangle<float> box, juce::Rectangle<float> lBox, float valNorm, bool centerZero, const juce::String& tag)
    {
        g.setColour (theme.surface);
        g.fillRoundedRectangle (box, 2.0f);
        g.setColour (theme.border);
        g.drawRoundedRectangle (box, 2.0f, 1.0f);

        float t = centerZero ? (0.5f - valNorm * 0.5f) : (1.0f - valNorm);
        t = juce::jlimit (0.12f, 0.88f, t);
        float thumbY = box.getY() + t * box.getHeight();

        g.setColour (theme.text);
        g.fillRoundedRectangle (box.getX() + 1.5f, thumbY - 2.5f, box.getWidth() - 3.0f, 5.0f, 1.2f);
        g.setColour (theme.bg);
        g.drawHorizontalLine (static_cast<int> (thumbY), box.getX() + 2.0f, box.getRight() - 2.0f);

        g.setColour (theme.mutedText);
        g.setFont (juce::FontOptions (7.5f, juce::Font::bold));
        g.drawText (tag, lBox, juce::Justification::centred, false);
    };

    drawWheel (pitchBox, pitchLbl, processor.pitchWheelNorm.load(), true, "P");
    drawWheel (modBox, modLbl, processor.modWheelNorm.load(), false, "M");
}

void PitchModWheelsComponent::updateWheelFromY (int y)
{
    auto inner = getLocalBounds().reduced (3, 2);
    inner.removeFromBottom (9);
    float normY = juce::jlimit (0.0f, 1.0f, (y - inner.getY()) / static_cast<float> (std::max (1, inner.getHeight())));
    if (activeWheel == 1)
    {
        float pw = juce::jlimit (-1.0f, 1.0f, (0.5f - normY) * 2.0f);
        if (std::abs (pw) < 0.08f) pw = 0.0f;
        processor.pitchWheelNorm.store (pw);
    }
    else if (activeWheel == 2)
    {
        processor.modWheelNorm.store (1.0f - normY);
    }
    repaint();
}

void PitchModWheelsComponent::mouseDown (const juce::MouseEvent& e)
{
    activeWheel = (e.x < getWidth() / 2) ? 1 : 2;
    updateWheelFromY (e.y);
}

void PitchModWheelsComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (activeWheel != 0) updateWheelFromY (e.y);
}

void PitchModWheelsComponent::mouseUp (const juce::MouseEvent&)
{
    if (activeWheel == 1)
        processor.pitchWheelNorm.store (0.0f);
    activeWheel = 0;
    repaint();
}

// ==============================================================================
// PresetBarWidget (Symmetrical Square [v] and [S] Buttons + Centered Dashed Box)
// ==============================================================================
PresetBarWidget::PresetBarWidget (RythmeAudioProcessor& p,
                                  std::function<void()> onSaveClicked,
                                  std::function<void(const juce::File&)> onLoadFile,
                                  std::function<void()> onSetRootFolder,
                                  std::function<void()> onBrowseLoadPreset)
    : processor (p), saveCb (std::move (onSaveClicked)), loadFileCb (std::move (onLoadFile)),
      setRootCb (std::move (onSetRootFolder)), browseLoadCb (std::move (onBrowseLoadPreset))
{
    theme = RythmeAudioProcessor::getThemePalette (0);

    dropdownBtn.setButtonText ("v");
    dropdownBtn.onClick = [this]() { openCascadingPresetMenu(); };
    addAndMakeVisible (dropdownBtn);

    libNameEditor.setMultiLine (false);
    libNameEditor.setJustification (juce::Justification::centred);
    libNameEditor.setTextToShowWhenEmpty ("- - - - - - - - -", theme.mutedText);
    libNameEditor.setText (processor.libraryNameText, juce::dontSendNotification);
    libNameEditor.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    libNameEditor.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    libNameEditor.onTextChange = [this]()
    {
        processor.libraryNameText = libNameEditor.getText().trim();
        processor.currentPresetName = processor.libraryNameText;
    };
    addAndMakeVisible (libNameEditor);

    saveIconBtn.setButtonText ("[S]");
    saveIconBtn.setTooltip ("Save Preset (.mn6x)");
    saveIconBtn.onClick = [this]() { if (saveCb) saveCb(); };
    addAndMakeVisible (saveIconBtn);
}

void PresetBarWidget::setThemePalette (const KinetikThemePalette& pal)
{
    theme = pal;
    libNameEditor.setTextToShowWhenEmpty ("- - - - - - - - -", pal.mutedText);
    libNameEditor.applyColourToAllText (pal.text);
    repaint();
}

void PresetBarWidget::refreshTextFromProcessor()
{
    libNameEditor.setText (processor.libraryNameText, juce::dontSendNotification);
}

void PresetBarWidget::flashSavedFeedback (bool success)
{
    saveIconBtn.setButtonText (success ? "OK" : "ERR");
    juce::Component::SafePointer<PresetBarWidget> safeThis (this);
    juce::Timer::callAfterDelay (1100, [safeThis]()
    {
        if (safeThis != nullptr)
            safeThis->saveIconBtn.setButtonText ("[S]");
    });
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
    menu.addItem (2, "Load Preset (.mn6x)...");
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
            if (res == 1) { if (setRootCb) setRootCb(); return; }
            if (res == 2) { if (browseLoadCb) browseLoadCb(); return; }
            auto it = idMap.find (res);
            if (it != idMap.end() && loadFileCb)
                loadFileCb (it->second);
        });
}

void PresetBarWidget::resized()
{
    auto r = getLocalBounds().reduced (2);
    int btnW = r.getHeight(); // Symmetrical square [v] and [S] buttons!
    dropdownBtn.setBounds (r.removeFromLeft (btnW));
    saveIconBtn.setBounds (r.removeFromRight (btnW));
    libNameEditor.setBounds (r.reduced (3, 0));
}

void PresetBarWidget::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme.bg);
    g.fillRoundedRectangle (r, 4.0f);
    drawSketchRect (g, r.reduced (0.5f), 4.0f, 1.3f, theme.border);
}

// ==============================================================================
// AdsrGraphComponent (Large Interactive Curve Editor — Drag A / D / S / R Nodes!)
// ==============================================================================
AdsrGraphComponent::AdsrGraphComponent (RythmeAudioProcessor& p) : processor (p)
{
    theme = RythmeAudioProcessor::getThemePalette (0);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void AdsrGraphComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (theme.bg);
    g.fillRoundedRectangle (area, 4.0f);
    drawSketchRect (g, area.reduced (0.5f), 4.0f, 1.2f, theme.border);

    float atk = 0.10f, dec = 300.0f, sus = 85.0f, rel = 250.0f;
    {
        const juce::ScopedLock sl (processor.zoneLock);
        if (processor.selectedZoneIndex >= 0 && processor.selectedZoneIndex < static_cast<int> (processor.zones.size()))
        {
            const auto& z = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)];
            atk = z.attackMs; dec = z.decayMs; sus = z.sustainPct; rel = z.releaseMs;
        }
    }

    auto inner = area.reduced (10.0f, 10.0f);
    g.setColour (theme.grid);
    g.drawHorizontalLine (static_cast<int> (inner.getCentreY()), inner.getX(), inner.getRight());
    g.drawVerticalLine (static_cast<int> (inner.getCentreX()), inner.getY(), inner.getBottom());

    float w = inner.getWidth();
    float h = inner.getHeight();
    float x0 = inner.getX();
    float yBot = inner.getBottom();
    float yTop = inner.getY();

    float aW = juce::jlimit (8.0f, w * 0.28f, std::sqrt (atk / 5000.0f) * (w * 0.28f) + 8.0f);
    float dW = juce::jlimit (12.0f, w * 0.28f, std::sqrt (dec / 8000.0f) * (w * 0.28f) + 12.0f);
    float rW = juce::jlimit (12.0f, w * 0.28f, std::sqrt (rel / 10000.0f) * (w * 0.28f) + 12.0f);
    float sW = std::max (14.0f, w - (aW + dW + rW));

    float xA = x0 + aW;
    float xD = xA + dW;
    float xS = xD + sW;
    float xR = xS + rW;
    float ySus = yBot - (juce::jlimit (0.0f, 100.0f, sus) / 100.0f) * h;

    // Exponential decay & release curve segments
    juce::Path envPath;
    envPath.startNewSubPath (x0, yBot);
    envPath.lineTo (xA, yTop);
    for (int i = 1; i <= 16; ++i)
    {
        float t = i / 16.0f;
        float e = std::exp (-4.2f * t);
        float px = xA + t * (xD - xA);
        float py = ySus + (yTop - ySus) * e;
        envPath.lineTo (px, py);
    }
    envPath.lineTo (xS, ySus);
    for (int i = 1; i <= 16; ++i)
    {
        float t = i / 16.0f;
        float e = std::exp (-4.5f * t);
        float px = xS + t * (xR - xS);
        float py = yBot + (ySus - yBot) * e;
        envPath.lineTo (px, py);
    }

    juce::Path fillPath (envPath);
    fillPath.lineTo (x0, yBot);
    fillPath.closeSubPath();

    g.setColour (theme.accent.withAlpha (0.20f));
    g.fillPath (fillPath);
    g.setColour (theme.accent);
    g.strokePath (envPath, juce::PathStrokeType (2.0f));

    g.setColour (theme.text);
    g.fillEllipse (xA - 3.5f, yTop - 3.5f, 7.0f, 7.0f);
    g.fillEllipse (xD - 3.5f, ySus - 3.5f, 7.0f, 7.0f);
    g.fillEllipse (xS - 3.5f, ySus - 3.5f, 7.0f, 7.0f);
    g.fillEllipse (xR - 3.5f, yBot - 3.5f, 7.0f, 7.0f);
}

void AdsrGraphComponent::mouseDown (const juce::MouseEvent& e)
{
    const juce::ScopedLock sl (processor.zoneLock);
    if (processor.selectedZoneIndex < 0 || processor.selectedZoneIndex >= static_cast<int> (processor.zones.size()))
        return;
    const auto& z = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)];

    auto inner = getLocalBounds().toFloat().reduced (10.0f, 10.0f);
    float w = inner.getWidth();
    float h = inner.getHeight();
    float x0 = inner.getX();
    float yBot = inner.getBottom();
    float yTop = inner.getY();

    float aW = juce::jlimit (8.0f, w * 0.28f, std::sqrt (z.attackMs / 5000.0f) * (w * 0.28f) + 8.0f);
    float dW = juce::jlimit (12.0f, w * 0.28f, std::sqrt (z.decayMs / 8000.0f) * (w * 0.28f) + 12.0f);
    float rW = juce::jlimit (12.0f, w * 0.28f, std::sqrt (z.releaseMs / 10000.0f) * (w * 0.28f) + 12.0f);
    float sW = std::max (14.0f, w - (aW + dW + rW));

    float xA = x0 + aW;
    float xD = xA + dW;
    float xS = xD + sW;
    float xR = xS + rW;
    float ySus = yBot - (juce::jlimit (0.0f, 100.0f, z.sustainPct) / 100.0f) * h;

    float dA = e.position.getDistanceFrom ({ xA, yTop });
    float dD = e.position.getDistanceFrom ({ xD, ySus });
    float dR = e.position.getDistanceFrom ({ xR, yBot });
    float dS = e.position.getDistanceFrom ({ xS, ySus });

    float minD = std::min ({ dA, dD, dR, dS });
    if (minD > 24.0f) { activeNode = DragNode::None; return; }
    if (minD == dA) activeNode = DragNode::Attack;
    else if (minD == dD || minD == dS) activeNode = DragNode::DecaySustain;
    else activeNode = DragNode::Release;
}

void AdsrGraphComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (activeNode == DragNode::None) return;
    {
        const juce::ScopedLock sl (processor.zoneLock);
        if (processor.selectedZoneIndex < 0 || processor.selectedZoneIndex >= static_cast<int> (processor.zones.size()))
            return;
        auto& z = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)];

        auto inner = getLocalBounds().toFloat().reduced (10.0f, 10.0f);
        float w = std::max (10.0f, inner.getWidth());
        float h = std::max (10.0f, inner.getHeight());

        if (activeNode == DragNode::Attack)
        {
            float norm = juce::jlimit (0.0f, 1.0f, (e.position.x - inner.getX() - 8.0f) / (w * 0.28f));
            z.attackMs = norm * norm * 5000.0f;
        }
        else if (activeNode == DragNode::DecaySustain)
        {
            float aW = juce::jlimit (8.0f, w * 0.28f, std::sqrt (z.attackMs / 5000.0f) * (w * 0.28f) + 8.0f);
            float normD = juce::jlimit (0.0f, 1.0f, (e.position.x - (inner.getX() + aW) - 12.0f) / (w * 0.28f));
            z.decayMs = juce::jlimit (1.0f, 8000.0f, normD * normD * 8000.0f);
            float normS = juce::jlimit (0.0f, 1.0f, (inner.getBottom() - e.position.y) / h);
            z.sustainPct = normS * 100.0f;
        }
        else if (activeNode == DragNode::Release)
        {
            float normR = juce::jlimit (0.0f, 1.0f, (e.position.x - (inner.getRight() - w * 0.28f)) / (w * 0.28f));
            z.releaseMs = juce::jlimit (1.0f, 10000.0f, normR * normR * 10000.0f);
        }
    }
    repaint();
    if (onEnvelopeChanged) onEnvelopeChanged();
}

void AdsrGraphComponent::mouseUp (const juce::MouseEvent&)
{
    activeNode = DragNode::None;
}

// ==============================================================================
// FxCardRackStrip (7-Card Pro FX Rack with One-Click ON/OFF Power Dot)
// ==============================================================================
FxCardRackStrip::FxCardRackStrip (RythmeAudioProcessor& p) : processor (p)
{
    theme = RythmeAudioProcessor::getThemePalette (0);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void FxCardRackStrip::setSelectedSlot (int idx)
{
    selectedSlot = juce::jlimit (0, SampleGroup::numFxSlots - 1, idx);
    repaint();
}

void FxCardRackStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    int n = SampleGroup::numFxSlots;
    float gap = 4.0f;
    float cardW = (r.getWidth() - gap * (n - 1)) / static_cast<float> (n);

    std::array<SimpleFxSlot, SampleGroup::numFxSlots> slotsCopy;
    {
        const juce::ScopedLock sl (processor.zoneLock);
        if (processor.selectedGroupIndex >= 0 && processor.selectedGroupIndex < static_cast<int> (processor.groups.size()))
            slotsCopy = processor.groups[static_cast<size_t> (processor.selectedGroupIndex)].fxSlots;
    }

    for (int i = 0; i < n; ++i)
    {
        juce::Rectangle<float> cr (r.getX() + i * (cardW + gap), r.getY(), cardW, r.getHeight());
        const auto& sl = slotsCopy[static_cast<size_t> (i)];
        bool isSel = (i == selectedSlot);

        g.setColour (isSel ? theme.surface : theme.bg);
        g.fillRoundedRectangle (cr, 4.0f);
        drawSketchRect (g, cr.reduced (0.5f), 4.0f, isSel ? 1.65f : 1.1f, isSel ? theme.accent : theme.border);

        // Power dot on left side of card
        juce::Rectangle<float> dotR (cr.getX() + 6.0f, cr.getCentreY() - 4.0f, 8.0f, 8.0f);
        if (sl.enabled)
        {
            g.setColour (theme.accent);
            g.fillEllipse (dotR);
        }
        else
        {
            g.setColour (theme.mutedText);
            g.drawEllipse (dotR, 1.3f);
        }

        g.setColour (sl.enabled || isSel ? theme.text : theme.mutedText);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (SimpleFxSlot::getShortCardName (sl.type),
                    cr.withTrimmedLeft (16.0f).reduced (2.0f, 0.0f),
                    juce::Justification::centred, false);
    }
}

void FxCardRackStrip::mouseDown (const juce::MouseEvent& e)
{
    auto r = getLocalBounds().toFloat();
    int n = SampleGroup::numFxSlots;
    float gap = 4.0f;
    float cardW = (r.getWidth() - gap * (n - 1)) / static_cast<float> (n);

    for (int i = 0; i < n; ++i)
    {
        juce::Rectangle<float> cr (r.getX() + i * (cardW + gap), r.getY(), cardW, r.getHeight());
        if (cr.contains (e.position))
        {
            selectedSlot = i;
            if (onSlotSelected) onSlotSelected (i);

            // Clicking the left 20px power dot or right-clicking toggles FX ON/OFF immediately!
            if (e.position.x <= cr.getX() + 20.0f || e.mods.isRightButtonDown())
            {
                if (onSlotToggled) onSlotToggled (i);
            }
            repaint();
            return;
        }
    }
}

// ==============================================================================
// FabStyleFxDisplay (FabFilter Pro-Q FFT Spectrum + Shaded Curve, Pro-C Live GR, etc.)
// ==============================================================================
FabStyleFxDisplay::FabStyleFxDisplay (RythmeAudioProcessor& p) : processor (p)
{
    theme = RythmeAudioProcessor::getThemePalette (0);
    smoothedBins.fill (0.0f);
}

void FabStyleFxDisplay::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (theme.bg);
    g.fillRoundedRectangle (area, 5.0f);
    drawSketchRect (g, area.reduced (0.5f), 5.0f, 1.25f, theme.border);

    SimpleFxSlot slotCopy;
    {
        const juce::ScopedLock sl (processor.zoneLock);
        if (processor.selectedGroupIndex >= 0 && processor.selectedGroupIndex < static_cast<int> (processor.groups.size()))
            slotCopy = processor.groups[static_cast<size_t> (processor.selectedGroupIndex)]
                           .fxSlots[static_cast<size_t> (juce::jlimit (0, SampleGroup::numFxSlots - 1, slotIdx))];
    }

    auto r = area.reduced (8.0f, 6.0f);
    g.setColour (theme.grid);
    for (int i = 1; i <= 5; ++i)
        g.drawVerticalLine (static_cast<int> (r.getX() + (r.getWidth() * i) / 6.0f), r.getY(), r.getBottom());
    g.drawHorizontalLine (static_cast<int> (r.getCentreY()), r.getX(), r.getRight());

    // Header badge showing full FX name & status
    g.setColour (slotCopy.enabled ? theme.text : theme.mutedText);
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText (SimpleFxSlot::getTypeName (slotCopy.type) + (slotCopy.enabled ? "  [ON]" : "  [OFF]"),
                r.withHeight (13.0f), juce::Justification::centredLeft, false);

    juce::Colour curveCol = slotCopy.enabled ? theme.accent : theme.mutedText;

    if (slotCopy.type == SimpleFxType::EQ)
    {
        // 1. Real-Time Bottom-Up FFT Frequency Spectrum (20 Hz -> 20 kHz Log Scale)
        std::array<float, RythmeAudioProcessor::fftSize> recent {};
        bool sounding = false;
        processor.copyRecentAudioForFft (recent, sounding);

        if (sounding)
        {
            fftWork.fill (0.0f);
            for (int i = 0; i < RythmeAudioProcessor::fftSize; ++i)
                fftWork[static_cast<size_t> (i)] = recent[static_cast<size_t> (i)];
            hannWindow.multiplyWithWindowingTable (fftWork.data(), RythmeAudioProcessor::fftSize);
            forwardFft.performFrequencyOnlyForwardTransform (fftWork.data());
        }

        int numBins = static_cast<int> (smoothedBins.size());
        int maxFftBin = RythmeAudioProcessor::fftSize / 2 - 1;
        juce::Path specPath;
        specPath.startNewSubPath (r.getX(), r.getBottom());

        for (int b = 0; b < numBins; ++b)
        {
            float t = b / static_cast<float> (numBins - 1);
            // Log frequency mapping from 20Hz to 20kHz
            float freqNorm = std::pow (1000.0f, t) / 1000.0f;
            int binIdx = juce::jlimit (1, maxFftBin, static_cast<int> (freqNorm * maxFftBin));
            float mag = sounding ? (fftWork[static_cast<size_t> (binIdx)] / (RythmeAudioProcessor::fftSize * 0.25f)) : 0.0f;
            float dbNorm = sounding ? juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (mag + 1e-6f, -66.0f) + 66.0f) / 66.0f) : 0.0f;

            float prev = smoothedBins[static_cast<size_t> (b)];
            float next = (dbNorm > prev) ? (prev * 0.35f + dbNorm * 0.65f) : (prev * 0.82f);
            smoothedBins[static_cast<size_t> (b)] = next;

            float px = r.getX() + t * r.getWidth();
            float py = r.getBottom() - next * (r.getHeight() * 0.82f);
            specPath.lineTo (px, py);
        }
        specPath.lineTo (r.getRight(), r.getBottom());
        specPath.closeSubPath();

        g.setColour (theme.text.withAlpha (0.16f));
        g.fillPath (specPath);
        g.setColour (theme.text.withAlpha (0.35f));
        g.strokePath (specPath, juce::PathStrokeType (1.0f));

        // 2. FabFilter-Style Shaded EQ Frequency Response Curve (Low Cut HPF, Mid Bell, High Cut LPF)
        juce::Path eqCurve;
        int steps = 96;
        for (int i = 0; i <= steps; ++i)
        {
            float t = i / static_cast<float> (steps);
            float px = r.getX() + t * r.getWidth();

            // High-Pass Cut slope on left (p1)
            float hpCutT = slotCopy.p1 * 0.42f;
            float lowDrop = (slotCopy.p1 > 0.01f && t < hpCutT)
                          ? std::pow (1.0f - (t / std::max (0.005f, hpCutT)), 1.6f) * (r.getHeight() * 0.46f)
                          : 0.0f;

            // Mid Peaking Bell (p2 = freq, p3 = gain)
            float midBump = std::exp (-std::pow ((t - slotCopy.p2) * 5.2f, 2.0f)) * ((slotCopy.p3 - 0.5f) * r.getHeight() * 0.72f);

            // Low-Pass Cut slope on right (p4)
            float lpCutT = 0.28f + slotCopy.p4 * 0.72f;
            float highDrop = (slotCopy.p4 < 0.99f && t > lpCutT)
                           ? std::pow ((t - lpCutT) / std::max (0.005f, 1.0f - lpCutT), 1.6f) * (r.getHeight() * 0.46f)
                           : 0.0f;

            float outShift = (slotCopy.p5 - 0.5f) * 14.0f;
            float py = r.getCentreY() + lowDrop - midBump + highDrop - outShift;
            py = juce::jlimit (r.getY() + 3.0f, r.getBottom() - 3.0f, py);
            if (i == 0) eqCurve.startNewSubPath (px, py); else eqCurve.lineTo (px, py);
        }

        // Fill shaded area between 0 dB center line and EQ curve (true FabFilter Pro-Q shaded bands!)
        juce::Path shadedEq (eqCurve);
        shadedEq.lineTo (r.getRight(), r.getCentreY());
        shadedEq.lineTo (r.getX(), r.getCentreY());
        shadedEq.closeSubPath();

        g.setColour (curveCol.withAlpha (0.26f));
        g.fillPath (shadedEq);
        g.setColour (curveCol);
        g.strokePath (eqCurve, juce::PathStrokeType (2.1f));

        // Mid node handle dot
        float midX = r.getX() + slotCopy.p2 * r.getWidth();
        float midY = juce::jlimit (r.getY() + 4.0f, r.getBottom() - 4.0f,
                                   r.getCentreY() - (slotCopy.p3 - 0.5f) * r.getHeight() * 0.72f - (slotCopy.p5 - 0.5f) * 14.0f);
        g.setColour (curveCol);
        g.fillEllipse (midX - 4.0f, midY - 4.0f, 8.0f, 8.0f);
    }
    else if (slotCopy.type == SimpleFxType::Compressor)
    {
        // Left 55%: Threshold/Ratio Knee Curve; Right 45%: Live Gain Reduction (GR) Trace & Meter
        auto leftBox  = r.withWidth (r.getWidth() * 0.52f).reduced (2.0f, 4.0f);
        auto rightBox = r.withLeft (leftBox.getRight() + 8.0f).reduced (2.0f, 4.0f);

        juce::Path knee;
        float thresh = 1.0f - slotCopy.p1 * 0.85f;
        float ratio = 1.0f + slotCopy.p2 * 11.0f;
        for (int i = 0; i <= 48; ++i)
        {
            float t = i / 48.0f;
            float outT = (t <= thresh) ? t : (thresh + (t - thresh) / ratio);
            float px = leftBox.getX() + t * leftBox.getWidth();
            float py = leftBox.getBottom() - outT * leftBox.getHeight();
            if (i == 0) knee.startNewSubPath (px, py); else knee.lineTo (px, py);
        }
        g.setColour (curveCol);
        g.strokePath (knee, juce::PathStrokeType (2.0f));

        // Live Gain Reduction Scrolling Trace (top-down red/accent reduction)
        auto meterStrip = rightBox.removeFromRight (14.0f);
        rightBox.removeFromRight (4.0f);

        juce::Path grPath;
        grPath.startNewSubPath (rightBox.getX(), rightBox.getY());
        int nHist = SimpleFxSlot::grHistSize;
        for (int i = 0; i < nHist; ++i)
        {
            int idx = (slotCopy.grWriteIdx + i) % nHist;
            float grNorm = juce::jlimit (0.0f, 1.0f, slotCopy.grHistory[static_cast<size_t> (idx)]);
            float px = rightBox.getX() + (i / static_cast<float> (nHist - 1)) * rightBox.getWidth();
            float py = rightBox.getY() + grNorm * rightBox.getHeight();
            grPath.lineTo (px, py);
        }
        grPath.lineTo (rightBox.getRight(), rightBox.getY());
        grPath.closeSubPath();

        g.setColour (juce::Colour (0xffee5253).withAlpha (0.30f));
        g.fillPath (grPath);
        g.setColour (juce::Colour (0xffee5253));
        g.strokePath (grPath, juce::PathStrokeType (1.5f));

        // Vertical GR bar on far right
        g.setColour (theme.surface);
        g.fillRoundedRectangle (meterStrip, 2.0f);
        g.setColour (theme.border);
        g.drawRoundedRectangle (meterStrip, 2.0f, 1.0f);
        float grFill = juce::jlimit (0.0f, 1.0f, -slotCopy.lastGrDb / 24.0f);
        if (grFill > 0.01f)
        {
            g.setColour (juce::Colour (0xffee5253));
            g.fillRect (meterStrip.reduced (2.0f).withHeight ((meterStrip.getHeight() - 4.0f) * grFill));
        }
    }
    else if (slotCopy.type == SimpleFxType::Saturation)
    {
        juce::Path curve;
        float drive = 1.0f + slotCopy.p1 * 6.0f;
        float invNorm = 1.0f / fastTanh (drive);
        for (int i = 0; i <= 64; ++i)
        {
            float t = i / 64.0f;
            float x = (t * 2.0f - 1.0f) * drive;
            float y = fastTanh (x) * invNorm;
            float px = r.getX() + t * r.getWidth();
            float py = r.getCentreY() - y * (r.getHeight() * 0.42f);
            if (i == 0) curve.startNewSubPath (px, py); else curve.lineTo (px, py);
        }
        g.setColour (curveCol);
        g.strokePath (curve, juce::PathStrokeType (2.0f));
    }
    else if (slotCopy.type == SimpleFxType::Transient)
    {
        // Transient Shaper Envelope Pulse Preview (Attack spike + Sustain body)
        juce::Path trCurve;
        float atkBoost = 0.45f + slotCopy.p1 * 0.50f;
        float susLevel = 0.15f + slotCopy.p2 * 0.55f;
        for (int i = 0; i <= 64; ++i)
        {
            float t = i / 64.0f;
            float spike = atkBoost * std::exp (-t * (8.0f + slotCopy.p3 * 12.0f));
            float body  = susLevel * (1.0f - std::exp (-t * 14.0f)) * std::exp (-t * 1.4f);
            float amp = juce::jlimit (0.0f, 1.0f, spike + body);
            float px = r.getX() + t * r.getWidth();
            float py = r.getBottom() - 4.0f - amp * (r.getHeight() - 12.0f);
            if (i == 0) trCurve.startNewSubPath (px, py); else trCurve.lineTo (px, py);
        }
        juce::Path fillT (trCurve);
        fillT.lineTo (r.getRight(), r.getBottom() - 4.0f);
        fillT.lineTo (r.getX(), r.getBottom() - 4.0f);
        fillT.closeSubPath();
        g.setColour (curveCol.withAlpha (0.22f));
        g.fillPath (fillT);
        g.setColour (curveCol);
        g.strokePath (trCurve, juce::PathStrokeType (2.0f));
    }
    else if (slotCopy.type == SimpleFxType::Width)
    {
        // Stereo Field Lissajous / Spread Ellipse Visualizer
        float cx = r.getCentreX();
        float cy = r.getCentreY() + 3.0f;
        float rx = 14.0f + slotCopy.p1 * (r.getWidth() * 0.36f);
        float ry = 8.0f + slotCopy.p2 * (r.getHeight() * 0.28f);
        g.setColour (curveCol.withAlpha (0.22f));
        g.fillEllipse (cx - rx, cy - ry, rx * 2.0f, ry * 2.0f);
        g.setColour (curveCol);
        g.drawEllipse (cx - rx, cy - ry, rx * 2.0f, ry * 2.0f, 1.8f);
        g.drawEllipse (cx - rx * 0.55f, cy - ry * 0.55f, rx * 1.1f, ry * 1.1f, 1.1f);
    }
    else
    {
        // Delay / Reverb Tail Decay Envelope
        juce::Path tailCurve;
        float decayRate = 1.4f + (1.0f - slotCopy.p2) * 4.5f;
        float density = (slotCopy.type == SimpleFxType::Reverb) ? 38.0f : (8.0f + (1.0f - slotCopy.p1) * 18.0f);
        for (int i = 0; i <= 72; ++i)
        {
            float t = i / 72.0f;
            float env = std::exp (-t * decayRate) * std::cos (t * density);
            float px = r.getX() + t * r.getWidth();
            float py = r.getCentreY() + 3.0f - env * (r.getHeight() * 0.38f) * (0.3f + 0.7f * slotCopy.p5);
            if (i == 0) tailCurve.startNewSubPath (px, py); else tailCurve.lineTo (px, py);
        }
        g.setColour (curveCol);
        g.strokePath (tailCurve, juce::PathStrokeType (1.8f));
    }
}

// ==============================================================================
// LiveMainWaveDisplay
// ==============================================================================
LiveMainWaveDisplay::LiveMainWaveDisplay (RythmeAudioProcessor& p) : processor (p)
{
    theme = RythmeAudioProcessor::getThemePalette (0);
}

void LiveMainWaveDisplay::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (theme.bg);
    g.fillRoundedRectangle (area, 5.0f);
    drawSketchRect (g, area.reduced (0.5f), 5.0f, 1.4f, theme.border);

    auto screen = area.reduced (7.0f, 6.0f);
    g.setColour (theme.grid);
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine (static_cast<int> (screen.getX() + (screen.getWidth() * i) / 8.0f), screen.getY(), screen.getBottom());
    g.drawHorizontalLine (static_cast<int> (screen.getCentreY()), screen.getX(), screen.getRight());

    std::array<float, RythmeAudioProcessor::scopeHistorySize> hist {};
    bool sounding = false;
    processor.copyScopeHistory (hist, sounding);

    float midY = screen.getCentreY();
    float halfH = screen.getHeight() * 0.44f;

    juce::Path wave;
    wave.startNewSubPath (screen.getX(), midY);
    for (int i = 0; i < RythmeAudioProcessor::scopeHistorySize; ++i)
    {
        float px = screen.getX() + (i / static_cast<float> (RythmeAudioProcessor::scopeHistorySize - 1)) * screen.getWidth();
        float py = midY - hist[static_cast<size_t> (i)] * halfH;
        wave.lineTo (px, py);
    }

    if (sounding)
    {
        g.setColour (theme.accent.withAlpha (0.22f));
        g.strokePath (wave, juce::PathStrokeType (4.5f));
        g.setColour (theme.accent);
        g.strokePath (wave, juce::PathStrokeType (1.8f));
    }
    else
    {
        g.setColour (theme.mutedText);
        g.strokePath (wave, juce::PathStrokeType (1.2f));
    }
}

// ==============================================================================
// ColorPianoStrip (Dark High-Contrast Sample Colours + Polyphonic Live Highlight)
// ==============================================================================
ColorPianoStrip::ColorPianoStrip (RythmeAudioProcessor& p) : processor (p)
{
    theme = RythmeAudioProcessor::getThemePalette (0);
}

void ColorPianoStrip::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll (theme.bg);
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
        juce::Colour mappedCol = RythmeAudioProcessor::getPaletteColour (0);
        for (int zIdx = 0; zIdx < static_cast<int> (processor.zones.size()); ++zIdx)
        {
            const auto& z = processor.zones[static_cast<size_t> (zIdx)];
            if (note >= z.lowKey && note <= z.highKey)
            {
                if (! isMapped || zIdx == processor.selectedZoneIndex || z.groupIndex == processor.selectedGroupIndex)
                    mappedCol = z.colour;
                isMapped = true;
            }
        }

        bool isHeld = (note >= 0 && note < 128 && processor.activeNotesUi[static_cast<size_t> (note)].load() != 0);
        bool isPressed = (note == pressedNote || isHeld || note == activeNote);

        if (isPressed)
            g.setColour (isMapped ? mappedCol.brighter (0.55f) : (isBlack ? juce::Colours::white : juce::Colour (0xff2c6bed)));
        else if (isKs)
            g.setColour (note == activeKs ? juce::Colour (0xffe53935) : juce::Colour (0xff9e1c20));
        else if (isMapped)
            g.setColour (isBlack ? mappedCol.darker (0.28f) : mappedCol);
        else
            g.setColour (isBlack ? juce::Colour (0xff101319) : juce::Colour (0xfff4f6fa));

        g.fillRect (kr);
        if (isMapped && ! isPressed)
        {
            g.setColour (mappedCol.brighter (0.35f));
            g.fillRect (kr.getX() + 0.5f, kr.getY(), std::max (1.0f, kr.getWidth() - 1.0f), 4.0f);
        }
        g.setColour (juce::Colour (0xff07080a));
        g.drawRect (kr, 0.8f);

        if (isPressed)
        {
            g.setColour (juce::Colours::white);
            g.drawRect (kr.reduced (0.5f), 1.6f);
            g.fillEllipse (kr.getCentreX() - 2.4f, kr.getY() + 4.0f, 4.8f, 4.8f);
        }

        if (n == 0)
        {
            int oct = note / 12;
            g.setColour ((isBlack || isMapped || isPressed) ? juce::Colours::white : juce::Colour (0xff101319));
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

void ColorPianoStrip::mouseDrag (const juce::MouseEvent& e)
{
    int note = startNote + static_cast<int> ((e.x / static_cast<float> (std::max (1, getWidth()))) * numKeys);
    note = juce::jlimit (startNote, startNote + numKeys - 1, note);
    if (note != pressedNote)
    {
        if (pressedNote >= 0) processor.uiNoteOff (pressedNote);
        pressedNote = note;
        processor.uiNoteOn (note, 0.9f);
        repaint();
    }
}

void ColorPianoStrip::mouseUp (const juce::MouseEvent&)
{
    if (pressedNote >= 0) { processor.uiNoteOff (pressedNote); pressedNote = -1; repaint(); }
}

// ==============================================================================
// KeyMapperComponent (Single-Note Hand-Cursor Move Fix + Auto Root Key + Live Info)
// ==============================================================================
KeyMapperComponent::KeyMapperComponent (RythmeAudioProcessor& p) : processor (p)
{
    theme = RythmeAudioProcessor::getThemePalette (0);
}

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
    if (processor.selectedZoneIndex >= 0 && processor.selectedZoneIndex < n
        && ! isZoneVisibleInMapper (processor.zones[static_cast<size_t> (processor.selectedZoneIndex)]))
    {
        processor.selectedZoneIndex = selectedZoneIndices.empty() ? -1 : *selectedZoneIndices.begin();
    }
}

void KeyMapperComponent::selectAllZones()
{
    const juce::ScopedLock sl (processor.zoneLock);
    selectedZoneIndices.clear();
    for (int i = 0; i < static_cast<int> (processor.zones.size()); ++i)
        if (isZoneVisibleInMapper (processor.zones[static_cast<size_t> (i)]))
        {
            selectedZoneIndices.insert (i);
            processor.selectedZoneIndex = i;
        }
    if (onSelectionChanged) onSelectionChanged();
    repaint();
}

void KeyMapperComponent::deleteSelectedZones()
{
    std::vector<int> toDel (selectedZoneIndices.begin(), selectedZoneIndices.end());
    if (toDel.empty() && processor.selectedZoneIndex >= 0)
        toDel.push_back (processor.selectedZoneIndex);
    processor.deleteZones (std::move (toDel));
    selectedZoneIndices.clear();
    if (processor.selectedZoneIndex >= 0)
        selectedZoneIndices.insert (processor.selectedZoneIndex);
    if (onSelectionChanged) onSelectionChanged();
    repaint();
}

void KeyMapperComponent::clearSelection() { selectedZoneIndices.clear(); repaint(); }

void KeyMapperComponent::selectSingleZone (int idx)
{
    selectedZoneIndices.clear();
    if (idx >= 0)
    {
        selectedZoneIndices.insert (idx);
        processor.selectedZoneIndex = idx;
    }
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
    return juce::jlimit (0, 127, static_cast<int> (std::round (std::round (rawVel / step) * step)));
}

juce::Rectangle<float> KeyMapperComponent::getZoneRect (const SampleZone& z, juce::Rectangle<int> gridArea) const
{
    float keyW = gridArea.getWidth() / static_cast<float> (numKeys);
    float x1 = gridArea.getX() + (z.lowKey - startNote) * keyW;
    float x2 = gridArea.getX() + (z.highKey - startNote + 1) * keyW;
    float y1 = gridArea.getBottom() - (z.highVel / 127.0f) * gridArea.getHeight();
    float y2 = gridArea.getBottom() - (z.lowVel / 127.0f) * gridArea.getHeight();
    return { x1, y1, std::max (6.0f, x2 - x1), std::max (8.0f, y2 - y1) };
}

KeyMapperComponent::DragMode KeyMapperComponent::hitTestZoneMode (const juce::Rectangle<float>& r, juce::Point<float> pt) const
{
    // If zone is narrow (1-2 keys wide), interior always triggers Move (Hand cursor),
    // and only outer edge overhang triggers Left/Right Resize!
    bool isNarrow = (r.getWidth() <= 16.0f);
    if (isNarrow)
    {
        if (pt.x < r.getX() + 1.0f) return DragMode::LeftEdge;
        if (pt.x > r.getRight() - 1.0f) return DragMode::RightEdge;
        if (std::abs (pt.y - r.getY()) <= 5.0f && r.getHeight() >= 18.0f) return DragMode::TopEdge;
        if (std::abs (pt.y - r.getBottom()) <= 5.0f && r.getHeight() >= 18.0f) return DragMode::BottomEdge;
        return DragMode::Move;
    }

    if (std::abs (pt.x - r.getX()) <= 5.0f) return DragMode::LeftEdge;
    if (std::abs (pt.x - r.getRight()) <= 5.0f) return DragMode::RightEdge;
    if (std::abs (pt.y - r.getY()) <= 6.0f) return DragMode::TopEdge;
    if (std::abs (pt.y - r.getBottom()) <= 6.0f) return DragMode::BottomEdge;
    return DragMode::Move;
}

void KeyMapperComponent::mouseMove (const juce::MouseEvent& e)
{
    if (readOnly) { setMouseCursor (juce::MouseCursor::NormalCursor); return; }
    auto gridArea = getGridArea();
    if (! gridArea.contains (e.getPosition()))
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
        return;
    }

    const juce::ScopedLock sl (processor.zoneLock);
    sanitizeSelection();
    for (int i = static_cast<int> (processor.zones.size()) - 1; i >= 0; --i)
    {
        const auto& z = processor.zones[static_cast<size_t> (i)];
        if (! isZoneVisibleInMapper (z)) continue;
        auto r = getZoneRect (z, gridArea);
        if (r.expanded (3.0f).contains (e.position))
        {
            auto mode = hitTestZoneMode (r, e.position);
            if (mode == DragMode::LeftEdge || mode == DragMode::RightEdge)
                setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            else if (mode == DragMode::TopEdge || mode == DragMode::BottomEdge)
                setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
            else
                setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            return;
        }
    }
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void KeyMapperComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll (theme.bg);
    drawSketchRect (g, area.toFloat().reduced (0.5f), 4.0f, 1.3f, theme.border);

    auto gridArea = getGridArea();
    auto keyArea = getKeyboardArea();

    int divs = readOnly ? 4 : (velGridDivs > 0 ? velGridDivs : 4);
    for (int i = 0; i <= divs; ++i)
    {
        float norm = i / static_cast<float> (divs);
        int y = gridArea.getBottom() - static_cast<int> (norm * gridArea.getHeight());
        g.setColour (theme.grid);
        g.drawHorizontalLine (y, static_cast<float> (gridArea.getX()), static_cast<float> (gridArea.getRight()));
        if (! readOnly && (divs <= 8 || i % 2 == 0))
        {
            g.setColour (theme.mutedText);
            g.setFont (8.5f);
            g.drawText (juce::String (static_cast<int> (std::round (norm * 127.0f))), 2, y - 6, 22, 12, juce::Justification::centredRight, false);
        }
    }

    float keyW = gridArea.getWidth() / static_cast<float> (numKeys);
    g.setColour (theme.grid);
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

        g.setColour (z.colour.withAlpha (isSelected ? 0.72f : 0.42f));
        g.fillRect (r);
        g.setColour (isSelected ? juce::Colours::white : z.colour.brighter (0.35f));
        g.drawRect (r, isSelected ? 2.0f : 1.1f);

        juce::String label = z.name;
        if (z.layers.size() > 1) label += " (" + juce::String (z.layers.size()) + "RR)";
        g.setColour (juce::Colours::white);
        g.setFont (9.0f);
        if (r.getWidth() > 18.0f)
            g.drawText (label, r.reduced (3, 1), juce::Justification::centredLeft, true);
    }

    // Live Zone Info Badge in top-right corner of KeyMapper
    if (! readOnly && processor.selectedZoneIndex >= 0 && processor.selectedZoneIndex < static_cast<int> (processor.zones.size()))
    {
        const auto& sz = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)];
        juce::String kLow  = juce::MidiMessage::getMidiNoteName (sz.lowKey, true, true, 5);
        juce::String kHigh = juce::MidiMessage::getMidiNoteName (sz.highKey, true, true, 5);
        juce::String kRoot = juce::MidiMessage::getMidiNoteName (sz.rootKey, true, true, 5);
        juce::String info  = "Key: " + kLow + "-" + kHigh + "  |  Root: " + kRoot + "  |  Vel: " + juce::String (sz.lowVel) + "-" + juce::String (sz.highVel);
        auto badgeR = juce::Rectangle<int> (gridArea.getRight() - 252, gridArea.getY() + 3, 248, 15);
        g.setColour (theme.bg.withAlpha (0.86f));
        g.fillRoundedRectangle (badgeR.toFloat(), 3.0f);
        g.setColour (theme.text);
        g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
        g.drawText (info, badgeR.reduced (5, 0), juce::Justification::centredRight, false);
    }

    if (isMarqueeDragging)
    {
        auto mr = juce::Rectangle<float>::leftTopRightBottom (
            std::min (marqueeStart.x, marqueeCurrent.x), std::min (marqueeStart.y, marqueeCurrent.y),
            std::max (marqueeStart.x, marqueeCurrent.x), std::max (marqueeStart.y, marqueeCurrent.y));
        g.setColour (theme.accent.withAlpha (0.18f));
        g.fillRect (mr);
        g.setColour (theme.accent);
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
        bool isMapped = false;
        juce::Colour mappedCol = RythmeAudioProcessor::getPaletteColour (0);
        for (int zIdx = 0; zIdx < static_cast<int> (processor.zones.size()); ++zIdx)
        {
            const auto& z = processor.zones[static_cast<size_t> (zIdx)];
            if (isZoneVisibleInMapper (z) && note >= z.lowKey && note <= z.highKey)
            {
                if (! isMapped || zIdx == processor.selectedZoneIndex)
                    mappedCol = z.colour;
                isMapped = true;
            }
        }

        bool isHeld = (note >= 0 && note < 128 && processor.activeNotesUi[static_cast<size_t> (note)].load() != 0);
        if (note == pressedKey || isHeld || note == activeNote)
            g.setColour (isMapped ? mappedCol.brighter (0.55f) : juce::Colour (0xff2c6bed));
        else if (isKs)
            g.setColour (note == activeKs ? juce::Colour (0xffe53935) : juce::Colour (0xff9e1c20));
        else if (! readOnly && note == selectedRootKey)
            g.setColour (juce::Colour (0xffffb300));
        else if (isMapped)
            g.setColour (isBlack ? mappedCol.darker (0.28f) : mappedCol);
        else
            g.setColour (isBlack ? juce::Colour (0xff101319) : juce::Colour (0xfff4f6fa));

        g.fillRect (kr);
        g.setColour (juce::Colour (0xff07080a));
        g.drawRect (kr, 0.7f);

        if (n == 0)
        {
            int oct = note / 12;
            g.setColour ((isBlack || isMapped || isHeld) ? juce::Colours::white : juce::Colour (0xff101319));
            g.setFont (7.0f);
            g.drawText ("C" + juce::String (oct), kr.expanded (4.0f, 0.0f), juce::Justification::centredBottom, false);
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
        if (r.expanded (3.0f).contains (e.position))
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
                    dragSnapshots.push_back ({ idx, z.lowKey, z.highKey, z.rootKey });
                }

            dragMode = hitTestZoneMode (r, e.position);
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
            z.lowKey  = s.lowKey + d;
            z.highKey = s.highKey + d;
            z.rootKey = juce::jlimit (0, 120, s.rootKey + d); // Root key moves together automatically!
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

// ==============================================================================
// WaveEditorComponent (HD Min/Max Peaks + Real-Time Tapered Fade-In/Out + Crossfade Shade + Playhead)
// ==============================================================================
WaveEditorComponent::WaveEditorComponent (RythmeAudioProcessor& p) : processor (p)
{
    theme = RythmeAudioProcessor::getThemePalette (0);
}

void WaveEditorComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll (theme.bg);
    drawSketchRect (g, area.toFloat().reduced (0.5f), 4.0f, 1.3f, theme.border);

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

    float wF = static_cast<float> (area.getWidth());
    float startX = area.getX() + z.startNorm * wF;
    float endX = area.getX() + z.endNorm * wF;
    float lStartX = area.getX() + z.loopStartNorm * wF;
    float lEndX = area.getX() + z.loopEndNorm * wF;
    float spanW = std::max (4.0f, endX - startX);
    float fadeInX = startX + z.fadeInNorm * spanW;
    float fadeOutX = endX - z.fadeOutNorm * spanW;

    // Trimmed regions dim overlay
    g.setColour (juce::Colour (0x66000000));
    g.fillRect (static_cast<float> (area.getX()), static_cast<float> (area.getY()), startX - area.getX(), static_cast<float> (area.getHeight()));
    g.fillRect (endX, static_cast<float> (area.getY()), area.getRight() - endX, static_cast<float> (area.getHeight()));

    if (z.sampleLoop && lEndX > lStartX)
    {
        g.setColour (theme.accent.withAlpha (0.14f));
        g.fillRect (lStartX, static_cast<float> (area.getY()), lEndX - lStartX, static_cast<float> (area.getHeight()));

        // Highlight the Equal-Power Loop Crossfade region on the right end of loop
        float xfW = std::min ((lEndX - lStartX) * 0.45f, (lEndX - lStartX) * z.loopCrossfade);
        if (xfW > 2.0f)
        {
            g.setColour (theme.accent.withAlpha (0.24f));
            g.fillRect (lEndX - xfW, static_cast<float> (area.getY()), xfW, static_cast<float> (area.getHeight()));
        }
    }

    // HD Min/Max Peak Waveform with Real-Time Tapered Fade-In & Fade-Out Amplitude Envelope!
    float midY = static_cast<float> (area.getCentreY());
    float halfH = area.getHeight() * 0.42f;
    int pxW = std::max (1, area.getWidth());
    double sampsPerPx = static_cast<double> (numSamples) / static_cast<double> (pxW);

    g.setColour (theme.text);
    for (int x = 0; x < pxW; ++x)
    {
        float curX = static_cast<float> (area.getX() + x);
        float envGain = 1.0f;
        if (curX < startX || curX > endX)
            envGain = 0.28f;
        else
        {
            if (z.fadeInNorm > 0.002f && curX < fadeInX)
            {
                float t = juce::jlimit (0.0f, 1.0f, (curX - startX) / std::max (1.0f, fadeInX - startX));
                envGain *= t * t;
            }
            if (z.fadeOutNorm > 0.002f && curX > fadeOutX)
            {
                float t = juce::jlimit (0.0f, 1.0f, (endX - curX) / std::max (1.0f, endX - fadeOutX));
                envGain *= t * t;
            }
        }

        int s0 = juce::jlimit (0, numSamples - 1, static_cast<int> (x * sampsPerPx));
        int s1 = juce::jlimit (s0 + 1, numSamples, static_cast<int> ((x + 1) * sampsPerPx));
        int stride = std::max (1, (s1 - s0) / 16);
        float minV = 0.0f, maxV = 0.0f;
        for (int s = s0; s < s1; s += stride)
        {
            float v = data[s];
            if (v < minV) minV = v;
            if (v > maxV) maxV = v;
        }
        float yTop = midY - maxV * halfH * envGain;
        float yBot = midY - minV * halfH * envGain;
        g.drawVerticalLine (area.getX() + x, yTop, std::max (yTop + 1.0f, yBot));
    }

    // Fade-In / Fade-Out curve guides + draggable top corner handles
    float topLineY = midY - halfH;
    g.setColour (theme.accent);
    if (z.fadeInNorm > 0.001f)
        g.drawLine (startX, midY, fadeInX, topLineY, 1.5f);
    if (z.fadeOutNorm > 0.001f)
        g.drawLine (fadeOutX, topLineY, endX, midY, 1.5f);

    g.fillRoundedRectangle (fadeInX - 4.0f, topLineY - 4.0f, 8.0f, 8.0f, 2.0f);
    g.fillRoundedRectangle (fadeOutX - 4.0f, topLineY - 4.0f, 8.0f, 8.0f, 2.0f);

    // Start / End lines
    g.setColour (theme.text);
    g.drawVerticalLine (static_cast<int> (startX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
    g.drawVerticalLine (static_cast<int> (endX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));

    // Loop Start / End markers
    if (z.sampleLoop)
    {
        g.setColour (theme.accent);
        g.drawVerticalLine (static_cast<int> (lStartX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
        g.drawVerticalLine (static_cast<int> (lEndX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
        g.setFont (9.0f);
        g.drawText ("LS", static_cast<int> (lStartX) + 2, area.getY() + 2, 18, 12, juce::Justification::left);
        g.drawText ("LE", static_cast<int> (lEndX) - 20, area.getY() + 2, 18, 12, juce::Justification::right);
    }

    // Live Voice Playhead Line
    float phNorm = processor.selectedZonePlayheadNorm.load();
    if (phNorm >= 0.0f && phNorm <= 1.0f)
    {
        float phX = area.getX() + phNorm * wF;
        g.setColour (juce::Colour (0xffffb300));
        g.drawVerticalLine (static_cast<int> (phX), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
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
    float topLineY = getHeight() * 0.5f - getHeight() * 0.42f;

    if (std::abs (e.x - fadeInX) <= 10.0f && std::abs (e.y - topLineY) <= 16.0f) { activeDrag = DragTarget::FadeIn; return; }
    if (std::abs (e.x - fadeOutX) <= 10.0f && std::abs (e.y - topLineY) <= 16.0f) { activeDrag = DragTarget::FadeOut; return; }

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
    {
        z.startNorm = juce::jlimit (0.0f, z.endNorm - 0.02f, rawNormX);
        z.loopStartNorm = juce::jlimit (z.startNorm, z.endNorm - 0.02f, z.loopStartNorm);
        z.loopEndNorm   = juce::jlimit (z.loopStartNorm + 0.02f, z.endNorm, z.loopEndNorm);
    }
    else if (activeDrag == DragTarget::End)
    {
        z.endNorm = juce::jlimit (z.startNorm + 0.02f, 1.0f, rawNormX);
        z.loopEndNorm   = juce::jlimit (z.startNorm + 0.02f, z.endNorm, z.loopEndNorm);
        z.loopStartNorm = juce::jlimit (z.startNorm, z.loopEndNorm - 0.02f, z.loopStartNorm);
    }
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
        z.fadeInNorm = juce::jlimit (0.0f, 0.48f, (rawNormX - z.startNorm) / span);
    }
    else if (activeDrag == DragTarget::FadeOut)
    {
        float span = std::max (0.02f, z.endNorm - z.startNorm);
        z.fadeOutNorm = juce::jlimit (0.0f, 0.48f, (z.endNorm - rawNormX) / span);
    }

    repaint();
}

void WaveEditorComponent::mouseUp (const juce::MouseEvent&) { activeDrag = DragTarget::None; }

// ==============================================================================
// RhythmSequencerComponent
// ==============================================================================
RhythmSequencerComponent::RhythmSequencerComponent (RythmeAudioProcessor& p) : processor (p)
{
    theme = RythmeAudioProcessor::getThemePalette (0);
}

float RhythmSequencerComponent::snapToGrid (float normX) const
{
    int divs = processor.getGridDivisions();
    float step = 1.0f / static_cast<float> (divs);
    return juce::jlimit (0.0f, 1.0f - step * 0.25f, std::round (normX / step) * step);
}

void RhythmSequencerComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll (theme.bg);
    drawSketchRect (g, area.toFloat().reduced (0.5f), 4.0f, 1.3f, theme.border);

    int divs = processor.getGridDivisions();
    float stepW = area.getWidth() / static_cast<float> (divs);
    for (int i = 1; i < divs; ++i)
    {
        bool isBeat = (divs >= 4 && (i % (divs / 4) == 0));
        g.setColour (isBeat ? theme.mutedText.withAlpha (0.45f) : theme.grid);
        g.drawVerticalLine (static_cast<int> (area.getX() + i * stepW), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
    }

    {
        const juce::ScopedLock sl (processor.zoneLock);
        if (! processor.zones.empty() && processor.selectedZoneIndex >= 0 && processor.selectedZoneIndex < static_cast<int> (processor.zones.size()))
        {
            const auto& z = processor.zones[static_cast<size_t> (processor.selectedZoneIndex)];
            for (const auto& ev : z.seqEvents)
            {
                float cx = area.getX() + ev.position * area.getWidth();
                float barH = ev.velocity * (area.getHeight() - 18.0f);
                float topY = area.getBottom() - barH;
                g.setColour (theme.accent.withAlpha (0.85f));
                g.drawLine (cx, static_cast<float> (area.getBottom()), cx, topY, 2.2f);
                g.setColour (theme.accent);
                g.fillEllipse (cx - 4.5f, topY - 4.5f, 9.0f, 9.0f);
            }
        }
    }

    float ph = processor.seqPlayheadNorm.load();
    if (ph > 0.0001f)
    {
        float px = area.getX() + ph * area.getWidth();
        g.setColour (theme.accent);
        g.drawVerticalLine (static_cast<int> (px), static_cast<float> (area.getY()), static_cast<float> (area.getBottom()));
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

// ==============================================================================
// LayoutCustomizeOverlay
// ==============================================================================
LayoutCustomizeOverlay::LayoutCustomizeOverlay (std::function<void(const juce::String&)> onDeleteKey,
                                                std::function<void()> onBoundsChangedCb)
    : onDeleteCallback (std::move (onDeleteKey)), onBoundsChangedCallback (std::move (onBoundsChangedCb))
{
    theme = RythmeAudioProcessor::getThemePalette (0);
}

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
    g.fillAll (juce::Colour (0x18000000));

    if (gridStepPx > 1)
    {
        g.setColour (theme.accent.withAlpha (0.20f));
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
        g.setColour (theme.accent.withAlpha (selected ? 0.24f : 0.09f));
        g.fillRect (clipped);
        g.setColour (selected ? theme.accent : theme.border.withAlpha (0.85f));
        g.drawRect (clipped, selected ? 1.6f : 1.1f);

        juce::Path corner;
        corner.startNewSubPath (clipped.getRight() - 8.0f, clipped.getBottom());
        corner.lineTo (clipped.getRight(), clipped.getBottom() - 8.0f);
        corner.lineTo (clipped.getRight(), clipped.getBottom());
        corner.closeSubPath();
        g.setColour (theme.accent);
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
    if (activeIdx >= 0 && onBoundsChangedCallback)
        onBoundsChangedCallback();
    repaint();
}

// ==============================================================================
// MainPluginView
// ==============================================================================
void MainPluginView::applyAllStyles()
{
    auto pal = RythmeAudioProcessor::getThemePalette (audioProcessor.uiThemeId);
    customLnf.applyTheme (pal);

    leftScaleBtn.setThemePalette (pal);
    presetBar.setThemePalette (pal);
    cpuRamBadge.setThemePalette (pal);
    editModeBtn.setThemePalette (pal);
    topGearBtn.setThemePalette (pal);

    topIoMasterBar.setThemePalette (pal);
    mainWaveDisplay.setThemePalette (pal);
    rhythmSeqHeaderLbl.setColour (juce::Label::textColourId, pal.text);
    seqComp.setThemePalette (pal);
    gridUnit.setThemePalette (pal);

    editKeyMapper.setThemePalette (pal);
    waveEditor.setThemePalette (pal);
    adsrGraph.setThemePalette (pal);
    fxCardRack.setThemePalette (pal);
    fxDisplay.setThemePalette (pal);
    customizeOverlay.setThemePalette (pal);
    pitchModWheels.setThemePalette (pal);
    bottomPiano.setThemePalette (pal);

    groupNameEdit.applyColourToAllText (pal.text);

    int fStyle = juce::jlimit (1, 10, audioProcessor.faderStyleId);
    auto fCol = (audioProcessor.faderColourIdx == 0 && audioProcessor.uiThemeId == 0)
              ? pal.accent
              : RythmeAudioProcessor::getPaletteColour (audioProcessor.faderColourIdx);
    StyledHorizontalFader* allFaders[] = {
        &shuffleFader, &timeRandFader, &velRandFader,
        &grpVolFader, &crossfadeFader,
        &fxP1, &fxP2, &fxP3, &fxP4, &fxP5
    };
    for (auto* hf : allFaders)
    {
        hf->setThemePalette (pal);
        hf->setStyleAndColour (fStyle, fCol);
    }

    grpPanFader.setThemePalette (pal);
    grpPanFader.setStyleAndColour (fStyle, fCol);
    panFader.setThemePalette (pal);
    panFader.setStyleAndColour (fStyle, fCol);

    int kStyle = juce::jlimit (1, 10, audioProcessor.knobStyleId);
    auto kCol = (audioProcessor.knobColourIdx == 0 && audioProcessor.uiThemeId == 0)
              ? pal.border
              : RythmeAudioProcessor::getPaletteColour (audioProcessor.knobColourIdx);
    DragTypeNumberBox* allBoxes[] = {
        &bpmBox, &glideBox, &pitchBox, &volBox,
        &attackBox, &decayBox, &sustainBox, &releaseBox
    };
    for (auto* bx : allBoxes)
    {
        bx->setThemePalette (pal);
        bx->setStyleAndColour (kStyle, kCol);
    }

    int sStyle = juce::jlimit (1, 10, audioProcessor.switchStyleId);
    auto sCol = (audioProcessor.switchColourIdx == 0 && audioProcessor.uiThemeId == 0)
              ? pal.accent
              : RythmeAudioProcessor::getPaletteColour (audioProcessor.switchColourIdx);
    StyledSwitchButton* allSwitches[] = {
        &playPauseBtn, &hostSyncBtn, &seqLoopBtn, &seqClearBtn,
        &addGroupBtn, &delGroupBtn, &groupMuteBtn, &groupSoloBtn, &selGroupOnlyBtn,
        &frrBtn, &fvlBtn, &pitchTrackBtn, &playModeBtn, &selectAllZonesBtn, &delZoneBtn, &autoGridBtn,
        &wavePlayBtn, &delLayerBtn, &realRRBtn, &sampleLoopBtn, &autoLoopZeroBtn, &oneShotBtn,
        &reverseBtn, &normalizeBtn, &resetZoneBtn,
        &fxBypassBtn,
        &gearDeleteBtn, &gearResetBtn, &gearDoneBtn
    };
    for (auto* sw : allSwitches)
    {
        sw->setThemePalette (pal);
        sw->setStyleAndColour (sStyle, sCol);
    }

    sendLookAndFeelChange();
    repaint();
}

MainPluginView::~MainPluginView()
{
    if (layoutEditMode)
    {
        saveCustomizedBoundsFromOverlay();
        audioProcessor.saveUiLayoutToDisk();
    }
    setLookAndFeel (nullptr);
}

MainPluginView::MainPluginView (RythmeAudioProcessor& p, std::function<void(float)> onScaleSelected)
    : audioProcessor (p), scaleCallback (std::move (onScaleSelected)),
      leftScaleBtn (p, onScaleSelected),
      presetBar (p,
                 [this]() { savePresetAction(); },
                 [this] (const juce::File& f) { loadLibraryItem (f); },
                 [this]() { chooseRootLibraryFolder(); },
                 [this]() { browseLoadPresetAction(); }),
      cpuRamBadge (p),
      topIoMasterBar (p),
      mainWaveDisplay (p), seqComp (p), editKeyMapper (p), waveEditor (p),
      adsrGraph (p), fxCardRack (p), fxDisplay (p),
      customizeOverlay ([this] (const juce::String& deletedKey)
      {
          if (activeTab == ActiveTab::Main) audioProcessor.mainHiddenKeys.insert (deletedKey);
          else audioProcessor.editHiddenKeys.insert (deletedKey);
          audioProcessor.saveUiLayoutToDisk();
      },
      [this]()
      {
          saveCustomizedBoundsFromOverlay();
          audioProcessor.saveUiLayoutToDisk();
      }),
      pitchModWheels (p),
      bottomPiano (p)
{
    setLookAndFeel (&customLnf);

    addAndMakeVisible (leftScaleBtn);
    addAndMakeVisible (presetBar);
    addAndMakeVisible (cpuRamBadge);

    addAndMakeVisible (editModeBtn);
    editModeBtn.setTooltip ("Toggle EDIT Page");
    editModeBtn.onClick = [this]()
    {
        setActiveTab (editModeBtn.getToggleState() ? ActiveTab::Edit : ActiveTab::Main);
    };

    addAndMakeVisible (topGearBtn);
    topGearBtn.setTooltip ("Customize Layout, Themes, Styles & Colours");
    topGearBtn.onClick = [this]() { enterLayoutCustomizeMode (! layoutEditMode); };

    // ---------------- MAIN TAB ----------------
    addAndMakeVisible (topIoMasterBar);
    addAndMakeVisible (mainWaveDisplay);

    rhythmSeqHeaderLbl.setText ("RHYTHM SEQUENCER", juce::dontSendNotification);
    rhythmSeqHeaderLbl.setFont (juce::FontOptions (12.5f, juce::Font::bold));
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

    addAndMakeVisible (gridUnit);
    gridUnit.combo.addItemList ({ "1/4", "1/8", "1/12 (T)", "1/16", "1/24 (T)", "1/32", "1/48 (T)", "1/64" }, 1);
    gridAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (audioProcessor.apvts, "grid", gridUnit.combo);
    gridUnit.combo.onChange = [this]() { seqComp.repaint(); };

    addAndMakeVisible (seqLoopBtn);
    seqLoopAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "seqLoop", seqLoopBtn);

    addAndMakeVisible (seqClearBtn);
    seqClearBtn.onClick = [this]() { audioProcessor.clearSelectedZoneRhythm(); seqComp.repaint(); };

    // ---------------- EDIT TAB ----------------
    addChildComponent (editViewport);
    editViewport.setViewedComponent (&editContent, false);
    editViewport.setScrollBarsShown (true, false);

    editContent.addAndMakeVisible (addGroupBtn);
    addGroupBtn.onClick = [this]()
    {
        audioProcessor.createEmptyGroup();
        refreshGroupBoxes(); syncGroupControls(); syncFxControls();
        editKeyMapper.repaint();
    };

    editContent.addAndMakeVisible (delGroupBtn);
    delGroupBtn.onClick = [this]()
    {
        if (audioProcessor.deleteSelectedGroup())
        {
            refreshGroupBoxes(); syncGroupControls(); syncFxControls(); syncPerZoneControls();
            editKeyMapper.repaint();
        }
    };

    editContent.addAndMakeVisible (groupSelectBox);
    groupSelectBox.onChange = [this]()
    {
        int id = groupSelectBox.getSelectedId();
        if (id > 0)
        {
            audioProcessor.selectedGroupIndex = id - 1;
            syncGroupControls(); syncFxControls();
            editKeyMapper.repaint();
        }
    };

    editContent.addAndMakeVisible (groupNameEdit);
    groupNameEdit.setMultiLine (false);
    groupNameEdit.onTextChange = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedGroupIndex >= 0 && audioProcessor.selectedGroupIndex < static_cast<int> (audioProcessor.groups.size()))
        {
            audioProcessor.groups[static_cast<size_t> (audioProcessor.selectedGroupIndex)].name = groupNameEdit.getText();
            refreshGroupBoxes();
        }
    };

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
        bool on = selGroupOnlyBtn.getToggleState();
        audioProcessor.selectGroupOnly.store (on);
        editKeyMapper.setFilterToSelectedGroup (on);
    };

    editContent.addAndMakeVisible (groupKsBox);
    groupKsBox.addItem ("KS: None", 1);
    for (int n = 0; n <= 120; ++n)
        groupKsBox.addItem ("KS: " + juce::MidiMessage::getMidiNoteName (n, true, true, 5), n + 2);
    groupKsBox.onChange = [this]()
    {
        int id = groupKsBox.getSelectedId();
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedGroupIndex >= 0 && audioProcessor.selectedGroupIndex < static_cast<int> (audioProcessor.groups.size()))
            audioProcessor.groups[static_cast<size_t> (audioProcessor.selectedGroupIndex)].keyswitchNote = (id <= 1) ? -1 : (id - 2);
        editKeyMapper.repaint();
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

    // Mapping Header Row (FRR, FVL, PITCH, POLY/MONO/LEGATO, Glide, ALL, Del, Auto Grid, Vel Grid, Zone Colour)
    editContent.addAndMakeVisible (frrBtn);
    frrAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "frr", frrBtn);

    editContent.addAndMakeVisible (fvlBtn);
    fvlAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "fvl", fvlBtn);

    editContent.addAndMakeVisible (pitchTrackBtn);
    pitchTrackAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "pitchTrack", pitchTrackBtn);

    editContent.addAndMakeVisible (playModeBtn);
    playModeBtn.setTooltip ("Cycle Play Mode: POLY -> MONO -> LEGATO");
    playModeBtn.onClick = [this]()
    {
        int cur = static_cast<int> (audioProcessor.apvts.getRawParameterValue ("playMode")->load());
        int nxt = (cur + 1) % 3;
        if (auto* p = audioProcessor.apvts.getParameter ("playMode"))
            p->setValueNotifyingHost (nxt / 2.0f);
        syncPlayModeButton();
    };

    editContent.addAndMakeVisible (glideBox);
    glideBox.setTooltip ("Legato / Portamento Glide Time (ms)");
    glideBox.setValue (audioProcessor.apvts.getRawParameterValue ("glideMs")->load(), false);
    glideBox.onValueChanged = [this] (double v)
    {
        if (auto* p = audioProcessor.apvts.getParameter ("glideMs"))
            p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (v)));
    };
    syncPlayModeButton();

    editContent.addAndMakeVisible (selectAllZonesBtn);
    selectAllZonesBtn.onClick = [this]() { editKeyMapper.selectAllZones(); };

    editContent.addAndMakeVisible (delZoneBtn);
    delZoneBtn.onClick = [this]() { editKeyMapper.deleteSelectedZones(); syncPerZoneControls(); };

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

    // Wave Header Row (Play/Stop, Track Dropdown, Del Layer, RR, LOOP, Z-Cross, 1-Shot, Choke, REV, NORM, Reset)
    editContent.addAndMakeVisible (wavePlayBtn);
    wavePlayBtn.setTooltip ("Preview / Stop Selected Zone");
    wavePlayBtn.onClick = [this]()
    {
        audioProcessor.toggleZonePreview (audioProcessor.selectedZoneIndex);
        bool active = audioProcessor.isZonePreviewActive();
        wavePlayBtn.setButtonText (active ? "Stop" : "Play");
        wavePlayBtn.setToggleState (active, juce::dontSendNotification);
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

    editContent.addAndMakeVisible (delLayerBtn);
    delLayerBtn.setTooltip ("Delete Selected Sample Layer");
    delLayerBtn.onClick = [this]()
    {
        int zIdx = audioProcessor.selectedZoneIndex;
        int lIdx = trackSelectBox.getSelectedId() - 1;
        if (audioProcessor.deleteLayerFromZone (zIdx, lIdx))
        {
            syncPerZoneControls();
            editKeyMapper.repaint();
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
    autoLoopZeroBtn.setTooltip ("Snap Loop Points to Phase-Matched Rising Zero-Crossings");
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
            }
        }
        waveEditor.repaint();
    };

    editContent.addAndMakeVisible (oneShotBtn);
    oneShotBtn.setTooltip ("One-Shot Mode (Full sample plays through on trigger, ideal for drums/percussion)");
    oneShotBtn.onClick = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].oneShot = oneShotBtn.getToggleState();
    };

    editContent.addAndMakeVisible (chokeGroupBox);
    chokeGroupBox.setTooltip ("Voice Choke Group (e.g., Open Hat cuts Closed Hat)");
    chokeGroupBox.addItem ("Chk: Off", 1);
    for (int cg = 1; cg <= 8; ++cg)
        chokeGroupBox.addItem ("Chk: " + juce::String (cg), cg + 1);
    chokeGroupBox.setSelectedId (1, juce::dontSendNotification);
    chokeGroupBox.onChange = [this]()
    {
        int cg = juce::jlimit (0, 8, chokeGroupBox.getSelectedId() - 1);
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].chokeGroup = cg;
    };

    editContent.addAndMakeVisible (crossfadeFader);
    crossfadeFader.onValueChanged = [this] (double v)
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
            audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)].loopCrossfade = static_cast<float> (v);
        waveEditor.repaint();
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
    resetZoneBtn.onClick = [this]() { audioProcessor.resetSelectedZoneDefaults(); syncPerZoneControls(); waveEditor.repaint(); };

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

    // Large Interactive ADSR Section
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

    adsrGraph.onEnvelopeChanged = [this]()
    {
        const juce::ScopedLock sl (audioProcessor.zoneLock);
        if (audioProcessor.selectedZoneIndex >= 0 && audioProcessor.selectedZoneIndex < static_cast<int> (audioProcessor.zones.size()))
        {
            const auto& z = audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)];
            attackBox.setValue (z.attackMs, false);
            decayBox.setValue (z.decayMs, false);
            sustainBox.setValue (z.sustainPct, false);
            releaseBox.setValue (z.releaseMs, false);
        }
    };
    editContent.addAndMakeVisible (adsrGraph);

    // 7-Card Pro FX Rack + Bypass + Visualizer + 5 Parameter Faders
    editContent.addAndMakeVisible (fxCardRack);
    fxCardRack.onSlotSelected = [this] (int idx)
    {
        fxDisplay.setSlotIndex (idx);
        syncFxControls();
    };
    fxCardRack.onSlotToggled = [this] (int idx)
    {
        {
            const juce::ScopedLock sl (audioProcessor.zoneLock);
            if (audioProcessor.selectedGroupIndex >= 0 && audioProcessor.selectedGroupIndex < static_cast<int> (audioProcessor.groups.size()))
            {
                auto& slot = audioProcessor.groups[static_cast<size_t> (audioProcessor.selectedGroupIndex)]
                                 .fxSlots[static_cast<size_t> (juce::jlimit (0, SampleGroup::numFxSlots - 1, idx))];
                slot.enabled = ! slot.enabled;
                if (slot.enabled)
                    slot.prepare (audioProcessor.getCurrentSampleRate());
            }
        }
        fxCardRack.repaint();
        fxDisplay.repaint();
    };

    editContent.addAndMakeVisible (fxBypassBtn);
    fxBypassBtn.setToggleState (audioProcessor.fxBypass.load(), juce::dontSendNotification);
    fxBypassBtn.onClick = [this]() { audioProcessor.fxBypass.store (fxBypassBtn.getToggleState()); fxDisplay.repaint(); };

    editContent.addAndMakeVisible (fxDisplay);

    StyledHorizontalFader* fxFaders[5] = { &fxP1, &fxP2, &fxP3, &fxP4, &fxP5 };
    for (int k = 0; k < 5; ++k)
    {
        editContent.addAndMakeVisible (*fxFaders[k]);
        fxFaders[k]->onValueChanged = [this, k] (double v)
        {
            int sIdx = fxCardRack.getSelectedSlot();
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

    // ---------------- GEAR CUSTOMIZE CONTROLS (8 UI THEMES + GRID + STYLES/COLOURS) ----------------
    addChildComponent (gearThemeBox);
    for (int t = 0; t < RythmeAudioProcessor::numUiThemes; ++t)
        gearThemeBox.addItem (RythmeAudioProcessor::getThemePalette (t).name, t + 1);
    gearThemeBox.setSelectedId (audioProcessor.uiThemeId + 1, juce::dontSendNotification);
    gearThemeBox.onChange = [this]()
    {
        audioProcessor.uiThemeId = juce::jlimit (0, RythmeAudioProcessor::numUiThemes - 1, gearThemeBox.getSelectedId() - 1);
        applyAllStyles();
        audioProcessor.saveUiLayoutToDisk();
    };

    addChildComponent (gearGridBox);
    gearGridBox.addItemList ({ "Grid: Free", "Grid: 10x", "Grid: 20x", "Grid: 30x", "Grid: 40x", "Grid: 50x" }, 1);
    int initGridId = (audioProcessor.customizeGridStep <= 0) ? 1 : juce::jlimit (2, 6, (audioProcessor.customizeGridStep / 10) + 1);
    gearGridBox.setSelectedId (initGridId, juce::dontSendNotification);
    customizeOverlay.setGridStep (audioProcessor.customizeGridStep);
    gearGridBox.onChange = [this]()
    {
        int id = gearGridBox.getSelectedId();
        audioProcessor.customizeGridStep = (id <= 1) ? 0 : (id - 1) * 10;
        customizeOverlay.setGridStep (audioProcessor.customizeGridStep);
        audioProcessor.saveUiLayoutToDisk();
    };

    addChildComponent (gearFaderStyleBox);
    for (int i = 1; i <= 10; ++i) gearFaderStyleBox.addItem ("Fdr St " + juce::String (i), i);
    gearFaderStyleBox.setSelectedId (audioProcessor.faderStyleId, juce::dontSendNotification);
    gearFaderStyleBox.onChange = [this]()
    {
        audioProcessor.faderStyleId = juce::jlimit (1, 10, gearFaderStyleBox.getSelectedId());
        applyAllStyles();
        audioProcessor.saveUiLayoutToDisk();
    };

    addChildComponent (gearFaderColBox);
    for (int c = 0; c < RythmeAudioProcessor::numPaletteColours; ++c)
        gearFaderColBox.addItem ("Fdr: " + RythmeAudioProcessor::getPaletteColourName (c), c + 1);
    gearFaderColBox.setSelectedId (audioProcessor.faderColourIdx + 1, juce::dontSendNotification);
    gearFaderColBox.onChange = [this]()
    {
        audioProcessor.faderColourIdx = juce::jlimit (0, RythmeAudioProcessor::numPaletteColours - 1, gearFaderColBox.getSelectedId() - 1);
        applyAllStyles();
        audioProcessor.saveUiLayoutToDisk();
    };

    addChildComponent (gearKnobStyleBox);
    for (int i = 1; i <= 10; ++i) gearKnobStyleBox.addItem ("Knb St " + juce::String (i), i);
    gearKnobStyleBox.setSelectedId (audioProcessor.knobStyleId, juce::dontSendNotification);
    gearKnobStyleBox.onChange = [this]()
    {
        audioProcessor.knobStyleId = juce::jlimit (1, 10, gearKnobStyleBox.getSelectedId());
        applyAllStyles();
        audioProcessor.saveUiLayoutToDisk();
    };

    addChildComponent (gearKnobColBox);
    for (int c = 0; c < RythmeAudioProcessor::numPaletteColours; ++c)
        gearKnobColBox.addItem ("Knb: " + RythmeAudioProcessor::getPaletteColourName (c), c + 1);
    gearKnobColBox.setSelectedId (audioProcessor.knobColourIdx + 1, juce::dontSendNotification);
    gearKnobColBox.onChange = [this]()
    {
        audioProcessor.knobColourIdx = juce::jlimit (0, RythmeAudioProcessor::numPaletteColours - 1, gearKnobColBox.getSelectedId() - 1);
        applyAllStyles();
        audioProcessor.saveUiLayoutToDisk();
    };

    addChildComponent (gearSwitchStyleBox);
    for (int i = 1; i <= 10; ++i) gearSwitchStyleBox.addItem ("Sw St " + juce::String (i), i);
    gearSwitchStyleBox.setSelectedId (audioProcessor.switchStyleId, juce::dontSendNotification);
    gearSwitchStyleBox.onChange = [this]()
    {
        audioProcessor.switchStyleId = juce::jlimit (1, 10, gearSwitchStyleBox.getSelectedId());
        applyAllStyles();
        audioProcessor.saveUiLayoutToDisk();
    };

    addChildComponent (gearSwitchColBox);
    for (int c = 0; c < RythmeAudioProcessor::numPaletteColours; ++c)
        gearSwitchColBox.addItem ("Sw: " + RythmeAudioProcessor::getPaletteColourName (c), c + 1);
    gearSwitchColBox.setSelectedId (audioProcessor.switchColourIdx + 1, juce::dontSendNotification);
    gearSwitchColBox.onChange = [this]()
    {
        audioProcessor.switchColourIdx = juce::jlimit (0, RythmeAudioProcessor::numPaletteColours - 1, gearSwitchColBox.getSelectedId() - 1);
        applyAllStyles();
        audioProcessor.saveUiLayoutToDisk();
    };

    addChildComponent (gearDeleteBtn);
    gearDeleteBtn.onClick = [this]() { customizeOverlay.deleteSelectedOrMarked(); };

    addChildComponent (gearResetBtn);
    gearResetBtn.onClick = [this]()
    {
        if (activeTab == ActiveTab::Main)
        {
            audioProcessor.mainCustomBounds.clear();
            audioProcessor.mainHiddenKeys.clear();
            topIoMasterBar.setVisible (true);
            mainWaveDisplay.setVisible (true);
            rhythmSeqHeaderLbl.setVisible (true);
            seqComp.setVisible (true);
            playPauseBtn.setVisible (true);
            hostSyncBtn.setVisible (true);
            bpmBox.setVisible (true);
            shuffleFader.setVisible (true);
            timeRandFader.setVisible (true);
            velRandFader.setVisible (true);
            gridUnit.setVisible (true);
            seqLoopBtn.setVisible (true);
            seqClearBtn.setVisible (true);
        }
        else
        {
            audioProcessor.editCustomBounds.clear();
            audioProcessor.editHiddenKeys.clear();
            for (int i = 0; i < editContent.getNumChildComponents(); ++i)
                editContent.getChildComponent (i)->setVisible (true);
        }
        audioProcessor.saveUiLayoutToDisk();
        customizeOverlay.clearPendingDeletes();
        resized();
        customizeOverlay.repaint();
    };

    addChildComponent (gearDoneBtn);
    gearDoneBtn.onClick = [this]() { enterLayoutCustomizeMode (false); };

    addChildComponent (customizeOverlay);

    addAndMakeVisible (pitchModWheels);
    addAndMakeVisible (bottomPiano);

    refreshGroupBoxes();
    syncGroupControls();
    syncFxControls();
    applyAllStyles();
    setActiveTab (ActiveTab::Main);
    syncPerZoneControls();
    lastSeenStateRevision = audioProcessor.stateRevision.load();
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
    juce::String typedName = presetBar.getCurrentTypedText();
    if (typedName.isNotEmpty())
    {
        audioProcessor.libraryNameText = typedName;
        audioProcessor.currentPresetName = typedName;
    }
    juce::String suggested = typedName.isNotEmpty() ? typedName
                           : (audioProcessor.currentPresetName.isNotEmpty() ? audioProcessor.currentPresetName : "Preset");

    juce::File baseDir = audioProcessor.rootLibraryFolder.isDirectory()
        ? audioProcessor.rootLibraryFolder
        : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("KINETIK Presets");
    baseDir.createDirectory();

    auto defaultFile = baseDir.getChildFile (suggested + ".mn6x");

    fileChooser = std::make_unique<juce::FileChooser> ("Save KINETIK Preset (.mn6x)", defaultFile, "*.mn6x");
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f != juce::File())
            {
                if (! f.hasFileExtension (".mn6x")) f = f.withFileExtension (".mn6x");
                audioProcessor.libraryNameText = f.getFileNameWithoutExtension();
                audioProcessor.currentPresetName = audioProcessor.libraryNameText;
                bool ok = audioProcessor.savePresetMn6x (f);
                presetBar.refreshTextFromProcessor();
                presetBar.flashSavedFeedback (ok);
            }
        });
}

void MainPluginView::browseLoadPresetAction()
{
    juce::File baseDir = audioProcessor.rootLibraryFolder.isDirectory()
        ? audioProcessor.rootLibraryFolder
        : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("KINETIK Presets");
    if (! baseDir.isDirectory())
        baseDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    fileChooser = std::make_unique<juce::FileChooser> ("Load KINETIK Preset (.mn6x)", baseDir, "*.mn6x");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f.existsAsFile())
                loadLibraryItem (f);
        });
}

void MainPluginView::saveCustomizedBoundsFromOverlay()
{
    if (activeTab == ActiveTab::Main)
    {
        for (const auto& k : customizeOverlay.getPendingDeleteKeys())
            audioProcessor.mainHiddenKeys.insert (k);
        customizeOverlay.clearPendingDeletes();

        auto saveIfVis = [this] (const juce::String& key, juce::Component& c)
        {
            if (audioProcessor.mainHiddenKeys.count (key) == 0 && c.isVisible())
                audioProcessor.mainCustomBounds[key] = c.getBounds();
        };
        saveIfVis ("ioBar",  topIoMasterBar);
        saveIfVis ("wave",   mainWaveDisplay);
        saveIfVis ("seqHdr", rhythmSeqHeaderLbl);
        saveIfVis ("seq",    seqComp);
        saveIfVis ("play",   playPauseBtn);
        saveIfVis ("sync",   hostSyncBtn);
        saveIfVis ("bpm",    bpmBox);
        saveIfVis ("shuf",   shuffleFader);
        saveIfVis ("trnd",   timeRandFader);
        saveIfVis ("vrnd",   velRandFader);
        saveIfVis ("grid",   gridUnit);
        saveIfVis ("loop",   seqLoopBtn);
        saveIfVis ("clr",    seqClearBtn);
    }
    else
    {
        for (const auto& k : customizeOverlay.getPendingDeleteKeys())
            audioProcessor.editHiddenKeys.insert (k);
        customizeOverlay.clearPendingDeletes();

        auto saveIfVis = [this] (const juce::String& key, juce::Component& c)
        {
            if (audioProcessor.editHiddenKeys.count (key) == 0 && c.isVisible())
                audioProcessor.editCustomBounds[key] = c.getBounds();
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
        saveIfVis ("pMode",   playModeBtn);
        saveIfVis ("glide",   glideBox);
        saveIfVis ("allZ",    selectAllZonesBtn);
        saveIfVis ("delZ",    delZoneBtn);
        saveIfVis ("autoG",   autoGridBtn);
        saveIfVis ("velG",    velGridBox);
        saveIfVis ("colZ",    zoneColourBox);
        saveIfVis ("mapper",  editKeyMapper);

        saveIfVis ("wPlay",   wavePlayBtn);
        saveIfVis ("trk",     trackSelectBox);
        saveIfVis ("delLyr",  delLayerBtn);
        saveIfVis ("rr",      realRRBtn);
        saveIfVis ("sLoop",   sampleLoopBtn);
        saveIfVis ("zCross",  autoLoopZeroBtn);
        saveIfVis ("1shot",   oneShotBtn);
        saveIfVis ("choke",   chokeGroupBox);
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

        saveIfVis ("fxRack",  fxCardRack);
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
    {
        saveCustomizedBoundsFromOverlay();
        audioProcessor.saveUiLayoutToDisk();
    }

    layoutEditMode = enable;
    pitchModWheels.setVisible (! enable);
    bottomPiano.setVisible (! enable);

    gearThemeBox.setVisible (enable);
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
                { "ioBar",  &topIoMasterBar },
                { "wave",   &mainWaveDisplay },
                { "seqHdr", &rhythmSeqHeaderLbl },
                { "seq",    &seqComp },
                { "play",   &playPauseBtn },
                { "sync",   &hostSyncBtn },
                { "bpm",    &bpmBox },
                { "shuf",   &shuffleFader },
                { "trnd",   &timeRandFader },
                { "vrnd",   &velRandFader },
                { "grid",   &gridUnit },
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
                { "pMode",   &playModeBtn },
                { "glide",   &glideBox },
                { "allZ",    &selectAllZonesBtn },
                { "delZ",    &delZoneBtn },
                { "autoG",   &autoGridBtn },
                { "velG",    &velGridBox },
                { "colZ",    &zoneColourBox },
                { "mapper",  &editKeyMapper },

                { "wPlay",   &wavePlayBtn },
                { "trk",     &trackSelectBox },
                { "delLyr",  &delLayerBtn },
                { "rr",      &realRRBtn },
                { "sLoop",   &sampleLoopBtn },
                { "zCross",  &autoLoopZeroBtn },
                { "1shot",   &oneShotBtn },
                { "choke",   &chokeGroupBox },
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

                { "fxRack",  &fxCardRack },
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
        if (! audioProcessor.loadPresetMn6x (file)) return;
        syncAllUiFromProcessor();
        audioProcessor.setLoadedItemInPanel (file.getFileNameWithoutExtension(), file, -1);
    }
    else
    {
        int nextKey = juce::jlimit (0, 120, 60 + static_cast<int> (audioProcessor.zones.size()));
        audioProcessor.loadSampleIntoNewZone (file, nextKey, nextKey, nextKey);
        audioProcessor.libraryNameText = file.getFileNameWithoutExtension();
        presetBar.refreshTextFromProcessor();
        audioProcessor.setLoadedItemInPanel (file.getFileNameWithoutExtension(), file, nextKey);
        editKeyMapper.selectSingleZone (audioProcessor.selectedZoneIndex);
        syncPerZoneControls();
    }
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

void MainPluginView::syncPlayModeButton()
{
    int pm = static_cast<int> (audioProcessor.apvts.getRawParameterValue ("playMode")->load());
    playModeBtn.setButtonText (pm == 0 ? "POLY" : (pm == 1 ? "MONO" : "LEGATO"));
    playModeBtn.setToggleState (pm > 0, juce::dontSendNotification);
    glideBox.setEnabled (pm > 0);
}

void MainPluginView::syncFxControls()
{
    int sIdx = fxCardRack.getSelectedSlot();
    const juce::ScopedLock sl (audioProcessor.zoneLock);
    if (audioProcessor.selectedGroupIndex < 0 || audioProcessor.selectedGroupIndex >= static_cast<int> (audioProcessor.groups.size())) return;
    const auto& slot = audioProcessor.groups[static_cast<size_t> (audioProcessor.selectedGroupIndex)].fxSlots[static_cast<size_t> (sIdx)];

    StyledHorizontalFader* fxFaders[5] = { &fxP1, &fxP2, &fxP3, &fxP4, &fxP5 };
    float vals[5] = { slot.p1, slot.p2, slot.p3, slot.p4, slot.p5 };
    for (int k = 0; k < 5; ++k)
    {
        fxFaders[k]->setLabelText (SimpleFxSlot::getParamLabel (slot.type, k));
        fxFaders[k]->setValue (vals[k], false);
    }
    fxCardRack.repaint();
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

    topIoMasterBar.setVisible (isMain && audioProcessor.mainHiddenKeys.count ("ioBar") == 0);
    mainWaveDisplay.setVisible (isMain && audioProcessor.mainHiddenKeys.count ("wave") == 0);
    rhythmSeqHeaderLbl.setVisible (isMain && audioProcessor.mainHiddenKeys.count ("seqHdr") == 0);
    seqComp.setVisible (isMain && audioProcessor.mainHiddenKeys.count ("seq") == 0);
    playPauseBtn.setVisible (isMain && audioProcessor.mainHiddenKeys.count ("play") == 0);
    hostSyncBtn.setVisible (isMain && audioProcessor.mainHiddenKeys.count ("sync") == 0);
    bpmBox.setVisible (isMain && audioProcessor.mainHiddenKeys.count ("bpm") == 0);
    shuffleFader.setVisible (isMain && audioProcessor.mainHiddenKeys.count ("shuf") == 0);
    timeRandFader.setVisible (isMain && audioProcessor.mainHiddenKeys.count ("trnd") == 0);
    velRandFader.setVisible (isMain && audioProcessor.mainHiddenKeys.count ("vrnd") == 0);
    gridUnit.setVisible (isMain && audioProcessor.mainHiddenKeys.count ("grid") == 0);
    seqLoopBtn.setVisible (isMain && audioProcessor.mainHiddenKeys.count ("loop") == 0);
    seqClearBtn.setVisible (isMain && audioProcessor.mainHiddenKeys.count ("clr") == 0);

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
    if (! next)
        audioProcessor.stopAllVoicesSmooth();
}

void MainPluginView::syncPerZoneControls()
{
    trackSelectBox.clear (juce::dontSendNotification);
    const juce::ScopedLock sl (audioProcessor.zoneLock);
    if (audioProcessor.selectedZoneIndex < 0 || audioProcessor.selectedZoneIndex >= static_cast<int> (audioProcessor.zones.size()))
    {
        delLayerBtn.setEnabled (false);
        realRRBtn.setToggleState (false, juce::dontSendNotification);
        sampleLoopBtn.setToggleState (false, juce::dontSendNotification);
        autoLoopZeroBtn.setToggleState (true, juce::dontSendNotification);
        oneShotBtn.setToggleState (false, juce::dontSendNotification);
        chokeGroupBox.setSelectedId (1, juce::dontSendNotification);
        reverseBtn.setToggleState (false, juce::dontSendNotification);
        zoneColourBox.setSelectedId (1, juce::dontSendNotification);
        adsrGraph.repaint();
        return;
    }

    const auto& z = audioProcessor.zones[static_cast<size_t> (audioProcessor.selectedZoneIndex)];
    for (int i = 0; i < static_cast<int> (z.layers.size()); ++i)
        trackSelectBox.addItem (z.layers[static_cast<size_t> (i)].name, i + 1);
    if (! z.layers.empty())
        trackSelectBox.setSelectedId (juce::jlimit (0, static_cast<int> (z.layers.size()) - 1, z.selectedLayerIndex) + 1, juce::dontSendNotification);
    delLayerBtn.setEnabled (! z.layers.empty());

    zoneColourBox.setSelectedId (z.colourIndex + 1, juce::dontSendNotification);
    realRRBtn.setToggleState (z.realRREnabled, juce::dontSendNotification);
    sampleLoopBtn.setToggleState (z.sampleLoop, juce::dontSendNotification);
    autoLoopZeroBtn.setToggleState (z.autoLoopZeroCross, juce::dontSendNotification);
    oneShotBtn.setToggleState (z.oneShot, juce::dontSendNotification);
    chokeGroupBox.setSelectedId (juce::jlimit (0, 8, z.chokeGroup) + 1, juce::dontSendNotification);
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
    waveEditor.repaint();
}

void MainPluginView::syncAllUiFromProcessor()
{
    lastSeenStateRevision = audioProcessor.stateRevision.load();
    presetBar.refreshTextFromProcessor();
    bpmBox.setValue (audioProcessor.apvts.getRawParameterValue ("internalBpm")->load(), false);
    shuffleFader.setValue (audioProcessor.apvts.getRawParameterValue ("shuffle")->load(), false);
    timeRandFader.setValue (audioProcessor.apvts.getRawParameterValue ("timeRand")->load(), false);
    velRandFader.setValue (audioProcessor.apvts.getRawParameterValue ("velRand")->load(), false);
    glideBox.setValue (audioProcessor.apvts.getRawParameterValue ("glideMs")->load(), false);
    syncPlayModeButton();

    fxBypassBtn.setToggleState (audioProcessor.fxBypass.load(), juce::dontSendNotification);
    selGroupOnlyBtn.setToggleState (audioProcessor.selectGroupOnly.load(), juce::dontSendNotification);
    editKeyMapper.setFilterToSelectedGroup (audioProcessor.selectGroupOnly.load());
    editKeyMapper.selectSingleZone (audioProcessor.selectedZoneIndex);

    gearThemeBox.setSelectedId (audioProcessor.uiThemeId + 1, juce::dontSendNotification);
    gearFaderStyleBox.setSelectedId (audioProcessor.faderStyleId, juce::dontSendNotification);
    gearFaderColBox.setSelectedId (audioProcessor.faderColourIdx + 1, juce::dontSendNotification);
    gearKnobStyleBox.setSelectedId (audioProcessor.knobStyleId, juce::dontSendNotification);
    gearKnobColBox.setSelectedId (audioProcessor.knobColourIdx + 1, juce::dontSendNotification);
    gearSwitchStyleBox.setSelectedId (audioProcessor.switchStyleId, juce::dontSendNotification);
    gearSwitchColBox.setSelectedId (audioProcessor.switchColourIdx + 1, juce::dontSendNotification);
    int gridId = (audioProcessor.customizeGridStep <= 0) ? 1 : juce::jlimit (2, 6, (audioProcessor.customizeGridStep / 10) + 1);
    gearGridBox.setSelectedId (gridId, juce::dontSendNotification);
    customizeOverlay.setGridStep (audioProcessor.customizeGridStep);

    refreshGroupBoxes();
    syncGroupControls();
    syncFxControls();
    syncPerZoneControls();
    applyAllStyles();
    resized();
}

void MainPluginView::repaintDynamicAreas()
{
    if (lastSeenStateRevision != audioProcessor.stateRevision.load())
        syncAllUiFromProcessor();

    bool prevRunning = audioProcessor.isZonePreviewActive();
    if (wavePlayBtn.getToggleState() != prevRunning)
    {
        wavePlayBtn.setToggleState (prevRunning, juce::dontSendNotification);
        wavePlayBtn.setButtonText (prevRunning ? "Stop" : "Play");
    }

    // Smart Low-CPU UI Repaint: full 24Hz when active; throttled ~4Hz when idle!
    bool isActive = (audioProcessor.outputMeterL.load() > 0.002f)
                 || (audioProcessor.outputMeterR.load() > 0.002f)
                 || (audioProcessor.lastPlayedNote.load() >= 0)
                 || audioProcessor.internalPlaying.load()
                 || prevRunning;

    if (! isActive)
    {
        if (++idleRepaintSkip < 6)
            return;
    }
    idleRepaintSkip = 0;

    cpuRamBadge.repaint();
    if (topIoMasterBar.isVisible()) topIoMasterBar.repaint();
    if (mainWaveDisplay.isVisible()) mainWaveDisplay.repaint();
    if (seqComp.isVisible()) seqComp.repaint();
    if (editKeyMapper.isVisible()) editKeyMapper.repaint();
    if (waveEditor.isVisible()) waveEditor.repaint();
    if (fxDisplay.isVisible()) fxDisplay.repaint();
    pitchModWheels.repaint();
    bottomPiano.repaint();
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
                loadLibraryItem (f);
                return;
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

    for (const auto& path : files)
    {
        juce::File f (path);
        if (f.hasFileExtension (".mn6x"))
        {
            loadLibraryItem (f);
            return;
        }
        else
        {
            audioProcessor.loadSampleIntoNewZone (f, targetKey, targetKey, targetKey);
            targetKey = juce::jlimit (0, 120, targetKey + 1);
        }
    }
    editKeyMapper.selectSingleZone (audioProcessor.selectedZoneIndex);
    syncPerZoneControls(); repaintDynamicAreas();
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
    int innerW = contentW - 8; // Exact 738px on default width
    int y = 4;

    // Row 1: Group Bar (all uniform 26px height, spanning full width with zero dead gap)
    auto grpTop = juce::Rectangle<int> (4, y, innerW, 28);
    applySavedOrDefault (addGroupBtn,     audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "addGrp",  grpTop.removeFromLeft (56).reduced (1));
    applySavedOrDefault (delGroupBtn,     audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "delGrp",  grpTop.removeFromLeft (62).reduced (1));
    applySavedOrDefault (groupSelectBox,  audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "grpSel",  grpTop.removeFromLeft (114).reduced (1));
    applySavedOrDefault (groupMuteBtn,    audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "grpM",    grpTop.removeFromRight (32).reduced (1));
    applySavedOrDefault (groupSoloBtn,    audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "grpS",    grpTop.removeFromRight (32).reduced (1));
    applySavedOrDefault (selGroupOnlyBtn, audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "selGrp",  grpTop.removeFromRight (64).reduced (1));
    applySavedOrDefault (groupKsBox,      audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "grpKs",   grpTop.removeFromRight (96).reduced (1));
    applySavedOrDefault (groupNameEdit,   audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "grpName", grpTop.reduced (2, 1));
    y += 32;

    // Row 2: Group Volume & Group Pan
    auto grpCtrlRow = juce::Rectangle<int> (4, y, innerW, 36);
    int halfGW = grpCtrlRow.getWidth() / 2;
    applySavedOrDefault (grpVolFader, audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "grpVol", grpCtrlRow.removeFromLeft (halfGW).reduced (3, 0));
    applySavedOrDefault (grpPanFader, audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "grpPan", grpCtrlRow.reduced (3, 0));
    y += 40;

    // Row 3: Key Mapper Header (FRR, FVL, PITCH, POLY/MONO/LEGATO, Glide, ALL, Del, Auto Grid, Vel Grid, Zone Colour)
    auto mapHdr = juce::Rectangle<int> (4, y, innerW, 28);
    applySavedOrDefault (frrBtn,            audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "frr",    mapHdr.removeFromLeft (42).reduced (1));
    applySavedOrDefault (fvlBtn,            audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "fvl",    mapHdr.removeFromLeft (42).reduced (1));
    applySavedOrDefault (pitchTrackBtn,     audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "pitchT", mapHdr.removeFromLeft (50).reduced (1));
    applySavedOrDefault (playModeBtn,       audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "pMode",  mapHdr.removeFromLeft (62).reduced (1));
    applySavedOrDefault (glideBox,          audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "glide",  mapHdr.removeFromLeft (56).reduced (1));
    applySavedOrDefault (selectAllZonesBtn, audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "allZ",   mapHdr.removeFromLeft (38).reduced (1));
    applySavedOrDefault (delZoneBtn,        audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "delZ",   mapHdr.removeFromLeft (38).reduced (1));
    applySavedOrDefault (autoGridBtn,       audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "autoG",  mapHdr.removeFromLeft (68).reduced (1));
    applySavedOrDefault (zoneColourBox,     audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "colZ",   mapHdr.removeFromRight (108).reduced (1));
    applySavedOrDefault (velGridBox,        audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "velG",   mapHdr.reduced (1));
    y += 30;

    // Row 4: Key Mapper Grid
    applySavedOrDefault (editKeyMapper, audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "mapper", { 4, y, innerW, 224 });
    y += 232;

    // Row 5: Wave Header (Play, Track Dropdown, Del Layer, RR, LOOP, Z-Cross, 1-Shot, Choke, REV, NORM, Reset)
    auto wavHdr = juce::Rectangle<int> (4, y, innerW, 28);
    applySavedOrDefault (wavePlayBtn,     audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "wPlay",  wavHdr.removeFromLeft (42).reduced (1));
    applySavedOrDefault (trackSelectBox,  audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "trk",    wavHdr.removeFromLeft (114).reduced (1));
    applySavedOrDefault (delLayerBtn,     audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "delLyr", wavHdr.removeFromLeft (36).reduced (1));
    applySavedOrDefault (realRRBtn,       audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "rr",     wavHdr.removeFromLeft (34).reduced (1));
    applySavedOrDefault (sampleLoopBtn,   audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "sLoop",  wavHdr.removeFromLeft (44).reduced (1));
    applySavedOrDefault (autoLoopZeroBtn, audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "zCross", wavHdr.removeFromLeft (62).reduced (1));
    applySavedOrDefault (oneShotBtn,      audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "1shot",  wavHdr.removeFromLeft (54).reduced (1));
    applySavedOrDefault (chokeGroupBox,   audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "choke",  wavHdr.removeFromLeft (72).reduced (1));
    applySavedOrDefault (reverseBtn,      audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "rev",    wavHdr.removeFromLeft (40).reduced (1));
    applySavedOrDefault (normalizeBtn,    audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "norm",   wavHdr.removeFromLeft (48).reduced (1));
    applySavedOrDefault (resetZoneBtn,    audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "rstZ",   wavHdr.reduced (1));
    y += 30;

    // Row 6: HD Wave Editor
    applySavedOrDefault (waveEditor,     audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "waveEd", { 4, y, innerW, 166 });
    y += 172;

    // Row 7: Loop Crossfade Fader
    applySavedOrDefault (crossfadeFader, audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "xfade",  { 4, y, innerW, 34 });
    y += 38;

    // Row 8: Zone Pitch / Pan / Volume
    auto ppvRow = juce::Rectangle<int> (4, y, innerW, 38);
    applySavedOrDefault (pitchBox, audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "pitch", ppvRow.removeFromLeft (130).reduced (3, 0));
    applySavedOrDefault (volBox,   audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "vol",   ppvRow.removeFromRight (130).reduced (3, 0));
    applySavedOrDefault (panFader, audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "pan",   ppvRow.reduced (4, 0));
    y += 44;

    // Row 9: Large Interactive ADSR Section (2x2 Enlarged Boxes on Left + Tall 94px Interactive Curve Map on Right)
    auto adsrBlock = juce::Rectangle<int> (4, y, innerW, 94);
    auto adsrBoxes = adsrBlock.removeFromLeft (240);
    auto adsrTop2  = adsrBoxes.removeFromTop (45);
    adsrBoxes.removeFromTop (4);
    auto adsrBot2  = adsrBoxes;
    applySavedOrDefault (attackBox,  audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "atk",   adsrTop2.removeFromLeft (118).reduced (2, 0));
    applySavedOrDefault (decayBox,   audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "dec",   adsrTop2.reduced (2, 0));
    applySavedOrDefault (sustainBox, audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "sus",   adsrBot2.removeFromLeft (118).reduced (2, 0));
    applySavedOrDefault (releaseBox, audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "rel",   adsrBot2.reduced (2, 0));
    applySavedOrDefault (adsrGraph,  audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "adsrG", adsrBlock.reduced (4, 1));
    y += 102;

    // Row 10: 7-Card Pro FX Rack + Master FX Bypass
    auto fxHdr = juce::Rectangle<int> (4, y, innerW, 28);
    applySavedOrDefault (fxBypassBtn, audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "fxByp",  fxHdr.removeFromRight (64).reduced (1));
    fxHdr.removeFromRight (4);
    applySavedOrDefault (fxCardRack,  audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "fxRack", fxHdr.reduced (1));
    y += 32;

    // Row 11: FabFilter-Style Pro FX Display (Real-Time FFT Spectrum + Shaded Cut/Boost EQ, Live GR Comp, etc.)
    applySavedOrDefault (fxDisplay, audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "fxDisp", { 4, y, innerW, 108 });
    y += 112;

    // Row 12 & 13: 5 FX Parameter Faders
    auto fxRow1 = juce::Rectangle<int> (4, y, innerW, 34);
    int fxW3 = fxRow1.getWidth() / 3;
    applySavedOrDefault (fxP1, audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "fxP1", fxRow1.removeFromLeft (fxW3).reduced (3, 0));
    applySavedOrDefault (fxP2, audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "fxP2", fxRow1.removeFromLeft (fxW3).reduced (3, 0));
    applySavedOrDefault (fxP3, audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "fxP3", fxRow1.reduced (3, 0));
    y += 38;

    auto fxRow2 = juce::Rectangle<int> (4, y, innerW, 34);
    int fxW2 = fxRow2.getWidth() / 2;
    applySavedOrDefault (fxP4, audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "fxP4", fxRow2.removeFromLeft (fxW2).reduced (3, 0));
    applySavedOrDefault (fxP5, audioProcessor.editCustomBounds, audioProcessor.editHiddenKeys, "fxP5", fxRow2.reduced (3, 0));
    y += 44;

    int maxBottom = y + 8;
    for (int i = 0; i < editContent.getNumChildComponents(); ++i)
        maxBottom = std::max (maxBottom, editContent.getChildComponent (i)->getBottom() + 12);
    editContent.setSize (contentW, maxBottom);
}

void MainPluginView::paint (juce::Graphics& g)
{
    auto pal = customLnf.getPalette();
    g.fillAll (pal.bg);
    drawSketchRect (g, getLocalBounds().toFloat().reduced (3.0f), 6.0f, 1.4f, pal.border);

    if (layoutEditMode && ! gearToolbarBounds.isEmpty())
    {
        auto tb = gearToolbarBounds.toFloat();
        g.setColour (pal.panel);
        g.fillRoundedRectangle (tb, 4.0f);
        drawSketchRect (g, tb.reduced (0.5f), 4.0f, 1.3f, pal.border);
    }
}

void MainPluginView::resized()
{
    auto area = getLocalBounds().reduced (10, 8); // Exact 760 x 620

    // Bottom 50px slot: Piano Keyboard + Pitch/Mod Wheels normally, OR opaque 2-row Gear Customize Toolbar
    auto bottomSlot = area.removeFromBottom (50);
    gearToolbarBounds = bottomSlot.expanded (0, 2);
    auto keyRow = bottomSlot;
    pitchModWheels.setBounds (keyRow.removeFromLeft (46).reduced (0, 1));
    keyRow.removeFromLeft (4);
    bottomPiano.setBounds (keyRow.reduced (0, 1));
    area.removeFromBottom (8);

    // Symmetrical Top Bar (34px):
    // Left 148px = [KINETIK v1.3.5]  |  Center = Symmetrical PresetBar  |  Right 148px = [CPU/RAM 76px] + [Edit 32px] + [Gear 32px]
    auto topBar = area.removeFromTop (34);
    leftScaleBtn.setBounds (topBar.removeFromLeft (148).reduced (1));
    topGearBtn.setBounds   (topBar.removeFromRight (32).reduced (1));
    topBar.removeFromRight (3);
    editModeBtn.setBounds  (topBar.removeFromRight (32).reduced (1));
    topBar.removeFromRight (4);
    cpuRamBadge.setBounds  (topBar.removeFromRight (77).reduced (1));
    topBar.removeFromLeft (12);
    topBar.removeFromRight (12);
    presetBar.setBounds (topBar.reduced (0, 1));

    auto workArea = area;

    if (activeTab == ActiveTab::Main)
    {
        auto mainBody = area;
        mainBody.removeFromTop (6);
        auto defIoBar = mainBody.removeFromTop (28);
        mainBody.removeFromTop (6);
        auto defWave = mainBody.removeFromTop (164);
        mainBody.removeFromTop (8);
        auto seqHdr = mainBody.removeFromTop (18);
        auto defSeqHdr = seqHdr.removeFromLeft (200);
        mainBody.removeFromTop (4);
        auto defSeq = mainBody.removeFromTop (236);
        mainBody.removeFromTop (8);
        auto bottomCtrl = mainBody.removeFromTop (50);

        // Exact 760px distribution across all 9 bottom controls (uniform height alignment!)
        auto defPlay = bottomCtrl.removeFromLeft (64).reduced (2, 4);
        auto defSync = bottomCtrl.removeFromLeft (64).reduced (2, 4);
        auto defBpm  = bottomCtrl.removeFromLeft (74).reduced (2, 2);
        auto defShuf = bottomCtrl.removeFromLeft (118).reduced (3, 2);
        auto defTrnd = bottomCtrl.removeFromLeft (118).reduced (3, 2);
        auto defVrnd = bottomCtrl.removeFromLeft (118).reduced (3, 2);
        auto defGrid = bottomCtrl.removeFromLeft (88).reduced (2, 2);
        auto defLoop = bottomCtrl.removeFromLeft (64).reduced (2, 4);
        auto defClr  = bottomCtrl.removeFromLeft (52).reduced (2, 4);

        applySavedOrDefault (topIoMasterBar,     audioProcessor.mainCustomBounds, audioProcessor.mainHiddenKeys, "ioBar",  defIoBar);
        applySavedOrDefault (mainWaveDisplay,    audioProcessor.mainCustomBounds, audioProcessor.mainHiddenKeys, "wave",   defWave);
        applySavedOrDefault (rhythmSeqHeaderLbl, audioProcessor.mainCustomBounds, audioProcessor.mainHiddenKeys, "seqHdr", defSeqHdr);
        applySavedOrDefault (seqComp,            audioProcessor.mainCustomBounds, audioProcessor.mainHiddenKeys, "seq",    defSeq);
        applySavedOrDefault (playPauseBtn,       audioProcessor.mainCustomBounds, audioProcessor.mainHiddenKeys, "play",   defPlay);
        applySavedOrDefault (hostSyncBtn,        audioProcessor.mainCustomBounds, audioProcessor.mainHiddenKeys, "sync",   defSync);
        applySavedOrDefault (bpmBox,             audioProcessor.mainCustomBounds, audioProcessor.mainHiddenKeys, "bpm",    defBpm);
        applySavedOrDefault (shuffleFader,       audioProcessor.mainCustomBounds, audioProcessor.mainHiddenKeys, "shuf",   defShuf);
        applySavedOrDefault (timeRandFader,      audioProcessor.mainCustomBounds, audioProcessor.mainHiddenKeys, "trnd",   defTrnd);
        applySavedOrDefault (velRandFader,       audioProcessor.mainCustomBounds, audioProcessor.mainHiddenKeys, "vrnd",   defVrnd);
        applySavedOrDefault (gridUnit,           audioProcessor.mainCustomBounds, audioProcessor.mainHiddenKeys, "grid",   defGrid);
        applySavedOrDefault (seqLoopBtn,         audioProcessor.mainCustomBounds, audioProcessor.mainHiddenKeys, "loop",   defLoop);
        applySavedOrDefault (seqClearBtn,        audioProcessor.mainCustomBounds, audioProcessor.mainHiddenKeys, "clr",    defClr);
    }
    else
    {
        auto editArea = area;
        editArea.removeFromTop (6);
        editViewport.setBounds (editArea);
        layoutEditContent (editArea.getWidth());
    }

    if (layoutEditMode)
    {
        // Symmetrical 2-row Gear Customize Toolbar spanning the exact 752px inner width (zero dead gap!)
        auto barArea = gearToolbarBounds.reduced (4, 2);
        auto gRow1 = barArea.removeFromTop (24);
        int wTheme = 128, wGrid = 96, wSty = 92, wCol = 136;
        gearThemeBox.setBounds       (gRow1.removeFromLeft (wTheme).reduced (2, 1));
        gearGridBox.setBounds        (gRow1.removeFromLeft (wGrid).reduced (2, 1));
        gearFaderStyleBox.setBounds  (gRow1.removeFromLeft (wSty).reduced (2, 1));
        gearFaderColBox.setBounds    (gRow1.removeFromLeft (wCol).reduced (2, 1));
        gearKnobStyleBox.setBounds   (gRow1.removeFromLeft (wSty).reduced (2, 1));
        gearKnobColBox.setBounds     (gRow1.reduced (2, 1));

        barArea.removeFromTop (2);
        auto gRow2 = barArea.removeFromTop (24);
        gearSwitchStyleBox.setBounds (gRow2.removeFromLeft (112).reduced (2, 1));
        gearSwitchColBox.setBounds   (gRow2.removeFromLeft (156).reduced (2, 1));
        gearDoneBtn.setBounds        (gRow2.removeFromRight (84).reduced (2, 1));
        gearResetBtn.setBounds       (gRow2.removeFromRight (84).reduced (2, 1));
        gearDeleteBtn.setBounds      (gRow2.removeFromRight (84).reduced (2, 1));

        customizeOverlay.setBounds (workArea);
        customizeOverlay.toFront (false);
        gearThemeBox.toFront (false);
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

// ==============================================================================
// Outer Scalable Editor (FL Studio VST3 Host Resize Safe)
// ==============================================================================
RythmeAudioProcessorEditor::RythmeAudioProcessorEditor (RythmeAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      mainView (p, [this] (float s) { applyUiScale (s); })
{
    isUpdatingScale = true;
    setWantsKeyboardFocus (true);

    currentScale = juce::jlimit (0.5f, 1.5f, audioProcessor.uiScale);
    int initW = static_cast<int> (std::round (baseWidth * currentScale));
    int initH = static_cast<int> (std::round (baseHeight * currentScale));

    addAndMakeVisible (mainView);
    mainView.setBounds (0, 0, baseWidth, baseHeight);
    mainView.setTransform (juce::AffineTransform::scale (currentScale));

    setSize (initW, initH);
    setResizable (true, false);
    setResizeLimits (static_cast<int> (std::round (baseWidth * 0.50f)),
                     static_cast<int> (std::round (baseHeight * 0.50f)),
                     static_cast<int> (std::round (baseWidth * 1.50f)),
                     static_cast<int> (std::round (baseHeight * 1.50f)));

    isUpdatingScale = false;
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
    isUpdatingScale = true;
    currentScale = juce::jlimit (0.5f, 1.5f, newScale);
    audioProcessor.uiScale = currentScale;
    audioProcessor.saveUiLayoutToDisk();

    int targetW = static_cast<int> (std::round (baseWidth * currentScale));
    int targetH = static_cast<int> (std::round (baseHeight * currentScale));

    mainView.setBounds (0, 0, baseWidth, baseHeight);
    mainView.setTransform (juce::AffineTransform::scale (currentScale));
    if (getWidth() != targetW || getHeight() != targetH)
        setSize (targetW, targetH);

    isUpdatingScale = false;
    repaint();
}

void RythmeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (RythmeAudioProcessor::getThemePalette (audioProcessor.uiThemeId).bg);
}

void RythmeAudioProcessorEditor::resized()
{
    if (! isUpdatingScale && getWidth() > 0 && getHeight() > 0)
    {
        float sx = getWidth() / static_cast<float> (baseWidth);
        float sy = getHeight() / static_cast<float> (baseHeight);
        currentScale = juce::jlimit (0.5f, 1.5f, std::min (sx, sy));
        audioProcessor.uiScale = currentScale;
    }
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
