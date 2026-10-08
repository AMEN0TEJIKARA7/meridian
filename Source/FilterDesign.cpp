#include "FilterDesign.h"

namespace meridian
{
namespace
{
    constexpr double pi = juce::MathConstants<double>::pi;

    // Analog prototype section, normalised so the corner frequency is 1 rad/s:
    //   H(s) = (B2 s^2 + B1 s + B0) / (A2 s^2 + A1 s + A0)
    struct Analog { double B2, B1, B0, A2, A1, A0; };

    // Bilinear transform with the corner prewarped to fc.
    Coeffs bilinear (const Analog& a, double fc, double fs)
    {
        const double K  = std::tan (pi * fc / fs);
        const double K2 = K * K;

        const double n0 = a.B2 + a.B1 * K + a.B0 * K2;
        const double n1 = 2.0 * (a.B0 * K2 - a.B2);
        const double n2 = a.B2 - a.B1 * K + a.B0 * K2;
        const double d0 = a.A2 + a.A1 * K + a.A0 * K2;
        const double d1 = 2.0 * (a.A0 * K2 - a.A2);
        const double d2 = a.A2 - a.A1 * K + a.A0 * K2;

        return { n0 / d0, n1 / d0, n2 / d0, d1 / d0, d2 / d0 };
    }

    // Butterworth section Qs for order n (pairs only, highest Q last).
    int butterworthQs (int n, double* qs)
    {
        const int pairs = n / 2;
        for (int k = 0; k < pairs; ++k)
        {
            const double theta = (2.0 * k + 1.0) * pi / (2.0 * n);
            qs[k] = 1.0 / (2.0 * std::cos (theta));
        }
        return pairs;
    }

    int orderForSlope (int slopeIndex)
    {
        const int db = kSlopeDb[(size_t) juce::jlimit (0, kNumSlopes - 1, slopeIndex)];
        return db == 0 ? 24 : db / 6;  // brickwall = 144 dB/oct Butterworth
    }

    // Q knob scales the most resonant section; 0.707 gives a flat Butterworth corner.
    double resonanceScale (double userQ) { return userQ * juce::MathConstants<double>::sqrt2; }

    void push (SectionSet& set, const Coeffs& c)
    {
        if (set.count < kMaxSections)
            set.s[(size_t) set.count++] = c;
    }

    void designCut (SectionSet& set, bool high, int order, double q, double fc, double fs)
    {
        double qs[16];
        const int pairs = butterworthQs (order, qs);
        if (pairs > 0)
            qs[pairs - 1] *= resonanceScale (q);

        if (order % 2 == 1)
            push (set, bilinear (high ? Analog { 0, 1, 0, 0, 1, 1 }      // s / (s + 1)
                                      : Analog { 0, 0, 1, 0, 1, 1 },     // 1 / (s + 1)
                                 fc, fs));

        for (int k = 0; k < pairs; ++k)
            push (set, bilinear (high ? Analog { 1, 0, 0, 1, 1.0 / qs[k], 1 }
                                      : Analog { 0, 0, 1, 1, 1.0 / qs[k], 1 },
                                 fc, fs));
    }

    // High-order shelf (Butterworth-type): poles on a circle of radius G^(1/2N),
    // zeros on G^(-1/2N), so the corner sits at exactly half the gain in dB.
    void designShelf (SectionSet& set, bool high, int order, double gainDb, double q, double fc, double fs)
    {
        const double G = std::pow (10.0, gainDb / 20.0);
        const double a = std::pow (G,  1.0 / (2.0 * order));   // radius that sits "outside"
        const double b = std::pow (G, -1.0 / (2.0 * order));

        // High shelf: zeros at b, poles at a, times G.  Low shelf: zeros at a, poles at b.
        const double rz = high ? b : a;
        const double rp = high ? a : b;

        double qs[16];
        const int pairs = butterworthQs (order, qs);
        if (pairs > 0)
            qs[pairs - 1] *= resonanceScale (q);

        if (order % 2 == 1)
            push (set, bilinear ({ 0, 1, rz, 0, 1, rp }, fc, fs));

        for (int k = 0; k < pairs; ++k)
            push (set, bilinear ({ 1, rz / qs[k], rz * rz, 1, rp / qs[k], rp * rp }, fc, fs));

        if (high)
            set.gain *= G;
    }
}

const juce::StringArray& shapeNames()
{
    static const juce::StringArray n { "Bell", "Low Shelf", "Low Cut", "High Shelf", "High Cut",
                                       "Notch", "Band Pass", "Tilt Shelf", "Flat Tilt" };
    return n;
}

const juce::StringArray& slopeNames()
{
    static const juce::StringArray n { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "30 dB/oct",
                                       "36 dB/oct", "48 dB/oct", "72 dB/oct", "96 dB/oct", "Brickwall" };
    return n;
}

const juce::StringArray& placementNames()
{
    static const juce::StringArray n { "Stereo", "Left", "Right", "Mid", "Side" };
    return n;
}

bool shapeHasGain (Shape s)
{
    return s == Shape::Bell || s == Shape::LowShelf || s == Shape::HighShelf
        || s == Shape::TiltShelf || s == Shape::FlatTilt;
}

bool shapeHasSlope (Shape s)
{
    return s == Shape::LowCut || s == Shape::HighCut || s == Shape::LowShelf
        || s == Shape::HighShelf || s == Shape::TiltShelf || s == Shape::BandPass;
}

std::complex<double> Coeffs::response (double w) const noexcept
{
    const std::complex<double> z1 = std::polar (1.0, -w);
    const std::complex<double> z2 = z1 * z1;
    return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
}

double SectionSet::magnitudeDb (double freq, double sampleRate) const noexcept
{
    const double w = 2.0 * pi * freq / sampleRate;
    double mag = gain;
    for (int i = 0; i < count; ++i)
        mag *= std::abs (s[(size_t) i].response (w));
    return 20.0 * std::log10 (juce::jmax (mag, 1.0e-12));
}

SectionSet design (const BandSettings& band, double fs)
{
    SectionSet set;
    if (! band.used || ! band.enabled)
        return set;

    const double nyq = 0.5 * fs;
    const double fc  = juce::jlimit (5.0, nyq * 0.995, (double) band.freq);
    const double q   = juce::jlimit (0.025, 40.0, (double) band.q);
    const double g   = band.gain;
    const int order  = orderForSlope (band.slope);

    switch (band.shape)
    {
        case Shape::Bell:
        {
            const double A = std::pow (10.0, g / 40.0);
            push (set, bilinear ({ 1, A / q, 1, 1, 1.0 / (A * q), 1 }, fc, fs));
            break;
        }

        case Shape::Notch:
            push (set, bilinear ({ 1, 0, 1, 1, 1.0 / q, 1 }, fc, fs));
            break;

        case Shape::BandPass:
        {
            const int n = juce::jlimit (1, kMaxSections, (order + 1) / 2);
            for (int i = 0; i < n; ++i)
                push (set, bilinear ({ 0, 1.0 / q, 0, 1, 1.0 / q, 1 }, fc, fs));
            break;
        }

        case Shape::LowCut:
            designCut (set, true, order, q, fc, fs);
            break;

        case Shape::HighCut:
            // A high cut at or above Nyquist does nothing; skip it rather than distort the top octave.
            if (band.freq < nyq * 0.98)
                designCut (set, false, order, q, fc, fs);
            break;

        case Shape::LowShelf:
            designShelf (set, false, juce::jmin (order, 16), g, q, fc, fs);
            break;

        case Shape::HighShelf:
            designShelf (set, true, juce::jmin (order, 16), g, q, fc, fs);
            break;

        case Shape::TiltShelf:
            // High shelf of the full gain, pulled down by half: tilts around the corner.
            designShelf (set, true, juce::jmin (order, 16), g, q, fc, fs);
            set.gain *= std::pow (10.0, -g / 40.0);
            break;

        case Shape::FlatTilt:
        {
            // Ten first-order shelves, one per octave across 20 Hz–20 kHz, each carrying a tenth of
            // the gain. Sums to an even dB/oct tilt; then pivot it to 0 dB at the band frequency.
            for (int k = 0; k < 10; ++k)
            {
                const double fk = 20.0 * std::pow (2.0, k + 0.5);
                if (fk >= nyq * 0.95)
                    break;
                const double G = std::pow (10.0, (g / 10.0) / 20.0);
                const double a = std::sqrt (G), b = 1.0 / std::sqrt (G);
                push (set, bilinear ({ 0, 1, b, 0, 1, a }, fk, fs));
                set.gain *= G;
            }
            const double atPivot = set.magnitudeDb (fc, fs);
            set.gain *= std::pow (10.0, -atPivot / 20.0);
            break;
        }
    }

    return set;
}
}
