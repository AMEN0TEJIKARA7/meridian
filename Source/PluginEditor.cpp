#include "PluginEditor.h"

using namespace meridian;

// ---------------------------------------------------------------------------------------------
void CaptionButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    getLookAndFeel().drawButtonBackground (g, *this, {}, over, down);

    const auto capFont = theme::serif (14.0f), valFont = theme::mono (12.5f);
    const auto caption = getButtonText();
    const float cw = juce::GlyphArrangement::getStringWidth (capFont, caption);
    const float vw = value.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth (valFont, value);
    const float gap = value.isEmpty() ? 0.0f : 7.0f;
    auto r = getLocalBounds().toFloat();
    float x = r.getCentreX() - (cw + gap + vw) * 0.5f;

    g.setFont (capFont);
    g.setColour (over ? theme::text : theme::textSoft);
    g.drawText (caption, juce::Rectangle<float> (x, r.getY(), cw + 2.0f, r.getHeight()), juce::Justification::centredLeft, false);
    if (value.isNotEmpty())
    {
        g.setFont (valFont);
        g.setColour (theme::text);
        g.drawText (value, juce::Rectangle<float> (x + cw + gap, r.getY() + 0.5f, vw + 2.0f, r.getHeight()), juce::Justification::centredLeft, false);
    }
}

void DotToggle::paintButton (juce::Graphics& g, bool over, bool down)
{
    getLookAndFeel().drawButtonBackground (g, *this, {}, over, down);

    const bool on = getToggleState();
    const auto font = theme::serif (14.0f);
    const float tw = juce::GlyphArrangement::getStringWidth (font, getButtonText());
    auto r = getLocalBounds().toFloat();
    const float x = r.getCentreX() - (7.0f + 8.0f + tw) * 0.5f;

    auto dot = juce::Rectangle<float> (x, r.getCentreY() - 3.5f, 7.0f, 7.0f);
    if (on) { g.setColour (theme::accent); g.fillEllipse (dot); }
    else    { g.setColour (theme::faint);  g.drawEllipse (dot.reduced (0.5f), 1.0f); }

    g.setFont (font);
    g.setColour (on || over ? theme::text : theme::textSoft);
    g.drawText (getButtonText(), juce::Rectangle<float> (x + 15.0f, r.getY(), tw + 2.0f, r.getHeight()), juce::Justification::centredLeft, false);
}

void DragNumber::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (isMouseOverOrDragging())
    {
        g.setColour (juce::Colour (0xff0e0e0e));
        g.fillRoundedRectangle (r, 5.0f);
    }
    g.setFont (theme::serifItalic (13.0f));
    g.setColour (theme::dim);
    g.drawText (caption, r.removeFromTop (r.getHeight() * 0.5f).withTrimmedBottom (1.0f), juce::Justification::centredBottom);
    g.setFont (theme::mono (13.0f));
    g.setColour (theme::text);
    g.drawText (getTextFromValue (getValue()), r.withTrimmedTop (2.0f), juce::Justification::centredTop);
}

void LevelMeter::setLevels (float l, float r)
{
    if (std::abs (l - levelL) > 0.001f || std::abs (r - levelR) > 0.001f)
    {
        levelL = l; levelR = r;
        repaint();
    }
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float barW = 5.0f, gap = 3.0f;
    float x = r.getCentreX() - barW - gap * 0.5f;
    for (float lvl : { levelL, levelR })
    {
        auto bar = juce::Rectangle<float> (x, r.getY(), barW, r.getHeight());
        g.setColour (theme::raised);
        g.fillRoundedRectangle (bar, 2.0f);

        // -60..+6 dBFS
        const float db = juce::Decibels::gainToDecibels (lvl, -60.0f);
        const float t = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
        auto fill = bar.withTrimmedTop (bar.getHeight() * (1.0f - t));
        g.setColour (db > 0.0f ? juce::Colour (0xffd98b6c) : theme::text.withAlpha (0.8f));
        g.fillRoundedRectangle (fill, 2.0f);
        x += barW + gap;
    }
}

// ---------------------------------------------------------------------------------------------
MeridianAudioProcessorEditor::MeridianAudioProcessorEditor (MeridianAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);   // popup menus & dialogs
    setWantsKeyboardFocus (true);

    // ---- Header ----
    prevPreset.onClick = [this] { proc.stepPreset (-1); refreshHeader(); };
    nextPreset.onClick = [this] { proc.stepPreset (+1); refreshHeader(); };
    presetButton.getProperties().set (MeridianLookAndFeel::kFilled, true);
    presetButton.onClick = [this] { showPresetMenu(); };
    undoButton.onClick = [this] { proc.undoManager.undo(); };
    redoButton.onClick = [this] { proc.undoManager.redo(); };
    gearButton.onClick = [this] { showSettingsMenu(); };

    for (auto* b : { &aButton, &bButton, &copyButton })
    {
        b->getProperties().set (MeridianLookAndFeel::kMono, true);
        b->getProperties().set (MeridianLookAndFeel::kSegment, true);
    }
    aButton.setTooltip ("Setting A");
    bButton.setTooltip ("Setting B");
    aButton.onClick = [this] { proc.switchToSlot (0); refreshHeader(); };
    bButton.onClick = [this] { proc.switchToSlot (1); refreshHeader(); };
    copyButton.onClick = [this] { proc.copyCurrentToOther(); };

    for (juce::Component* c : std::initializer_list<juce::Component*> { &prevPreset, &presetButton, &nextPreset, &undoButton, &redoButton,
                                                                         &aButton, &bButton, &copyButton, &gearButton })
        addAndMakeVisible (c);

    addAndMakeVisible (display);

    // ---- Footer ----
    modeButton.setButtonText ("Zero Latency");
    modeButton.getProperties().set (MeridianLookAndFeel::kFilled, true);
    modeButton.onClick = [this] { showModeMenu(); };
    analyzerButton.setButtonText ("Analyzer");
    analyzerButton.onClick = [this] { showAnalyzerMenu(); };
    rangeButton.setButtonText ("Range");
    rangeButton.onClick = [this] { showRangeMenu(); };

    autoGainButton.setButtonText ("Auto Gain");
    autoGainButton.setClickingTogglesState (true);
    autoGainButton.setTooltip ("Compensates the overall level change of the EQ curve");

    for (juce::Component* c : std::initializer_list<juce::Component*> { &modeButton, &analyzerButton, &rangeButton, &autoGainButton,
                                                                         &gainScale, &outGain, &outPan, &meter })
        addAndMakeVisible (c);

    gainScale.setTooltip ("Scales the gain of every band (drag up/down, double-click to reset)");
    outGain.setTooltip ("Output gain");
    outPan.setTooltip ("Output balance");

    gainScaleAtt = std::make_unique<SliderAtt> (proc.apvts, params::gainScale, gainScale);
    outGainAtt   = std::make_unique<SliderAtt> (proc.apvts, params::outGain,   outGain);
    outPanAtt    = std::make_unique<SliderAtt> (proc.apvts, params::outPan,    outPan);
    autoGainAtt  = std::make_unique<ButtonAtt> (proc.apvts, params::autoGain,  autoGainButton);
    gainScale.setDoubleClickReturnValue (true, 100.0);
    outGain.setDoubleClickReturnValue (true, 0.0);
    outPan.setDoubleClickReturnValue (true, 0.0);
    for (auto* s : { &gainScale, &outGain, &outPan })
        s->onDragStart = [this] { proc.undoManager.beginNewTransaction(); };

    juce::Component::SafePointer<MeridianAudioProcessorEditor> safe (this);
    proc.onPresetChanged = [safe]
    {
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->refreshHeader(); });
    };

    setResizable (true, true);
    setResizeLimits (960, 540, 2400, 1400);
    setSize (1200, kHeaderH + 528 + kFooterH);
    refreshHeader();
    startTimerHz (30);
}

MeridianAudioProcessorEditor::~MeridianAudioProcessorEditor()
{
    proc.onPresetChanged = nullptr;
    stopTimer();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void MeridianAudioProcessorEditor::refreshHeader()
{
    presetButton.setButtonText (proc.getPresetName());
    const int slot = proc.getActiveSlot();
    aButton.getProperties().set (MeridianLookAndFeel::kFilled, slot == 0);
    bButton.getProperties().set (MeridianLookAndFeel::kFilled, slot == 1);
    copyButton.setButtonText (slot == 0 ? juce::String (juce::CharPointer_UTF8 ("A\xe2\x86\x92" "B"))
                                        : juce::String (juce::CharPointer_UTF8 ("B\xe2\x86\x92" "A")));
    copyButton.setTooltip (slot == 0 ? "Copy A to B" : "Copy B to A");

    static const char* modes[] { "OFF", "PRE", "POST", "PRE+POST" };
    analyzerButton.value = modes[juce::jlimit (0, 3, display.analyzerMode())];
    const float range = display.rangeDb();
    rangeButton.value = juce::String (juce::CharPointer_UTF8 ("\xc2\xb1")) + juce::String ((int) range);

    for (juce::Component* c : std::initializer_list<juce::Component*> { &aButton, &bButton, &copyButton, &analyzerButton, &rangeButton, &presetButton })
        c->repaint();
    repaint (0, 0, getWidth(), kHeaderH);
}

void MeridianAudioProcessorEditor::timerCallback()
{
    const float decay = 0.80f;
    meterL = juce::jmax (proc.peakL.exchange (0.0f), meterL * decay);
    meterR = juce::jmax (proc.peakR.exchange (0.0f), meterR * decay);
    meter.setLevels (meterL, meterR);

    undoButton.setEnabled (proc.undoManager.canUndo());
    redoButton.setEnabled (proc.undoManager.canRedo());

    // Band count in the header.
    int used = 0;
    for (int b = 0; b < kMaxBands; ++b)
        if (proc.apvts.getRawParameterValue (params::bandId (b, "used"))->load() > 0.5f)
            ++used;
    if (used != lastCount)
    {
        lastCount = used;
        repaint (0, 0, getWidth(), kHeaderH);
    }
}

void MeridianAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::chrome);

    // Header
    auto header = getLocalBounds().removeFromTop (kHeaderH);
    g.setColour (theme::border.darker (0.2f));
    g.drawHorizontalLine (kHeaderH - 1, 0.0f, (float) getWidth());

    g.setFont (theme::wordmark (32.0f));
    g.setColour (theme::text);
    const float wmW = juce::GlyphArrangement::getStringWidth (theme::wordmark (32.0f), "Meridian");
    g.drawText ("Meridian", juce::Rectangle<float> (18.0f, 0.0f, wmW + 4.0f, (float) kHeaderH).translated (0.0f, 1.0f),
                juce::Justification::centredLeft, false);
    g.setFont (theme::mono (11.5f));
    g.setColour (theme::dim);
    g.drawText ("EQ", juce::Rectangle<float> (18.0f + wmW + 8.0f, 0.0f, 30.0f, (float) kHeaderH).translated (0.0f, 4.0f),
                juce::Justification::centredLeft, false);

    // Divider after the wordmark
    g.setColour (theme::border);
    g.fillRect (juce::Rectangle<float> (prevPreset.getX() - 18.0f, header.getCentreY() - 11.0f, 1.0f, 22.0f));

    // A/B group outline
    auto ab = aButton.getBounds().getUnion (copyButton.getBounds()).toFloat().reduced (0.5f);
    g.setColour (theme::border);
    g.drawRoundedRectangle (ab, 6.0f, 1.0f);
    g.drawVerticalLine (copyButton.getX(), ab.getY(), ab.getBottom());

    // Band count
    int used = 0;
    for (int b = 0; b < kMaxBands; ++b)
        if (proc.apvts.getRawParameterValue (params::bandId (b, "used"))->load() > 0.5f)
            ++used;
    g.setFont (theme::serifItalic (14.0f));
    g.setColour (theme::dim);
    g.drawText (juce::String (used) + (used == 1 ? " band" : " bands"),
                juce::Rectangle<int> (gearButton.getX() - 110, 0, 100, kHeaderH), juce::Justification::centredRight);

    // Footer line
    g.setColour (theme::border.darker (0.2f));
    g.drawHorizontalLine (getHeight() - kFooterH, 0.0f, (float) getWidth());
}

void MeridianAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();

    // ---- Header ----
    auto header = r.removeFromTop (kHeaderH).reduced (18, 0);
    const float wmW = juce::GlyphArrangement::getStringWidth (theme::wordmark (32.0f), "Meridian");
    header.removeFromLeft ((int) wmW + 8 + 26 + 36);   // wordmark + "EQ" + divider spacing

    auto centreRow = [] (juce::Rectangle<int> area, int h) { return area.withSizeKeepingCentre (area.getWidth(), h); };

    prevPreset.setBounds (centreRow (header.removeFromLeft (28), 28));   header.removeFromLeft (6);
    presetButton.setBounds (centreRow (header.removeFromLeft (220), 28)); header.removeFromLeft (6);
    nextPreset.setBounds (centreRow (header.removeFromLeft (28), 28));   header.removeFromLeft (16);
    undoButton.setBounds (centreRow (header.removeFromLeft (28), 28));   header.removeFromLeft (2);
    redoButton.setBounds (centreRow (header.removeFromLeft (28), 28));   header.removeFromLeft (16);
    aButton.setBounds (centreRow (header.removeFromLeft (34), 26));
    bButton.setBounds (centreRow (header.removeFromLeft (34), 26));
    copyButton.setBounds (centreRow (header.removeFromLeft (46), 26));

    gearButton.setBounds (centreRow (header.removeFromRight (30), 30));

    // ---- Footer ----
    auto footer = r.removeFromBottom (kFooterH).reduced (18, 0);
    auto fb = [&] (juce::Component& c, int w) { c.setBounds (centreRow (footer.removeFromLeft (w), 32)); footer.removeFromLeft (10); };
    fb (modeButton, 128);
    fb (analyzerButton, 156);
    fb (rangeButton, 104);

    meter.setBounds (footer.removeFromRight (16).withSizeKeepingCentre (16, 38));
    footer.removeFromRight (14);
    outPan.setBounds (footer.removeFromRight (64).withSizeKeepingCentre (64, 44));
    footer.removeFromRight (6);
    outGain.setBounds (footer.removeFromRight (84).withSizeKeepingCentre (84, 44));
    footer.removeFromRight (6);
    gainScale.setBounds (footer.removeFromRight (84).withSizeKeepingCentre (84, 44));
    footer.removeFromRight (12);
    autoGainButton.setBounds (footer.removeFromRight (118).withSizeKeepingCentre (118, 32));

    display.setBounds (r);
}

bool MeridianAudioProcessorEditor::keyPressed (const juce::KeyPress& k)
{
    const bool cmd = k.getModifiers().isCommandDown();
    if (cmd && k.getKeyCode() == 'Z')
    {
        k.getModifiers().isShiftDown() ? proc.undoManager.redo() : proc.undoManager.undo();
        return true;
    }
    if (cmd && k.getKeyCode() == 'Y')
    {
        proc.undoManager.redo();
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------------------------
void MeridianAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu m;
    auto files = proc.presetDirectory().findChildFiles (juce::File::findFiles, false, "*.mrdn");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareNatural (b.getFileName()) < 0; });

    m.addSectionHeader ("Presets");
    if (files.isEmpty())
        m.addItem (-1, "No saved presets yet", false);
    for (int i = 0; i < files.size(); ++i)
        m.addItem (100 + i, files[i].getFileNameWithoutExtension(), true,
                   files[i].getFileNameWithoutExtension() == proc.getPresetName());
    m.addSeparator();
    m.addItem (1, "Save Preset As...");
    m.addItem (2, "Load From File...");
    m.addItem (3, "Reset to Default");
    m.addItem (4, "Open Preset Folder");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton).withMinimumWidth (presetButton.getWidth()),
                     [this, files] (int r)
                     {
                         if (r >= 100) { proc.loadPreset (files[r - 100]); refreshHeader(); }
                         else if (r == 1)
                         {
                             chooser = std::make_unique<juce::FileChooser> ("Save preset", proc.presetDirectory().getChildFile ("New Preset.mrdn"), "*.mrdn");
                             chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                                                   [this] (const juce::FileChooser& fc)
                                                   {
                                                       auto f = fc.getResult();
                                                       if (f != juce::File())
                                                       {
                                                           proc.savePreset (f.withFileExtension ("mrdn"));
                                                           refreshHeader();
                                                       }
                                                   });
                         }
                         else if (r == 2)
                         {
                             chooser = std::make_unique<juce::FileChooser> ("Load preset", proc.presetDirectory(), "*.mrdn");
                             chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                                   [this] (const juce::FileChooser& fc)
                                                   {
                                                       if (fc.getResult().existsAsFile())
                                                       {
                                                           proc.loadPreset (fc.getResult());
                                                           refreshHeader();
                                                       }
                                                   });
                         }
                         else if (r == 3) { proc.resetToDefault(); display.selectBand (-1); refreshHeader(); }
                         else if (r == 4) { proc.presetDirectory().startAsProcess(); }
                     });
}

void MeridianAudioProcessorEditor::showSettingsMenu()
{
    auto ui = proc.uiState();
    juce::PopupMenu m, speed, tilt;
    const int sp = ui.getProperty ("anaSpeed");
    const float tl = ui.getProperty ("anaTilt");
    speed.addItem (10, "Slow",   true, sp == 0);
    speed.addItem (11, "Medium", true, sp == 1);
    speed.addItem (12, "Fast",   true, sp == 2);
    tilt.addItem (20, "0 dB/oct",   true, juce::approximatelyEqual (tl, 0.0f));
    tilt.addItem (21, "3 dB/oct",   true, juce::approximatelyEqual (tl, 3.0f));
    tilt.addItem (22, "4.5 dB/oct", true, juce::approximatelyEqual (tl, 4.5f));

    m.addSectionHeader ("Analyzer");
    m.addSubMenu ("Speed", speed);
    m.addSubMenu ("Tilt", tilt);
    m.addSeparator();
    m.addItem (30, "Remove All Bands");
    m.addSeparator();
    m.addItem (-1, "Meridian 0.1.0", false);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&gearButton).withMinimumWidth (190),
                     [this] (int r)
                     {
                         auto u = proc.uiState();
                         if (r >= 10 && r <= 12) u.setProperty ("anaSpeed", r - 10, nullptr);
                         else if (r >= 20 && r <= 22) u.setProperty ("anaTilt", r == 20 ? 0.0f : r == 21 ? 3.0f : 4.5f, nullptr);
                         else if (r == 30)
                         {
                             proc.undoManager.beginNewTransaction();
                             proc.soloBand = -1;
                             for (int b = 0; b < kMaxBands; ++b)
                                 if (proc.apvts.getRawParameterValue (params::bandId (b, "used"))->load() > 0.5f)
                                     proc.removeBand (b);
                             display.selectBand (-1);
                         }
                     });
}

void MeridianAudioProcessorEditor::showModeMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Processing Mode");
    m.addItem (1, "Zero Latency", true, true);
    m.addItem (2, "Natural Phase  (v2)", false);
    m.addItem (3, "Linear Phase  (v2)", false);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&modeButton).withMinimumWidth (200));
}

void MeridianAudioProcessorEditor::showAnalyzerMenu()
{
    juce::PopupMenu m;
    const int mode = display.analyzerMode();
    const char* names[] { "Off", "Pre", "Post", "Pre + Post" };
    m.addSectionHeader ("Spectrum Analyzer");
    for (int i = 0; i < 4; ++i)
        m.addItem (i + 1, names[i], true, i == mode);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&analyzerButton).withMinimumWidth (analyzerButton.getWidth()),
                     [this] (int r)
                     {
                         if (r > 0) { proc.uiState().setProperty ("analyzer", r - 1, nullptr); refreshHeader(); }
                     });
}

void MeridianAudioProcessorEditor::showRangeMenu()
{
    juce::PopupMenu m;
    const float range = display.rangeDb();
    const float ranges[] { 3.0f, 6.0f, 12.0f, 30.0f };
    m.addSectionHeader ("Display Range");
    for (int i = 0; i < 4; ++i)
        m.addItem (i + 1, juce::String (juce::CharPointer_UTF8 ("\xc2\xb1")) + juce::String ((int) ranges[i]) + " dB", true, juce::approximatelyEqual (ranges[i], range));
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&rangeButton).withMinimumWidth (rangeButton.getWidth()),
                     [this, ranges] (int r)
                     {
                         if (r > 0) { proc.uiState().setProperty ("range", ranges[r - 1], nullptr); refreshHeader(); }
                     });
}
