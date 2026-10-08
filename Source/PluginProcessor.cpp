#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace meridian;

MeridianAudioProcessor::MeridianAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "MERIDIAN", params::createLayout())
{
    for (int b = 0; b < kMaxBands; ++b)
    {
        auto get = [&] (const char* f) { return apvts.getRawParameterValue (params::bandId (b, f)); };
        bp[(size_t) b] = { get ("used"), get ("on"), get ("shape"), get ("freq"),
                           get ("gain"), get ("q"),  get ("slope"), get ("place") };
    }
    outGainP   = apvts.getRawParameterValue (params::outGain);
    outPanP    = apvts.getRawParameterValue (params::outPan);
    gainScaleP = apvts.getRawParameterValue (params::gainScale);
    autoGainP  = apvts.getRawParameterValue (params::autoGain);

    uiState();
}

bool MeridianAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

// ---------------------------------------------------------------------------------------------
BandSettings MeridianAudioProcessor::readBand (int b) const
{
    const auto& p = bp[(size_t) b];
    BandSettings s;
    s.used      = p.used->load() > 0.5f;
    s.enabled   = p.on->load() > 0.5f;
    s.shape     = (Shape) juce::roundToInt (p.shape->load());
    s.freq      = p.freq->load();
    s.gain      = p.gain->load() * gainScaleP->load() * 0.01f;
    s.q         = p.q->load();
    s.slope     = juce::roundToInt (p.slope->load());
    s.placement = (Placement) juce::roundToInt (p.place->load());
    return s;
}

float MeridianAudioProcessor::readRawGain (int b) const { return bp[(size_t) b].gain->load(); }

void MeridianAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    currentSampleRate = sampleRate;

    for (int b = 0; b < kMaxBands; ++b)
    {
        auto& d = bands[(size_t) b];
        d.target = readBand (b);
        d.current = d.target;
        d.freq.reset (sr, 0.03);  d.freq.setCurrentAndTargetValue (d.target.freq);
        d.q.reset (sr, 0.03);     d.q.setCurrentAndTargetValue (d.target.q);
        d.gain.reset (sr, 0.03);  d.gain.setCurrentAndTargetValue (d.target.gain);
        d.sections = design (d.current, sr);
        d.resetState();
        d.wasActive = d.target.used;
        d.dirty = false;
    }

    outGainSmoothed.reset (sr, 0.03);  outGainSmoothed.setCurrentAndTargetValue (outGainP->load());
    panSmoothed.reset (sr, 0.03);      panSmoothed.setCurrentAndTargetValue (outPanP->load());
    autoGainSmoothed.reset (sr, 0.15); autoGainSmoothed.setCurrentAndTargetValue (0.0f);
    targetsChanged = true;
    std::memset (soloZ, 0, sizeof (soloZ));
    lastSolo = -1;
}

void MeridianAudioProcessor::updateTargets()
{
    for (int b = 0; b < kMaxBands; ++b)
    {
        auto& d = bands[(size_t) b];
        const auto t = readBand (b);

        const bool structural = t.used != d.target.used || t.enabled != d.target.enabled
                             || t.shape != d.target.shape || t.slope != d.target.slope
                             || t.placement != d.target.placement;

        if (structural || ! t.sameFilterAs (d.target))
            targetsChanged = true;

        // Newly switched on: start at the target, don't glide in from stale values.
        if (t.used && ! d.wasActive)
        {
            d.freq.setCurrentAndTargetValue (t.freq);
            d.q.setCurrentAndTargetValue (t.q);
            d.gain.setCurrentAndTargetValue (t.gain);
            d.resetState();
            d.wasActive = true;
            d.dirty = true;
        }
        else if (! t.used)
        {
            d.wasActive = false;
        }

        if (structural)
            d.dirty = true;

        d.target = t;
        d.freq.setTargetValue (t.freq);
        d.q.setTargetValue (t.q);
        d.gain.setTargetValue (t.gain);
    }
}

float MeridianAudioProcessor::computeAutoGainDb() const
{
    // Average the EQ curve (in dB) over 40 log-spaced points, 30 Hz–16 kHz: equal weight per
    // octave, roughly how pink-ish programme material spreads its energy.
    constexpr int N = 40;
    std::array<SectionSet, kMaxBands> sets;
    for (int b = 0; b < kMaxBands; ++b)
        sets[(size_t) b] = design (bands[(size_t) b].target, sr);

    double sum = 0.0;
    for (int i = 0; i < N; ++i)
    {
        const double f = 30.0 * std::pow (16000.0 / 30.0, i / (double) (N - 1));
        for (int b = 0; b < kMaxBands; ++b)
        {
            const auto& t = bands[(size_t) b].target;
            if (! t.used || ! t.enabled) continue;
            // Side/L/R bands affect only part of the signal: count them at half weight.
            const double w = t.placement == Placement::Stereo || t.placement == Placement::Mid ? 1.0 : 0.5;
            sum += w * sets[(size_t) b].magnitudeDb (f, sr);
        }
    }
    return juce::jlimit (-24.0f, 24.0f, (float) (-sum / N));
}

void MeridianAudioProcessor::runSections (const SectionSet& set, double (*z)[2], float* x, int n)
{
    for (int k = 0; k < set.count; ++k)
    {
        const auto& c = set.s[(size_t) k];
        double z1 = z[k][0], z2 = z[k][1];
        for (int i = 0; i < n; ++i)
        {
            const double in  = x[i];
            const double out = c.b0 * in + z1;
            z1 = c.b1 * in - c.a1 * out + z2;
            z2 = c.b2 * in - c.a2 * out;
            x[i] = (float) out;
        }
        z[k][0] = z1;
        z[k][1] = z2;
    }

    if (! juce::exactlyEqual (set.gain, 1.0))
        for (int i = 0; i < n; ++i)
            x[i] = (float) (x[i] * set.gain);
}

void MeridianAudioProcessor::processBand (BandDSP& d, float* l, float* r, int n, bool stereo)
{
    switch (d.current.placement)
    {
        case Placement::Stereo:
            runSections (d.sections, d.z[0], l, n);
            if (stereo) runSections (d.sections, d.z[1], r, n);
            break;

        case Placement::Left:
            runSections (d.sections, d.z[0], l, n);
            break;

        case Placement::Right:
            runSections (d.sections, d.z[1], stereo ? r : l, n);
            break;

        case Placement::Mid:
        case Placement::Side:
        {
            if (! stereo)
            {
                if (d.current.placement == Placement::Mid)
                    runSections (d.sections, d.z[0], l, n);
                break;
            }
            float m[kChunk], s[kChunk];
            for (int i = 0; i < n; ++i)
            {
                m[i] = 0.5f * (l[i] + r[i]);
                s[i] = 0.5f * (l[i] - r[i]);
            }
            runSections (d.sections, d.z[0], d.current.placement == Placement::Mid ? m : s, n);
            for (int i = 0; i < n; ++i)
            {
                l[i] = m[i] + s[i];
                r[i] = m[i] - s[i];
            }
            break;
        }
    }
}

void MeridianAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();
    if (numCh == 0 || numSamples == 0)
        return;

    const bool stereo = numCh == 2;
    float* L = buffer.getWritePointer (0);
    float* R = stereo ? buffer.getWritePointer (1) : nullptr;

    preFifo.push (L, R, numSamples);

    updateTargets();
    if (targetsChanged)
    {
        lastAutoGainDb = computeAutoGainDb();
        targetsChanged = false;
    }
    autoGainSmoothed.setTargetValue (autoGainP->load() > 0.5f ? lastAutoGainDb : 0.0f);
    appliedAutoGainDb = autoGainP->load() > 0.5f ? lastAutoGainDb : 0.0f;
    outGainSmoothed.setTargetValue (outGainP->load());
    panSmoothed.setTargetValue (outPanP->load());

    // Solo: replace the EQ with a band-pass around the soloed band so you hear what it touches.
    const int solo = soloBand.load();
    const bool soloActive = solo >= 0 && solo < kMaxBands && bands[(size_t) solo].target.used;
    if (soloActive)
    {
        const auto& t = bands[(size_t) solo].target;
        BandSettings bpSet;
        bpSet.used = true;
        bpSet.shape = Shape::BandPass;
        bpSet.freq = t.freq;
        bpSet.q = juce::jlimit (0.5f, 40.0f, t.q);
        bpSet.slope = 1;
        soloSections = design (bpSet, sr);
        if (solo != lastSolo)
            std::memset (soloZ, 0, sizeof (soloZ));
    }
    lastSolo = soloActive ? solo : -1;

    for (int start = 0; start < numSamples; start += kChunk)
    {
        const int n = juce::jmin (kChunk, numSamples - start);
        float* l = L + start;
        float* r = stereo ? R + start : nullptr;

        for (auto& d : bands)
        {
            if (! d.target.used)
                continue;

            const bool gliding = d.freq.isSmoothing() || d.q.isSmoothing() || d.gain.isSmoothing();
            if (gliding || d.dirty)
            {
                d.current = d.target;
                d.current.freq = d.freq.skip (n);
                d.current.q    = d.q.skip (n);
                d.current.gain = d.gain.skip (n);
                d.sections = design (d.current, sr);
                d.dirty = false;
            }

            if (! soloActive)
                processBand (d, l, r, n, stereo);
        }

        if (soloActive)
        {
            runSections (soloSections, soloZ[0], l, n);
            if (stereo) runSections (soloSections, soloZ[1], r, n);
        }
    }

    // Output: gain, auto gain, balance-style pan.
    float pkL = 0.0f, pkR = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float g   = juce::Decibels::decibelsToGain (outGainSmoothed.getNextValue() + autoGainSmoothed.getNextValue());
        const float pan = panSmoothed.getNextValue();
        L[i] *= g * (pan > 0.0f ? 1.0f - pan : 1.0f);
        pkL = juce::jmax (pkL, std::abs (L[i]));
        if (stereo)
        {
            R[i] *= g * (pan < 0.0f ? 1.0f + pan : 1.0f);
            pkR = juce::jmax (pkR, std::abs (R[i]));
        }
    }
    if (! stereo) pkR = pkL;

    peakL = juce::jmax (peakL.load(), pkL);
    peakR = juce::jmax (peakR.load(), pkR);
    postFifo.push (L, R, numSamples);

    for (int ch = numCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
}

// ---------------------------------------------------------------------------------------------
void MeridianAudioProcessor::setParam (const juce::String& id, float realValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

int MeridianAudioProcessor::addBand (Shape shape, float freq, float gainDb, float q)
{
    for (int b = 0; b < kMaxBands; ++b)
    {
        if (bp[(size_t) b].used->load() > 0.5f)
            continue;

        const int slope = (shape == Shape::LowCut || shape == Shape::HighCut) ? 3 : 1;
        setParam (params::bandId (b, "shape"), (float) shape);
        setParam (params::bandId (b, "freq"),  freq);
        setParam (params::bandId (b, "gain"),  shapeHasGain (shape) ? gainDb : 0.0f);
        setParam (params::bandId (b, "q"),     q);
        setParam (params::bandId (b, "slope"), (float) slope);
        setParam (params::bandId (b, "place"), 0.0f);
        setParam (params::bandId (b, "on"),    1.0f);
        setParam (params::bandId (b, "used"),  1.0f);
        return b;
    }
    return -1;
}

void MeridianAudioProcessor::removeBand (int band)
{
    if (soloBand.load() == band)
        soloBand = -1;
    setParam (params::bandId (band, "used"), 0.0f);
}

juce::ValueTree MeridianAudioProcessor::uiState()
{
    auto ui = apvts.state.getOrCreateChildWithName ("UI", nullptr);
    if (! ui.hasProperty ("range"))       ui.setProperty ("range", 12.0f, nullptr);
    if (! ui.hasProperty ("analyzer"))    ui.setProperty ("analyzer", 3, nullptr);   // 0 off, 1 pre, 2 post, 3 both
    if (! ui.hasProperty ("anaSpeed"))    ui.setProperty ("anaSpeed", 1, nullptr);   // slow / medium / fast
    if (! ui.hasProperty ("anaTilt"))     ui.setProperty ("anaTilt", 4.5f, nullptr); // dB/oct
    return ui;
}

void MeridianAudioProcessor::switchToSlot (int slot)
{
    if (slot == activeSlot)
        return;
    slots[activeSlot] = apvts.copyState();
    activeSlot = slot;
    if (slots[slot].isValid())
        apvts.replaceState (slots[slot].createCopy());
    // An empty B slot starts as a copy of A: nothing to load.
    uiState();
}

void MeridianAudioProcessor::copyCurrentToOther()
{
    slots[1 - activeSlot] = apvts.copyState();
}

// ---------------------------------------------------------------------------------------------
juce::File MeridianAudioProcessor::presetDirectory() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Seraph").getChildFile ("Meridian").getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

void MeridianAudioProcessor::savePreset (const juce::File& file)
{
    if (auto xml = apvts.copyState().createXml())
    {
        xml->writeTo (file);
        presetName = file.getFileNameWithoutExtension();
        if (onPresetChanged) onPresetChanged();
    }
}

bool MeridianAudioProcessor::loadPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;

    undoManager.beginNewTransaction();
    soloBand = -1;
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    uiState();
    presetName = file.getFileNameWithoutExtension();
    if (onPresetChanged) onPresetChanged();
    return true;
}

void MeridianAudioProcessor::resetToDefault()
{
    undoManager.beginNewTransaction();
    soloBand = -1;
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
    presetName = "Default Setting";
    if (onPresetChanged) onPresetChanged();
}

void MeridianAudioProcessor::stepPreset (int delta)
{
    auto files = presetDirectory().findChildFiles (juce::File::findFiles, false, "*.mrdn");
    if (files.isEmpty())
        return;
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareNatural (b.getFileName()) < 0; });

    int idx = -1;
    for (int i = 0; i < files.size(); ++i)
        if (files[i].getFileNameWithoutExtension() == presetName)
            idx = i;

    idx = idx < 0 ? (delta > 0 ? 0 : files.size() - 1)
                  : (idx + delta + files.size()) % files.size();
    loadPreset (files[idx]);
}

// ---------------------------------------------------------------------------------------------
void MeridianAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presetName, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MeridianAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            presetName = tree.getProperty ("presetName", "Default Setting").toString();
            apvts.replaceState (tree);
            uiState();
            if (onPresetChanged) onPresetChanged();
        }
}

juce::AudioProcessorEditor* MeridianAudioProcessor::createEditor()
{
    return new MeridianAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MeridianAudioProcessor();
}
