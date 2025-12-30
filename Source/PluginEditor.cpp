#include "PluginEditor.h"

namespace
{
    constexpr const char* kParamDelayMs = "delay_ms";
}

PluginEditor::PluginEditor (PluginProcessor& p)
: juce::AudioProcessorEditor (&p), processor (p)
{
    delayLabel.setText ("Delay (ms)", juce::dontSendNotification);
    delayLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (delayLabel);

    delaySlider.setSliderStyle (juce::Slider::LinearHorizontal);
    delaySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 90, 22);
    addAndMakeVisible (delaySlider);

    delayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, kParamDelayMs, delaySlider);

    setSize (420, 120);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawText ("Personalities", getLocalBounds().removeFromTop(30), juce::Justification::centred);
}

void PluginEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    r.removeFromTop (34);

    auto row = r.removeFromTop (28);
    delayLabel.setBounds (row.removeFromLeft (110));
    delaySlider.setBounds (row);
}