#include "BandPanel.h"

using namespace meridian;

// ---------------------------------------------------------------------------------------------
void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    if (border)
    {
        g.setColour (down ? theme::raised : (over ? juce::Colour (0xff0e0e0e) : juce::Colours::transparentBlack));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (over ? theme::border2.brighter (0.15f) : theme::border2);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }

    auto c = r.getCentre();
    const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f * (border ? 0.85f : 1.0f);
    auto box = juce::Rectangle<float> (s * 2.0f * 0.5f, s * 2.0f * 0.5f).withCentre (c);
    juce::Path p;

    switch (icon)
    {
        case Icon::Power:
            p.addCentredArc (c.x, c.y + 0.5f, box.getWidth() * 0.5f, box.getWidth() * 0.5f, 0.0f,
                             juce::degreesToRadians (40.0f), juce::degreesToRadians (320.0f), true);
            p.startNewSubPath (c.x, box.getY() - 1.5f);
            p.lineTo (c.x, c.y);
            break;
        case Icon::Solo:
        {
            const float w = box.getWidth() * 0.62f, top = box.getY(), bot = box.getBottom() + 1.0f;
            p.startNewSubPath (c.x - w, c.y + 2.0f);
            p.lineTo (c.x - w, c.y);
            p.quadraticTo (c.x - w, top - 1.0f, c.x, top - 1.0f);
            p.quadraticTo (c.x + w, top - 1.0f, c.x + w, c.y);
            p.lineTo (c.x + w, c.y + 2.0f);
            p.addRoundedRectangle (c.x - w - 1.5f, c.y + 1.0f, 3.5f, bot - c.y - 1.0f, 1.0f);
            p.addRoundedRectangle (c.x + w - 2.0f, c.y + 1.0f, 3.5f, bot - c.y - 1.0f, 1.0f);
            break;
        }
        case Icon::Close:
            box = box.reduced (box.getWidth() * 0.12f);
            p.startNewSubPath (box.getTopLeft());     p.lineTo (box.getBottomRight());
            p.startNewSubPath (box.getTopRight());    p.lineTo (box.getBottomLeft());
            break;
        case Icon::Undo:
        case Icon::Redo:
        {
            const float dir = icon == Icon::Undo ? 1.0f : -1.0f;
            auto b = box.expanded (1.0f);
            p.startNewSubPath (c.x - dir * b.getWidth() * 0.5f + dir * 3.0f, b.getY() + 1.0f);
            p.lineTo (c.x - dir * b.getWidth() * 0.5f, b.getY() + 4.0f);
            p.lineTo (c.x - dir * b.getWidth() * 0.5f + dir * 3.0f, b.getY() + 7.0f);
            p.startNewSubPath (c.x - dir * b.getWidth() * 0.5f, b.getY() + 4.0f);
            p.lineTo (c.x + dir * b.getWidth() * 0.1f, b.getY() + 4.0f);
            p.quadraticTo (c.x + dir * b.getWidth() * 0.5f, b.getY() + 4.0f, c.x + dir * b.getWidth() * 0.5f, c.y + 1.5f);
            p.quadraticTo (c.x + dir * b.getWidth() * 0.5f, b.getBottom(), c.x, b.getBottom());
            break;
        }
        case Icon::Prev:
        case Icon::Next:
        {
            const float dir = icon == Icon::Prev ? -1.0f : 1.0f;
            const float h = box.getHeight() * 0.42f, w = h * 0.55f;
            p.startNewSubPath (c.x - dir * w, c.y - h);
            p.lineTo (c.x + dir * w, c.y);
            p.lineTo (c.x - dir * w, c.y + h);
            break;
        }
        case Icon::Gear:
        {
            const float R = box.getWidth() * 0.62f, r0 = box.getWidth() * 0.26f;
            p.addEllipse (c.x - r0, c.y - r0, r0 * 2.0f, r0 * 2.0f);
            for (int i = 0; i < 8; ++i)
            {
                const float a = i * juce::MathConstants<float>::pi / 4.0f;
                p.startNewSubPath (c.x + std::cos (a) * R * 0.66f, c.y + std::sin (a) * R * 0.66f);
                p.lineTo (c.x + std::cos (a) * R, c.y + std::sin (a) * R);
            }
            break;
        }
    }

    juce::Colour col = active ? activeColour : iconColour;
    if (over) col = col.brighter (0.25f);
    if (! isEnabled()) col = col.withAlpha (0.4f);
    g.setColour (col);
    g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void ShapeButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    getLookAndFeel().drawButtonBackground (g, *this, {}, over, down);

    const auto font = theme::serif (14.0f);
    const auto label = getButtonText();
    const float textW = juce::GlyphArrangement::getStringWidth (font, label);
    const float iconW = 22.0f, gap = 8.0f;
    const float total = iconW + gap + textW;
    auto area = getLocalBounds().toFloat();
    const float x0 = area.getCentreX() - total * 0.5f;

    g.setColour (theme::text);
    g.strokePath (shapeIcon (shape, { x0, area.getCentreY() - 6.5f, iconW, 13.0f }),
                  juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setFont (font);
    g.drawText (label, juce::Rectangle<float> (x0 + iconW + gap, area.getY(), textW + 4.0f, area.getHeight()),
                juce::Justification::centredLeft, false);
}

// ---------------------------------------------------------------------------------------------
BandPanel::BandPanel (MeridianAudioProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (false);
    setInterceptsMouseClicks (true, true);

    shapeButton.getProperties().set (MeridianLookAndFeel::kFilled, true);
    shapeButton.onClick = [this] { showShapeMenu(); };
    addAndMakeVisible (shapeButton);

    slopeButton.getProperties().set (MeridianLookAndFeel::kMono, true);
    slopeButton.onClick = [this] { showSlopeMenu(); };
    addAndMakeVisible (slopeButton);

    placementButton.onClick = [this] { showPlacementMenu(); };
    addAndMakeVisible (placementButton);

    for (auto* k : { &freqKnob, &gainKnob, &qKnob })
    {
        k->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 92, 18);
        k->setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        k->setMouseDragSensitivity (220);
        k->onDragStart = [this] { proc.undoManager.beginNewTransaction(); };
        addAndMakeVisible (*k);
    }
    gainKnob.getProperties().set ("bipolar", true);

    bypassButton.onClick = [this]
    {
        proc.undoManager.beginNewTransaction();
        proc.setParam (params::bandId (band, "on"), param ("on") > 0.5f ? 0.0f : 1.0f);
        refresh();
    };
    soloButton.onClick = [this]
    {
        proc.soloBand = proc.soloBand.load() == band ? -1 : band;
        refresh();
    };
    deleteButton.onClick = [this]
    {
        proc.undoManager.beginNewTransaction();
        const int b = band;
        if (onDeleteBand) onDeleteBand (b);
    };
    for (auto* b : { &bypassButton, &soloButton, &deleteButton })
        addAndMakeVisible (*b);

    setSize (kWidth, kHeight);
}

BandPanel::~BandPanel()
{
    freqAtt.reset(); gainAtt.reset(); qAtt.reset();
}

float BandPanel::param (const char* field) const
{
    return proc.apvts.getRawParameterValue (params::bandId (band, field))->load();
}

void BandPanel::setBand (int b)
{
    if (b == band)
        return;
    band = b;
    freqAtt.reset(); gainAtt.reset(); qAtt.reset();
    if (band < 0)
        return;

    freqAtt = std::make_unique<Att> (proc.apvts, params::bandId (band, "freq"), freqKnob);
    gainAtt = std::make_unique<Att> (proc.apvts, params::bandId (band, "gain"), gainKnob);
    qAtt    = std::make_unique<Att> (proc.apvts, params::bandId (band, "q"),    qKnob);

    for (auto* k : { &freqKnob, &gainKnob, &qKnob })
        k->setColour (juce::Slider::rotarySliderFillColourId, theme::bandColour (band));

    refresh();
}

void BandPanel::refresh()
{
    if (band < 0)
        return;

    const int shape = juce::roundToInt (param ("shape"));
    shapeButton.shape = shape;
    shapeButton.setButtonText (shapeNames()[shape]);
    shapeButton.repaint();

    slopeButton.setButtonText (slopeNames()[juce::roundToInt (param ("slope"))]);
    const bool wantSlope = shapeHasSlope ((Shape) shape);
    if (slopeButton.isVisible() != wantSlope)
    {
        slopeButton.setVisible (wantSlope);
        resized();
    }
    placementButton.setButtonText (placementNames()[juce::roundToInt (param ("place"))]);

    gainKnob.setEnabled (shapeHasGain ((Shape) shape));

    const bool enabled = param ("on") > 0.5f;
    bypassButton.active = enabled;
    bypassButton.activeColour = theme::bandColour (band);
    bypassButton.setTooltip (enabled ? "Bypass band" : "Enable band");
    bypassButton.repaint();

    soloButton.active = proc.soloBand.load() == band;
    soloButton.activeColour = theme::accent;
    soloButton.repaint();

    repaint (getLocalBounds().removeFromRight (128));
}

void BandPanel::showShapeMenu()
{
    juce::PopupMenu m;
    const int current = juce::roundToInt (param ("shape"));
    for (int i = 0; i < kNumShapes; ++i)
    {
        auto icon = std::make_unique<juce::DrawablePath>();
        icon->setPath (shapeIcon (i, { 0, 0, 28, 16 }));
        icon->setFill (juce::Colours::transparentBlack);
        icon->setStrokeFill (theme::text);
        icon->setStrokeType (juce::PathStrokeType (1.4f));

        juce::PopupMenu::Item item (shapeNames()[i]);
        item.itemID = i + 1;
        item.isTicked = i == current;
        item.image = std::move (icon);
        m.addItem (std::move (item));
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&shapeButton).withMinimumWidth (180),
                     [this, b = band] (int r)
                     {
                         if (r > 0 && b == band)
                         {
                             proc.undoManager.beginNewTransaction();
                             proc.setParam (params::bandId (band, "shape"), (float) (r - 1));
                             refresh();
                         }
                     });
}

void BandPanel::showSlopeMenu()
{
    juce::PopupMenu m;
    const int current = juce::roundToInt (param ("slope"));
    for (int i = 0; i < kNumSlopes; ++i)
        m.addItem (i + 1, slopeNames()[i], true, i == current);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slopeButton).withMinimumWidth (150),
                     [this, b = band] (int r)
                     {
                         if (r > 0 && b == band)
                         {
                             proc.undoManager.beginNewTransaction();
                             proc.setParam (params::bandId (band, "slope"), (float) (r - 1));
                             refresh();
                         }
                     });
}

void BandPanel::showPlacementMenu()
{
    juce::PopupMenu m;
    const int current = juce::roundToInt (param ("place"));
    for (int i = 0; i < kNumPlacements; ++i)
        m.addItem (i + 1, placementNames()[i], true, i == current);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&placementButton).withMinimumWidth (140),
                     [this, b = band] (int r)
                     {
                         if (r > 0 && b == band)
                         {
                             proc.undoManager.beginNewTransaction();
                             proc.setParam (params::bandId (band, "place"), (float) (r - 1));
                             refresh();
                         }
                     });
}

void BandPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (theme::border2);
    g.drawRoundedRectangle (r, 10.0f, 1.0f);

    // Knob captions, centred over each knob.
    g.setFont (theme::serifItalic (14.0f));
    g.setColour (theme::textSoft);
    const char* names[] { "Frequency", "Gain", "Q" };
    juce::Slider* knobs[] { &freqKnob, &gainKnob, &qKnob };
    for (int i = 0; i < 3; ++i)
    {
        auto kb = knobs[i]->getBounds();
        g.setColour (knobs[i]->isEnabled() ? theme::textSoft : theme::faint.withAlpha (0.6f));
        g.drawText (names[i], kb.getX(), kb.getY() - 19, kb.getWidth(), 17, juce::Justification::centred);
    }

    // Band label under the icon row.
    if (band >= 0)
    {
        int used = 0;
        for (int b = 0; b < kMaxBands; ++b)
            if (proc.apvts.getRawParameterValue (params::bandId (b, "used"))->load() > 0.5f)
                ++used;
        auto row = deleteButton.getBounds();
        g.setFont (theme::mono (11.0f));
        g.setColour (theme::faint);
        g.drawText ("BAND " + juce::String (band + 1) + " / " + juce::String (used),
                    bypassButton.getX(), row.getBottom() + 10, row.getRight() - bypassButton.getX(), 16,
                    juce::Justification::centred);
    }
}

void BandPanel::resized()
{
    auto r = getLocalBounds().reduced (16, 14);

    // Left column: shape / slope / placement, stacked and centred vertically
    // (the slope row disappears for shapes that have no slope).
    auto left = r.removeFromLeft (132);
    const bool slope = slopeButton.isVisible();
    const int stackH = 30 + 8 + 26 + (slope ? 8 + 26 : 0);
    left = left.withSizeKeepingCentre (left.getWidth(), stackH);
    shapeButton.setBounds (left.removeFromTop (30));
    left.removeFromTop (8);
    if (slope)
    {
        slopeButton.setBounds (left.removeFromTop (26));
        left.removeFromTop (8);
    }
    placementButton.setBounds (left.removeFromTop (26));

    // Right column: icon row + band label (painted), centred vertically.
    auto right = r.removeFromRight (108);
    right = right.withSizeKeepingCentre (right.getWidth(), 30 + 10 + 16);
    auto icons = right.removeFromTop (30);
    const int iw = (icons.getWidth() - 12) / 3;
    bypassButton.setBounds (icons.removeFromLeft (iw));  icons.removeFromLeft (6);
    soloButton.setBounds (icons.removeFromLeft (iw));    icons.removeFromLeft (6);
    deleteButton.setBounds (icons);

    // Knobs: centred in the remaining space, captions painted above them.
    auto mid = r.reduced (8, 0);
    const int kw = 92, gap = 10;
    const int total = kw * 3 + gap * 2;
    int x = mid.getX() + (mid.getWidth() - total) / 2;
    for (auto* k : { &freqKnob, &gainKnob, &qKnob })
    {
        k->setBounds (x, mid.getY() + 20, kw, mid.getHeight() - 20);
        x += kw + gap;
    }
}
