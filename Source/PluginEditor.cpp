#include "PluginEditor.h"
#include "BinaryData.h"
#include "PersonalitiesBuildInfo.h"
#include <cmath>

namespace
{
    constexpr int kUiBaseWidth = 720;
    constexpr int kUiBaseHeight = 509;
    constexpr const char* kParamDelayMs = "delay_ms";
    constexpr const char* kParamClusterWindowMs = "match_window_ms";
    constexpr const char* kParamCorrection = "correction";
    constexpr const char* kParamMute = "mute";
    constexpr const char* kParamBypass = "bypass";
    constexpr const char* kParamVelocityCorrection = "velocity_correction";
    constexpr const char* kParamTempoShiftBackBar = "tempo_shift_back_bar";
}

void PluginEditor::PulseIndicator::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (active ? activeColour : inactiveColour);
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

void PluginEditor::PulseIndicator::setColours (juce::Colour active, juce::Colour inactive)
{
    activeColour = active;
    inactiveColour = inactive;
    repaint();
}

void PluginEditor::CorrectionDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (8.0f);
    const auto center = bounds.getCentre();
    const float width = bounds.getWidth();
    const float height = bounds.getHeight();
    const float scale = juce::jmin (width, height) * 0.38f;
    const float corner = 14.0f;

    if (! minimalStyle)
    {
        const auto shadowBounds = bounds.translated (0.0f, 6.0f);
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (shadowBounds, corner);

        juce::ColourGradient glassGrad (juce::Colour (0x283447).withAlpha (0.9f),
            bounds.getTopLeft(),
            juce::Colour (0x0b0f18).withAlpha (0.95f),
            bounds.getBottomRight(), false);
        g.setGradientFill (glassGrad);
        g.fillRoundedRectangle (bounds, corner);

        auto innerBounds = bounds.reduced (2.0f);
        juce::ColourGradient innerGrad (juce::Colours::white.withAlpha (0.18f),
            innerBounds.getTopLeft(),
            juce::Colours::transparentBlack,
            innerBounds.getCentre(), false);
        g.setGradientFill (innerGrad);
        g.fillRoundedRectangle (innerBounds, corner - 2.0f);

        auto highlightBand = bounds.withHeight (bounds.getHeight() * 0.35f).reduced (8.0f, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRoundedRectangle (highlightBand, corner - 6.0f);

        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.drawRoundedRectangle (bounds, corner, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (bounds.reduced (4.0f), corner - 4.0f, 1.0f);
    }

    const juce::Point<float> origin (center.x, center.y + scale * 0.28f);
    const juce::Point<float> axisX (scale * 0.75f, scale * 0.36f);
    const juce::Point<float> axisY (-scale * 0.75f, scale * 0.36f);
    const juce::Point<float> axisZ (0.0f, -scale * 0.9f);

    const juce::Point<float> baseA = origin;
    const juce::Point<float> baseB = origin + axisX;
    const juce::Point<float> baseD = origin + axisY;
    const juce::Point<float> baseC = baseB + axisY;

    const juce::Point<float> topA = baseA + axisZ;
    const juce::Point<float> topB = baseB + axisZ;
    const juce::Point<float> topC = baseC + axisZ;
    const juce::Point<float> topD = baseD + axisZ;

    if (! minimalStyle)
    {
        juce::Path basePlane;
        basePlane.startNewSubPath (baseA);
        basePlane.lineTo (baseB);
        basePlane.lineTo (baseC);
        basePlane.lineTo (baseD);
        basePlane.closeSubPath();
        g.setColour (juce::Colour (0x0c1016).withAlpha (0.7f));
        g.fillPath (basePlane);

        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.strokePath (basePlane, juce::PathStrokeType (1.0f));

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        for (int i = 1; i <= 4; ++i)
        {
            const float t = static_cast<float> (i) / 5.0f;
            const auto p1 = baseA + axisX * t;
            const auto p2 = baseD + axisX * t;
            g.drawLine (p1.x, p1.y, p2.x, p2.y, 1.0f);

            const auto q1 = baseA + axisY * t;
            const auto q2 = baseB + axisY * t;
            g.drawLine (q1.x, q1.y, q2.x, q2.y, 1.0f);
        }

        auto drawAxis = [&g](juce::Point<float> start, juce::Point<float> end, juce::Colour colour)
        {
            g.setColour (colour.withAlpha (0.25f));
            g.drawLine (start.x, start.y, end.x, end.y, 4.0f);
            g.setColour (colour.withAlpha (0.8f));
            g.drawLine (start.x, start.y, end.x, end.y, 2.0f);
        };

        drawAxis (baseA, baseB, juce::Colour (0x5cd5ff));
        drawAxis (baseA, baseD, juce::Colour (0xff7bb0));
        drawAxis (baseA, topA, juce::Colour (0xa8ff7b));
    }

    auto toScreen = [&](const TrailPoint& point)
    {
        return origin + axisX * point.x + axisY * point.y + axisZ * point.z;
    };

    if (trailCount > 1)
    {
        for (int i = 1; i < trailCount; ++i)
        {
            const int index0 = (trailHead - trailCount + i - 1 + kTrailLength) % kTrailLength;
            const int index1 = (trailHead - trailCount + i + kTrailLength) % kTrailLength;
            const auto& pointA = trail[static_cast<size_t> (index0)];
            const auto& pointB = trail[static_cast<size_t> (index1)];
            const auto posA = toScreen (pointA);
            const auto posB = toScreen (pointB);
            const float age = static_cast<float> (i) / static_cast<float> (trailCount - 1);
            const float hue = juce::jmap (0.5f * (pointA.magnitude + pointB.magnitude), 0.62f, 0.06f);
            const float glowWidth = 5.0f + 7.0f * age;
            const float coreWidth = 1.5f + 3.5f * age;
            const juce::Colour glowColour = juce::Colour::fromHSV (hue, 0.4f, 1.0f, 0.06f + 0.18f * age);
            const juce::Colour coreColour = juce::Colour::fromHSV (hue, 0.75f, 1.0f, 0.12f + 0.5f * age);
            g.setColour (glowColour);
            g.drawLine (posA.x, posA.y, posB.x, posB.y, glowWidth);
            g.setColour (coreColour);
            g.drawLine (posA.x, posA.y, posB.x, posB.y, coreWidth);
        }
    }

    for (int i = 0; i < trailCount; ++i)
    {
        const int index = (trailHead - trailCount + i + kTrailLength) % kTrailLength;
        const auto& point = trail[static_cast<size_t> (index)];
        const auto pos = toScreen (point);
        const float age = static_cast<float> (i + 1) / static_cast<float> (trailCount);
        const float alpha = 0.06f + 0.38f * age;
        const float hue = juce::jmap (point.magnitude, 0.62f, 0.06f);
        const juce::Colour trailColour = juce::Colour::fromHSV (hue, 0.6f, 1.0f, alpha);
        const float radius = 3.0f + age * 7.5f;
        g.setColour (trailColour);
        g.fillEllipse (pos.x - radius, pos.y - radius, radius * 2.0f, radius * 2.0f);
    }

    const float magnitude = juce::jlimit (0.0f, 1.0f, smoothedMagnitude);
    const float hue = juce::jmap (magnitude, 0.62f, 0.06f);
    const juce::Colour glowColour = juce::Colour::fromHSV (hue, 0.9f, 1.0f, 0.95f);
    const juce::Point<float> position = origin + axisX * smoothedOn + axisY * smoothedOff + axisZ * smoothedVel;
    const juce::Point<float> shadowPos = origin + axisX * smoothedOn + axisY * smoothedOff;

    const float shadowRadius = 12.0f + magnitude * 14.0f;
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (shadowPos.x - shadowRadius * 1.2f, shadowPos.y - shadowRadius * 0.6f,
        shadowRadius * 2.4f, shadowRadius * 1.2f);

    g.setColour (glowColour.withAlpha (0.25f));
    g.fillEllipse (position.x - shadowRadius, position.y - shadowRadius,
        shadowRadius * 2.0f, shadowRadius * 2.0f);

    const float orbRadius = 12.0f + magnitude * 14.0f;
    juce::ColourGradient orbGrad (glowColour,
        { position.x - orbRadius * 0.4f, position.y - orbRadius * 0.5f },
        juce::Colours::transparentBlack,
        { position.x + orbRadius, position.y + orbRadius }, true);
    g.setGradientFill (orbGrad);
    g.fillEllipse (position.x - orbRadius, position.y - orbRadius,
        orbRadius * 2.0f, orbRadius * 2.0f);

    juce::ColourGradient glassHighlight (juce::Colours::white.withAlpha (0.7f),
        { position.x - orbRadius * 0.5f, position.y - orbRadius * 0.6f },
        juce::Colours::transparentWhite,
        { position.x + orbRadius * 0.2f, position.y + orbRadius * 0.2f }, true);
    g.setGradientFill (glassHighlight);
    g.fillEllipse (position.x - orbRadius * 0.6f, position.y - orbRadius * 0.6f,
        orbRadius * 1.2f, orbRadius * 1.2f);

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawEllipse (position.x - orbRadius, position.y - orbRadius,
        orbRadius * 2.0f, orbRadius * 2.0f, 1.0f);
}

void PluginEditor::CorrectionDisplay::setValues (float noteOnDeltaMs,
                                                 float noteOffDeltaMs,
                                                 float velocityDelta,
                                                 float slackMs)
{
    const float timeScale = juce::jmax (1.0f, slackMs);
    const float targetOn = juce::jlimit (-1.0f, 1.0f, noteOnDeltaMs / timeScale);
    const float targetOff = juce::jlimit (-1.0f, 1.0f, noteOffDeltaMs / timeScale);
    const float targetVel = juce::jlimit (-1.0f, 1.0f, velocityDelta / 127.0f);

    smoothedOn += 0.18f * (targetOn - smoothedOn);
    smoothedOff += 0.18f * (targetOff - smoothedOff);
    smoothedVel += 0.18f * (targetVel - smoothedVel);
    const float targetMag = std::sqrt (smoothedOn * smoothedOn
        + smoothedOff * smoothedOff + smoothedVel * smoothedVel);
    smoothedMagnitude += 0.2f * (targetMag - smoothedMagnitude);

    trail[trailHead] = { smoothedOn, smoothedOff, smoothedVel, smoothedMagnitude };
    trailHead = (trailHead + 1) % kTrailLength;
    if (trailCount < kTrailLength)
        ++trailCount;

    repaint();
}

PluginEditor::ExpandButton::ExpandButton()
    : juce::Button ("Expand")
{
}

void PluginEditor::ExpandButton::setExpanded (bool shouldBeExpanded)
{
    if (isExpanded == shouldBeExpanded)
        return;

    isExpanded = shouldBeExpanded;
    repaint();
}

void PluginEditor::ExpandButton::paintButton (juce::Graphics& g,
                                              bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown)
{
    auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 1.0f;

    juce::Colour fill = juce::Colour (0x1a1d24);
    if (shouldDrawButtonAsDown)
        fill = fill.brighter (0.2f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter (0.1f);

    g.setColour (fill);
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);

    const float arrowSize = radius * 0.65f;
    const float shaft = arrowSize * 0.45f;
    const float head = arrowSize * 0.25f;

    auto drawArrow = [&](float xDir, float yDir)
    {
        juce::Point<float> tip = centre + juce::Point<float> (xDir, yDir) * arrowSize;
        juce::Point<float> tail = centre + juce::Point<float> (-xDir, -yDir) * shaft;
        juce::Point<float> left = tip + juce::Point<float> (-yDir, xDir) * head;
        juce::Point<float> right = tip + juce::Point<float> (yDir, -xDir) * head;

        g.drawLine ({ tail, tip }, 2.0f);
        g.drawLine ({ left, tip }, 2.0f);
        g.drawLine ({ right, tip }, 2.0f);
    };

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    const float direction = isExpanded ? -1.0f : 1.0f;
    drawArrow (direction, direction);
    drawArrow (-direction, direction);
}

PluginEditor::PluginEditor (PluginProcessor& p)
: juce::AudioProcessorEditor (&p), processor (p)
{
    backgroundOpen = juce::ImageCache::getFromMemory (BinaryData::master_uiopen_png,
        BinaryData::master_uiopen_pngSize);
    backgroundClosed = juce::ImageCache::getFromMemory (BinaryData::master_uiclosed_png,
        BinaryData::master_uiclosed_pngSize);

    referenceLabel.setText ("Performer", juce::dontSendNotification);
    referenceLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (referenceLabel);

    referenceStatusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (referenceStatusLabel);

    referenceLoadedIndicator.setColours (juce::Colours::green, juce::Colours::darkgrey);
    referenceLoadedIndicator.setActive (false);
    addAndMakeVisible (referenceLoadedIndicator);

    virtuosoTabButton.setButtonText ("Virtuoso");
    virtuosoTabButton.setClickingTogglesState (true);
    virtuosoTabButton.setEnabled (false);
    addAndMakeVisible (virtuosoTabButton);

    influencerTabButton.setButtonText ("Influencer");
    influencerTabButton.setClickingTogglesState (true);
    influencerTabButton.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (influencerTabButton);

    actualiserTabButton.setButtonText ("Actualiser");
    actualiserTabButton.setClickingTogglesState (true);
    actualiserTabButton.setEnabled (false);
    addAndMakeVisible (actualiserTabButton);

    tabContainer.setText ("");
    tabContainer.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (tabContainer);

    developerBox.setText ("");
    developerBox.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (developerBox);

    developerToggle.setButtonText ("Developer");
    developerToggle.setClickingTogglesState (true);
    addAndMakeVisible (developerToggle);

    addAndMakeVisible (correctionDisplay);

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

    modeBox.addItem ("Influencer", 1);
    modeBox.addItem ("Virtuoso", 2);
    modeBox.addItem ("Actualiser", 3);
    modeBox.setSelectedId (1, juce::dontSendNotification);
    modeBox.setEnabled (false);
    modeBox.setJustificationType (juce::Justification::centredLeft);
    modeBox.setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    modeBox.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    modeBox.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    modeBox.setColour (juce::ComboBox::arrowColourId, juce::Colours::white.withAlpha (0.7f));
    addAndMakeVisible (modeBox);

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
    referenceBox.setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    referenceBox.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    referenceBox.setColour (juce::ComboBox::arrowColourId, juce::Colours::white.withAlpha (0.7f));

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
            referenceStatusLabel.setText ("", juce::dontSendNotification);
            referenceLoadedIndicator.setActive (true);
        }
        else
        {
            referenceStatusLabel.setText ("Load failed: " + errorMessage, juce::dontSendNotification);
            referenceLoadedIndicator.setActive (false);
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
                referenceStatusLabel.setText ("", juce::dontSendNotification);
                referenceLoadedIndicator.setActive (true);
                break;
            }
        }
    }

    if (! referenceFiles.isEmpty() && referenceStatusLabel.getText().isEmpty())
        referenceStatusLabel.setText ("No reference loaded.", juce::dontSendNotification);
    if (currentPath.isEmpty())
        referenceLoadedIndicator.setActive (false);

    slackLabel.setText ("Slack (ms)", juce::dontSendNotification);
    slackLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (slackLabel);

    slackSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slackSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible (slackSlider);

    clusterWindowLabel.setText ("Cluster Window (ms)", juce::dontSendNotification);
    clusterWindowLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (clusterWindowLabel);

    clusterWindowSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    clusterWindowSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
    clusterWindowSlider.setRange (20.0, 1000.0, 1.0);
    addAndMakeVisible (clusterWindowSlider);

    correctionLabel.setText ("Influence", juce::dontSendNotification);
    correctionLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (correctionLabel);

    correctionSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    correctionSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    correctionSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (value * 100.0, 0);
    };
    correctionSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        return juce::jlimit (0.0, 1.0, text.getDoubleValue() / 100.0);
    };
    correctionSlider.setTextValueSuffix ("%");
    correctionSlider.setColour (juce::Slider::trackColourId, juce::Colour (0x55d8d8d8));
    correctionSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff7f8cff));
    addAndMakeVisible (correctionSlider);

    inputLabel.setText ("IN", juce::dontSendNotification);
    inputLabel.setJustificationType (juce::Justification::centred);
    inputLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0x80233149));
    inputLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    inputLabel.setColour (juce::Label::outlineColourId, juce::Colour (0x40294d6b));
    addAndMakeVisible (inputLabel);

    addAndMakeVisible (inputIndicator);

    outputLabel.setText ("OUT", juce::dontSendNotification);
    outputLabel.setJustificationType (juce::Justification::centred);
    outputLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0x80233149));
    outputLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    outputLabel.setColour (juce::Label::outlineColourId, juce::Colour (0x40294d6b));
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

    refIoiLabel.setText ("Ref IOI (min/med ms)", juce::dontSendNotification);
    refIoiLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (refIoiLabel);

    refIoiValueLabel.setText ("-- / --", juce::dontSendNotification);
    refIoiValueLabel.setJustificationType (juce::Justification::centredLeft);
    refIoiValueLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (refIoiValueLabel);

    startOffsetLabel.setText ("Start Offset", juce::dontSendNotification);
    startOffsetLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (startOffsetLabel);

    startOffsetValueLabel.setText ("Not captured", juce::dontSendNotification);
    startOffsetValueLabel.setJustificationType (juce::Justification::centredLeft);
    startOffsetValueLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (startOffsetValueLabel);

    resetStartOffsetButton.setButtonText ("Reset Start Offset");
    addAndMakeVisible (resetStartOffsetButton);

    copyLogButton.setButtonText ("Copy Miss Log");
    addAndMakeVisible (copyLogButton);

    tempoShiftButton.setButtonText ("Tempo -1 Bar");
    tempoShiftButton.setClickingTogglesState (true);
    addAndMakeVisible (tempoShiftButton);

    velocityButton.setButtonText ("Vel Corr");
    velocityButton.setClickingTogglesState (true);
    addAndMakeVisible (velocityButton);

    muteButton.setButtonText ("Mute");
    muteButton.setClickingTogglesState (true);
    muteButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3e4046));
    muteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff767d87));
    muteButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible (muteButton);

    bypassButton.setButtonText ("Bypass");
    bypassButton.setClickingTogglesState (true);
    bypassButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff782b2f));
    bypassButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffa3393f));
    bypassButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible (bypassButton);

    expandButton.onClick = [this]
    {
        isExpanded = true;
        expandButton.setExpanded (isExpanded);
        updateUiVisibility();
        resized();
        repaint();
    };
    expandButton.setExpanded (isExpanded);
    addAndMakeVisible (expandButton);

    slackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, kParamDelayMs, slackSlider);
    clusterWindowAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, kParamClusterWindowMs, clusterWindowSlider);
    correctionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, kParamCorrection, correctionSlider);
    velocityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, kParamVelocityCorrection, velocityButton);
    muteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, kParamMute, muteButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, kParamBypass, bypassButton);
    tempoShiftAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, kParamTempoShiftBackBar, tempoShiftButton);

    developerToggle.setToggleState (false, juce::dontSendNotification);
    developerToggle.onClick = [this]()
    {
        updateUiVisibility();
        resized();
    };

    auto applyClusterWindow = [this]
    {
        if (processor.isTransportPlaying())
        {
            referenceStatusLabel.setText ("Stop transport to update cluster window.",
                juce::dontSendNotification);
            return;
        }

        if (processor.getReferencePath().isEmpty())
        {
            referenceStatusLabel.setText ("Load a personality to update cluster window.",
                juce::dontSendNotification);
            return;
        }

        juce::String errorMessage;
        if (processor.rebuildReferenceClusters (static_cast<float> (clusterWindowSlider.getValue()), errorMessage))
        {
            referenceStatusLabel.setText ("Cluster window updated.",
                juce::dontSendNotification);
        }
        else
        {
            referenceStatusLabel.setText ("Cluster update failed: " + errorMessage,
                juce::dontSendNotification);
        }
    };

    clusterWindowSlider.onDragEnd = applyClusterWindow;
    clusterWindowSlider.onValueChange = [this, applyClusterWindow]
    {
        if (! clusterWindowSlider.isMouseButtonDown())
            applyClusterWindow();
    };

    resetStartOffsetButton.onClick = [this]
    {
        if (processor.isTransportPlaying())
        {
            referenceStatusLabel.setText ("Stop transport to reset start offset.",
                juce::dontSendNotification);
            return;
        }

        processor.requestStartOffsetReset();
        lastStartOffsetValid = false;
        lastStartOffsetMs = 0.0f;
        lastStartOffsetBars = 0.0f;
        startOffsetValueLabel.setText ("Not captured", juce::dontSendNotification);
        referenceStatusLabel.setText ("Start offset reset.",
            juce::dontSendNotification);
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

    lastRefIoiMinMs = processor.getReferenceIoiMinMs();
    lastRefIoiMedianMs = processor.getReferenceIoiMedianMs();
    auto formatIoi = [] (float value)
    {
        return (value > 0.0f) ? juce::String (value, 1) : juce::String ("--");
    };
    refIoiValueLabel.setText (formatIoi (lastRefIoiMinMs) + " / " + formatIoi (lastRefIoiMedianMs),
        juce::dontSendNotification);

    lastStartOffsetValid = processor.hasStartOffset();
    if (lastStartOffsetValid)
    {
        lastStartOffsetMs = processor.getStartOffsetMs();
        lastStartOffsetBars = processor.getStartOffsetBars();
        const juce::String signPrefix = (lastStartOffsetBars >= 0.0f) ? "+" : "";
        startOffsetValueLabel.setText (signPrefix + juce::String (lastStartOffsetBars, 2)
                + " bars (" + juce::String (lastStartOffsetMs, 0) + " ms)",
            juce::dontSendNotification);
    }

    resetStartOffsetButton.setEnabled (! lastTransportPlaying);
    copyLogButton.setEnabled (! lastTransportPlaying);
    clusterWindowSlider.setEnabled (! lastTransportPlaying);

    correctionDisplay.setMinimalStyle (true);
    updateUiVisibility();

    tabContainer.toBack();
    developerBox.toBack();

    startTimerHz (30);

    setSize (kUiBaseWidth, kUiBaseHeight);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    const auto& background = isExpanded ? backgroundOpen : backgroundClosed;
    if (background.isValid())
        g.drawImage (background, getLocalBounds().toFloat());
}

void PluginEditor::resized()
{
    const float scaleX = static_cast<float> (getWidth()) / static_cast<float> (kUiBaseWidth);
    const float scaleY = static_cast<float> (getHeight()) / static_cast<float> (kUiBaseHeight);

    auto scaleRect = [&](int x, int y, int w, int h)
    {
        return juce::Rectangle<int> (juce::roundToInt (x * scaleX),
                                     juce::roundToInt (y * scaleY),
                                     juce::roundToInt (w * scaleX),
                                     juce::roundToInt (h * scaleY));
    };

    constexpr int leftPanelX = 10;
    constexpr int leftPanelY = 161;
    constexpr int leftPanelW = 250;
    constexpr int leftPanelH = 278;
    constexpr int rightPanelX = 268;
    constexpr int rightPanelY = 161;
    constexpr int rightPanelW = 409;
    constexpr int rightPanelH = 278;

    if (! isExpanded)
    {
        const int buttonSize = juce::roundToInt (44.0f * (scaleX + scaleY) * 0.5f);
        expandButton.setBounds (getLocalBounds().withSizeKeepingCentre (buttonSize, buttonSize));
    }

    muteButton.setBounds (scaleRect (502, 28, 46, 18));
    bypassButton.setBounds (scaleRect (502, 52, 46, 18));
    modeBox.setBounds (scaleRect (556, 34, 120, 24));
    inputLabel.setBounds (scaleRect (684, 28, 24, 18));
    inputIndicator.setBounds (scaleRect (672, 32, 8, 8));
    outputLabel.setBounds (scaleRect (684, 52, 24, 18));
    outputIndicator.setBounds (scaleRect (672, 56, 8, 8));

    referenceBox.setBounds (scaleRect (leftPanelX + 72, leftPanelY + 168, 160, 22));
    correctionSlider.setBounds (scaleRect (leftPanelX + 18, leftPanelY + 214, 214, 16));

    correctionDisplay.setBounds (scaleRect (rightPanelX + 16, rightPanelY + 12,
        rightPanelW - 32, rightPanelH - 24));

    buildInfoLabel.setBounds (scaleRect (360, 485, 340, 16));
}

void PluginEditor::updateUiVisibility()
{
    const bool showDeveloper = developerToggle.getToggleState();

    expandButton.setVisible (! isExpanded);
    referenceBox.setVisible (isExpanded);
    correctionSlider.setVisible (isExpanded);
    correctionDisplay.setVisible (isExpanded && ! showDeveloper);

    referenceLabel.setVisible (false);
    correctionLabel.setVisible (false);
    referenceLoadedIndicator.setVisible (false);
    tabContainer.setVisible (false);
    developerBox.setVisible (false);
    developerToggle.setVisible (false);
    virtuosoTabButton.setVisible (false);
    influencerTabButton.setVisible (false);
    actualiserTabButton.setVisible (false);

    referenceStatusLabel.setVisible (isExpanded && showDeveloper);
    slackLabel.setVisible (isExpanded && showDeveloper);
    slackSlider.setVisible (isExpanded && showDeveloper);
    clusterWindowLabel.setVisible (isExpanded && showDeveloper);
    clusterWindowSlider.setVisible (isExpanded && showDeveloper);
    timingLabel.setVisible (isExpanded && showDeveloper);
    timingValueLabel.setVisible (isExpanded && showDeveloper);
    transportLabel.setVisible (isExpanded && showDeveloper);
    transportValueLabel.setVisible (isExpanded && showDeveloper);
    matchLabel.setVisible (isExpanded && showDeveloper);
    matchValueLabel.setVisible (isExpanded && showDeveloper);
    cpuLabel.setVisible (isExpanded && showDeveloper);
    cpuValueLabel.setVisible (isExpanded && showDeveloper);
    bpmLabel.setVisible (isExpanded && showDeveloper);
    bpmValueLabel.setVisible (isExpanded && showDeveloper);
    refIoiLabel.setVisible (isExpanded && showDeveloper);
    refIoiValueLabel.setVisible (isExpanded && showDeveloper);
    startOffsetLabel.setVisible (isExpanded && showDeveloper);
    startOffsetValueLabel.setVisible (isExpanded && showDeveloper);
    resetStartOffsetButton.setVisible (isExpanded && showDeveloper);
    copyLogButton.setVisible (isExpanded && showDeveloper);
    tempoShiftButton.setVisible (isExpanded && showDeveloper);
    velocityButton.setVisible (isExpanded && showDeveloper);

    buildInfoLabel.setVisible (false);
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

    correctionDisplay.setValues (processor.getLastTimingDeltaMs(),
        processor.getLastNoteOffDeltaMs(),
        processor.getLastVelocityDelta(),
        static_cast<float> (slackSlider.getValue()));

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
        resetStartOffsetButton.setEnabled (! isPlaying);
        copyLogButton.setEnabled (! isPlaying);
        clusterWindowSlider.setEnabled (! isPlaying);
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

    const float refIoiMinMs = processor.getReferenceIoiMinMs();
    const float refIoiMedianMs = processor.getReferenceIoiMedianMs();
    if (std::abs (refIoiMinMs - lastRefIoiMinMs) > 0.01f
        || std::abs (refIoiMedianMs - lastRefIoiMedianMs) > 0.01f)
    {
        lastRefIoiMinMs = refIoiMinMs;
        lastRefIoiMedianMs = refIoiMedianMs;
        auto formatIoi = [] (float value)
        {
            return (value > 0.0f) ? juce::String (value, 1) : juce::String ("--");
        };
        refIoiValueLabel.setText (formatIoi (refIoiMinMs) + " / " + formatIoi (refIoiMedianMs),
            juce::dontSendNotification);
    }

    const bool hasStartOffset = processor.hasStartOffset();
    if (! hasStartOffset)
    {
        if (lastStartOffsetValid)
        {
            lastStartOffsetValid = false;
            startOffsetValueLabel.setText ("Not captured", juce::dontSendNotification);
        }
    }
    else
    {
        const float offsetMs = processor.getStartOffsetMs();
        const float offsetBars = processor.getStartOffsetBars();
        if (! lastStartOffsetValid
            || std::abs (offsetMs - lastStartOffsetMs) > 0.5f
            || std::abs (offsetBars - lastStartOffsetBars) > 0.01f)
        {
            lastStartOffsetValid = true;
            lastStartOffsetMs = offsetMs;
            lastStartOffsetBars = offsetBars;
            const juce::String signPrefix = (offsetBars >= 0.0f) ? "+" : "";
            startOffsetValueLabel.setText (signPrefix + juce::String (offsetBars, 2)
                    + " bars (" + juce::String (offsetMs, 0) + " ms)",
                juce::dontSendNotification);
        }
    }
}
