#pragma once

#if __has_include("JuceHeader.h")
// Projucer build
#include "JuceHeader.h"
#else
// CMake build: include only the modules you need
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
// ... add more as needed
// #include "BinaryData.h" // Only if you use binary data in this file
#endif

#include <limits>
#include <cmath>

// 8-pole (48 dB/octave) low-pass filter using four cascaded biquads
class LowPass
{
public:
    LowPass();

    void prepare(double sampleRate, int samplesPerBlock);
    void setCutoff(float frequencyHz);
    void reset();
    float processSample(float input);

private:
    juce::dsp::IIR::Filter<float> filter1, filter2, filter3, filter4, filter5, filter6; // 8-pole filter (cascade)
    juce::dsp::IIR::Coefficients<float>::Ptr coeffs;
    float currentCutoff = 20000.0f; // Default to a safe, typical value
    double currentSampleRate = 44100.0;

    // Safe cutoff limits
    static constexpr float minCutoff = 30.0f;
    static constexpr float maxCutoffRatio = 0.49f; // 49% of sample rate (just below Nyquist)
};
