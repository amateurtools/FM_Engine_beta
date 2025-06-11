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
        100.0f,
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
            30.0f, 20000.0f,
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
//====== from https://github.com/Mrugalla/Absorb/blob/master/Source/Processor.cpp

juce::AudioProcessor::BusesProperties VPM2AudioProcessor::makeBusesProperties()
{
    juce::AudioProcessor::BusesProperties bp;
    bp.addBus(true,  "Input",     juce::AudioChannelSet::stereo(), true);   // Main stereo input
    bp.addBus(false, "Output",    juce::AudioChannelSet::stereo(), true);   // Main stereo output
    bp.addBus(true,  "Sidechain", juce::AudioChannelSet::stereo(), true);  // Stereo sidechain, disabled by default
    return bp;
}


//==============================================================================
VPM2AudioProcessor::VPM2AudioProcessor()
    : AudioProcessor(makeBusesProperties()),
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
    apvts.addParameterListener("PDC", this);
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
    apvts.removeParameterListener("PDC", this);
}

void VPM2AudioProcessor::updateLatency()
{
    bool pdcEnabled = getPDCEnabled();
    float maxDelayMs = *apvts.getRawParameterValue("MAX_DELAY_MS");
    double sampleRate = getSampleRate();
    bool oversamplingEnabled = *apvts.getRawParameterValue("OVERSAMPLING"); // 0 = off, 1 = on

    // Set oversampling factor (adjust if you use a different factor)
    int oversampleFactor = oversamplingEnabled ? 2 : 1;

    if (pdcEnabled)
    {
        // Calculate latency in samples at the input rate
        int latencySamples = static_cast<int>((0.5 * maxDelayMs * 0.001 * sampleRate) / oversampleFactor);
        setLatencySamples(latencySamples);
    }
    else
    {
        setLatencySamples(0);
    }
}


void VPM2AudioProcessor::parameterChanged(const juce::String& parameterID, float /*newValue*/)
{
    // const juce::SpinLock::ScopedLockType lock (delayStateLock);

    if (parameterID == "MOD_DEPTH")
    {
        if (auto* p = apvts.getRawParameterValue("MOD_DEPTH"))
        {
            float currentModDepth = *p;
            // Use currentModDepth...
        }
    }
    else if (parameterID == "MAX_DELAY_MS")
    {
        float currentMaxDelayMs = *apvts.getRawParameterValue("MAX_DELAY_MS");
        delayL.setMaxDelayMs(currentMaxDelayMs);
        delayR.setMaxDelayMs(currentMaxDelayMs);
        // Always update latency to reflect the new state
        updateLatency();
    }
    else if (parameterID == "PDC")
    {
        // Only update latency when PDC changes
        updateLatency();
    }
    else if (parameterID == "ALGORITHM")
    {
        if (auto* p = apvts.getRawParameterValue("ALGORITHM"))
        {
            int currentAlgorithmIndex = static_cast<int>(*p);
            // Use currentAlgorithmIndex...
        }
    }
    else if (parameterID == "INVERT")
    {
        if (auto* p = apvts.getRawParameterValue("INVERT"))
        {
            bool isInverted = (*p > 0.5f);
            // Use isInverted...
        }
    }
    else if (parameterID == "LP_CUTOFF")
    {
        if (auto* p = apvts.getRawParameterValue("LP_CUTOFF"))
        {
            float currentLPCutoff = *p;
            // Use currentLPCutoff, e.g.:
            // modulatorLowPassL.setCutoff(currentLPCutoff);
            // modulatorLowPassR.setCutoff(currentLPCutoff);
        }
    }
}

// ================= OFFLINE RENDERING!!! ======================================

// void VPM2AudioProcessor::setNonRealtime(bool isNonRealtime) noexcept
// {
//     isOfflineMode = isNonRealtime;
    
//     DBG("=== RENDER MODE CHANGE ===");
//     DBG("isNonRealtime: " << (isNonRealtime ? "YES (OFFLINE RENDER)" : "NO (REALTIME)"));
    
//     if (isNonRealtime)
//     {
//         // During offline render, disable PDC latency compensation
//         // because the host handles timing differently
//         setLatencySamples(0);
        
//         // Reset delay states to ensure clean render
//         delayL.reset();
//         delayR.reset();
//         modulatorLowPassL.reset();
//         modulatorLowPassR.reset();
//     }
//     else
//     {
//         // Back to real-time: restore normal latency
//         updateLatency();
//     }
// }

//==============================================================================
void VPM2AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Pre-allocate buffers for max block size
    routedBuffer.setSize(4, samplesPerBlock);
    tempProcessingBuffer.setSize(3, samplesPerBlock);
    normalizedModL.resize(samplesPerBlock, 0.0f);
    normalizedModR.resize(samplesPerBlock, 0.0f);

    oversampler.initProcessing(samplesPerBlock);
    oversampler.reset();
    
    // Store the max samples per block for assertions and buffer sizing
    currentMaxBlockSize = samplesPerBlock; 
    // ADDED DBG LINE:
    DBG("prepareToPlay: samplesPerBlock received = " << samplesPerBlock << ", currentMaxBlockSize set to = " << currentMaxBlockSize);

    // Retrieve PDC enabled state from parameter

    float maxDelayMs = *apvts.getRawParameterValue("MAX_DELAY_MS");

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

    // Smoothing times in milliseconds (adjust to taste)
    float modDepthSmoothingTimeMs = 10.0f;
    float cutoffSmoothingTimeMs = 50.0f;

    modDepthSmoothingCoeff = std::exp(-1.0f / (0.001f * modDepthSmoothingTimeMs * sampleRate));

    // Initialize smoothed values to current parameter values
    smoothedModDepth = *apvts.getRawParameterValue("MOD_DEPTH");

    // smooth out the insane cutoff madness to prevent LPF explosions
    smoothedCutoff.reset(sampleRate, cutoffSmoothingTimeMs * 0.001f); 
    smoothedCutoff.setTargetValue (*apvts.getRawParameterValue ("LP_CUTOFF"));

    updateLatency();  // This will set latency based on current PDC and MAX_DELAY_MS
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

bool VPM2AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Require exactly 2 input buses (main + sidechain)
    if (layouts.inputBuses.size() != 2)
    {
        DBG("REJECT: Expected exactly 2 input buses (main + sidechain), got " << layouts.inputBuses.size());
        return false;
    }

    // Require exactly 1 output bus
    if (layouts.outputBuses.size() != 1)
    {
        DBG("REJECT: Expected exactly 1 output bus, got " << layouts.outputBuses.size());
        return false;
    }

    // Main input must be stereo
    auto mainInputChannels = layouts.getMainInputChannelSet();
    if (mainInputChannels != juce::AudioChannelSet::stereo())
    {
        DBG("REJECT: Main input must be stereo, got " << mainInputChannels.getDescription());
        return false;
    }

    // Main output must be stereo
    auto mainOutputChannels = layouts.getMainOutputChannelSet();
    if (mainOutputChannels != juce::AudioChannelSet::stereo())
    {
        DBG("REJECT: Main output must be stereo, got " << mainOutputChannels.getDescription());
        return false;
    }

    // Sidechain input (bus index 1) must be stereo
    auto sidechainChannels = layouts.getChannelSet(true, 1);
    if (sidechainChannels != juce::AudioChannelSet::stereo())
    {
        DBG("REJECT: Sidechain input must be stereo, got " << sidechainChannels.getDescription());
        return false;
    }

    DBG("ACCEPT: Stereo main in/out and stereo sidechain.");
    return true;
}


//==============================================================================
// 4. ENHANCED processBlock with better channel debugging

void VPM2AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // ascertain the name of the second bus aka sidechain. is it called sidechain or aux or what?
    if (getBusCount(true) > 1)
    {
        auto* bus = getBus(true, 1);
        juce::String busName = bus->getName();
        DBG("Second input bus name: " << busName);

        if (busName == "Sidechain")
        {
            // Treat as sidechain
        }
        else
        {
            // Could be a discrete/aux input, handle accordingly
        }
    }
    
    // === PDC STUFF ===
    bool pdcEnabled = false;
    if (auto* p = apvts.getRawParameterValue("PDC"))
        pdcEnabled = (*p > 0.5f);

    float newBaseDelay = pdcEnabled ? (0.5f * delayL.getMaxDelayMs()) : 0.0f;
    if (newBaseDelay != lastBaseDelay)
    {
        delayL.setBaseDelayMs(newBaseDelay);
        delayR.setBaseDelayMs(newBaseDelay);
        lastBaseDelay = newBaseDelay;
    }

    // ===================

    const int numSamples = buffer.getNumSamples();  // this gets how many samples per block
    const int numChannels = buffer.getNumChannels();

    // --- Buffer and channel count checks ---
    jassert(numSamples > 0);
    jassert(numChannels > 0);

    if (numSamples <= 0)
        return;

    // Ensure buffers are large enough (only resize if needed)
    if (numSamples > routedBuffer.getNumSamples())
        routedBuffer.setSize(4, numSamples);
    if (numSamples > tempProcessingBuffer.getNumSamples())
        tempProcessingBuffer.setSize(3, numSamples);
    if (numSamples > (int)normalizedModL.size()) {
        normalizedModL.resize(numSamples, 0.0f);
        normalizedModR.resize(numSamples, 0.0f);
    }
    
    routedBuffer.clear();
    tempProcessingBuffer.clear();
    juce::FloatVectorOperations::clear(normalizedModL.data(), numSamples);
    juce::FloatVectorOperations::clear(normalizedModR.data(), numSamples);
    
    auto mainInput = getBusBuffer(buffer, true, 0);
    auto mainOutput = getBusBuffer(buffer, false, 0);
    auto sidechainInput = getBusBuffer(buffer, true, 1);
    
    jassert(mainInput.getNumChannels() == 2);
    jassert(mainOutput.getNumChannels() == 2);
    jassert(sidechainInput.getNumChannels() == 2);

    const float* inL = mainInput.getReadPointer(0);
    const float* inR = (mainInput.getNumChannels() > 1) ? mainInput.getReadPointer(1) : nullptr;
    const float* scL = (sidechainInput.getNumChannels() > 0) ? sidechainInput.getReadPointer(0) : nullptr;
    const float* scR = (sidechainInput.getNumChannels() > 1) ? sidechainInput.getReadPointer(1) : nullptr;

    #if JUCE_DEBUG
    // Print a tidy debug block every 1000 processBlock calls
    static int blockDebugCounter = 0;
    static bool lastNonRealtime = false;

    if (++blockDebugCounter % 1000 == 0)
    {
        DBG("========== PROCESS BLOCK DEBUG ==========");
        DBG("Mode: " << (isNonRealtime() ? "OFFLINE" : "REALTIME"));
        DBG("Input channels: " << getTotalNumInputChannels() 
            << ", Output channels: " << getTotalNumOutputChannels());
        DBG("Input bus count: " << getBusCount(true));
        DBG("Buffer channels: " << buffer.getNumChannels() 
            << ", Samples: " << buffer.getNumSamples());

        // Sidechain info
        if (getBusCount(true) > 1)
        {
            auto sidechainBuffer = getBusBuffer(buffer, true, 1);
            DBG("Sidechain channels: " << sidechainBuffer.getNumChannels());

            if (sidechainBuffer.getNumChannels() > 0)
            {
                float scRMS = sidechainBuffer.getRMSLevel(0, 0, sidechainBuffer.getNumSamples());
                DBG("Sidechain RMS (ch0): " << scRMS);
            }
            else
            {
                DBG("Sidechain bus present but no channels enabled.");
            }
        }
        else
        {
            DBG("No sidechain bus present.");
        }
        DBG("==========================================");
    }

    // Report mode changes (realtime/offline) only when they occur
    bool currentNonRealtime = isNonRealtime();
    static bool lastReportedNonRealtime = false;
    if (currentNonRealtime != lastReportedNonRealtime)
    {
        DBG("=== PROCESSING MODE CHANGE ===");
        DBG("Now running in: " << (currentNonRealtime ? "OFFLINE RENDER" : "REALTIME"));
        lastReportedNonRealtime = currentNonRealtime;
    }
    #endif

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
        
        // Check for PDC and choose bipolar or unipolar signal representation
        if (pdcEnabled)
        {
            // Use bipolar modulator directly (no normalization)
            normalizedModL[i] = processedModL[i];
            normalizedModR[i] = processedModR[i];
            // Optionally, add assertions for bipolar range if you want:
            // jassert(normalizedModL[i] >= -1.0f && normalizedModL[i] <= 1.0f);
            // jassert(normalizedModR[i] >= -1.0f && normalizedModR[i] <= 1.0f);
        }
        else
        {
            // Original normalized mapping
            normalizedModL[i] = (processedModL[i] + 1.0f) * 0.5f;
            normalizedModR[i] = (processedModR[i] + 1.0f) * 0.5f;
            jassert(std::isfinite(normalizedModL[i]));
            jassert(std::isfinite(normalizedModR[i]));
            jassert(normalizedModL[i] >= 0.0f && normalizedModL[i] <= 1.0f);
            jassert(normalizedModR[i] >= 0.0f && normalizedModR[i] <= 1.0f);
        }
        
        // (Optional) Store in routed buffer if needed for oversampling
        jassert(routedBuffer.getNumChannels() > 2);
        jassert(routedBuffer.getNumChannels() > 3);
        routedBuffer.setSample(2, i, processedModL[i]);
        routedBuffer.setSample(3, i, processedModR[i]);
    }
    
    // Save smoothed state for next block
    smoothedModDepth = smoothedModDepthLocal;

    //=== PDC STUFF ===
    
    delayL.setPDCEnabled(pdcEnabled);
    delayR.setPDCEnabled(pdcEnabled);
    delayL.setBaseDelayMs(pdcEnabled ? 0.5f * delayL.getMaxDelayMs() : 0.0f);
    delayR.setBaseDelayMs(pdcEnabled ? 0.5f * delayR.getMaxDelayMs() : 0.0f);
    
    //=================

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
// Save plugin state (all parameters, including PDC)
void VPM2AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    DBG("[VPM2] getStateInformation called");
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
    {
        DBG("[VPM2] Saving state. PDC value: " << *apvts.getRawParameterValue("PDC"));
        copyXmlToBinary(*xml, destData);
    }
    else
    {
        DBG("[VPM2] Error: Could not create XML from state!");
    }
}

//==============================================================================
// Restore plugin state (all parameters, including PDC)
void VPM2AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    DBG("[VPM2] setStateInformation called");
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
    {
        DBG("[VPM2] Loaded state XML:\n" << xmlState->toString());

        if (xmlState->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
            shouldResetDelay = true;
            shouldResetLowPass = true;

            DBG("[VPM2] State loaded. PDC value after load: " << *apvts.getRawParameterValue("PDC"));
        }
        else
        {
            DBG("[VPM2] Error: XML tag name does not match state type!");
        }
    }
    else
    {
        DBG("[VPM2] Error: Could not parse XML from binary data!");
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