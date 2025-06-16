#pragma once

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

#include "Dial.h"
#include "SlidingSwitch.h"
#include "SidewaysToggleSwitch.h"
#include "CustomCutoffSlider.h"
#include "PluginProcessor.h" // for the getMaxDelayMsFromChoice() function

class VPM2AudioProcessor; // Forward declaration

class VpmGUIAudioProcessorEditor  : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    VpmGUIAudioProcessorEditor (VPM2AudioProcessor&);
    ~VpmGUIAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void updateTooltips(); 

private:
    // all this because i want a stepped dial that uses arbitrary values in a vector
    static constexpr float minAngle = juce::degreesToRadians(225.0f);
    static constexpr float midAngle = juce::degreesToRadians(270.0f);
    static constexpr float maxAngle = juce::degreesToRadians(315.0f);
    static const std::vector<float> dialAngles;
    static const std::vector<float> steppedNormalizedValues;

    // CustomLookAndFeel customLookAndFeel; // needs to be FIRST!!
    VPM2AudioProcessor& processor;

    // InfoToggleButton InfoToggle; // tooltip toggle letter button

    juce::Image backgroundImage;
    Dial modDepthDial;  // mod amount dial
    Dial maxDelayDial;  // max delay range dial
    SlidingSwitch slideSwitch; //algorithm 3 positon selector switch
    SidewaysToggleSwitch invertToggle; // invert
    SidewaysToggleSwitch pdcToggle;
    SidewaysToggleSwitch interpolationTypeToggle; // invert
    SidewaysToggleSwitch oversamplingToggle;
    CustomCutoffSlider lpfSlider;

    juce::OwnedArray<Dial> dials;

    // Attachments
    juce::Slider modDepthSlider; // Continuous
    juce::Slider maxDelaySlider; // Stepped

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modDepthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> maxDelayAttachment;

    std::unique_ptr<juce::ParameterAttachment> algorithmAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> invertAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pdcAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> interpolationTypeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> oversamplingAttachment;


    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lpfSliderAttachment;

    

    // Helper for continuous (and stepped!) dials
    void addContinuousDial(
        int x, int y, int w, int h,
        float angleMin, float angleMax,
        const juce::String& parameterID,
        const juce::String& label);

    // LABELS
    juce::Label amountLabel;
    juce::Label rangeLabel;
    juce::Label invertLabel;
    juce::Label pdcLabel;
    juce::Label interpolationTypeLabel;
    juce::Label oversamplingLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VpmGUIAudioProcessorEditor)
};