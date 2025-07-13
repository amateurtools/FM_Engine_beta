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


class FmEngineAudioProcessor; // Forward declaration

class FmEngineAudioProcessorEditor  : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    FmEngineAudioProcessorEditor (FmEngineAudioProcessor&);
    ~FmEngineAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void updateTooltips(); 

    // for hidden control panel
    void mouseDown(const juce::MouseEvent& event) override;

private:
    std::vector<juce::Component*> guiComponents;
    
    // all this because i want a stepped dial that uses arbitrary values in a vector
    static constexpr float minAngle = juce::degreesToRadians(225.0f);
    static constexpr float midAngle = juce::degreesToRadians(270.0f);
    static constexpr float maxAngle = juce::degreesToRadians(315.0f);
    static const std::vector<float> dialAngles;
    static const std::vector<float> steppedNormalizedValues;

    // CustomLookAndFeel customLookAndFeel; // needs to be FIRST!!
    FmEngineAudioProcessor& processor;

    // InfoToggleButton InfoToggle; // tooltip toggle letter button

    juce::Image backgroundImage;
    Dial modDepthDial;  // mod amount dial
    Dial maxDelayDial;  // max delay range dial
    SlidingSwitch slideSwitch; //algorithm 3 positon selector switch
    SidewaysToggleSwitch swapToggle; // swap
    SidewaysToggleSwitch predelayToggle;
    SidewaysToggleSwitch limiterToggle; // swap
    SidewaysToggleSwitch oversamplingToggle;
    CustomCutoffSlider lpfSlider;

    juce::OwnedArray<Dial> dials;

    // Attachments
    juce::Slider modDepthSlider; // Continuous
    juce::Slider maxDelaySlider; // Stepped

    // Hidden Control Panel stuff =================================================================

    bool controlPanelVisible = false;
    juce::Rectangle<int> controlPanelBounds { 0, 201, 337, 200 };
    juce::Rectangle<int> sandwichIconBounds { 10, 10, 20, 20 }; // Top-left corner, adjust as needed

    // SidewaysToggleSwitch modClipArctanOrSineFold;
    // SidewaysToggleSwitch outputArctanClip;
    // Dial modCarBalance;
    // Dial gainIn;
    // Dial dryWet;
    // Dial gainOut;

    // Optionally, keep a pointer/reference to the slider you want to hide
    // juce::Slider* sliderToHide = nullptr;

    // =============================================================================================

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modDepthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> maxDelayAttachment;

    std::unique_ptr<juce::ParameterAttachment> algorithmAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> swapAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> predelayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> limiterAttachment;
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
    juce::Label swapLabel;
    juce::Label predelayLabel;
    juce::Label limiterLabel;
    juce::Label oversamplingLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FmEngineAudioProcessorEditor)
};