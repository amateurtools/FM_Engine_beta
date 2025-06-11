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

class VPM2AudioProcessor; // Forward declaration

// class CustomLookAndFeel : public juce::LookAndFeel_V4
// {
// public:
//     CustomLookAndFeel() = default;

// In your CustomLookAndFeel:

//     void drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) override
//     {
//         // Larger, bold font
//         juce::Font font(juce::FontOptions().withHeight(20.0f).withStyle("Bold"));
//         g.setFont(font);

//         // Semi-transparent, rounded background
//         juce::Colour bgColour = juce::Colour(40, 40, 40).withAlpha(0.92f);
//         g.setColour(bgColour);
//         g.fillRoundedRectangle(juce::Rectangle<float>(0, 0, (float)width, (float)height), 8.0f);

//         // Tooltip text color
//         g.setColour(juce::Colour(220, 220, 220));

//         // Squarish padding for more balanced aspect
//         int paddingX = 18, paddingY = 12;
//         juce::Rectangle<int> textBounds(paddingX, paddingY, width - 2 * paddingX, height - 2 * paddingY);

//         // Draw word-wrapped text, up to 8 lines for squarish tooltips
//         g.drawFittedText(text, textBounds, juce::Justification::centredLeft, 8);
//     }
// };

// class InfoToggleButton : public juce::TextButton
// {
// public:
//     InfoToggleButton()
//     {
//         setClickingTogglesState(true); // Make it toggleable
//     }

//     void paintButton(juce::Graphics& g, bool, bool) override
//     {
//         g.setFont(juce::Font(juce::FontOptions()
//             .withName("Consolas, SF Mono, Menlo, Monaco, monospace")
//             .withHeight(18.0f)
//             .withStyle("Bold")));
//         g.setColour(getToggleState() ? juce::Colours::red : juce::Colours::black);
//         g.drawText("?", getLocalBounds(), juce::Justification::centred, false);
//     }
// };

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