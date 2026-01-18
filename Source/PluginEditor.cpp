#include "PluginEditor.h"
#include "BinaryData.h"
#include "PersonalitiesBuildInfo.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace
{
    constexpr int kUiBaseWidth = 720;
    constexpr int kUiBaseHeight = 509;
    constexpr float kOverlayAlpha = 0.45f;
    constexpr float kAssetScale = 0.5f;
    constexpr int kEffectStrengthMinX = 82;
    constexpr int kEffectStrengthMaxX = 410;
    constexpr int kEffectStrengthY = 818;
    constexpr int kMidiInX = 1368;
    constexpr int kMidiInY = 100;
    constexpr int kMidiOutX = 1368;
    constexpr int kMidiOutY = 148;
    constexpr int kMuteX = 1026;
    constexpr int kMuteY = 108;
    constexpr int kBypassX = 1026;
    constexpr int kBypassY = 144;
    constexpr int kModeSelectorX = 1188;
    constexpr int kModeSelectorY = 118;
    constexpr int kResetX = 20;
    constexpr int kResetY = 958;
    constexpr int kTooltipsX = 262;
    constexpr int kTooltipsY = 962;
    constexpr int kDeveloperIndicatorX = 1112;
    constexpr int kDeveloperIndicatorY = 36;
    constexpr int kDeveloperConsoleButtonX = 1338;
    constexpr int kDeveloperConsoleButtonY = 22;
    constexpr int kDeveloperConsoleX = 532;
    constexpr int kDeveloperConsoleY = 336;
    constexpr int kDeveloperConsoleW = 824;
    constexpr int kDeveloperConsoleH = 526;
    constexpr int kPianoRollX = 82;
    constexpr int kPianoRollY = 336;
    constexpr int kPianoRollW = 386;
    constexpr int kPianoRollH = 322;
    constexpr int kDeveloperConsolePadding = 20;
    constexpr int kBuildInfoX = 602;
    constexpr int kBuildInfoY = 160;
    constexpr float kDropdownFontSize = 11.0f;
    constexpr const char* kPreferredDisplayFontName = "SF Pro Display";
    constexpr const char* kFallbackDisplayFontName = ".SF NS Display";
    constexpr const char* kFallbackDisplayFontNameAlt = "SF Pro Text";
    constexpr double kDeveloperFadeDurationMs = 500.0;
    constexpr const char* kParamDelayMs = "delay_ms";
    constexpr const char* kParamClusterWindowMs = "match_window_ms";
    constexpr const char* kParamCorrection = "correction";
    constexpr const char* kParamMissingTimeoutMs = "missing_timeout_ms";
    constexpr const char* kParamExtraNoteBudget = "extra_note_budget";
    constexpr const char* kParamPitchTolerance = "pitch_tolerance";
    constexpr const char* kParamMute = "mute";
    constexpr const char* kParamBypass = "bypass";
    constexpr const char* kParamVelocityCorrection = "velocity_correction";

    const juce::String kChooseLabel = juce::String::fromUTF8 ("Choose\xe2\x80\xa6");

    const juce::String& getDisplayFontName()
    {
        static const juce::String name = []()
        {
            const auto names = juce::Font::findAllTypefaceNames();
            if (names.contains (kPreferredDisplayFontName))
                return juce::String (kPreferredDisplayFontName);
            if (names.contains (kFallbackDisplayFontName))
                return juce::String (kFallbackDisplayFontName);
            if (names.contains (kFallbackDisplayFontNameAlt))
                return juce::String (kFallbackDisplayFontNameAlt);
            return juce::Font::getDefaultSansSerifFontName();
        }();
        return name;
    }

    juce::Font makeDisplayFont (float size)
    {
        return juce::Font (juce::FontOptions (getDisplayFontName(), size, juce::Font::plain));
    }

    int measureTextWidth (const juce::Font& font, const juce::String& text)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (font, text, 0.0f, 0.0f);
        return static_cast<int> (std::ceil (glyphs.getBoundingBox (0, -1, true).getWidth()));
    }

    bool tryParseFloat (const juce::String& text, float& value)
    {
        const auto trimmed = text.trim();
        if (trimmed.isEmpty())
            return false;

        const auto* raw = trimmed.toRawUTF8();
        char* end = nullptr;
        const double parsed = std::strtod (raw, &end);
        if (end == raw)
            return false;

        while (*end != '\0')
        {
            if (! std::isspace (static_cast<unsigned char> (*end)))
                return false;
            ++end;
        }

        if (! std::isfinite (parsed))
            return false;

        value = static_cast<float> (parsed);
        return true;
    }
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

void PluginEditor::ImageIndicator::paint (juce::Graphics& g)
{
    const auto& image = active ? activeImage : inactiveImage;
    if (! image.isValid())
        return;

    g.drawImage (image, getLocalBounds().toFloat());
}

void PluginEditor::ImageIndicator::setActive (bool shouldBeActive)
{
    if (active == shouldBeActive)
        return;

    active = shouldBeActive;
    repaint();
}

void PluginEditor::ImageIndicator::setImages (juce::Image activeImageIn, juce::Image inactiveImageIn)
{
    activeImage = std::move (activeImageIn);
    inactiveImage = std::move (inactiveImageIn);
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

void PluginEditor::ExpandButton::setImage (juce::Image image)
{
    buttonImage = std::move (image);
    repaint();
}

void PluginEditor::ExpandButton::setKeyHandler (std::function<bool (const juce::KeyPress&)> handler)
{
    keyHandler = std::move (handler);
}

bool PluginEditor::ExpandButton::keyPressed (const juce::KeyPress& key)
{
    if (keyHandler && keyHandler (key))
        return true;

    return juce::Button::keyPressed (key);
}

void PluginEditor::DeveloperPanelBackdrop::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0f1118));
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (juce::Colour (0x402a2f3a));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 12.0f, 1.0f);
}

void PluginEditor::PianoRollComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty())
        return;

    if (debugOverlayEnabled)
    {
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawRect (bounds, 1.0f);
        g.setFont (makeDisplayFont (9.0f));
        juce::String info;
        if (statusMessage.isNotEmpty())
        {
            info = statusMessage;
        }
        else if (! referenceData)
        {
            info = "No reference data";
        }
        else if (referenceData->notes.empty())
        {
            info = "Reference notes: 0";
        }
        else if (hasPitchRange)
        {
            info = "Notes: " + juce::String (static_cast<int> (referenceData->notes.size()))
                + " Range: " + juce::String (minNote) + "-" + juce::String (maxNote);
        }
        else
        {
            info = "Reference range unavailable";
        }
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.drawText (info, bounds.reduced (4.0f), juce::Justification::topLeft, false);
    }

    if (! referenceData || referenceData->notes.empty() || ! hasPitchRange || sampleRate <= 0.0)
        return;

    g.reduceClipRegion (getLocalBounds());

    constexpr double windowSeconds = 5.0;
    const double windowSamples = sampleRate * windowSeconds;
    if (windowSamples <= 0.0)
        return;

    const double windowStartSample = static_cast<double> (nowSample) - windowSamples * 0.5;
    const int pitchSpan = juce::jmax (1, maxNote - minNote);
    const float noteHeight = bounds.getHeight() / static_cast<float> (pitchSpan + 1);

    auto sampleToX = [&](double sample)
    {
        const double normalised = (sample - windowStartSample) / windowSamples;
        return bounds.getX() + static_cast<float> (normalised * bounds.getWidth());
    };

    auto makeNoteRect = [&](int noteNumber, double startSample, double endSample)
    {
        const int clampedNote = juce::jlimit (minNote, maxNote, noteNumber);
        const int index = clampedNote - minNote;
        const float y = bounds.getBottom() - static_cast<float> (index + 1) * noteHeight;
        float x1 = sampleToX (startSample);
        float x2 = sampleToX (endSample);
        if (x2 < x1)
            std::swap (x1, x2);
        auto rect = juce::Rectangle<float> (x1, y, x2 - x1, noteHeight);
        if (rect.getWidth() < 1.0f)
            rect.setWidth (1.0f);
        return rect.getIntersection (bounds);
    };

    const juce::Colour referenceColour (0xffff6fa3);
    const juce::Colour userColour (0xff4dd1ff);
    const juce::Colour overlapColour (0xff555ed2);

    for (size_t i = 0; i < referenceData->notes.size(); ++i)
    {
        const auto& note = referenceData->notes[i];
        const double alignedOn = static_cast<double> (referenceTransportStartSample)
            + static_cast<double> (note.onSample)
            - static_cast<double> (referenceData->firstNoteSample);
        const double alignedOff = static_cast<double> (referenceTransportStartSample)
            + static_cast<double> (note.offSample)
            - static_cast<double> (referenceData->firstNoteSample);
        const auto rect = makeNoteRect (note.noteNumber, alignedOn, alignedOff);
        if (rect.isEmpty())
            continue;

        const bool matched = (i < referenceMatched.size()) && (referenceMatched[i] != 0);
        g.setColour (referenceColour);
        if (matched)
            g.fillRect (rect);
        else
            g.drawRect (rect, 1.0f);
    }

    for (const auto& note : userNotes)
    {
        const double offSample = note.isActive ? static_cast<double> (nowSample)
            : static_cast<double> (note.offSample);
        const auto rect = makeNoteRect (note.noteNumber,
            static_cast<double> (note.onSample),
            offSample);
        if (rect.isEmpty())
            continue;

        g.setColour (userColour);
        if (note.matched)
            g.fillRect (rect);
        else
            g.drawRect (rect, 1.0f);
    }

    for (const auto& note : userNotes)
    {
        if (! note.matched || note.refIndex < 0 || ! referenceData)
            continue;
        if (! juce::isPositiveAndBelow (note.refIndex, static_cast<int> (referenceData->notes.size())))
            continue;

        const auto& refNote = referenceData->notes[static_cast<size_t> (note.refIndex)];
        const double alignedOn = static_cast<double> (referenceTransportStartSample)
            + static_cast<double> (refNote.onSample)
            - static_cast<double> (referenceData->firstNoteSample);
        const double alignedOff = static_cast<double> (referenceTransportStartSample)
            + static_cast<double> (refNote.offSample)
            - static_cast<double> (referenceData->firstNoteSample);
        const auto refRect = makeNoteRect (refNote.noteNumber, alignedOn, alignedOff);
        const double userOff = note.isActive ? static_cast<double> (nowSample)
            : static_cast<double> (note.offSample);
        const auto userRect = makeNoteRect (note.noteNumber,
            static_cast<double> (note.onSample),
            userOff);
        const auto overlap = refRect.getIntersection (userRect);
        if (overlap.isEmpty())
            continue;

        g.setColour (overlapColour);
        g.fillRect (overlap);
    }
}

void PluginEditor::PianoRollComponent::setReferenceData (
    std::shared_ptr<const PluginProcessor::ReferenceDisplayData> data)
{
    if (referenceData == data)
        return;

    referenceData = std::move (data);
    referenceMatched.clear();
    userNotes.clear();
    orderCounter = 0;
    if (referenceData)
        referenceMatched.assign (referenceData->notes.size(), 0);
    rebuildPitchRange();
    repaint();
}

void PluginEditor::PianoRollComponent::addUiEvents (const std::vector<PluginProcessor::UiNoteEvent>& events)
{
    if (events.empty())
        return;

    for (const auto& event : events)
    {
        if (event.isNoteOn)
        {
            UserNote note;
            note.noteNumber = event.noteNumber;
            note.channel = event.channel;
            note.refIndex = event.refIndex;
            note.onSample = event.sample;
            note.offSample = event.sample;
            note.order = orderCounter++;
            note.isActive = true;
            note.matched = event.refIndex >= 0;
            userNotes.push_back (note);

            if (note.refIndex >= 0
                && juce::isPositiveAndBelow (note.refIndex, static_cast<int> (referenceMatched.size())))
            {
                referenceMatched[static_cast<size_t> (note.refIndex)] = 1;
            }
        }
        else
        {
            int matchIndex = -1;
            uint64_t oldestOrder = 0;
            for (size_t i = 0; i < userNotes.size(); ++i)
            {
                const auto& candidate = userNotes[i];
                if (! candidate.isActive)
                    continue;
                if (candidate.noteNumber != event.noteNumber || candidate.channel != event.channel)
                    continue;

                if (matchIndex < 0 || candidate.order < oldestOrder)
                {
                    matchIndex = static_cast<int> (i);
                    oldestOrder = candidate.order;
                }
            }

            if (matchIndex >= 0)
            {
                auto& note = userNotes[static_cast<size_t> (matchIndex)];
                note.isActive = false;
                note.offSample = event.sample;
            }
        }
    }

    pruneOldNotes();
    repaint();
}

void PluginEditor::PianoRollComponent::setTimeline (uint64_t nowSampleIn,
                                                    uint64_t referenceStartSampleIn,
                                                    double sampleRateIn)
{
    const bool sampleRateChanged = sampleRateIn > 0.0 && std::abs (sampleRateIn - sampleRate) > 0.01;
    const bool timeReset = nowSampleIn + 1 < lastNowSample;

    if (sampleRateIn > 0.0)
        sampleRate = sampleRateIn;

    nowSample = nowSampleIn;
    referenceTransportStartSample = referenceStartSampleIn;

    if (sampleRateChanged || timeReset)
        reset();

    lastNowSample = nowSample;
    pruneOldNotes();
}

void PluginEditor::PianoRollComponent::reset()
{
    userNotes.clear();
    orderCounter = 0;
    if (referenceData)
        referenceMatched.assign (referenceData->notes.size(), 0);
    else
        referenceMatched.clear();
    repaint();
}

void PluginEditor::PianoRollComponent::setDebugOverlayEnabled (bool shouldShow)
{
    if (debugOverlayEnabled == shouldShow)
        return;

    debugOverlayEnabled = shouldShow;
    repaint();
}

void PluginEditor::PianoRollComponent::setStatusMessage (juce::String message)
{
    if (statusMessage == message)
        return;

    statusMessage = std::move (message);
    repaint();
}

void PluginEditor::PianoRollComponent::rebuildPitchRange()
{
    hasPitchRange = false;
    minNote = 0;
    maxNote = 127;

    if (! referenceData || referenceData->notes.empty())
        return;

    minNote = 127;
    maxNote = 0;
    for (const auto& note : referenceData->notes)
    {
        minNote = juce::jmin (minNote, note.noteNumber);
        maxNote = juce::jmax (maxNote, note.noteNumber);
    }

    if (minNote <= maxNote)
        hasPitchRange = true;
}

void PluginEditor::PianoRollComponent::pruneOldNotes()
{
    if (sampleRate <= 0.0)
        return;

    constexpr double windowSeconds = 5.0;
    const uint64_t halfWindowSamples = static_cast<uint64_t> (std::llround (sampleRate * windowSeconds * 0.5));
    const uint64_t earliestSample = nowSample > halfWindowSamples
        ? nowSample - halfWindowSamples
        : 0;

    size_t writeIndex = 0;
    for (size_t i = 0; i < userNotes.size(); ++i)
    {
        const auto& note = userNotes[i];
        if (note.isActive || note.offSample >= earliestSample)
        {
            userNotes[writeIndex++] = note;
        }
    }

    if (writeIndex < userNotes.size())
        userNotes.resize (writeIndex);
}

void PluginEditor::ExpandButton::paintButton (juce::Graphics& g,
                                              bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown)
{
    if (! buttonImage.isValid())
        return;

    juce::Graphics::ScopedSaveState state (g);
    float opacity = 1.0f;
    if (shouldDrawButtonAsDown)
        opacity = 0.8f;
    else if (shouldDrawButtonAsHighlighted)
        opacity = 0.9f;
    g.setOpacity (opacity);
    g.drawImage (buttonImage, getLocalBounds().toFloat());
}

void PluginEditor::InfluenceSliderLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                                                 float sliderPos, float minSliderPos, float maxSliderPos,
                                                                 const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos,
                                                maxSliderPos, style, slider);
        return;
    }

    if (! handleImage.isValid())
    {
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos,
                                                maxSliderPos, style, slider);
        return;
    }

    juce::ignoreUnused (sliderPos, minSliderPos, maxSliderPos);

    const float handleWidth = handleImage.getWidth() * kAssetScale;
    const float handleHeight = handleImage.getHeight() * kAssetScale;
    const float range = juce::jmax (0.0f, static_cast<float> (width) - handleWidth);
    const float normalised = static_cast<float> (slider.valueToProportionOfLength (slider.getValue()));
    const float handleX = static_cast<float> (x) + range * normalised;
    const float handleY = static_cast<float> (y);

    juce::Graphics::ScopedSaveState state (g);
    if (! slider.isEnabled())
        g.setOpacity (0.5f);
    g.drawImage (handleImage, handleX, handleY, handleWidth, handleHeight,
                 0, 0, handleImage.getWidth(), handleImage.getHeight());

    const int percent = juce::jlimit (0, 100, static_cast<int> (std::lround (slider.getValue() * 100.0)));
    g.setColour (juce::Colours::white);
    g.setFont (makeDisplayFont (juce::jlimit (8.0f, 12.0f, handleHeight * 0.6f)));
    g.drawFittedText (juce::String (percent) + "%", juce::Rectangle<int> (
            juce::roundToInt (handleX),
            juce::roundToInt (handleY),
            juce::roundToInt (handleWidth),
            juce::roundToInt (handleHeight)),
        juce::Justification::centred, 1);
}

void PluginEditor::InfluenceSliderLookAndFeel::setHandleImage (juce::Image image)
{
    handleImage = std::move (image);
}

juce::Font PluginEditor::DropdownLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return makeDisplayFont (kDropdownFontSize);
}

juce::Font PluginEditor::DropdownLookAndFeel::getPopupMenuFont()
{
    return makeDisplayFont (kDropdownFontSize);
}

void PluginEditor::DropdownLookAndFeel::drawComboBox (juce::Graphics& g,
                                                      int width, int height, bool,
                                                      int, int, int, int,
                                                      juce::ComboBox& box)
{
    if (box.getComponentID() != "performerDropdown")
        return;

    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width),
        static_cast<float> (height));
    const auto arrowZone = bounds.withLeft (bounds.getRight() - bounds.getHeight());
    const float arrowWidth = arrowZone.getWidth() * 0.5f;
    const float arrowHeight = arrowZone.getHeight() * 0.3f;
    const float arrowX = arrowZone.getX() + (arrowZone.getWidth() - arrowWidth) * 0.5f;
    const float arrowY = arrowZone.getCentreY() - arrowHeight * 0.5f;

    juce::Path path;
    path.addTriangle (arrowX, arrowY,
        arrowX + arrowWidth, arrowY,
        arrowX + arrowWidth * 0.5f, arrowY + arrowHeight);

    g.setColour (box.findColour (juce::ComboBox::textColourId));
    g.fillPath (path);
}

juce::Label* PluginEditor::DropdownLookAndFeel::createComboBoxTextBox (juce::ComboBox& box)
{
    auto* label = new juce::Label();
    label->setJustificationType (box.getJustificationType());
    label->setFont (getComboBoxFont (box));
    label->setMinimumHorizontalScale (1.0f);
    return label;
}

void PluginEditor::DropdownLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (1, 1,
                     box.getWidth() - 2,
                     box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (box.getJustificationType());
    label.setMinimumHorizontalScale (1.0f);
}

PluginEditor::ImageToggleButton::ImageToggleButton()
    : juce::Button ("ImageToggle")
{
}

void PluginEditor::ImageToggleButton::setImages (juce::Image onImageIn, juce::Image offImageIn)
{
    onImage = std::move (onImageIn);
    offImage = std::move (offImageIn);
    repaint();
}

void PluginEditor::ImageToggleButton::paintButton (juce::Graphics& g,
                                                   bool shouldDrawButtonAsHighlighted,
                                                   bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    const auto& image = getToggleState() ? onImage : offImage;
    if (! image.isValid())
        return;

    juce::Graphics::ScopedSaveState state (g);
    if (! isEnabled())
        g.setOpacity (0.5f);
    g.drawImage (image, getLocalBounds().toFloat());
}

PluginEditor::ImageMomentaryButton::ImageMomentaryButton()
    : juce::Button ("ImageMomentary")
{
}

void PluginEditor::ImageMomentaryButton::setImage (juce::Image imageIn)
{
    image = std::move (imageIn);
    repaint();
}

void PluginEditor::ImageMomentaryButton::paintButton (juce::Graphics& g,
                                                      bool shouldDrawButtonAsHighlighted,
                                                      bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsHighlighted);

    if (! image.isValid())
        return;

    g.drawImage (image, getLocalBounds().toFloat());

    if (shouldDrawButtonAsDown)
    {
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.fillRect (getLocalBounds());
    }
}

PluginEditor::ImageCheckboxButton::ImageCheckboxButton()
    : juce::Button ("ImageCheckbox")
{
}

void PluginEditor::ImageCheckboxButton::setImages (juce::Image offImageIn, juce::Image onImageIn)
{
    offImage = std::move (offImageIn);
    onImage = std::move (onImageIn);
    repaint();
}

void PluginEditor::ImageCheckboxButton::paintButton (juce::Graphics& g,
                                                     bool shouldDrawButtonAsHighlighted,
                                                     bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    const auto& image = getToggleState() ? onImage : offImage;
    if (! image.isValid())
    {
        const auto& fallback = getToggleState() ? offImage : onImage;
        if (! fallback.isValid())
            return;
        g.drawImage (fallback, getLocalBounds().toFloat());
        return;
    }

    g.drawImage (image, getLocalBounds().toFloat());
}

PluginEditor::PluginEditor (PluginProcessor& p)
: juce::AudioProcessorEditor (&p), processor (p)
{
    juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypefaceName (getDisplayFontName());

    backgroundOpen = juce::ImageCache::getFromMemory (BinaryData::backgroundopenx0y0_png,
        BinaryData::backgroundopenx0y0_pngSize);
    backgroundClosed = juce::ImageCache::getFromMemory (BinaryData::backgroundclosedx0y0_png,
        BinaryData::backgroundclosedx0y0_pngSize);
    openButtonImage = juce::ImageCache::getFromMemory (BinaryData::buttonopennerx654y560_png,
        BinaryData::buttonopennerx654y560_pngSize);
    performerDropdownImage = juce::ImageCache::getFromMemory (
        BinaryData::dropdown_menuperformer_selectorx176y718_png,
        BinaryData::dropdown_menuperformer_selectorx176y718_pngSize);
    effectStrengthHandleImage = juce::ImageCache::getFromMemory (
        BinaryData::slidereffect_strengthx82y818_0x410y818_100_png,
        BinaryData::slidereffect_strengthx82y818_0x410y818_100_pngSize);
    muteOffImage = juce::ImageCache::getFromMemory (
        BinaryData::buttonmuteoffx1026y108_png,
        BinaryData::buttonmuteoffx1026y108_pngSize);
    muteOnImage = juce::ImageCache::getFromMemory (
        BinaryData::buttonmuteonx1026y108_png,
        BinaryData::buttonmuteonx1026y108_pngSize);
    bypassOffImage = juce::ImageCache::getFromMemory (
        BinaryData::buttonbypassoffx1026y144_png,
        BinaryData::buttonbypassoffx1026y144_pngSize);
    bypassOnImage = juce::ImageCache::getFromMemory (
        BinaryData::buttonbypassonx1026y144_png,
        BinaryData::buttonbypassonx1026y144_pngSize);
    modeDropdownImage = juce::ImageCache::getFromMemory (
        BinaryData::dropdownmodeselectorx1188y118_png,
        BinaryData::dropdownmodeselectorx1188y118_pngSize);
    developerModeIndicatorImage = juce::ImageCache::getFromMemory (
        BinaryData::indicatordeveloper_modex1112y36_png,
        BinaryData::indicatordeveloper_modex1112y36_pngSize);
    developerConsoleButtonOffImage = juce::ImageCache::getFromMemory (
        BinaryData::buttonopen_developer_consoleoffx1338y22_png,
        BinaryData::buttonopen_developer_consoleoffx1338y22_pngSize);
    developerConsoleButtonOnImage = juce::ImageCache::getFromMemory (
        BinaryData::buttonopen_developer_consoleonx1338y22_png,
        BinaryData::buttonopen_developer_consoleonx1338y22_pngSize);
    resetButtonImage = juce::ImageCache::getFromMemory (
        BinaryData::buttonresetx20y958_png,
        BinaryData::buttonresetx20y958_pngSize);
    tooltipsCheckboxOffImage = juce::ImageCache::getFromMemory (
        BinaryData::checkboxtooltipsoffx262y962_png,
        BinaryData::checkboxtooltipsoffx262y962_pngSize);
    tooltipsCheckboxOnImage = juce::ImageCache::getFromMemory (
        BinaryData::checkboxtooltipsonx262y962_png,
        BinaryData::checkboxtooltipsonx262y962_pngSize);
    midiInInactiveImage = juce::ImageCache::getFromMemory (
        BinaryData::indicatormidi_ininactivex1368y100_png,
        BinaryData::indicatormidi_ininactivex1368y100_pngSize);
    midiInActiveImage = juce::ImageCache::getFromMemory (
        BinaryData::indicatormidi_inactivex1368y100_png,
        BinaryData::indicatormidi_inactivex1368y100_pngSize);
    midiOutInactiveImage = juce::ImageCache::getFromMemory (
        BinaryData::indicatormidi_outinactivex1368y148_png,
        BinaryData::indicatormidi_outinactivex1368y148_pngSize);
    midiOutActiveImage = juce::ImageCache::getFromMemory (
        BinaryData::indicatormidi_outactivex1368y148_png,
        BinaryData::indicatormidi_outactivex1368y148_pngSize);
    expandButton.setImage (openButtonImage);
    influenceSliderLookAndFeel.setHandleImage (effectStrengthHandleImage);

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

    developerPanelBackdrop.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (developerPanelBackdrop);

    addAndMakeVisible (correctionDisplay);
    advancedUserOptions.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (advancedUserOptions);

    referenceBox.setComponentID ("performerDropdown");
    referenceBox.setLookAndFeel (&dropdownLookAndFeel);
    addAndMakeVisible (referenceBox);
    rebuildReferenceList();

    modeBox.setLookAndFeel (&dropdownLookAndFeel);
    modeBox.clear();
    modeBox.addItem ("Naturaliser", 1);
    modeBox.addItem ("Virtuoso", 2);
    modeBox.addItem ("Influencer", 3);
    modeBox.addItem ("Actualiser", 4);
    modeBox.addItem ("Composer", 5);
    modeBox.setItemEnabled (1, false);
    modeBox.setItemEnabled (2, false);
    modeBox.setItemEnabled (3, true);
    modeBox.setItemEnabled (4, false);
    modeBox.setItemEnabled (5, false);
    modeBox.setTextWhenNothingSelected (kChooseLabel);
    modeBox.setSelectedId (0, juce::dontSendNotification);
    modeBox.setEnabled (true);
    modeBox.setJustificationType (juce::Justification::centredLeft);
    modeBox.setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    modeBox.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    modeBox.setColour (juce::ComboBox::textColourId, juce::Colours::lightgrey);
    modeBox.setColour (juce::ComboBox::arrowColourId, juce::Colours::transparentBlack);
    modeBox.onChange = [this]
    {
        const bool influencerSelected = (modeBox.getSelectedId() == 3);
        modeBox.setColour (juce::ComboBox::textColourId,
            influencerSelected ? juce::Colours::white : juce::Colours::lightgrey);
    };
    addAndMakeVisible (modeBox);

    referenceBox.onChange = [this]
    {
        const int selectedId = referenceBox.getSelectedId();
        if (selectedId <= 0)
            return;

        referenceBox.setColour (juce::ComboBox::textColourId, juce::Colour (0xff555ed2));

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
                referenceBox.setColour (juce::ComboBox::textColourId, juce::Colour (0xff555ed2));
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

    configureNumberEntry (slackEntry);
    slackEntry.onTextChange = [this]()
    {
        commitNumberEntry (slackEntry, kParamDelayMs);
    };
    addAndMakeVisible (slackEntry);

    clusterWindowLabel.setText ("Cluster Window (ms)", juce::dontSendNotification);
    clusterWindowLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (clusterWindowLabel);

    configureNumberEntry (clusterWindowEntry);
    addAndMakeVisible (clusterWindowEntry);

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
    correctionSlider.setLookAndFeel (&influenceSliderLookAndFeel);
    addAndMakeVisible (correctionSlider);

    missingTimeoutLabel.setText ("Missing Timeout (ms)", juce::dontSendNotification);
    missingTimeoutLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (missingTimeoutLabel);

    configureNumberEntry (missingTimeoutEntry);
    missingTimeoutEntry.onTextChange = [this]()
    {
        commitNumberEntry (missingTimeoutEntry, kParamMissingTimeoutMs);
    };
    addAndMakeVisible (missingTimeoutEntry);

    extraNoteBudgetLabel.setText ("Extra Note Budget", juce::dontSendNotification);
    extraNoteBudgetLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (extraNoteBudgetLabel);

    configureNumberEntry (extraNoteBudgetEntry);
    extraNoteBudgetEntry.onTextChange = [this]()
    {
        commitNumberEntry (extraNoteBudgetEntry, kParamExtraNoteBudget);
    };
    addAndMakeVisible (extraNoteBudgetEntry);

    pitchToleranceLabel.setText ("Pitch Tolerance (st)", juce::dontSendNotification);
    pitchToleranceLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (pitchToleranceLabel);

    configureNumberEntry (pitchToleranceEntry);
    pitchToleranceEntry.onTextChange = [this]()
    {
        commitNumberEntry (pitchToleranceEntry, kParamPitchTolerance);
    };
    addAndMakeVisible (pitchToleranceEntry);

    inputIndicator.setImages (midiInActiveImage, midiInInactiveImage);
    outputIndicator.setImages (midiOutActiveImage, midiOutInactiveImage);
    addAndMakeVisible (inputIndicator);
    addAndMakeVisible (outputIndicator);

    timingLabel.setText ("Timing (ms)", juce::dontSendNotification);
    timingLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (timingLabel);

    timingValueLabel.setText ("0.00", juce::dontSendNotification);
    timingValueLabel.setJustificationType (juce::Justification::centred);
    timingValueLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (timingValueLabel);

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

    velocityButton.setButtonText ("Vel Corr");
    velocityButton.setClickingTogglesState (true);
    addAndMakeVisible (velocityButton);

    resetStartOffsetButton.setButtonText ("Reset Start Offset");
    addAndMakeVisible (resetStartOffsetButton);

    copyLogButton.setButtonText ("Copy Miss Log");
    addAndMakeVisible (copyLogButton);

    muteButton.setClickingTogglesState (true);
    muteButton.setImages (muteOnImage, muteOffImage);
    addAndMakeVisible (muteButton);

    bypassButton.setClickingTogglesState (true);
    bypassButton.setImages (bypassOnImage, bypassOffImage);
    addAndMakeVisible (bypassButton);

    resetButton.setImage (resetButtonImage);
    resetButton.onClick = [this]
    {
        resetPluginState();
    };
    addAndMakeVisible (resetButton);

    tooltipsCheckbox.setClickingTogglesState (true);
    tooltipsCheckbox.setToggleState (false, juce::dontSendNotification);
    tooltipsCheckbox.setImages (tooltipsCheckboxOffImage, tooltipsCheckboxOnImage);
    addAndMakeVisible (tooltipsCheckbox);

    developerConsoleButton.setImages (developerConsoleButtonOnImage, developerConsoleButtonOffImage);
    developerConsoleButton.setClickingTogglesState (true);
    developerConsoleButton.setToggleState (false, juce::dontSendNotification);
    developerConsoleButton.setAlpha (0.0f);
    developerConsoleButton.setVisible (false);
    developerConsoleButton.onClick = [this]()
    {
        setDeveloperConsoleOpen (developerConsoleButton.getToggleState());
    };
    addAndMakeVisible (developerConsoleButton);

    expandButton.onClick = [this]
    {
        isExpanded = true;
        expandButton.setExpanded (isExpanded);
        updateUiVisibility();
        resized();
        repaint();
    };
    expandButton.setKeyHandler ([this] (const juce::KeyPress& key)
    {
        return handleDeveloperShortcut (key);
    });
    expandButton.setExpanded (isExpanded);
    addAndMakeVisible (expandButton);

    correctionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, kParamCorrection, correctionSlider);
    velocityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, kParamVelocityCorrection, velocityButton);
    muteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, kParamMute, muteButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, kParamBypass, bypassButton);

    clusterWindowEntry.onTextChange = [this]
    {
        commitNumberEntry (clusterWindowEntry, kParamClusterWindowMs);
        if (auto* value = processor.apvts.getRawParameterValue (kParamClusterWindowMs))
            lastClusterWindowMs = value->load();
        applyClusterWindowFromUi();
    };

    syncNumberEntry (slackEntry, kParamDelayMs);
    syncNumberEntry (clusterWindowEntry, kParamClusterWindowMs);
    syncNumberEntry (missingTimeoutEntry, kParamMissingTimeoutMs);
    syncNumberEntry (extraNoteBudgetEntry, kParamExtraNoteBudget);
    syncNumberEntry (pitchToleranceEntry, kParamPitchTolerance);
    if (auto* value = processor.apvts.getRawParameterValue (kParamClusterWindowMs))
        lastClusterWindowMs = value->load();

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
    buildInfoLabel.setJustificationType (juce::Justification::centredLeft);
    buildInfoLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    buildInfoLabel.setFont (makeDisplayFont (10.0f));
    addAndMakeVisible (buildInfoLabel);

    lastInputNoteOnCounter = processor.getInputNoteOnCounter();
    lastInputFlashMs = juce::Time::getMillisecondCounterHiRes();
    lastOutputNoteOnCounter = processor.getOutputNoteOnCounter();
    lastOutputFlashMs = lastInputFlashMs;
    lastDeveloperFadeMs = lastInputFlashMs;
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
    clusterWindowEntry.setEnabled (! lastTransportPlaying);

    correctionDisplay.setMinimalStyle (true);
    advancedUserOptions.setDebugOverlayEnabled (boundsOverlayEnabled);
    updateUiVisibility();

    tabContainer.toBack();
    developerBox.toBack();
    developerPanelBackdrop.toBack();

    startTimerHz (30);

    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    setSize (kUiBaseWidth, kUiBaseHeight);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    const auto& background = isExpanded ? backgroundOpen : backgroundClosed;
    if (background.isValid())
        g.drawImage (background, getLocalBounds().toFloat());

    if (modeDropdownImage.isValid())
    {
        const auto dest = juce::Rectangle<float> (
            kModeSelectorX * kAssetScale,
            kModeSelectorY * kAssetScale,
            modeDropdownImage.getWidth() * kAssetScale,
            modeDropdownImage.getHeight() * kAssetScale);
        g.drawImage (modeDropdownImage, dest);
    }

    if (isExpanded && developerModeIndicatorImage.isValid() && developerModeAlpha > 0.0f)
    {
        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (developerModeAlpha);
        const auto dest = juce::Rectangle<float> (
            kDeveloperIndicatorX * kAssetScale,
            kDeveloperIndicatorY * kAssetScale,
            developerModeIndicatorImage.getWidth() * kAssetScale,
            developerModeIndicatorImage.getHeight() * kAssetScale);
        g.drawImage (developerModeIndicatorImage, dest);
    }

    if (isExpanded && performerDropdownImage.isValid())
    {
        const auto dest = juce::Rectangle<float> (
            176.0f * kAssetScale,
            718.0f * kAssetScale,
            performerDropdownImage.getWidth() * kAssetScale,
            performerDropdownImage.getHeight() * kAssetScale);
        g.drawImage (performerDropdownImage, dest);
    }
}

void PluginEditor::paintOverChildren (juce::Graphics& g)
{
    if (overlayEnabled && isExpanded && backgroundOpen.isValid())
    {
        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (kOverlayAlpha);
        g.drawImage (backgroundOpen, getLocalBounds().toFloat());
    }

    if (! boundsOverlayEnabled)
        return;

    juce::Graphics::ScopedSaveState state (g);
    g.setFont (makeDisplayFont (12.0f));

    auto getTextWidth = [&](const juce::String& text)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (g.getCurrentFont(), text, 0.0f, 0.0f);
        return static_cast<int> (std::ceil (glyphs.getBoundingBox (0, -1, true).getWidth()));
    };

    auto drawLabel = [&](const juce::String& text, int x, int y)
    {
        const int padding = 2;
        const int height = 14;
        const int width = getTextWidth (text) + padding * 2;
        int drawX = x;
        int drawY = y - height;
        if (drawY < 0)
            drawY = y + 2;
        if (drawX + width > getWidth())
            drawX = getWidth() - width;
        if (drawX < 0)
            drawX = 0;

        const juce::Rectangle<int> box (drawX, drawY, width, height);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRect (box);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawText (text, box, juce::Justification::centredLeft, false);
    };

    auto drawBounds = [&](juce::Component& comp, const char* name)
    {
        if (! comp.isVisible())
            return;

        const auto bounds = comp.getBounds();
        if (bounds.isEmpty())
            return;

        g.setColour (juce::Colours::aqua.withAlpha (0.6f));
        g.drawRect (bounds, 1.0f);

        const juce::String label = juce::String (name) + " "
            + juce::String (bounds.getX()) + "," + juce::String (bounds.getY())
            + " " + juce::String (bounds.getWidth()) + "x" + juce::String (bounds.getHeight());
        drawLabel (label, bounds.getX(), bounds.getY());
    };

    drawBounds (referenceBox, "referenceBox");
    drawBounds (correctionSlider, "correctionSlider");
    drawBounds (correctionDisplay, "correctionDisplay");
    drawBounds (advancedUserOptions, "advanced_user_options");
    drawBounds (developerPanelBackdrop, "developerPanelBackdrop");
    drawBounds (expandButton, "expandButton");
    drawBounds (slackEntry, "slackEntry");
    drawBounds (slackLabel, "slackLabel");
    drawBounds (clusterWindowEntry, "clusterWindowEntry");
    drawBounds (clusterWindowLabel, "clusterWindowLabel");
    drawBounds (missingTimeoutEntry, "missingTimeoutEntry");
    drawBounds (missingTimeoutLabel, "missingTimeoutLabel");
    drawBounds (extraNoteBudgetEntry, "extraNoteBudgetEntry");
    drawBounds (extraNoteBudgetLabel, "extraNoteBudgetLabel");
    drawBounds (pitchToleranceEntry, "pitchToleranceEntry");
    drawBounds (pitchToleranceLabel, "pitchToleranceLabel");
    drawBounds (velocityButton, "velocityButton");
    drawBounds (resetStartOffsetButton, "resetStartOffsetButton");
    drawBounds (copyLogButton, "copyLogButton");
    drawBounds (referenceStatusLabel, "referenceStatusLabel");
    drawBounds (timingLabel, "timingLabel");
    drawBounds (timingValueLabel, "timingValueLabel");
    drawBounds (matchLabel, "matchLabel");
    drawBounds (matchValueLabel, "matchValueLabel");
    drawBounds (cpuLabel, "cpuLabel");
    drawBounds (cpuValueLabel, "cpuValueLabel");
    drawBounds (bpmLabel, "bpmLabel");
    drawBounds (bpmValueLabel, "bpmValueLabel");
    drawBounds (refIoiLabel, "refIoiLabel");
    drawBounds (refIoiValueLabel, "refIoiValueLabel");
    drawBounds (startOffsetLabel, "startOffsetLabel");
    drawBounds (startOffsetValueLabel, "startOffsetValueLabel");
    drawBounds (muteButton, "muteButton");
    drawBounds (bypassButton, "bypassButton");
    drawBounds (resetButton, "resetButton");
    drawBounds (tooltipsCheckbox, "tooltipsCheckbox");
    drawBounds (developerConsoleButton, "developerConsoleButton");
    drawBounds (modeBox, "modeBox");
    drawBounds (inputIndicator, "inputIndicator");
    drawBounds (outputIndicator, "outputIndicator");
    drawBounds (buildInfoLabel, "buildInfoLabel");

    if (hasMousePosition)
    {
        const int x = lastMousePosition.x;
        const int y = lastMousePosition.y;
        g.setColour (juce::Colours::yellow.withAlpha (0.8f));
        g.drawLine (static_cast<float> (x - 6), static_cast<float> (y),
            static_cast<float> (x + 6), static_cast<float> (y), 1.0f);
        g.drawLine (static_cast<float> (x), static_cast<float> (y - 6),
            static_cast<float> (x), static_cast<float> (y + 6), 1.0f);
        drawLabel ("mouse " + juce::String (x) + "," + juce::String (y), x + 8, y + 8);
    }
}

void PluginEditor::resized()
{
    const float scaleX = 1.0f;
    const float scaleY = 1.0f;

    auto scaleRect = [&](int x, int y, int w, int h)
    {
        return juce::Rectangle<int> (juce::roundToInt (x * scaleX),
                                     juce::roundToInt (y * scaleY),
                                     juce::roundToInt (w * scaleX),
                                     juce::roundToInt (h * scaleY));
    };
    auto assetRect = [&](int x, int y, int w, int h)
    {
        return scaleRect (juce::roundToInt (x * kAssetScale),
                          juce::roundToInt (y * kAssetScale),
                          juce::roundToInt (w * kAssetScale),
                          juce::roundToInt (h * kAssetScale));
    };

    constexpr int leftPanelX = 10;
    constexpr int leftPanelY = 161;
    constexpr int leftPanelW = 250;
    constexpr int leftPanelH = 278;
    constexpr int rightPanelX = 268;
    constexpr int rightPanelY = 161;
    constexpr int rightPanelW = 409;
    constexpr int rightPanelH = 278;

    if (! isExpanded && openButtonImage.isValid())
    {
        expandButton.setBounds (assetRect (654, 560,
            openButtonImage.getWidth(), openButtonImage.getHeight()));
    }

    if (muteOffImage.isValid())
    {
        muteButton.setBounds (assetRect (kMuteX, kMuteY,
            muteOffImage.getWidth(), muteOffImage.getHeight()));
    }
    else
    {
        muteButton.setBounds (scaleRect (512, 53, 29, 15));
    }

    if (bypassOffImage.isValid())
    {
        bypassButton.setBounds (assetRect (kBypassX, kBypassY,
            bypassOffImage.getWidth(), bypassOffImage.getHeight()));
    }
    else
    {
        bypassButton.setBounds (scaleRect (512, 72, 29, 15));
    }

    if (resetButtonImage.isValid())
    {
        resetButton.setBounds (assetRect (kResetX, kResetY,
            resetButtonImage.getWidth(), resetButtonImage.getHeight()));
    }

    if (tooltipsCheckboxOffImage.isValid())
    {
        tooltipsCheckbox.setBounds (assetRect (kTooltipsX, kTooltipsY,
            tooltipsCheckboxOffImage.getWidth(), tooltipsCheckboxOffImage.getHeight()));
    }
    if (modeDropdownImage.isValid())
    {
        modeBox.setBounds (assetRect (kModeSelectorX, kModeSelectorY,
            modeDropdownImage.getWidth(), modeDropdownImage.getHeight()));
    }
    else
    {
        modeBox.setBounds (scaleRect (590, 60, 76, 20));
    }
    if (developerConsoleButtonOffImage.isValid())
    {
        developerConsoleButton.setBounds (assetRect (kDeveloperConsoleButtonX, kDeveloperConsoleButtonY,
            developerConsoleButtonOffImage.getWidth(), developerConsoleButtonOffImage.getHeight()));
    }
    if (midiInInactiveImage.isValid())
    {
        inputIndicator.setBounds (assetRect (kMidiInX, kMidiInY,
            midiInInactiveImage.getWidth(), midiInInactiveImage.getHeight()));
    }
    else
    {
        inputIndicator.setBounds (scaleRect (681, 50, 20, 17));
    }
    if (midiOutInactiveImage.isValid())
    {
        outputIndicator.setBounds (assetRect (kMidiOutX, kMidiOutY,
            midiOutInactiveImage.getWidth(), midiOutInactiveImage.getHeight()));
    }
    else
    {
        outputIndicator.setBounds (scaleRect (681, 72, 20, 17));
    }

    if (performerDropdownImage.isValid())
    {
        referenceBox.setBounds (assetRect (176, 718,
            performerDropdownImage.getWidth(),
            performerDropdownImage.getHeight()));
    }
    else
    {
        referenceBox.setBounds (scaleRect (leftPanelX + 72, leftPanelY + 168, 160, 22));
    }
    if (effectStrengthHandleImage.isValid())
    {
        const int handleWidth = effectStrengthHandleImage.getWidth();
        const int handleHeight = effectStrengthHandleImage.getHeight();
        const int rangeWidth = kEffectStrengthMaxX - kEffectStrengthMinX;
        correctionSlider.setBounds (assetRect (kEffectStrengthMinX, kEffectStrengthY,
            rangeWidth + handleWidth, handleHeight));
    }
    else
    {
        correctionSlider.setBounds (scaleRect (leftPanelX + 31, leftPanelY + 246, 193, 16));
    }

    correctionDisplay.setBounds (scaleRect (rightPanelX + 16, rightPanelY + 12,
        rightPanelW - 32, rightPanelH - 24));
    advancedUserOptions.setBounds (assetRect (kPianoRollX, kPianoRollY, kPianoRollW, kPianoRollH));

    const auto devPanelBounds = assetRect (kDeveloperConsoleX, kDeveloperConsoleY,
        kDeveloperConsoleW, kDeveloperConsoleH);
    developerPanelBackdrop.setBounds (devPanelBounds);

    const int padding = juce::roundToInt (kDeveloperConsolePadding * kAssetScale);
    const auto contentBounds = devPanelBounds.reduced (padding);

    const int rowHeight = juce::roundToInt (18.0f * scaleY);
    const int rowGap = juce::roundToInt (6.0f * scaleY);
    const int columnGap = juce::roundToInt (12.0f * scaleX);
    const int columnWidth = (contentBounds.getWidth() - columnGap) / 2;
    const int sliderGap = juce::roundToInt (8.0f * scaleX);
    const int labelWidth = juce::jlimit (60,
        columnWidth - sliderGap - 40,
        juce::roundToInt (120.0f * scaleX));

    referenceStatusLabel.setBounds (contentBounds.getX(), contentBounds.getY(),
        contentBounds.getWidth(), rowHeight);

    int leftY = contentBounds.getY() + rowHeight + rowGap;
    int rightY = leftY;
    const int leftX = contentBounds.getX();
    const int rightX = contentBounds.getX() + columnWidth + columnGap;

    auto placeEntryRow = [&](juce::Label& label, juce::Label& entry)
    {
        label.setBounds (leftX, leftY, labelWidth, rowHeight);
        entry.setBounds (leftX + labelWidth + sliderGap, leftY,
            columnWidth - labelWidth - sliderGap, rowHeight);
        leftY += rowHeight + rowGap;
    };

    placeEntryRow (slackLabel, slackEntry);
    placeEntryRow (clusterWindowLabel, clusterWindowEntry);
    placeEntryRow (missingTimeoutLabel, missingTimeoutEntry);
    placeEntryRow (extraNoteBudgetLabel, extraNoteBudgetEntry);
    placeEntryRow (pitchToleranceLabel, pitchToleranceEntry);

    velocityButton.setBounds (leftX, leftY, columnWidth, rowHeight);
    leftY += rowHeight + rowGap;
    resetStartOffsetButton.setBounds (leftX, leftY, columnWidth, rowHeight);
    leftY += rowHeight + rowGap;
    copyLogButton.setBounds (leftX, leftY, columnWidth, rowHeight);

    auto placeValueRow = [&](juce::Label& label, juce::Label& value)
    {
        label.setBounds (rightX, rightY, labelWidth, rowHeight);
        value.setBounds (rightX + labelWidth + sliderGap, rightY,
            columnWidth - labelWidth - sliderGap, rowHeight);
        rightY += rowHeight + rowGap;
    };

    placeValueRow (timingLabel, timingValueLabel);
    placeValueRow (matchLabel, matchValueLabel);
    placeValueRow (cpuLabel, cpuValueLabel);
    placeValueRow (bpmLabel, bpmValueLabel);
    placeValueRow (refIoiLabel, refIoiValueLabel);
    placeValueRow (startOffsetLabel, startOffsetValueLabel);

    const int buildInfoX = juce::roundToInt (kBuildInfoX * kAssetScale);
    const int buildInfoY = juce::roundToInt (kBuildInfoY * kAssetScale);
    const int buildInfoWidth = measureTextWidth (buildInfoLabel.getFont(), buildInfoLabel.getText()) + 6;
    const int buildInfoHeight = juce::roundToInt (buildInfoLabel.getFont().getHeight() + 4.0f);
    buildInfoLabel.setBounds (buildInfoX, buildInfoY,
        juce::jmax (1, juce::jmin (buildInfoWidth, getWidth() - buildInfoX - 4)),
        buildInfoHeight);
}

bool PluginEditor::keyPressed (const juce::KeyPress& key)
{
    if (handleDeveloperShortcut (key))
        return true;

    const auto keyChar = key.getTextCharacter();
    if (keyChar == 'o' || keyChar == 'O')
    {
        overlayEnabled = ! overlayEnabled;
        repaint();
        return true;
    }

    if (keyChar == 'b' || keyChar == 'B')
    {
        boundsOverlayEnabled = ! boundsOverlayEnabled;
        advancedUserOptions.setDebugOverlayEnabled (boundsOverlayEnabled);
        repaint();
        return true;
    }

    return false;
}

bool PluginEditor::handleDeveloperShortcut (const juce::KeyPress& key)
{
    const auto modifiers = key.getModifiers();
    const auto keyCode = key.getKeyCode();
    if ((keyCode == 'd' || keyCode == 'D')
        && modifiers.isCommandDown()
        && modifiers.isShiftDown())
    {
        setDeveloperModeActive (! developerModeActive);
        return true;
    }

    return false;
}

void PluginEditor::mouseMove (const juce::MouseEvent& event)
{
    lastMousePosition = event.getPosition();
    hasMousePosition = true;
    if (boundsOverlayEnabled)
        repaint();
}

void PluginEditor::mouseDrag (const juce::MouseEvent& event)
{
    mouseMove (event);
}

void PluginEditor::updateUiVisibility()
{
    const bool showDeveloperConsole = developerModeActive && developerConsoleOpen;

    expandButton.setVisible (! isExpanded);
    referenceBox.setVisible (isExpanded);
    correctionSlider.setVisible (isExpanded);
    correctionDisplay.setVisible (isExpanded && ! showDeveloperConsole);
    advancedUserOptions.setVisible (isExpanded && ! showDeveloperConsole);

    referenceLabel.setVisible (false);
    correctionLabel.setVisible (false);
    referenceLoadedIndicator.setVisible (false);
    tabContainer.setVisible (false);
    developerBox.setVisible (false);
    developerPanelBackdrop.setVisible (isExpanded && showDeveloperConsole);
    virtuosoTabButton.setVisible (false);
    influencerTabButton.setVisible (false);
    actualiserTabButton.setVisible (false);

    referenceStatusLabel.setVisible (isExpanded && showDeveloperConsole);
    slackLabel.setVisible (isExpanded && showDeveloperConsole);
    slackEntry.setVisible (isExpanded && showDeveloperConsole);
    clusterWindowLabel.setVisible (isExpanded && showDeveloperConsole);
    clusterWindowEntry.setVisible (isExpanded && showDeveloperConsole);
    missingTimeoutLabel.setVisible (isExpanded && showDeveloperConsole);
    missingTimeoutEntry.setVisible (isExpanded && showDeveloperConsole);
    extraNoteBudgetLabel.setVisible (isExpanded && showDeveloperConsole);
    extraNoteBudgetEntry.setVisible (isExpanded && showDeveloperConsole);
    pitchToleranceLabel.setVisible (isExpanded && showDeveloperConsole);
    pitchToleranceEntry.setVisible (isExpanded && showDeveloperConsole);
    velocityButton.setVisible (isExpanded && showDeveloperConsole);
    timingLabel.setVisible (isExpanded && showDeveloperConsole);
    timingValueLabel.setVisible (isExpanded && showDeveloperConsole);
    matchLabel.setVisible (isExpanded && showDeveloperConsole);
    matchValueLabel.setVisible (isExpanded && showDeveloperConsole);
    cpuLabel.setVisible (isExpanded && showDeveloperConsole);
    cpuValueLabel.setVisible (isExpanded && showDeveloperConsole);
    bpmLabel.setVisible (isExpanded && showDeveloperConsole);
    bpmValueLabel.setVisible (isExpanded && showDeveloperConsole);
    refIoiLabel.setVisible (isExpanded && showDeveloperConsole);
    refIoiValueLabel.setVisible (isExpanded && showDeveloperConsole);
    startOffsetLabel.setVisible (isExpanded && showDeveloperConsole);
    startOffsetValueLabel.setVisible (isExpanded && showDeveloperConsole);
    resetStartOffsetButton.setVisible (isExpanded && showDeveloperConsole);
    copyLogButton.setVisible (isExpanded && showDeveloperConsole);

    resetButton.setVisible (true);
    tooltipsCheckbox.setVisible (true);

    updateDeveloperOverlayComponents();
}

void PluginEditor::setDeveloperModeActive (bool shouldBeActive)
{
    if (developerModeActive == shouldBeActive)
        return;

    developerModeActive = shouldBeActive;
    developerModeTargetAlpha = developerModeActive ? 1.0f : 0.0f;

    if (! developerModeActive)
    {
        developerConsoleOpen = false;
        developerConsoleButton.setToggleState (false, juce::dontSendNotification);
    }

    updateUiVisibility();
    resized();
    repaint();
}

void PluginEditor::setDeveloperConsoleOpen (bool shouldBeOpen)
{
    const bool nextState = developerModeActive && shouldBeOpen;
    if (developerConsoleOpen == nextState)
        return;

    developerConsoleOpen = nextState;
    developerConsoleButton.setToggleState (developerConsoleOpen, juce::dontSendNotification);
    updateUiVisibility();
    resized();
    repaint();
}

void PluginEditor::updateDeveloperModeFade (double nowMs)
{
    if (lastDeveloperFadeMs <= 0.0)
        lastDeveloperFadeMs = nowMs;

    const double deltaMs = nowMs - lastDeveloperFadeMs;
    lastDeveloperFadeMs = nowMs;

    if (deltaMs <= 0.0)
        return;

    if (developerModeAlpha == developerModeTargetAlpha)
        return;

    const float step = static_cast<float> (deltaMs / kDeveloperFadeDurationMs);
    if (developerModeTargetAlpha > developerModeAlpha)
        developerModeAlpha = juce::jmin (developerModeTargetAlpha, developerModeAlpha + step);
    else
        developerModeAlpha = juce::jmax (developerModeTargetAlpha, developerModeAlpha - step);

    updateDeveloperOverlayComponents();
    repaint();
}

void PluginEditor::updateDeveloperOverlayComponents()
{
    const float alpha = developerModeAlpha * (isExpanded ? 1.0f : 0.0f);
    const bool showButton = isExpanded
        && (developerModeAlpha > 0.0f || developerModeTargetAlpha > 0.0f);
    developerConsoleButton.setVisible (showButton);
    developerConsoleButton.setEnabled (developerModeActive);
    developerConsoleButton.setAlpha (alpha);

    buildInfoLabel.setVisible (showButton);
    buildInfoLabel.setAlpha (alpha * 0.5f);
}

void PluginEditor::configureNumberEntry (juce::Label& label)
{
    label.setEditable (true, true, false);
    label.setJustificationType (juce::Justification::centredRight);
    label.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    label.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label.setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
}

void PluginEditor::commitNumberEntry (juce::Label& label, const char* paramId)
{
    auto* param = processor.apvts.getParameter (paramId);
    if (param == nullptr)
        return;

    float value = 0.0f;
    if (! tryParseFloat (label.getText(), value))
    {
        syncNumberEntry (label, paramId);
        return;
    }

    const auto range = processor.apvts.getParameterRange (paramId);
    const float clamped = range.snapToLegalValue (value);
    const float normalized = range.convertTo0to1 (clamped);
    param->beginChangeGesture();
    param->setValueNotifyingHost (normalized);
    param->endChangeGesture();
    label.setText (param->getCurrentValueAsText(), juce::dontSendNotification);
}

void PluginEditor::syncNumberEntry (juce::Label& label, const char* paramId)
{
    if (label.isBeingEdited())
        return;

    if (auto* param = processor.apvts.getParameter (paramId))
    {
        const auto text = param->getCurrentValueAsText();
        if (label.getText() != text)
            label.setText (text, juce::dontSendNotification);
    }
}

void PluginEditor::applyClusterWindowFromUi()
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

    const auto* value = processor.apvts.getRawParameterValue (kParamClusterWindowMs);
    const float clusterWindowMs = (value != nullptr) ? value->load() : 0.0f;
    juce::String errorMessage;
    if (processor.rebuildReferenceClusters (clusterWindowMs, errorMessage))
    {
        referenceStatusLabel.setText ("Cluster window updated.",
            juce::dontSendNotification);
    }
    else
    {
        referenceStatusLabel.setText ("Cluster update failed: " + errorMessage,
            juce::dontSendNotification);
    }
}

void PluginEditor::rebuildReferenceList()
{
    referenceFiles.clear();

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

    referenceBox.clear (juce::dontSendNotification);
    for (int i = 0; i < referenceFiles.size(); ++i)
        referenceBox.addItem (referenceFiles[i].getFileNameWithoutExtension(), i + 1);

    referenceBox.setTextWhenNoChoicesAvailable ("No personalities found");
    if (referenceFiles.isEmpty())
    {
        referenceBox.setTextWhenNothingSelected (kChooseLabel);
        referenceBox.setSelectedId (0, juce::dontSendNotification);
        referenceBox.setColour (juce::ComboBox::textColourId, juce::Colours::white);
        referenceBox.setEnabled (false);
    }
    else
    {
        referenceBox.setTextWhenNothingSelected (kChooseLabel);
        referenceBox.setColour (juce::ComboBox::textColourId, juce::Colours::white);
        referenceBox.setEnabled (true);
    }

    referenceBox.setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    referenceBox.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    referenceBox.setColour (juce::ComboBox::arrowColourId, juce::Colours::transparentBlack);
}

void PluginEditor::resetParametersToDefaults()
{
    for (auto* parameter : processor.getParameters())
    {
        if (parameter != nullptr)
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
    }
}

void PluginEditor::resetPluginState()
{
    juce::String errorMessage;
    if (! processor.resetToDefaults (errorMessage))
    {
        if (errorMessage.isNotEmpty())
            referenceStatusLabel.setText (errorMessage, juce::dontSendNotification);
        return;
    }

    resetParametersToDefaults();

    developerModeActive = false;
    developerConsoleOpen = false;
    developerModeAlpha = 0.0f;
    developerModeTargetAlpha = 0.0f;
    developerConsoleButton.setToggleState (false, juce::dontSendNotification);

    rebuildReferenceList();
    referenceLoadedIndicator.setActive (false);
    referenceStatusLabel.setText (referenceFiles.isEmpty() ? "No personalities found." : "No reference loaded.",
        juce::dontSendNotification);

    referenceBox.setSelectedId (0, juce::dontSendNotification);
    referenceBox.setColour (juce::ComboBox::textColourId, juce::Colours::white);

    modeBox.setSelectedId (0, juce::dontSendNotification);
    modeBox.setColour (juce::ComboBox::textColourId, juce::Colours::lightgrey);

    tooltipsCheckbox.setToggleState (false, juce::dontSendNotification);
    advancedUserOptions.setReferenceData (processor.getReferenceDisplayDataForUi());
    advancedUserOptions.reset();
    lastUiNoteSample = 0;

    lastInputNoteOnCounter = processor.getInputNoteOnCounter();
    lastOutputNoteOnCounter = processor.getOutputNoteOnCounter();
    lastInputFlashMs = 0.0;
    lastOutputFlashMs = 0.0;
    inputIndicator.setActive (false);
    outputIndicator.setActive (false);

    isExpanded = false;
    expandButton.setExpanded (false);
    updateUiVisibility();
    resized();
    repaint();
}

void PluginEditor::timerCallback()
{
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();
    updateDeveloperModeFade (nowMs);

    if (! processor.isTransportPlaying())
    {
        juce::String pendingPath;
        if (processor.consumePendingReferencePath (pendingPath))
        {
            juce::String errorMessage;
            if (processor.loadReferenceFromFile (juce::File (pendingPath), errorMessage))
            {
                referenceStatusLabel.setText ("", juce::dontSendNotification);
                referenceLoadedIndicator.setActive (true);
            }
            else
            {
                referenceStatusLabel.setText ("Load failed: " + errorMessage, juce::dontSendNotification);
                referenceLoadedIndicator.setActive (false);
            }
        }
    }

    advancedUserOptions.setReferenceData (processor.getReferenceDisplayDataForUi());
    const auto loadError = processor.getReferenceLoadError();
    advancedUserOptions.setStatusMessage (loadError.isNotEmpty() ? "Load error: " + loadError : juce::String());
    processor.popUiNoteEvents (uiNoteEvents, 512);
    if (! uiNoteEvents.empty())
        lastUiNoteSample = uiNoteEvents.back().sample;

    const auto timelineSample = processor.getTimelineSampleForUi();
    const auto referenceStartSample = processor.getReferenceTransportStartSampleForUi();
    const auto sampleRate = processor.getSampleRateForUi();
    const bool transportPlaying = processor.isTransportPlaying();
    const uint64_t halfWindowSamples = sampleRate > 0.0
        ? static_cast<uint64_t> (std::llround (sampleRate * 2.5))
        : 0;
    const bool recentUserNote = ! transportPlaying
        && lastUiNoteSample > 0
        && timelineSample >= lastUiNoteSample
        && (timelineSample - lastUiNoteSample) <= halfWindowSamples;
    const uint64_t nowSample = (transportPlaying || recentUserNote) ? timelineSample : referenceStartSample;

    advancedUserOptions.setTimeline (nowSample, referenceStartSample, sampleRate);
    advancedUserOptions.addUiEvents (uiNoteEvents);
    if (advancedUserOptions.isVisible())
        advancedUserOptions.repaint();

    const auto inputCounter = processor.getInputNoteOnCounter();
    if (inputCounter != lastInputNoteOnCounter)
    {
        lastInputNoteOnCounter = inputCounter;
        lastInputFlashMs = nowMs;
    }

    const bool inputActive = (nowMs - lastInputFlashMs) <= 120.0;
    inputIndicator.setActive (inputActive);

    const auto outputCounter = processor.getOutputNoteOnCounter();
    if (outputCounter != lastOutputNoteOnCounter)
    {
        lastOutputNoteOnCounter = outputCounter;
        lastOutputFlashMs = nowMs;
    }

    const bool outputActive = (nowMs - lastOutputFlashMs) <= 120.0;
    outputIndicator.setActive (outputActive);

    const auto* slackValue = processor.apvts.getRawParameterValue (kParamDelayMs);
    const float slackMs = (slackValue != nullptr) ? slackValue->load() : 0.0f;
    correctionDisplay.setValues (processor.getLastTimingDeltaMs(),
        processor.getLastNoteOffDeltaMs(),
        processor.getLastVelocityDelta(),
        slackMs);

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
        resetStartOffsetButton.setEnabled (! isPlaying);
        copyLogButton.setEnabled (! isPlaying);
        clusterWindowEntry.setEnabled (! isPlaying);
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

    const auto* clusterValue = processor.apvts.getRawParameterValue (kParamClusterWindowMs);
    const float clusterWindowMs = (clusterValue != nullptr) ? clusterValue->load() : 0.0f;
    if (! clusterWindowEntry.isBeingEdited()
        && std::abs (clusterWindowMs - lastClusterWindowMs) > 0.5f)
    {
        lastClusterWindowMs = clusterWindowMs;
        applyClusterWindowFromUi();
    }

    syncNumberEntry (slackEntry, kParamDelayMs);
    syncNumberEntry (clusterWindowEntry, kParamClusterWindowMs);
    syncNumberEntry (missingTimeoutEntry, kParamMissingTimeoutMs);
    syncNumberEntry (extraNoteBudgetEntry, kParamExtraNoteBudget);
    syncNumberEntry (pitchToleranceEntry, kParamPitchTolerance);
}
