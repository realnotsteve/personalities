#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
    constexpr const char* kParamDelayMs = "delay_ms";
    constexpr const char* kParamClusterWindowMs = "match_window_ms";
    constexpr const char* kParamCorrection = "correction";
    constexpr const char* kParamMute = "mute";
    constexpr const char* kParamBypass = "bypass";
    constexpr const char* kParamVelocityCorrection = "velocity_correction";
    constexpr const char* kParamTempoShiftBackBar = "tempo_shift_back_bar";
    constexpr const char* kReferencePathProperty = "reference_path";
    constexpr float kMaxSlackMs = 2000.0f;
    constexpr float kMinClusterWindowMs = 20.0f;
    constexpr float kMaxClusterWindowMs = 1000.0f;

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

    double convertTicksToSeconds (double time,
                                  const juce::MidiMessageSequence& tempoEvents,
                                  int timeFormat)
    {
        if (timeFormat < 0)
            return time / (-(timeFormat >> 8) * (timeFormat & 0xff));

        double lastTime = 0.0;
        double correctedTime = 0.0;
        const auto ticksPerQuarter = static_cast<double> (timeFormat & 0x7fff);
        const auto tickLen = 1.0 / ticksPerQuarter;
        double secsPerTick = 0.5 * tickLen;
        const auto numEvents = tempoEvents.getNumEvents();

        for (int i = 0; i < numEvents; ++i)
        {
            auto& message = tempoEvents.getEventPointer (i)->message;
            const auto eventTime = message.getTimeStamp();

            if (eventTime >= time)
                break;

            correctedTime += (eventTime - lastTime) * secsPerTick;
            lastTime = eventTime;

            if (message.isTempoMetaEvent())
                secsPerTick = tickLen * message.getTempoSecondsPerQuarterNote();

            while (i + 1 < numEvents)
            {
                auto& next = tempoEvents.getEventPointer (i + 1)->message;
                if (! juce::approximatelyEqual (next.getTimeStamp(), eventTime))
                    break;

                if (next.isTempoMetaEvent())
                    secsPerTick = tickLen * next.getTempoSecondsPerQuarterNote();

                ++i;
            }
        }

        return correctedTime + (time - lastTime) * secsPerTick;
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
    clusterWindowMsParam = apvts.getRawParameterValue (kParamClusterWindowMs);
    correctionParam = apvts.getRawParameterValue (kParamCorrection);
    muteParam = apvts.getRawParameterValue (kParamMute);
    bypassParam = apvts.getRawParameterValue (kParamBypass);
    velocityCorrectionParam = apvts.getRawParameterValue (kParamVelocityCorrection);
    tempoShiftParam = apvts.getRawParameterValue (kParamTempoShiftBackBar);
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
        juce::ParameterID { kParamClusterWindowMs, 1 },
        "Cluster Window (ms)",
        juce::NormalisableRange<float> { kMinClusterWindowMs, kMaxClusterWindowMs, 1.0f },
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

    layout.add (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { kParamTempoShiftBackBar, 1 },
        "Tempo -1 Bar",
        false
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

uint32_t PluginProcessor::getMatchedNoteOnCounter() const noexcept
{
    return matchedNoteOnCounter.load (std::memory_order_relaxed);
}

uint32_t PluginProcessor::getMissedNoteOnCounter() const noexcept
{
    return missedNoteOnCounter.load (std::memory_order_relaxed);
}

bool PluginProcessor::isTransportPlaying() const noexcept
{
    return transportPlaying.load (std::memory_order_relaxed);
}

float PluginProcessor::getCpuLoadPercent() const noexcept
{
    return cpuLoadPercent.load (std::memory_order_relaxed);
}

float PluginProcessor::getHostBpm() const noexcept
{
    return hostBpm.load (std::memory_order_relaxed);
}

float PluginProcessor::getReferenceBpm() const noexcept
{
    return referenceBpm.load (std::memory_order_relaxed);
}

float PluginProcessor::getClusterWindowMs() const noexcept
{
    if (auto reference = std::atomic_load (&referenceData))
        return static_cast<float> (reference->clusterWindowSeconds * 1000.0);
    return 0.0f;
}

float PluginProcessor::getStartOffsetMs() const noexcept
{
    return startOffsetMs.load (std::memory_order_relaxed);
}

float PluginProcessor::getStartOffsetBars() const noexcept
{
    return startOffsetBars.load (std::memory_order_relaxed);
}

bool PluginProcessor::hasStartOffset() const noexcept
{
    return startOffsetValid.load (std::memory_order_relaxed);
}

juce::String PluginProcessor::createMissLogReport() const
{
    juce::String report;
    report << "Personalities Miss Log\n";
    report << "Reference: " << (referencePath.isNotEmpty() ? referencePath : "None") << "\n";

    const auto count = missLogCount.load (std::memory_order_acquire);
    const bool overflow = missLogOverflow.load (std::memory_order_relaxed);
    report << "Entries: " << static_cast<int> (count);
    if (overflow)
        report << " (overflow)";
    report << "\n";
    report << "Columns: time_ms,note,vel,channel,slack_ms,cluster_ms,correction,host_bpm,reference_bpm,ref_cluster\n";

    for (uint32_t i = 0; i < count; ++i)
    {
        const auto& entry = missLog[i];
        report << juce::String (entry.timeMs, 2) << ","
               << static_cast<int> (entry.noteNumber) << ","
               << static_cast<int> (entry.velocity) << ","
               << static_cast<int> (entry.channel) << ","
               << juce::String (entry.slackMs, 1) << ","
               << juce::String (entry.clusterWindowMs, 1) << ","
               << juce::String (entry.correction, 3) << ","
               << juce::String (entry.hostBpm, 2) << ","
               << juce::String (entry.referenceBpm, 2) << ","
               << entry.referenceClusterIndex << "\n";
    }

    return report;
}

bool PluginProcessor::rebuildReferenceClusters (float clusterWindowMs, juce::String& errorMessage)
{
    if (transportPlaying.load (std::memory_order_relaxed))
    {
        errorMessage = "Stop the transport before updating the cluster window.";
        return false;
    }

    if (referencePath.isEmpty())
    {
        errorMessage = "No reference loaded.";
        return false;
    }

    const double clusterWindowSeconds = (clusterWindowMs > 0.0f)
        ? static_cast<double> (clusterWindowMs) / 1000.0
        : 0.0;

    auto baseReference = buildReferenceFromFile (juce::File (referencePath),
        0.0,
        clusterWindowSeconds,
        errorMessage);
    if (baseReference == nullptr)
        return false;

    juce::String shiftError;
    auto shiftedReference = buildReferenceFromFile (juce::File (referencePath),
        1.0,
        clusterWindowSeconds,
        shiftError);
    if (shiftedReference == nullptr)
        shiftedReference = baseReference;

    std::atomic_store (&referenceData, baseReference);
    std::atomic_store (&referenceDataShifted, shiftedReference);
    referenceTempoIndex = 0;
    clearMissLog();
    resetPlaybackState();
    return true;
}

void PluginProcessor::requestStartOffsetReset() noexcept
{
    startOffsetMs.store (0.0f, std::memory_order_relaxed);
    startOffsetBars.store (0.0f, std::memory_order_relaxed);
    startOffsetValid.store (false, std::memory_order_relaxed);
    startOffsetResetRequested.store (true, std::memory_order_release);
}

void PluginProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    sampleRateHz = newSampleRate;
    timelineSample = 0;
    lastHostSample = -1;
    transportPlaying.store (false, std::memory_order_relaxed);
    transportWasPlaying = false;
    tempoShiftMode = 0;
    resetPlaybackState();
    cpuLoadPercent.store (0.0f, std::memory_order_relaxed);
    hostBpm.store (-1.0f, std::memory_order_relaxed);
    referenceBpm.store (-1.0f, std::memory_order_relaxed);
    clearMissLog();

    outputBuffer.clear();
    outputBuffer.ensureSize (kMaxOutputEvents * (kMaxMidiBytes + kMidiEventOverheadBytes));

    if (auto ref = std::atomic_load (&referenceData))
    {
        if (! ref->sampleTimesValid || ref->sampleRate != sampleRateHz)
            updateReferenceSampleTimes (*ref, sampleRateHz);

        if (ref->matched.size() != ref->notes.size())
            ref->matched.assign (ref->notes.size(), 0);
    }

    if (auto refShifted = std::atomic_load (&referenceDataShifted))
    {
        if (! refShifted->sampleTimesValid || refShifted->sampleRate != sampleRateHz)
            updateReferenceSampleTimes (*refShifted, sampleRateHz);

        if (refShifted->matched.size() != refShifted->notes.size())
            refShifted->matched.assign (refShifted->notes.size(), 0);
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
    const auto cpuStartTick = juce::Time::getHighResolutionTicks();

    // Always silent audio for host stability
    buffer.clear();

    outputBuffer.clear();
    const bool isMuted = (muteParam != nullptr) && (muteParam->load() >= 0.5f);
    const bool isBypassed = (bypassParam != nullptr) && (bypassParam->load() >= 0.5f);
    if (isMuted || isBypassed)
        queueSize = 0;

    const int numSamples = buffer.getNumSamples();
    auto updateCpuLoad = [this, cpuStartTick, numSamples]()
    {
        const auto ticksPerSecond = juce::Time::getHighResolutionTicksPerSecond();
        if (ticksPerSecond <= 0 || sampleRateHz <= 0.0 || numSamples <= 0)
            return;

        const auto cpuEndTick = juce::Time::getHighResolutionTicks();
        const double elapsedSeconds = static_cast<double> (cpuEndTick - cpuStartTick)
            / static_cast<double> (ticksPerSecond);
        const double blockSeconds = static_cast<double> (numSamples) / sampleRateHz;
        if (blockSeconds <= 0.0)
            return;

        const float loadPercent = static_cast<float> ((elapsedSeconds / blockSeconds) * 100.0);
        const float previous = cpuLoadPercent.load (std::memory_order_relaxed);
        const float smoothed = previous * 0.9f + loadPercent * 0.1f;
        cpuLoadPercent.store (smoothed, std::memory_order_relaxed);
    };
    bool isPlaying = false;
    int64_t hostSample = -1;
    float hostBpmValue = -1.0f;

    if (auto* playhead = getPlayHead())
    {
        if (auto position = playhead->getPosition())
        {
            isPlaying = position->getIsPlaying();

            if (auto timeInSamples = position->getTimeInSamples())
                hostSample = *timeInSamples;

            if (auto bpm = position->getBpm())
                hostBpmValue = static_cast<float> (*bpm);
        }
    }

    transportPlaying.store (isPlaying, std::memory_order_relaxed);
    hostBpm.store (hostBpmValue, std::memory_order_relaxed);

    if (startOffsetResetRequested.exchange (false, std::memory_order_acq_rel))
    {
        userStartSampleCaptured = false;
        userStartSample = 0;
        startOffsetMs.store (0.0f, std::memory_order_relaxed);
        startOffsetBars.store (0.0f, std::memory_order_relaxed);
        startOffsetValid.store (false, std::memory_order_relaxed);
    }

    const float slackMs = (delayMsParam != nullptr) ? delayMsParam->load() : 0.0f;
    const bool requestedMinusOne = (tempoShiftParam != nullptr)
        && (tempoShiftParam->load() >= 0.5f);
    const int requestedMode = requestedMinusOne ? 1 : 0;

    if (isPlaying)
    {
        if (! transportWasPlaying)
            tempoShiftMode = requestedMode;
    }
    else if (requestedMode != tempoShiftMode)
    {
        tempoShiftMode = requestedMode;
        resetPlaybackState();
        clearMissLog();
    }

    if (isPlaying)
    {
        if (! transportWasPlaying)
        {
            resetPlaybackState();
            clearMissLog();
            latchedSlackSamples = msToSamples (sampleRateHz, slackMs);
            timelineSample = (hostSample >= 0) ? static_cast<uint64_t> (hostSample) : 0;
            referenceTransportStartSample = timelineSample;
            playbackStartSample = timelineSample;
        }
        else if (hostSample >= 0 && lastHostSample >= 0 && hostSample < lastHostSample)
        {
            resetPlaybackState();
            clearMissLog();
            timelineSample = static_cast<uint64_t> (hostSample);
            referenceTransportStartSample = timelineSample;
            playbackStartSample = timelineSample;
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
    auto reference = std::atomic_load (&referenceData);
    if (tempoShiftMode == 1)
    {
        if (auto shifted = std::atomic_load (&referenceDataShifted))
            reference = shifted;
    }
    const bool hasReference = isPlaying
        && reference != nullptr
        && reference->sampleTimesValid
        && ! reference->notes.empty()
        && ! reference->clusters.empty();
    const float effectiveCorrection = hasReference ? correction : 0.0f;
    const uint64_t referenceStartSample = hasReference ? reference->firstNoteSample : 0;
    const bool velocityCorrectionEnabled = (velocityCorrectionParam == nullptr)
        || (velocityCorrectionParam->load() >= 0.5f);
    const float clusterWindowMs = hasReference
        ? static_cast<float> (reference->clusterWindowSeconds * 1000.0)
        : 0.0f;
    const ReferenceData* referenceForOffset = hasReference ? reference.get() : nullptr;
    auto captureStartOffsetIfNeeded = [&](uint64_t userSample)
    {
        if (! isPlaying || userStartSampleCaptured)
            return;

        userStartSampleCaptured = true;
        userStartSample = userSample;
        referenceTransportStartSample = userSample;
        referenceTempoIndex = 0;

        double offsetSeconds = 0.0;
        if (sampleRateHz > 0.0 && userSample >= playbackStartSample)
        {
            offsetSeconds = static_cast<double> (userSample - playbackStartSample) / sampleRateHz;
        }

        startOffsetMs.store (static_cast<float> (offsetSeconds * 1000.0), std::memory_order_relaxed);

        float offsetBarsValue = 0.0f;
        if (referenceForOffset != nullptr && referenceForOffset->barDurationSeconds > 0.0)
            offsetBarsValue = static_cast<float> (offsetSeconds / referenceForOffset->barDurationSeconds);

        startOffsetBars.store (offsetBarsValue, std::memory_order_relaxed);
        startOffsetValid.store (true, std::memory_order_relaxed);
    };

    float referenceBpmValue = -1.0f;
    if (hasReference && sampleRateHz > 0.0 && ! reference->tempoEvents.empty())
    {
        const double elapsedSeconds = (blockStart >= referenceTransportStartSample)
            ? static_cast<double> (blockStart - referenceTransportStartSample) / sampleRateHz
            : 0.0;
        const double referenceTimeSeconds = reference->firstNoteTimeSeconds + elapsedSeconds;
        const auto& tempoEvents = reference->tempoEvents;
        const int numTempoEvents = static_cast<int> (tempoEvents.size());

        if (referenceTempoIndex >= numTempoEvents)
            referenceTempoIndex = numTempoEvents - 1;
        if (referenceTempoIndex < 0)
            referenceTempoIndex = 0;

        while (referenceTempoIndex + 1 < numTempoEvents
            && referenceTimeSeconds >= tempoEvents[static_cast<size_t> (referenceTempoIndex + 1)].timeSeconds)
        {
            ++referenceTempoIndex;
        }

        while (referenceTempoIndex > 0
            && referenceTimeSeconds < tempoEvents[static_cast<size_t> (referenceTempoIndex)].timeSeconds)
        {
            --referenceTempoIndex;
        }

        referenceBpmValue = static_cast<float> (tempoEvents[static_cast<size_t> (referenceTempoIndex)].bpm);
    }

    referenceBpm.store (referenceBpmValue, std::memory_order_relaxed);

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
                captureStartOffsetIfNeeded (userSample);
                inputNoteOnCounter.fetch_add (1, std::memory_order_relaxed);
                lastTimingDeltaMs.store (0.0f, std::memory_order_relaxed);
                if (! isMuted)
                    outputNoteOnCounter.fetch_add (1, std::memory_order_relaxed);

                int referenceVelocityForStats = -1;

    if (hasReference)
    {
        const int channel = (data[0] & 0x0F) + 1;
        const int refIndex = matchReferenceNoteInCluster (static_cast<int> (data[1]),
            channel,
            *reference);
                    if (refIndex >= 0)
                    {
                        matchedNoteOnCounter.fetch_add (1, std::memory_order_relaxed);
                        if (activeNoteCount < kMaxActiveNotes)
                            activeNotes[activeNoteCount++] = { static_cast<int> (data[1]), channel, refIndex };
                        if (refIndex < static_cast<int> (reference->notes.size()))
                            referenceVelocityForStats = reference->notes[static_cast<size_t> (refIndex)].onVelocity;
                    }
                    else
                    {
                        missedNoteOnCounter.fetch_add (1, std::memory_order_relaxed);
                        logMiss (static_cast<int> (data[1]),
                            static_cast<int> (data[2]),
                            channel,
                            userSample,
                            slackMs,
                            clusterWindowMs,
                            correction,
                            hostBpmValue,
                            referenceBpmValue);
                        handleClusterMiss (*reference);
                    }
                }

                updateVelocityStats (data[2], referenceVelocityForStats);
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
        updateCpuLoad();
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
                captureStartOffsetIfNeeded (userSample);
                inputNoteOnCounter.fetch_add (1, std::memory_order_relaxed);
                int refIndex = -1;
                const ReferenceNote* refNote = nullptr;

                if (hasReference)
                {
                    refIndex = matchReferenceNoteInCluster (static_cast<int> (data[1]),
                        channel,
                        *reference);
                    if (refIndex >= 0 && refIndex < static_cast<int> (reference->notes.size()))
                        refNote = &reference->notes[refIndex];
                    if (refIndex >= 0)
                        matchedNoteOnCounter.fetch_add (1, std::memory_order_relaxed);
                    else
                    {
                        missedNoteOnCounter.fetch_add (1, std::memory_order_relaxed);
                        logMiss (static_cast<int> (data[1]),
                            static_cast<int> (data[2]),
                            channel,
                            userSample,
                            slackMs,
                            clusterWindowMs,
                            correction,
                            hostBpmValue,
                            referenceBpmValue);
                        handleClusterMiss (*reference);
                    }
                }

                const uint64_t alignedRefSample = (refNote != nullptr && refNote->onSample >= referenceStartSample)
                    ? referenceTransportStartSample + (refNote->onSample - referenceStartSample)
                    : userSample;
                const uint64_t correctedSample = lerpSamples (userSample, alignedRefSample, effectiveCorrection);
                const uint64_t dueSample = slackSamples + correctedSample;
                const uint8_t inputVelocity = data[2];
                uint8_t outVelocity = inputVelocity;
                if (velocityCorrectionEnabled)
                {
                    const uint8_t targetVelocity = (refNote != nullptr)
                        ? scaleReferenceVelocity (refNote->onVelocity)
                        : inputVelocity;
                    outVelocity = lerpVelocity (inputVelocity, targetVelocity, effectiveCorrection);
                }
                updateVelocityStats (inputVelocity, (refNote != nullptr) ? refNote->onVelocity : -1);

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
                const uint8_t inputVelocity = data[2];
                uint8_t outVelocity = inputVelocity;
                if (velocityCorrectionEnabled)
                {
                    const uint8_t targetVelocity = (refNote != nullptr)
                        ? scaleReferenceVelocity (refNote->offVelocity)
                        : inputVelocity;
                    outVelocity = lerpVelocity (inputVelocity, targetVelocity, effectiveCorrection);
                }

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
    updateCpuLoad();
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

std::shared_ptr<PluginProcessor::ReferenceData> PluginProcessor::buildReferenceFromFile (const juce::File& file,
                                                                                        double shiftBars,
                                                                                        double clusterWindowSeconds,
                                                                                        juce::String& errorMessage)
{
    if (! file.existsAsFile())
    {
        errorMessage = "Reference file not found.";
        return nullptr;
    }

    juce::FileInputStream stream (file);
    if (! stream.openedOk())
    {
        errorMessage = "Unable to open reference file.";
        return nullptr;
    }

    juce::MidiFile midiFile;
    if (! midiFile.readFrom (stream))
    {
        errorMessage = "Invalid MIDI file.";
        return nullptr;
    }

    const int timeFormat = midiFile.getTimeFormat();

    juce::MidiMessageSequence combined;
    for (int i = 0; i < midiFile.getNumTracks(); ++i)
        combined.addSequence (*midiFile.getTrack (i), 0.0);

    combined.sort();
    combined.updateMatchedPairs();

    int timeSigNumerator = 4;
    int timeSigDenominator = 4;
    juce::MidiMessageSequence timeSigEvents;
    midiFile.findAllTimeSigEvents (timeSigEvents);
    timeSigEvents.sort();
    if (timeSigEvents.getNumEvents() > 0)
    {
        int numerator = 4;
        int denominator = 4;
        timeSigEvents.getEventPointer (0)->message.getTimeSignatureInfo (numerator, denominator);
        timeSigNumerator = juce::jmax (1, numerator);
        timeSigDenominator = juce::jmax (1, denominator);
    }

    juce::MidiMessageSequence tempoEvents;
    midiFile.findAllTempoEvents (tempoEvents);
    tempoEvents.sort();
    if (tempoEvents.getNumEvents() == 0)
    {
        auto defaultTempo = juce::MidiMessage::tempoMetaEvent (500000);
        defaultTempo.setTimeStamp (0.0);
        tempoEvents.addEvent (defaultTempo);
    }

    double barTicks = 0.0;
    if (timeFormat > 0)
    {
        const double ticksPerQuarter = static_cast<double> (timeFormat & 0x7fff);
        const double beatFactor = 4.0 / static_cast<double> (juce::jmax (1, timeSigDenominator));
        const double barBeats = static_cast<double> (juce::jmax (1, timeSigNumerator)) * beatFactor;
        barTicks = ticksPerQuarter * barBeats;
    }

    const double shiftTicks = (barTicks > 0.0) ? (shiftBars * barTicks) : 0.0;
    juce::MidiMessageSequence tempoEventsShifted;
    for (int i = 0; i < tempoEvents.getNumEvents(); ++i)
    {
        auto message = tempoEvents.getEventPointer (i)->message;
        if (shiftTicks != 0.0)
            message.setTimeStamp (juce::jmax (0.0, message.getTimeStamp() - shiftTicks));
        tempoEventsShifted.addEvent (message);
    }
    tempoEventsShifted.sort();

    auto reference = std::make_shared<ReferenceData>();
    reference->sourcePath = file.getFullPathName();
    reference->notes.reserve (static_cast<size_t> (combined.getNumEvents()));

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
        note.onTimeSeconds = convertTicksToSeconds (message.getTimeStamp(), tempoEventsShifted, timeFormat);
        note.offTimeSeconds = convertTicksToSeconds (noteOffMessage.getTimeStamp(), tempoEventsShifted, timeFormat);

        reference->notes.push_back (note);
    }

    if (reference->notes.empty())
    {
        errorMessage = "No note data found in reference file.";
        return nullptr;
    }

    std::vector<double> noteDeltas;
    noteDeltas.reserve (reference->notes.size());
    for (size_t i = 1; i < reference->notes.size(); ++i)
    {
        const double delta = reference->notes[i].onTimeSeconds - reference->notes[i - 1].onTimeSeconds;
        if (delta > 0.0)
            noteDeltas.push_back (delta);
    }

    double derivedClusterWindowSeconds = 0.05;
    if (! noteDeltas.empty())
    {
        std::sort (noteDeltas.begin(), noteDeltas.end());
        const double medianDelta = noteDeltas[noteDeltas.size() / 2];
        derivedClusterWindowSeconds = medianDelta * 0.4;
    }

    const double appliedClusterWindowSeconds = (clusterWindowSeconds > 0.0)
        ? clusterWindowSeconds
        : derivedClusterWindowSeconds;

    const double clampedClusterWindowSeconds = juce::jlimit (0.02, 1.0, appliedClusterWindowSeconds);
    reference->clusterWindowSeconds = clampedClusterWindowSeconds;

    reference->clusters.clear();
    reference->clusters.reserve (reference->notes.size());
    ReferenceCluster cluster;
    cluster.startIndex = 0;
    cluster.noteCount = 1;
    cluster.startTimeSeconds = reference->notes.front().onTimeSeconds;
    cluster.endTimeSeconds = reference->notes.front().onTimeSeconds;

    for (int i = 1; i < static_cast<int> (reference->notes.size()); ++i)
    {
        const double timeSeconds = reference->notes[static_cast<size_t> (i)].onTimeSeconds;
        if ((timeSeconds - cluster.startTimeSeconds) <= reference->clusterWindowSeconds)
        {
            ++cluster.noteCount;
            cluster.endTimeSeconds = timeSeconds;
        }
        else
        {
            reference->clusters.push_back (cluster);
            cluster.startIndex = i;
            cluster.noteCount = 1;
            cluster.startTimeSeconds = timeSeconds;
            cluster.endTimeSeconds = timeSeconds;
        }
    }
    reference->clusters.push_back (cluster);

    std::vector<ReferenceTempoEvent> tempoSeconds;
    tempoSeconds.reserve (static_cast<size_t> (tempoEventsShifted.getNumEvents()));

    for (int i = 0; i < tempoEventsShifted.getNumEvents(); ++i)
    {
        const auto& message = tempoEventsShifted.getEventPointer (i)->message;
        if (! message.isTempoMetaEvent())
            continue;

        const double secondsPerQuarter = message.getTempoSecondsPerQuarterNote();
        const double bpm = secondsPerQuarter > 0.0 ? (60.0 / secondsPerQuarter) : 120.0;
        const double timeSeconds = convertTicksToSeconds (message.getTimeStamp(), tempoEventsShifted, timeFormat);
        tempoSeconds.push_back ({ timeSeconds, bpm });
    }

    if (tempoSeconds.empty())
        tempoSeconds.push_back ({ 0.0, 120.0 });

    std::sort (tempoSeconds.begin(), tempoSeconds.end(),
        [] (const ReferenceTempoEvent& a, const ReferenceTempoEvent& b)
        {
            return a.timeSeconds < b.timeSeconds;
        });

    std::vector<ReferenceTempoEvent> collapsedTempo;
    collapsedTempo.reserve (tempoSeconds.size());
    for (const auto& event : tempoSeconds)
    {
        if (collapsedTempo.empty() || event.timeSeconds > collapsedTempo.back().timeSeconds + 1.0e-9)
            collapsedTempo.push_back (event);
        else
            collapsedTempo.back() = event;
    }

    reference->tempoEvents = std::move (collapsedTempo);
    reference->firstNoteTimeSeconds = reference->notes.front().onTimeSeconds;
    reference->timeSigNumerator = timeSigNumerator;
    reference->timeSigDenominator = timeSigDenominator;

    const double bpmForBar = reference->tempoEvents.front().bpm > 0.0
        ? reference->tempoEvents.front().bpm
        : 120.0;
    const double beatFactor = 4.0 / static_cast<double> (juce::jmax (1, timeSigDenominator));
    const double barBeats = static_cast<double> (juce::jmax (1, timeSigNumerator)) * beatFactor;
    reference->barDurationSeconds = barBeats * (60.0 / bpmForBar);

    if (sampleRateHz > 0.0)
        updateReferenceSampleTimes (*reference, sampleRateHz);

    reference->matched.assign (reference->notes.size(), 0);
    return reference;
}

bool PluginProcessor::loadReferenceFromFile (const juce::File& file, juce::String& errorMessage)
{
    if (transportPlaying.load (std::memory_order_relaxed))
    {
        errorMessage = "Stop the transport before loading a reference.";
        return false;
    }

    double clusterWindowSeconds = 0.0;
    if (clusterWindowMsParam != nullptr)
    {
        const float ms = clusterWindowMsParam->load();
        if (ms > 0.0f)
            clusterWindowSeconds = static_cast<double> (ms) / 1000.0;
    }

    auto baseReference = buildReferenceFromFile (file,
        0.0,
        clusterWindowSeconds,
        errorMessage);
    if (baseReference == nullptr)
        return false;

    juce::String shiftError;
    auto shiftedReference = buildReferenceFromFile (file,
        1.0,
        clusterWindowSeconds,
        shiftError);
    if (shiftedReference == nullptr)
        shiftedReference = baseReference;

    std::atomic_store (&referenceData, baseReference);
    std::atomic_store (&referenceDataShifted, shiftedReference);
    referencePath = baseReference->sourcePath;
    apvts.state.setProperty (kReferencePathProperty, referencePath, nullptr);
    referenceTempoIndex = 0;
    resetPlaybackState();
    clearMissLog();
    userStartSampleCaptured = false;
    startOffsetMs.store (0.0f, std::memory_order_relaxed);
    startOffsetBars.store (0.0f, std::memory_order_relaxed);
    startOffsetValid.store (false, std::memory_order_relaxed);

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

int PluginProcessor::matchReferenceNoteInCluster (int noteNumber,
                                                  int channel,
                                                  ReferenceData& reference) noexcept
{
    const auto totalClusters = static_cast<int> (reference.clusters.size());
    if (referenceClusterCursor >= totalClusters)
        return -1;

    if (reference.matched.size() != reference.notes.size())
        return -1;

    auto findNoteIndexInCluster = [&](int clusterIndex) -> int
    {
        if (clusterIndex < 0 || clusterIndex >= totalClusters)
            return -1;

        const auto& cluster = reference.clusters[static_cast<size_t> (clusterIndex)];
        const int startIndex = cluster.startIndex;
        const int endIndex = startIndex + cluster.noteCount;

        for (int i = startIndex; i < endIndex; ++i)
        {
            if (i < 0 || i >= static_cast<int> (reference.notes.size()))
                continue;
            if (reference.matched[static_cast<size_t> (i)] != 0)
                continue;

            const auto& refNote = reference.notes[static_cast<size_t> (i)];
            if (refNote.noteNumber == noteNumber && refNote.channel == channel)
                return i;
        }

        return -1;
    };

    auto countMatchedInCluster = [&](int clusterIndex) -> int
    {
        if (clusterIndex < 0 || clusterIndex >= totalClusters)
            return 0;

        const auto& cluster = reference.clusters[static_cast<size_t> (clusterIndex)];
        const int startIndex = cluster.startIndex;
        const int endIndex = startIndex + cluster.noteCount;
        int count = 0;

        for (int i = startIndex; i < endIndex; ++i)
        {
            if (i < 0 || i >= static_cast<int> (reference.notes.size()))
                continue;
            if (reference.matched[static_cast<size_t> (i)] != 0)
                ++count;
        }

        return count;
    };

    auto applyMatchAtCluster = [&](int clusterIndex, int noteIndex) -> int
    {
        reference.matched[static_cast<size_t> (noteIndex)] = 1;
        referenceClusterCursor = clusterIndex;
        referenceClusterMatchedCount = countMatchedInCluster (clusterIndex);
        clusterMissStreak = 0;

        const auto& cluster = reference.clusters[static_cast<size_t> (clusterIndex)];
        if (referenceClusterMatchedCount >= cluster.noteCount)
        {
            ++referenceClusterCursor;
            referenceClusterMatchedCount = 0;
            clusterMissStreak = 0;
        }

        return noteIndex;
    };

    const int directIndex = findNoteIndexInCluster (referenceClusterCursor);
    if (directIndex >= 0)
        return applyMatchAtCluster (referenceClusterCursor, directIndex);

    const int maxCluster = juce::jmin (totalClusters - 1,
        referenceClusterCursor + kMaxClusterLookahead);
    for (int clusterIndex = referenceClusterCursor + 1; clusterIndex <= maxCluster; ++clusterIndex)
    {
        const int lookaheadIndex = findNoteIndexInCluster (clusterIndex);
        if (lookaheadIndex >= 0)
            return applyMatchAtCluster (clusterIndex, lookaheadIndex);
    }

    return -1;
}

void PluginProcessor::handleClusterMiss (ReferenceData& reference) noexcept
{
    const auto totalClusters = static_cast<int> (reference.clusters.size());
    if (referenceClusterCursor >= totalClusters)
        return;

    ++clusterMissStreak;
    if (clusterMissStreak < kMaxClusterMissStreak)
        return;

    if (referenceClusterCursor + 1 < totalClusters)
    {
        ++referenceClusterCursor;
        referenceClusterMatchedCount = 0;
    }

    clusterMissStreak = 0;
}

void PluginProcessor::resetPlaybackState() noexcept
{
    queueSize = 0;
    orderCounter = 0;
    activeNoteCount = 0;
    referenceClusterCursor = 0;
    referenceClusterMatchedCount = 0;
    clusterMissStreak = 0;
    referenceTempoIndex = 0;
    playbackStartSample = 0;
    userStartSample = 0;
    userStartSampleCaptured = false;
    startOffsetMs.store (0.0f, std::memory_order_relaxed);
    startOffsetBars.store (0.0f, std::memory_order_relaxed);
    startOffsetValid.store (false, std::memory_order_relaxed);
    matchedNoteOnCounter.store (0, std::memory_order_relaxed);
    missedNoteOnCounter.store (0, std::memory_order_relaxed);
    resetVelocityStats();

    if (auto ref = std::atomic_load (&referenceData))
    {
        if (ref->matched.size() == ref->notes.size())
            std::fill (ref->matched.begin(), ref->matched.end(), 0);
    }

    if (auto refShifted = std::atomic_load (&referenceDataShifted))
    {
        if (refShifted->matched.size() == refShifted->notes.size())
            std::fill (refShifted->matched.begin(), refShifted->matched.end(), 0);
    }
}

void PluginProcessor::resetVelocityStats() noexcept
{
    userVelocityEma = 64.0f;
    referenceVelocityEma = 64.0f;
    userVelocityEmaValid = false;
    referenceVelocityEmaValid = false;
}

void PluginProcessor::updateVelocityStats (uint8_t userVelocity, int referenceVelocity) noexcept
{
    const float inputVelocity = static_cast<float> (userVelocity);
    if (! userVelocityEmaValid)
    {
        userVelocityEma = inputVelocity;
        userVelocityEmaValid = true;
    }
    else
    {
        userVelocityEma += kVelocityEmaAlpha * (inputVelocity - userVelocityEma);
    }

    if (referenceVelocity >= 0)
    {
        const float refVelocity = static_cast<float> (referenceVelocity);
        if (! referenceVelocityEmaValid)
        {
            referenceVelocityEma = refVelocity;
            referenceVelocityEmaValid = true;
        }
        else
        {
            referenceVelocityEma += kVelocityEmaAlpha * (refVelocity - referenceVelocityEma);
        }
    }
}

float PluginProcessor::getVelocityScale() const noexcept
{
    if (! userVelocityEmaValid || ! referenceVelocityEmaValid || referenceVelocityEma < 0.5f)
        return 1.0f;

    const float rawScale = userVelocityEma / referenceVelocityEma;
    // Clamp to avoid extreme scaling while the averages settle.
    return juce::jlimit (0.25f, 4.0f, rawScale);
}

uint8_t PluginProcessor::scaleReferenceVelocity (uint8_t referenceVelocity) const noexcept
{
    const float scaled = static_cast<float> (referenceVelocity) * getVelocityScale();
    const int rounded = static_cast<int> (std::lround (scaled));
    return static_cast<uint8_t> (juce::jlimit (0, 127, rounded));
}

void PluginProcessor::clearMissLog() noexcept
{
    missLogCount.store (0, std::memory_order_release);
    missLogOverflow.store (false, std::memory_order_relaxed);
}

void PluginProcessor::logMiss (int noteNumber,
                               int velocity,
                               int channel,
                               uint64_t userSample,
                               float slackMs,
                               float clusterWindowMs,
                               float correction,
                               float hostBpmValue,
                               float referenceBpmValue) noexcept
{
    if (! transportPlaying.load (std::memory_order_relaxed))
        return;

    const uint32_t index = missLogCount.load (std::memory_order_relaxed);
    if (index >= kMaxMissLogEntries)
    {
        missLogOverflow.store (true, std::memory_order_relaxed);
        return;
    }

    MissLogEntry entry;
    entry.timeMs = 0.0f;
    if (sampleRateHz > 0.0 && userSample >= referenceTransportStartSample)
    {
        const double elapsedSeconds = static_cast<double> (userSample - referenceTransportStartSample) / sampleRateHz;
        entry.timeMs = static_cast<float> (elapsedSeconds * 1000.0);
    }

    entry.noteNumber = static_cast<uint8_t> (juce::jlimit (0, 127, noteNumber));
    entry.velocity = static_cast<uint8_t> (juce::jlimit (0, 127, velocity));
    entry.channel = static_cast<uint8_t> (juce::jlimit (1, 16, channel));
    entry.slackMs = slackMs;
    entry.clusterWindowMs = clusterWindowMs;
    entry.correction = correction;
    entry.hostBpm = hostBpmValue;
    entry.referenceBpm = referenceBpmValue;
    entry.referenceClusterIndex = referenceClusterCursor;

    missLog[index] = entry;
    missLogCount.store (index + 1, std::memory_order_release);
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
