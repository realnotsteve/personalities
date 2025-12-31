#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
    constexpr const char* kParamDelayMs = "delay_ms";
    constexpr const char* kParamMatchWindowMs = "match_window_ms";
    constexpr const char* kParamCorrection = "correction";
    constexpr const char* kParamMute = "mute";
    constexpr const char* kParamBypass = "bypass";
    constexpr const char* kParamVelocityCorrection = "velocity_correction";
    constexpr const char* kReferencePathProperty = "reference_path";
    constexpr float kMaxSlackMs = 2000.0f;
    constexpr float kMaxMatchWindowMs = 200.0f;

    uint64_t msToSamples (double sampleRate, float ms) noexcept
    {
        const double samples = sampleRate * static_cast<double> (ms) / 1000.0;
        const auto rounded = std::llround (samples);
        return static_cast<uint64_t> (juce::jmax (0LL, rounded));
    }

    uint64_t lerpSamples (uint64_t a, uint64_t b, float t) noexcept
    {
        const double blended = (1.0 - static_cast<double> (t)) * static_cast<double> (a)
            + static_cast<double> (t) * static_cast<double> (b);
        const auto rounded = std::llround (blended);
        return static_cast<uint64_t> (juce::jmax (0LL, rounded));
    }

    uint8_t lerpVelocity (uint8_t a, uint8_t b, float t) noexcept
    {
        const float blended = (1.0f - t) * static_cast<float> (a) + t * static_cast<float> (b);
        const int rounded = static_cast<int> (std::lround (blended));
        return static_cast<uint8_t> (juce::jlimit (0, 127, rounded));
    }

}

PluginProcessor::PluginProcessor()
: juce::AudioProcessor (
      BusesProperties()
       #if ! JucePlugin_IsMidiEffect
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
       #endif
      ),
  apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    delayMsParam = apvts.getRawParameterValue (kParamDelayMs);
    matchWindowMsParam = apvts.getRawParameterValue (kParamMatchWindowMs);
    correctionParam = apvts.getRawParameterValue (kParamCorrection);
    muteParam = apvts.getRawParameterValue (kParamMute);
    bypassParam = apvts.getRawParameterValue (kParamBypass);
    velocityCorrectionParam = apvts.getRawParameterValue (kParamVelocityCorrection);
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { kParamDelayMs, 1 },
        "Slack (ms)",
        juce::NormalisableRange<float> { 0.0f, kMaxSlackMs, 1.0f },
        50.0f
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { kParamMatchWindowMs, 1 },
        "Match Window (ms)",
        juce::NormalisableRange<float> { 0.0f, kMaxMatchWindowMs, 1.0f },
        60.0f
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { kParamCorrection, 1 },
        "Correction",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f
    ));

    layout.add (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { kParamMute, 1 },
        "Mute",
        false
    ));

    layout.add (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { kParamBypass, 1 },
        "Bypass",
        false
    ));

    layout.add (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { kParamVelocityCorrection, 1 },
        "Velocity Correction",
        true
    ));

    return layout;
}

uint32_t PluginProcessor::getInputNoteOnCounter() const noexcept
{
    return inputNoteOnCounter.load (std::memory_order_relaxed);
}

uint32_t PluginProcessor::getOutputNoteOnCounter() const noexcept
{
    return outputNoteOnCounter.load (std::memory_order_relaxed);
}

float PluginProcessor::getLastTimingDeltaMs() const noexcept
{
    return lastTimingDeltaMs.load (std::memory_order_relaxed);
}

void PluginProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    sampleRateHz = newSampleRate;
    timelineSample = 0;
    lastHostSample = -1;
    transportPlaying.store (false, std::memory_order_relaxed);
    transportWasPlaying = false;
    resetPlaybackState();

    outputBuffer.clear();
    outputBuffer.ensureSize (kMaxOutputEvents * (kMaxMidiBytes + kMidiEventOverheadBytes));

    if (auto ref = std::atomic_load (&referenceData))
    {
        if (! ref->sampleTimesValid || ref->sampleRate != sampleRateHz)
            updateReferenceSampleTimes (*ref, sampleRateHz);

        if (ref->matched.size() != ref->notes.size())
            ref->matched.assign (ref->notes.size(), 0);
    }
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
   #if JucePlugin_IsMidiEffect
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::disabled()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::disabled();
   #else
    // We expect no input bus and a stereo output bus.
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
   #endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // Always silent audio for host stability
    buffer.clear();

    outputBuffer.clear();
    const bool isMuted = (muteParam != nullptr) && (muteParam->load() >= 0.5f);
    const bool isBypassed = (bypassParam != nullptr) && (bypassParam->load() >= 0.5f);
    if (isMuted || isBypassed)
        queueSize = 0;

    const int numSamples = buffer.getNumSamples();
    bool isPlaying = false;
    int64_t hostSample = -1;

    if (auto* playhead = getPlayHead())
    {
        if (auto position = playhead->getPosition())
        {
            isPlaying = position->getIsPlaying();

            if (auto timeInSamples = position->getTimeInSamples())
                hostSample = *timeInSamples;
        }
    }

    transportPlaying.store (isPlaying, std::memory_order_relaxed);

    const float slackMs = (delayMsParam != nullptr) ? delayMsParam->load() : 0.0f;
    const float matchWindowMs = (matchWindowMsParam != nullptr) ? matchWindowMsParam->load() : 0.0f;

    if (isPlaying)
    {
        if (! transportWasPlaying)
        {
            resetPlaybackState();
            latchedSlackSamples = msToSamples (sampleRateHz, slackMs);
            latchedMatchWindowSamples = msToSamples (sampleRateHz, matchWindowMs);
            timelineSample = (hostSample >= 0) ? static_cast<uint64_t> (hostSample) : 0;
            referenceTransportStartSample = timelineSample;
        }
        else if (hostSample >= 0 && lastHostSample >= 0 && hostSample < lastHostSample)
        {
            resetPlaybackState();
            timelineSample = static_cast<uint64_t> (hostSample);
            referenceTransportStartSample = timelineSample;
        }
    }
    else if (transportWasPlaying)
    {
        resetPlaybackState();
        referenceTransportStartSample = 0;
    }

    const uint64_t blockStart = (isPlaying && hostSample >= 0)
        ? static_cast<uint64_t> (hostSample)
        : timelineSample;
    const uint64_t blockEnd = blockStart + static_cast<uint64_t> (numSamples);

    float correction = isPlaying && (correctionParam != nullptr) ? correctionParam->load() : 0.0f;
    correction = juce::jlimit (0.0f, 1.0f, correction);

    const uint64_t slackSamples = isPlaying ? latchedSlackSamples : msToSamples (sampleRateHz, slackMs);
    const uint64_t matchWindowSamples = isPlaying ? latchedMatchWindowSamples : msToSamples (sampleRateHz, matchWindowMs);
    auto reference = std::atomic_load (&referenceData);
    const bool hasReference = isPlaying
        && reference != nullptr
        && reference->sampleTimesValid
        && ! reference->notes.empty();
    const float effectiveCorrection = hasReference ? correction : 0.0f;
    const uint64_t referenceStartSample = hasReference ? reference->firstNoteSample : 0;
    const bool velocityCorrectionEnabled = (velocityCorrectionParam == nullptr)
        || (velocityCorrectionParam->load() >= 0.5f);

    if (isBypassed)
    {
        for (const auto metadata : midi)
        {
            if (metadata.numBytes < 3)
                continue;

            const int sampleOffset = metadata.samplePosition;
            const int clampedOffset = juce::jmax (0, sampleOffset);
            const uint64_t userSample = blockStart + static_cast<uint64_t> (clampedOffset);

            const uint8_t* data = metadata.data;
            const uint8_t status = static_cast<uint8_t> (data[0] & 0xF0);

            if (status == 0x90 && data[2] > 0)
            {
                inputNoteOnCounter.fetch_add (1, std::memory_order_relaxed);
                lastTimingDeltaMs.store (0.0f, std::memory_order_relaxed);
                if (! isMuted)
                    outputNoteOnCounter.fetch_add (1, std::memory_order_relaxed);

                if (hasReference)
                {
                    const int channel = (data[0] & 0x0F) + 1;
                    const int refIndex = matchReferenceNoteForOn (static_cast<int> (data[1]),
                        channel,
                        userSample,
                        matchWindowSamples,
                        referenceStartSample,
                        *reference);
                    if (refIndex >= 0 && activeNoteCount < kMaxActiveNotes)
                        activeNotes[activeNoteCount++] = { static_cast<int> (data[1]), channel, refIndex };
                }
            }
            else if (status == 0x80 || (status == 0x90 && data[2] == 0))
            {
                if (hasReference)
                {
                    const int channel = (data[0] & 0x0F) + 1;
                    for (int i = activeNoteCount - 1; i >= 0; --i)
                    {
                        if (activeNotes[i].noteNumber == data[1] && activeNotes[i].channel == channel)
                        {
                            activeNotes[i] = activeNotes[activeNoteCount - 1];
                            --activeNoteCount;
                            break;
                        }
                    }
                }
            }
        }

        if (isMuted)
            midi.clear();

        timelineSample = blockEnd;
        lastHostSample = hostSample;
        transportWasPlaying = isPlaying;
        return;
    }

    int outputEventCount = 0;

    auto countOutputNoteOn = [&](const uint8_t* data, uint8_t size)
    {
        if (size < 3)
            return;
        if ((data[0] & 0xF0) == 0x90 && data[2] > 0)
            outputNoteOnCounter.fetch_add (1, std::memory_order_relaxed);
    };

    auto enqueueEvent = [&](const uint8_t* data, uint8_t size, uint64_t dueSample, int passThroughOffset)
    {
        if (isMuted)
            return;
        if (queueSize < kMaxQueuedEvents)
        {
            ScheduledMidiEvent event;
            event.dueSample = dueSample;
            event.order = orderCounter++;
            event.size = size;
            std::memcpy (event.data, data, static_cast<size_t> (size));

            insertScheduledEvent (event);
        }
        else if (outputEventCount < kMaxOutputEvents)
        {
            // Queue overflow: pass through without delay.
            outputBuffer.addEvent (data, size, passThroughOffset);
            countOutputNoteOn (data, size);
            ++outputEventCount;
        }
    };

    for (const auto metadata : midi)
    {
        const int sampleOffset = metadata.samplePosition;
        const int clampedOffset = juce::jmax (0, sampleOffset);
        const uint64_t userSample = blockStart + static_cast<uint64_t> (clampedOffset);

        if (metadata.numBytes > kMaxMidiBytes)
        {
            // Drop oversized messages (e.g. long SysEx) to avoid heap allocation on the audio thread.
            continue;
        }

        const uint8_t* data = metadata.data;
        const uint8_t size = static_cast<uint8_t> (metadata.numBytes);

        if (size >= 3)
        {
            const uint8_t status = static_cast<uint8_t> (data[0] & 0xF0);
            const int channel = (data[0] & 0x0F) + 1;

            if (status == 0x90 && data[2] > 0)
            {
                inputNoteOnCounter.fetch_add (1, std::memory_order_relaxed);
                int refIndex = -1;
                const ReferenceNote* refNote = nullptr;

                if (hasReference)
                {
                    refIndex = matchReferenceNoteForOn (static_cast<int> (data[1]),
                        channel,
                        userSample,
                        matchWindowSamples,
                        referenceStartSample,
                        *reference);
                    if (refIndex >= 0 && refIndex < static_cast<int> (reference->notes.size()))
                        refNote = &reference->notes[refIndex];
                }

                const uint64_t alignedRefSample = (refNote != nullptr && refNote->onSample >= referenceStartSample)
                    ? referenceTransportStartSample + (refNote->onSample - referenceStartSample)
                    : userSample;
                const uint64_t correctedSample = lerpSamples (userSample, alignedRefSample, effectiveCorrection);
                const uint64_t dueSample = slackSamples + correctedSample;
                const uint8_t refVelocity = (refNote != nullptr) ? refNote->onVelocity : data[2];
                const uint8_t outVelocity = velocityCorrectionEnabled
                    ? lerpVelocity (data[2], refVelocity, effectiveCorrection)
                    : data[2];

                const int64_t deltaSamples = static_cast<int64_t> (correctedSample)
                    - static_cast<int64_t> (userSample);
                const float deltaMs = sampleRateHz > 0.0
                    ? static_cast<float> (1000.0 * (static_cast<double> (deltaSamples) / sampleRateHz))
                    : 0.0f;
                lastTimingDeltaMs.store (deltaMs, std::memory_order_relaxed);

                uint8_t outData[3] = { static_cast<uint8_t> (0x90 | (channel - 1)),
                                       data[1],
                                       outVelocity };
                enqueueEvent (outData, 3, dueSample, clampedOffset);

                if (hasReference && refIndex >= 0 && activeNoteCount < kMaxActiveNotes)
                {
                    activeNotes[activeNoteCount++] = { static_cast<int> (data[1]), channel, refIndex };
                }
            }
            else if (status == 0x80 || (status == 0x90 && data[2] == 0))
            {
                int refIndex = -1;

                if (hasReference)
                {
                    for (int i = activeNoteCount - 1; i >= 0; --i)
                    {
                        if (activeNotes[i].noteNumber == data[1] && activeNotes[i].channel == channel)
                        {
                            refIndex = activeNotes[i].refIndex;
                            activeNotes[i] = activeNotes[activeNoteCount - 1];
                            --activeNoteCount;
                            break;
                        }
                    }
                }

                const ReferenceNote* refNote = (refIndex >= 0 && refIndex < static_cast<int> (reference->notes.size()))
                    ? &reference->notes[refIndex]
                    : nullptr;
                const uint64_t alignedRefSample = (refNote != nullptr && refNote->offSample >= referenceStartSample)
                    ? referenceTransportStartSample + (refNote->offSample - referenceStartSample)
                    : userSample;
                const uint64_t correctedSample = lerpSamples (userSample, alignedRefSample, effectiveCorrection);
                const uint64_t dueSample = slackSamples + correctedSample;
                const uint8_t refVelocity = (refNote != nullptr) ? refNote->offVelocity : data[2];
                const uint8_t outVelocity = velocityCorrectionEnabled
                    ? lerpVelocity (data[2], refVelocity, effectiveCorrection)
                    : data[2];

                uint8_t outData[3] = { static_cast<uint8_t> (0x80 | (channel - 1)),
                                       data[1],
                                       outVelocity };
                enqueueEvent (outData, 3, dueSample, clampedOffset);
            }
            else
            {
                const uint64_t dueSample = userSample + slackSamples;
                enqueueEvent (data, size, dueSample, clampedOffset);
            }
        }
        else
        {
            const uint64_t dueSample = userSample + slackSamples;
            enqueueEvent (data, size, dueSample, clampedOffset);
        }
    }

    while (queueSize > 0)
    {
        if (isMuted)
            break;
        const auto& event = queue[0];

        if (event.dueSample >= blockEnd)
            break;

        const int sampleOffset = (event.dueSample > blockStart)
            ? static_cast<int> (event.dueSample - blockStart)
            : 0;

        if (outputEventCount < kMaxOutputEvents)
        {
            outputBuffer.addEvent (event.data, event.size, sampleOffset);
            countOutputNoteOn (event.data, event.size);
            ++outputEventCount;
        }

        for (int i = 1; i < queueSize; ++i)
            queue[i - 1] = queue[i];
        --queueSize;
    }

    midi.swapWith (outputBuffer);
    timelineSample = blockEnd;
    lastHostSample = hostSample;
    transportWasPlaying = isPlaying;
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (kReferencePathProperty, referencePath, nullptr);
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        referencePath = apvts.state.getProperty (kReferencePathProperty).toString();

        if (referencePath.isNotEmpty())
        {
            const juce::String pathToLoad = referencePath;
            juce::MessageManager::callAsync ([this, pathToLoad]()
            {
                juce::String errorMessage;
                loadReferenceFromFile (juce::File (pathToLoad), errorMessage);
            });
        }
    }
}

void PluginProcessor::updateReferenceSampleTimes (ReferenceData& data, double sampleRate)
{
    for (auto& note : data.notes)
    {
        const auto onSamples = std::llround (note.onTimeSeconds * sampleRate);
        const auto offSamples = std::llround (note.offTimeSeconds * sampleRate);
        note.onSample = static_cast<uint64_t> (juce::jmax (0LL, onSamples));
        note.offSample = static_cast<uint64_t> (juce::jmax (0LL, offSamples));
    }

    data.firstNoteSample = data.notes.empty() ? 0 : data.notes.front().onSample;
    data.sampleRate = sampleRate;
    data.sampleTimesValid = true;
}

bool PluginProcessor::loadReferenceFromFile (const juce::File& file, juce::String& errorMessage)
{
    if (transportPlaying.load (std::memory_order_relaxed))
    {
        errorMessage = "Stop the transport before loading a reference.";
        return false;
    }

    if (! file.existsAsFile())
    {
        errorMessage = "Reference file not found.";
        return false;
    }

    juce::FileInputStream stream (file);
    if (! stream.openedOk())
    {
        errorMessage = "Unable to open reference file.";
        return false;
    }

    juce::MidiFile midiFile;
    if (! midiFile.readFrom (stream))
    {
        errorMessage = "Invalid MIDI file.";
        return false;
    }

    midiFile.convertTimestampTicksToSeconds();

    juce::MidiMessageSequence combined;
    for (int i = 0; i < midiFile.getNumTracks(); ++i)
        combined.addSequence (*midiFile.getTrack (i), 0.0);

    combined.sort();
    combined.updateMatchedPairs();

    auto newReference = std::make_shared<ReferenceData>();
    newReference->sourcePath = file.getFullPathName();
    newReference->notes.reserve (static_cast<size_t> (combined.getNumEvents()));

    for (int i = 0; i < combined.getNumEvents(); ++i)
    {
        const auto* event = combined.getEventPointer (i);
        if (event == nullptr)
            continue;

        const auto& message = event->message;
        if (! message.isNoteOn())
            continue;

        const auto* noteOffEvent = event->noteOffObject;
        if (noteOffEvent == nullptr)
            continue;

        const auto& noteOffMessage = noteOffEvent->message;

        ReferenceNote note;
        note.noteNumber = message.getNoteNumber();
        note.channel = message.getChannel();
        note.onVelocity = static_cast<uint8_t> (juce::jlimit (0, 127,
            static_cast<int> (std::lround (message.getVelocity() * 127.0f))));
        note.offVelocity = static_cast<uint8_t> (juce::jlimit (0, 127,
            static_cast<int> (std::lround (noteOffMessage.getVelocity() * 127.0f))));
        note.onTimeSeconds = message.getTimeStamp();
        note.offTimeSeconds = noteOffMessage.getTimeStamp();

        newReference->notes.push_back (note);
    }

    if (newReference->notes.empty())
    {
        errorMessage = "No note data found in reference file.";
        return false;
    }

    if (sampleRateHz > 0.0)
        updateReferenceSampleTimes (*newReference, sampleRateHz);

    newReference->matched.assign (newReference->notes.size(), 0);
    std::atomic_store (&referenceData, newReference);
    referencePath = newReference->sourcePath;
    apvts.state.setProperty (kReferencePathProperty, referencePath, nullptr);

    return true;
}

juce::String PluginProcessor::getReferencePath() const
{
    return referencePath;
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

int PluginProcessor::matchReferenceNoteForOn (int noteNumber,
                                              int channel,
                                              uint64_t userSample,
                                              uint64_t matchWindowSamples,
                                              uint64_t referenceStartSample,
                                              ReferenceData& reference) noexcept
{
    const int totalNotes = static_cast<int> (reference.notes.size());
    if (referenceCursor >= totalNotes)
        return -1;

    if (reference.matched.size() != reference.notes.size())
    {
        const int fallbackIndex = referenceCursor;
        referenceCursor = juce::jmin (referenceCursor + 1, totalNotes);
        return fallbackIndex;
    }

    int selectedIndex = -1;

    if (matchWindowSamples == 0)
    {
        selectedIndex = referenceCursor;
    }
    else
    {
        const uint64_t windowStart = (userSample > matchWindowSamples)
            ? userSample - matchWindowSamples
            : 0;
        const uint64_t windowEnd = userSample + matchWindowSamples;

        int bestIndex = -1;
        uint64_t bestDistance = 0;

        for (int i = referenceCursor; i < totalNotes; ++i)
        {
            if (reference.matched[static_cast<size_t> (i)] != 0)
                continue;

            const auto& refNote = reference.notes[static_cast<size_t> (i)];
            const uint64_t alignedRefSample = (refNote.onSample >= referenceStartSample)
                ? referenceTransportStartSample + (refNote.onSample - referenceStartSample)
                : referenceTransportStartSample;

            if (alignedRefSample > windowEnd)
                break;
            if (alignedRefSample < windowStart)
                continue;
            if (refNote.noteNumber != noteNumber || refNote.channel != channel)
                continue;

            const uint64_t distance = (alignedRefSample >= userSample)
                ? (alignedRefSample - userSample)
                : (userSample - alignedRefSample);

            if (bestIndex < 0 || distance < bestDistance)
            {
                bestIndex = i;
                bestDistance = distance;
            }
        }

        selectedIndex = (bestIndex >= 0) ? bestIndex : referenceCursor;
    }

    if (selectedIndex < 0 || selectedIndex >= totalNotes)
        return -1;

    reference.matched[static_cast<size_t> (selectedIndex)] = 1;

    while (referenceCursor < totalNotes
        && reference.matched[static_cast<size_t> (referenceCursor)] != 0)
    {
        ++referenceCursor;
    }

    return selectedIndex;
}

void PluginProcessor::resetPlaybackState() noexcept
{
    queueSize = 0;
    orderCounter = 0;
    activeNoteCount = 0;
    referenceCursor = 0;

    if (auto ref = std::atomic_load (&referenceData))
    {
        if (ref->matched.size() == ref->notes.size())
            std::fill (ref->matched.begin(), ref->matched.end(), 0);
    }
}

void PluginProcessor::insertScheduledEvent (const ScheduledMidiEvent& event) noexcept
{
    int insertIndex = queueSize;

    while (insertIndex > 0)
    {
        const auto& prev = queue[insertIndex - 1];

        if (prev.dueSample < event.dueSample)
            break;
        if (prev.dueSample == event.dueSample && prev.order <= event.order)
            break;

        queue[insertIndex] = prev;
        --insertIndex;
    }

    queue[insertIndex] = event;
    ++queueSize;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
