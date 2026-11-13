cmake_minimum_required(VERSION 3.22)
project(BeatStation VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

file(GLOB KIT_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Kits/*.wav)
juce_add_binary_data(KitData HEADER_NAME "KitData.h" NAMESPACE KitData SOURCES ${KIT_FILES})
set_target_properties(KitData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

juce_add_plugin(BeatStation
    COMPANY_NAME "Basement Instruments"
    PLUGIN_MANUFACTURER_CODE Bsmt
    PLUGIN_CODE Bsta
    FORMATS VST3 Standalone
    PRODUCT_NAME "Beat Station"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Instrument Sampler
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(BeatStation PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(BeatStation PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BeatStation
    PRIVATE KitData juce::juce_audio_utils juce::juce_audio_devices juce::juce_audio_formats juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
