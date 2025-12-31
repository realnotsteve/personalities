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

    struct ReferenceData
    {
        juce::String sourcePath;
        std::vector<ReferenceNote> notes;
        double sampleRate = 0.0;
        bool sampleTimesValid = false;
        uint64_t firstNoteSample = 0;
    };

    struct ActiveNote
    {
        int noteNumber = 0;
        int channel = 1;
        int refIndex = -1;
    };

    struct ScheduledMidiEvent
    {
        uint64_t dueSample = 0;
        uint64_t order = 0;
        uint8_t size = 0;
        uint8_t data[8] = {};
    };

    static constexpr int kMaxQueuedEvents = 4096;
    static constexpr int kMaxMidiBytes = 8;
    static constexpr int kMidiEventOverheadBytes = sizeof (std::int32_t) + sizeof (std::uint16_t);
    static constexpr int kMaxOutputEvents = kMaxQueuedEvents;
    static constexpr int kMaxActiveNotes = 2048;

    void insertScheduledEvent (const ScheduledMidiEvent& event) noexcept;
    void resetPlaybackState() noexcept;
    void updateReferenceSampleTimes (ReferenceData& data, double sampleRate);

    std::array<ScheduledMidiEvent, kMaxQueuedEvents> queue {};
    int queueSize = 0;
    uint64_t timelineSample = 0;
    uint64_t orderCounter = 0;
    uint64_t latchedSlackSamples = 0;
    uint64_t referenceTransportStartSample = 0;
    double sampleRateHz = 44100.0;
    std::atomic<float>* delayMsParam = nullptr;
    std::atomic<float>* correctionParam = nullptr;
    std::atomic<float>* muteParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;
    std::atomic<float>* velocityCorrectionParam = nullptr;
    std::atomic<uint32_t> inputNoteOnCounter { 0 };
    std::atomic<uint32_t> outputNoteOnCounter { 0 };
    std::atomic<float> lastTimingDeltaMs { 0.0f };
    std::shared_ptr<ReferenceData> referenceData;
    std::array<ActiveNote, kMaxActiveNotes> activeNotes {};
    int activeNoteCount = 0;
    int referenceCursor = 0;
    int64_t lastHostSample = -1;
    bool transportWasPlaying = false;
    std::atomic<bool> transportPlaying { false };
    juce::String referencePath;
    juce::MidiBuffer outputBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
