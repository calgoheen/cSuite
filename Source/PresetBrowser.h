#pragma once

#include <JuceHeader.h>

class Session;

class PresetBrowser
{
public:
    explicit PresetBrowser (Session& session);

    void showSaveDialog();
    void showLoadDialog();

private:
    static juce::File getDirectory();

    Session& session;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE (PresetBrowser)
};
