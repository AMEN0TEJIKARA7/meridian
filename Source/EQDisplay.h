#pragma once

#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "BandPanel.h"

// The interactive frequency display: grid, spectrum analyzer, EQ curve, band nodes,
// and the band panel that floats under the selected node.
class EQDisplay : public juce::Component, private juce::Timer
{
public:
    explicit EQDisplay (MeridianAudioProcessor&);
    ~EQDisplay() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    void selectBand (int band);
    int  getSelectedBand() const { return selected; }

    // Display settings (stored in the processor's UI state).
    float rangeDb() const;
    int   analyzerMode() const;

    static constexpr float minF = 10.0f, maxF = 30000.0f;

    void tick() { timerCallback(); }   // advance one frame by hand (offline UI test)
    void setHoverForTest (int b) { hover = b; }

private:
    void timerCallback() override;

    // Mapping
    juce::Rectangle<float> plotArea() const;
    float xForFreq (float f) const;
    float freqForX (float x) const;
    float yForDb (float db) const;
    float dbForY (float y) const;
    float yForAnalyzer (float db) const;

    // Bands
    bool bandUsed (int b) const;
    juce::Point<float> nodePosition (int b) const;
    int  hitTestNode (juce::Point<float>) const;
    int  hitTestQHandle (juce::Point<float>) const;   // -1 none, 0 left, 1 right (selected band only)
    std::pair<float, float> qHandleFreqs (int b) const;
    bool hasQHandles (int b) const;
    void positionPanel();
    void deleteBand (int b);
    void showBandMenu (int b);
    void setParamValue (int b, const char* field, float v);
    juce::RangedAudioParameter* param (int b, const char* field) const;

    // Drawing helpers
    void drawGrid (juce::Graphics&, juce::Rectangle<float>);
    void drawAnalyzer (juce::Graphics&, juce::Rectangle<float>);
    void drawCurves (juce::Graphics&, juce::Rectangle<float>);
    void drawNodes (juce::Graphics&);
    void drawReadout (juce::Graphics&);

    // Analyzer
    void updateAnalyzer (meridian::SampleFifo&, std::vector<float>& history, std::vector<float>& levels);

    MeridianAudioProcessor& proc;
    BandPanel panel;

    int selected = -1, hover = -1, dragBand = -1, dragHandle = -1;
    juce::Point<float> dragStart;
    float startFreq = 0, startGain = 0, startQ = 0;
    juce::Point<float> mousePos { -1, -1 };
    bool mouseInside = false;
    int frameCounter = 0;

    static constexpr int fftOrder = 13, fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false };
    std::vector<float> fftData, pullBuffer;
    std::vector<float> preHistory, postHistory;
    std::vector<float> preLevels, postLevels;   // dB per analyzer column
    static constexpr int kColumnStep = 2;      // px per analyzer/curve column

    std::array<meridian::SectionSet, meridian::kMaxBands> designs;
    std::array<meridian::BandSettings, meridian::kMaxBands> settings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQDisplay)
};
