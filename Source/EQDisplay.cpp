#include "EQDisplay.h"

using namespace meridian;

namespace
{
    constexpr float kNodeRadius = 9.0f;

    juce::String noteName (float f)
    {
        static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const float midi = 69.0f + 12.0f * std::log2 (f / 440.0f);
        const int n = juce::roundToInt (midi);
        const int cents = juce::roundToInt ((midi - (float) n) * 100.0f);
        juce::String s = juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
        if (cents != 0) s << (cents > 0 ? " +" : " ") << cents << "c";
        return s;
    }

    juce::String freqLabel (float f)
    {
        if (f >= 1000.0f) return juce::String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + " kHz";
        return juce::String (f, f < 100.0f ? 1 : 0) + " Hz";
    }
}

EQDisplay::EQDisplay (MeridianAudioProcessor& p) : proc (p), panel (p)
{
    setWantsKeyboardFocus (true);
    setOpaque (true);

    panel.onDeleteBand = [this] (int b) { deleteBand (b); };
    addChildComponent (panel);

    fftData.assign ((size_t) fftSize * 2, 0.0f);
    pullBuffer.assign (1 << 15, 0.0f);
    preHistory.assign ((size_t) fftSize, 0.0f);
    postHistory.assign ((size_t) fftSize, 0.0f);

    startTimerHz (60);
}

EQDisplay::~EQDisplay() { stopTimer(); }

// ---------------------------------------------------------------------------------------------
float EQDisplay::rangeDb() const
{
    return (float) proc.apvts.state.getChildWithName ("UI").getProperty ("range", 12.0f);
}

int EQDisplay::analyzerMode() const
{
    return (int) proc.apvts.state.getChildWithName ("UI").getProperty ("analyzer", 3);
}

juce::Rectangle<float> EQDisplay::plotArea() const { return getLocalBounds().toFloat(); }

float EQDisplay::xForFreq (float f) const
{
    const auto a = plotArea();
    return a.getX() + a.getWidth() * std::log (f / minF) / std::log (maxF / minF);
}

float EQDisplay::freqForX (float x) const
{
    const auto a = plotArea();
    return minF * std::pow (maxF / minF, (x - a.getX()) / a.getWidth());
}

float EQDisplay::yForDb (float db) const
{
    const auto a = plotArea();
    const float half = a.getHeight() * 0.5f - 44.0f;
    return a.getCentreY() - db / rangeDb() * half;
}

float EQDisplay::dbForY (float y) const
{
    const auto a = plotArea();
    const float half = a.getHeight() * 0.5f - 44.0f;
    return (a.getCentreY() - y) / half * rangeDb();
}

float EQDisplay::yForAnalyzer (float db) const
{
    const auto a = plotArea();
    constexpr float top = 0.0f, bottom = -90.0f;
    return a.getY() + (top - db) / (top - bottom) * a.getHeight();
}

juce::RangedAudioParameter* EQDisplay::param (int b, const char* field) const
{
    return proc.apvts.getParameter (params::bandId (b, field));
}

void EQDisplay::setParamValue (int b, const char* field, float v)
{
    if (auto* p = param (b, field))
        p->setValueNotifyingHost (p->convertTo0to1 (v));
}

bool EQDisplay::bandUsed (int b) const
{
    return b >= 0 && b < kMaxBands && settings[(size_t) b].used;
}

juce::Point<float> EQDisplay::nodePosition (int b) const
{
    const auto& s = settings[(size_t) b];
    const float y = shapeHasGain (s.shape) ? yForDb (s.gain) : yForDb (0.0f);
    return { xForFreq (s.freq), juce::jlimit (6.0f, (float) getHeight() - 6.0f, y) };
}

bool EQDisplay::hasQHandles (int b) const
{
    const auto shape = settings[(size_t) b].shape;
    return shape == Shape::Bell || shape == Shape::Notch || shape == Shape::BandPass;
}

std::pair<float, float> EQDisplay::qHandleFreqs (int b) const
{
    // Bandwidth in octaves for a given Q; handles sit at the band edges.
    const auto& s = settings[(size_t) b];
    const float bw = 2.0f * std::asinh (1.0f / (2.0f * s.q)) / std::log (2.0f);
    const float k = std::pow (2.0f, juce::jmin (bw, 6.0f) * 0.5f);
    return { s.freq / k, s.freq * k };
}

int EQDisplay::hitTestNode (juce::Point<float> p) const
{
    // Topmost first: selected band wins ties.
    if (bandUsed (selected) && nodePosition (selected).getDistanceFrom (p) <= kNodeRadius + 5.0f)
        return selected;
    for (int b = kMaxBands - 1; b >= 0; --b)
        if (bandUsed (b) && nodePosition (b).getDistanceFrom (p) <= kNodeRadius + 5.0f)
            return b;
    return -1;
}

int EQDisplay::hitTestQHandle (juce::Point<float> p) const
{
    if (! bandUsed (selected) || ! hasQHandles (selected))
        return -1;
    const auto node = nodePosition (selected);
    const auto [lo, hi] = qHandleFreqs (selected);
    if (std::abs (p.y - node.y) > 8.0f)
        return -1;
    if (std::abs (p.x - xForFreq (lo)) <= 7.0f) return 0;
    if (std::abs (p.x - xForFreq (hi)) <= 7.0f) return 1;
    return -1;
}

// ---------------------------------------------------------------------------------------------
void EQDisplay::selectBand (int b)
{
    selected = bandUsed (b) ? b : -1;
    panel.setBand (selected);
    panel.setVisible (selected >= 0);
    positionPanel();
    repaint();
}

void EQDisplay::positionPanel()
{
    if (selected < 0)
        return;

    const auto node = nodePosition (selected);
    const int w = BandPanel::kWidth, h = BandPanel::kHeight;
    int x = juce::roundToInt (node.x) - w / 2;
    int y = juce::roundToInt (node.y) + 40;
    if (y + h > getHeight() - 26)                       // not enough room below: go above the node
        y = juce::roundToInt (node.y) - 40 - h;
    x = juce::jlimit (8, juce::jmax (8, getWidth() - w - 8), x);
    y = juce::jlimit (8, juce::jmax (8, getHeight() - h - 26), y);
    panel.setTopLeftPosition (x, y);
}

void EQDisplay::deleteBand (int b)
{
    proc.removeBand (b);
    settings[(size_t) b].used = false;
    if (selected == b)
        selectBand (-1);
    hover = -1;
    repaint();
}

void EQDisplay::showBandMenu (int b)
{
    juce::PopupMenu m, shapes, slopes, places;
    const auto& s = settings[(size_t) b];
    for (int i = 0; i < kNumShapes; ++i)     shapes.addItem (100 + i, shapeNames()[i], true, (int) s.shape == i);
    for (int i = 0; i < kNumSlopes; ++i)     slopes.addItem (200 + i, slopeNames()[i], shapeHasSlope (s.shape), s.slope == i);
    for (int i = 0; i < kNumPlacements; ++i) places.addItem (300 + i, placementNames()[i], true, (int) s.placement == i);

    m.addSectionHeader ("Band " + juce::String (b + 1));
    m.addSubMenu ("Shape", shapes);
    m.addSubMenu ("Slope", slopes, shapeHasSlope (s.shape));
    m.addSubMenu ("Stereo Placement", places);
    m.addSeparator();
    m.addItem (1, s.enabled ? "Bypass" : "Enable");
    m.addItem (2, proc.soloBand.load() == b ? "Unsolo" : "Solo");
    m.addItem (3, "Delete");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ juce::Desktop::getMousePosition(), juce::Desktop::getMousePosition() }),
                     [this, b] (int r)
                     {
                         if (r <= 0 || ! bandUsed (b)) return;
                         proc.undoManager.beginNewTransaction();
                         if (r == 1)        proc.setParam (params::bandId (b, "on"), settings[(size_t) b].enabled ? 0.0f : 1.0f);
                         else if (r == 2)   proc.soloBand = proc.soloBand.load() == b ? -1 : b;
                         else if (r == 3)   deleteBand (b);
                         else if (r >= 300) proc.setParam (params::bandId (b, "place"), (float) (r - 300));
                         else if (r >= 200) proc.setParam (params::bandId (b, "slope"), (float) (r - 200));
                         else if (r >= 100) proc.setParam (params::bandId (b, "shape"), (float) (r - 100));
                         panel.refresh();
                     });
}

// ---------------------------------------------------------------------------------------------
void EQDisplay::mouseMove (const juce::MouseEvent& e)
{
    mousePos = e.position;
    mouseInside = true;
    const int h = hitTestNode (e.position);
    const int qh = hitTestQHandle (e.position);
    if (h != hover) { hover = h; }
    setMouseCursor (qh >= 0 ? juce::MouseCursor::LeftRightResizeCursor
                            : (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor));
    repaint();
}

void EQDisplay::mouseExit (const juce::MouseEvent&)
{
    mouseInside = false;
    hover = -1;
    repaint();
}

void EQDisplay::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    proc.undoManager.beginNewTransaction();

    dragHandle = hitTestQHandle (e.position);
    const int hit = dragHandle >= 0 ? selected : hitTestNode (e.position);

    if (hit < 0)
    {
        if (! e.mods.isPopupMenu())
            selectBand (-1);
        return;
    }

    if (e.mods.isPopupMenu())
    {
        selectBand (hit);
        showBandMenu (hit);
        return;
    }

    if (e.mods.isAltDown())          // Alt-click: delete
    {
        deleteBand (hit);
        return;
    }

    selectBand (hit);
    dragBand  = hit;
    dragStart = e.position;
    startFreq = proc.apvts.getRawParameterValue (params::bandId (hit, "freq"))->load();
    startGain = proc.apvts.getRawParameterValue (params::bandId (hit, "gain"))->load();
    startQ    = proc.apvts.getRawParameterValue (params::bandId (hit, "q"))->load();

    for (auto* f : { "freq", "gain", "q" })
        param (hit, f)->beginChangeGesture();
}

void EQDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand < 0)
        return;

    mousePos = e.position;
    const float fine = e.mods.isShiftDown() ? 0.15f : 1.0f;
    const float dx = (e.position.x - dragStart.x) * fine;
    const float dy = (e.position.y - dragStart.y) * fine;
    const auto& s = settings[(size_t) dragBand];

    if (dragHandle >= 0)
    {
        // Dragging a handle outward widens the band (lower Q).
        const float outward = dragHandle == 0 ? -dx : dx;
        setParamValue (dragBand, "q", juce::jlimit (params::minQ, params::maxQ, startQ * std::pow (2.0f, -outward / 45.0f)));
    }
    else
    {
        const float f = startFreq * std::pow (maxF / minF, dx / plotArea().getWidth());
        setParamValue (dragBand, "freq", juce::jlimit (params::minFreq, params::maxFreq, f));

        if (shapeHasGain (s.shape))
        {
            const float scale = juce::jmax (0.01f, proc.apvts.getRawParameterValue (params::gainScale)->load() * 0.01f);
            const float half = plotArea().getHeight() * 0.5f - 44.0f;
            const float dDb = -dy / half * rangeDb() / scale;
            setParamValue (dragBand, "gain", juce::jlimit (-params::maxGain, params::maxGain, startGain + dDb));
        }
        else
        {
            setParamValue (dragBand, "q", juce::jlimit (params::minQ, params::maxQ, startQ * std::pow (2.0f, -dy / 40.0f)));
        }
    }

    timerCallback();
}

void EQDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragBand >= 0)
        for (auto* f : { "freq", "gain", "q" })
            param (dragBand, f)->endChangeGesture();
    dragBand = -1;
    dragHandle = -1;
}

void EQDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int hit = hitTestNode (e.position);
    proc.undoManager.beginNewTransaction();

    if (hit >= 0)   // double-click a node: bypass/enable
    {
        proc.setParam (params::bandId (hit, "on"), settings[(size_t) hit].enabled ? 0.0f : 1.0f);
        panel.refresh();
        return;
    }

    // Double-click empty space: new band. Near the edges you get cuts, elsewhere a bell.
    const float f = juce::jlimit (params::minFreq, params::maxFreq, freqForX (e.position.x));
    const float scale = juce::jmax (0.01f, proc.apvts.getRawParameterValue (params::gainScale)->load() * 0.01f);
    const float g = juce::jlimit (-params::maxGain, params::maxGain, dbForY (e.position.y) / scale);

    Shape shape = Shape::Bell;
    if (f < 30.0f)          shape = Shape::LowCut;
    else if (f > 15000.0f)  shape = Shape::HighCut;

    const int b = proc.addBand (shape, f, g, shape == Shape::Bell ? 1.0f : 0.7071f);
    if (b >= 0)
    {
        timerCallback();
        selectBand (b);
    }
}

void EQDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int b = hitTestNode (e.position);
    if (b < 0) b = selected;
    if (b < 0)
        return;

    auto* p = param (b, "q");
    const float q = proc.apvts.getRawParameterValue (params::bandId (b, "q"))->load();
    const float step = (w.isReversed ? -w.deltaY : w.deltaY) * (e.mods.isShiftDown() ? 0.25f : 1.5f);
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (params::minQ, params::maxQ, q * std::pow (2.0f, step))));
    p->endChangeGesture();
}

bool EQDisplay::keyPressed (const juce::KeyPress& k)
{
    if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && selected >= 0)
    {
        proc.undoManager.beginNewTransaction();
        deleteBand (selected);
        return true;
    }
    if (k == juce::KeyPress::escapeKey)
    {
        selectBand (-1);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------------------------
void EQDisplay::updateAnalyzer (SampleFifo& fifo, std::vector<float>& history, std::vector<float>& levels)
{
    // Append new samples to the history window (keeps the most recent fftSize samples).
    int got = fifo.pull (pullBuffer.data(), (int) pullBuffer.size());
    const bool silent = got == 0;
    if (got > fftSize)
    {
        std::copy (pullBuffer.begin() + (got - fftSize), pullBuffer.begin() + got, history.begin());
    }
    else if (got > 0)
    {
        std::move (history.begin() + got, history.end(), history.begin());
        std::copy (pullBuffer.begin(), pullBuffer.begin() + got, history.end() - got);
    }

    const int cols = getWidth() / kColumnStep + 1;
    if ((int) levels.size() != cols)
        levels.assign ((size_t) cols, -120.0f);

    const auto ui = proc.apvts.state.getChildWithName ("UI");
    const int speed = (int) ui.getProperty ("anaSpeed", 1);
    const float tilt = (float) ui.getProperty ("anaTilt", 4.5f);
    const float fallPerFrame = (speed == 0 ? 18.0f : speed == 1 ? 36.0f : 72.0f) / 60.0f;  // dB/s at 60 fps

    if (silent)
    {
        for (auto& l : levels) l -= fallPerFrame;
        return;
    }

    std::fill (fftData.begin(), fftData.end(), 0.0f);
    std::copy (history.begin(), history.end(), fftData.begin());
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    const double sr = proc.currentSampleRate.load();
    const float norm = (float) fftSize * 0.25f;   // Hann coherent gain 0.5, one-sided x2
    const int maxBin = fftSize / 2 - 1;

    for (int c = 0; c < cols; ++c)
    {
        const float x  = (float) (c * kColumnStep);
        const float f  = freqForX (x);
        const float b0 = freqForX (x - kColumnStep * 0.5f) * fftSize / (float) sr;
        const float b1 = freqForX (x + kColumnStep * 0.5f) * fftSize / (float) sr;

        float mag;
        if (b1 - b0 < 1.0f)
        {
            // Fewer bins than pixels: interpolate between neighbouring bins.
            const float bin = juce::jlimit (0.0f, (float) maxBin - 1.0f, f * fftSize / (float) sr);
            const int i = (int) bin;
            const float t = bin - (float) i;
            mag = fftData[(size_t) i] * (1.0f - t) + fftData[(size_t) i + 1] * t;
        }
        else
        {
            mag = 0.0f;
            for (int i = juce::jmax (0, (int) b0); i <= juce::jmin (maxBin, (int) b1); ++i)
                mag = juce::jmax (mag, fftData[(size_t) i]);
        }

        float db = juce::Decibels::gainToDecibels (mag / norm, -140.0f) + tilt * std::log2 (f / 1000.0f);
        if (f > sr * 0.5) db = -140.0f;

        auto& l = levels[(size_t) c];
        l = db > l ? db : juce::jmax (db, l - fallPerFrame);
    }
}

void EQDisplay::timerCallback()
{
    const double sr = proc.currentSampleRate.load();
    for (int b = 0; b < kMaxBands; ++b)
    {
        settings[(size_t) b] = proc.readBand (b);
        designs[(size_t) b] = design (settings[(size_t) b], sr);
    }

    if (selected >= 0 && ! bandUsed (selected))
        selectBand (-1);

    if (analyzerMode() != 0)
    {
        updateAnalyzer (proc.preFifo,  preHistory,  preLevels);
        updateAnalyzer (proc.postFifo, postHistory, postLevels);
    }
    else
    {
        float scratch[1024];
        while (proc.preFifo.pull (scratch, 1024) > 0) {}
        while (proc.postFifo.pull (scratch, 1024) > 0) {}
    }

    positionPanel();
    if (++frameCounter % 6 == 0)
        panel.refresh();

    repaint();
}

// ---------------------------------------------------------------------------------------------
void EQDisplay::paint (juce::Graphics& g)
{
    const auto area = plotArea();
    g.fillAll (theme::display);
    drawGrid (g, area);
    if (analyzerMode() != 0)
        drawAnalyzer (g, area);
    drawCurves (g, area);
    drawNodes (g);
    drawReadout (g);
}

void EQDisplay::resized()
{
    preLevels.clear();
    postLevels.clear();
    positionPanel();
}

void EQDisplay::drawGrid (juce::Graphics& g, juce::Rectangle<float> a)
{
    // Minor frequency lines
    g.setColour (juce::Colour (0xff0b0b0b));
    for (float decade : { 10.0f, 100.0f, 1000.0f, 10000.0f })
        for (int m = 2; m <= 9; ++m)
        {
            const float f = decade * (float) m;
            if (f > minF && f < maxF && m != 2 && m != 5)
                g.drawVerticalLine (juce::roundToInt (xForFreq (f)), a.getY(), a.getBottom());
        }

    // Major lines + labels
    const float majors[] { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    const char* labels[] { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };
    g.setFont (theme::mono (12.0f));
    for (int i = 0; i < 10; ++i)
    {
        const float x = xForFreq (majors[i]);
        g.setColour (theme::grid);
        g.drawVerticalLine (juce::roundToInt (x), a.getY(), a.getBottom());
        g.setColour (theme::faint);
        g.drawText (labels[i], juce::Rectangle<float> (x - 30.0f, a.getBottom() - 20.0f, 60.0f, 16.0f), juce::Justification::centred);
    }

    // dB lines: quarter-range steps, labels every other line.
    const float range = rangeDb();
    const float step = range / 4.0f;
    for (int i = -4; i <= 4; ++i)
    {
        const float db = step * (float) i;
        const float y = yForDb (db);
        g.setColour (i == 0 ? theme::zeroLine : theme::grid);
        g.drawHorizontalLine (juce::roundToInt (y), a.getX(), a.getRight());

        if (i % 2 == 0)
        {
            juce::String t = i == 0 ? "0" : juce::String (std::abs (db), step < 1.0f ? 1 : 0);
            if (i > 0) t = "+" + t;
            if (i < 0) t = juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) + t;
            g.setColour (i == 0 ? theme::textSoft : theme::faint);
            g.drawText (t, juce::Rectangle<float> (a.getRight() - 52.0f, y - 8.0f, 44.0f, 16.0f), juce::Justification::centredRight);
        }
    }
}

void EQDisplay::drawAnalyzer (juce::Graphics& g, juce::Rectangle<float> a)
{
    const int mode = analyzerMode();

    auto makePath = [&] (const std::vector<float>& levels, bool closed)
    {
        juce::Path p;
        if (levels.empty()) return p;
        for (size_t c = 0; c < levels.size(); ++c)
        {
            const float x = (float) c * kColumnStep;
            const float y = juce::jlimit (a.getY() - 2.0f, a.getBottom() + 2.0f, yForAnalyzer (levels[c]));
            if (c == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        if (closed)
        {
            p.lineTo (a.getRight(), a.getBottom() + 2.0f);
            p.lineTo (a.getX(), a.getBottom() + 2.0f);
            p.closeSubPath();
        }
        return p;
    };

    if (mode == 1 || mode == 3)   // pre: outline only, dim
    {
        g.setColour (theme::text.withAlpha (mode == 1 ? 0.16f : 0.09f));
        g.strokePath (makePath (preLevels, false), juce::PathStrokeType (1.0f));
        if (mode == 1)
        {
            g.setColour (theme::text.withAlpha (0.04f));
            g.fillPath (makePath (preLevels, true));
        }
    }
    if (mode == 2 || mode == 3)   // post: filled
    {
        g.setColour (theme::text.withAlpha (0.045f));
        g.fillPath (makePath (postLevels, true));
        g.setColour (theme::text.withAlpha (0.14f));
        g.strokePath (makePath (postLevels, false), juce::PathStrokeType (1.0f));
    }
}

void EQDisplay::drawCurves (juce::Graphics& g, juce::Rectangle<float> a)
{
    const double sr = proc.currentSampleRate.load();
    const int cols = (int) a.getWidth() / kColumnStep + 2;
    const float yTop = a.getY() - 4.0f, yBot = a.getBottom() + 4.0f;
    auto clampY = [&] (float y) { return juce::jlimit (yTop, yBot, y); };

    bool anySplit = false;
    for (int b = 0; b < kMaxBands; ++b)
        if (settings[(size_t) b].used && settings[(size_t) b].placement != Placement::Stereo)
            anySplit = true;

    std::vector<float> mainDb ((size_t) cols, 0.0f), altDb ((size_t) cols, 0.0f), selDb ((size_t) cols, 0.0f), hovDb ((size_t) cols, 0.0f);

    for (int c = 0; c < cols; ++c)
    {
        const float f = freqForX (a.getX() + (float) (c * kColumnStep));
        if (f >= sr * 0.5) { mainDb[(size_t) c] = mainDb[(size_t) juce::jmax (0, c - 1)]; altDb[(size_t) c] = altDb[(size_t) juce::jmax (0, c - 1)];
                             selDb[(size_t) c] = selDb[(size_t) juce::jmax (0, c - 1)];  hovDb[(size_t) c] = hovDb[(size_t) juce::jmax (0, c - 1)]; continue; }
        for (int b = 0; b < kMaxBands; ++b)
        {
            const auto& s = settings[(size_t) b];
            if (! s.used || designs[(size_t) b].count == 0) continue;
            const float db = (float) designs[(size_t) b].magnitudeDb (f, sr);
            const auto pl = s.placement;
            if (pl == Placement::Stereo || pl == Placement::Left || pl == Placement::Mid)   mainDb[(size_t) c] += db;
            if (pl == Placement::Stereo || pl == Placement::Right || pl == Placement::Side) altDb[(size_t) c] += db;
            if (b == selected) selDb[(size_t) c] = db;
            if (b == hover)    hovDb[(size_t) c] = db;
        }
    }

    auto curvePath = [&] (const std::vector<float>& db)
    {
        juce::Path p;
        for (int c = 0; c < cols; ++c)
        {
            const float x = a.getX() + (float) (c * kColumnStep), y = clampY (yForDb (db[(size_t) c]));
            if (c == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        return p;
    };
    auto fillPath = [&] (const std::vector<float>& db)
    {
        auto p = curvePath (db);
        p.lineTo (a.getX() + (float) ((cols - 1) * kColumnStep), yForDb (0.0f));
        p.lineTo (a.getX(), yForDb (0.0f));
        p.closeSubPath();
        return p;
    };

    // Selected band: tinted area between its own curve and 0 dB.
    if (bandUsed (selected) && settings[(size_t) selected].enabled)
    {
        g.setColour (theme::bandColour (selected).withAlpha (0.22f));
        g.fillPath (fillPath (selDb));
    }
    if (bandUsed (hover) && hover != selected && settings[(size_t) hover].enabled)
    {
        g.setColour (theme::bandColour (hover).withAlpha (0.5f));
        g.strokePath (curvePath (hovDb), juce::PathStrokeType (1.0f));
    }

    if (anySplit)
    {
        juce::Path dashed;
        const float dashes[] { 4.0f, 3.0f };
        juce::PathStrokeType (1.4f).createDashedStroke (dashed, curvePath (altDb), dashes, 2);
        g.setColour (theme::textSoft.withAlpha (0.7f));
        g.fillPath (dashed);
    }

    g.setColour (theme::text);
    g.strokePath (curvePath (mainDb), juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void EQDisplay::drawNodes (juce::Graphics& g)
{
    g.setFont (theme::monoMedium (12.0f));

    auto drawNode = [&] (int b)
    {
        const auto& s = settings[(size_t) b];
        const auto p = nodePosition (b);
        const auto col = theme::bandColour (b);
        const bool sel = b == selected;
        const float r = sel ? kNodeRadius + 1.0f : kNodeRadius;

        if (sel && hasQHandles (b))
        {
            const auto [lo, hi] = qHandleFreqs (b);
            g.setColour (col);
            for (float f : { lo, hi })
            {
                const float x = xForFreq (f);
                g.drawLine (x - 5.0f, p.y, x + 5.0f, p.y, 2.0f);
            }
        }

        if (sel)
        {
            g.setColour (col.withAlpha (0.55f));
            g.drawEllipse (p.x - 16.0f, p.y - 16.0f, 32.0f, 32.0f, 1.5f);
        }
        if (b == hover && ! sel)
        {
            g.setColour (col.withAlpha (0.35f));
            g.drawEllipse (p.x - 14.0f, p.y - 14.0f, 28.0f, 28.0f, 1.0f);
        }

        if (s.enabled)
        {
            g.setColour (col);
            g.fillEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f);
            g.setColour (theme::chrome);
        }
        else
        {
            juce::Path ring, dashed;
            ring.addEllipse (p.x - r + 0.75f, p.y - r + 0.75f, r * 2.0f - 1.5f, r * 2.0f - 1.5f);
            const float dashes[] { 2.5f, 2.5f };
            juce::PathStrokeType (1.5f).createDashedStroke (dashed, ring, dashes, 2);
            g.setColour (theme::display);
            g.fillEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f);
            g.setColour (col);
            g.fillPath (dashed);
        }

        g.drawText (juce::String (b + 1), juce::Rectangle<float> (p.x - 12.0f, p.y - 8.0f, 24.0f, 16.0f).translated (0.0f, 0.5f),
                    juce::Justification::centred);

        if (proc.soloBand.load() == b)
        {
            g.setColour (theme::accent);
            g.drawEllipse (p.x - 20.0f, p.y - 20.0f, 40.0f, 40.0f, 1.0f);
        }
    };

    for (int b = 0; b < kMaxBands; ++b)
        if (bandUsed (b) && b != selected)
            drawNode (b);
    if (bandUsed (selected))
        drawNode (selected);
}

void EQDisplay::drawReadout (juce::Graphics& g)
{
    // Readout pill above the dragged / hovered / selected node.
    const int b = dragBand >= 0 ? dragBand : (hover >= 0 ? hover : selected);
    if (bandUsed (b))
    {
        const auto& s = settings[(size_t) b];
        const auto p = nodePosition (b);
        const juce::String sep (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
        juce::String t = freqLabel (s.freq) + sep;
        t += shapeHasGain (s.shape) ? params::formatDb (s.gain) : ("Q " + juce::String (s.q, 2));

        const auto font = theme::mono (12.5f);
        const float w = juce::GlyphArrangement::getStringWidth (font, t) + 20.0f, h = 22.0f;
        float y = p.y - 26.0f - h;
        if (y < 4.0f) y = p.y + 26.0f;
        auto pill = juce::Rectangle<float> (p.x - w * 0.5f, y, w, h);
        pill.setX (juce::jlimit (4.0f, (float) getWidth() - w - 4.0f, pill.getX()));

        g.setColour (theme::chrome.withAlpha (0.92f));
        g.fillRoundedRectangle (pill, 4.0f);
        g.setColour (theme::border2);
        g.drawRoundedRectangle (pill, 4.0f, 1.0f);
        g.setColour (theme::text);
        g.setFont (font);
        g.drawText (t, pill, juce::Justification::centred);
        return;
    }

    // Cursor readout in the bottom-left when hovering empty space: frequency and note.
    if (mouseInside && hover < 0 && ! panel.getBounds().toFloat().contains (mousePos))
    {
        const float f = freqForX (mousePos.x);
        const juce::String t = freqLabel (f) + "   " + noteName (f) + "   " + params::formatDb (dbForY (mousePos.y), 1);
        g.setFont (theme::mono (12.0f));
        g.setColour (theme::dim);
        g.drawText (t, juce::Rectangle<float> (12.0f, 10.0f, 360.0f, 16.0f), juce::Justification::centredLeft);
    }
}
