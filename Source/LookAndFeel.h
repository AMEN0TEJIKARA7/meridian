#pragma once

#include "Theme.h"

class MeridianLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MeridianLookAndFeel();

    // Property keys a button can set to change how it's drawn.
    static constexpr const char* kFilled   = "mFilled";    // raised background
    static constexpr const char* kMono     = "mMono";      // monospace label
    static constexpr const char* kBorder   = "mBorder";    // false = borderless icon button
    static constexpr const char* kSegment  = "mSegment";   // part of a segmented group (square corners)

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                            const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;
    int getPopupMenuBorderSize() override { return 4; }

    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;
};

// Shape icon used in menus, the band panel and the spec: drawn, not bitmap.
juce::Path shapeIcon (int shapeIndex, juce::Rectangle<float> area);
