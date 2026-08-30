#include "StripComponent.h"

namespace
{

constexpr int gap = 8;

} // namespace

StripComponent::StripComponent (int rows) : rowsPerModule (rows) { setInterceptsMouseClicks (false, true); }

void StripComponent::resized() { layoutModules (nullptr); }

void StripComponent::parentSizeChanged() { updateSize(); }

void StripComponent::mouseDown (const juce::MouseEvent& e)
{
    auto* m = dynamic_cast<ModuleComponent*> (e.eventComponent);
    if (m == nullptr)
        return;

    if (e.mods.isPopupMenu())
    {
        if (onRightClick != nullptr)
            onRightClick (indexOf (m));

        return;
    }

    dragged = m;
    dragStartIndex = indexOf (m);
    dragStartX = m->getX();
    m->toFront (false);

    if (onPress != nullptr)
        onPress (dragStartIndex);
}

void StripComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragged == nullptr)
        return;

    dragged->setTopLeftPosition (dragStartX + e.getDistanceFromDragStartX(), gap);

    const int centre = dragged->getBounds().getCentreX();
    int target = 0;
    for (const auto& slot : slots)
        if (slot.module.get() != dragged && slot.module->getBounds().getCentreX() < centre)
            ++target;

    const int current = indexOf (dragged);
    if (target != current)
    {
        moveSlot (current, target);
        layoutModules (dragged);
    }
}

void StripComponent::mouseUp (const juce::MouseEvent&)
{
    if (dragged == nullptr)
        return;

    const int finalIndex = indexOf (dragged);
    resized(); // snap the dragged slot to its resting position

    if (finalIndex != dragStartIndex)
    {
        if (onReorder != nullptr)
            onReorder (dragStartIndex, finalIndex);
    }
    else if (onClick != nullptr)
    {
        onClick (finalIndex);
    }

    dragged = nullptr;
}

void StripComponent::sync (const std::vector<cgo::NodeRef>& order, const ModuleFactory& makeModule)
{
    for (int i = (int) slots.size(); --i >= 0;)
    {
        const auto& slot = slots[(size_t) i];

        if (slot.module->isDetached() || std::find (order.begin(), order.end(), slot.node) == order.end())
        {
            if (slot.module.get() == dragged)
                dragged = nullptr;

            slots.erase (slots.begin() + i);
        }
    }

    for (int target = 0; target < (int) order.size(); target++)
    {
        const auto node = order[(size_t) target];
        const int current = indexOf (node);

        if (current < 0)
        {
            auto module = makeModule (node);

            module->setRowCount (rowsPerModule);
            module->addMouseListener (this, false);
            addAndMakeVisible (*module);

            slots.insert (slots.begin() + target, Slot { node, std::move (module) });
        }
        else if (current != target)
        {
            moveSlot (current, target);
        }
    }

    updateSize();
}

int StripComponent::size() const { return (int) slots.size(); }

int StripComponent::indexOf (cgo::NodeRef node) const
{
    for (int i = 0; i < (int) slots.size(); i++)
        if (slots[(size_t) i].node == node)
            return i;

    return -1;
}

cgo::NodeRef StripComponent::getNode (int index) const
{
    jassert (juce::isPositiveAndBelow (index, (int) slots.size()));
    return slots[(size_t) index].node;
}

ModuleComponent& StripComponent::getModule (int index) const
{
    jassert (juce::isPositiveAndBelow (index, (int) slots.size()));
    return *slots[(size_t) index].module;
}

bool StripComponent::isDragging() const { return dragged != nullptr; }

int StripComponent::getPreferredHeight() const { return ModuleComponent::getHeightForRows (rowsPerModule) + 2 * gap; }

juce::Rectangle<int> StripComponent::getSlotBounds (int index) const
{
    if (! juce::isPositiveAndBelow (index, (int) slots.size()))
        return {};

    return slots[(size_t) index].module->getBounds().expanded (gap, 0);
}

int StripComponent::indexOf (const ModuleComponent* module) const
{
    for (int i = 0; i < (int) slots.size(); i++)
        if (slots[(size_t) i].module.get() == module)
            return i;

    return -1;
}

void StripComponent::moveSlot (int from, int to)
{
    auto slot = std::move (slots[(size_t) from]);

    slots.erase (slots.begin() + from);
    slots.insert (slots.begin() + to, std::move (slot));
}

void StripComponent::updateSize()
{
    int width = gap;

    for (const auto& slot : slots)
        width += slot.module->getWidth() + gap;

    setSize (juce::jmax (width, getParentWidth()), getPreferredHeight());
    layoutModules (nullptr);
}

void StripComponent::layoutModules (const ModuleComponent* skip)
{
    int x = gap;

    for (const auto& slot : slots)
    {
        if (slot.module.get() != skip)
            slot.module->setTopLeftPosition (x, gap);

        x += slot.module->getWidth() + gap;
    }
}
