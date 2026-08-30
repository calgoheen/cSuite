#include "ModuleComponent.h"
#include "Session.h"
#include "SuiteLookAndFeel.h"

namespace
{

constexpr int cellWidth = 64;
constexpr int knobHeight = 48;
constexpr int labelHeight = 16;
constexpr float labelFontHeight = 13.0f;
constexpr int titleHeight = 22;
constexpr int padding = 8;
constexpr int minTitleWidth = 64;
constexpr int tabInset = 4;
constexpr float badgeSize = 9.0f;
constexpr int repaintHz = 30;

constexpr float arrowThickness = 1.5f;
constexpr float arrowHeadWidth = 5.0f;
constexpr float arrowHeadLength = 4.0f;

constexpr int dragChipPadding = 6;
constexpr int dragChipGap = 16;
constexpr double dragImageScale = 2.0;

constexpr int cellHeight = labelHeight + knobHeight;

} // namespace

ModuleComponent::GrabTab::GrabTab (ModuleComponent& o) : owner (o) {}

void ModuleComponent::GrabTab::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();

    juce::Path arrows;

    for (const auto& end : { juce::Point<float> (bounds.getX(), centre.y),
                             juce::Point<float> (bounds.getRight(), centre.y),
                             juce::Point<float> (centre.x, bounds.getY()),
                             juce::Point<float> (centre.x, bounds.getBottom()) })
        arrows.addArrow ({ centre, end }, arrowThickness, arrowHeadWidth, arrowHeadLength);

    g.setColour (findColour (cgo::ModKnob::modulationColourId));
    g.fillPath (arrows);
}

void ModuleComponent::GrabTab::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;

    auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);

    if (container == nullptr || container->isDragAndDropActive())
        return;

    const juce::ScaledImage image (owner.createDragImage(), dragImageScale);
    const auto bounds = image.getScaledBounds();

    const juce::Point<int> offsetFromMouse { -(int) bounds.getWidth() / 2, 0 };

    container->startDragging (dragDescription, this, image, false, &offsetFromMouse);
}

int ModuleComponent::getHeightForRows (int rows) { return titleHeight + padding + juce::jmax (1, rows) * (cellHeight + padding); }

ModuleComponent::ModuleComponent (cgo::ParameterOwner& owner, juce::String displayName, ModuleContext ctx) : context (std::move (ctx))
{
    titleLabel.setText (displayName, juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setFont (juce::Font (juce::FontOptions ((float) titleHeight - 6.0f, juce::Font::bold)));

    titleLabel.setInterceptsMouseClicks (false, true);
    titleLabel.onTextChange = [this] { commitTitle(); };

    addAndMakeVisible (titleLabel);

    if (const auto* id = std::get_if<cgo::ModulatorID> (&context.node))
    {
        grabTab = std::make_unique<GrabTab> (*this);
        grabTab->dragDescription = juce::var ((juce::int64) id->uid);
        grabTab->setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        addAndMakeVisible (*grabTab);
    }

    const auto& params = owner.getModulatedParameters();

    for (int i = 0; i < params.size(); i++)
    {
        auto* param = params[i];
        ParamControl control;

        control.label = std::make_unique<juce::Label> (juce::String(), param->parameter.getName (64));
        control.label->setJustificationType (juce::Justification::centred);
        control.label->setFont (juce::Font (juce::FontOptions (labelFontHeight)));
        addAndMakeVisible (*control.label);

        control.parameter = param;
        control.knob = std::make_unique<cgo::ModKnob>();
        control.knob->setPopupDisplayEnabled (true, false, context.popupParent);

        control.knob->onDragStart = [this, i]
        {
            if (const auto* dragged = controls[(size_t) i].parameter)
                context.session.beginGesture (dragged->parameter.getName (64));
        };

        control.knob->onDragEnd = [this] { context.session.endGesture(); };

        control.knob->onDepthChanged = [this, i] (float depth)
        {
            const auto connection = controls[(size_t) i].activeConnection;

            if (connection.has_value())
                context.session.setModulationDepth (*connection, depth);
        };

        control.knob->onDepthGestureStart = [this] { context.session.beginGesture ("Modulation depth"); };
        control.knob->onDepthGestureEnd = [this] { context.session.endGesture(); };

        control.knob->onDrop = [this, i] (const juce::var& payload) { handleDrop (i, payload); };
        control.knob->onRightClick = [this, i] { showModulationMenu (i); };

        addAndMakeVisible (*control.knob);

        control.attachment = std::make_unique<juce::SliderParameterAttachment> (param->parameter, *control.knob, nullptr);

        controls.push_back (std::move (control));
    }

    updateLayout();

    for (auto* child : getChildren())
        child->addMouseListener (this, true);

    refreshModulation();

    startTimerHz (repaintHz);
}

void ModuleComponent::resized()
{
    auto area = getLocalBounds().reduced (padding, 0);

    titleLabel.setBounds (area.removeFromTop (titleHeight));

    if (grabTab != nullptr)
    {
        const int size = titleHeight - 2 * tabInset;
        grabTab->setBounds (getWidth() - tabInset - size, tabInset, size, size);
    }

    const int gridWidth = columns * (cellWidth + padding) - padding;
    const int left = (getWidth() - gridWidth) / 2;

    for (int i = 0; i < (int) controls.size(); i++)
    {
        // Modules fill downwards and spill into the next column
        const int x = left + (i / rows) * (cellWidth + padding);
        const int y = titleHeight + padding + (i % rows) * (cellHeight + padding);

        controls[(size_t) i].label->setBounds (x, y, cellWidth, labelHeight);
        controls[(size_t) i].knob->setBounds (x, y + labelHeight, cellWidth, knobHeight);
    }
}

void ModuleComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().reduced (1).toFloat();

    g.setColour (findColour (backgroundColourId));
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setColour (findColour (selected ? selectedOutlineColourId : outlineColourId));
    g.drawRoundedRectangle (bounds, 6.0f, selected ? 2.0f : 1.0f);

    if (const auto* id = std::get_if<cgo::ModulatorID> (&context.node))
    {
        const auto focus = context.getModFocus != nullptr ? context.getModFocus() : ModFocus {};

        if (focus.source == *id)
        {
            g.setColour (findColour (cgo::ModKnob::modulationColourId).withAlpha (focus.isPreview ? cgo::ModKnob::unfocusedAlpha : 1.0f));
            g.fillEllipse ((float) padding, ((float) titleHeight - badgeSize) * 0.5f, badgeSize, badgeSize);
        }
    }
}

void ModuleComponent::mouseEnter (const juce::MouseEvent&) { setHovered (true); }

void ModuleComponent::mouseExit (const juce::MouseEvent&)
{
    juce::Component::SafePointer<ModuleComponent> safe (this);
    juce::MessageManager::callAsync (
        [safe]
        {
            if (safe != nullptr)
                safe->setHovered (safe->isMouseOver (true));
        });
}

void ModuleComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.eventComponent != this || ! titleLabel.getBounds().contains (e.getPosition()))
        return;

    beginRename();
}

void ModuleComponent::setRowCount (int newRows)
{
    newRows = juce::jmax (1, newRows);

    if (newRows == rows)
        return;

    rows = newRows;
    updateLayout();
}

void ModuleComponent::setSelected (bool shouldBeSelected)
{
    if (selected == shouldBeSelected)
        return;

    selected = shouldBeSelected;
    repaint();
}

void ModuleComponent::beginRename() { titleLabel.showEditor(); }

void ModuleComponent::refreshLabel()
{
    if (detached || titleLabel.isBeingEdited())
        return;

    const auto label = std::visit (
        [this] (auto id) -> juce::String
        {
            if constexpr (std::is_same_v<std::decay_t<decltype (id)>, cgo::ProcessorID>)
                return context.session.getProcessorLabel (id);
            else
                return context.session.getModulatorLabel (id);
        },
        context.node);

    titleLabel.setText (label, juce::dontSendNotification);
}

void ModuleComponent::refreshModulation()
{
    if (detached)
        return;

    const auto focus = context.getModFocus != nullptr ? context.getModFocus() : ModFocus {};
    const auto& modulation = context.session.getGraph().modulation();

    for (int i = 0; i < (int) controls.size(); i++)
    {
        auto& control = controls[(size_t) i];

        const auto modulations = modulation.getModulationsFor (context.node, i);

        const auto focused =
            std::find_if (modulations.begin(), modulations.end(), [&focus] (const auto& e) { return focus.source.has_value() && e.source == *focus.source; });

        std::optional<cgo::ModKnob::Ring> ring;

        control.activeConnection.reset();

        if (focused != modulations.end())
        {
            ring = cgo::ModKnob::Ring { focused->depth, focused->bipolar, focus.isPreview };
            control.activeConnection = focused->id;
        }

        control.knob->setModulation (ring, ! ring.has_value() && ! modulations.empty());
        control.knob->setLiveValue (getLiveValue (*control.parameter));
    }

    repaint();
}

void ModuleComponent::setAcceptsModulationDrops (bool shouldAccept)
{
    for (auto& control : controls)
        control.knob->setAcceptsDrops (shouldAccept);
}

void ModuleComponent::detach()
{
    if (detached)
        return;

    detached = true;

    stopTimer();

    for (auto& control : controls)
    {
        control.attachment.reset();
        control.parameter = nullptr;
    }

    setInterceptsMouseClicks (false, false);
}

bool ModuleComponent::isDetached() const { return detached; }

void ModuleComponent::timerCallback()
{
    for (auto& control : controls)
        control.knob->setLiveValue (getLiveValue (*control.parameter));
}

void ModuleComponent::updateLayout()
{
    const int n = (int) controls.size();
    columns = juce::jmax (1, (n + rows - 1) / rows);

    const int knobWidth = padding + columns * (cellWidth + padding);
    const int headerWidth = 2 * padding + minTitleWidth + (grabTab != nullptr ? titleHeight : 0);

    setSize (juce::jmax (knobWidth, headerWidth), getHeightForRows (rows));
}

juce::Image ModuleComponent::createDragImage()
{
    const auto text = titleLabel.getText();
    const auto font = getLookAndFeel().getLabelFont (titleLabel);

    const int width = juce::GlyphArrangement::getStringWidthInt (font, text) + 2 * dragChipPadding;
    const int height = dragChipGap + titleHeight;

    juce::Image image (juce::Image::ARGB, (int) (width * dragImageScale), (int) (height * dragImageScale), true);

    juce::Graphics g (image);
    g.addTransform (juce::AffineTransform::scale ((float) dragImageScale));

    const auto bounds =
        juce::Rectangle<float> (0.0f, (float) dragChipGap, (float) width, (float) titleHeight).reduced (SuiteLookAndFeel::bubbleOutlineThickness * 0.5f);

    g.setColour (findColour (juce::BubbleComponent::backgroundColourId));
    g.fillRoundedRectangle (bounds, SuiteLookAndFeel::bubbleCornerSize);

    g.setColour (findColour (juce::BubbleComponent::outlineColourId));
    g.drawRoundedRectangle (bounds, SuiteLookAndFeel::bubbleCornerSize, SuiteLookAndFeel::bubbleOutlineThickness);

    g.setColour (findColour (juce::TooltipWindow::textColourId));
    g.setFont (font);
    g.drawText (text, bounds, juce::Justification::centred);

    return image;
}

void ModuleComponent::setHovered (bool nowHovered)
{
    if (hovered == nowHovered)
        return;

    hovered = nowHovered;

    if (onHoverChanged != nullptr)
        onHoverChanged (hovered);
}

void ModuleComponent::commitTitle()
{
    const auto edited = titleLabel.getText().trim();

    if (edited.isNotEmpty())
        std::visit (
            [this, &edited] (auto id)
            {
                if constexpr (std::is_same_v<std::decay_t<decltype (id)>, cgo::ProcessorID>)
                    context.session.setProcessorLabel (id, edited);
                else
                    context.session.setModulatorLabel (id, edited);
            },
            context.node);

    refreshLabel();
}

void ModuleComponent::handleDrop (int paramIndex, const juce::var& payload)
{
    if (! payload.isInt64() && ! payload.isInt())
        return;

    const cgo::ModulatorID source { (juce::uint32) (juce::int64) payload };

    if (! context.session.getGraph().modulators().contains (source))
        return;

    context.session.addModulation (source, context.node, paramIndex);

    if (onModulationDropped != nullptr)
        onModulationDropped (source);

    refreshModulation();
}

void ModuleComponent::showModulationMenu (int paramIndex)
{
    const auto& modulation = context.session.getGraph().modulation();

    const auto entries = modulation.getModulationsFor (context.node, paramIndex);

    if (entries.empty())
        return;

    juce::Component::SafePointer<ModuleComponent> safe (this);

    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());

    std::vector<cgo::ConnectionID> all;
    all.reserve (entries.size());

    for (const auto& entry : entries)
    {
        const auto id = entry.id;
        const bool bipolar = entry.bipolar;

        juce::PopupMenu source;

        source.addItem ("Delete",
                        [safe, id]
                        {
                            if (safe != nullptr)
                                safe->context.session.removeModulation (id);
                        });
        source.addItem ("Bipolar",
                        true,
                        bipolar,
                        [safe, id, bipolar]
                        {
                            if (safe != nullptr)
                                safe->context.session.setModulationBipolar (id, ! bipolar);
                        });

        menu.addSubMenu (context.session.getModulatorLabel (entry.source), source);

        all.push_back (id);
    }

    menu.addSeparator();

    menu.addItem ("Clear all modulation",
                  [safe, all]
                  {
                      if (safe == nullptr)
                          return;

                      Session::ScopedBatch batch { safe->context.session, "Clear all modulation" };

                      for (const auto id : all)
                          safe->context.session.removeModulation (id);
                  });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (*controls[(size_t) paramIndex].knob));
}

float ModuleComponent::getLiveValue (const cgo::ModulatedParameter& param) const { return param.range.convertTo0to1 (param.getCurrentValue()); }
