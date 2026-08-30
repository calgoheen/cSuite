#pragma once

#include <JuceHeader.h>

class SuiteLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static constexpr float bubbleCornerSize = 4.0f;
    static constexpr float bubbleOutlineThickness = 1.0f;

    SuiteLookAndFeel();

    void drawBubble (juce::Graphics& g, juce::BubbleComponent& comp, const juce::Point<float>& tip, const juce::Rectangle<float>& body) override;
    void setComponentEffectForBubbleComponent (juce::BubbleComponent& bubble) override;
    int getSliderPopupPlacement (juce::Slider& slider) override;

    juce::Font getLabelFont (juce::Label& label) override;
    juce::Font getTextButtonFont (juce::TextButton& button, int buttonHeight) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getSliderPopupFont (juce::Slider& slider) override;

    juce::Font getFont (float height) const;
    juce::Font getBoldFont (float height) const;

private:
    juce::Typeface::Ptr typeface;
    juce::Typeface::Ptr boldTypeface;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuiteLookAndFeel)
};
