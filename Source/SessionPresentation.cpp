#include "SessionPresentation.h"

namespace
{

namespace viewIds
{

const juce::Identifier VIEW { "VIEW" };
const juce::Identifier PROC { "PROC" };
const juce::Identifier MOD { "MOD" };
const juce::Identifier N { "N" };
const juce::Identifier id { "id" };
const juce::Identifier label { "label" };

} // namespace viewIds

template <typename Labels>
juce::String lookUpLabel (const Labels& labels, typename Labels::key_type id)
{
    const auto entry = labels.find (id);
    return entry != labels.end() ? entry->second : juce::String();
}

} // namespace

void SessionPresentation::addProcessor (cgo::ProcessorID id, const juce::String& typeId) { processorLabels[id] = cgo::NodeFactory::getDisplayName (typeId); }

void SessionPresentation::addModulator (cgo::ModulatorID id, const juce::String& typeId)
{
    modulatorLabels[id] = cgo::NodeFactory::getDisplayName (typeId);
    modulatorOrder.push_back (id);
}

void SessionPresentation::removeProcessor (cgo::ProcessorID id) { processorLabels.erase (id); }

void SessionPresentation::removeModulator (cgo::ModulatorID id)
{
    modulatorLabels.erase (id);
    modulatorOrder.erase (std::remove (modulatorOrder.begin(), modulatorOrder.end(), id), modulatorOrder.end());
}

void SessionPresentation::clear()
{
    processorLabels.clear();
    modulatorLabels.clear();
    modulatorOrder.clear();
}

juce::String SessionPresentation::getProcessorLabel (cgo::ProcessorID id) const { return lookUpLabel (processorLabels, id); }

juce::String SessionPresentation::getModulatorLabel (cgo::ModulatorID id) const { return lookUpLabel (modulatorLabels, id); }

void SessionPresentation::setProcessorLabel (cgo::ProcessorID id, const juce::String& label) { processorLabels[id] = label; }

void SessionPresentation::setModulatorLabel (cgo::ModulatorID id, const juce::String& label) { modulatorLabels[id] = label; }

const std::vector<cgo::ModulatorID>& SessionPresentation::getModulatorOrder() const { return modulatorOrder; }

bool SessionPresentation::moveModulator (int from, int to)
{
    if (from == to || ! juce::isPositiveAndBelow (from, (int) modulatorOrder.size()))
        return false;

    const auto id = modulatorOrder[(size_t) from];
    modulatorOrder.erase (modulatorOrder.begin() + from);
    modulatorOrder.insert (modulatorOrder.begin() + juce::jlimit (0, (int) modulatorOrder.size(), to), id);

    return true;
}

juce::ValueTree SessionPresentation::toValueTree() const
{
    auto entry = [] (juce::uint32 uid, const juce::String& label)
    {
        juce::ValueTree n { viewIds::N };

        n.setProperty (viewIds::id, (int) uid, nullptr);
        n.setProperty (viewIds::label, label, nullptr);

        return n;
    };

    juce::ValueTree processors { viewIds::PROC };

    for (const auto& [id, label] : processorLabels)
        processors.appendChild (entry (id.uid, label), nullptr);

    juce::ValueTree modulators { viewIds::MOD };

    for (const auto id : modulatorOrder)
        modulators.appendChild (entry (id.uid, getModulatorLabel (id)), nullptr);

    juce::ValueTree tree { viewIds::VIEW };

    tree.appendChild (processors, nullptr);
    tree.appendChild (modulators, nullptr);

    return tree;
}

void SessionPresentation::restoreFromValueTree (const juce::ValueTree& parent, const cgo::ModularGraph& graph)
{
    const auto view = parent.getChildWithName (viewIds::VIEW);

    for (const auto n : view.getChildWithName (viewIds::PROC))
    {
        const cgo::ProcessorID id { (juce::uint32) (int) n[viewIds::id] };

        if (graph.processors().contains (id))
            processorLabels[id] = n[viewIds::label].toString();
    }

    for (const auto n : view.getChildWithName (viewIds::MOD))
    {
        const cgo::ModulatorID id { (juce::uint32) (int) n[viewIds::id] };

        if (! graph.modulators().contains (id) || std::find (modulatorOrder.begin(), modulatorOrder.end(), id) != modulatorOrder.end())
            continue;

        modulatorLabels[id] = n[viewIds::label].toString();
        modulatorOrder.push_back (id);
    }

    fillGaps (graph);
}

void SessionPresentation::fillGaps (const cgo::ModularGraph& graph)
{
    for (const auto& entry : graph.processors().getAll())
        if (processorLabels.find (entry.id) == processorLabels.end())
            processorLabels[entry.id] = cgo::NodeFactory::getDisplayName (entry.processor->getTypeId());

    for (const auto& entry : graph.modulators().getAll())
    {
        if (modulatorLabels.find (entry.id) == modulatorLabels.end())
            modulatorLabels[entry.id] = cgo::NodeFactory::getDisplayName (entry.modulator->getTypeId());

        if (std::find (modulatorOrder.begin(), modulatorOrder.end(), entry.id) == modulatorOrder.end())
            modulatorOrder.push_back (entry.id);
    }
}
