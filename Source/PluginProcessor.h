#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

class PluginProcessor final : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return JucePlugin_WantsMidiInput; }
    bool producesMidi() const override { return JucePlugin_ProducesMidiOutput; }
    bool isMidiEffect() const override { return JucePlugin_IsMidiEffect; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    bool loadReferenceFromFile (const juce::File& file, juce::String& errorMessage);
    juce::String getReferencePath() const;
    uint32_t getInputNoteOnCounter() const noexcept;
    uint32_t getOutputNoteOnCounter() const noexcept;
    float getLastTimingDeltaMs() const noexcept;
    float getLastNoteOffDeltaMs() const noexcept;
    float getLastVelocityDelta() const noexcept;
    uint32_t getMatchedNoteOnCounter() const noexcept;
    uint32_t getMissedNoteOnCounter() const noexcept;
    bool isTransportPlaying() const noexcept;
    float getCpuLoadPercent() const noexcept;
    float getHostBpm() const noexcept;
    float getReferenceBpm() const noexcept;
    float getReferenceIoiMinMs() const noexcept;
    float getReferenceIoiMedianMs() const noexcept;
    float getClusterWindowMs() const noexcept;
    float getStartOffsetMs() const noexcept;
    float getStartOffsetBars() const noexcept;
    bool hasStartOffset() const noexcept;
    juce::String createMissLogReport() const;
    bool rebuildReferenceClusters (float clusterWindowMs, juce::String& errorMessage);
    void requestStartOffsetReset() noexcept;
    bool resetToDefaults (juce::String& errorMessage);
    struct UiNoteEvent
    {
        uint64_t sample = 0;
        int noteNumber = 0;
        int channel = 1;
        int refIndex = -1;
        bool isNoteOn = false;
    };

    struct ReferenceDisplayNote
    {
        int noteNumber = 0;
        int channel = 1;
        uint64_t onSample = 0;
        uint64_t offSample = 0;
    };

    struct ReferenceDisplayData
    {
        juce::String sourcePath;
        std::vector<ReferenceDisplayNote> notes;
        uint64_t firstNoteSample = 0;
    };

    int popUiNoteEvents (std::vector<UiNoteEvent>& dest, int maxEvents);
    uint64_t getTimelineSampleForUi() const noexcept;
    uint64_t getReferenceTransportStartSampleForUi() const noexcept;
    double getSampleRateForUi() const noexcept;
    std::shared_ptr<const ReferenceDisplayData> getReferenceDisplayDataForUi() const noexcept;
    juce::String getReferenceLoadError() const;
    bool consumePendingReferencePath (juce::String& path);

    // Parameters
    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    struct ReferenceNote
    {
        int noteNumber = 0;
        int channel = 1;
        uint8_t onVelocity = 0;
        uint8_t offVelocity = 0;
        double onTimeSeconds = 0.0;
        double offTimeSeconds = 0.0;
        uint64_t onSample = 0;
        uint64_t offSample = 0;
    };

    struct ReferenceTempoEvent
    {
        double timeSeconds = 0.0;
        double bpm = 120.0;
    };

    struct ReferenceCluster
    {
        int startIndex = 0;
        int noteCount = 0;
        double startTimeSeconds = 0.0;
        double endTimeSeconds = 0.0;
    };

    struct ReferenceData
    {
        juce::String sourcePath;
        std::vector<ReferenceNote> notes;
        std::vector<uint8_t> matched;
        std::vector<ReferenceCluster> clusters;
        std::vector<int> clusterMatchedCounts;
        std::vector<ReferenceTempoEvent> tempoEvents;
        int timeSigNumerator = 4;
        int timeSigDenominator = 4;
        double barDurationSeconds = 0.0;
        double clusterWindowSeconds = 0.0;
        double minIoiSeconds = -1.0;
        double medianIoiSeconds = -1.0;
        double sampleRate = 0.0;
        bool sampleTimesValid = false;
        uint64_t firstNoteSample = 0;
        double firstNoteTimeSeconds = 0.0;
    };

    struct ActiveNote
    {
        int noteNumber = 0;
        int channel = 1;
        int refIndex = -1;
        uint64_t onOrder = 0;
    };

    struct ScheduledMidiEvent
    {
        uint64_t dueSample = 0;
        uint64_t order = 0;
        uint8_t size = 0;
        uint8_t data[8] = {};
    };

    struct MissLogEntry
    {
        float timeMs = 0.0f;
        float slackMs = 0.0f;
        float clusterWindowMs = 0.0f;
        float correction = 0.0f;
        float hostBpm = -1.0f;
        float referenceBpm = -1.0f;
        uint8_t noteNumber = 0;
        uint8_t velocity = 0;
        uint8_t channel = 1;
        uint8_t reserved = 0;
        int referenceClusterIndex = 0;
    };

    static constexpr int kMaxQueuedEvents = 4096;
    static constexpr int kMaxMidiBytes = 8;
    static constexpr int kMidiEventOverheadBytes = sizeof (std::int32_t) + sizeof (std::uint16_t);
    static constexpr int kMaxOutputEvents = kMaxQueuedEvents;
    static constexpr int kMaxActiveNotes = 2048;
    static constexpr uint32_t kMaxMissLogEntries = 4096;
    static constexpr int kMaxClusterMissStreak = 4;
    static constexpr int kMaxClusterLookahead = 24;
    static constexpr int kMaxUiNoteEvents = 4096;
    static constexpr float kVelocityEmaAlpha = 0.05f;

    void insertScheduledEvent (const ScheduledMidiEvent& event) noexcept;
    int removeOldestActiveNote (int noteNumber, int channel) noexcept;
    int matchReferenceNoteInCluster (int noteNumber,
                                     int channel,
                                     int pitchTolerance,
                                     ReferenceData& reference,
                                     int maxLookaheadClusters) noexcept;
    void handleClusterMiss (ReferenceData& reference) noexcept;
    void advanceClusterCursor (ReferenceData& reference) noexcept;
    void resetPlaybackState() noexcept;
    void updateReferenceSampleTimes (ReferenceData& data, double sampleRate);
    std::shared_ptr<ReferenceData> buildReferenceFromFile (const juce::File& file,
                                                           double shiftBars,
                                                           double clusterWindowSeconds,
                                                           juce::String& errorMessage);
    void resetVelocityStats() noexcept;
    void updateVelocityStats (uint8_t userVelocity, int referenceVelocity) noexcept;
    float getVelocityScale() const noexcept;
    uint8_t scaleReferenceVelocity (uint8_t referenceVelocity) const noexcept;
    void clearMissLog() noexcept;
    void pushUiNoteEvent (uint64_t sample,
                          int noteNumber,
                          int channel,
                          int refIndex,
                          bool isNoteOn) noexcept;
    std::shared_ptr<ReferenceDisplayData> buildReferenceDisplayData (const ReferenceData& reference) const;
    void updateUiTimelineState() noexcept;
    void logMiss (int noteNumber,
                  int velocity,
                  int channel,
                  uint64_t userSample,
                  float slackMs,
                  float clusterWindowMs,
                  float correction,
                  float hostBpmValue,
                  float referenceBpmValue) noexcept;

    std::array<ScheduledMidiEvent, kMaxQueuedEvents> queue {};
    int queueSize = 0;
    uint64_t timelineSample = 0;
    uint64_t orderCounter = 0;
    uint64_t latchedSlackSamples = 0;
    uint64_t referenceTransportStartSample = 0;
    std::array<UiNoteEvent, kMaxUiNoteEvents> uiNoteEvents {};
    juce::AbstractFifo uiNoteFifo { kMaxUiNoteEvents };
    std::atomic<uint64_t> timelineSampleForUi { 0 };
    std::atomic<uint64_t> referenceTransportStartSampleForUi { 0 };
    std::atomic<double> sampleRateForUi { 44100.0 };
    std::atomic<int> tempoShiftModeForUi { 0 };
    double sampleRateHz = 44100.0;
    std::atomic<float>* delayMsParam = nullptr;
    std::atomic<float>* clusterWindowMsParam = nullptr;
    std::atomic<float>* correctionParam = nullptr;
    std::atomic<float>* missingTimeoutMsParam = nullptr;
    std::atomic<float>* extraNoteBudgetParam = nullptr;
    std::atomic<float>* pitchToleranceParam = nullptr;
    std::atomic<float>* muteParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;
    std::atomic<float>* velocityCorrectionParam = nullptr;
    std::atomic<float>* tempoShiftParam = nullptr;
    std::atomic<uint32_t> inputNoteOnCounter { 0 };
    std::atomic<uint32_t> outputNoteOnCounter { 0 };
    std::atomic<float> lastTimingDeltaMs { 0.0f };
    std::atomic<float> lastNoteOffDeltaMs { 0.0f };
    std::atomic<float> lastVelocityDelta { 0.0f };
    std::atomic<uint32_t> matchedNoteOnCounter { 0 };
    std::atomic<uint32_t> missedNoteOnCounter { 0 };
    std::atomic<float> cpuLoadPercent { 0.0f };
    std::atomic<float> hostBpm { -1.0f };
    std::atomic<float> referenceBpm { -1.0f };
    std::shared_ptr<ReferenceData> referenceData;
    std::shared_ptr<ReferenceData> referenceDataShifted;
    std::shared_ptr<ReferenceDisplayData> referenceDisplayData;
    std::shared_ptr<ReferenceDisplayData> referenceDisplayDataShifted;
    std::array<ActiveNote, kMaxActiveNotes> activeNotes {};
    int activeNoteCount = 0;
    int referenceClusterCursor = 0;
    int referenceClusterMatchedCount = 0;
    int clusterMissStreak = 0;
    int extraNoteStreak = 0;
    int referenceTempoIndex = 0;
    uint64_t noteOnOrderCounter = 0;
    float userVelocityEma = 64.0f;
    float referenceVelocityEma = 64.0f;
    bool userVelocityEmaValid = false;
    bool referenceVelocityEmaValid = false;
    uint64_t playbackStartSample = 0;
    uint64_t userStartSample = 0;
    bool userStartSampleCaptured = false;
    std::atomic<float> startOffsetMs { 0.0f };
    std::atomic<float> startOffsetBars { 0.0f };
    std::atomic<bool> startOffsetValid { false };
    int64_t lastHostSample = -1;
    bool transportWasPlaying = false;
    int tempoShiftMode = 0;
    std::atomic<bool> transportPlaying { false };
    juce::String referencePath;
    juce::String pendingReferencePath;
    juce::String lastReferenceLoadError;
    juce::MidiBuffer outputBuffer;
    std::array<MissLogEntry, kMaxMissLogEntries> missLog {};
    std::atomic<uint32_t> missLogCount { 0 };
    std::atomic<bool> missLogOverflow { false };
    std::atomic<bool> startOffsetResetRequested { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
