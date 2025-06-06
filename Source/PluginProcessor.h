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

#include "LowPass.h"
#include "Routing.h" // Assuming Routing.h defines RoutingOutputs struct


//==============================================================================
class VPM2AudioProcessor  : public juce::AudioProcessor,
                              public juce::AudioProcessorValueTreeState::Listener // Make sure this is present
{
public:
    //==============================================================================
    VPM2AudioProcessor();
    ~VPM2AudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
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

    //================== Low Pass Filter Solo the modulator ====================================
    std::atomic<bool> bypassOversampling { false }; // atomic bool for the LPF solo in processblock

private:

    //================== Low Pass Solo Crossfade =======================================
    // Crossfade state for LPF solo
    float lpfSoloFade = 0.0f; // 0 = normal, 1 = fully LPF solo
    static constexpr float lpfSoloFadeTimeMs = 20.0f; // Fade time in ms (adjust as needed)

    // Pointers to your parameters
    juce::AudioParameterFloat* modDepthParam = nullptr;
    juce::AudioParameterFloat* maxDelayMsParam = nullptr;
    juce::AudioParameterChoice* algorithmParam = nullptr;
    juce::AudioParameterBool* invertParam;
    juce::AudioParameterFloat* lpCutoffParam = nullptr;
    juce::AudioParameterBool* interpolationTypeParam = nullptr;
    juce::AudioParameterBool* oversamplingParam = nullptr;
  
    // Your DSP components (now two mono Delay instances)
    InterpolatedDelay delayL, delayR; // One per carrier channel
    
    LowPass modulatorLowPassL;
    LowPass modulatorLowPassR;

    juce::SpinLock delayStateLock; // Thread Safety

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
    juce::SmoothedValue<float> smoothedCutoff;

    // Smoothing coefficients (set in prepareToPlay)
    float modDepthSmoothingCoeff = 0.0f;
    float cutoffSmoothingCoeff = 0.0f;

    // PDC RELATED
    int lastReportedLatency = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VPM2AudioProcessor)
};