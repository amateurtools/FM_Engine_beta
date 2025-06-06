#include "LowPass.h"
#include <limits>
#include <cmath>

LowPass::LowPass()
    : currentSampleRate(44100.0),
      currentCutoff(1000.0f)
{
}

void LowPass::prepare(double sampleRate, int)
{
    jassert(sampleRate > std::numeric_limits<double>::epsilon());
    currentSampleRate = sampleRate;
    setCutoff(currentCutoff);
    filter1.reset();
    filter2.reset();
}

void LowPass::setCutoff(float frequencyHz)
{
    const float minCutoff = 20.0f; // Safe minimum for audio
    const float maxCutoff = static_cast<float>(currentSampleRate * 0.49f); // Just below Nyquist
    float safeCutoff = juce::jlimit(minCutoff, maxCutoff, frequencyHz);

    // Avoid unnecessary coefficient calculation if unchanged
    if (std::abs(currentCutoff - safeCutoff) > 0.01f)
        currentCutoff = safeCutoff;

    if (currentSampleRate > std::numeric_limits<double>::epsilon())
    {
        // Only set coefficients if cutoff is valid
        coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, currentCutoff);
        *filter1.coefficients = *coeffs;
        *filter2.coefficients = *coeffs;
    }
}

void LowPass::reset()
{
    filter1.reset();
    filter2.reset();
}

float LowPass::processSample(float input)
{
    float y = filter1.processSample(input);
    y = filter2.processSample(y);

    // Sanitize output to avoid NaN/Inf propagation
    if (!std::isfinite(y))
        y = 0.0f;

    return y;
}
