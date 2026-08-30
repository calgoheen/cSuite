#pragma once

#include <JuceHeader.h>
#include <cgo_graph/cgo_graph.h>
#include <cgo_gui/cgo_gui.h>

class Session;

struct ModFocus
{
    std::optional<cgo::ModulatorID> source;
    bool isPreview = false;

    bool operator== (const ModFocus& other) const { return source == other.source && isPreview == other.isPreview; }
    bool operator!= (const ModFocus& other) const { return ! operator== (other); }
};

struct ModuleContext
{
    Session& session;
    cgo::NodeRef node;
    std::function<ModFocus()> getModFocus;
    juce::Component* popupParent = nullptr;
};

class ModuleComponent : public juce::Component, private juce::Timer
{
public:
    enum ColourIds
    {
        backgroundColourId = 0x1e00200,
        outlineColourId,
        selectedOutlineColourId
    };

    static int getHeightForRows (int rows);

    ModuleComponent (cgo::ParameterOwner& owner, juce::String displayName, ModuleContext context);

    void resized() override;
    void paint (juce::Graphics& g) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    void setRowCount (int rows);

    void setSelected (bool shouldBeSelected);
    void beginRename();
    void refreshLabel();
    void refreshModulation();
    void setAcceptsModulationDrops (bool shouldAccept);
    void detach();
    bool isDetached() const;

    std::function<void (bool)> onHoverChanged;
    std::function<void (cgo::ModulatorID source)> onModulationDropped;

private:
    class GrabTab : public juce::Component
    {
    public:
        explicit GrabTab (ModuleComponent& owner);

        void paint (juce::Graphics& g) override;
        void mouseDrag (const juce::MouseEvent& e) override;

        juce::var dragDescription;

    private:
        ModuleComponent& owner;
    };

    struct ParamControl
    {
        cgo::ModulatedParameter* parameter = nullptr;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<cgo::ModKnob> knob;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
        std::optional<cgo::ConnectionID> activeConnection;
    };

    void timerCallback() override;
    void updateLayout();
    juce::Image createDragImage();
    void setHovered (bool nowHovered);
    void commitTitle();
    float getLiveValue (const cgo::ModulatedParameter& param) const;
    void handleDrop (int paramIndex, const juce::var& payload);
    void showModulationMenu (int paramIndex);

    ModuleContext context;

    juce::Label titleLabel;
    std::unique_ptr<GrabTab> grabTab; // modulators only
    std::vector<ParamControl> controls;
    int rows = 1;
    int columns = 1;
    bool selected = false;
    bool hovered = false;
    bool detached = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleComponent)
};
