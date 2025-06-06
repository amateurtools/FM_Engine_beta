// this is the three position inverted triangle algorithm selector switch class

#include "SlidingSwitch.h"

SlidingSwitch::SlidingSwitch()
{
    setSize(recommendedWidth, recommendedHeight);
    setInterceptsMouseClicks(true, true);
    updateKnobPosition();
}

void SlidingSwitch::paint(juce::Graphics& g)
{
    // Draw the full background (60x20)
    g.setColour(juce::Colour::fromRGB(40, 40, 40));
    g.fillRect(getLocalBounds());

    // Use class constant for knob size
    float knobY = 1.0f; // 1px from top and bottom

    float knobX = 1.0f; // default to left
    if (currentIndex == 1)
        knobX = (getWidth() - knobSize) / 2.0f;
    else if (currentIndex == 2)
        knobX = getWidth() - knobSize - 1.0f;

    // Draw the knob (18x18, with 1px margin all around)
    g.setColour(juce::Colour::fromRGB(80, 80, 80));
    g.fillRect(knobX, knobY, knobSize, knobSize);

    // Draw triangle indicator centered on knob (optional)
    juce::Path trianglePath;
    float triangleBaseWidth = 10.0f;
    float triangleHeight = 6.0f;
    float centerX = knobX + knobSize / 2.0f;
    float centerY = knobY + knobSize / 2.0f;

    juce::Point<float> point1(centerX - triangleBaseWidth / 2.0f, centerY - triangleHeight / 2.0f);
    juce::Point<float> point2(centerX + triangleBaseWidth / 2.0f, centerY - triangleHeight / 2.0f);
    juce::Point<float> point3(centerX, centerY + triangleHeight / 2.0f);

    trianglePath.startNewSubPath(point1);
    trianglePath.lineTo(point2);
    trianglePath.lineTo(point3);
    trianglePath.closeSubPath();

    g.setColour(juce::Colour::fromRGB(160, 160, 160));
    g.fillPath(trianglePath);
}

void SlidingSwitch::resized()
{
    updateKnobPosition();
}

void SlidingSwitch::mouseDown(const juce::MouseEvent& event)
{
    float width = getWidth();
    float clickX = event.x;

    int newIndex = 0;
    if (clickX < width / 3.0f)
        newIndex = 0;
    else if (clickX < 2.0f * width / 3.0f)
        newIndex = 1;
    else
        newIndex = 2;

    isDragging = true;
    setPosition(newIndex);
    if (onPositionChanged)
        onPositionChanged(currentIndex);
}

void SlidingSwitch::mouseDrag(const juce::MouseEvent& event)
{
    mouseDown(event); // Same logic as click
}

void SlidingSwitch::mouseUp(const juce::MouseEvent&)
{
    isDragging = false;
}

int SlidingSwitch::getPosition() const
{
    return currentIndex;
}

void SlidingSwitch::setPosition(int newPosition)
{
    int clamped = juce::jlimit(0, 2, newPosition);
    if (currentIndex != clamped)
    {
        currentIndex = clamped;
        updateKnobPosition();
        if (onPositionChanged)
            onPositionChanged(currentIndex);
    }
    else
    {
        updateKnobPosition();
    }
}

void SlidingSwitch::updateKnobPosition()
{
    float width = getWidth();

    switch (currentIndex)
    {
        case 0: knobX = 1.0f; break; // left, 1px margin
        case 1: knobX = (width - knobSize) / 2.0f; break; // center
        case 2: knobX = width - knobSize - 1.0f; break; // right, 1px margin
    }
    repaint();
}
