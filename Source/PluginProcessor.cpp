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
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 2.0f), // skew = 0.5f
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
        juce::ParameterID{"LIMITER", 1}, "Limiter", false
    ));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"SWAP", 1}, "Swap", false
    ));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"OVERSAMPLING", 1}, "Oversampling", false
    ));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"PREDELAY", 1}, "PREDELAY", false
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

    // i need this mapping but it's been difficult to get it working in this part. trying in the slider itself.
    // params.push_back(std::make_unique<juce::AudioParameterFloat>(
    //     juce::ParameterID{"LP_CUTOFF", 1}, "Lowpass Cutoff",
    //     juce::NormalisableRange<float>(
    //         20.0f, 20000.0f,
    //         // Normalised [0,1] -> Hz
    //         [](float start, float end, float norm) {
    //             juce::ignoreUnused(start, end);
    //             if (norm < 0.5f)
    //                 // Linear from 20 to 100 Hz
    //                 return 20.0f + (60.0f - 20.0f) * (norm / 0.5f);
    //             else
    //                 // Log from 100 Hz to 20,000 Hz
    //                 return 60.0f * std::pow(20000.0f / 60.0f, (norm - 0.5f) / 0.5f);
    //         },
    //         // Hz -> Normalised [0,1]
    //         [](float start, float end, float value) {
    //             juce::ignoreUnused(start, end);
    //             if (value < 60.0f)
    //                 // Linear region
    //                 return 0.5f * (value - 20.0f) / (60.0f - 20.0f);
    //             else
    //                 // Log region
    //                 return 0.5f + 0.5f * std::log(value / 60.0f) / std::log(20000.0f / 60.0f);
    //         }
    //     ),
    //     20000.0f // default value
    // ));

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

    limiterParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("LIMITER"));

    swapParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("SWAP"));
    oversamplingParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("OVERSAMPLING"));
    predelayParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("PREDELAY"));

    lpCutoffParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("LP_CUTOFF"));

    jassert(modDepthParam);
    jassert(maxDelayMsParam);
    jassert(algorithmParam);

    jassert(limiterParam);

    jassert(swapParam);
    jassert(oversamplingParam);
    jassert(predelayParam);

    jassert(lpCutoffParam);

    apvts.addParameterListener("MOD_DEPTH", this);
    apvts.addParameterListener("MAX_DELAY_MS", this);
    apvts.addParameterListener("ALGORITHM", this);

    apvts.addParameterListener("LIMITER", this);

    apvts.addParameterListener("SWAP", this);
    apvts.addParameterListener("OVERSAMPLING", this);
    apvts.addParameterListener("PREDELAY", this);

    apvts.addParameterListener("LP_CUTOFF", this);
}

FmEngineAudioProcessor::~FmEngineAudioProcessor()
{
    apvts.removeParameterListener("MOD_DEPTH", this);
    apvts.removeParameterListener("MAX_DELAY_MS", this);
    apvts.removeParameterListener("ALGORITHM", this);

    apvts.removeParameterListener("LIMITER", this);

    apvts.removeParameterListener("SWAP", this);
    apvts.removeParameterListener("OVERSAMPLING", this);
    apvts.removeParameterListener("PREDELAY", this);

    apvts.removeParameterListener("LP_CUTOFF", this);
}

void FmEngineAudioProcessor::updateLatency()
{
    bool predelayEnabled = getPredelayEnabled();
    float maxDelayMs = getMaxDelayMsFromChoice();
    double sampleRate = getSampleRate();

    if (sampleRate <= 0.0)
        return; // Defensive: avoid division by zero or negative rates

    if (predelayEnabled)
    {
        // Calculate latency in samples, using 0.5 factor if that's your design
        int latencySamples = static_cast<int>(maxDelayMs * 0.001 * sampleRate * 0.5);
        latencySamples = std::max(0, latencySamples); // Ensure non-negative

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
    else if (parameterID == "SWAP")
    {
        if (auto* p = apvts.getRawParameterValue("SWAP"))
        {
            bool isSwaped = (*p > 0.5f);
        }
    }
    else if (parameterID == "LIMITER")
    {
        if (auto* p = apvts.getRawParameterValue("LIMITER"))
        {
            bool currentLimiter = (*p > 0.5f);
        }
    }
    else if (parameterID == "PREDELAY")
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
    float cutoffSmoothingTimeMs = 15.0f; // it sucks for audio rate modulation of this LPF but that's not supposed to be a feature

    modDepthSmoothingCoeff = std::exp(-1.0f / (0.001f * modDepthSmoothingTimeMs * sampleRate));

    // Initialize smoothed values to current parameter values
    // I've tried the builtin juce method to smooth the param but the one pole way might work better.
    smoothedModDepth = *apvts.getRawParameterValue("MOD_DEPTH");

    // smooth out the LPF cutoff value for safety
    smoothedCutoff.reset(sampleRate, cutoffSmoothingTimeMs * 0.001f); 
    smoothedCutoff.setTargetValue (*apvts.getRawParameterValue ("LP_CUTOFF"));

    // coeffs for HPF
    highPassL.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 10.0f, 0.707f);
    highPassR.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 10.0f, 0.707f);

    // limiterModL.prepare(getSampleRate(), getBlockSize());
    // limiterModL.setCeiling(-0.1f); // example: -0.1 dB ceiling
    // limiterModR.prepare(getSampleRate(), getBlockSize());
    // limiterModR.setCeiling(-0.1f); // example: -0.1 dB ceiling
    limiterOutL.prepare(getSampleRate(), getBlockSize());
    limiterOutL.setCeiling(-0.1f); // example: -0.1 dB ceiling
    limiterOutR.prepare(getSampleRate(), getBlockSize());
    limiterOutR.setCeiling(-0.1f); // example: -0.1 dB ceiling

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

    // === PREDELAY STUFF ===
    bool predelayEnabled = apvts.getRawParameterValue("PREDELAY")->load() > 0.5f;

    // if predelay is enabled, set the basedelay, otherwise basedelay is always set to 0.0f
    float newBaseDelay = predelayEnabled ? (0.5f * delayL.getMaxDelayMs()) : 0.0f;
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

    bool swap = (*apvts.getRawParameterValue("SWAP") > 0.5f);

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

        RoutingOutputs rout = routeSample(carrierL, carrierR, modL, modR, algorithm, swap);
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

    // stuff for deciding to clip the output and clip type
    auto* limitRaw = apvts.getRawParameterValue("LIMITER");
    bool currentLimiter = (limitRaw && *limitRaw > 0.5f);

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
    
        // if you want to limit the modulator

        // try lookahead limiter instead of sine clipper ============================= LIMIT MOD
        // problem: creates difficult to resolve latency between mod and carrier.
        // if (currentLimiter) {
        // filteredL = limiterModL.processSample(filteredL);
        // filteredR = limiterModR.processSample(filteredR);
        // }     
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

    // auto clipper = [](float x, float threshold) -> float {
    //         if (threshold == 0.0f) return 0.0f; // Avoid division by zero
    //         // x = std::clamp(x, -threshold, threshold); // Normalize input
    //         x = std::sin((x / threshold) * (M_PI / 2)) * threshold;
    //         return x;
    // };

    auto clipper = [](float x) -> float {
            x = std::sin((x) * (M_PI / 2)); 
            x = x * 0.6310f; // 0.6310f corresponds to a gain reduction of -4db that this clipper seems to add
            return x;
    };

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

            // this appears to be interpolation for the oversampling process
            float modLraw = normalizedModL[idx0] + frac * (normalizedModL[idx1] - normalizedModL[idx0]);
            float modRraw = normalizedModR[idx0] + frac * (normalizedModR[idx1] - normalizedModR[idx0]);

            if (currentLimiter) {
                modLraw = clipper(modLraw);  // tried limiting. trying sine clip again.
                modRraw = clipper(modRraw);  // sine clip adds ringing. limiter creates latency issue.
            }

            // float modL = juce::jlimit(-1.0f, 1.0f, modLraw); // hard clipping for modulator in safety
            // float modR = juce::jlimit(-1.0f, 1.0f, modRraw); // the delay itself is limited but might as well

            float outL = delayL.process(osCarrierL[i], modLraw);
            float outR = delayR.process(osCarrierR[i], modRraw);

            // if (currentLimiter) {
            //     outL = clipper(outL);  // let's just use one clipper for final output
            //     outR = clipper(outR);  // they add harmonics but may be a preference
            // }

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

            // float modL = juce::jlimit(-1.0f, 1.0f, modLraw); // hard clipping for modulator in safety
            // float modR = juce::jlimit(-1.0f, 1.0f, modRraw); // the delay itself is limited but might as well

            if (currentLimiter) {
                modLraw = clipper(modLraw);  // let's just use one clipper for final output
                modRraw = clipper(modRraw);  // they add harmonics but may be a preference
            }

            float outL = delayL.process(carrierL[i], modLraw);
            float outR = delayR.process(carrierR[i], modRraw);

            // if (currentLimiter) {
            //     outL = clipper(outL);  // let's just use one clipper for final output
            //     outR = clipper(outR);  // they add harmonics but may be a preference
            // }

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

        jassert(std::isfinite(normalL));
        jassert(std::isfinite(normalR));

        // LPF solo output (the filtered modulator) - NOW SAFE!
        float lpfL = processedModL[i];
        float lpfR = processedModR[i];

        jassert(std::isfinite(lpfL));
        jassert(std::isfinite(lpfR));

        // Crossfade
        float outL = (1.0f - fadeMix) * normalL + fadeMix * lpfL;
        float outR = (1.0f - fadeMix) * normalR + fadeMix * lpfR;

        // Safe output - ensure we have enough channels
        // High-pass filter after delay (and after any other processing)
        float hpL = highPassL.processSample(outL);
        float hpR = highPassR.processSample(outR);

        // try lookahead limiter instead of sine clipper for final output
        if (currentLimiter) {
        hpL = limiterOutL.processSample(hpL);
        hpR = limiterOutR.processSample(hpR);
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
// Save plugin state (all parameters, including PREDELAY)
void FmEngineAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    DBG("[FmEngine] getStateInformation called");
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
    {
        DBG("[FmEngine] Saving state. PREDELAY value: " << *apvts.getRawParameterValue("PREDELAY"));
        copyXmlToBinary(*xml, destData);
    }
    else
    {
        DBG("[FmEngine] Error: Could not create XML from state!");
    }
}

//==============================================================================
// Restore plugin state (all parameters, including PREDELAY)
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

            DBG("[FmEngine] State loaded. PREDELAY value after load: " << *apvts.getRawParameterValue("PREDELAY"));
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