cmake_minimum_required(VERSION 3.15)

set(PERSONALITIES_VERSION_MAJOR 0)
set(PERSONALITIES_VERSION_MINOR 0)
set(PERSONALITIES_VERSION_PATCH 3)

set(PERSONALITIES_VERSION
    "${PERSONALITIES_VERSION_MAJOR}.${PERSONALITIES_VERSION_MINOR}.${PERSONALITIES_VERSION_PATCH}")

project(Personalities VERSION ${PERSONALITIES_VERSION})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

set(PERSONALITIES_BUILD_INFO_DIR "${CMAKE_BINARY_DIR}/generated")
set(PERSONALITIES_BUILD_INFO_HEADER "${PERSONALITIES_BUILD_INFO_DIR}/PersonalitiesBuildInfo.h")
set(PERSONALITIES_BUILD_NUMBER_FILE "${PERSONALITIES_BUILD_INFO_DIR}/build_number.txt")
file(MAKE_DIRECTORY "${PERSONALITIES_BUILD_INFO_DIR}")

juce_add_plugin(Personalities
    COMPANY_NAME "PRISM"
    PRODUCT_NAME "Personalities"

    PLUGIN_MANUFACTURER_CODE Aurl
    PLUGIN_CODE pERS
    BUNDLE_ID "com.aureality.personalities"

    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE

    FORMATS VST3
)

juce_add_plugin(Personalities_NoteFX
    COMPANY_NAME "PRISM"
    PRODUCT_NAME "Personalities Note FX"

    PLUGIN_MANUFACTURER_CODE Aurl
    PLUGIN_CODE pFX1
    BUNDLE_ID "com.aureality.personalities.notefx"

    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE

    FORMATS VST3
    VST3_CATEGORIES Fx "Note FX"
)

target_sources(Personalities PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)
target_sources(Personalities_NoteFX PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

juce_generate_juce_header(Personalities)
juce_generate_juce_header(Personalities_NoteFX)

target_link_libraries(Personalities PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_extra
)
target_link_libraries(Personalities_NoteFX PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_extra
)

add_custom_target(Personalities_BuildInfo
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT_HEADER="${PERSONALITIES_BUILD_INFO_HEADER}"
        -DBUILD_NUMBER_FILE="${PERSONALITIES_BUILD_NUMBER_FILE}"
        -DVERSION_MAJOR=${PERSONALITIES_VERSION_MAJOR}
        -DVERSION_MINOR=${PERSONALITIES_VERSION_MINOR}
        -DVERSION_PATCH=${PERSONALITIES_VERSION_PATCH}
        -P "${CMAKE_SOURCE_DIR}/cmake/UpdateBuildInfo.cmake"
    BYPRODUCTS "${PERSONALITIES_BUILD_INFO_HEADER}" "${PERSONALITIES_BUILD_NUMBER_FILE}"
    COMMENT "Updating build info"
)

add_dependencies(Personalities Personalities_BuildInfo)
add_dependencies(Personalities_NoteFX Personalities)

target_include_directories(Personalities PRIVATE "${PERSONALITIES_BUILD_INFO_DIR}")
target_include_directories(Personalities_NoteFX PRIVATE "${PERSONALITIES_BUILD_INFO_DIR}")

add_custom_target(Personalities_AllPlugins
    DEPENDS Personalities_VST3 Personalities_NoteFX_VST3
)

target_compile_definitions(Personalities PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)
target_compile_definitions(Personalities_NoteFX PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

option(PERSONALITIES_COPY_VST3 "Copy VST3 to user plugin folder after build" ON)

if(APPLE AND PERSONALITIES_COPY_VST3)
    set(PERSONALITIES_VST3_INSTALL_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3")
    add_custom_command(TARGET Personalities_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PERSONALITIES_VST3_INSTALL_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<TARGET_BUNDLE_DIR:Personalities_VST3>"
            "${PERSONALITIES_VST3_INSTALL_DIR}/Personalities.vst3"
        COMMENT "Copying Personalities.vst3 to ${PERSONALITIES_VST3_INSTALL_DIR}"
    )
    add_custom_command(TARGET Personalities_NoteFX_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PERSONALITIES_VST3_INSTALL_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<TARGET_BUNDLE_DIR:Personalities_NoteFX_VST3>"
            "${PERSONALITIES_VST3_INSTALL_DIR}/Personalities Note FX.vst3"
        COMMENT "Copying Personalities Note FX.vst3 to ${PERSONALITIES_VST3_INSTALL_DIR}"
    )
endif()
