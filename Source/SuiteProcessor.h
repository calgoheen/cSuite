#pragma once

#include "Session.h"

class SuiteProcessor : public juce::AudioProcessor
{
public:
    SuiteProcessor();
    ~SuiteProcessor() override = default;

    bool isBusesLayoutSupported (const juce::AudioProcessor::BusesLayout& layouts) const override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void setPlayHead (juce::AudioPlayHead* newPlayHead) override;
    void releaseResources() override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    const juce::String getName() const override;
    double getTailLengthSeconds() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    bool hasEditor() const override;
    juce::AudioProcessorEditor* createEditor() override;

private:
    Session session;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuiteProcessor)
};
