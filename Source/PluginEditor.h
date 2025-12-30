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

    juce::Slider delaySlider;
    juce::Label  delayLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};