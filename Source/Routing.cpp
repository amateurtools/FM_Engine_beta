#if __has_include("JuceHeader.h")
// Projucer build
#include "JuceHeader.h"
#else
// CMake build: include only the modules you need
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
// ... add more as needed
// #include "BinaryData.h" // Only if you use binary data in this file
#endif

// Routing.cpp
#include "Routing.h"
#include <algorithm>  // For std::swap
#include <cmath>      // For std::atan


// Removed softClip from here as per user's request to move to PluginProcessor.cpp

RoutingOutputs routeSample(float L, float R, float SC_L, float SC_R, int algorithm, int invert)
{
    RoutingOutputs output;
    output.sideChain.left = SC_L;
    output.sideChain.right = SC_R;

    StereoSample carrier {};
    StereoSample modulator {};

    switch (algorithm)
    {
        case 0:  // algorithm 1: L = carrier, R = modulator (mono)
            carrier.left = L;
            carrier.right = L;
            modulator.left = R; // softClip removed
            modulator.right = R;
            break;

        case 1:  // algorithm 2: mono mix of L+R and SC_L+SC_R
            {
                float carrierMono = (L + R) * 0.5f;
                float modulatorMono = (SC_L + SC_R) * 0.5f; // softClip removed
                carrier.left = carrierMono;
                carrier.right = carrierMono;
                modulator.left = modulatorMono;
                modulator.right = modulatorMono;
            }
            break;

        case 2:  // algorithm 3: full stereo
            carrier.left = L;
            carrier.right = R;
            modulator.left = SC_L; // softClip removed
            modulator.right = SC_R; // softClip removed
            break;

        default:  // fallback to algo 1
            carrier.left = L;
            carrier.right = 0.0f;
            modulator.left = R; // softClip removed
            modulator.right = 0.0f;
            break;
    }

    if (invert == 1)
    {
        // Swap carrier and modulator if invert is active
        std::swap(carrier.left, modulator.left);
        std::swap(carrier.right, modulator.right);
    }

    output.carrier = carrier;
    output.modulator = modulator;

    return output;
}