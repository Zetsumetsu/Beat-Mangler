juce_add_console_app(BreakManglerSmokeTest)
target_sources(BreakManglerSmokeTest PRIVATE
    Main.cpp
    ../../src/PluginProcessor.cpp
    ../../src/PluginEditor.cpp)
target_include_directories(BreakManglerSmokeTest PRIVATE
    ../../src
    ../../../dsp)
target_compile_definitions(BreakManglerSmokeTest PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(BreakManglerSmokeTest PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors)
