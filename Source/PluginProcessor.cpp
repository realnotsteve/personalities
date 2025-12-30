#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr const char* kParamDelayMs = "delay_ms";
}

PluginProcessor::PluginProcessor()
: juce::AudioProcessor (
      BusesProperties()
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
  apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { kParamDelayMs, 1 },
        "Delay (ms)",
        juce::NormalisableRange<float> { 0.0f, 500.0f, 0.1f },
        50.0f
    ));

    return layout;
}

void PluginProcessor::prepareToPlay (double, int)
{
    // Preallocate delay structures later (M3). Keep RT-safe.
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // We expect no input bus and a stereo output bus.
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // Always silent audio for host stability
    buffer.clear();

    // M2: MIDI pass-through. (M3: replace with scheduled delay output)
    juce::ignoreUnused (midi);
    // no-op: incoming midi already in `midi`
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}