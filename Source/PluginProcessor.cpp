/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath> // for std::tanh

//==============================================================================
// Static helper function to create parameters for the AudioProcessorValueTreeState
juce::AudioProcessorValueTreeState::ParameterLayout VPM2AudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"MOD_DEPTH", 1}, "Modulation Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 0.5f), // skew = 0.5f
        0.0f
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"MAX_DELAY_MS", 1}, "Range (Max Delay)",
        juce::NormalisableRange<float>(100.0f, 900.0f, 100.0f),
        500.0f,
        " ms"
    ));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"ALGORITHM", 1}, "Algorithm",
        juce::StringArray({ "Algo 1 (L=Car, R=Mod)", "Algo 2 (Mono Mix)", "Algo 3 (Stereo)" }),
        0
    ));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"INVERT", 1}, "Invert", false
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"LP_CUTOFF", 1}, "Lowpass Cutoff",
        juce::NormalisableRange<float>(
            20.0f, 20000.0f,
            [](float start, float end, float normalised) {
                return start * std::pow(end / start, normalised);
            },
            [](float start, float end, float value) {
                return std::log(value / start) / std::log(end / start);
            }
        ),
        20000.0f // default value
    ));    

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"PDC", 1}, "PDC", false
    ));

    // Interpolation Type: 0 = Linear (default), 1 = Lagrange
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"INTERPOLATION_TYPE", 1}, "Interpolation Type", false
    ));

    // Oversampling: 0 = Off (default), 1 = On
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"OVERSAMPLING", 1}, "Oversampling", false
    ));

    return { params.begin(), params.end() };
}


// Apply arctangent soft clipping: input can be any float, output will be in [-1, 1]
static float softClip(float x)
{
    return std::atan(x) * (2.0f / juce::MathConstants<float>::pi);
}

//==============================================================================
VPM2AudioProcessor::VPM2AudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)   // Main input
        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)  // Sidechain input
        .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)), // Main output
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    // Attach listeners to parameters
    modDepthParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("MOD_DEPTH"));
    maxDelayMsParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("MAX_DELAY_MS"));
    algorithmParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("ALGORITHM"));
    invertParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("INVERT"));
    lpCutoffParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("LP_CUTOFF"));
    interpolationTypeParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("INTERPOLATION_TYPE"));
    oversamplingParam      = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("OVERSAMPLING"));  

    jassert(modDepthParam);
    jassert(maxDelayMsParam);
    jassert(algorithmParam);
    jassert(invertParam);
    jassert(lpCutoffParam);
    jassert(interpolationTypeParam);
    jassert(oversamplingParam);

    apvts.addParameterListener("MOD_DEPTH", this);
    apvts.addParameterListener("MAX_DELAY_MS", this);
    apvts.addParameterListener("ALGORITHM", this);
    apvts.addParameterListener("INVERT", this);
    apvts.addParameterListener("LP_CUTOFF", this);
    apvts.addParameterListener("INTERPOLATION_TYPE", this);
    apvts.addParameterListener("OVERSAMPLING", this);
}

VPM2AudioProcessor::~VPM2AudioProcessor()
{
    apvts.removeParameterListener("MOD_DEPTH", this);
    apvts.removeParameterListener("MAX_DELAY_MS", this);
    apvts.removeParameterListener("ALGORITHM", this);
    apvts.removeParameterListener("INVERT", this);
    apvts.removeParameterListener("LP_CUTOFF", this);
    apvts.removeParameterListener("INTERPOLATION_TYPE", this);
    apvts.removeParameterListener("OVERSAMPLING", this);

}

#include "PluginProcessor.h" // Make sure this is included
#include "JuceHeader.h"      // Or your main JUCE header for juce::jlimit etc.

void VPM2AudioProcessor::parameterChanged(const juce::String& parameterID, float /*newValue*/)
{
    // Important: The newValue argument here is ALWAYS the normalized (0.0-1.0) value.
    // To get the actual raw, de-normalized, and stepped value,
    // you must retrieve it from the AudioProcessorValueTreeState.

    const juce::SpinLock::ScopedLockType lock (delayStateLock); // Protect shared state

    // MOD_DEPTH (your "Amount" parameter) - Scales modulator's influence
    if (parameterID == "MOD_DEPTH")
    {
        // Get the actual raw value of MOD_DEPTH from the APVTS.
        // It's already correctly ranged (0.0-1.0) and has a step of 0.01 from your layout.
        float currentModDepth = *apvts.getRawParameterValue("MOD_DEPTH");
        // Use currentModDepth wherever you apply modulation depth in your DSP
        // Example: modulator.setDepth(currentModDepth);
        // DBG("MOD_DEPTH set to: " << currentModDepth); // For debugging
    }
    // MAX_DELAY_MS (your "Range" parameter) - Sets the maximum delay time and buffer size
    else if (parameterID == "MAX_DELAY_MS")
    {
        // Get the actual raw, stepped value of MAX_DELAY_MS from the APVTS.
        // Your NormalisableRange (100.0f, 900.0f, 100.0f) ensures it's already snapped.
        float currentMaxDelayMs = *apvts.getRawParameterValue("MAX_DELAY_MS");
        delayL.setMaxDelayMs(currentMaxDelayMs);
        delayR.setMaxDelayMs(currentMaxDelayMs);
        // DBG("MAX_DELAY_MS set to: " << currentMaxDelayMs); // For debugging
    }
    // ALGORITHM - For Routing.cpp
    else if (parameterID == "ALGORITHM")
    {
        // AudioParameterChoice parameters store their index as a float (0.0, 1.0, 2.0 etc.)
        // This value is already the correct raw index from your NormalisableRange definition.
        int currentAlgorithmIndex = static_cast<int>(*apvts.getRawParameterValue("ALGORITHM"));
        // Use currentAlgorithmIndex for your routing logic
        // Example: router.setAlgorithm(currentAlgorithmIndex);
        // DBG("ALGORITHM set to: " << currentAlgorithmIndex); // For debugging
    }
    // INVERT - For Routing.cpp
    else if (parameterID == "INVERT")
    {
        bool isInverted = (*apvts.getRawParameterValue("INVERT") > 0.5f);
        // Use isInverted for your routing logic
        // router.setInvert(isInverted);
    }
    
    // LP_CUTOFF - For Modulator LowPass Filter
    else if (parameterID == "LP_CUTOFF")
    {
        // Get the actual raw, de-normalized value for LP_CUTOFF.
        // Your NormalisableRange handles the skewing.

        // DBG("LP_CUTOFF set to: " << currentLPCutoff); // For debugging
    }

}



//==============================================================================
void VPM2AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    oversampler.initProcessing(samplesPerBlock);
    oversampler.reset();
    
    // Store the max samples per block for assertions and buffer sizing
    currentMaxBlockSize = samplesPerBlock; 
    // ADDED DBG LINE:
    DBG("prepareToPlay: samplesPerBlock received = " << samplesPerBlock << ", currentMaxBlockSize set to = " << currentMaxBlockSize);

    // Prepare DSP components - Use BASE sample rate for delay preparation
    modulatorLowPassL.prepare(sampleRate, samplesPerBlock);
    modulatorLowPassR.prepare(sampleRate, samplesPerBlock);

    float safeMaxDelay = juce::jlimit(100.0f, 900.0f, maxDelayMsParam->get()); // avoids annoying crash
    
    // FIXED: Use oversampled sample rate for delay preparation when oversampling is enabled
    double finalSampleRate = sampleRate;
    if (*apvts.getRawParameterValue("OVERSAMPLING") > 0.5f)
    {
        finalSampleRate = sampleRate; oversampler.getOversamplingFactor();
    }
    
    delayL.prepare(finalSampleRate, safeMaxDelay);
    delayR.prepare(finalSampleRate, safeMaxDelay);

    delayL.setInterpolationType(InterpolatedDelay::InterpolationType::Lagrange);
    delayR.setInterpolationType(InterpolatedDelay::InterpolationType::Lagrange);

    // Initialize the silent sidechain buffer (conceptually a member variable)
    // This assumes silentSidechainBuffer is a member of VPM2AudioProcessor
    silentSidechainBuffer.setSize(2, currentMaxBlockSize, false, true, true);
    silentSidechainBuffer.clear(); // Fill with zeros

    modulatorLowPassL.setCutoff(lpCutoffParam->get()); // Using setCutoff
    modulatorLowPassR.setCutoff(lpCutoffParam->get()); // Using setCutoff

    // Reset components if flags are set (e.g., after loading preset or parameter change requiring full reset)
    if (shouldResetDelay)
    {
        delayL.reset();
        delayR.reset();
        shouldResetDelay = false;
    }

    if (shouldResetLowPass)
    {
        modulatorLowPassL.reset();
        modulatorLowPassR.reset();
        shouldResetLowPass = false;
    }

    // PDC calculation: max delay (samples) + fixed HalfRateFilter latency
    // HalfRateFilter latency is about 3 samples at original sample rate for 3rd order
    // (This is an approximation; exact latency depends on implementation)
    // The delay line latency is based on the max possible delay, not the current modulated delay.
    int maxDelayLineLatencySamples = static_cast<int>(maxDelayMsParam->get() * 0.001f * sampleRate);
    int hrfFixedLatencySamples = 3; // Approx fixed latency of HalfRateFilter
    setLatencySamples((maxDelayLineLatencySamples / 2) + hrfFixedLatencySamples);

    // Smoothing times in milliseconds (adjust to taste)
    float modDepthSmoothingTimeMs = 10.0f;
    float cutoffSmoothingTimeMs = 50.0f;

    modDepthSmoothingCoeff = std::exp(-1.0f / (0.001f * modDepthSmoothingTimeMs * sampleRate));

    // Initialize smoothed values to current parameter values
    smoothedModDepth = *apvts.getRawParameterValue("MOD_DEPTH");

    // smooth out the insane cutoff madness to prevent LPF explosions
    smoothedCutoff.reset(sampleRate, cutoffSmoothingTimeMs * 0.001f); 
    smoothedCutoff.setTargetValue (*apvts.getRawParameterValue ("LP_CUTOFF"));
}

void VPM2AudioProcessor::releaseResources()
{
    // Only clear buffers that are actually used
    silentSidechainBuffer.setSize(0, 0); // If you use this buffer

    // Reset your DSP components to clear their internal states/buffers
    delayL.reset();
    delayR.reset();
    modulatorLowPassL.reset();
    modulatorLowPassR.reset();

    // Add any other resource cleanup your plugin requires
}

// bool VPM2AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
// {
//     auto inputChannels = layouts.getMainInputChannelSet();
//     auto outputChannels = layouts.getMainOutputChannelSet();
    
//     // Debug: Log what the host is requesting
//     DBG("Host requesting input channels: " << inputChannels.getDescription());
//     DBG("Host requesting output channels: " << outputChannels.getDescription());
    
//     // Accept exactly 4 input channels (be strict to force host compliance)
//     bool validInput = (inputChannels == juce::AudioChannelSet::discreteChannels(4));
    
//     // Require exactly 4 output channels
//     bool validOutput = (outputChannels == juce::AudioChannelSet::discreteChannels(4));
    
//     bool result = validInput && validOutput;
//     DBG("Bus layout supported: " << (result ? "YES" : "NO"));
    
//     return result;
// }

bool VPM2AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto inputChannels = layouts.getMainInputChannelSet();
    auto outputChannels = layouts.getMainOutputChannelSet();

    DBG("Host requesting input channels: " << inputChannels.getDescription() 
        << " (count: " << inputChannels.size() << ")");
    DBG("Host requesting output channels: " << outputChannels.getDescription()
        << " (count: " << outputChannels.size() << ")");

    bool validInput = (inputChannels == juce::AudioChannelSet::mono() ||
                       inputChannels == juce::AudioChannelSet::stereo() ||
                       inputChannels == juce::AudioChannelSet::discreteChannels(4));

    bool validOutput = (outputChannels == juce::AudioChannelSet::stereo());

    bool result = validInput && validOutput;
    DBG("Bus layout supported: " << (result ? "YES" : "NO"));

    return result;
}

//==============================================================================
// 4. ENHANCED processBlock with better channel debugging

void VPM2AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // --- Buffer and channel count checks ---
    jassert(numSamples > 0);
    jassert(numChannels > 0);

    // Local block buffers
    juce::AudioBuffer<float> routedBuffer(4, numSamples);
    juce::AudioBuffer<float> tempProcessingBuffer(3, numSamples);

    std::vector<float> normalizedModL(numSamples, 0.0f);
    std::vector<float> normalizedModR(numSamples, 0.0f);

    // Parameter checks
    float maxDelayMs = *apvts.getRawParameterValue("MAX_DELAY_MS");
    jassert(std::isfinite(maxDelayMs) && maxDelayMs > 0.0f);
    delayL.setMaxDelayMs(maxDelayMs);
    delayR.setMaxDelayMs(maxDelayMs);

    // Debug: Log actual channel count received
    static int debugCounter = 0;
    if (debugCounter++ % 1000 == 0)
    {
        DBG("processBlock: received " << numChannels << " channels, " 
            << numSamples << " samples");
    }

    if (numSamples <= 0)
        return;

    // CRITICAL: Check if we actually have 4 input channels
    if (numChannels < 4)
    {
        DBG("WARNING: Only received " << numChannels << " channels, expecting 4!");
        buffer.setSize(4, numSamples, true, true, true);
    }
    jassert(buffer.getNumChannels() >= 4);

    // Main input: always bus 0
    auto mainInput = getBusBuffer(buffer, true, 0);
    jassert(mainInput.getNumChannels() > 0);

    // Sidechain input: bus 1 (if present)
    juce::AudioBuffer<float> sidechainInput;
    if (getBusCount(true) > 1)
        sidechainInput = getBusBuffer(buffer, true, 1);

    // Defensive: Check sidechain buffer
    jassert(sidechainInput.getNumChannels() <= 2);

    const float* inL = mainInput.getReadPointer(0);
    const float* inR = (mainInput.getNumChannels() > 1) ? mainInput.getReadPointer(1) : nullptr;
    const float* scL = (sidechainInput.getNumChannels() > 0) ? sidechainInput.getReadPointer(0) : nullptr;
    const float* scR = (sidechainInput.getNumChannels() > 1) ? sidechainInput.getReadPointer(1) : nullptr;

    // Debug: Check if sidechain channels have actual audio
    if (scL && scR && debugCounter % 1000 == 0)
    {
        float scL_rms = 0.0f, scR_rms = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            jassert(i < numSamples);
            scL_rms += scL[i] * scL[i];
            scR_rms += scR[i] * scR[i];
        }
        scL_rms = std::sqrt(scL_rms / numSamples);
        scR_rms = std::sqrt(scR_rms / numSamples);

        if (scL_rms > 0.001f || scR_rms > 0.001f)
        {
            DBG("Sidechain activity detected! L RMS: " << scL_rms << ", R RMS: " << scR_rms);
        }
    }

    int algorithm = algorithmParam->getIndex();

    float modDepth = *apvts.getRawParameterValue("MOD_DEPTH");
    jassert(std::isfinite(modDepth));

    // Get pointers to routed buffer channels
    auto* routedCarrierL = routedBuffer.getWritePointer(0);
    auto* routedCarrierR = routedBuffer.getWritePointer(1);
    auto* routedModL     = routedBuffer.getWritePointer(2);
    auto* routedModR     = routedBuffer.getWritePointer(3);

    jassert(routedCarrierL != nullptr);
    jassert(routedCarrierR != nullptr);
    jassert(routedModL != nullptr);
    jassert(routedModR != nullptr);

    bool invert = (*apvts.getRawParameterValue("INVERT") > 0.5f);

    bool lagrange = apvts.getRawParameterValue("INTERPOLATION_TYPE")->load();
    InterpolatedDelay::InterpolationType interpType = lagrange
        ? InterpolatedDelay::InterpolationType::Lagrange
        : InterpolatedDelay::InterpolationType::Linear;
    delayL.setInterpolationType(interpType);
    delayR.setInterpolationType(interpType);

    bool oversamplingEnabled = (*apvts.getRawParameterValue("OVERSAMPLING") > 0.5f);

    const float fadeTimeSamples = lpfSoloFadeTimeMs * 0.001f * getSampleRate();
    const float fadeStep = (fadeTimeSamples > 0.0f) ? (1.0f / fadeTimeSamples) : 1.0f;

    auto sanitize = [](float x) -> float { return std::isfinite(x) ? x : 0.0f; };

    // --- FIXED: Safe routing with null pointer protection ---
    for (int i = 0; i < numSamples; ++i)
    {
        jassert(i < numSamples);
        float carrierL = inL ? inL[i] : 0.0f;
        float carrierR = inR ? inR[i] : carrierL;
        float modL = scL ? scL[i] : 0.0f;
        float modR = scR ? scR[i] : modL;

        RoutingOutputs rout = routeSample(carrierL, carrierR, modL, modR, algorithm, invert);
        routedCarrierL[i] = rout.carrier.left;
        routedCarrierR[i] = rout.carrier.right;
        routedModL[i]     = rout.modulator.left;
        routedModR[i]     = rout.modulator.right;

        jassert(std::isfinite(routedCarrierL[i]));
        jassert(std::isfinite(routedCarrierR[i]));
        jassert(std::isfinite(routedModL[i]));
        jassert(std::isfinite(routedModR[i]));
    }

    // --- PRE-PROCESS MODULATOR: Smoothing, Cubic Clipping, Lowpass ---

    auto* smoothedModDepthBuffer = tempProcessingBuffer.getWritePointer(0);

    auto* processedModL = tempProcessingBuffer.getWritePointer(1);
    auto* processedModR = tempProcessingBuffer.getWritePointer(2);

    jassert(smoothedModDepthBuffer != nullptr);
    jassert(processedModL != nullptr);
    jassert(processedModR != nullptr);

    float smoothedModDepthLocal = smoothedModDepth;

    smoothedCutoff.setTargetValue(*apvts.getRawParameterValue("LP_CUTOFF"));

    auto tanhSoftClip = [](float x) -> float {
        return std::tanh(x);
    };
    
    for (int i = 0; i < numSamples; ++i)
    {
        // Buffer bounds checks (for all arrays and pointers sized to numSamples)
        jassert(i >= 0 && i < numSamples);
        jassert(i < routedBuffer.getNumSamples()); // For routedBuffer
    
        // Parameter smoothing
        smoothedModDepthLocal = (1.0f - modDepthSmoothingCoeff) * modDepth
                              + modDepthSmoothingCoeff * smoothedModDepthLocal;
        smoothedModDepthBuffer[i] = smoothedModDepthLocal;
        jassert(std::isfinite(smoothedModDepthLocal));
    
        // Sanitize and process modulator
        float safeModL = sanitize(routedModL[i]);
        float safeModR = sanitize(routedModR[i]);
        jassert(std::isfinite(safeModL));
        jassert(std::isfinite(safeModR));
    
        float smoothedCutoffValue = smoothedCutoff.getNextValue();
        jassert(std::isfinite(smoothedCutoffValue));
        modulatorLowPassL.setCutoff(smoothedCutoffValue);
        modulatorLowPassR.setCutoff(smoothedCutoffValue);
    
        // Filter first
        float filteredL = modulatorLowPassL.processSample(safeModL);
        float filteredR = modulatorLowPassR.processSample(safeModR);
        jassert(std::isfinite(filteredL));
        jassert(std::isfinite(filteredR));
    
        // Then tanh soft clip
        float clippedModL = tanhSoftClip(filteredL);
        float clippedModR = tanhSoftClip(filteredR);
        jassert(std::isfinite(clippedModL));
        jassert(std::isfinite(clippedModR));
    
        // === APPLY MOD DEPTH HERE ===
        float depthModL = clippedModL * smoothedModDepthLocal;
        float depthModR = clippedModR * smoothedModDepthLocal;
        jassert(std::isfinite(depthModL));
        jassert(std::isfinite(depthModR));
    
        // Sanitize after depth (optional, for safety)
        processedModL[i] = sanitize(depthModL);
        processedModR[i] = sanitize(depthModR);
        jassert(std::isfinite(processedModL[i]));
        jassert(std::isfinite(processedModR[i]));
    
        // --- NORMALIZE and store for later use ---
        normalizedModL[i] = (processedModL[i] + 1.0f) * 0.5f;
        normalizedModR[i] = (processedModR[i] + 1.0f) * 0.5f;
        jassert(std::isfinite(normalizedModL[i]));
        jassert(std::isfinite(normalizedModR[i]));
        jassert(normalizedModL[i] >= 0.0f && normalizedModL[i] <= 1.0f);
        jassert(normalizedModR[i] >= 0.0f && normalizedModR[i] <= 1.0f);
    
        // (Optional) Store in routed buffer if needed for oversampling
        jassert(routedBuffer.getNumChannels() > 2);
        jassert(routedBuffer.getNumChannels() > 3);
        routedBuffer.setSample(2, i, processedModL[i]);
        routedBuffer.setSample(3, i, processedModR[i]);
    }
    
    
    
    // Save smoothed state for next block
    smoothedModDepth = smoothedModDepthLocal;
    

    if (oversamplingEnabled)
    {
        // --- OVERSAMPLING UP ---
        juce::dsp::AudioBlock<float> routedBlock(routedBuffer);
        auto oversampledBlock = oversampler.processSamplesUp(routedBlock);
    
        // --- DELAY PROCESSING (INSIDE OVERSAMPLED BLOCK) ---
        const int osFactor  = oversampler.getOversamplingFactor();
        const int osSamples = numSamples * osFactor;
    
        jassert(osSamples <= oversampledBlock.getNumSamples());
        jassert(oversampledBlock.getNumChannels() >= 4);
    
        auto* osCarrierL = oversampledBlock.getChannelPointer(0);
        auto* osCarrierR = oversampledBlock.getChannelPointer(1);
        auto* osModL     = oversampledBlock.getChannelPointer(2);
        auto* osModR     = oversampledBlock.getChannelPointer(3);
    
        for (int i = 0; i < osSamples; ++i)
        {
            int sampleIdx = i / osFactor;
            jassert(sampleIdx >= 0 && sampleIdx < numSamples);
            sampleIdx = juce::jmin(sampleIdx, numSamples - 1);
    
            // Use precomputed normalized value
            jassert(sampleIdx < normalizedModL.size());
            jassert(sampleIdx < normalizedModR.size());
            float outL = delayL.process(osCarrierL[i], normalizedModL[sampleIdx]);
            float outR = delayR.process(osCarrierR[i], normalizedModR[sampleIdx]);
    
            jassert(std::isfinite(outL));
            jassert(std::isfinite(outR));
            osCarrierL[i] = std::isfinite(outL) ? outL : 0.0f;
            osCarrierR[i] = std::isfinite(outR) ? outR : 0.0f;
        }
    
        // --- OVERSAMPLING DOWN ---
        oversampler.processSamplesDown(routedBlock);
    }
    else
    {
        // No oversampling: process directly on routedBuffer
        jassert(routedBuffer.getNumChannels() >= 2);
        jassert(routedBuffer.getNumSamples() >= numSamples);
    
        auto* carrierL = routedBuffer.getWritePointer(0);
        auto* carrierR = routedBuffer.getWritePointer(1);
    
        for (int i = 0; i < numSamples; ++i)
        {
            jassert(i < normalizedModL.size());
            jassert(i < normalizedModR.size());
            float outL = delayL.process(carrierL[i], normalizedModL[i]);
            float outR = delayR.process(carrierR[i], normalizedModR[i]);
    
            jassert(std::isfinite(outL));
            jassert(std::isfinite(outR));
            carrierL[i] = std::isfinite(outL) ? outL : 0.0f;
            carrierR[i] = std::isfinite(outR) ? outR : 0.0f;
        }
    }
    
    // stuff that has to do with smoothly crossfading the LPF solo function
    const float targetFade = bypassOversampling ? 1.0f : 0.0f;
    if (lpfSoloFade < targetFade)
        lpfSoloFade = std::min(targetFade, lpfSoloFade + fadeStep * numSamples);
    else if (lpfSoloFade > targetFade)
        lpfSoloFade = std::max(targetFade, lpfSoloFade - fadeStep * numSamples);
    
    float fadeMix = 0.5f * (1.0f - std::cos(lpfSoloFade * juce::MathConstants<float>::pi));
    
    // ============= FIXED OUTPUT SECTION - NO MORE VECTOR BOUNDS VIOLATIONS =============
    for (int i = 0; i < numSamples; ++i)
    {
        jassert(i < routedBuffer.getNumSamples());
        jassert(i < numSamples); // for processedModL and processedModR if they are float*

        // Normal output (what you would normally send to buffer)
        float normalL = routedBuffer.getSample(0, i);
        float normalR = routedBuffer.getSample(1, i);

        // LPF solo output (the filtered modulator) - NOW SAFE!
        float lpfL = processedModL[i];
        float lpfR = processedModR[i];

        jassert(std::isfinite(normalL));
        jassert(std::isfinite(normalR));
        jassert(std::isfinite(lpfL));
        jassert(std::isfinite(lpfR));

        // Crossfade
        float outL = (1.0f - fadeMix) * normalL + fadeMix * lpfL;
        float outR = (1.0f - fadeMix) * normalR + fadeMix * lpfR;

        jassert(std::isfinite(outL));
        jassert(std::isfinite(outR));

        // Safe output - ensure we have enough channels
        if (buffer.getNumChannels() > 0)
        {
            jassert(i < buffer.getNumSamples());
            buffer.setSample(0, i, outL);
        }
        if (buffer.getNumChannels() > 1)
        {
            jassert(i < buffer.getNumSamples());
            buffer.setSample(1, i, outR);
        }
    }

    // --- LATENCY REPORTING ---
    bool pdcEnabled = (*apvts.getRawParameterValue("PDC") > 0.5f);
    double sampleRate = getSampleRate();

    int pdcSamples = 0;
    if (pdcEnabled)
        pdcSamples = static_cast<int>(0.5 * maxDelayMs * 0.001 * sampleRate);

    if (pdcSamples != lastReportedLatency)
    {
        setLatencySamples(pdcSamples);
        lastReportedLatency = pdcSamples;
    }
}

//==============================================================================
bool VPM2AudioProcessor::hasEditor() const
{
    return true; // Always return true if your plugin has a GUI
}

juce::AudioProcessorEditor* VPM2AudioProcessor::createEditor()
{
    return new VpmGUIAudioProcessorEditor(*this); // Replace with your editor class name
    //return new juce::GenericAudioProcessorEditor(*this); // Use JUCE's safe, generic editor
}

//==============================================================================
void VPM2AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // DBG("getStateInformation called.");
    juce::ValueTree params = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (params.createXml());
    copyXmlToBinary (*xml, destData);
    // DBG("getStateInformation received.");
}

void VPM2AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // DBG("setStateInformation called.");
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
            shouldResetDelay = true;
            shouldResetLowPass = true;
            // DBG("  State loaded, shouldResetDelay and shouldResetLowPass set to true.");
        }
    }
}

const juce::String VPM2AudioProcessor::getName() const
{
    return JucePlugin_Name;
    // DBG("JucePlugin_Name gotten");
}

double VPM2AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

bool VPM2AudioProcessor::acceptsMidi() const
{
    return false;
}

bool VPM2AudioProcessor::producesMidi() const
{
    return false;
}

bool VPM2AudioProcessor::isMidiEffect() const
{
    return false;
}

int VPM2AudioProcessor::getNumPrograms()
{
    return 1; // No programs
}

int VPM2AudioProcessor::getCurrentProgram()
{
    return 0;
}

void VPM2AudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused(index);
}

const juce::String VPM2AudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused(index);
    return {};
}

void VPM2AudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VPM2AudioProcessor();
}