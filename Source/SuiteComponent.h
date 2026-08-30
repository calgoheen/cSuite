#pragma once

#include <JuceHeader.h>
#include "Session.h"
#include "PresetBrowser.h"
#include "StripComponent.h"
#include "SuiteLookAndFeel.h"

class SuiteComponent : public juce::Component,
                       public juce::DragAndDropContainer,
                       private Session::Listener,
                       private juce::AsyncUpdater,
                       private juce::ChangeListener,
                       private juce::Timer
{
public:
    enum ColourIds
    {
        titleBarColourId = 0x1e00300,
        titleTextColourId
    };

    SuiteComponent (Session& session);
    ~SuiteComponent() override;

    void resized() override;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed (const juce::KeyPress& key) override;

    void dragOperationStarted (const juce::DragAndDropTarget::SourceDetails& details) override;
    void dragOperationEnded (const juce::DragAndDropTarget::SourceDetails& details) override;

    ModFocus getActiveModFocus() const;

private:
    class AddBar : public juce::Component
    {
    public:
        AddBar();

        void resized() override;

        void addButton (const juce::String& text, std::function<void()> onClick);

    private:
        juce::OwnedArray<juce::TextButton> buttons;
    };

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override; // the edit history changed
    void nodeAboutToBeRemoved (cgo::NodeRef node) override;
    void sessionChanged() override;
    void handleAsyncUpdate() override;

    void syncView();
    void revealNode (cgo::NodeRef node);

    void showNodeMenu();

    bool warnIfNoRoomFor (const cgo::ParameterOwner& module);

    void addProcessorModule (std::unique_ptr<cgo::Processor> module);
    void addModulatorModule (std::unique_ptr<cgo::Modulator> module);
    void removeProcessorById (cgo::ProcessorID id);
    void removeModulatorById (cgo::ModulatorID id);

    void setSelection (std::optional<cgo::NodeRef> node);
    void applySelection();
    void applyLabels();
    void applyHistory();
    void deleteSelection();
    void renameSelection();

    void setModFocus (std::optional<cgo::ModulatorID> id);
    void setHoverFocus (std::optional<cgo::ModulatorID> id);
    void applyModFocus();

    void duplicateSelection();

    juce::SharedResourcePointer<SuiteLookAndFeel> lookAndFeel;

    Session& session;
    PresetBrowser presets { session };

    juce::Label titleLabel;

    juce::TextButton undoButton;
    juce::TextButton redoButton;
    juce::TextButton newButton;
    juce::TextButton saveButton;
    juce::TextButton loadButton;

    StripComponent modulatorStrip { 2 };
    StripComponent processorStrip { 4 };
    juce::Viewport modulatorViewport;
    juce::Viewport processorViewport;

    AddBar modulatorAddBar;
    AddBar processorAddBar;

    std::optional<cgo::NodeRef> selection;
    std::optional<cgo::ModulatorID> modFocus;
    std::optional<cgo::ModulatorID> hoverFocus;
    std::optional<cgo::NodeRef> pendingReveal;

    static constexpr int designWidth = 1200;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuiteComponent)
};
