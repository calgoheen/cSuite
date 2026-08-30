#pragma once

#include <cgo_graph/cgo_graph.h>

class SessionPresentation
{
public:
    SessionPresentation() = default;

    void addProcessor (cgo::ProcessorID id, const juce::String& typeId);
    void addModulator (cgo::ModulatorID id, const juce::String& typeId);
    void removeProcessor (cgo::ProcessorID id);
    void removeModulator (cgo::ModulatorID id);
    void clear();

    juce::String getProcessorLabel (cgo::ProcessorID id) const;
    juce::String getModulatorLabel (cgo::ModulatorID id) const;
    void setProcessorLabel (cgo::ProcessorID id, const juce::String& label);
    void setModulatorLabel (cgo::ModulatorID id, const juce::String& label);

    const std::vector<cgo::ModulatorID>& getModulatorOrder() const;

    bool moveModulator (int from, int to);

    juce::ValueTree toValueTree() const;
    void restoreFromValueTree (const juce::ValueTree& parent, const cgo::ModularGraph& graph);

private:
    void fillGaps (const cgo::ModularGraph& graph);

    std::map<cgo::ProcessorID, juce::String> processorLabels;
    std::map<cgo::ModulatorID, juce::String> modulatorLabels;
    std::vector<cgo::ModulatorID> modulatorOrder;

    JUCE_DECLARE_NON_COPYABLE (SessionPresentation)
};
