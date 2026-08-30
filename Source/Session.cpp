#include "Session.h"

namespace
{

namespace suiteIds
{

const juce::Identifier SUITE { "SUITE" };
const juce::Identifier version { "version" };

} // namespace suiteIds

namespace nodeIds
{

const juce::Identifier NODE { "NODE" };
const juce::Identifier SLOTS { "SLOTS" };
const juce::Identifier S { "S" };
const juce::Identifier kind { "kind" };
const juce::Identifier id { "id" };
const juce::Identifier type { "type" };
const juce::Identifier label { "label" };
const juce::Identifier index { "index" };
const juce::Identifier param { "param" };
const juce::Identifier p { "p" };

} // namespace nodeIds

constexpr int schemaVersion = 1;
constexpr int sidechainChannels = 2;

cgo::NodeRef nodeRefOf (const juce::ValueTree& tree)
{
    const auto uid = (juce::uint32) (int) tree[nodeIds::id];

    if (tree[nodeIds::kind].toString() == cgo::NodeKind::processor)
        return cgo::NodeRef { cgo::ProcessorID { uid } };

    return cgo::NodeRef { cgo::ModulatorID { uid } };
}

const juce::ValueTree* findNode (const std::vector<juce::ValueTree>& nodes, const juce::ValueTree& node)
{
    for (const auto& tree : nodes)
        if (nodeRefOf (tree) == nodeRefOf (node) && tree[nodeIds::type] == node[nodeIds::type])
            return &tree;

    return nullptr;
}

} // namespace

Session::ScopedBatch::ScopedBatch (Session& s, const juce::String& transactionName) : session (s), modulationBatch (std::in_place, s.graph.modulation())
{
    session.history.beginTransaction (transactionName);
}

Session::ScopedBatch::~ScopedBatch()
{
    modulationBatch.reset();

    session.history.endTransaction();
    session.flushNotifications();
}

Session::Session (juce::AudioProcessor& host) : hostProcessor (host), graph (false, sidechainChannels), parameterManager (host), history (*this)
{
    updatePassThrough();
}

void Session::prepareToPlay (double sampleRate, int samplesPerBlock) { graph.prepareToPlay (sampleRate, samplesPerBlock); }

void Session::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) { graph.processBlock (buffer, midi); }

void Session::setPlayHead (juce::AudioPlayHead* newPlayHead) { graph.setPlayHead (newPlayHead); }

void Session::addListener (Listener* listener) { listeners.add (listener); }

void Session::removeListener (Listener* listener) { listeners.remove (listener); }

int Session::getNumFreeSlots() const noexcept { return parameterManager.getNumFreeSlots(); }

void Session::newSession() { replaceSession (juce::ValueTree { suiteIds::SUITE }, "New session"); }

bool Session::saveToFile (const juce::File& file)
{
    file.getParentDirectory().createDirectory();

    return toValueTree().createXml()->writeTo (file);
}

bool Session::loadFromFile (const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse (file);

    return xml != nullptr && replaceSession (juce::ValueTree::fromXml (*xml), "Load preset");
}

juce::ValueTree Session::toValueTree() const
{
    juce::ValueTree state { suiteIds::SUITE };

    state.setProperty (suiteIds::version, schemaVersion, nullptr);
    state.appendChild (graph.toValueTree(), nullptr);
    state.appendChild (parameterManager.toValueTree (graph), nullptr);
    state.appendChild (presentation.toValueTree(), nullptr);

    return state;
}

bool Session::restoreFromValueTree (const juce::ValueTree& state)
{
    if (! state.hasType (suiteIds::SUITE) || (int) state[suiteIds::version] > schemaVersion)
        return false;

    const SessionHistory::ScopedRestore restoreGuard { history };

    ScopedBatch batch { *this };

    clearSession();

    graph.processors().disconnectInputFromOutput();

    graph.restoreFromValueTree (state);
    parameterManager.restoreFromValueTree (state, graph);
    presentation.restoreFromValueTree (state, graph);

    updatePassThrough();

    return true;
}

cgo::ProcessorID Session::addProcessor (std::unique_ptr<cgo::Processor> processor)
{
    ScopedBatch batch { *this };

    const auto typeId = processor->getTypeId();

    auto order = getProcessorOrder();

    const auto id = graph.addProcessor (std::move (processor));

    parameterManager.attach (graph.processors().getProcessor (id));
    presentation.addProcessor (id, typeId);

    order.push_back (id);
    rebuildChain (order);

    history.nameTransaction ("Add " + presentation.getProcessorLabel (id));

    notifySessionChanged();

    return id;
}

cgo::ModulatorID Session::addModulator (std::unique_ptr<cgo::Modulator> modulator)
{
    ScopedBatch batch { *this };

    const auto typeId = modulator->getTypeId();
    const auto id = graph.addModulator (std::move (modulator));

    parameterManager.attach (graph.modulators().getModulator (id));
    presentation.addModulator (id, typeId);

    history.nameTransaction ("Add " + presentation.getModulatorLabel (id));

    notifySessionChanged();

    return id;
}

void Session::removeProcessor (cgo::ProcessorID id)
{
    ScopedBatch batch { *this, "Delete " + presentation.getProcessorLabel (id) };

    listeners.call (&Listener::nodeAboutToBeRemoved, cgo::NodeRef { id });

    auto order = getProcessorOrder();
    order.erase (std::remove (order.begin(), order.end(), id), order.end());

    parameterManager.detach (graph.processors().getProcessor (id));
    graph.removeProcessor (id);
    presentation.removeProcessor (id);

    rebuildChain (order);

    notifySessionChanged();
}

void Session::removeModulator (cgo::ModulatorID id)
{
    ScopedBatch batch { *this, "Delete " + presentation.getModulatorLabel (id) };

    listeners.call (&Listener::nodeAboutToBeRemoved, cgo::NodeRef { id });

    parameterManager.detach (graph.modulators().getModulator (id));
    graph.removeModulator (id);
    presentation.removeModulator (id);

    notifySessionChanged();
}

std::optional<cgo::ProcessorID> Session::duplicateProcessor (cgo::ProcessorID id)
{
    if (! graph.processors().contains (id))
        return std::nullopt;

    const auto original = cgo::NodeRef { id };
    const int numParams = graph.processors().getProcessor (id).getModulatedParameters().size();

    auto tree = nodeToValueTree (original);

    tree.setProperty (nodeIds::index, (int) tree[nodeIds::index] + 1, nullptr);

    ScopedBatch batch { *this, "Duplicate " + presentation.getProcessorLabel (id) };

    const auto clone = insertNodeFromValueTree (tree, false);

    if (! clone.has_value())
        return std::nullopt;

    copyModulations (original, *clone, numParams);

    return std::get<cgo::ProcessorID> (*clone);
}

std::optional<cgo::ModulatorID> Session::duplicateModulator (cgo::ModulatorID id)
{
    if (! graph.modulators().contains (id))
        return std::nullopt;

    const auto original = cgo::NodeRef { id };
    const int numParams = graph.modulators().getModulator (id).getModulatedParameters().size();

    auto tree = nodeToValueTree (original);

    tree.setProperty (nodeIds::index, (int) tree[nodeIds::index] + 1, nullptr);

    ScopedBatch batch { *this, "Duplicate " + presentation.getModulatorLabel (id) };

    const auto clone = insertNodeFromValueTree (tree, false);

    if (! clone.has_value())
        return std::nullopt;

    copyModulations (original, *clone, numParams);

    return std::get<cgo::ModulatorID> (*clone);
}

void Session::beginGesture (const juce::String& name) { history.beginGesture (name); }

void Session::endGesture()
{
    if (history.endGesture())
        notifyStateDirty();
}

std::optional<cgo::ConnectionID> Session::addModulation (cgo::ModulatorID source, cgo::NodeRef targetNode, int targetParam, float depth, bool bipolar)
{
    ScopedBatch batch { *this, "Modulate " + presentation.getModulatorLabel (source) };

    const auto id = graph.modulation().addModulation (source, targetNode, targetParam, depth, bipolar);

    if (id.has_value())
        notifySessionChanged();

    return id;
}

std::optional<cgo::ConnectionID> Session::addDepthModulation (cgo::ConnectionID connection, cgo::ModulatorID source, float depth, bool bipolar)
{
    ScopedBatch batch { *this, "Modulate " + presentation.getModulatorLabel (source) };

    const auto id = graph.modulation().addDepthModulation (connection, source, depth, bipolar);

    if (id.has_value())
        notifySessionChanged();

    return id;
}

void Session::removeModulation (cgo::ConnectionID id)
{
    ScopedBatch batch { *this, "Remove modulation" };

    graph.modulation().removeModulation (id);

    notifySessionChanged();
}

void Session::setModulationBipolar (cgo::ConnectionID id, bool bipolar)
{
    const auto connection = graph.modulation().getModulation (id);

    if (! connection.has_value() || connection->bipolar == bipolar)
        return;

    ScopedBatch batch { *this, "Modulation polarity" };

    graph.modulation().setModulationBipolar (id, bipolar);

    notifySessionChanged();
}

void Session::setModulationDepth (cgo::ConnectionID id, float depth) { graph.modulation().setModulationDepth (id, depth); }

juce::String Session::getProcessorLabel (cgo::ProcessorID id) const { return presentation.getProcessorLabel (id); }

juce::String Session::getModulatorLabel (cgo::ModulatorID id) const { return presentation.getModulatorLabel (id); }

void Session::setProcessorLabel (cgo::ProcessorID id, const juce::String& label)
{
    const auto previous = presentation.getProcessorLabel (id);

    if (previous == label)
        return;

    ScopedBatch batch { *this, "Rename " + label };

    presentation.setProcessorLabel (id, label);

    notifyStateDirty();
}

void Session::setModulatorLabel (cgo::ModulatorID id, const juce::String& label)
{
    const auto previous = presentation.getModulatorLabel (id);

    if (previous == label)
        return;

    ScopedBatch batch { *this, "Rename " + label };

    presentation.setModulatorLabel (id, label);

    notifyStateDirty();
}

std::vector<cgo::ProcessorID> Session::getProcessorOrder() const
{
    const auto connections = graph.processors().getConnections();

    auto successorOf = [&connections] (std::optional<cgo::ProcessorID> node) -> std::optional<cgo::ProcessorID>
    {
        for (const auto& c : connections)
            if (c.source == node && c.destination.has_value())
                return c.destination;

        return std::nullopt;
    };

    std::vector<cgo::ProcessorID> order;

    for (auto node = successorOf (std::nullopt); node.has_value(); node = successorOf (node))
        order.push_back (*node);

    return order;
}

void Session::moveProcessor (int from, int to)
{
    auto order = getProcessorOrder();

    if (from == to || ! juce::isPositiveAndBelow (from, (int) order.size()))
        return;

    ScopedBatch batch { *this, "Move" };

    const auto id = order[(size_t) from];
    order.erase (order.begin() + from);
    order.insert (order.begin() + juce::jlimit (0, (int) order.size(), to), id);

    rebuildChain (order);

    notifySessionChanged();
}

const std::vector<cgo::ModulatorID>& Session::getModulatorOrder() const { return presentation.getModulatorOrder(); }

void Session::moveModulator (int from, int to)
{
    ScopedBatch batch { *this, "Move" };

    if (! presentation.moveModulator (from, to))
        return;

    notifySessionChanged();
}

SessionHistory::Snapshot Session::captureSession() const
{
    SessionHistory::Snapshot snapshot;

    for (const auto& entry : graph.modulators().getAll())
        snapshot.nodes.push_back (nodeToValueTree (cgo::NodeRef { entry.id }));

    for (const auto& entry : graph.processors().getAll())
        snapshot.nodes.push_back (nodeToValueTree (cgo::NodeRef { entry.id }));

    const auto keyOf = [] (const juce::ValueTree& tree) { return std::pair { tree[nodeIds::kind].toString(), (int) tree[nodeIds::id] }; };

    std::sort (snapshot.nodes.begin(), snapshot.nodes.end(), [&keyOf] (const juce::ValueTree& a, const juce::ValueTree& b) { return keyOf (a) < keyOf (b); });

    snapshot.modulations = graph.modulation().getModulations();

    return snapshot;
}

void Session::applySession (const SessionHistory::Snapshot& from, const SessionHistory::Snapshot& to)
{
    ScopedBatch batch { *this };

    const auto current = captureSession();

    for (const auto& tree : current.nodes)
        if (findNode (to.nodes, tree) == nullptr)
            removeNode (nodeRefOf (tree));

    for (const auto& tree : to.nodes)
        if (findNode (current.nodes, tree) == nullptr)
            insertNodeFromValueTree (tree, true);

    for (const auto& tree : to.nodes)
    {
        const auto node = nodeRefOf (tree);

        if (findNode (current.nodes, tree) == nullptr || ! graph.contains (node))
            continue;

        setNodeLabel (node, tree[nodeIds::label].toString());
        applyParameters (node, findNode (from.nodes, tree), tree);
    }

    applyOrder (to.nodes);
    applyModulations (to.modulations);

    notifySessionChanged();
}

void Session::applyParameters (cgo::NodeRef node, const juce::ValueTree* from, const juce::ValueTree& to)
{
    const auto& owner = graph.getNode (node);

    const auto target = owner.savedParameterValues (to);
    const auto previous = from != nullptr ? owner.savedParameterValues (*from) : std::vector<std::optional<float>> {};

    for (size_t i = 0; i < target.size(); i++)
    {
        if (! target[i].has_value() || (i < previous.size() && previous[i] == target[i]))
            continue;

        setParameterValue (node, (int) i, *target[i]);
    }
}

void Session::applyOrder (const std::vector<juce::ValueTree>& nodes)
{
    std::vector<std::pair<int, cgo::ProcessorID>> processors;
    std::vector<std::pair<int, cgo::ModulatorID>> modulators;

    for (const auto& tree : nodes)
    {
        const auto node = nodeRefOf (tree);

        if (! graph.contains (node))
            continue;

        const int index = tree[nodeIds::index];

        if (const auto* id = std::get_if<cgo::ProcessorID> (&node))
            processors.push_back ({ index, *id });
        else
            modulators.push_back ({ index, std::get<cgo::ModulatorID> (node) });
    }

    const auto byIndex = [] (const auto& a, const auto& b) { return a.first < b.first; };

    std::sort (processors.begin(), processors.end(), byIndex);
    std::sort (modulators.begin(), modulators.end(), byIndex);

    std::vector<cgo::ProcessorID> chain;

    for (const auto& processor : processors)
        chain.push_back (processor.second);

    if (chain != getProcessorOrder())
        rebuildChain (chain);

    for (int position = 0; position < (int) modulators.size(); position++)
    {
        const auto& order = presentation.getModulatorOrder();
        const auto found = std::find (order.begin(), order.end(), modulators[(size_t) position].second);

        if (found != order.end())
            presentation.moveModulator ((int) std::distance (order.begin(), found), position);
    }
}

void Session::applyModulations (const std::vector<cgo::ModulationGraph::ModulationEntry>& target)
{
    auto& modulation = graph.modulation();

    const auto isWanted = [&target] (cgo::ConnectionID id)
    { return std::any_of (target.begin(), target.end(), [id] (const auto& entry) { return entry.id == id; }); };

    for (const auto& entry : modulation.getModulations())
        if (! isWanted (entry.id) && modulation.getModulation (entry.id).has_value())
            modulation.removeModulation (entry.id);

    modulation.restoreModulations (target);

    for (const auto& entry : target)
    {
        const auto connection = modulation.getModulation (entry.id);

        if (! connection.has_value())
            continue;

        if (! juce::exactlyEqual (connection->depth, entry.depth))
            modulation.setModulationDepth (entry.id, entry.depth);

        if (connection->bipolar != entry.bipolar)
            modulation.setModulationBipolar (entry.id, entry.bipolar);
    }
}

void Session::removeNode (cgo::NodeRef node)
{
    std::visit (
        [this] (auto id)
        {
            if constexpr (std::is_same_v<std::decay_t<decltype (id)>, cgo::ProcessorID>)
                removeProcessor (id);
            else
                removeModulator (id);
        },
        node);
}

void Session::setNodeLabel (cgo::NodeRef node, const juce::String& label)
{
    std::visit (
        [this, &label] (auto id)
        {
            if constexpr (std::is_same_v<std::decay_t<decltype (id)>, cgo::ProcessorID>)
                setProcessorLabel (id, label);
            else
                setModulatorLabel (id, label);
        },
        node);
}

void Session::setParameterValue (cgo::NodeRef node, int paramIndex, float normalised)
{
    if (auto* param = graph.findModulatedParameter (node, paramIndex))
        param->parameter.setValueNotifyingHost (normalised);
}

void Session::notifySessionChanged()
{
    sessionChangePending = true;
    flushNotifications();
}

void Session::notifyStateDirty()
{
    stateDirtyPending = true;
    flushNotifications();
}

void Session::flushNotifications()
{
    if (history.isTransactionOpen())
        return;

    const bool sessionChanged = sessionChangePending;
    const bool stateDirty = stateDirtyPending;

    sessionChangePending = false;
    stateDirtyPending = false;

    if (! history.isRestoring() && (sessionChanged || stateDirty))
        hostProcessor.updateHostDisplay (juce::AudioProcessorListener::ChangeDetails {}
                                             .withLatencyChanged (sessionChanged)
                                             .withParameterInfoChanged (sessionChanged)
                                             .withNonParameterStateChanged (true));

    if (sessionChanged)
        listeners.call (&Listener::sessionChanged);
}

void Session::copyModulations (cgo::NodeRef from, cgo::NodeRef to, int numParams)
{
    for (int i = 0; i < numParams; i++)
        for (const auto& e : graph.modulation().getModulationsFor (from, i))
            addModulation (e.source, to, i, e.depth, e.bipolar);
}

void Session::rebuildChain (const std::vector<cgo::ProcessorID>& order)
{
    graph.processors().clearConnections();

    if (! order.empty())
    {
        graph.processors().connectToInput (order.front());

        for (size_t i = 1; i < order.size(); i++)
            graph.processors().connect (order[i - 1], order[i]);

        graph.processors().connectToOutput (order.back());
    }

    updatePassThrough();
}

void Session::clearSession()
{
    ScopedBatch batch { *this };

    for (const auto& entry : graph.processors().getAll())
        removeProcessor (entry.id);

    for (const auto& entry : graph.modulators().getAll())
        removeModulator (entry.id);

    parameterManager.reset();
    presentation.clear();

    updatePassThrough();
}

void Session::updatePassThrough()
{
    if (graph.processors().getAll().empty())
        graph.processors().connectInputToOutput();
    else
        graph.processors().disconnectInputFromOutput();
}

juce::ValueTree Session::nodeToValueTree (cgo::NodeRef node) const
{
    const auto& owner = graph.getNode (node);

    juce::ValueTree tree { nodeIds::NODE };

    std::visit (
        [this, &tree] (auto id)
        {
            if constexpr (std::is_same_v<std::decay_t<decltype (id)>, cgo::ProcessorID>)
            {
                const auto order = getProcessorOrder();

                tree.setProperty (nodeIds::kind, cgo::NodeKind::processor, nullptr);
                tree.setProperty (nodeIds::type, graph.processors().getProcessor (id).getTypeId(), nullptr);
                tree.setProperty (nodeIds::label, presentation.getProcessorLabel (id), nullptr);
                tree.setProperty (nodeIds::index, (int) std::distance (order.begin(), std::find (order.begin(), order.end(), id)), nullptr);
            }
            else
            {
                const auto& order = presentation.getModulatorOrder();

                tree.setProperty (nodeIds::kind, cgo::NodeKind::modulator, nullptr);
                tree.setProperty (nodeIds::type, graph.modulators().getModulator (id).getTypeId(), nullptr);
                tree.setProperty (nodeIds::label, presentation.getModulatorLabel (id), nullptr);
                tree.setProperty (nodeIds::index, (int) std::distance (order.begin(), std::find (order.begin(), order.end(), id)), nullptr);
            }

            tree.setProperty (nodeIds::id, (int) id.uid, nullptr);
        },
        node);

    tree.appendChild (owner.parametersToValueTree(), nullptr);

    juce::ValueTree routing { nodeIds::SLOTS };

    const auto slots = parameterManager.getSlots (owner);
    const auto& params = owner.getModulatedParameters();

    for (int i = 0; i < juce::jmin ((int) slots.size(), params.size()); i++)
    {
        if (slots[(size_t) i] < 0)
            continue;

        juce::ValueTree slot { nodeIds::S };

        slot.setProperty (nodeIds::param, params[i]->parameter.getParameterID(), nullptr);
        slot.setProperty (nodeIds::p, slots[(size_t) i], nullptr);

        routing.appendChild (slot, nullptr);
    }

    tree.appendChild (routing, nullptr);

    return tree;
}

std::optional<cgo::NodeRef> Session::insertNodeFromValueTree (const juce::ValueTree& tree, bool keepIdentity)
{
    const auto typeId = tree[nodeIds::type].toString();
    const auto label = tree[nodeIds::label].toString();
    const auto uid = (juce::uint32) (int) tree[nodeIds::id];
    const int index = tree[nodeIds::index];
    const bool isProcessor = tree[nodeIds::kind].toString() == cgo::NodeKind::processor;

    auto processor = isProcessor ? cgo::NodeFactory::createProcessor (typeId) : nullptr;
    auto modulator = isProcessor ? nullptr : cgo::NodeFactory::createModulator (typeId);

    auto* made = isProcessor ? static_cast<cgo::ParameterOwner*> (processor.get()) : modulator.get();

    if (made == nullptr)
    {
        jassertfalse;
        return std::nullopt;
    }

    if (! keepIdentity && parameterManager.getNumFreeSlots() < made->getModulatedParameters().size())
        return std::nullopt;

    made->restoreParameters (tree);

    ScopedBatch batch { *this };

    const auto node = [&]
    {
        if (isProcessor)
        {
            const auto id = graph.addProcessor (std::move (processor), keepIdentity ? std::optional { cgo::ProcessorID { uid } } : std::nullopt);

            presentation.addProcessor (id, typeId);
            presentation.setProcessorLabel (id, label);

            auto order = getProcessorOrder();
            order.insert (order.begin() + juce::jlimit (0, (int) order.size(), index), id);

            rebuildChain (order);

            return cgo::NodeRef { id };
        }

        const auto id = graph.addModulator (std::move (modulator), keepIdentity ? std::optional { cgo::ModulatorID { uid } } : std::nullopt);

        presentation.addModulator (id, typeId);
        presentation.setModulatorLabel (id, label);
        presentation.moveModulator ((int) presentation.getModulatorOrder().size() - 1, index);

        return cgo::NodeRef { id };
    }();

    auto& owner = graph.getNode (node);

    if (keepIdentity)
    {
        for (const auto slot : tree.getChildWithName (nodeIds::SLOTS))
            parameterManager.restoreLink (slot[nodeIds::p], owner, slot[nodeIds::param].toString());
    }
    else
    {
        parameterManager.attach (owner);
    }

    notifySessionChanged();

    return node;
}

bool Session::replaceSession (const juce::ValueTree& state, const juce::String& name)
{
    ScopedBatch batch { *this, name };

    if (! restoreFromValueTree (state))
        return false;

    notifySessionChanged();

    return true;
}
