cmake_minimum_required(VERSION 3.15)
project(FM_Engine_beta VERSION 0.06062025)

add_definitions(-DJUCE_WEB_BROWSER=0)

add_subdirectory(JUCE) # JUCE source in ./JUCE

if(APPLE)
    find_package(CURL)
    if(CURL_FOUND)
        message(STATUS "libcurl found on macOS, will link.")
        set(PLATFORM_LIBS CURL::libcurl)
    else()
        message(STATUS "libcurl not found on macOS, will not link.")
        set(PLATFORM_LIBS "")
    endif()
elseif(NOT APPLE)
    set(PLATFORM_LIBS curl)
endif()

# --- GTK3 workaround: force CMake to find and use GTK3 via pkg-config (Linux only) ---
if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    include_directories(${GTK3_INCLUDE_DIRS})
    link_directories(${GTK3_LIBRARY_DIRS})
    add_definitions(${GTK3_CFLAGS_OTHER})
endif()
# ------------------------------------------------------------------------------------

# --- Embed your PNG as binary data ---
juce_add_binary_data(BinaryData SOURCES
    BinaryData/background.png
    BinaryData/knob.png
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
    FORMATS VST3 AU              # Added AU format for macOS
    PRODUCT_NAME "FM_Engine_beta"
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(FM_Engine_beta PRIVATE
    Source/LowPass.cpp
    Source/PluginEditor.cpp
    Source/PluginProcessor.cpp
    Source/Routing.cpp
    Source/SlidingSwitch.cpp
    Source/InterpolatedDelay.h
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

juce_generate_juce_header(FM_Engine_beta)

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
)

# Only link curl on non-macOS systems if needed
if(NOT APPLE)
    target_link_libraries(FM_Engine_beta PRIVATE curl)
endif()

# =============================================================================
# Compiler Optimizations (macOS-compatible)
# =============================================================================

# Set optimization flags based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if(APPLE)
        # macOS optimizations - Apple Silicon (ARM) and Intel compatible
        target_compile_options(FM_Engine_beta PRIVATE
            -O3                    # Maximum optimization
            -ffast-math            # Aggressive floating-point optimizations
            -funroll-loops         # Unroll loops for speed
            -fno-math-errno        # Don't set errno for math functions
            -ffinite-math-only     # Assume finite math (no NaN/Inf checks)
        )
        
        # ARM-specific optimizations for Apple Silicon
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            message(STATUS "Building for Apple Silicon (ARM64)")
            target_compile_options(FM_Engine_beta PRIVATE
                -mcpu=apple-m1     # Optimize for Apple Silicon (M1/M2/M3)
            )
        else()
            # Intel Mac optimizations - use mtune instead of march
            message(STATUS "Building for Intel Mac")
            target_compile_options(FM_Engine_beta PRIVATE
                //-mtune=native      # Optimize for your specific Intel CPU (Clang compatible)
                -msse4.2           # Enable SSE 4.2 instructions
            )
        endif()
        
    elseif(MSVC)
        # Visual Studio / Windows
        target_compile_options(FM_Engine_beta PRIVATE
            /O2          # Maximize speed
            /Oi          # Enable intrinsic functions
            /Ot          # Favor fast code
            /GL          # Whole program optimization
            /fp:fast     # Fast floating point (like -ffast-math)
            /arch:AVX2   # Enable AVX2 instructions (or AVX, SSE2 for older CPUs)
        )
        
        # Link-time optimizations
        target_link_options(FM_Engine_beta PRIVATE
            /LTCG        # Link-time code generation
        )
        
    else()
        # GCC / Clang / Linux
        target_compile_options(FM_Engine_beta PRIVATE
            -O3                    # Maximum optimization
            -march=native          # Optimize for your CPU (use CPU-specific SIMD)
            -ffast-math            # Aggressive floating-point optimizations
            -funroll-loops         # Unroll loops for speed
            -fno-math-errno        # Don't set errno for math functions
            -ffinite-math-only     # Assume finite math (no NaN/Inf checks)
        )
    endif()
    
    # Enable interprocedural optimization (IPO/LTO) if supported
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set_target_properties(FM_Engine_beta PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION TRUE
        )
        message(STATUS "IPO/LTO enabled for FM_Engine_beta")
    else()
        message(STATUS "IPO/LTO not supported: ${ipo_error}")
    endif()
    
else()
    # Debug build - minimal optimizations, better debugging
    message(STATUS "Building in Debug mode - optimizations disabled")
endif()

# =============================================================================
# JUCE-specific optimizations
# =============================================================================

# Enable JUCE's SIMD optimizations
target_compile_definitions(FM_Engine_beta PRIVATE
    JUCE_USE_SIMD=1                    # Enable JUCE SIMD where available
)

# Enable vDSP on macOS for accelerated DSP operations
if(APPLE)
    target_compile_definitions(FM_Engine_beta PRIVATE
        JUCE_DSP_USE_VDSP_FRAMEWORK=1  # Use Apple's Accelerate framework
    )
endif()

# Disable debug features in Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(FM_Engine_beta PRIVATE
        JUCE_DISABLE_ASSERTIONS=1      # Remove assertion checks
        NDEBUG=1                       # Standard release flag
    )
endif()

# =============================================================================
# Universal Binary for macOS (Intel + Apple Silicon)
# =============================================================================
if(APPLE)
    # Build universal binary that runs on both Intel and Apple Silicon
    set_target_properties(FM_Engine_beta PROPERTIES
        OSX_ARCHITECTURES "arm64;x86_64"
    )
    message(STATUS "Building Universal Binary (arm64 + x86_64)")
endif()

# =============================================================================
# Build type message
# =============================================================================
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
if(NOT CMAKE_BUILD_TYPE)
    message(WARNING "CMAKE_BUILD_TYPE not set! Use: cmake -B build -DCMAKE_BUILD_TYPE=Release")
endif()