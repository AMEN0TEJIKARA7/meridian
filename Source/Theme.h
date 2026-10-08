#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "MeridianFonts.h"

namespace theme
{
    // Ground layers
    const juce::Colour display  { 0xff020202 };
    const juce::Colour chrome   { 0xff050505 };
    const juce::Colour panel    { 0xff0b0b0b };
    const juce::Colour raised   { 0xff141414 };
    const juce::Colour knobCap  { 0xff161616 };
    const juce::Colour border   { 0xff1e1e1e };
    const juce::Colour border2  { 0xff262626 };
    const juce::Colour grid     { 0xff121212 };
    const juce::Colour zeroLine { 0xff262626 };
    const juce::Colour track    { 0xff222222 };

    // Text
    const juce::Colour text     { 0xffece6da };
    const juce::Colour textSoft { 0xffa8a296 };
    const juce::Colour dim      { 0xff8a847a };
    const juce::Colour faint    { 0xff7e786e };

    const juce::Colour accent   { 0xffc9a86a };

    inline juce::Colour bandColour (int band)
    {
        static const juce::Colour c[] { juce::Colour (0xff7fa7c9), juce::Colour (0xffc9a86a), juce::Colour (0xffd98b6c),
                                        juce::Colour (0xff8db89a), juce::Colour (0xffa795c9), juce::Colour (0xffc97f98) };
        return c[band % 6];
    }

    // ---- Fonts (bundled, so they look the same on every machine) ----
    struct Faces
    {
        juce::Typeface::Ptr mono, monoMedium, serif, serifItalic, display;

        static Faces& get()
        {
            static Faces f;
            return f;
        }

    private:
        Faces()
        {
            using namespace MeridianFonts;
            mono        = juce::Typeface::createSystemTypefaceFor (IBMPlexMonoRegular_ttf,      (size_t) IBMPlexMonoRegular_ttfSize);
            monoMedium  = juce::Typeface::createSystemTypefaceFor (IBMPlexMonoMedium_ttf,       (size_t) IBMPlexMonoMedium_ttfSize);
            serif       = juce::Typeface::createSystemTypefaceFor (SourceSerif4Semibold_ttf,    (size_t) SourceSerif4Semibold_ttfSize);
            serifItalic = juce::Typeface::createSystemTypefaceFor (SourceSerif4SemiboldIt_ttf,  (size_t) SourceSerif4SemiboldIt_ttfSize);
            display     = juce::Typeface::createSystemTypefaceFor (CormorantGaramondBold_ttf,   (size_t) CormorantGaramondBold_ttfSize);
        }
    };

    inline juce::Font mono (float h)        { return juce::Font (juce::FontOptions (Faces::get().mono).withHeight (h)); }
    inline juce::Font monoMedium (float h)  { return juce::Font (juce::FontOptions (Faces::get().monoMedium).withHeight (h)); }
    inline juce::Font serif (float h)       { return juce::Font (juce::FontOptions (Faces::get().serif).withHeight (h)); }
    inline juce::Font serifItalic (float h) { return juce::Font (juce::FontOptions (Faces::get().serifItalic).withHeight (h)); }
    inline juce::Font wordmark (float h)    { return juce::Font (juce::FontOptions (Faces::get().display).withHeight (h)); }
}
