#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FilterDesign.h"
#include "Params.h"

namespace meridian
{
    // Lock-free mono sample FIFO: audio thread pushes, the analyzer (message thread) pulls.
    class SampleFifo
    {
    public:
        SampleFifo() : fifo (1 << 15) { buffer.resize ((size_t) fifo.getTotalSize()); }

        void push (const float* l, const float* r, int n) noexcept
        {
            int s1, n1, s2, n2;
            fifo.prepareToWrite (n, s1, n1, s2, n2);
            auto write = [&] (int start, int count, int offset)
            {
                for (int i = 0; i < count; ++i)
                    buffer[(size_t) (start + i)] = r != nullptr ? 0.5f * (l[offset + i] + r[offset + i]) : l[offset + i];
            };
            write (s1, n1, 0);
            write (s2, n2, n1);
            fifo.finishedWrite (n1 + n2);
        }

        int pull (float* dest, int maxN) noexcept
        {
            int s1, n1, s2, n2;
            fifo.prepareToRead (maxN, s1, n1, s2, n2);
            std::copy_n (buffer.data() + s1, n1, dest);
            std::copy_n (buffer.data() + s2, n2, dest + n1);
            fifo.finishedRead (n1 + n2);
            return n1 + n2;
        }

    private:
        juce::AbstractFifo fifo;
        std::vector<float> buffer;
    };
}

class MeridianAudioProcessor : public juce::AudioProcessor
{
public:
    MeridianAudioProcessor();
    ~MeridianAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- shared with the editor ----
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

    meridian::SampleFifo preFifo, postFifo;
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };
    std::atomic<int>   soloBand { -1 };
    std::atomic<double> currentSampleRate { 48000.0 };
    std::atomic<float> appliedAutoGainDb { 0.0f };

    // Band settings as the host currently sees them (target values, gain-scaled).
    meridian::BandSettings readBand (int band) const;
    float readRawGain (int band) const;  // unscaled, for the panel

    // Band management (message thread).
    int  addBand (meridian::Shape shape, float freq, float gainDb, float q);
    void removeBand (int band);
    void setParam (const juce::String& id, float realValue);   // with host notification

    // A/B comparison. Slots live in the processor so they survive closing the editor.
    void switchToSlot (int slot);
    void copyCurrentToOther();
    int  getActiveSlot() const { return activeSlot; }

    // Display-only settings stored in the state tree (not automatable).
    juce::ValueTree uiState();

    // Presets
    juce::File presetDirectory() const;
    void savePreset (const juce::File&);
    bool loadPreset (const juce::File&);
    void resetToDefault();
    void stepPreset (int delta);
    juce::String getPresetName() const { return presetName; }
    std::function<void()> onPresetChanged;

private:
    struct BandDSP
    {
        meridian::BandSettings target, current;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> freq, q;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gain;
        meridian::SectionSet sections;
        double z[2][meridian::kMaxSections][2] {};   // [channel][section][z1,z2]
        bool wasActive = false;
        bool dirty = true;

        void resetState() { std::memset (z, 0, sizeof (z)); }
    };

    void updateTargets();
    void processBand (BandDSP&, float* l, float* r, int n, bool stereo);
    static void runSections (const meridian::SectionSet&, double (*z)[2], float* x, int n);
    float computeAutoGainDb() const;

    struct BandParamPtrs
    {
        std::atomic<float>* used; std::atomic<float>* on; std::atomic<float>* shape; std::atomic<float>* freq;
        std::atomic<float>* gain; std::atomic<float>* q;  std::atomic<float>* slope; std::atomic<float>* place;
    };
    std::array<BandParamPtrs, meridian::kMaxBands> bp {};
    std::atomic<float>* outGainP   = nullptr;
    std::atomic<float>* outPanP    = nullptr;
    std::atomic<float>* gainScaleP = nullptr;
    std::atomic<float>* autoGainP  = nullptr;

    std::array<BandDSP, meridian::kMaxBands> bands;
    double sr = 48000.0;

    juce::SmoothedValue<float> outGainSmoothed, autoGainSmoothed, panSmoothed;
    float lastAutoGainDb = 0.0f;
    bool  targetsChanged = true;

    // Solo: band-pass around the soloed band.
    meridian::SectionSet soloSections;
    double soloZ[2][meridian::kMaxSections][2] {};
    int lastSolo = -1;

    static constexpr int kChunk = 32;   // coefficient update interval while parameters glide

    // A/B
    juce::ValueTree slots[2];
    int activeSlot = 0;
    juce::String presetName { "Default Setting" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeridianAudioProcessor)
};
