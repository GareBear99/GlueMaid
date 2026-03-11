cmake_minimum_required(VERSION 3.15)

project(GlueMaid VERSION 1.0.0)

if (DEFINED JUCE_DIR)
  list(APPEND CMAKE_PREFIX_PATH "${JUCE_DIR}")
  find_package(JUCE CONFIG REQUIRED)
elseif(DEFINED JUCE_PATH)
  add_subdirectory("${JUCE_PATH}" JUCE)
else()
  message(FATAL_ERROR "Please set JUCE_DIR or JUCE_PATH to your local JUCE folder.")
endif()

juce_add_plugin(GlueMaid
    COMPANY_NAME "GareBear99"
    PRODUCT_NAME "GlueMaid"
    PLUGIN_MANUFACTURER_CODE GaBr
    PLUGIN_CODE GlMd
    FORMATS AU VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_compile_features(GlueMaid PRIVATE cxx_std_17)

target_sources(GlueMaid PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_compile_definitions(GlueMaid PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(GlueMaid PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
)
