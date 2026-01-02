#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class PulseIndicator final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
        void setActive (bool shouldBeActive);
        bool isActive() const noexcept { return active; }

    private:
        bool active = false;
    };

    void timerCallback() override;

    PluginProcessor& processor;

    juce::ComboBox referenceBox;
    juce::Label referenceLabel;
    juce::Label referenceStatusLabel;

    juce::Slider slackSlider;
    juce::Label  slackLabel;
    juce::Slider clusterWindowSlider;
    juce::Label  clusterWindowLabel;
    juce::Slider correctionSlider;
    juce::Label  correctionLabel;
    juce::Label buildInfoLabel;
    juce::Label inputLabel;
    PulseIndicator inputIndicator;
    juce::Label outputLabel;
    PulseIndicator outputIndicator;
    juce::Label timingLabel;
    juce::Label timingValueLabel;
    juce::Label transportLabel;
    juce::Label transportValueLabel;
    juce::Label matchLabel;
    juce::Label matchValueLabel;
    juce::Label cpuLabel;
    juce::Label cpuValueLabel;
    juce::Label bpmLabel;
    juce::Label bpmValueLabel;
    juce::Label refIoiLabel;
    juce::Label refIoiValueLabel;
    juce::Label startOffsetLabel;
    juce::Label startOffsetValueLabel;
    juce::TextButton resetStartOffsetButton;
    juce::TextButton copyLogButton;
    juce::ToggleButton tempoShiftButton;
    juce::ToggleButton velocityButton;
    juce::ToggleButton muteButton;
    juce::ToggleButton bypassButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> clusterWindowAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> correctionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> velocityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tempoShiftAttachment;
    juce::Array<juce::File> referenceFiles;
    uint32_t lastInputNoteOnCounter = 0;
    double lastInputFlashMs = 0.0;
    uint32_t lastOutputNoteOnCounter = 0;
    double lastOutputFlashMs = 0.0;
    float lastTimingDeltaMs = 0.0f;
    uint32_t lastMatchedNoteOnCounter = 0;
    uint32_t lastMissedNoteOnCounter = 0;
    bool lastTransportPlaying = false;
    float lastCpuPercent = 0.0f;
    float lastHostBpm = -1.0f;
    float lastReferenceBpm = -1.0f;
    float lastRefIoiMinMs = -1.0f;
    float lastRefIoiMedianMs = -1.0f;
    float lastStartOffsetMs = 0.0f;
    float lastStartOffsetBars = 0.0f;
    bool lastStartOffsetValid = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
