#pragma once

#include "PluginProcessor.h"
#include "LookAndFeel.h"
#include "EQDisplay.h"

// Caption in serif + value in mono, centred together (footer buttons).
class CaptionButton : public juce::TextButton
{
public:
    juce::String value;
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

// Toggle with a status dot (Auto Gain).
class DotToggle : public juce::TextButton
{
public:
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

// Drag-to-change number with an italic caption (Gain Scale, Output, Pan).
class DragNumber : public juce::Slider
{
public:
    explicit DragNumber (juce::String cap) : caption (std::move (cap))
    {
        setSliderStyle (juce::Slider::LinearBarVertical);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setMouseDragSensitivity (300);
        setVelocityBasedMode (false);
        setSliderSnapsToMousePosition (false);
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }
    void paint (juce::Graphics&) override;

private:
    juce::String caption;
};

class LevelMeter : public juce::Component
{
public:
    void setLevels (float l, float r);
    void paint (juce::Graphics&) override;

private:
    float levelL = 0.0f, levelR = 0.0f;
};

class MeridianAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MeridianAudioProcessorEditor (MeridianAudioProcessor&);
    ~MeridianAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    EQDisplay& getDisplay() { return display; }   // used by the offline UI test

private:
    void timerCallback() override;
    void refreshHeader();
    void showPresetMenu();
    void showSettingsMenu();
    void showModeMenu();
    void showAnalyzerMenu();
    void showRangeMenu();

    MeridianAudioProcessor& proc;
    MeridianLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    // Header
    IconButton prevPreset { "Previous preset", IconButton::Icon::Prev },
               nextPreset { "Next preset",     IconButton::Icon::Next },
               undoButton { "Undo",            IconButton::Icon::Undo, false },
               redoButton { "Redo",            IconButton::Icon::Redo, false },
               gearButton { "Settings",        IconButton::Icon::Gear, false };
    juce::TextButton presetButton, aButton { "A" }, bButton { "B" }, copyButton;
    std::unique_ptr<juce::FileChooser> chooser;

    EQDisplay display;

    // Footer
    CaptionButton modeButton, analyzerButton, rangeButton;
    DotToggle autoGainButton;
    DragNumber gainScale { "Gain Scale" }, outGain { "Output" }, outPan { "Pan" };
    LevelMeter meter;

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAtt> gainScaleAtt, outGainAtt, outPanAtt;
    std::unique_ptr<ButtonAtt> autoGainAtt;

    float meterL = 0.0f, meterR = 0.0f;
    int lastCount = -1;

    static constexpr int kHeaderH = 52, kFooterH = 64;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeridianAudioProcessorEditor)
};
