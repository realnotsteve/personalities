cmake_minimum_required(VERSION 3.15)

project(Personalities VERSION 0.0.3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

juce_add_plugin(Personalities
    COMPANY_NAME "PRISM"
    PRODUCT_NAME "Personalities"

    PLUGIN_MANUFACTURER_CODE Aurl
    PLUGIN_CODE pERS
    BUNDLE_ID "com.aureality.personalities"

    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    PRODUCES_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE

    FORMATS VST3

    # We output silent audio (hosts like Studio One tend to behave better with an audio bus present)
    AUDIO_OUTPUT_CHANNELS 2
    AUDIO_INPUT_CHANNELS 0
)

target_sources(Personalities PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

juce_generate_juce_header(Personalities)

target_link_libraries(Personalities PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_extra
)

target_compile_definitions(Personalities PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
