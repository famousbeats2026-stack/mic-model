cmake_minimum_required(VERSION 3.22)

# Famous Beats Mic Model — makes a Neumann TLM 103 sound like a Sony
# C-800G tube condenser, from the two mics' published response curves.
# Difference EQ (tame the TLM 103's broad 5 kHz presence shelf, add
# low-mid warmth and smooth air, roll off above 16 kHz) + a 6AU6-style
# tube saturation stage. VST3 + AU audio effect for macOS.
# Build on a Mac (see README.md) or let GitHub Actions build it for you.

# Target machine: macOS 10.15 (Catalina) and newer, Intel and Apple Silicon.
# These MUST be set before project() — the Xcode generator ignores them
# when set after project() (that mistake shipped an arm64-only build of
# the Vocal Cleaner the first time). The GitHub workflow also passes them
# on the cmake command line, which is the belt-and-braces guarantee.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build universal binary")

project(FamousBeatsMicModel VERSION 1.0.0)

# ---- JUCE (downloaded automatically at configure time, not bundled) ----
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.8
)
FetchContent_MakeAvailable(JUCE)

# ---- The plugin ----
juce_add_plugin(FamousBeatsMicModel
    COMPANY_NAME "Famous Beats Studio"
    PRODUCT_NAME "Famous Beats Mic Model"
    BUNDLE_ID com.famousbeatsstudio.micmodel
    PLUGIN_MANUFACTURER_CODE FbSt
    PLUGIN_CODE FBMM
    FORMATS VST3 AU
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
)

# Generate JuceHeader.h (needed because the editor #includes <JuceHeader.h>).
juce_generate_juce_header(FamousBeatsMicModel)

target_sources(FamousBeatsMicModel PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(FamousBeatsMicModel
    PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(FamousBeatsMicModel
    PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ---- Test harness (Linux only, off by default) ----
# Builds test/mic_model_test.cpp against the plugin target and feeds
# generated audio through the model: silence, no clipping/NaN, and a
# sine sweep that must follow the intended difference curve (presence
# cut, warmth, air, HF rolloff), plus the tube-drive harmonic check.
# Enable with: cmake -B build -DFBS_BUILD_TESTS=ON
option(FBS_BUILD_TESTS "Build the mic model test harness" OFF)
if(FBS_BUILD_TESTS)
    add_executable(mic_model_test test/mic_model_test.cpp)
    target_link_libraries(mic_model_test PRIVATE
        FamousBeatsMicModel
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_dsp)
    # The test includes PluginProcessor.h, which uses explicit module
    # includes (no generated JuceHeader.h needed).
    target_include_directories(mic_model_test PRIVATE
        ${CMAKE_BINARY_DIR}/FamousBeatsMicModel_artefacts/JuceLibraryCode)
endif()
