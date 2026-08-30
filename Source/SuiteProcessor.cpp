#include "SuiteProcessor.h"
#include "SuiteComponent.h"

SuiteProcessor::SuiteProcessor()
  : juce::AudioProcessor (BusesProperties()
                              .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                              .withInput ("Sidechain", juce::AudioChannelSet::stereo(), true)),
    session (*this)
{
}

bool SuiteProcessor::isBusesLayoutSupported (const juce::AudioProcessor::BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    const auto sidechain = layouts.getChannelSet (true, 1);

    if (sidechain != juce::AudioChannelSet::disabled() && sidechain != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono())
        return sidechain == juce::AudioChannelSet::disabled();

    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void SuiteProcessor::prepareToPlay (double sampleRate, int samplesPerBlock) { session.prepareToPlay (sampleRate, samplesPerBlock); }

void SuiteProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    session.processBlock (buffer, midi);
}

void SuiteProcessor::setPlayHead (juce::AudioPlayHead* newPlayHead)
{
    juce::AudioProcessor::setPlayHead (newPlayHead);
    session.setPlayHead (newPlayHead);
}

void SuiteProcessor::releaseResources() {}

void SuiteProcessor::getStateInformation (juce::MemoryBlock& destData) { copyXmlToBinary (*session.toValueTree().createXml(), destData); }

void SuiteProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    JUCE_ASSERT_MESSAGE_THREAD

    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr)
        return;

    session.restoreFromValueTree (juce::ValueTree::fromXml (*xml));

    session.getHistory().clear();
}

const juce::String SuiteProcessor::getName() const { return JucePlugin_Name; }

double SuiteProcessor::getTailLengthSeconds() const { return 0.0; }

bool SuiteProcessor::acceptsMidi() const { return false; }

bool SuiteProcessor::producesMidi() const { return false; }

int SuiteProcessor::getNumPrograms() { return 1; }

int SuiteProcessor::getCurrentProgram() { return 0; }

void SuiteProcessor::setCurrentProgram (int) {}

const juce::String SuiteProcessor::getProgramName (int) { return "default"; }

void SuiteProcessor::changeProgramName (int, const juce::String&) {}

bool SuiteProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* SuiteProcessor::createEditor() { return new cgo::ResizableEditor (*this, std::make_unique<SuiteComponent> (session)); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SuiteProcessor(); }
