#include "PluginEditor.h"
#include "PersonalitiesBuildInfo.h"
#include <cmath>

namespace
{
    constexpr const char* kParamDelayMs = "delay_ms";
    constexpr const char* kParamMatchWindowMs = "match_window_ms";
    constexpr const char* kParamCorrection = "correction";
    constexpr const char* kParamMute = "mute";
    constexpr const char* kParamBypass = "bypass";
    constexpr const char* kParamVelocityCorrection = "velocity_correction";
}

void PluginEditor::PulseIndicator::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (active ? juce::Colours::white : juce::Colours::darkgrey);
    g.fillEllipse (bounds);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawEllipse (bounds, 1.0f);
}

void PluginEditor::PulseIndicator::setActive (bool shouldBeActive)
{
    if (active == shouldBeActive)
        return;

    active = shouldBeActive;
    repaint();
}

PluginEditor::PluginEditor (PluginProcessor& p)
: juce::AudioProcessorEditor (&p), processor (p)
{
    referenceLabel.setText ("Personality", juce::dontSendNotification);
    referenceLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (referenceLabel);

    referenceStatusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (referenceStatusLabel);

    const auto referenceDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
        .getChildFile ("Downloads")
        .getChildFile ("PRISM");
    if (referenceDir.isDirectory())
        referenceDir.findChildFiles (referenceFiles, juce::File::findFiles, false, "*.mid;*.midi");

    struct FileSorter
    {
        int compareElements (const juce::File& a, const juce::File& b) const
        {
            return a.getFileName().compareIgnoreCase (b.getFileName());
        }
    };

    FileSorter sorter;
    referenceFiles.sort (sorter);

    referenceBox.clear();
    for (int i = 0; i < referenceFiles.size(); ++i)
        referenceBox.addItem (referenceFiles[i].getFileNameWithoutExtension(), i + 1);

    referenceBox.setTextWhenNoChoicesAvailable ("No personalities found");
    addAndMakeVisible (referenceBox);

    if (referenceFiles.isEmpty())
    {
        referenceBox.setTextWhenNothingSelected ("No personalities found");
        referenceBox.setSelectedId (0, juce::dontSendNotification);
        referenceBox.setColour (juce::ComboBox::textColourId, juce::Colours::lightgrey);
        referenceBox.setEnabled (false);
    }
    else
    {
        referenceBox.setTextWhenNothingSelected ("Select personality...");
        referenceBox.setColour (juce::ComboBox::textColourId, juce::Colours::white);
        referenceBox.setEnabled (true);
    }

    referenceBox.onChange = [this]
    {
        const int selectedId = referenceBox.getSelectedId();
        if (selectedId <= 0)
            return;

        const int index = selectedId - 1;
        if (! juce::isPositiveAndBelow (index, referenceFiles.size()))
            return;

        juce::String errorMessage;
        if (processor.loadReferenceFromFile (referenceFiles[index], errorMessage))
        {
            referenceStatusLabel.setText ("Loaded: " + referenceFiles[index].getFileNameWithoutExtension(),
                juce::dontSendNotification);
        }
        else
        {
            referenceStatusLabel.setText ("Load failed: " + errorMessage, juce::dontSendNotification);
        }
    };

    const auto currentPath = processor.getReferencePath();
    if (currentPath.isNotEmpty())
    {
        for (int i = 0; i < referenceFiles.size(); ++i)
        {
            if (referenceFiles[i].getFullPathName() == currentPath)
            {
                referenceBox.setSelectedId (i + 1, juce::dontSendNotification);
                referenceStatusLabel.setText ("Loaded: " + referenceFiles[i].getFileNameWithoutExtension(),
                    juce::dontSendNotification);
                break;
            }
        }
    }

    if (! referenceFiles.isEmpty() && referenceStatusLabel.getText().isEmpty())
        referenceStatusLabel.setText ("No reference loaded.", juce::dontSendNotification);

    slackLabel.setText ("Slack (ms)", juce::dontSendNotification);
    slackLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (slackLabel);

    slackSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slackSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible (slackSlider);

    matchWindowLabel.setText ("Match Window (ms)", juce::dontSendNotification);
    matchWindowLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (matchWindowLabel);

    matchWindowSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    matchWindowSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
    addAndMakeVisible (matchWindowSlider);

    autoButton.setButtonText ("Auto");
    addAndMakeVisible (autoButton);

    correctionLabel.setText ("Correction", juce::dontSendNotification);
    correctionLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (correctionLabel);

    correctionSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    correctionSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible (correctionSlider);

    inputLabel.setText ("Input", juce::dontSendNotification);
    inputLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (inputLabel);

    addAndMakeVisible (inputIndicator);

    outputLabel.setText ("Output", juce::dontSendNotification);
    outputLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (outputLabel);

    addAndMakeVisible (outputIndicator);

    timingLabel.setText ("Timing (ms)", juce::dontSendNotification);
    timingLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (timingLabel);

    timingValueLabel.setText ("0.00", juce::dontSendNotification);
    timingValueLabel.setJustificationType (juce::Justification::centred);
    timingValueLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (timingValueLabel);

    transportLabel.setText ("Transport", juce::dontSendNotification);
    transportLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (transportLabel);

    transportValueLabel.setText ("Stopped", juce::dontSendNotification);
    transportValueLabel.setJustificationType (juce::Justification::centredLeft);
    transportValueLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (transportValueLabel);

    matchLabel.setText ("Match/Miss", juce::dontSendNotification);
    matchLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (matchLabel);

    matchValueLabel.setText ("0 / 0", juce::dontSendNotification);
    matchValueLabel.setJustificationType (juce::Justification::centredLeft);
    matchValueLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (matchValueLabel);

    cpuLabel.setText ("CPU", juce::dontSendNotification);
    cpuLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (cpuLabel);

    cpuValueLabel.setText ("0.0%", juce::dontSendNotification);
    cpuValueLabel.setJustificationType (juce::Justification::centredLeft);
    cpuValueLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (cpuValueLabel);

    bpmLabel.setText ("BPM (Host/Ref)", juce::dontSendNotification);
    bpmLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (bpmLabel);

    bpmValueLabel.setText ("-- / --", juce::dontSendNotification);
    bpmValueLabel.setJustificationType (juce::Justification::centredLeft);
    bpmValueLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (bpmValueLabel);

    copyLogButton.setButtonText ("Copy Miss Log");
    addAndMakeVisible (copyLogButton);

    velocityButton.setButtonText ("Vel Corr");
    velocityButton.setClickingTogglesState (true);
    addAndMakeVisible (velocityButton);

    muteButton.setButtonText ("Mute");
    muteButton.setClickingTogglesState (true);
    addAndMakeVisible (muteButton);

    bypassButton.setButtonText ("Bypass");
    bypassButton.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton);

    slackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, kParamDelayMs, slackSlider);
    matchWindowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, kParamMatchWindowMs, matchWindowSlider);
    correctionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, kParamCorrection, correctionSlider);
    velocityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, kParamVelocityCorrection, velocityButton);
    muteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, kParamMute, muteButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, kParamBypass, bypassButton);

    autoButton.onClick = [this]
    {
        float matchWindowMs = 0.0f;
        float slackMs = 0.0f;
        if (processor.computeAutoMatchSettings (matchWindowMs, slackMs))
        {
            processor.applyAutoMatchSettings (matchWindowMs, slackMs);
            referenceStatusLabel.setText ("Auto set: Slack "
                    + juce::String (slackMs, 0)
                    + " ms, Window "
                    + juce::String (matchWindowMs, 0)
                    + " ms",
                juce::dontSendNotification);
        }
        else
        {
            referenceStatusLabel.setText ("Auto failed: load a personality.",
                juce::dontSendNotification);
        }
    };

    copyLogButton.onClick = [this]
    {
        if (processor.isTransportPlaying())
        {
            referenceStatusLabel.setText ("Stop transport to copy miss log.",
                juce::dontSendNotification);
            return;
        }

        const auto report = processor.createMissLogReport();
        juce::SystemClipboard::copyTextToClipboard (report);
        referenceStatusLabel.setText ("Miss log copied to clipboard.",
            juce::dontSendNotification);
    };

    juce::String buildInfoText = "v";
    buildInfoText << PERSONALITIES_VERSION_STRING << " | built " << PERSONALITIES_BUILD_TIMESTAMP;
    buildInfoLabel.setText (buildInfoText, juce::dontSendNotification);
    buildInfoLabel.setJustificationType (juce::Justification::centredRight);
    buildInfoLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    buildInfoLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (buildInfoLabel);

    lastInputNoteOnCounter = processor.getInputNoteOnCounter();
    lastInputFlashMs = juce::Time::getMillisecondCounterHiRes();
    lastOutputNoteOnCounter = processor.getOutputNoteOnCounter();
    lastOutputFlashMs = lastInputFlashMs;
    lastTimingDeltaMs = processor.getLastTimingDeltaMs();
    const juce::String timingPrefix = (lastTimingDeltaMs > 0.0f) ? "+" : "";
    timingValueLabel.setText (timingPrefix + juce::String (lastTimingDeltaMs, 2),
        juce::dontSendNotification);

    lastMatchedNoteOnCounter = processor.getMatchedNoteOnCounter();
    lastMissedNoteOnCounter = processor.getMissedNoteOnCounter();
    const uint32_t initialTotal = lastMatchedNoteOnCounter + lastMissedNoteOnCounter;
    const float initialMissPercent = initialTotal > 0
        ? (100.0f * static_cast<float> (lastMissedNoteOnCounter) / static_cast<float> (initialTotal))
        : 0.0f;
    matchValueLabel.setText (juce::String (lastMatchedNoteOnCounter) + " / "
            + juce::String (lastMissedNoteOnCounter) + " ("
            + juce::String (initialMissPercent, 1) + "%)",
        juce::dontSendNotification);

    lastTransportPlaying = processor.isTransportPlaying();
    transportValueLabel.setText (lastTransportPlaying ? "Playing" : "Stopped",
        juce::dontSendNotification);
    transportValueLabel.setColour (juce::Label::textColourId,
        lastTransportPlaying ? juce::Colours::lightgreen : juce::Colours::lightgrey);

    lastCpuPercent = processor.getCpuLoadPercent();
    cpuValueLabel.setText (juce::String (lastCpuPercent, 1) + "%", juce::dontSendNotification);

    lastHostBpm = processor.getHostBpm();
    lastReferenceBpm = processor.getReferenceBpm();
    auto formatBpm = [] (float bpm)
    {
        return (bpm > 0.0f) ? juce::String (bpm, 2) : juce::String ("--");
    };
    bpmValueLabel.setText (formatBpm (lastHostBpm) + " / " + formatBpm (lastReferenceBpm),
        juce::dontSendNotification);

    copyLogButton.setEnabled (! lastTransportPlaying);

    startTimerHz (30);

    setSize (670, 420);
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

    auto footer = r.removeFromBottom (18);
    buildInfoLabel.setBounds (footer);

    auto referenceRow = r.removeFromTop (26);
    referenceLabel.setBounds (referenceRow.removeFromLeft (120));
    referenceBox.setBounds (referenceRow.removeFromLeft (260));
    referenceStatusLabel.setBounds (referenceRow);

    r.removeFromTop (10);

    auto controlsArea = r.removeFromLeft (360);
    auto statusArea = r.reduced (10, 0);

    auto knobRow = controlsArea.removeFromTop (150);
    auto slackArea = knobRow.removeFromLeft (180);
    slackLabel.setBounds (slackArea.removeFromTop (20));
    slackSlider.setBounds (slackArea.reduced (10));

    auto correctionArea = knobRow;
    correctionLabel.setBounds (correctionArea.removeFromTop (20));
    correctionSlider.setBounds (correctionArea.reduced (10));

    controlsArea.removeFromTop (6);
    auto matchArea = controlsArea.removeFromTop (50);
    auto matchLabelRow = matchArea.removeFromTop (18);
    autoButton.setBounds (matchLabelRow.removeFromRight (60));
    matchWindowLabel.setBounds (matchLabelRow);
    matchWindowSlider.setBounds (matchArea);

    auto transportArea = statusArea.removeFromTop (24);
    transportLabel.setBounds (transportArea.removeFromLeft (90));
    transportValueLabel.setBounds (transportArea);

    auto inputArea = statusArea.removeFromTop (38);
    inputLabel.setBounds (inputArea.removeFromTop (18));
    inputIndicator.setBounds (inputArea.removeFromTop (20).withSizeKeepingCentre (18, 18));

    auto outputArea = statusArea.removeFromTop (38);
    outputLabel.setBounds (outputArea.removeFromTop (18));
    outputIndicator.setBounds (outputArea.removeFromTop (20).withSizeKeepingCentre (18, 18));

    auto timingArea = statusArea.removeFromTop (40);
    timingLabel.setBounds (timingArea.removeFromTop (18));
    timingValueLabel.setBounds (timingArea.removeFromTop (20));

    auto matchRow = statusArea.removeFromTop (24);
    matchLabel.setBounds (matchRow.removeFromLeft (90));
    matchValueLabel.setBounds (matchRow);

    auto cpuRow = statusArea.removeFromTop (24);
    cpuLabel.setBounds (cpuRow.removeFromLeft (90));
    cpuValueLabel.setBounds (cpuRow);

    auto bpmRow = statusArea.removeFromTop (24);
    bpmLabel.setBounds (bpmRow.removeFromLeft (90));
    bpmValueLabel.setBounds (bpmRow);

    auto copyRow = statusArea.removeFromTop (26);
    copyLogButton.setBounds (copyRow);

    statusArea.removeFromTop (4);
    velocityButton.setBounds (statusArea.removeFromTop (24));
    muteButton.setBounds (statusArea.removeFromTop (24));
    bypassButton.setBounds (statusArea.removeFromTop (24));
}

void PluginEditor::timerCallback()
{
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();

    const auto inputCounter = processor.getInputNoteOnCounter();
    if (inputCounter != lastInputNoteOnCounter)
    {
        lastInputNoteOnCounter = inputCounter;
        lastInputFlashMs = nowMs;
        inputIndicator.setActive (true);
    }

    if (inputIndicator.isActive() && (nowMs - lastInputFlashMs) > 120.0)
        inputIndicator.setActive (false);

    const auto outputCounter = processor.getOutputNoteOnCounter();
    if (outputCounter != lastOutputNoteOnCounter)
    {
        lastOutputNoteOnCounter = outputCounter;
        lastOutputFlashMs = nowMs;
        outputIndicator.setActive (true);
    }

    if (outputIndicator.isActive() && (nowMs - lastOutputFlashMs) > 120.0)
        outputIndicator.setActive (false);

    float deltaMs = processor.getLastTimingDeltaMs();
    if (std::abs (deltaMs) < 0.005f)
        deltaMs = 0.0f;

    if (std::abs (deltaMs - lastTimingDeltaMs) > 0.005f)
    {
        lastTimingDeltaMs = deltaMs;
        const juce::String prefix = (deltaMs > 0.0f) ? "+" : "";
        timingValueLabel.setText (prefix + juce::String (deltaMs, 2), juce::dontSendNotification);
    }

    const bool isPlaying = processor.isTransportPlaying();
    if (isPlaying != lastTransportPlaying)
    {
        lastTransportPlaying = isPlaying;
        transportValueLabel.setText (isPlaying ? "Playing" : "Stopped", juce::dontSendNotification);
        transportValueLabel.setColour (juce::Label::textColourId,
            isPlaying ? juce::Colours::lightgreen : juce::Colours::lightgrey);
        copyLogButton.setEnabled (! isPlaying);
    }

    const auto matched = processor.getMatchedNoteOnCounter();
    const auto missed = processor.getMissedNoteOnCounter();
    if (matched != lastMatchedNoteOnCounter || missed != lastMissedNoteOnCounter)
    {
        lastMatchedNoteOnCounter = matched;
        lastMissedNoteOnCounter = missed;
        const uint32_t total = matched + missed;
        const float missPercent = total > 0
            ? (100.0f * static_cast<float> (missed) / static_cast<float> (total))
            : 0.0f;
        matchValueLabel.setText (juce::String (matched) + " / " + juce::String (missed)
                + " (" + juce::String (missPercent, 1) + "%)",
            juce::dontSendNotification);
    }

    const float cpuPercent = processor.getCpuLoadPercent();
    if (std::abs (cpuPercent - lastCpuPercent) > 0.1f)
    {
        lastCpuPercent = cpuPercent;
        cpuValueLabel.setText (juce::String (cpuPercent, 1) + "%", juce::dontSendNotification);
    }

    const float hostBpm = processor.getHostBpm();
    const float refBpm = processor.getReferenceBpm();
    if (std::abs (hostBpm - lastHostBpm) > 0.05f || std::abs (refBpm - lastReferenceBpm) > 0.05f)
    {
        lastHostBpm = hostBpm;
        lastReferenceBpm = refBpm;
        auto formatBpm = [] (float bpm)
        {
            return (bpm > 0.0f) ? juce::String (bpm, 2) : juce::String ("--");
        };
        bpmValueLabel.setText (formatBpm (hostBpm) + " / " + formatBpm (refBpm),
            juce::dontSendNotification);
    }
}
