#include "PluginEditor.h"

namespace
{
    constexpr const char* kParamDelayMs = "delay_ms";
    constexpr const char* kParamCorrection = "correction";
}

PluginEditor::PluginEditor (PluginProcessor& p)
: juce::AudioProcessorEditor (&p), processor (p)
{
    referenceLabel.setText ("Reference MIDI", juce::dontSendNotification);
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

    referenceBox.addItem ("Select MIDI...", 1);
    for (int i = 0; i < referenceFiles.size(); ++i)
        referenceBox.addItem (referenceFiles[i].getFileName(), i + 2);

    addAndMakeVisible (referenceBox);
    referenceBox.setEnabled (! referenceFiles.isEmpty());

    referenceBox.onChange = [this]
    {
        const int selectedId = referenceBox.getSelectedId();
        if (selectedId <= 1)
            return;

        const int index = selectedId - 2;
        if (! juce::isPositiveAndBelow (index, referenceFiles.size()))
            return;

        juce::String errorMessage;
        if (processor.loadReferenceFromFile (referenceFiles[index], errorMessage))
        {
            referenceStatusLabel.setText ("Loaded: " + referenceFiles[index].getFileName(),
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
                referenceBox.setSelectedId (i + 2, juce::dontSendNotification);
                referenceStatusLabel.setText ("Loaded: " + referenceFiles[i].getFileName(),
                    juce::dontSendNotification);
                break;
            }
        }
    }

    if (referenceFiles.isEmpty() && referenceStatusLabel.getText().isEmpty())
        referenceStatusLabel.setText ("No MIDI files found in ~/Downloads/PRISM/",
            juce::dontSendNotification);
    else if (referenceStatusLabel.getText().isEmpty())
        referenceStatusLabel.setText ("No reference loaded.", juce::dontSendNotification);

    slackLabel.setText ("Slack (ms)", juce::dontSendNotification);
    slackLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (slackLabel);

    slackSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slackSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible (slackSlider);

    correctionLabel.setText ("Correction", juce::dontSendNotification);
    correctionLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (correctionLabel);

    correctionSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    correctionSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible (correctionSlider);

    slackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, kParamDelayMs, slackSlider);
    correctionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, kParamCorrection, correctionSlider);

    setSize (520, 220);
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

    auto referenceRow = r.removeFromTop (26);
    referenceLabel.setBounds (referenceRow.removeFromLeft (120));
    referenceBox.setBounds (referenceRow.removeFromLeft (260));
    referenceStatusLabel.setBounds (referenceRow);

    r.removeFromTop (10);

    auto knobRow = r.removeFromTop (140);
    auto slackArea = knobRow.removeFromLeft (180);
    slackLabel.setBounds (slackArea.removeFromTop (20));
    slackSlider.setBounds (slackArea.reduced (10));

    auto correctionArea = knobRow.removeFromLeft (180);
    correctionLabel.setBounds (correctionArea.removeFromTop (20));
    correctionSlider.setBounds (correctionArea.reduced (10));
}
