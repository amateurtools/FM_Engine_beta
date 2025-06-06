cmake_minimum_required(VERSION 3.15)
project(FM_Engine_beta VERSION 0.06062025)

add_definitions(-DJUCE_WEB_BROWSER=0)

add_subdirectory(JUCE) # JUCE source in ./JUCE

# --- GTK3 workaround: force CMake to find and use GTK3 via pkg-config ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER})
# ------------------------------------------------------------------------

# --- Embed your PNG as binary data ---
juce_add_binary_data(BinaryData SOURCES
    BinaryData/background.png
    # Add other resource files here if needed
)

juce_add_plugin(FM_Engine_beta
    COMPANY_NAME "AmateurToolsDSP"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    NEEDS_WEB_BROWSER FALSE
    IS_MIDI_EFFECT FALSE
    PLUGIN_MANUFACTURER_CODE Amat # 4-char, at least one uppercase
    PLUGIN_CODE FMeb             # 4-char, at least one uppercase
    FORMATS VST3
    PRODUCT_NAME "FM_Engine_beta"
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(FM_Engine_beta PRIVATE
    Source/Dial.cpp
    Source/LowPass.cpp
    Source/PluginEditor.cpp
    Source/PluginProcessor.cpp
    Source/Routing.cpp
    Source/SlidingSwitch.cpp
    Source/InterpolatedDelay.h
    Source/Dial.h
    Source/LowPass.h
    Source/PluginEditor.h
    Source/PluginProcessor.h
    Source/Routing.h
    Source/SidewaysToggleSwitch.h
    Source/SlidingSwitch.h
)

target_compile_definitions(FM_Engine_beta PUBLIC
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(FM_Engine_beta PRIVATE
    BinaryData
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
    curl
)
