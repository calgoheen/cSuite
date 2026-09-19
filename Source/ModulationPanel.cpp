#include "ModulationPanel.h"
#include "Session.h"
#include "SuiteLookAndFeel.h"

namespace
{

constexpr int shadowMargin = 18;
constexpr int panelWidth = 272;
constexpr int padding = 10;
constexpr int gap = 8;
constexpr int screenMargin = 6;
constexpr int anchorGap = 8;

constexpr int headerHeight = 30;
constexpr int rowHeight = 48;
constexpr int actionHeight = 28;
constexpr int separatorHeight = 1;

constexpr int iconSize = 16;
constexpr int polarityWidth = 52;
constexpr int polarityHeight = 16;
constexpr int valueWidth = 40;
constexpr int gutterWidth = 26;
constexpr int mainInset = 6;
constexpr float gutterIconBox = 12.0f;

constexpr float gutterBadgeWidth = 16.0f;
constexpr float gutterBadgeHeight = 11.0f;
constexpr float gutterBadgeGap = 3.0f;

constexpr int nameLineTop = 7;
constexpr int nameLineHeight = 18;
constexpr int depthLineTop = 27;
constexpr int depthLineHeight = 16;

constexpr float cornerSize = 6.0f;
constexpr float iconThickness = 1.4f;

constexpr float titleFontHeight = 15.0f;
constexpr float nameFontHeight = 14.0f;
constexpr float valueFontHeight = 12.0f;
constexpr float polarityFontHeight = 10.0f;
constexpr float actionFontHeight = 14.0f;
constexpr float badgeFontHeight = 9.0f;

constexpr float depthPerUnitForDepthTarget = 0.5f;

enum class Icon
{
    close,
    back,
    chevron,
    add
};

juce::Path makeIcon (Icon icon)
{
    juce::Path path;

    switch (icon)
    {
        case Icon::close:
            path.startNewSubPath (0.15f, 0.15f);
            path.lineTo (0.85f, 0.85f);
            path.startNewSubPath (0.85f, 0.15f);
            path.lineTo (0.15f, 0.85f);
            break;

        case Icon::back:
            path.startNewSubPath (0.62f, 0.12f);
            path.lineTo (0.30f, 0.50f);
            path.lineTo (0.62f, 0.88f);
            break;

        case Icon::chevron:
            path.startNewSubPath (0.38f, 0.12f);
            path.lineTo (0.70f, 0.50f);
            path.lineTo (0.38f, 0.88f);
            break;

        case Icon::add:
            path.startNewSubPath (0.50f, 0.10f);
            path.lineTo (0.50f, 0.90f);
            path.startNewSubPath (0.10f, 0.50f);
            path.lineTo (0.90f, 0.50f);
            break;
    }

    return path;
}

void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> bounds, juce::Colour colour)
{
    auto path = makeIcon (icon);
    path.applyTransform (juce::AffineTransform::scale (bounds.getWidth(), bounds.getHeight()).translated (bounds.getX(), bounds.getY()));

    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (iconThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font panelFont (const juce::Component& component, float height, bool bold = false)
{
    if (auto* laf = dynamic_cast<SuiteLookAndFeel*> (&component.getLookAndFeel()))
        return bold ? laf->getBoldFont (height) : laf->getFont (height);

    return juce::Font (juce::FontOptions (height));
}

juce::String depthText (float value) { return (value < 0.0f ? "" : "+") + juce::String (value, 2); }

} // namespace

class ModulationPanel::IconButton final : public juce::Button
{
public:
    enum class Style
    {
        plain,
        gutter
    };

    explicit IconButton (Icon i, Style s = Style::plain) : juce::Button ({}), icon (i), style (s) { setWantsKeyboardFocus (false); }

    void setBadge (int count)
    {
        if (std::exchange (badge, count) != count)
            repaint();
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto alpha = down ? 0.7f : highlighted ? 1.0f : 0.65f;
        const auto colour = findColour (textColourId);

        if (style != Style::gutter)
        {
            drawIcon (g, icon, bounds, colour.withMultipliedAlpha (alpha));
            return;
        }

        if (highlighted || down)
        {
            g.setColour (findColour (highlightColourId).withMultipliedAlpha (down ? 0.9f : 0.55f));
            g.fillRect (bounds);
        }

        const auto stacked = badge > 0;
        const auto groupHeight = stacked ? gutterBadgeHeight + gutterBadgeGap + gutterIconBox : gutterIconBox;

        auto group = bounds.withSizeKeepingCentre (bounds.getWidth(), groupHeight);

        if (stacked)
        {
            const auto box = group.removeFromTop (gutterBadgeHeight).withSizeKeepingCentre (gutterBadgeWidth, gutterBadgeHeight);

            g.setColour (findColour (accentColourId));
            g.fillRoundedRectangle (box, 3.0f);

            g.setColour (findColour (backgroundColourId));
            g.setFont (panelFont (*this, badgeFontHeight, true));
            g.drawText (juce::String (badge), box, juce::Justification::centred);

            group.removeFromTop (gutterBadgeGap);
        }

        drawIcon (g, icon, group.removeFromTop (gutterIconBox).withSizeKeepingCentre (gutterIconBox, gutterIconBox), colour.withMultipliedAlpha (alpha));
    }

private:
    const Icon icon;
    const Style style;
    int badge = 0;
};

class ModulationPanel::ActionRow final : public juce::Button
{
public:
    ActionRow (const juce::String& label, bool plus) : juce::Button (label), leadingPlus (plus) { setWantsKeyboardFocus (false); }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        if (highlighted || down)
        {
            g.setColour (findColour (highlightColourId).withMultipliedAlpha (down ? 0.9f : 0.5f));
            g.fillRect (getLocalBounds());
        }

        const auto colour = findColour (textColourId).withMultipliedAlpha (highlighted || down ? 1.0f : 0.8f);

        auto area = getLocalBounds().reduced (padding, 0);

        if (leadingPlus)
        {
            drawIcon (g, Icon::add, area.removeFromLeft (10).toFloat().withSizeKeepingCentre (9.0f, 9.0f), colour);
            area.removeFromLeft (gap);
        }

        g.setColour (colour);
        g.setFont (panelFont (*this, actionFontHeight));
        g.drawText (getButtonText(), area, juce::Justification::centredLeft, true);
    }

private:
    const bool leadingPlus;
};

class ModulationPanel::PolarityToggle final : public juce::Component
{
public:
    PolarityToggle() { setWantsKeyboardFocus (false); }

    void setBipolar (bool shouldBeBipolar)
    {
        if (bipolar == shouldBeBipolar)
            return;

        bipolar = shouldBeBipolar;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float radius = bounds.getHeight() * 0.5f;

        auto half = [&bounds] (bool right)
        { return right ? bounds.withTrimmedLeft (bounds.getWidth() * 0.5f) : bounds.withTrimmedRight (bounds.getWidth() * 0.5f); };

        g.setColour (findColour (backgroundColourId).darker (0.5f));
        g.fillRoundedRectangle (bounds, radius);

        g.setColour (findColour (accentColourId).withMultipliedAlpha (hovered ? 1.0f : 0.85f));
        g.fillRoundedRectangle (half (bipolar), radius);

        g.setFont (panelFont (*this, polarityFontHeight, true));

        for (const bool right : { false, true })
        {
            g.setColour (bipolar == right ? findColour (backgroundColourId) : findColour (dimTextColourId));
            g.drawText (right ? "Bi" : "Uni", half (right), juce::Justification::centred);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked() && onChange != nullptr)
            onChange (e.position.x > (float) getWidth() * 0.5f);
    }

    void mouseEnter (const juce::MouseEvent&) override
    {
        hovered = true;
        repaint();
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hovered = false;
        repaint();
    }

    std::function<void (bool)> onChange;

private:
    bool bipolar = false;
    bool hovered = false;
};

class ModulationPanel::ConnectionRow final : public juce::Component
{
public:
    ConnectionRow (ModulationPanel& o, const Entry& entry, bool drillable, bool separator)
      : owner (o),
        id (entry.id),
        source (entry.source),
        depthPerUnit (std::holds_alternative<cgo::ModulationGraph::DepthTarget> (entry.target) ? depthPerUnitForDepthTarget : 1.0f),
        canDrill (drillable),
        topSeparator (separator)
    {
        slider.setRange (-1.0f, 1.0f);

        slider.onValueChange = [this] (float value)
        {
            owner.session.setModulationDepth (id, value * depthPerUnit);

            if (owner.onDepthChanged != nullptr)
                owner.onDepthChanged();

            repaint();
        };

        slider.onDragStart = [this]
        {
            dragging = true;
            owner.session.beginGesture ("Modulation depth");
        };

        slider.onDragEnd = [this]
        {
            dragging = false;
            owner.session.endGesture();
        };

        polarity.onChange = [this] (bool bipolar) { owner.session.setModulationBipolar (id, bipolar); };

        remove.onClick = [this] { owner.session.removeModulation (id); };

        drill.onClick = [this]
        {
            juce::Component::SafePointer<ConnectionRow> safe (this);

            juce::MessageManager::callAsync (
                [safe]
                {
                    if (safe != nullptr)
                        safe->owner.drillInto (safe->id);
                });
        };

        addAndMakeVisible (slider);
        addAndMakeVisible (polarity);
        addAndMakeVisible (remove);

        if (canDrill)
            addAndMakeVisible (drill);

        for (auto* child : getChildren())
            child->addMouseListener (this, true);
    }

    cgo::ConnectionID getConnectionID() const { return id; }

    void update (const Entry& entry, int numDepthMods)
    {
        name = owner.session.getModulatorLabel (entry.source);

        drill.setBadge (canDrill ? numDepthMods : 0);
        polarity.setBipolar (entry.bipolar);
        syncDepth (entry.depth);

        repaint();
    }

    void syncDepth (float depth)
    {
        if (dragging || juce::approximatelyEqual (slider.getValue(), depth / depthPerUnit))
            return;

        slider.setValue (depth / depthPerUnit);
        repaint();
    }

    void resized() override
    {
        auto area = getLocalBounds();

        remove.setBounds (area.removeFromLeft (gutterWidth));

        if (canDrill)
            drill.setBounds (area.removeFromRight (gutterWidth));

        const auto main = area.reduced (mainInset, 0);

        auto nameLine = main.withTrimmedTop (nameLineTop).withHeight (nameLineHeight);
        auto depthLine = main.withTrimmedTop (depthLineTop).withHeight (depthLineHeight);

        polarity.setBounds (nameLine.removeFromRight (polarityWidth).withSizeKeepingCentre (polarityWidth, polarityHeight));
        nameArea = nameLine.withTrimmedRight (gap);

        valueArea = depthLine.removeFromRight (valueWidth);
        depthLine.removeFromRight (gap);
        slider.setBounds (depthLine);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (findColour (outlineColourId).withMultipliedAlpha (0.7f));

        if (topSeparator)
            g.fillRect (getLocalBounds().removeFromTop (separatorHeight));

        g.fillRect (getLocalBounds().withWidth (separatorHeight).withX (gutterWidth));

        if (canDrill)
            g.fillRect (getLocalBounds().withWidth (separatorHeight).withX (getWidth() - gutterWidth - separatorHeight));

        g.setColour (findColour (textColourId));
        g.setFont (panelFont (*this, nameFontHeight, true));
        g.drawText (name, nameArea, juce::Justification::centredLeft, true);

        g.setColour (findColour (dimTextColourId));
        g.setFont (panelFont (*this, valueFontHeight));
        g.drawText (depthText (slider.getValue()), valueArea, juce::Justification::centredLeft);
    }

    void mouseEnter (const juce::MouseEvent&) override { setHovered (true); }

    void mouseExit (const juce::MouseEvent&) override
    {
        juce::Component::SafePointer<ConnectionRow> safe (this);

        juce::MessageManager::callAsync (
            [safe]
            {
                if (safe != nullptr)
                    safe->setHovered (safe->isMouseOver (true));
            });
    }

private:
    void setHovered (bool nowHovered)
    {
        if (hovered == nowHovered)
            return;

        hovered = nowHovered;

        owner.rowHovered (source, hovered);
    }

    ModulationPanel& owner;

    const cgo::ConnectionID id;
    const cgo::ModulatorID source;
    const float depthPerUnit;
    const bool canDrill;
    const bool topSeparator;

    juce::String name;
    bool hovered = false;
    bool dragging = false;

    juce::Rectangle<int> nameArea;
    juce::Rectangle<int> valueArea;

    cgo::DepthSlider slider;
    PolarityToggle polarity;
    IconButton remove { Icon::close, IconButton::Style::gutter };
    IconButton drill { Icon::chevron, IconButton::Style::gutter };
};

ModulationPanel::ModulationPanel (Session& s, cgo::FrameStepper& fs) : session (s), stepper (fs)
{
    backButton = std::make_unique<IconButton> (Icon::back);
    backButton->onClick = [this] { goBack(); };
    addChildComponent (*backButton);

    closeButton = std::make_unique<IconButton> (Icon::close);
    closeButton->onClick = [this] { dismiss(); };
    addAndMakeVisible (*closeButton);

    addRow = std::make_unique<ActionRow> ("Add modulator", true);
    addRow->onClick = [this] { showSourceMenu(); };
    addAndMakeVisible (*addRow);

    clearRow = std::make_unique<ActionRow> ("Clear all modulation", false);
    clearRow->onClick = [this] { clearAll(); };
    addChildComponent (*clearRow);

    rowsViewport.setViewedComponent (&rowsHolder, false);
    rowsViewport.setScrollBarsShown (true, false, true, false);
    addAndMakeVisible (rowsViewport);

    setVisible (false);
    setWantsKeyboardFocus (false);

    stepper.add (this);
}

ModulationPanel::~ModulationPanel()
{
    stepper.remove (this);
    disarmWatcher();

    rows.clear();
}

void ModulationPanel::showFor (cgo::NodeRef node, int paramIndex, juce::Rectangle<int> anchorBounds)
{
    const auto& graph = session.getGraph();

    if (graph.findModulatedParameter (node, paramIndex) == nullptr)
        return;

    if (graph.modulation().getModulationsFor (node, paramIndex).empty() && session.getModulatorOrder().empty())
        return;

    disarmWatcher();

    target = Target { node, paramIndex };
    anchor = anchorBounds;
    positioned = false;
    drilled.reset();
    drilledSource.reset();
    hoveredSource.reset();
    shownIds.clear();
    shownDrilled.reset();
    rows.clear();

    refresh();

    setVisible (true);
    toFront (false);

    juce::Component::SafePointer<ModulationPanel> safe (this);

    juce::MessageManager::callAsync (
        [safe]
        {
            if (safe != nullptr && safe->isVisible())
                safe->armWatcher();
        });
}

void ModulationPanel::dismiss()
{
    if (! target.has_value())
        return;

    disarmWatcher();

    target.reset();
    drilled.reset();
    drilledSource.reset();
    hoveredSource.reset();
    shownIds.clear();
    shownDrilled.reset();
    rows.clear();

    setVisible (false);

    updateFocus();
}

void ModulationPanel::refresh()
{
    if (! target.has_value())
        return;

    const auto& graph = session.getGraph();

    if (graph.findModulatedParameter (target->node, target->paramIndex) == nullptr)
    {
        dismiss();
        return;
    }

    if (drilled.has_value() && ! graph.modulation().getModulation (*drilled).has_value())
    {
        drilled.reset();
        drilledSource.reset();
        shownIds.clear();
        shownDrilled.reset();
    }

    const auto entries = getEntries();

    std::vector<cgo::ConnectionID> ids;
    ids.reserve (entries.size());

    for (const auto& entry : entries)
        ids.push_back (entry.id);

    if (ids != shownIds || drilled != shownDrilled)
    {
        rebuildRows (entries);

        shownIds = std::move (ids);
        shownDrilled = drilled;
    }

    for (size_t i = 0; i < entries.size(); i++)
        rows[(int) i]->update (entries[i], getDepthModCount (entries[i]));

    updateChrome();
    updateLayout();
    updateFocus();

    repaint();
}

void ModulationPanel::paint (juce::Graphics& g)
{
    const auto content = getLocalBounds().reduced (shadowMargin);

    juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 12, { 0, 3 }).drawForRectangle (g, content);

    g.setColour (findColour (backgroundColourId));
    g.fillRoundedRectangle (content.toFloat(), cornerSize);

    g.setColour (findColour (outlineColourId));
    g.drawRoundedRectangle (content.toFloat().reduced (0.5f), cornerSize, 1.0f);

    g.setColour (findColour (outlineColourId).withMultipliedAlpha (0.8f));

    for (const int y : { headerSeparatorY, footerSeparatorY })
        if (y >= 0)
            g.fillRect (content.getX(), y, content.getWidth(), separatorHeight);

    g.setColour (findColour (textColourId));
    g.setFont (panelFont (*this, titleFontHeight, true));
    g.drawText (title, titleArea, juce::Justification::centredLeft, true);
}

void ModulationPanel::resized()
{
    auto content = getLocalBounds().reduced (shadowMargin);

    headerArea = content.removeFromTop (headerHeight);

    auto header = headerArea.reduced (padding - 2, 0);

    closeButton->setBounds (header.removeFromRight (iconSize).withSizeKeepingCentre (iconSize - 4, iconSize - 4));

    if (backButton->isVisible())
        backButton->setBounds (header.removeFromLeft (iconSize).withSizeKeepingCentre (iconSize - 4, iconSize - 4));

    titleArea = header.withTrimmedLeft (backButton->isVisible() ? gap : 2).withTrimmedRight (gap);

    headerSeparatorY = content.getY();
    content.removeFromTop (separatorHeight);

    auto footer = content.removeFromBottom (actionHeight * (clearRow->isVisible() ? 2 : 1));

    if (rows.isEmpty())
    {
        footerSeparatorY = -1;
    }
    else
    {
        footerSeparatorY = footer.getY() - separatorHeight;
        content.removeFromBottom (separatorHeight);
    }

    addRow->setBounds (footer.removeFromTop (actionHeight));

    if (clearRow->isVisible())
        clearRow->setBounds (footer.removeFromTop (actionHeight));

    rowsViewport.setBounds (content);
}

bool ModulationPanel::hitTest (int x, int y) { return getLocalBounds().reduced (shadowMargin).contains (x, y); }

juce::MouseCursor ModulationPanel::getMouseCursor()
{
    return headerArea.contains (getMouseXYRelative()) ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor;
}

void ModulationPanel::mouseDown (const juce::MouseEvent& e)
{
    dragging = headerArea.contains (e.getPosition());

    if (dragging)
        dragger.startDraggingComponent (this, e);
}

void ModulationPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;

    dragger.dragComponent (this, e, nullptr);

    contentPosition = getBounds().reduced (shadowMargin).getPosition();

    updatePosition (getHeight() - 2 * shadowMargin);
}

void ModulationPanel::mouseUp (const juce::MouseEvent&) { dragging = false; }

void ModulationPanel::step()
{
    if (! target.has_value())
        return;

    const auto& modulation = session.getGraph().modulation();

    for (auto* row : rows)
        if (const auto entry = modulation.getModulation (row->getConnectionID()))
            row->syncDepth (entry->depth);
}

std::vector<ModulationPanel::Entry> ModulationPanel::getEntries() const
{
    if (! target.has_value())
        return {};

    const auto& modulation = session.getGraph().modulation();

    if (drilled.has_value())
        return modulation.getDepthModulations (*drilled);

    return modulation.getModulationsFor (target->node, target->paramIndex);
}

int ModulationPanel::getDepthModCount (const Entry& entry) const
{
    if (std::holds_alternative<cgo::ModulationGraph::DepthTarget> (entry.target))
        return 0;

    return (int) session.getGraph().modulation().getDepthModulations (entry.id).size();
}

void ModulationPanel::drillInto (cgo::ConnectionID connection)
{
    const auto entry = session.getGraph().modulation().getModulation (connection);

    if (! entry.has_value())
        return;

    drilled = connection;
    drilledSource = entry->source;
    hoveredSource.reset();
    shownIds.clear();
    shownDrilled.reset();

    rowsViewport.setViewPosition (0, 0);

    refresh();
}

void ModulationPanel::goBack()
{
    drilled.reset();
    drilledSource.reset();
    hoveredSource.reset();
    shownIds.clear();
    shownDrilled.reset();

    rowsViewport.setViewPosition (0, 0);

    refresh();
}

void ModulationPanel::clearAll()
{
    if (! target.has_value() || drilled.has_value())
        return;

    Session::ScopedBatch batch { session, "Clear all modulation" };

    for (const auto& entry : session.getGraph().modulation().getModulationsFor (target->node, target->paramIndex))
        session.removeModulation (entry.id);
}

void ModulationPanel::showSourceMenu()
{
    if (! target.has_value())
        return;

    const auto& modulation = session.getGraph().modulation();

    juce::Component::SafePointer<ModulationPanel> safe (this);

    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());

    for (const auto source : session.getModulatorOrder())
    {
        const auto label = session.getModulatorLabel (source);

        if (const auto connection = drilled)
            menu.addItem (label,
                          modulation.canAddDepthModulation (*connection, source),
                          false,
                          [safe, connection, source]
                          {
                              if (safe != nullptr)
                                  safe->session.addDepthModulation (*connection, source);
                          });
        else
            menu.addItem (label,
                          modulation.canAddModulation (source, target->node, target->paramIndex),
                          false,
                          [safe, node = target->node, param = target->paramIndex, source]
                          {
                              if (safe != nullptr)
                                  safe->session.addModulation (source, node, param);
                          });
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (addRow.get()));
}

void ModulationPanel::rowHovered (cgo::ModulatorID source, bool isHovered)
{
    if (drilled.has_value())
        return;

    if (isHovered)
        setHoveredSource (source);
    else if (hoveredSource == source)
        setHoveredSource (std::nullopt);
}

void ModulationPanel::setHoveredSource (std::optional<cgo::ModulatorID> source)
{
    if (hoveredSource == source)
        return;

    hoveredSource = source;
    updateFocus();
}

void ModulationPanel::updateFocus()
{
    std::optional<ModFocus> wanted;

    if (target.has_value())
        if (const auto source = drilledSource.has_value() ? drilledSource : hoveredSource)
            wanted = ModFocus { source, true };

    if (focus == wanted)
        return;

    focus = wanted;

    if (onFocusChanged != nullptr)
        onFocusChanged();
}

void ModulationPanel::rebuildRows (const std::vector<Entry>& entries)
{
    rows.clear();

    for (size_t i = 0; i < entries.size(); i++)
        rowsHolder.addAndMakeVisible (rows.add (new ConnectionRow (*this, entries[i], ! drilled.has_value(), i > 0)));
}

void ModulationPanel::updateChrome()
{
    const bool inDepth = drilled.has_value();

    backButton->setVisible (inDepth);
    clearRow->setVisible (! inDepth && ! rows.isEmpty());

    addRow->setButtonText (inDepth ? "Add depth modulator" : "Add modulator");

    if (inDepth && drilledSource.has_value())
        title = session.getModulatorLabel (*drilledSource);
    else if (const auto* param = session.getGraph().findModulatedParameter (target->node, target->paramIndex))
        title = param->parameter.getName (64);
}

void ModulationPanel::updateLayout()
{
    auto* parent = getParentComponent();

    const int chrome = headerHeight + separatorHeight + (rows.isEmpty() ? 0 : separatorHeight) + actionHeight * (clearRow->isVisible() ? 2 : 1);
    const int wantedRows = rows.size() * rowHeight;

    const int available = parent != nullptr ? parent->getHeight() - 2 * screenMargin - chrome : wantedRows;
    const int visibleRows = juce::jlimit (0, wantedRows, (available / rowHeight) * rowHeight);

    const int holderWidth = panelWidth - (visibleRows < wantedRows ? rowsViewport.getScrollBarThickness() : 0);

    rowsHolder.setSize (holderWidth, wantedRows);

    for (int i = 0; i < rows.size(); i++)
        rows[i]->setBounds (0, i * rowHeight, holderWidth, rowHeight);

    updatePosition (chrome + visibleRows);
}

void ModulationPanel::updatePosition (int contentHeight)
{
    auto* parent = getParentComponent();

    if (parent == nullptr)
        return;

    if (! positioned)
    {
        contentPosition = { anchor.getRight() + anchorGap, anchor.getCentreY() - contentHeight / 2 };

        if (contentPosition.x + panelWidth > parent->getWidth() - screenMargin)
            contentPosition.x = anchor.getX() - anchorGap - panelWidth;

        positioned = true;
    }

    juce::Rectangle<int> content (contentPosition.x, contentPosition.y, panelWidth, contentHeight);

    content.setX (juce::jlimit (screenMargin, juce::jmax (screenMargin, parent->getWidth() - screenMargin - panelWidth), content.getX()));
    content.setY (juce::jlimit (screenMargin, juce::jmax (screenMargin, parent->getHeight() - screenMargin - contentHeight), content.getY()));

    setBounds (content.expanded (shadowMargin));
    resized();
}

void ModulationPanel::armWatcher()
{
    if (watching)
        return;

    juce::Desktop::getInstance().addGlobalMouseListener (&watcher);
    watching = true;
}

void ModulationPanel::disarmWatcher()
{
    if (! watching)
        return;

    juce::Desktop::getInstance().removeGlobalMouseListener (&watcher);
    watching = false;
}

void ModulationPanel::handleOutsideEvent (const juce::MouseEvent& e)
{
    if (! isVisible() || e.eventComponent == nullptr)
        return;

    if (e.eventComponent == this || isParentOf (e.eventComponent))
        return;

    auto* top = getTopLevelComponent();

    if (top != e.eventComponent && ! top->isParentOf (e.eventComponent))
        return;

    dismiss();
}
