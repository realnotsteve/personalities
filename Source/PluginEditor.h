#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PluginProcessor& processor;

    juce::ComboBox referenceBox;
    juce::Label referenceLabel;
    juce::Label referenceStatusLabel;

    juce::Slider slackSlider;
    juce::Label  slackLabel;
    juce::Slider correctionSlider;
    juce::Label  correctionLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> correctionAttachment;
    juce::Array<juce::File> referenceFiles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
