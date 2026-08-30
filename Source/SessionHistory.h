#pragma once

#include <cgo_graph/cgo_graph.h>

class SessionHistory : public juce::ChangeBroadcaster
{
public:
    struct Snapshot
    {
        std::vector<juce::ValueTree> nodes;
        std::vector<cgo::ModulationGraph::ModulationEntry> modulations;
    };

    struct Source
    {
        virtual ~Source() = default;
        virtual Snapshot captureSession() const = 0;
        virtual void applySession (const Snapshot& from, const Snapshot& to) = 0;
    };

    class ScopedRestore
    {
    public:
        explicit ScopedRestore (SessionHistory& history);
        ~ScopedRestore();

    private:
        SessionHistory& history;
        const bool wasRestoring;

        JUCE_DECLARE_NON_COPYABLE (ScopedRestore)
    };

    explicit SessionHistory (Source& source);

    void undo();
    void redo();

    bool canUndo() const noexcept;
    bool canRedo() const noexcept;

    juce::String getUndoName() const;
    juce::String getRedoName() const;

    void clear();

    bool isRecording() const noexcept;
    bool isRestoring() const noexcept { return restoring; }

    void beginTransaction (const juce::String& name);
    void endTransaction();
    bool isTransactionOpen() const noexcept { return depth > 0; }

    void nameTransaction (const juce::String& name);

    void beginGesture (const juce::String& name);
    bool endGesture();

private:
    struct Step
    {
        Snapshot before;
        Snapshot after;
        juce::String name;
    };

    static constexpr size_t maxSteps = 64;

    bool commit (Snapshot before, const juce::String& name);
    void apply (const Snapshot& from, const Snapshot& to);

    Source& source;

    std::vector<Step> steps;
    size_t nextIndex = 0;

    std::optional<Snapshot> openSnapshot;
    juce::String openName;

    std::optional<Snapshot> gestureSnapshot;
    juce::String gestureName;

    int depth = 0;
    bool restoring = false;
    bool applying = false;

    JUCE_DECLARE_NON_COPYABLE (SessionHistory)
};
