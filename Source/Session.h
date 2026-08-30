#pragma once

#include <cgo_graph/cgo_graph.h>

#include "SessionHistory.h"
#include "SessionPresentation.h"

class Session : private SessionHistory::Source
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void nodeAboutToBeRemoved (cgo::NodeRef node) = 0;
        virtual void sessionChanged() = 0;
    };

    class ScopedBatch
    {
    public:
        explicit ScopedBatch (Session& session, const juce::String& transactionName = {});
        ~ScopedBatch();

    private:
        Session& session;
        std::optional<cgo::ModulationGraph::ScopedBatch> modulationBatch;

        JUCE_DECLARE_NON_COPYABLE (ScopedBatch)
    };

    explicit Session (juce::AudioProcessor& hostProcessor);

    void prepareToPlay (double sampleRate, int samplesPerBlock);
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);
    void setPlayHead (juce::AudioPlayHead* newPlayHead);

    const cgo::ModularGraph& getGraph() const noexcept { return graph; }

    SessionHistory& getHistory() noexcept { return history; }

    void addListener (Listener* listener);
    void removeListener (Listener* listener);

    int getNumFreeSlots() const noexcept;

    void newSession();
    bool saveToFile (const juce::File& file);
    bool loadFromFile (const juce::File& file);

    juce::ValueTree toValueTree() const;
    bool restoreFromValueTree (const juce::ValueTree& state);

    cgo::ProcessorID addProcessor (std::unique_ptr<cgo::Processor> processor);
    cgo::ModulatorID addModulator (std::unique_ptr<cgo::Modulator> modulator);
    void removeProcessor (cgo::ProcessorID id);
    void removeModulator (cgo::ModulatorID id);

    std::optional<cgo::ProcessorID> duplicateProcessor (cgo::ProcessorID id);
    std::optional<cgo::ModulatorID> duplicateModulator (cgo::ModulatorID id);

    void beginGesture (const juce::String& name);
    void endGesture();

    std::optional<cgo::ConnectionID>
        addModulation (cgo::ModulatorID source, cgo::NodeRef targetNode, int targetParam, float depth = 1.0f, bool bipolar = false);
    std::optional<cgo::ConnectionID> addDepthModulation (cgo::ConnectionID connection, cgo::ModulatorID source, float depth = 1.0f, bool bipolar = false);
    void removeModulation (cgo::ConnectionID id);
    void setModulationBipolar (cgo::ConnectionID id, bool bipolar);
    void setModulationDepth (cgo::ConnectionID id, float depth);

    juce::String getProcessorLabel (cgo::ProcessorID id) const;
    juce::String getModulatorLabel (cgo::ModulatorID id) const;
    void setProcessorLabel (cgo::ProcessorID id, const juce::String& label);
    void setModulatorLabel (cgo::ModulatorID id, const juce::String& label);

    std::vector<cgo::ProcessorID> getProcessorOrder() const;
    void moveProcessor (int from, int to);
    const std::vector<cgo::ModulatorID>& getModulatorOrder() const;
    void moveModulator (int from, int to);

private:
    SessionHistory::Snapshot captureSession() const override;
    void applySession (const SessionHistory::Snapshot& from, const SessionHistory::Snapshot& to) override;

    void applyParameters (cgo::NodeRef node, const juce::ValueTree* from, const juce::ValueTree& to);
    void applyOrder (const std::vector<juce::ValueTree>& nodes);
    void applyModulations (const std::vector<cgo::ModulationGraph::ModulationEntry>& target);

    void removeNode (cgo::NodeRef node);
    void setNodeLabel (cgo::NodeRef node, const juce::String& label);

    void setParameterValue (cgo::NodeRef node, int paramIndex, float normalised);
    void notifySessionChanged();
    void notifyStateDirty();
    void flushNotifications();

    void copyModulations (cgo::NodeRef from, cgo::NodeRef to, int numParams);

    void rebuildChain (const std::vector<cgo::ProcessorID>& order);
    void clearSession();
    void updatePassThrough();

    juce::ValueTree nodeToValueTree (cgo::NodeRef node) const;
    std::optional<cgo::NodeRef> insertNodeFromValueTree (const juce::ValueTree& tree, bool keepIdentity);

    bool replaceSession (const juce::ValueTree& state, const juce::String& name);

    juce::AudioProcessor& hostProcessor;
    cgo::ModularGraph graph;
    cgo::ParameterManager parameterManager;
    SessionPresentation presentation;
    SessionHistory history;

    juce::ListenerList<Listener> listeners;

    bool sessionChangePending = false;
    bool stateDirtyPending = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
};
