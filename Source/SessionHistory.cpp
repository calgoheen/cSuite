#include "SessionHistory.h"

namespace
{

bool sameModulation (const cgo::ModulationGraph::ModulationEntry& a, const cgo::ModulationGraph::ModulationEntry& b)
{
    return a.id == b.id && a.source == b.source && a.target == b.target && juce::exactlyEqual (a.depth, b.depth) && a.bipolar == b.bipolar;
}

bool sameSession (const SessionHistory::Snapshot& a, const SessionHistory::Snapshot& b)
{
    if (a.nodes.size() != b.nodes.size() || a.modulations.size() != b.modulations.size())
        return false;

    for (size_t i = 0; i < a.nodes.size(); i++)
        if (! a.nodes[i].isEquivalentTo (b.nodes[i]))
            return false;

    return std::equal (a.modulations.begin(), a.modulations.end(), b.modulations.begin(), sameModulation);
}

} // namespace

SessionHistory::ScopedRestore::ScopedRestore (SessionHistory& h) : history (h), wasRestoring (h.restoring) { history.restoring = true; }

SessionHistory::ScopedRestore::~ScopedRestore() { history.restoring = wasRestoring; }

SessionHistory::SessionHistory (Source& s) : source (s) {}

void SessionHistory::undo()
{
    if (! canUndo())
        return;

    const auto& step = steps[--nextIndex];

    apply (step.after, step.before);
    sendChangeMessage();
}

void SessionHistory::redo()
{
    if (! canRedo())
        return;

    const auto& step = steps[nextIndex++];

    apply (step.before, step.after);
    sendChangeMessage();
}

bool SessionHistory::canUndo() const noexcept { return nextIndex > 0; }

bool SessionHistory::canRedo() const noexcept { return nextIndex < steps.size(); }

juce::String SessionHistory::getUndoName() const { return canUndo() ? steps[nextIndex - 1].name : juce::String(); }

juce::String SessionHistory::getRedoName() const { return canRedo() ? steps[nextIndex].name : juce::String(); }

void SessionHistory::clear()
{
    steps.clear();
    nextIndex = 0;

    openSnapshot.reset();
    gestureSnapshot.reset();

    sendChangeMessage();
}

bool SessionHistory::isRecording() const noexcept { return ! restoring && ! applying; }

void SessionHistory::beginTransaction (const juce::String& name)
{
    if (depth++ > 0)
        return;

    gestureSnapshot.reset();

    openName = name;

    if (isRecording())
        openSnapshot = source.captureSession();
}

void SessionHistory::endTransaction()
{
    if (--depth > 0 || ! openSnapshot.has_value())
        return;

    auto before = std::move (*openSnapshot);
    openSnapshot.reset();

    if (isRecording())
        commit (std::move (before), openName);
}

void SessionHistory::nameTransaction (const juce::String& name)
{
    if (depth > 0 && openName.isEmpty())
        openName = name;
}

void SessionHistory::beginGesture (const juce::String& name)
{
    if (! isRecording())
        return;

    gestureName = name;
    gestureSnapshot = source.captureSession();
}

bool SessionHistory::endGesture()
{
    if (! gestureSnapshot.has_value())
        return false;

    auto before = std::move (*gestureSnapshot);
    gestureSnapshot.reset();

    return isRecording() && commit (std::move (before), gestureName);
}

bool SessionHistory::commit (Snapshot before, const juce::String& name)
{
    auto after = source.captureSession();

    if (sameSession (before, after))
        return false;

    steps.erase (steps.begin() + (std::ptrdiff_t) nextIndex, steps.end());
    steps.push_back ({ std::move (before), std::move (after), name });

    if (steps.size() > maxSteps)
        steps.erase (steps.begin());

    nextIndex = steps.size();

    sendChangeMessage();

    return true;
}

void SessionHistory::apply (const Snapshot& from, const Snapshot& to)
{
    const juce::ScopedValueSetter<bool> guard { applying, true };

    source.applySession (from, to);
}
