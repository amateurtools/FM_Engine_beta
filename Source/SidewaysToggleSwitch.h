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
        auto bounds = getLocalBounds().toFloat();

        // Draw background with 2px corner radius
        g.setColour(juce::Colour(40, 40, 40));
        g.fillRoundedRectangle(bounds, 2.0f);

        // Draw DX7 green rounded rectangle indicator, 12x12, centered in left half
        const float indWidth = 12.0f;
        const float indHeight = 12.0f;
        const float indX = (20.0f - indWidth) / 2.0f;
        const float indY = (bounds.getHeight() - indHeight) / 2.0f;
        const float indCorner = 3.0f; // small rounding for indicator

        g.setColour(juce::Colour(119, 152, 103)); // DX7 green
        g.fillRoundedRectangle(indX, indY, indWidth, indHeight, indCorner);

        // Draw sliding knob (18x18, 2px rounding), covers indicator when off
        const float knobSize = 18.0f;
        const float knobY = (bounds.getHeight() - knobSize) / 2.0f;
        const float knobX = getToggleState() ? (bounds.getWidth() - knobSize - 1.0f) : 1.0f;
        const float knobCorner = 2.0f;

        g.setColour(juce::Colour(80, 80, 80));
        g.fillRoundedRectangle(knobX, knobY, knobSize, knobSize, knobCorner);
    }

    juce::Rectangle<int> getPreferredSize() const { return { 0, 0, 40, 20 }; }
};


