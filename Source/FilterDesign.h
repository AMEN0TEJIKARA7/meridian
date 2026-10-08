#pragma once

#include <array>
#include <complex>
#include <juce_core/juce_core.h>

// Pure filter maths, shared by the audio thread and the display so the curve you see
// is computed from exactly the coefficients that process the audio.
namespace meridian
{
    constexpr int kMaxBands    = 24;
    constexpr int kMaxSections = 12;

    enum class Shape { Bell, LowShelf, LowCut, HighShelf, HighCut, Notch, BandPass, TiltShelf, FlatTilt };
    enum class Placement { Stereo, Left, Right, Mid, Side };

    constexpr int kNumShapes     = 9;
    constexpr int kNumSlopes     = 10;
    constexpr int kNumPlacements = 5;

    // dB/oct for each slope choice. 0 = brickwall.
    constexpr std::array<int, kNumSlopes> kSlopeDb { 6, 12, 18, 24, 30, 36, 48, 72, 96, 0 };

    const juce::StringArray& shapeNames();
    const juce::StringArray& slopeNames();
    const juce::StringArray& placementNames();

    bool shapeHasGain  (Shape s);
    bool shapeHasSlope (Shape s);

    struct BandSettings
    {
        bool      used      = false;
        bool      enabled   = true;
        Shape     shape     = Shape::Bell;
        float     freq      = 1000.0f;
        float     gain      = 0.0f;   // dB (already multiplied by Gain Scale)
        float     q         = 1.0f;
        int       slope     = 1;      // index into kSlopeDb
        Placement placement = Placement::Stereo;

        bool sameFilterAs (const BandSettings& o) const noexcept
        {
            // Exact comparison on purpose: any change at all must trigger a redesign.
            return shape == o.shape && juce::exactlyEqual (freq, o.freq) && juce::exactlyEqual (gain, o.gain)
                && juce::exactlyEqual (q, o.q) && slope == o.slope;
        }
    };

    // Normalised biquad (a0 = 1). First-order sections have b2 = a2 = 0.
    struct Coeffs
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        std::complex<double> response (double w) const noexcept;  // w = 2*pi*f/fs
    };

    struct SectionSet
    {
        std::array<Coeffs, kMaxSections> s;
        int    count = 0;
        double gain  = 1.0;  // broadband linear gain applied with the sections

        double magnitudeDb (double freq, double sampleRate) const noexcept;
    };

    SectionSet design (const BandSettings& band, double sampleRate);
}
