#include "PresetBrowser.h"
#include "Session.h"

namespace
{

const juce::String presetWildcard { "*.xml" };

} // namespace

PresetBrowser::PresetBrowser (Session& s) : session (s) {}

void PresetBrowser::showSaveDialog()
{
    chooser = std::make_unique<juce::FileChooser> ("Save preset", getDirectory().getChildFile ("Untitled.xml"), presetWildcard);

    const auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting;

    chooser->launchAsync (flags,
                          [this] (const juce::FileChooser& browser)
                          {
                              if (browser.getResults().isEmpty())
                                  return;

                              const auto file = browser.getResult();

                              if (! session.saveToFile (file))
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                          "Couldn't save preset",
                                                                          "Nothing could be written to " + file.getFullPathName() + ".");
                          });
}

void PresetBrowser::showLoadDialog()
{
    chooser = std::make_unique<juce::FileChooser> ("Load preset", getDirectory(), presetWildcard);

    const auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (flags,
                          [this] (const juce::FileChooser& browser)
                          {
                              if (browser.getResults().isEmpty())
                                  return;

                              if (! session.loadFromFile (browser.getResult()))
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                          "Couldn't load preset",
                                                                          "This file isn't a cSuite preset, or it was saved by a newer version.");
                          });
}

juce::File PresetBrowser::getDirectory()
{
    auto directory = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);

#if JUCE_MAC
    directory = directory.getChildFile ("Application Support");
#endif

    directory = directory.getChildFile (JucePlugin_Manufacturer).getChildFile (JucePlugin_Name).getChildFile ("Presets");
    directory.createDirectory();

    return directory;
}
