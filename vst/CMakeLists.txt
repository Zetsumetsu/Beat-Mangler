cmake_minimum_required(VERSION 3.22)
project(BREAKMANGLER_VST VERSION 1.0.1)

# JUCE source: point JUCE_DIR at a local checkout, or let FetchContent grab it.
option(BREAKMANGLE_FETCH_JUCE "Fetch JUCE via CMake FetchContent" ON)
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (used when BREAKMANGLE_FETCH_JUCE is OFF)")

if(BREAKMANGLE_FETCH_JUCE)
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.6
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
else()
    add_subdirectory(${JUCE_DIR} JUCE)
endif()

juce_add_plugin(BreakManglerVST
    COMPANY_NAME "Zetsumetsu"
    PLUGIN_NAME "Break Mangler"
    PRODUCT_NAME "Break Mangler"
    VERSION "1.0.1"
    FORMATS VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    PLUGIN_MANUFACTURER_CODE Zets
    PLUGIN_CODE Btmn
    VST3_CATEGORIES "Instrument"
)

target_sources(BreakManglerVST PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)

# The portable DSP core lives in ../dsp (shared with the Daisy firmware).
target_include_directories(BreakManglerVST PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../dsp)

target_compile_definitions(BreakManglerVST PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BreakManglerVST PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)

# Headless smoke test (loads the built .vst3, loads a WAV, renders, checks).
# Dev tooling only; not part of the plugin.
option(BREAKMANGLE_BUILD_SMOKE_TEST "Build the headless plugin smoke test" OFF)
if(BREAKMANGLE_BUILD_SMOKE_TEST)
    add_subdirectory(tools/smoke_test)
endif()
