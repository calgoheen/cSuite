#include "SuiteLookAndFeel.h"
#include "ModuleComponent.h"
#include "SuiteComponent.h"

namespace palette
{

constexpr juce::uint32 background = 0xff17191c;
constexpr juce::uint32 surface = 0xff23262b;
constexpr juce::uint32 menu = 0xff1d2024;
constexpr juce::uint32 raised = 0xff2f343a;
constexpr juce::uint32 outline = 0xff383d44;
constexpr juce::uint32 text = 0xffe2e5e9;
constexpr juce::uint32 accent = 0xffe8963c;

} // namespace palette

SuiteLookAndFeel::SuiteLookAndFeel()
  : juce::LookAndFeel_V4 (juce::LookAndFeel_V4::ColourScheme { palette::background,
                                                               palette::surface,
                                                               palette::menu,
                                                               palette::outline,
                                                               palette::text,
                                                               palette::raised,
                                                               palette::background, // highlighted text, over the accent
                                                               palette::accent,
                                                               palette::text }),
    typeface (juce::Typeface::createSystemTypefaceFor (BinaryData::font_ttf, BinaryData::font_ttfSize)),
    boldTypeface (juce::Typeface::createSystemTypefaceFor (BinaryData::font_bold_ttf, BinaryData::font_bold_ttfSize))
{
    setColour (cgo::ModKnob::troughColourId, juce::Colour (palette::background));
    setColour (cgo::ModKnob::discOutlineColourId, juce::Colour (palette::text).withAlpha (0.5f));
    setColour (cgo::ModKnob::valueColourId, juce::Colour (palette::text));
    setColour (cgo::ModKnob::modulationColourId, juce::Colour (palette::accent));
    setColour (cgo::ModKnob::liveValueColourId, juce::Colour (palette::text));

    setColour (ModuleComponent::backgroundColourId, juce::Colour (palette::surface));
    setColour (ModuleComponent::outlineColourId, juce::Colour (palette::outline));
    setColour (ModuleComponent::selectedOutlineColourId, juce::Colour (palette::accent));

    setColour (SuiteComponent::titleBarColourId, juce::Colour (palette::raised));
    setColour (SuiteComponent::titleTextColourId, juce::Colour (palette::text));

    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (palette::background));
    setColour (juce::BubbleComponent::outlineColourId, juce::Colour (palette::text));
    setColour (juce::TooltipWindow::textColourId, juce::Colour (palette::text));
}

void SuiteLookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent& comp, const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    const auto bounds = body.reduced (bubbleOutlineThickness * 0.5f);

    g.setColour (comp.findColour (juce::BubbleComponent::backgroundColourId));
    g.fillRoundedRectangle (bounds, bubbleCornerSize);

    g.setColour (comp.findColour (juce::BubbleComponent::outlineColourId));
    g.drawRoundedRectangle (bounds, bubbleCornerSize, bubbleOutlineThickness);
}

void SuiteLookAndFeel::setComponentEffectForBubbleComponent (juce::BubbleComponent& bubble) { bubble.setComponentEffect (nullptr); }

int SuiteLookAndFeel::getSliderPopupPlacement (juce::Slider&) { return juce::BubbleComponent::below; }

juce::Font SuiteLookAndFeel::getLabelFont (juce::Label& label)
{
    const auto requested = label.getFont();

    return requested.isBold() ? getBoldFont (requested.getHeight()) : getFont (requested.getHeight());
}

juce::Font SuiteLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight) { return getFont (juce::jmin (16.0f, (float) buttonHeight * 0.6f)); }

juce::Font SuiteLookAndFeel::getPopupMenuFont() { return getFont (17.0f); }

juce::Font SuiteLookAndFeel::getSliderPopupFont (juce::Slider&) { return getFont (13.0f); }

juce::Font SuiteLookAndFeel::getFont (float height) const { return juce::Font (juce::FontOptions (typeface).withHeight (height)); }

juce::Font SuiteLookAndFeel::getBoldFont (float height) const { return juce::Font (juce::FontOptions (boldTypeface).withHeight (height)); }
