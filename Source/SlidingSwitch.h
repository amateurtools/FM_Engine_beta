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


/**
 * A simple 3-position horizontal sliding switch component.
 * 
 * Size recommendation: 60x20 pixels with an 18x18 knob.
 * 
 * Positions are indexed 0 (left), 1 (center), and 2 (right).
 */
class SlidingSwitch : public juce::Component
{
public:
    SlidingSwitch();

    /** Paints the component. */
    void paint(juce::Graphics&) override;

    /** Handles component resizing. */
    void resized() override;

    /** Handles mouse down events for interaction. */
    void mouseDown(const juce::MouseEvent&) override;

    /** Handles mouse drag events for interaction. */
    void mouseDrag(const juce::MouseEvent&) override;

    /** Handles mouse up events (currently unused). */
    void mouseUp(const juce::MouseEvent&) override;

    /**
     * Sets the switch to a new position (0, 1, or 2).
     * @param newPosition The new position index.
     */
    void setPosition(int newPosition);

    /**
     * Returns the current switch position (0, 1, or 2).
     */
    int getPosition() const;

    /**
     * Callback invoked when the position changes (user interaction or programmatic).
     * The parameter is the new position index.
     */
    std::function<void(int)> onPositionChanged;

    /**
     * Returns true if the user is currently dragging the knob.
     */
    bool isCurrentlyDragging() const { return isDragging; }

    /** Recommended size for this switch (for reference). */
    static constexpr int recommendedWidth = 60;
    static constexpr int recommendedHeight = 20;
    static constexpr float knobSize = 18.0f;

private:
    int currentIndex = 0;      // Current position index (0,1,2)
    float knobX = 0.0f;        // Current knob X position
    bool isDragging = false;   // True while dragging

    /** Updates knobX to reflect currentIndex and triggers repaint. */
    void updateKnobPosition();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlidingSwitch)
};
