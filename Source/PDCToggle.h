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


class PDCToggle : public juce::ToggleButton
{
public:
    PDCToggle() = default;
    ~PDCToggle() override = default;

    void paintButton(juce::Graphics& g, bool /*isMouseOver*/, bool /*isButtonDown*/) override
    {
        // Draw background rectangle
        g.setColour(juce::Colour(40, 40, 40));
        g.fillRect(0, 0, 40, 20);
    
        // Calculate square position
        int squareY = 1; // Centered vertically with 1px margin
        int squareX = getToggleState() ? 21 : 1; // 1px from left if off, 21px from left if on
    
        // Change color based on state
        g.setColour(getToggleState() ? juce::Colour(0, 200, 100) : juce::Colour(80, 80, 80));
        g.fillRect(squareX, squareY, 18, 18);
    }
    

    juce::Rectangle<int> getSquareBounds() const
    {
        int squareY = 1;
        int squareX = getToggleState() ? 21 : 1;
        return { squareX, squareY, 18, 18 };
    }
};