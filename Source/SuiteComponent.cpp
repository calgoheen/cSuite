#include "SuiteComponent.h"

namespace
{

constexpr int titleBarHeight = 26;
constexpr int addBarHeight = 20;
constexpr int addBarGap = 4;
constexpr int addButtonGap = 4;
constexpr int margin = 8;
constexpr int stripGap = 8;
constexpr int titleButtonInset = 4;
constexpr int titleButtonGap = 4;

int getRowHeight (const StripComponent& strip, const juce::Viewport& viewport)
{
    return addBarHeight + addBarGap + strip.getPreferredHeight() + viewport.getScrollBarThickness();
}

template <typename Id>
std::vector<cgo::NodeRef> toNodeRefs (const std::vector<Id>& ids)
{
    return { ids.begin(), ids.end() };
}

void revealSlot (juce::Viewport& viewport, juce::Rectangle<int> bounds)
{
    const int left = viewport.getViewPositionX();
    const int width = viewport.getViewWidth();

    if (bounds.getX() < left)
        viewport.setViewPosition (bounds.getX(), 0);
    else if (bounds.getRight() > left + width)
        viewport.setViewPosition (juce::jmin (bounds.getX(), bounds.getRight() - width), 0);
}

} // namespace

SuiteComponent::AddBar::AddBar() { setInterceptsMouseClicks (false, true); }

void SuiteComponent::AddBar::resized()
{
    int x = 0;

    for (auto* button : buttons)
    {
        const int width = button->getBestWidthForHeight (getHeight());
        button->setBounds (x, 0, width, getHeight());
        x += width + addButtonGap;
    }
}

void SuiteComponent::AddBar::addButton (const juce::String& text, std::function<void()> onClick)
{
    auto* button = buttons.add (new juce::TextButton (text));
    button->setWantsKeyboardFocus (false);
    button->onClick = std::move (onClick);
    addAndMakeVisible (button);
}

SuiteComponent::SuiteComponent (Session& s) : session (s)
{
    setLookAndFeel (lookAndFeel);

    titleLabel.setText ("cSuite", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setFont (juce::Font (juce::FontOptions ((float) titleBarHeight - 10.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::backgroundColourId, findColour (titleBarColourId));
    titleLabel.setColour (juce::Label::textColourId, findColour (titleTextColourId));
    titleLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (titleLabel);

    auto addTitleButton = [this] (juce::TextButton& button, const juce::String& text, std::function<void()> onClick)
    {
        button.setButtonText (text);
        button.setWantsKeyboardFocus (false);
        button.onClick = std::move (onClick);
        addAndMakeVisible (button);
    };

    addTitleButton (undoButton, "Undo", [this] { session.getHistory().undo(); });
    addTitleButton (redoButton, "Redo", [this] { session.getHistory().redo(); });
    addTitleButton (newButton, "New", [this] { session.newSession(); });
    addTitleButton (saveButton, "Save", [this] { presets.showSaveDialog(); });
    addTitleButton (loadButton, "Load", [this] { presets.showLoadDialog(); });

    modulatorViewport.setViewedComponent (&modulatorStrip, false);
    modulatorViewport.setScrollBarsShown (false, true);
    addAndMakeVisible (modulatorViewport);

    processorViewport.setViewedComponent (&processorStrip, false);
    processorViewport.setScrollBarsShown (false, true);
    addAndMakeVisible (processorViewport);

    for (const auto& type : cgo::NodeFactory::getModulatorTypes())
        modulatorAddBar.addButton (type.displayName, [this, id = type.typeId] { addModulatorModule (cgo::NodeFactory::createModulator (id)); });

    for (const auto& type : cgo::NodeFactory::getProcessorTypes())
        processorAddBar.addButton (type.displayName, [this, id = type.typeId] { addProcessorModule (cgo::NodeFactory::createProcessor (id)); });

    addAndMakeVisible (modulatorAddBar);
    addAndMakeVisible (processorAddBar);

    processorStrip.onReorder = [this] (int from, int to) { session.moveProcessor (from, to); };
    modulatorStrip.onReorder = [this] (int from, int to) { session.moveModulator (from, to); };

    processorStrip.onPress = [this] (int index) { setSelection (processorStrip.getNode (index)); };

    processorStrip.onRightClick = [this] (int index)
    {
        setSelection (processorStrip.getNode (index));
        showNodeMenu();
    };

    modulatorStrip.onPress = [this] (int index)
    {
        const auto node = modulatorStrip.getNode (index);

        setSelection (node);
        setModFocus (std::get<cgo::ModulatorID> (node));
    };

    modulatorStrip.onRightClick = [this] (int index)
    {
        setSelection (modulatorStrip.getNode (index));
        showNodeMenu();
    };

    session.addListener (this);
    session.getHistory().addChangeListener (this);

    applyHistory();

    setWantsKeyboardFocus (true);

    setSize (designWidth,
             titleBarHeight + 2 * margin + getRowHeight (modulatorStrip, modulatorViewport) + stripGap + getRowHeight (processorStrip, processorViewport));

    syncView();
}

SuiteComponent::~SuiteComponent()
{
    session.getHistory().removeChangeListener (this);
    session.removeListener (this);
    cancelPendingUpdate();
    setLookAndFeel (nullptr);
}

void SuiteComponent::resized()
{
    auto bounds = getLocalBounds();

    const auto titleBar = bounds.removeFromTop (titleBarHeight);

    titleLabel.setBounds (titleBar);

    auto titleButtons = titleBar.reduced (margin, titleButtonInset);

    for (auto* button : { &undoButton, &redoButton })
    {
        button->setBounds (titleButtons.removeFromLeft (button->getBestWidthForHeight (titleButtons.getHeight())));
        titleButtons.removeFromLeft (titleButtonGap);
    }

    for (auto* button : { &loadButton, &saveButton, &newButton })
    {
        button->setBounds (titleButtons.removeFromRight (button->getBestWidthForHeight (titleButtons.getHeight())));
        titleButtons.removeFromRight (titleButtonGap);
    }

    auto area = bounds.reduced (margin);

    auto layoutRow = [&area] (StripComponent& strip, AddBar& bar, juce::Viewport& viewport)
    {
        auto row = area.removeFromTop (getRowHeight (strip, viewport));

        bar.setBounds (row.removeFromTop (addBarHeight));
        row.removeFromTop (addBarGap);
        viewport.setBounds (row);
    };

    layoutRow (modulatorStrip, modulatorAddBar, modulatorViewport);
    area.removeFromTop (stripGap);
    layoutRow (processorStrip, processorAddBar, processorViewport);
}

void SuiteComponent::paint (juce::Graphics& g) { g.fillAll (findColour (juce::ResizableWindow::backgroundColourId)); }

void SuiteComponent::mouseDown (const juce::MouseEvent&)
{
    grabKeyboardFocus();

    if (selection.has_value())
        setSelection (std::nullopt);
    else
        setModFocus (std::nullopt);
}

void SuiteComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto position = e.getEventRelativeTo (this).getPosition();

    for (auto* viewport : { &modulatorViewport, &processorViewport })
        if (viewport->getBounds().contains (position))
            if (viewport->useMouseWheelMoveIfNeeded (e.getEventRelativeTo (viewport), wheel))
                return;

    juce::Component::mouseWheelMove (e, wheel);
}

bool SuiteComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        setSelection (std::nullopt);
        setModFocus (std::nullopt);
        return true;
    }

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        deleteSelection();
        return true;
    }

    if (key == juce::KeyPress ('D', juce::ModifierKeys::commandModifier, 0))
    {
        duplicateSelection();
        return true;
    }

    if (key == juce::KeyPress ('Z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)
        || key == juce::KeyPress ('Y', juce::ModifierKeys::commandModifier, 0))
    {
        session.getHistory().redo();
        return true;
    }

    if (key == juce::KeyPress ('Z', juce::ModifierKeys::commandModifier, 0))
    {
        session.getHistory().undo();
        return true;
    }

    return false;
}

void SuiteComponent::dragOperationStarted (const juce::DragAndDropTarget::SourceDetails& details)
{
    const cgo::NodeRef source { cgo::ModulatorID { (juce::uint32) (juce::int64) details.description } };

    for (int i = 0; i < modulatorStrip.size(); i++)
        modulatorStrip.getModule (i).setAcceptsModulationDrops (modulatorStrip.getNode (i) != source);

    startTimerHz (30); // autoscroll
}

void SuiteComponent::dragOperationEnded (const juce::DragAndDropTarget::SourceDetails&)
{
    stopTimer();

    for (int i = 0; i < modulatorStrip.size(); i++)
        modulatorStrip.getModule (i).setAcceptsModulationDrops (true);
}

void SuiteComponent::timerCallback()
{
    const auto position = getMouseXYRelative();

    for (auto* viewport : { &modulatorViewport, &processorViewport })
        if (viewport->getBounds().contains (position))
            viewport->autoScroll (position.x - viewport->getX(), position.y - viewport->getY(), 24, 12);
}

void SuiteComponent::changeListenerCallback (juce::ChangeBroadcaster*) { applyHistory(); }

void SuiteComponent::nodeAboutToBeRemoved (cgo::NodeRef node)
{
    for (auto* strip : { &modulatorStrip, &processorStrip })
        if (const int index = strip->indexOf (node); index >= 0)
            strip->getModule (index).detach();
}

void SuiteComponent::sessionChanged() { triggerAsyncUpdate(); }

void SuiteComponent::handleAsyncUpdate() { syncView(); }

void SuiteComponent::syncView()
{
    const auto& graph = session.getGraph();

    auto makeModule = [this, &graph] (cgo::NodeRef node, const juce::String& name)
    {
        auto module =
            std::make_unique<ModuleComponent> (graph.getNode (node), name, ModuleContext { session, node, [this] { return getActiveModFocus(); }, this });

        module->onModulationDropped = [this] (cgo::ModulatorID source) { setModFocus (source); };

        return module;
    };

    processorStrip.sync (toNodeRefs (session.getProcessorOrder()),
                         [&] (cgo::NodeRef node) { return makeModule (node, session.getProcessorLabel (std::get<cgo::ProcessorID> (node))); });

    modulatorStrip.sync (toNodeRefs (session.getModulatorOrder()),
                         [&] (cgo::NodeRef node)
                         {
                             const auto id = std::get<cgo::ModulatorID> (node);

                             auto module = makeModule (node, session.getModulatorLabel (id));

                             module->onHoverChanged = [this, id] (bool isHovered)
                             {
                                 if (isHovered)
                                     setHoverFocus (id);
                                 else if (hoverFocus == id)
                                     setHoverFocus (std::nullopt);
                             };

                             return module;
                         });

    if (selection.has_value() && ! graph.contains (*selection))
        selection.reset();

    if (modFocus.has_value() && ! graph.modulators().contains (*modFocus))
        modFocus.reset();

    if (hoverFocus.has_value() && ! graph.modulators().contains (*hoverFocus))
        hoverFocus.reset();

    applySelection();
    applyLabels();
    applyModFocus();

    if (pendingReveal.has_value())
    {
        revealNode (*pendingReveal);
        pendingReveal.reset();
    }
}

void SuiteComponent::revealNode (cgo::NodeRef node)
{
    for (auto [strip, viewport] : { std::pair { &modulatorStrip, &modulatorViewport }, std::pair { &processorStrip, &processorViewport } })
        if (const int index = strip->indexOf (node); index >= 0)
            revealSlot (*viewport, strip->getSlotBounds (index));
}

void SuiteComponent::showNodeMenu()
{
    if (! selection.has_value())
        return;

    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());

    menu.addItem ("Rename", [this] { renameSelection(); });
    menu.addItem ("Duplicate", [this] { duplicateSelection(); });
    menu.addItem ("Delete", [this] { deleteSelection(); });

    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
}

bool SuiteComponent::warnIfNoRoomFor (const cgo::ParameterOwner& module)
{
    const int needed = module.getModulatedParameters().size();
    const int free = session.getNumFreeSlots();

    if (free >= needed)
        return false;

    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                            "Not enough host parameter slots",
                                            "This module needs " + juce::String (needed) + " and only " + juce::String (free)
                                                + " are free. Delete a module to make room.");

    return true;
}

void SuiteComponent::addProcessorModule (std::unique_ptr<cgo::Processor> module)
{
    if (module == nullptr || warnIfNoRoomFor (*module))
        return;

    pendingReveal = cgo::NodeRef { session.addProcessor (std::move (module)) };
}

void SuiteComponent::addModulatorModule (std::unique_ptr<cgo::Modulator> module)
{
    if (module == nullptr || warnIfNoRoomFor (*module))
        return;

    pendingReveal = cgo::NodeRef { session.addModulator (std::move (module)) };
}

void SuiteComponent::removeProcessorById (cgo::ProcessorID id) { session.removeProcessor (id); }

void SuiteComponent::removeModulatorById (cgo::ModulatorID id) { session.removeModulator (id); }

void SuiteComponent::setSelection (std::optional<cgo::NodeRef> node)
{
    grabKeyboardFocus();

    if (selection == node)
        return;

    selection = node;
    applySelection();
}

void SuiteComponent::applySelection()
{
    for (auto* strip : { &modulatorStrip, &processorStrip })
        for (int i = 0; i < strip->size(); i++)
            strip->getModule (i).setSelected (selection == strip->getNode (i));
}

void SuiteComponent::applyLabels()
{
    for (auto* strip : { &modulatorStrip, &processorStrip })
        for (int i = 0; i < strip->size(); i++)
            strip->getModule (i).refreshLabel();
}

void SuiteComponent::applyHistory()
{
    const auto& history = session.getHistory();

    undoButton.setEnabled (history.canUndo());
    redoButton.setEnabled (history.canRedo());
}

void SuiteComponent::deleteSelection()
{
    if (! selection.has_value())
        return;

    const auto node = *selection;

    std::visit (
        [this] (auto id)
        {
            if constexpr (std::is_same_v<std::decay_t<decltype (id)>, cgo::ProcessorID>)
                removeProcessorById (id);
            else
                removeModulatorById (id);
        },
        node);
}

void SuiteComponent::renameSelection()
{
    if (! selection.has_value())
        return;

    for (auto* strip : { &modulatorStrip, &processorStrip })
        if (const int index = strip->indexOf (*selection); index >= 0)
            strip->getModule (index).beginRename();
}

ModFocus SuiteComponent::getActiveModFocus() const
{
    if (modFocus.has_value())
        return { modFocus, false };

    if (hoverFocus.has_value())
        return { hoverFocus, true };

    return {};
}

void SuiteComponent::setModFocus (std::optional<cgo::ModulatorID> id)
{
    if (modFocus == id)
        return;

    modFocus = id;
    applyModFocus();
}

void SuiteComponent::setHoverFocus (std::optional<cgo::ModulatorID> id)
{
    if (id.has_value() && (modulatorStrip.isDragging() || processorStrip.isDragging() || isDragAndDropActive()))
        return;

    if (hoverFocus == id)
        return;

    const auto before = getActiveModFocus();

    hoverFocus = id;

    if (getActiveModFocus() != before)
        applyModFocus();
}

void SuiteComponent::applyModFocus()
{
    for (auto* strip : { &modulatorStrip, &processorStrip })
        for (int i = 0; i < strip->size(); i++)
            strip->getModule (i).refreshModulation();
}

void SuiteComponent::duplicateSelection()
{
    if (! selection.has_value() || warnIfNoRoomFor (session.getGraph().getNode (*selection)))
        return;

    std::visit (
        [this] (auto id)
        {
            if constexpr (std::is_same_v<std::decay_t<decltype (id)>, cgo::ProcessorID>)
            {
                const auto newId = session.duplicateProcessor (id);

                if (! newId.has_value())
                    return;

                setSelection (cgo::NodeRef { *newId });
                pendingReveal = cgo::NodeRef { *newId };
            }
            else
            {
                const auto newId = session.duplicateModulator (id);

                if (! newId.has_value())
                    return;

                setSelection (cgo::NodeRef { *newId });
                setModFocus (*newId);
                pendingReveal = cgo::NodeRef { *newId };
            }
        },
        *selection);
}
