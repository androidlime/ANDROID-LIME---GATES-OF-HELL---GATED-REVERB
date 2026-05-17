cmake_minimum_required(VERSION 3.22)
project(MetallicPlugins VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.9
)
FetchContent_MakeAvailable(JUCE)

# ── Metallic EnVerb ────────────────────────────────────────────────────────
juce_add_plugin(MetallicEnVerb
    COMPANY_NAME            "YourName"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    PLUGIN_MANUFACTURER_CODE Yrnm
    PLUGIN_CODE             ANLE
    FORMATS                 VST3 Standalone
    PRODUCT_NAME            "Android Likes EnVerb"
    PLUGIN_CHANNEL_CONFIGURATIONS "{2, 2}"
)

target_sources(MetallicEnVerb PRIVATE
    Source/EnVerb/EVProcessor.cpp
    Source/EnVerb/EVEditor.cpp
)

target_compile_definitions(MetallicEnVerb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(MetallicEnVerb PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ── Metallic Digital Delay ─────────────────────────────────────────────────
juce_add_plugin(MetallicDD
    COMPANY_NAME            "YourName"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    PLUGIN_MANUFACTURER_CODE Yrnm
    PLUGIN_CODE             MtDD
    FORMATS                 VST3 Standalone
    PRODUCT_NAME            "Metallic Digital Delay"
    PLUGIN_CHANNEL_CONFIGURATIONS "{2, 2}"
)

target_sources(MetallicDD PRIVATE
    Source/DD/DDProcessor.cpp
    Source/DD/DDEditor.cpp
)

target_compile_definitions(MetallicDD PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(MetallicDD PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
