#pragma once

#include <JuceHeader.h>
#include <cgo_graph/cgo_graph.h>
#include "ModuleComponent.h"

class StripComponent : public juce::Component
{
public:
    using ModuleFactory = std::function<std::unique_ptr<ModuleComponent> (cgo::NodeRef)>;

    explicit StripComponent (int rowsPerModule);

    void resized() override;
    void parentSizeChanged() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

    void sync (const std::vector<cgo::NodeRef>& order, const ModuleFactory& makeModule);

    int size() const;
    int indexOf (cgo::NodeRef node) const;
    cgo::NodeRef getNode (int index) const;
    ModuleComponent& getModule (int index) const;

    bool isDragging() const;

    int getPreferredHeight() const;
    juce::Rectangle<int> getSlotBounds (int index) const;

    std::function<void (int from, int to)> onReorder; /**< A slot was dragged to a new index */
    std::function<void (int index)> onPress; /**< A slot was pressed, before any drag */
    std::function<void (int index)> onClick; /**< A slot was pressed and released without dragging */
    std::function<void (int index)> onRightClick; /**< A slot received a right click */

private:
    struct Slot
    {
        cgo::NodeRef node;
        std::unique_ptr<ModuleComponent> module;
    };

    int indexOf (const ModuleComponent* module) const;
    void moveSlot (int from, int to);
    void updateSize();
    void layoutModules (const ModuleComponent* skip);

    const int rowsPerModule;
    std::vector<Slot> slots;
    ModuleComponent* dragged = nullptr;
    int dragStartIndex = 0;
    int dragStartX = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StripComponent)
};
