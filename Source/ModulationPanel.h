#pragma once

#include <JuceHeader.h>
#include <cgo_graph/cgo_graph.h>
#include <cgo_gui/cgo_gui.h>

#include "ModuleComponent.h"

class Session;

class ModulationPanel : public juce::Component, private cgo::FrameStepper::Listener
{
public:
    enum ColourIds
    {
        backgroundColourId = 0x1e00400,
        outlineColourId,
        textColourId,
        dimTextColourId,
        highlightColourId,
        accentColourId
    };

    ModulationPanel (Session& session, cgo::FrameStepper& stepper);
    ~ModulationPanel() override;

    void showFor (cgo::NodeRef node, int paramIndex, juce::Rectangle<int> anchor);
    void dismiss();
    void refresh();

    std::optional<ModFocus> getFocus() const { return focus; }

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    juce::MouseCursor getMouseCursor() override;

    std::function<void()> onFocusChanged; /**< The panel wants a different connection lit up */
    std::function<void()> onDepthChanged;

private:
    class IconButton;
    class ActionRow;
    class PolarityToggle;
    class ConnectionRow;

    using Entry = cgo::ModulationGraph::ModulationEntry;

    struct Target
    {
        cgo::NodeRef node;
        int paramIndex = 0;
    };

    struct OutsideWatcher : public juce::MouseListener
    {
        explicit OutsideWatcher (ModulationPanel& o) : owner (o) {}

        void mouseDown (const juce::MouseEvent& e) override { owner.handleOutsideEvent (e); }

        ModulationPanel& owner;
    };

    void step() override;

    std::vector<Entry> getEntries() const;
    int getDepthModCount (const Entry& entry) const;

    void drillInto (cgo::ConnectionID connection);
    void goBack();
    void clearAll();
    void showSourceMenu();

    void rowHovered (cgo::ModulatorID source, bool isHovered);
    void setHoveredSource (std::optional<cgo::ModulatorID> source);
    void updateFocus();

    void rebuildRows (const std::vector<Entry>& entries);
    void updateChrome();
    void updateLayout();
    void updatePosition (int contentHeight);

    void armWatcher();
    void disarmWatcher();
    void handleOutsideEvent (const juce::MouseEvent& e);

    Session& session;
    cgo::FrameStepper& stepper;

    OutsideWatcher watcher { *this };
    bool watching = false;

    std::optional<Target> target;
    std::optional<cgo::ConnectionID> drilled;
    std::optional<cgo::ModulatorID> drilledSource;
    std::optional<cgo::ModulatorID> hoveredSource;
    std::optional<ModFocus> focus;

    juce::Rectangle<int> anchor;
    juce::Point<int> contentPosition;
    bool positioned = false;
    juce::String title;

    juce::ComponentDragger dragger;
    bool dragging = false;

    juce::Rectangle<int> headerArea;
    juce::Rectangle<int> titleArea;
    int headerSeparatorY = -1;
    int footerSeparatorY = -1;

    std::unique_ptr<IconButton> backButton;
    std::unique_ptr<IconButton> closeButton;
    std::unique_ptr<ActionRow> addRow;
    std::unique_ptr<ActionRow> clearRow;

    juce::Component rowsHolder;
    juce::Viewport rowsViewport;
    juce::OwnedArray<ConnectionRow> rows;
    std::vector<cgo::ConnectionID> shownIds;
    std::optional<cgo::ConnectionID> shownDrilled;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulationPanel)
};
