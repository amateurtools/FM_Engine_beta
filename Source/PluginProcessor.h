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

#include <vector>
#include <memory>

// Include your DSP component headers
#include "InterpolatedDelay.h"

#include "BrickWallLimiter.h"

#include "LowPass.h"
#include "Routing.h" // Assuming Routing.h defines RoutingOutputs struct



//==============================================================================
class FmEngineAudioProcessor  : public juce::AudioProcessor,
                              public juce::AudioProcessorValueTreeState::Listener // Make sure this is present
{
public:
    //==============================================================================
    FmEngineAudioProcessor();
    ~FmEngineAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
   bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    bool hasEditor() const override;
    juce::AudioProcessorEditor* createEditor() override;


    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // AudioProcessorValueTreeState for managing parameters
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Listener callback
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // ================= Max Delay Ms from Choice converter ==========================
    float getMaxDelayMsFromChoice() const
    {
        static constexpr float delayChoices[] = { 10.0f, 100.0f, 500.0f };
        if (auto* maxDelayMsParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("MAX_DELAY_MS")))
        {
            int idx = maxDelayMsParam->getIndex();
            return delayChoices[juce::jlimit(0, 2, idx)];
        }
        // Fallback (should not happen)
        return 10.0f;
    }

    //================== Low Pass Filter Solo the modulator ====================================
    std::atomic<bool> bypassOversampling { false }; // atomic bool for the LPF solo in processblock

    bool getPredelayEnabled() const
    {
        if (auto* p = apvts.getRawParameterValue("PREDELAY"))
            return (*p > 0.5f);
        return false;
    }

    //================== are we in realtime or offline mode (rendering?) ==============

    // void setNonRealtime(bool isNonRealtime) noexcept override;

    BrickWallLimiter limiterModL, limiterModR, limiterOutL, limiterOutR;

private:

    static juce::AudioProcessor::BusesProperties makeBusesProperties();

    //================== important buffers for processlbock ============================
    juce::AudioBuffer<float> routedBuffer;
    juce::AudioBuffer<float> tempProcessingBuffer;
    std::vector<float> normalizedModL;
    std::vector<float> normalizedModR;

    //================== Low Pass Solo Crossfade =======================================
    // Crossfade state for LPF solo
    float lpfSoloFade = 0.0f; // 0 = normal, 1 = fully LPF solo
    static constexpr float lpfSoloFadeTimeMs = 20.0f; // Fade time in ms (adjust as needed)

    //================== HPF to account for the insane low freq introduced =============
    juce::dsp::IIR::Filter<float> highPassL, highPassR;

    // Pointers to your parameters
    juce::AudioParameterFloat* modDepthParam = nullptr;
    juce::AudioParameterChoice* maxDelayMsParam = nullptr;
    juce::AudioParameterChoice* algorithmParam = nullptr;
    juce::AudioParameterBool* swapParam = nullptr;
    juce::AudioParameterBool* limiterParam = nullptr;
    juce::AudioParameterBool* oversamplingParam = nullptr;
    juce::AudioParameterBool* predelayParam = nullptr;
    juce::AudioParameterFloat* lpCutoffParam = nullptr;
  
    // Your DSP components (now two mono Delay instances)
    InterpolatedDelay delayL, delayR; // One per carrier channel

    float newBaseDelay = 0.0f;  // relevant to PREDELAY coordinating
    float lastBaseDelay = 0.0f; // relevant to PREDELAY coordinating
    
    LowPass modulatorLowPassL;
    LowPass modulatorLowPassR;



    // juce::SpinLock delayStateLock; // Thread Safety

    // Flags to indicate when DSP objects need to be reset/re-prepared
    bool shouldResetDelay = true; // Ensure these are still present as in your original file
    bool shouldResetLowPass = true;
    int currentMaxBlockSize = 0;
    juce::AudioBuffer<float> silentSidechainBuffer; // Used for silent sidechain input if bus is inactive

    // Oversampler for 4 channels (quad)
    juce::dsp::Oversampling<float> oversampler {
        4, // number of channels
        1, // number of oversampling stages (2x)
        juce::dsp::Oversampling<float>::FilterType::filterHalfBandFIREquiripple,
        true // process tail (anti-aliasing)
    };

    // for smoothing the modulation amount dial
    float smoothedModDepth = 0.0f;
    float modDepthSmoothingCoeff = 0.0f;

    // smoothing the LPF slider
    juce::SmoothedValue<float> smoothedCutoff;
    float cutoffSmoothingCoeff = 0.0f;

    // PREDELAY RELATED

    bool isOfflineMode = false;

    void updateLatency();

    int lastReportedLatency = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FmEngineAudioProcessor)
};