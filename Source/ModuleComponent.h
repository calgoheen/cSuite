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
    std::function<void (cgo::NodeRef node, int paramIndex, juce::Component& knob)> showModulationPanel;
    juce::Component* popupParent = nullptr;
    cgo::FrameStepper* stepper = nullptr;
};

class ModuleComponent : public juce::Component, private cgo::FrameStepper::Listener
{
public:
    enum ColourIds
    {
        backgroundColourId = 0x1e00200,
        outlineColourId,
        selectedOutlineColourId,
        activeColourId,
        bypassedColourId
    };

    static int getHeightForRows (int rows);

    ModuleComponent (cgo::ParameterOwner& owner, juce::String displayName, ModuleContext context);
    ~ModuleComponent() override;

    void resized() override;
    void paint (juce::Graphics& g) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    void setRowCount (int rows);

    void setSelected (bool shouldBeSelected);
    void beginRename();
    void refreshLabel();
    void refreshBypass();
    void refreshModulation();
    void setAcceptsModulationDrops (bool shouldAccept);
    void detach();
    bool isDetached() const;

    std::function<void (bool)> onHoverChanged;
    std::function<void (cgo::ModulatorID source)> onModulationDropped;

private:
    struct ParamControl
    {
        cgo::ModulatedParameter* parameter = nullptr;
        int parameterIndex = 0;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<cgo::ModKnob> knob;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
        std::optional<cgo::ConnectionID> activeConnection;
        std::shared_ptr<const cgo::ModulatedValue> activeDepth;
    };

    void step() override;
    void updateLayout();
    juce::Image createDragImage();
    void setHovered (bool nowHovered);
    void commitTitle();
    float getLiveValue (const cgo::ModulatedParameter& param) const;
    void handleDrop (int paramIndex, const juce::var& payload);

    ModuleContext context;

    juce::Label titleLabel;
    std::unique_ptr<juce::Component> grabTab; // modulators only
    std::unique_ptr<juce::Button> bypassButton; // processors only
    const juce::AudioProcessorParameter* bypassParameter = nullptr;
    std::vector<ParamControl> controls;
    int rows = 1;
    int columns = 1;
    bool selected = false;
    bool bypassed = false;
    bool hovered = false;
    bool detached = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleComponent)
};
