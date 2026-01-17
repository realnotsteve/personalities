#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override = default;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    class PulseIndicator final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
        void setActive (bool shouldBeActive);
        void setColours (juce::Colour active, juce::Colour inactive);
        bool isActive() const noexcept { return active; }

    private:
        bool active = false;
        juce::Colour activeColour = juce::Colours::white;
        juce::Colour inactiveColour = juce::Colours::darkgrey;
    };

    class ImageIndicator final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
        void setActive (bool shouldBeActive);
        void setImages (juce::Image activeImage, juce::Image inactiveImage);
        bool hasImages() const noexcept { return activeImage.isValid() || inactiveImage.isValid(); }

    private:
        bool active = false;
        juce::Image activeImage;
        juce::Image inactiveImage;
    };

    class CorrectionDisplay final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
        void setValues (float noteOnDeltaMs, float noteOffDeltaMs, float velocityDelta, float slackMs);
        void setMinimalStyle (bool shouldBeMinimal) { minimalStyle = shouldBeMinimal; repaint(); }

    private:
        struct TrailPoint
        {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            float magnitude = 0.0f;
        };

        static constexpr int kTrailLength = 28;
        float smoothedOn = 0.0f;
        float smoothedOff = 0.0f;
        float smoothedVel = 0.0f;
        float smoothedMagnitude = 0.0f;
        std::array<TrailPoint, kTrailLength> trail {};
        int trailHead = 0;
        int trailCount = 0;
        bool minimalStyle = false;
    };

    class DeveloperPanelBackdrop final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
    };

    class PianoRollComponent final : public juce::Component
    {
    public:
        void paint (juce::Graphics&) override;
        void setReferenceData (std::shared_ptr<const PluginProcessor::ReferenceDisplayData> data);
        void addUiEvents (const std::vector<PluginProcessor::UiNoteEvent>& events);
        void setTimeline (uint64_t nowSample, uint64_t referenceStartSample, double sampleRate);
        void reset();
        void setDebugOverlayEnabled (bool shouldShow);
        void setStatusMessage (juce::String message);

    private:
        struct UserNote
        {
            int noteNumber = 0;
            int channel = 1;
            int refIndex = -1;
            uint64_t onSample = 0;
            uint64_t offSample = 0;
            uint64_t order = 0;
            bool isActive = false;
            bool matched = false;
        };

        void rebuildPitchRange();
        void pruneOldNotes();

        std::shared_ptr<const PluginProcessor::ReferenceDisplayData> referenceData;
        std::vector<UserNote> userNotes;
        std::vector<uint8_t> referenceMatched;
        uint64_t nowSample = 0;
        uint64_t referenceTransportStartSample = 0;
        uint64_t lastNowSample = 0;
        uint64_t orderCounter = 0;
        double sampleRate = 44100.0;
        int minNote = 0;
        int maxNote = 127;
        bool hasPitchRange = false;
        bool debugOverlayEnabled = false;
        juce::String statusMessage;
    };

    class ExpandButton final : public juce::Button
    {
    public:
        ExpandButton();
        void setExpanded (bool shouldBeExpanded);
        void setImage (juce::Image image);
        void setKeyHandler (std::function<bool (const juce::KeyPress&)> handler);

    private:
        void paintButton (juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
        bool keyPressed (const juce::KeyPress& key) override;
        bool isExpanded = false;
        juce::Image buttonImage;
        std::function<bool (const juce::KeyPress&)> keyHandler;
    };

    class InfluenceSliderLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               const juce::Slider::SliderStyle, juce::Slider&) override;
        void setHandleImage (juce::Image image);

    private:
        juce::Image handleImage;
    };

    class DropdownLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        DropdownLookAndFeel() = default;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        juce::Font getPopupMenuFont() override;
        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
        juce::Label* createComboBoxTextBox (juce::ComboBox&) override;
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    };

    class ImageToggleButton final : public juce::Button
    {
    public:
        ImageToggleButton();
        void setImages (juce::Image onImage, juce::Image offImage);

    private:
        void paintButton (juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
        juce::Image onImage;
        juce::Image offImage;
    };

    class ImageMomentaryButton final : public juce::Button
    {
    public:
        ImageMomentaryButton();
        void setImage (juce::Image image);

    private:
        void paintButton (juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
        juce::Image image;
    };

    class ImageCheckboxButton final : public juce::Button
    {
    public:
        ImageCheckboxButton();
        void setImage (juce::Image image);
        void setCheckColour (juce::Colour colour) { checkColour = colour; }

    private:
        void paintButton (juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
        juce::Image image;
        juce::Colour checkColour = juce::Colour (0xff555ed2);
    };

    void timerCallback() override;
    void updateUiVisibility();
    void rebuildReferenceList();
    void resetParametersToDefaults();
    void resetPluginState();
    void setDeveloperModeActive (bool shouldBeActive);
    void setDeveloperConsoleOpen (bool shouldBeOpen);
    void updateDeveloperModeFade (double nowMs);
    void updateDeveloperOverlayComponents();
    bool handleDeveloperShortcut (const juce::KeyPress& key);

    PluginProcessor& processor;

    DropdownLookAndFeel dropdownLookAndFeel;
    juce::ComboBox referenceBox;
    juce::ComboBox modeBox;
    juce::Label referenceLabel;
    juce::Label referenceStatusLabel;
    PulseIndicator referenceLoadedIndicator;

    juce::TextButton virtuosoTabButton;
    juce::TextButton influencerTabButton;
    juce::TextButton actualiserTabButton;
    juce::GroupComponent tabContainer;
    juce::GroupComponent developerBox;
    CorrectionDisplay correctionDisplay;
    DeveloperPanelBackdrop developerPanelBackdrop;
    PianoRollComponent pianoRoll;

    InfluenceSliderLookAndFeel influenceSliderLookAndFeel;
    juce::Slider slackSlider;
    juce::Label  slackLabel;
    juce::Slider clusterWindowSlider;
    juce::Label  clusterWindowLabel;
    juce::Slider correctionSlider;
    juce::Label  correctionLabel;
    juce::Slider missingTimeoutSlider;
    juce::Label  missingTimeoutLabel;
    juce::Slider extraNoteBudgetSlider;
    juce::Label  extraNoteBudgetLabel;
    juce::Slider pitchToleranceSlider;
    juce::Label  pitchToleranceLabel;
    juce::Label buildInfoLabel;
    ImageIndicator inputIndicator;
    ImageIndicator outputIndicator;
    juce::Label timingLabel;
    juce::Label timingValueLabel;
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
    ImageToggleButton developerConsoleButton;
    ImageToggleButton muteButton;
    ImageToggleButton bypassButton;
    ImageMomentaryButton resetButton;
    ImageCheckboxButton tooltipsCheckbox;
    ExpandButton expandButton;

    juce::Image backgroundOpen;
    juce::Image backgroundClosed;
    juce::Image openButtonImage;
    juce::Image performerDropdownImage;
    juce::Image effectStrengthHandleImage;
    juce::Image muteOnImage;
    juce::Image muteOffImage;
    juce::Image bypassOnImage;
    juce::Image bypassOffImage;
    juce::Image modeDropdownImage;
    juce::Image developerModeIndicatorImage;
    juce::Image developerConsoleButtonOffImage;
    juce::Image developerConsoleButtonOnImage;
    juce::Image resetButtonImage;
    juce::Image tooltipsCheckboxImage;
    juce::Image midiInActiveImage;
    juce::Image midiInInactiveImage;
    juce::Image midiOutActiveImage;
    juce::Image midiOutInactiveImage;
    bool isExpanded = false;
    bool overlayEnabled = false;
    bool boundsOverlayEnabled = false;
    bool developerModeActive = false;
    bool developerConsoleOpen = false;
    bool hasMousePosition = false;
    juce::Point<int> lastMousePosition;
    float developerModeAlpha = 0.0f;
    float developerModeTargetAlpha = 0.0f;
    double lastDeveloperFadeMs = 0.0;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> clusterWindowAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> correctionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> missingTimeoutAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> extraNoteBudgetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchToleranceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
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
    uint64_t lastUiNoteSample = 0;
    std::vector<PluginProcessor::UiNoteEvent> uiNoteEvents;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
