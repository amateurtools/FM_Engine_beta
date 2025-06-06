cmake_minimum_required(VERSION 3.15)

project(FM_Engine_beta VERSION 0.6.6)

# Add JUCE as a subdirectory
add_subdirectory(JUCE)

juce_add_plugin(FM_Engine_beta
    COMPANY_NAME "AmateurToolsDSP"
    COMPANY_WEBSITE "https://github.com/joshgura/"
    COMPANY_EMAIL "amateurtools@gmail.com"
    COMPANY_COPYRIGHT "2014-2025"
    IS_SYNTH FALSE                   # Audio effect, not a synth
    NEEDS_MIDI_INPUT FALSE           # No MIDI input needed
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Amtr
    PLUGIN_CODE FMeb
    FORMATS VST3
    PRODUCT_NAME "FM_Engine_beta"
)

target_sources(FM_Engine_beta
    PRIVATE
        Source/InterpolatedDelay.h
        Source/Dial.cpp
        Source/Dial.h
        Source/LowPass.cpp
        Source/LowPass.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/Routing.cpp
        Source/Routing.h
        Source/SidewaysToggleSwitch.h
        Source/SlidingSwitch.cpp
        Source/SlidingSwitch.h
)

juce_add_binary_data(FM_Engine_beta_BinaryData
    SOURCES
        BinaryData/background.png
)

target_link_libraries(FM_Engine_beta
    PRIVATE
        FM_Engine_beta_BinaryData
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_compile_definitions(FM_Engine_beta
    PUBLIC
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        # Use effect category for audio processing plugin
        JucePlugin_VSTCategory=kPlugCategEffect
)
