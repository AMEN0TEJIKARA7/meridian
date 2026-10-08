cmake_minimum_required(VERSION 3.22)
project(Meridian VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Pulls JUCE from GitHub on first configure (cached afterwards).
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(MeridianFonts
    HEADER_NAME MeridianFonts.h
    NAMESPACE   MeridianFonts
    SOURCES
        Resources/Fonts/IBMPlexMono-Regular.ttf
        Resources/Fonts/IBMPlexMono-Medium.ttf
        Resources/Fonts/SourceSerif4-Semibold.ttf
        Resources/Fonts/SourceSerif4-SemiboldIt.ttf
        Resources/Fonts/CormorantGaramond-Bold.ttf
)
set_target_properties(MeridianFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

juce_add_plugin(Meridian
    COMPANY_NAME                "Seraph"
    PLUGIN_MANUFACTURER_CODE    Srph
    PLUGIN_CODE                 Mrdn
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "Meridian"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx EQ
)

set(MERIDIAN_SOURCES
    Source/FilterDesign.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/EQDisplay.cpp
    Source/BandPanel.cpp
    Source/LookAndFeel.cpp
)

target_sources(Meridian PRIVATE ${MERIDIAN_SOURCES})

target_compile_definitions(Meridian PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Meridian
    PRIVATE
        MeridianFonts
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Offline tests: cmake -B build -DMERIDIAN_BUILD_TESTS=ON
option(MERIDIAN_BUILD_TESTS "Build offline DSP/UI tests" OFF)
if(MERIDIAN_BUILD_TESTS)
    juce_add_console_app(MeridianTest PRODUCT_NAME "MeridianTest")
    target_sources(MeridianTest PRIVATE Tests/MeridianTest.cpp ${MERIDIAN_SOURCES})
    target_compile_definitions(MeridianTest PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JucePlugin_Name="Meridian" JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(MeridianTest PRIVATE MeridianFonts juce::juce_audio_utils juce::juce_dsp)
endif()
