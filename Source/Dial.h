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


// Custom rotary dial control for stepped or continuous values
class Dial : public juce::Component
{
public:
    Dial(float initialAngle = juce::degreesToRadians(120.0f), int id = 0);

    // Set the dial's angle (radians, within min/max)
    void setAngle(float newAngle);
    float getAngle() const;

    // Set or clear stepped values (for stepped dials)
    // Dial.h
    void setSteppedValues(const std::vector<float>& values);

    void clearSteppedValues();

    // Sensitivity adjustment for drag
    void setSensitivity(float newSensitivity);
    float getSensitivity() const;

    // Set the sweep range in radians
    void setAngleRange(float minRadians, float maxRadians) {
        minAngleRadians = minRadians;
        maxAngleRadians = maxRadians;
        setAngle(angle); // Clamp current angle
    }

    // For stepped dials, get the current value
    float getCurrentSteppedValue() const;

    // Label handling
    void setLabel(const juce::String& newLabel) { label = newLabel; }
    const juce::String& getLabel() const { return label; }

    // Drag mode: vertical, horizontal, or rotary
    enum DragMode { VerticalDrag, HorizontalDrag, RotaryDrag };
    void setDragMode(DragMode mode) { dragMode = mode; }

    bool isCurrentlyDragging() const { return isDragging; }

    // For custom drawing
    void draw(juce::Graphics& g, const juce::Point<float>& center, float radius);

    // JUCE component overrides
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    // Callback for angle changes (id, angle)
    std::function<void(int, float)> onAngleChanged;

    int getId() const { return id; }

private:
    float angle;
    float minAngleRadians = 0.0f;
    float maxAngleRadians = juce::MathConstants<float>::twoPi;

    float sensitivity = 0.005f;

    DragMode dragMode = VerticalDrag;

    std::vector<float> steppedValues;
    int currentStepIndex = -1; // -1 means no step selected

    juce::Point<float> dragStartPos;
    float dragStartAngle;
    bool isDragging = false;

    int id;
    juce::String label;
};
