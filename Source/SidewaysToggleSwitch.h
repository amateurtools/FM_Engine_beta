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


class SidewaysToggleSwitch : public juce::ToggleButton
{
public:
    SidewaysToggleSwitch() = default;
    ~SidewaysToggleSwitch() override = default;

    void paintButton(juce::Graphics& g, bool /*isMouseOver*/, bool /*isButtonDown*/) override
    {
        // Draw background
        g.setColour(juce::Colour(40, 40, 40));
        g.fillRect(0, 0, 40, 20);
    
        // Move knob left or right depending on state
        int squareSize = 18;
        int squareY = (20 - squareSize) / 2;
        int squareX = getToggleState() ? (40 - squareSize - 2) : 2;
    
        g.setColour(getToggleState() ? juce::Colour(80, 80, 80) : juce::Colour(80, 80, 80));
        g.fillRect(squareX, squareY, squareSize, squareSize);
    }
    

    // Optional: override getPreferredSize so layout managers can size it automatically
    juce::Rectangle<int> getPreferredSize() const { return { 0, 0, 40, 20 }; }
};
