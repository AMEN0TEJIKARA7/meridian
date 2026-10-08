#include "LookAndFeel.h"

MeridianLookAndFeel::MeridianLookAndFeel()
{
    using namespace theme;
    setColour (juce::ResizableWindow::backgroundColourId, chrome);
    setColour (juce::TextButton::buttonColourId,     juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonOnColourId,   raised);
    setColour (juce::TextButton::textColourOffId,    textSoft);
    setColour (juce::TextButton::textColourOnId,     text);
    setColour (juce::Slider::textBoxTextColourId,    text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.35f));
    setColour (juce::Slider::rotarySliderFillColourId, accent);
    setColour (juce::Label::textColourId,            text);
    setColour (juce::Label::textWhenEditingColourId, text);
    setColour (juce::Label::backgroundWhenEditingColourId, raised);
    setColour (juce::Label::outlineWhenEditingColourId, border2);
    setColour (juce::TextEditor::backgroundColourId, raised);
    setColour (juce::TextEditor::textColourId,       text);
    setColour (juce::TextEditor::highlightColourId,  accent.withAlpha (0.35f));
    setColour (juce::TextEditor::outlineColourId,    border2);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::CaretComponent::caretColourId,  accent);
    setColour (juce::PopupMenu::backgroundColourId,  juce::Colour (0xff0b0b0b));
    setColour (juce::PopupMenu::textColourId,        text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, raised);
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::PopupMenu::headerTextColourId,  dim);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff0b0b0b));
    setColour (juce::TooltipWindow::textColourId,    text);
    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff0b0b0b));
    setColour (juce::AlertWindow::textColourId,      text);
    setColour (juce::AlertWindow::outlineColourId,   border2);
}

juce::Typeface::Ptr MeridianLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return f.isItalic() ? theme::Faces::get().serifItalic : theme::Faces::get().serif;
    if (f.getTypefaceName() == juce::Font::getDefaultMonospacedFontName())
        return theme::Faces::get().mono;
    return LookAndFeel_V4::getTypefaceForFont (f);
}

void MeridianLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool bordered = b.getProperties().getWithDefault (kBorder, true);
    const bool filled   = b.getProperties().getWithDefault (kFilled, false);
    const bool segment  = b.getProperties().getWithDefault (kSegment, false);
    const float radius  = segment ? 0.0f : 6.0f;

    juce::Colour fill = filled ? theme::raised.darker (0.25f) : juce::Colours::transparentBlack;
    if (b.getToggleState() && b.getClickingTogglesState()) fill = theme::raised;
    if (over)  fill = fill.isTransparent() ? juce::Colour (0xff0e0e0e) : fill.brighter (0.06f);
    if (down)  fill = fill.brighter (0.08f);

    g.setColour (fill);
    g.fillRoundedRectangle (r, radius);

    if (bordered && ! segment)
    {
        g.setColour (over ? theme::border2.brighter (0.15f) : theme::border);
        g.drawRoundedRectangle (r, radius, 1.0f);
    }
}

juce::Font MeridianLookAndFeel::getTextButtonFont (juce::TextButton& b, int)
{
    return b.getProperties().getWithDefault (kMono, false) ? theme::mono (12.5f) : theme::serif (14.0f);
}

void MeridianLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool)
{
    const bool on = b.getToggleState() || b.getProperties().getWithDefault (kFilled, false);
    auto colour = on ? theme::text : (over ? theme::text : theme::textSoft);
    if (! b.isEnabled()) colour = theme::faint.withAlpha (0.6f);

    g.setColour (colour);
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 0), juce::Justification::centred, 1);
}

void MeridianLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                            float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float size  = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto  c     = bounds.getCentre();
    const float r     = size * 0.5f - 3.0f;
    const auto  fill  = s.isEnabled() ? s.findColour (juce::Slider::rotarySliderFillColourId) : theme::track;

    juce::Path bg;
    bg.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, endAngle, true);
    g.setColour (theme::track);
    g.strokePath (bg, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Bipolar parameters (gain, pan) fill from the centre; others from the start.
    const bool  bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const float angle   = startAngle + pos * (endAngle - startAngle);
    const float from    = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, r, r, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (fill);
        g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float capR = r * 0.65f;
    g.setColour (theme::knobCap);
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);

    const juce::Point<float> tip (c.x + capR * 0.9f * std::sin (angle), c.y - capR * 0.9f * std::cos (angle));
    g.setColour (s.isEnabled() ? theme::text : theme::faint);
    g.drawLine ({ c, tip }, 2.0f);
}

juce::Label* MeridianLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (theme::mono (13.0f));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, theme::text);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

juce::Font MeridianLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

void MeridianLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (juce::Colour (0xff0b0b0b));
    g.setColour (theme::border2);
    g.drawRect (0, 0, w, h, 1);
}

juce::Font MeridianLookAndFeel::getPopupMenuFont() { return theme::serif (15.0f); }

void MeridianLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name)
{
    g.setFont (theme::serifItalic (13.5f));
    g.setColour (theme::dim);
    g.drawFittedText (name, area.withTrimmedLeft (12).withTrimmedRight (8), juce::Justification::bottomLeft, 1);
}

void MeridianLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                             bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                                             const juce::String& shortcutKeyText, const juce::Drawable* icon, const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour (theme::border);
        g.fillRect (area.reduced (8, 0).withSizeKeepingCentre (area.getWidth() - 16, 1));
        return;
    }

    auto r = area.reduced (2, 0);
    if (isHighlighted && isActive)
    {
        g.setColour (theme::raised);
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
    }

    auto content = r.reduced (10, 0);
    auto tickArea = content.removeFromLeft (16);
    if (isTicked)
    {
        g.setColour (theme::accent);
        g.fillEllipse (tickArea.toFloat().withSizeKeepingCentre (6.0f, 6.0f));
    }

    if (icon != nullptr)
    {
        auto iconArea = content.removeFromLeft (32).toFloat().reduced (2.0f, 4.0f);
        icon->drawWithin (g, iconArea, juce::RectanglePlacement::centred, isActive ? 1.0f : 0.4f);
        content.removeFromLeft (6);
    }

    g.setColour (isActive ? theme::text : theme::faint.withAlpha (0.7f));
    g.setFont (getPopupMenuFont());
    g.drawFittedText (text, content, juce::Justification::centredLeft, 1);

    if (shortcutKeyText.isNotEmpty())
    {
        g.setFont (theme::mono (12.0f));
        g.setColour (theme::dim);
        g.drawText (shortcutKeyText, content, juce::Justification::centredRight);
    }

    if (hasSubMenu)
    {
        juce::Path arrow;
        auto a = content.removeFromRight (10).toFloat().withSizeKeepingCentre (5.0f, 9.0f);
        arrow.startNewSubPath (a.getX(), a.getY());
        arrow.lineTo (a.getRight(), a.getCentreY());
        arrow.lineTo (a.getX(), a.getBottom());
        g.setColour (theme::dim);
        g.strokePath (arrow, juce::PathStrokeType (1.4f));
    }
}

void MeridianLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.fillAll (juce::Colour (0xff0b0b0b));
    g.setColour (theme::border2);
    g.drawRect (0, 0, w, h, 1);
    g.setColour (theme::text);
    g.setFont (theme::serif (13.5f));
    g.drawFittedText (text, juce::Rectangle<int> (w, h).reduced (8, 4), juce::Justification::centred, 3);
}

juce::Rectangle<int> MeridianLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    const int w = juce::jmin (320, juce::GlyphArrangement::getStringWidthInt (theme::serif (13.5f), tipText) + 20);
    const int h = 26;
    return juce::Rectangle<int> (screenPos.x - w / 2, screenPos.y + 18, w, h).constrainedWithin (parentArea);
}

void MeridianLookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool over, bool dragging)
{
    g.setColour ((over || dragging) ? theme::dim : theme::border2);
    for (float i = 0.3f; i < 1.0f; i += 0.3f)
        g.drawLine ((float) w * i, (float) h - 1.0f, (float) w - 1.0f, (float) h * i, 1.0f);
}

juce::Path shapeIcon (int shape, juce::Rectangle<float> area)
{
    static const char* svg[] {
        "M1 12c7 0 8-9 13-9s6 9 13 9",          // Bell
        "M1 4h7c5 0 7 8 12 8h7",                // Low Shelf
        "M3 15C6 6 8 4 13 4h14",                // Low Cut
        "M1 12h7c5 0 7-8 12-8h7",               // High Shelf
        "M1 4h14c5 0 7 2 10 11",                // High Cut
        "M1 4h10c2 0 2 11 3 11s1-11 3-11h10",   // Notch
        "M2 15C7 4 10 3 14 3s7 1 12 12",        // Band Pass
        "M1 12h6c4 0 8-8 14-8h6",               // Tilt Shelf
        "M1 13L27 3"                            // Flat Tilt
    };
    auto p = juce::Drawable::parseSVGPath (svg[juce::jlimit (0, 8, shape)]);
    // Fixed 28x16 viewbox so flat shapes keep their proportions.
    const float scale = juce::jmin (area.getWidth() / 28.0f, area.getHeight() / 16.0f);
    p.applyTransform (juce::AffineTransform::scale (scale)
                          .translated (area.getCentreX() - 14.0f * scale, area.getCentreY() - 8.0f * scale));
    return p;
}
