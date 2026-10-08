#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FilterDesign.h"

// Parameter IDs and the layout. Every band always exists as parameters so hosts can automate
// any of the 24; "used" decides whether it shows up and processes.
namespace meridian::params
{
    inline juce::String bandId (int band, const char* field) { return "b" + juce::String (band + 1) + "_" + field; }

    inline const juce::String outGain   { "outGain" };
    inline const juce::String outPan    { "outPan" };
    inline const juce::String gainScale { "gainScale" };
    inline const juce::String autoGain  { "autoGain" };

    constexpr float minFreq = 10.0f,  maxFreq = 30000.0f;
    constexpr float minQ    = 0.025f, maxQ    = 40.0f;
    constexpr float maxGain = 30.0f;

    inline juce::NormalisableRange<float> logRange (float lo, float hi)
    {
        return { lo, hi,
                 [] (float a, float b, float t) { return a * std::pow (b / a, t); },
                 [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
                 [] (float a, float b, float v) { return juce::jlimit (a, b, v); } };
    }

    inline juce::String formatFreq (float hz)
    {
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz >= 10000.0f ? 2 : 3) + " kHz"
                             : juce::String (hz, 1) + " Hz";
    }

    inline juce::String formatDb (float db, int decimals = 2)
    {
        const auto s = juce::String (std::abs (db), decimals);
        if (std::abs (db) < 0.5f * std::pow (10.0f, (float) -decimals))
            return s + " dB";
        const juce::String minus (juce::CharPointer_UTF8 ("\xe2\x88\x92"));  // typographic minus
        return (db > 0.0f ? juce::String ("+") : minus) + s + " dB";
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;

        for (int b = 0; b < kMaxBands; ++b)
        {
            auto group = std::make_unique<AudioProcessorParameterGroup> ("band" + String (b + 1), "Band " + String (b + 1), " | ");
            const String n = "Band " + String (b + 1) + " ";

            group->addChild (std::make_unique<AudioParameterBool> (ParameterID { bandId (b, "used"), 1 }, n + "Used", false));
            group->addChild (std::make_unique<AudioParameterBool> (ParameterID { bandId (b, "on"), 1 }, n + "Enabled", true));
            group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { bandId (b, "shape"), 1 }, n + "Shape", shapeNames(), 0));
            group->addChild (std::make_unique<AudioParameterFloat> (ParameterID { bandId (b, "freq"), 1 }, n + "Frequency",
                                 logRange (minFreq, maxFreq), 1000.0f,
                                 AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return formatFreq (v); })));
            group->addChild (std::make_unique<AudioParameterFloat> (ParameterID { bandId (b, "gain"), 1 }, n + "Gain",
                                 NormalisableRange<float> (-maxGain, maxGain, 0.01f), 0.0f,
                                 AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return formatDb (v); })));
            group->addChild (std::make_unique<AudioParameterFloat> (ParameterID { bandId (b, "q"), 1 }, n + "Q",
                                 logRange (minQ, maxQ), 1.0f,
                                 AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 3); })));
            group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { bandId (b, "slope"), 1 }, n + "Slope", slopeNames(), 1));
            group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { bandId (b, "place"), 1 }, n + "Stereo Placement", placementNames(), 0));

            layout.add (std::move (group));
        }

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { outGain, 1 }, "Output Gain",
                        NormalisableRange<float> (-36.0f, 36.0f, 0.01f), 0.0f,
                        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return formatDb (v); })));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { outPan, 1 }, "Output Pan",
                        NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f,
                        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                        {
                            if (std::abs (v) < 0.005f) return String ("C");
                            return String (std::abs (v) * 100.0f, 0) + (v < 0 ? " L" : " R");
                        })));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { gainScale, 1 }, "Gain Scale",
                        NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f,
                        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 0) + "%"; })));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { autoGain, 1 }, "Auto Gain", false));

        return layout;
    }
}
