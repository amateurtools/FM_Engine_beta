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

juce::AudioProcessorValueTreeState::ParameterLayout FmEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"MOD_DEPTH", 1}, "Modulation Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 0.5f), // skew = 0.5f
        0.0f
    ));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"MAX_DELAY_MS", 1}, "Range (Max Delay)",
        juce::StringArray{ "10 ms", "100 ms", "500 ms" },
        0 // default index (10 ms)
    ));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"ALGORITHM", 1}, "Algorithm",
        juce::StringArray({ "Algo 1 (L=Car, R=Mod)", "Algo 2 (Mono Sum Sidechain)", "Algo 3 (St. Sum Sidechain)" }),
        0
    ));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"OUTPUT_CLIP", 1}, "Clip Output", false
    ));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"INVERT", 1}, "Invert", false
    ));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"OVERSAMPLING", 1}, "Oversampling", false
    ));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"PDC", 1}, "PDC", false
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

    return { params.begin(), params.end() };
}

//====== from https://github.com/Mrugalla/Absorb/blob/master/Source/Processor.cpp

// this was working, but is commented out to try discrete channels as a possible fix for rendering failures with SC
// juce::AudioProcessor::BusesProperties FmEngineAudioProcessor::makeBusesProperties()
// {
//     juce::AudioProcessor::BusesProperties bp;
//     bp.addBus(true,  "Input",     juce::AudioChannelSet::stereo(), true);   // Main stereo input
//     bp.addBus(false, "Output",    juce::AudioChannelSet::stereo(), true);   // Main stereo output
//     bp.addBus(true,  "Sidechain", juce::AudioChannelSet::stereo(), true);  // Stereo sidechain, disabled by default
//     return bp;
// }

juce::AudioProcessor::BusesProperties FmEngineAudioProcessor::makeBusesProperties()
{
    juce::AudioProcessor::BusesProperties bp;

    // --- Stereo main in/out + stereo sidechain (your original setup) ---
    bp.addBus(true,  "Input",     juce::AudioChannelSet::stereo(), true);   // Main stereo input
    bp.addBus(false, "Output",    juce::AudioChannelSet::stereo(), true);   // Main stereo output
    bp.addBus(true,  "Sidechain", juce::AudioChannelSet::stereo(), true);   // Stereo sidechain

    // --- 4 discrete in / 2 discrete out (alternative configuration) ---
    bp.addBus(true,  "DiscreteIn4",  juce::AudioChannelSet::discreteChannels(4), false); // 4 discrete input channels, disabled by default
    bp.addBus(false, "DiscreteOut2", juce::AudioChannelSet::discreteChannels(2), false); // 2 discrete output channels, disabled by default

    return bp;
}

//==============================================================================
FmEngineAudioProcessor::FmEngineAudioProcessor()
    : AudioProcessor(makeBusesProperties()),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout()),
      smoothedModDepth(0.0f),
      lastBaseDelay(0.0f),
      lpfSoloFade(0.0f),
      bypassOversampling(false)  // Or your default value
{
    // Attach listeners to parameters
    modDepthParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("MOD_DEPTH"));
    maxDelayMsParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("MAX_DELAY_MS"));
    algorithmParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("ALGORITHM"));

    outputClipParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("OUTPUT_CLIP"));

    invertParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("INVERT"));
    oversamplingParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("OVERSAMPLING"));
    pdcParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("PDC"));

    lpCutoffParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("LP_CUTOFF"));

    jassert(modDepthParam);
    jassert(maxDelayMsParam);
    jassert(algorithmParam);

    jassert(outputClipParam);

    jassert(invertParam);
    jassert(oversamplingParam);
    jassert(pdcParam);

    jassert(lpCutoffParam);

    apvts.addParameterListener("MOD_DEPTH", this);
    apvts.addParameterListener("MAX_DELAY_MS", this);
    apvts.addParameterListener("ALGORITHM", this);

    apvts.addParameterListener("OUTPUT_CLIP", this);

    apvts.addParameterListener("INVERT", this);
    apvts.addParameterListener("OVERSAMPLING", this);
    apvts.addParameterListener("PDC", this);

    apvts.addParameterListener("LP_CUTOFF", this);
}

FmEngineAudioProcessor::~FmEngineAudioProcessor()
{
    apvts.removeParameterListener("MOD_DEPTH", this);
    apvts.removeParameterListener("MAX_DELAY_MS", this);
    apvts.removeParameterListener("ALGORITHM", this);

    apvts.removeParameterListener("OUTPUT_CLIP", this);

    apvts.removeParameterListener("INVERT", this);
    apvts.removeParameterListener("OVERSAMPLING", this);
    apvts.removeParameterListener("PDC", this);

    apvts.removeParameterListener("LP_CUTOFF", this);
}

void FmEngineAudioProcessor::updateLatency()  // this function includes offset adjuster
{
    bool pdcEnabled = getPDCEnabled();
    float maxDelayMs = getMaxDelayMsFromChoice();
    double sampleRate = getSampleRate();

    if (getSampleRate() <= 0) return;

    if (pdcEnabled)
    {
        int latencySamples = static_cast<int>(maxDelayMs * 0.001 * sampleRate * 0.5); 
        latencySamples = std::max(0, latencySamples); // assure the value is positive
        setLatencySamples(latencySamples);
    }
    else
    {
        setLatencySamples(0);
    }
}

void FmEngineAudioProcessor::parameterChanged(const juce::String& parameterID, float /*newValue*/)
{
    if (parameterID == "MOD_DEPTH")
    {
        if (auto* p = apvts.getRawParameterValue("MOD_DEPTH"))
        {
            float currentModDepth = *p;
            // currentModDepth probably relates to parameter smoothing
        }
    }
    else if (parameterID == "MAX_DELAY_MS")
    {
        float currentMaxDelayMs = getMaxDelayMsFromChoice();
        delayL.setMaxDelayMs(currentMaxDelayMs);
        delayR.setMaxDelayMs(currentMaxDelayMs);
        updateLatency();
    }

    else if (parameterID == "ALGORITHM")
    {
        if (auto* p = apvts.getRawParameterValue("ALGORITHM"))
        {
            int currentAlgorithmIndex = static_cast<int>(*p);
        }
    }
    else if (parameterID == "INVERT")
    {
        if (auto* p = apvts.getRawParameterValue("INVERT"))
        {
            bool isInverted = (*p > 0.5f);
        }
    }
    else if (parameterID == "OUTPUT_CLIP")
    {
        if (auto* p = apvts.getRawParameterValue("OUTPUT_CLIP"))
        {
            bool currentOutputClip = (*p > 0.5f);
        }
    }
    else if (parameterID == "PDC")
    {
        updateLatency();
    }
    else if (parameterID == "OVERSAMPLING") {
        shouldResetDelay = true; // Force delay re-prepare
    }    
    else if (parameterID == "LP_CUTOFF")
    {
        if (auto* p = apvts.getRawParameterValue("LP_CUTOFF"))
        {
            float currentLPCutoff = *p;
            // Usage of currentLPCutoff, e.g.:
            // modulatorLowPassL.setCutoff(currentLPCutoff);
            // modulatorLowPassR.setCutoff(currentLPCutoff);
        }
    }
}

//==============================================================================
void FmEngineAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
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

    // Prepare DSP components - Use BASE sample rate for delay preparation
    modulatorLowPassL.prepare(sampleRate, samplesPerBlock);
    modulatorLowPassR.prepare(sampleRate, samplesPerBlock);

    float safeMaxDelay = getMaxDelayMsFromChoice();
    
    // FIXED: Use oversampled sample rate for delay preparation when oversampling is enabled
    double finalSampleRate = sampleRate;
    if (*apvts.getRawParameterValue("OVERSAMPLING") > 0.5f)
    {
        finalSampleRate = sampleRate * oversampler.getOversamplingFactor();
    }
    
    delayL.prepare(finalSampleRate, safeMaxDelay);
    delayR.prepare(finalSampleRate, safeMaxDelay);

    // Initialize the silent sidechain buffer (conceptually a member variable)
    // This assumes silentSidechainBuffer is a member of FmEngineAudioProcessor
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
    float cutoffSmoothingTimeMs = 30.0f;

    modDepthSmoothingCoeff = std::exp(-1.0f / (0.001f * modDepthSmoothingTimeMs * sampleRate));

    // Initialize smoothed values to current parameter values
    // I've tried the builtin juce method to smooth the param but the one pole way might work better.
    smoothedModDepth = *apvts.getRawParameterValue("MOD_DEPTH");

    // smooth out the LPF cutoff value for safety
    smoothedCutoff.reset(sampleRate, cutoffSmoothingTimeMs * 0.001f); 
    smoothedCutoff.setTargetValue (*apvts.getRawParameterValue ("LP_CUTOFF"));

    // coeffs for HPF
    highPassL.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 20.0f, 0.707f);
    highPassR.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 20.0f, 0.707f);

    updateLatency(); 
}

void FmEngineAudioProcessor::releaseResources()
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

// trying to add discrete channels as a potential remedy to the sidechain not rendering problem
bool FmEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // --- Original stereo main + stereo sidechain case ---
    if (layouts.inputBuses.size() == 2 && layouts.outputBuses.size() == 1)
    {
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

    // --- New case: 4 discrete in, 2 discrete out (single input and output bus) ---
    if (layouts.inputBuses.size() == 1 && layouts.outputBuses.size() == 1)
    {
        auto inSet  = layouts.getMainInputChannelSet();
        auto outSet = layouts.getMainOutputChannelSet();

        if (inSet == juce::AudioChannelSet::discreteChannels(4) &&
            outSet == juce::AudioChannelSet::discreteChannels(2))
        {
            DBG("ACCEPT: 4 discrete input channels, 2 discrete output channels.");
            return true;
        }
    }

    // If no supported layout matched, reject it
    DBG("REJECT: Unsupported bus layout.");
    return false;
}

//==============================================================================
// 4. ENHANCED processBlock with better channel debugging

void FmEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // === PDC STUFF ===
    bool pdcEnabled = apvts.getRawParameterValue("PDC")->load() > 0.5f;

    // if pdc is enabled, set the basedelay, otherwise basedelay is always set to 0.0f
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

    // Report mode changes (realtime/offline) only when they occur
    bool currentNonRealtime = isNonRealtime();

    static bool lastReportedNonRealtime = false;
    if (currentNonRealtime != lastReportedNonRealtime)
    {
        DBG("=== PROCESSING MODE CHANGE ===");
        DBG("Now running in: " << (currentNonRealtime ? "OFFLINE RENDER" : "REALTIME"));
        lastReportedNonRealtime = currentNonRealtime;
    }
    // #endif

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

    bool oversamplingEnabled = (*apvts.getRawParameterValue("OVERSAMPLING") > 0.5f);

    // for fading in and out of low pass solo mode
    const float fadeTimeSamples = lpfSoloFadeTimeMs * 0.001f * getSampleRate();
    const float fadeStep = (fadeTimeSamples > 0.0f) ? (1.0f / fadeTimeSamples) : 1.0f;

    // one liner to wash audio floats so they don't go to nans
    auto sanitize = [](float x) -> float { return std::isfinite(x) ? x : 0.0f; };

    // --- FIXED: Safe routing with null pointer protection ---
    for (int i = 0; i < numSamples; ++i)
    {
        jassert(i < numSamples);
        float carrierL = inL ? sanitize(inL[i]) : 0.0f;
        float carrierR = inR ? sanitize(inR[i]): carrierL;
        float modL = scL ? sanitize(scL[i]) : 0.0f;
        float modR = scR ? sanitize(scR[i]) : modL;

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

    // --- PRE-PROCESS MODULATOR: Smoothing, Clipping, Lowpass ---

    auto* smoothedModDepthBuffer = tempProcessingBuffer.getWritePointer(0);

    auto* processedModL = tempProcessingBuffer.getWritePointer(1);
    auto* processedModR = tempProcessingBuffer.getWritePointer(2);

    jassert(smoothedModDepthBuffer != nullptr);
    jassert(processedModL != nullptr);
    jassert(processedModR != nullptr);

    float smoothedModDepthLocal = smoothedModDepth;

    smoothedCutoff.setTargetValue(*apvts.getRawParameterValue("LP_CUTOFF"));

    // used for clipping the mod input at all times, and optionally at final output
    // auto audioClip = [](float x, float gain) -> float {    //tanh more elaborate version
    //     float c = gain;
    //     return std::tanh(c * x) / std::tanh(c);
    // };

        auto softClip = [](float x) -> float {   //tanh simple
        return std::tanh(x);
    };

    // auto softClip = [](float x, float threshold) -> float {   // sine clip is supposed to be cleaner
    //     if (x > threshold) return threshold;
    //     if (x < -threshold) return -threshold;
    //     return std::sin((x / threshold) * (M_PI / 2)) * threshold;
    // };

    // auto hardClip = [](float x, float threshold) -> float {  // actually hard clip but mis-named for 
    //     if (x > threshold) return threshold;
    //     if (x < -threshold) return -threshold;
    //     return x;
    // };

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
    
        // Apply soft clipping of the modulator signal
        float clippedModL = softClip(filteredL); // or your soft clipper of choice
        float clippedModR = softClip(filteredR);
        jassert(std::isfinite(clippedModL));
        jassert(std::isfinite(clippedModR));
    
        // === APPLY MOD DEPTH while signal is still bipolar ===

        float depthModL = filteredL * smoothedModDepthLocal;
        float depthModR = filteredR * smoothedModDepthLocal;
        jassert(std::isfinite(depthModL));
        jassert(std::isfinite(depthModR));
    
        // Sanitize after depth (optional, for safety)
        processedModL[i] = sanitize(depthModL);
        processedModR[i] = sanitize(depthModR);
        jassert(std::isfinite(processedModL[i]));
        jassert(std::isfinite(processedModR[i]));
        
        // normalize modulator from bipolar to unipolar
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
    // this is part of the "better" working one pole param smoothing method
    smoothedModDepth = smoothedModDepthLocal;
    //=================

    // stuff for deciding to clip the output and clip type
    // this is here because in oversampled mode the clipper gets oversampled.
    auto* outputClipRaw = apvts.getRawParameterValue("OUTPUT_CLIP");
    bool currentOutputClip = (outputClipRaw && *outputClipRaw > 0.5f);

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
            float pos = static_cast<float>(i) / osFactor;
            int idx0 = static_cast<int>(pos);
            int idx1 = juce::jmin(idx0 + 1, numSamples - 1);
            float frac = pos - idx0;

            float modLraw = normalizedModL[idx0] + frac * (normalizedModL[idx1] - normalizedModL[idx0]);
            float modRraw = normalizedModR[idx0] + frac * (normalizedModR[idx1] - normalizedModR[idx0]);

            float modL = juce::jlimit(0.0f, 1.0f, modLraw);
            float modR = juce::jlimit(0.0f, 1.0f, modRraw);

            float outL = delayL.process(osCarrierL[i], modL);
            float outR = delayR.process(osCarrierR[i], modR);

            jassert(std::isfinite(outL));
            jassert(std::isfinite(outR));

            if (currentOutputClip) {
                outL = softClip(outL * 0.9441f);  // reduce by .5db to help prevent ISP
                outR = softClip(outR * 0.9441f);
                outL = juce::jlimit(-1.0f, 1.0f, outL);
                outR = juce::jlimit(-1.0f, 1.0f, outR); 
            }

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

            float modLraw = normalizedModL[i];
            float modRraw = normalizedModR[i];

            float modL = juce::jlimit(0.0f, 1.0f, modLraw);
            float modR = juce::jlimit(0.0f, 1.0f, modRraw);

            float outL = delayL.process(carrierL[i], modL);
            float outR = delayR.process(carrierR[i], modR);

            jassert(std::isfinite(outL));
            jassert(std::isfinite(outR));

            if (currentOutputClip) {
                outL = softClip(outL * 0.9441f); // reduce by .5db to help prevent ISP
                outR = softClip(outR * 0.9441f);
                outL = juce::jlimit(-1.0f, 1.0f, outL);
                outR = juce::jlimit(-1.0f, 1.0f, outR); 
            }

            jassert(std::isfinite(outL));
            jassert(std::isfinite(outR));

            carrierL[i] = std::isfinite(outL) ? outL : 0.0f;
            carrierR[i] = std::isfinite(outR) ? outR : 0.0f;


        }
    }

    
    // stuff that has to do with smoothly crossfading the LPF solo function
    // in oversampled mode the clipper is in there so i wonder if there is a risk from spikes here.
    const float targetFade = bypassOversampling ? 1.0f : 0.0f;
    if (lpfSoloFade < targetFade)
        lpfSoloFade = std::min(targetFade, lpfSoloFade + fadeStep * numSamples);
    else if (lpfSoloFade > targetFade)
        lpfSoloFade = std::max(targetFade, lpfSoloFade - fadeStep * numSamples);

    lpfSoloFade = juce::jlimit(0.0f, 1.0f, lpfSoloFade);
    
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

        // Safe output - ensure we have enough channels
        // High-pass filter after delay (and after any other processing)
        float hpL = highPassL.processSample(outL);
        float hpR = highPassR.processSample(outR);

        if (currentOutputClip) {
            hpL = softClip(hpL * 0.9441f);  // reduce by .5db to help prevent ISP again, totalling -1db
            hpR = softClip(hpR * 0.9441f);
            hpL = juce::jlimit(-1.0f, 1.0f, hpL);
            hpR = juce::jlimit(-1.0f, 1.0f, hpR); 
        }

        if (buffer.getNumChannels() > 0)
            buffer.setSample(0, i, hpL);
        if (buffer.getNumChannels() > 1)
            buffer.setSample(1, i, hpR);
    }
}

//==============================================================================
bool FmEngineAudioProcessor::hasEditor() const
{
    return true; // Always return true if your plugin has a GUI
}

juce::AudioProcessorEditor* FmEngineAudioProcessor::createEditor()
{
    return new FmEngineAudioProcessorEditor(*this); // Replace with your editor class name
    //return new juce::GenericAudioProcessorEditor(*this); // Use JUCE's safe, generic editor
}

//==============================================================================
// Save plugin state (all parameters, including PDC)
void FmEngineAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    DBG("[FmEngine] getStateInformation called");
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
    {
        DBG("[FmEngine] Saving state. PDC value: " << *apvts.getRawParameterValue("PDC"));
        copyXmlToBinary(*xml, destData);
    }
    else
    {
        DBG("[FmEngine] Error: Could not create XML from state!");
    }
}

//==============================================================================
// Restore plugin state (all parameters, including PDC)
void FmEngineAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    DBG("[FmEngine] setStateInformation called");
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
    {
        DBG("[FmEngine] Loaded state XML:\n" << xmlState->toString());

        if (xmlState->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
            shouldResetDelay = true;
            shouldResetLowPass = true;

            DBG("[FmEngine] State loaded. PDC value after load: " << *apvts.getRawParameterValue("PDC"));
        }
        else
        {
            DBG("[FmEngine] Error: XML tag name does not match state type!");
        }
    }
    else
    {
        DBG("[FmEngine] Error: Could not parse XML from binary data!");
    }
}


const juce::String FmEngineAudioProcessor::getName() const
{
    return JucePlugin_Name;
    // DBG("JucePlugin_Name gotten");
}

double FmEngineAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

bool FmEngineAudioProcessor::acceptsMidi() const
{
    return false;
}

bool FmEngineAudioProcessor::producesMidi() const
{
    return false;
}

bool FmEngineAudioProcessor::isMidiEffect() const
{
    return false;
}

int FmEngineAudioProcessor::getNumPrograms()
{
    return 1; // No programs
}

int FmEngineAudioProcessor::getCurrentProgram()
{
    return 0;
}

void FmEngineAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused(index);
}

const juce::String FmEngineAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused(index);
    return {};
}

void FmEngineAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FmEngineAudioProcessor();
}