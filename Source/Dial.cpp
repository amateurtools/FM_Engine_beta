#if __has_include("JuceHeader.h")
// Projucer build
#include "JuceHeader.h"
#else
// CMake build: include only the modules you need
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#endif

#include "Dial.h"

Dial::Dial(float initialAngle, int dialId)
    : angle(initialAngle), id(dialId), sensitivity(0.005f), dragStartAngle(0.0f)
{
    setAngle(angle);
}

void Dial::setSteppedValues(const std::vector<float>& values)
{
    steppedValues = values;

    if (steppedValues.empty())
    {
        currentStepIndex = -1;
        return;
    }

    // Convert current angle to normalized [0, 1]
    float norm = (angle - minAngleRadians) / (maxAngleRadians - minAngleRadians);

    // Map normalized value to the stepped value range
    float valueRangeStart = steppedValues.front();
    float valueRangeEnd = steppedValues.back();
    float value = valueRangeStart + norm * (valueRangeEnd - valueRangeStart);

    // Find the closest stepped value & its index
    float closestStep = steppedValues.front();
    int closestIndex = 0;
    float minDiff = std::abs(value - closestStep);
    for (int i = 1; i < (int)steppedValues.size(); ++i)
    {
        float diff = std::abs(value - steppedValues[i]);
        if (diff < minDiff)
        {
            minDiff = diff;
            closestStep = steppedValues[i];
            closestIndex = i;
        }
    }

    currentStepIndex = closestIndex;

    // Optionally, update angle to match the closest step
    float denom = valueRangeEnd - valueRangeStart;
    float quantizedNorm = 0.0f;
    if (std::abs(denom) > 1e-8f)
        quantizedNorm = (closestStep - valueRangeStart) / denom;
    // else quantizedNorm stays 0.0f

    angle = minAngleRadians + quantizedNorm * (maxAngleRadians - minAngleRadians);

    repaint();
}

void Dial::clearSteppedValues()
{
    steppedValues.clear();
    currentStepIndex = -1;
}

// In Dial.cpp

void Dial::setAngle(float newAngle)
{
    float clampedAngle = juce::jlimit(minAngleRadians, maxAngleRadians, newAngle);

    if (!steppedValues.empty())
    {
        // Map angle to normalized [0, 1]
        float norm = (clampedAngle - minAngleRadians) / (maxAngleRadians - minAngleRadians);
        norm = juce::jlimit(0.0f, 1.0f, norm);

        // Map normalized to value range (100 to 900)
        float valueRange = steppedValues.back() - steppedValues.front();
        float value = steppedValues.front() + norm * valueRange;

        // Snap to nearest step
        float closestStep = steppedValues.front();
        float minDist = std::abs(value - closestStep);
        for (auto v : steppedValues)
        {
            float dist = std::abs(v - value);
            if (dist < minDist)
            {
                closestStep = v;
                minDist = dist;
            }
        }

        // Convert back to normalized
        norm = (closestStep - steppedValues.front()) / valueRange;
        clampedAngle = minAngleRadians + norm * (maxAngleRadians - minAngleRadians);
    }

    if (angle != clampedAngle)
    {
        angle = clampedAngle;
        repaint();
    }
}



float Dial::getAngle() const { return angle; }

void Dial::setSensitivity(float newSensitivity) { sensitivity = newSensitivity; }
float Dial::getSensitivity() const { return sensitivity; }

float Dial::getCurrentSteppedValue() const
{
    if (steppedValues.empty())
        return 0.0f; // or some default fallback

    if (currentStepIndex < 0 || currentStepIndex >= static_cast<int>(steppedValues.size()))
        return steppedValues.front(); // fallback to first stepped value

    return steppedValues[currentStepIndex];
}

void Dial::draw(juce::Graphics &g, const juce::Point<float> &center, float radius)
{
    // Draw the background circle for the knob
    g.setColour(juce::Colour::fromRGB(40, 40, 40)); // "black"
    g.drawEllipse(center.x - radius, center.y - radius, 2.0f * radius,
                  2.0f * radius, 2.0f);

    // Draw the pointer
    float pointerLength = radius - 5.0f;
    float triangleSize = 20.0f;
    juce::Point<float> tip =
        center + juce::Point<float>(std::cos(angle) * pointerLength,
                                    std::sin(angle) * pointerLength);
    juce::Point<float> perpVec =
        juce::Point<float>(-std::sin(angle), std::cos(angle)) *
        (triangleSize / (2.0f * std::sqrt(3.0f))) * 2.0f;
    juce::Point<float> direction = center - tip;
    float length =
        std::sqrt(direction.x * direction.x + direction.y * direction.y);
    juce::Point<float> baseCenter =
        tip + direction / length * (triangleSize / std::sqrt(3.0f));
    juce::Point<float> base1 = baseCenter - perpVec * 0.5f;
    juce::Point<float> base2 = baseCenter + perpVec * 0.5f;
    juce::Path triangle;
    triangle.addTriangle(tip, base1, base2);
    g.setColour(juce::Colour::fromRGB(160, 160, 160)); // "white"
    g.fillPath(triangle);
}

void Dial::paint(juce::Graphics &g)
{
    // Assume the component size is set to 100x100 elsewhere (e.g., setSize(100, 100);)
    auto bounds = getLocalBounds().toFloat();
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
    juce::Point<float> center(bounds.getCentreX(), bounds.getCentreY());

    // Call your custom draw method to render the dial
    draw(g, center, radius);
}


void Dial::mouseDown(const juce::MouseEvent &event)
{
    dragStartPos = event.position;
    dragStartAngle = angle;
    isDragging = true;
    DBG("Dial " << id << " (" << label << "): mouse down at "
                << event.position.toString()
                << ", startAngle=" << dragStartAngle);
}

void Dial::mouseDrag(const juce::MouseEvent &event)
{
    if (!isDragging)
        return;

    auto center = getLocalBounds().getCentre().toFloat();
    float angleRange = maxAngleRadians - minAngleRadians;
    float newAngle = angle;

    // Apply reduced sensitivity if Ctrl or Shift is held
    float effectiveSensitivity = sensitivity;
    if (event.mods.isCtrlDown() || event.mods.isShiftDown())
        effectiveSensitivity *= 0.5f;

    switch (dragMode)
    {
        case VerticalDrag:
        {
            float deltaY = -(event.position.y - dragStartPos.y) * effectiveSensitivity;
            float normalized = (dragStartAngle - minAngleRadians) / angleRange + deltaY;
            normalized = juce::jlimit(0.0f, 1.0f, normalized);
            newAngle = minAngleRadians + normalized * angleRange;
            break;
        }
        case HorizontalDrag:
        {
            float deltaX = (event.position.x - dragStartPos.x) * effectiveSensitivity;
            float normalized = (dragStartAngle - minAngleRadians) / angleRange + deltaX;
            normalized = juce::jlimit(0.0f, 1.0f, normalized);
            newAngle = minAngleRadians + normalized * angleRange;
            break;
        }
        case RotaryDrag:
        {
            juce::Point<float> delta = event.position - center;
            float newAngleFromDrag = std::atan2(delta.y, delta.x);

            juce::Point<float> startDelta = dragStartPos - center;
            float startAngleFromDrag = std::atan2(startDelta.y, startDelta.x);
            float angleDelta = newAngleFromDrag - startAngleFromDrag;

            if (angleDelta > juce::MathConstants<float>::pi)
                angleDelta -= juce::MathConstants<float>::twoPi;
            else if (angleDelta < -juce::MathConstants<float>::pi)
                angleDelta += juce::MathConstants<float>::twoPi;

            newAngle = dragStartAngle + angleDelta;
            break;
        }
    }

    // Store previous angle to detect changes
    float previousAngle = angle;
    setAngle(newAngle);

    // Only notify if angle actually changed
    if (std::abs(angle - previousAngle) > 1e-6f && onAngleChanged != nullptr)
        onAngleChanged(id, angle);

    DBG("Dial " << id << " (" << label << "): dragged to angle=" << angle
                << ", min=" << minAngleRadians << ", max=" << maxAngleRadians
                << ", effectiveSensitivity=" << effectiveSensitivity);
}


void Dial::mouseUp(const juce::MouseEvent &)
{
    isDragging = false;
    DBG("Dial " << id << " (" << label << "): mouse up, final angle=" << angle);
}

void Dial::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    float angleStep = 0.2f;
    if (event.mods.isCtrlDown() || event.mods.isShiftDown())
        angleStep *= 0.5f;
    float deltaAngle = wheel.deltaY * angleStep;
    float newAngle = angle + deltaAngle;

    setAngle(newAngle); // Unified update path

    // Notify listeners (e.g., editor lambda) about the angle change
    if (onAngleChanged)
        onAngleChanged(id, angle);

    DBG("Dial " << id << " (" << label
                << "): mouse wheel, deltaY=" << wheel.deltaY
                << ", newAngle=" << angle << ", angleStep=" << angleStep);
}



