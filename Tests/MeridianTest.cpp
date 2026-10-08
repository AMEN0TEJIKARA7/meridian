// Offline checks for Meridian: measured DSP response vs. the design maths, M/S routing,
// stability under abuse, auto gain, and a rendered screenshot of the editor.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>

using namespace meridian;

static int failures = 0;
static void check (bool ok, const char* what, double got, double want)
{
    std::printf ("%s %-48s got %8.3f  want %8.3f\n", ok ? "PASS" : "FAIL", what, got, want);
    if (! ok) ++failures;
}

static constexpr double fs = 48000.0;
static constexpr int block = 512;

static void clearBands (MeridianAudioProcessor& p)
{
    for (int b = 0; b < kMaxBands; ++b) p.setParam (params::bandId (b, "used"), 0.0f);
    p.setParam (params::autoGain, 0.0f);
    p.setParam (params::outGain, 0.0f);
    p.setParam (params::gainScale, 100.0f);
}

// Feed a sine (L and R with the given polarity) and return the steady-state gain in dB per channel.
static std::pair<double, double> measure (MeridianAudioProcessor& p, double freq, float rPolarity = 1.0f)
{
    p.prepareToPlay (fs, block);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    double phase = 0.0, inc = 2.0 * juce::MathConstants<double>::pi * freq / fs;
    double sumIn = 0, sumL = 0, sumR = 0;
    const int settle = (int) (fs * 0.5), total = (int) (fs * 1.0);
    for (int pos = 0; pos < total; pos += block)
    {
        for (int i = 0; i < block; ++i)
        {
            const float s = 0.25f * (float) std::sin (phase);
            phase += inc;
            buf.setSample (0, i, s);
            buf.setSample (1, i, s * rPolarity);
        }
        p.processBlock (buf, midi);
        if (pos >= settle)
            for (int i = 0; i < block; ++i)
            {
                const double in = 0.25 * std::sin (phase - inc * (block - i));
                sumIn += in * in;
                sumL += (double) buf.getSample (0, i) * buf.getSample (0, i);
                sumR += (double) buf.getSample (1, i) * buf.getSample (1, i);
            }
    }
    return { 10.0 * std::log10 (sumL / sumIn), 10.0 * std::log10 (sumR / sumIn) };
}

static void setBand (MeridianAudioProcessor& p, int b, Shape s, float f, float g, float q, int slope = 1, Placement pl = Placement::Stereo)
{
    p.setParam (params::bandId (b, "shape"), (float) s);
    p.setParam (params::bandId (b, "freq"), f);
    p.setParam (params::bandId (b, "gain"), g);
    p.setParam (params::bandId (b, "q"), q);
    p.setParam (params::bandId (b, "slope"), (float) slope);
    p.setParam (params::bandId (b, "place"), (float) pl);
    p.setParam (params::bandId (b, "on"), 1.0f);
    p.setParam (params::bandId (b, "used"), 1.0f);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    MeridianAudioProcessor p;

    // ---- Response accuracy: audio measured vs. design() prediction ----
    struct Case { const char* name; Shape s; float f, g, q; int slope; double testF; double want; };
    const Case cases[] {
        { "Bell +6 @1k Q1, at 1k",              Shape::Bell,      1000, 6, 1.0f,  1, 1000,  6.0 },
        { "Bell -12 @200 Q4, at 200",           Shape::Bell,       200, -12, 4.0f, 1, 200, -12.0 },
        { "Low cut 100 Hz 24dB/oct, at 50 Hz",  Shape::LowCut,     100, 0, 0.7071f, 3, 50, -24.1 },
        { "Low cut 100 Hz 24dB/oct, at 100 Hz", Shape::LowCut,     100, 0, 0.7071f, 3, 100, -3.01 },
        { "High cut 500 Hz 48dB/oct, at 1k",    Shape::HighCut,    500, 0, 0.7071f, 6, 1000, -48.2 },
        { "High cut 500 Hz 6dB/oct, at 1k",     Shape::HighCut,    500, 0, 0.7071f, 0, 1000, -6.99 },
        { "Low cut 80 Hz Q2 (resonant), at 80", Shape::LowCut,      80, 0, 2.0f,    1, 80,   6.02 },
        { "High shelf +6 @2k 24dB/oct, at 12k", Shape::HighShelf, 2000, 6, 0.7071f, 3, 12000, 6.0 },
        { "High shelf +6 @2k 24dB/oct, at 2k",  Shape::HighShelf, 2000, 6, 0.7071f, 3, 2000, 3.0 },
        { "Low shelf -8 @150 12dB/oct, at 30",  Shape::LowShelf,   150, -8, 0.7071f, 1, 30, -8.0 },
        { "Notch @3k Q8, at 3k",                Shape::Notch,     3000, 0, 8.0f,  1, 3000, -60.0 },
        { "Tilt shelf +6 @1k, at 20 Hz",        Shape::TiltShelf, 1000, 6, 0.7071f, 0, 20, -3.0 },
        { "Flat tilt +10 pivot 1k, at 1k",      Shape::FlatTilt,  1000, 10, 1.0f, 1, 1000, 0.0 },
    };

    for (const auto& c : cases)
    {
        clearBands (p);
        setBand (p, 0, c.s, c.f, c.g, c.q, c.slope);
        const double measured = measure (p, c.testF).first;
        const double predicted = design (p.readBand (0), fs).magnitudeDb (c.testF, fs);
        const bool notch = c.s == Shape::Notch;
        check (notch ? measured < -40.0 : std::abs (measured - c.want) < 0.35, c.name, measured, c.want);
        check (notch ? predicted < -40.0 : std::abs (measured - predicted) < 0.1, "   ...matches displayed curve", measured, predicted);
    }

    // Flat tilt slope: +10 dB over 20 Hz–20 kHz should be ~1 dB/oct.
    {
        clearBands (p);
        setBand (p, 0, Shape::FlatTilt, 1000, 10, 1.0f);
        const auto s = design (p.readBand (0), fs);
        const double slope = (s.magnitudeDb (4000, fs) - s.magnitudeDb (250, fs)) / 4.0;
        check (std::abs (slope - 1.0) < 0.15, "Flat tilt slope 250 Hz-4 kHz (dB/oct)", slope, 1.0);
    }

    // ---- Stereo placement ----
    {
        clearBands (p);
        setBand (p, 0, Shape::Bell, 1000, 6, 1.0f, 1, Placement::Side);
        auto mono = measure (p, 1000, 1.0f);    // L = R: no side content
        auto side = measure (p, 1000, -1.0f);   // L = -R: pure side
        check (std::abs (mono.first) < 0.05, "Side band: mono signal untouched", mono.first, 0.0);
        check (std::abs (side.first - 6.0) < 0.1, "Side band: side signal boosted", side.first, 6.0);

        clearBands (p);
        setBand (p, 0, Shape::Bell, 1000, -9, 1.0f, 1, Placement::Mid);
        mono = measure (p, 1000, 1.0f);
        side = measure (p, 1000, -1.0f);
        check (std::abs (mono.first + 9.0) < 0.1, "Mid band: mono signal cut", mono.first, -9.0);
        check (std::abs (side.first) < 0.05, "Mid band: side signal untouched", side.first, 0.0);

        clearBands (p);
        setBand (p, 0, Shape::Bell, 1000, 6, 1.0f, 1, Placement::Right);
        auto lr = measure (p, 1000, 1.0f);
        check (std::abs (lr.first) < 0.05, "Right band: left untouched", lr.first, 0.0);
        check (std::abs (lr.second - 6.0) < 0.1, "Right band: right boosted", lr.second, 6.0);
    }

    // ---- Gain scale + auto gain ----
    {
        clearBands (p);
        setBand (p, 0, Shape::Bell, 1000, 6, 1.0f);
        p.setParam (params::gainScale, 50.0f);
        check (std::abs (measure (p, 1000).first - 3.0) < 0.1, "Gain Scale 50% halves a +6 dB bell", measure (p, 1000).first, 3.0);
        p.setParam (params::gainScale, 100.0f);

        clearBands (p);
        setBand (p, 0, Shape::Bell, 1000, 9, 0.3f);
        p.setParam (params::autoGain, 1.0f);
        juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer m;
        p.prepareToPlay (fs, block);
        buf.clear(); p.processBlock (buf, m);
        check (p.appliedAutoGainDb.load() < -3.0f, "Auto gain compensates a broad +9 dB boost", p.appliedAutoGainDb.load(), -5.0);
    }

    // ---- Stability: 24 random bands, extreme settings, automation every block ----
    {
        clearBands (p);
        juce::Random rng (7);
        for (int b = 0; b < kMaxBands; ++b)
            setBand (p, b, (Shape) rng.nextInt (kNumShapes), 10.0f * std::pow (3000.0f, rng.nextFloat()),
                     rng.nextFloat() * 60.0f - 30.0f, 0.025f * std::pow (1600.0f, rng.nextFloat()), rng.nextInt (kNumSlopes),
                     (Placement) rng.nextInt (kNumPlacements));
        p.prepareToPlay (fs, block);
        juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer m;
        bool finite = true; float peak = 0;
        for (int i = 0; i < 400; ++i)
        {
            for (int c = 0; c < 2; ++c) for (int s = 0; s < block; ++s) buf.setSample (c, s, rng.nextFloat() * 0.5f - 0.25f);
            p.setParam (params::bandId (rng.nextInt (kMaxBands), "freq"), 10.0f * std::pow (3000.0f, rng.nextFloat()));
            p.setParam (params::bandId (rng.nextInt (kMaxBands), "q"), 0.025f * std::pow (1600.0f, rng.nextFloat()));
            p.processBlock (buf, m);
            for (int c = 0; c < 2; ++c) for (int s = 0; s < block; ++s)
            {
                const float v = buf.getSample (c, s);
                finite &= std::isfinite (v);
                peak = juce::jmax (peak, std::abs (v));
            }
        }
        check (finite, "24 random bands + automation: output finite", finite ? 1 : 0, 1);
        std::printf ("     (peak under abuse: %.1f dBFS)\n", juce::Decibels::gainToDecibels (peak));
    }

    // CPU: 8 typical bands, stereo, 48 kHz
    {
        clearBands (p);
        for (int b = 0; b < 8; ++b)
            setBand (p, b, b % 2 ? Shape::Bell : Shape::HighShelf, 60.0f * std::pow (2.0f, (float) b), 3.0f, 1.0f, 3);
        p.prepareToPlay (fs, block);
        juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer m;
        buf.clear();
        const auto t0 = juce::Time::getHighResolutionTicks();
        const int blocks = (int) (fs * 10 / block);
        for (int i = 0; i < blocks; ++i) p.processBlock (buf, m);
        const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
        std::printf ("INFO 8 bands, 10 s of stereo audio processed in %.1f ms (%.2f%% of one core)\n", secs * 1000.0, secs / 10.0 * 100.0);
    }

    // ---- UI snapshot ----
    if (argc > 1)
    {
        clearBands (p);
        setBand (p, 0, Shape::LowCut,    30,   0,    0.7071f, 3);
        setBand (p, 1, Shape::Bell,      120,  2.5f, 0.9f);
        setBand (p, 2, Shape::Bell,      420, -3.5f, 2.4f);
        setBand (p, 3, Shape::Bell,      3200, -2.0f, 1.6f, 1, Placement::Side);
        setBand (p, 4, Shape::HighShelf, 8000, 3.0f, 0.7071f, 1);
        setBand (p, 5, Shape::HighCut,   18000, 0,   0.7071f, 1);
        p.prepareToPlay (fs, block);

        std::unique_ptr<MeridianAudioProcessorEditor> ed (dynamic_cast<MeridianAudioProcessorEditor*> (p.createEditor()));
        auto& disp = ed->getDisplay();
        disp.tick();
        disp.selectBand (2);

        // Pink-ish noise through the processor so the analyzer has something to show.
        juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer m;
        juce::Random rng (3);
        float b0 = 0, b1 = 0, b2 = 0;
        for (int frame = 0; frame < 90; ++frame)
        {
            for (int k = 0; k < 2; ++k)
            {
                for (int s = 0; s < block; ++s)
                {
                    const float w = rng.nextFloat() * 2.0f - 1.0f;
                    b0 = 0.99765f * b0 + w * 0.0990460f; b1 = 0.96300f * b1 + w * 0.2965164f; b2 = 0.57000f * b2 + w * 1.0526913f;
                    const float pink = (b0 + b1 + b2 + w * 0.1848f) * 0.05f;
                    buf.setSample (0, s, pink); buf.setSample (1, s, pink);
                }
                p.processBlock (buf, m);
            }
            disp.tick();
        }

        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File out (argv[1]);
        out.deleteFile();
        juce::FileOutputStream os (out);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("INFO wrote %s (%dx%d)\n", argv[1], img.getWidth(), img.getHeight());

        if (argc > 2)   // second shot: a shelf selected (slope row visible), panel flips above if needed
        {
            disp.selectBand (4);
            disp.tick();
            auto img2 = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File out2 (argv[2]);
            out2.deleteFile();
            juce::FileOutputStream os2 (out2);
            juce::PNGImageFormat().writeImageToStream (img2, os2);
        }
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
