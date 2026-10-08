#pragma once

#include "PluginProcessor.h"
#include "LookAndFeel.h"

// Small icon-only button: the icon is a path drawn in the button's current colour.
class IconButton : public juce::Button
{
public:
    enum class Icon { Power, Solo, Close, Undo, Redo, Prev, Next, Gear };

    IconButton (const juce::String& name, Icon i, bool bordered = true)
        : juce::Button (name), icon (i), border (bordered)
    {
        setTooltip (name);
        setTitle (name);
    }

    juce::Colour iconColour = theme::dim;
    juce::Colour activeColour = theme::text;
    bool active = false;

    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    Icon icon;
    bool border;
};

// A TextButton that draws a filter-shape icon to the left of its label.
class ShapeButton : public juce::TextButton
{
public:
    int shape = 0;
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

class BandPanel : public juce::Component
{
public:
    explicit BandPanel (MeridianAudioProcessor&);
    ~BandPanel() override;

    void setBand (int band);
    int  getBand() const { return band; }
    void refresh();   // pull current shape/slope/state from the parameters

    std::function<void (int band)> onDeleteBand;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kWidth = 564, kHeight = 126;

private:
    void showShapeMenu();
    void showSlopeMenu();
    void showPlacementMenu();
    float param (const char* field) const;

    MeridianAudioProcessor& proc;
    int band = -1;

    ShapeButton shapeButton;
    juce::TextButton slopeButton, placementButton;
    juce::Slider freqKnob, gainKnob, qKnob;
    IconButton bypassButton { "Bypass band", IconButton::Icon::Power },
               soloButton   { "Solo band",   IconButton::Icon::Solo },
               deleteButton { "Delete band", IconButton::Icon::Close };

    using Att = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Att> freqAtt, gainAtt, qAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandPanel)
};
