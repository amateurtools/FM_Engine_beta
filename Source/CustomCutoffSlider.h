#pragma once

#if __has_include("JuceHeader.h")
// Projucer build
#include "JuceHeader.h"
#else
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#endif

class CustomCutoffSlider : public juce::Slider
{
public:
    CustomCutoffSlider()
        : juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox)
    {
        setRange(minCutoff, maxCutoff, 0.0); // No step, continuous
        setSize(300, 20);
        setWantsKeyboardFocus(false);
    }

    // --- HYBRID MAPPING OVERRIDES ---
    double proportionOfLengthToValue(double proportion) override
    {
        // [0,1] -> Hz
        if (proportion < 0.5)
            // Linear from 20 to midpointFrequency Hz
            return 20.0 + (midpointFrequency - 20.0) * (proportion / 0.5);
        else
            // Log from 60 Hz to 20,000 Hz
            return 200.0 * std::pow(20000.0 / midpointFrequency, (proportion - 0.5) / 0.5);
    }

    double valueToProportionOfLength(double value) override
    {
        // Hz -> [0,1]
        if (value < midpointFrequency)
            // Linear region
            return 0.5 * (value - 20.0) / (midpointFrequency - 20.0);
        else
            // Log region
            return 0.5 + 0.5 * std::log(value / midpointFrequency) / std::log(20000.0 / midpointFrequency);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(40, 40, 40));
        g.fillRect(bounds);

        // Knob dimensions
        const float knobWidth = 40.0f;
        const float knobHeight = 18.0f;

        float sliderRange = bounds.getWidth() - knobWidth;
        float normValue = (float) valueToProportionOfLength(getValue());
        float knobX = sliderRange * normValue;
        float knobY = (bounds.getHeight() - knobHeight) * 0.5f;

        // Draw knob
        g.setColour(juce::Colour(80, 80, 80));
        g.fillRoundedRectangle(knobX, knobY, knobWidth, knobHeight, 4.0f);

        // Draw "LPF" label centered on knob
        g.setColour(juce::Colour(170, 170, 170));
        g.setFont(juce::Font(juce::FontOptions()
            .withName("Arial")
            .withHeight(13.0f)
            .withStyle("Bold")));
        g.drawText("LPF",
                   juce::Rectangle<int>((int)knobX, (int)knobY, (int)knobWidth, (int)knobHeight),
                   juce::Justification::centred, false);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        auto bounds = getLocalBounds().toFloat();
        const float knobWidth = 40.0f;
        const float knobHeight = 20.0f;
        float sliderRange = bounds.getWidth() - knobWidth;
        float normValue = (float) valueToProportionOfLength(getValue());
        float knobX = sliderRange * normValue;
        float knobY = (bounds.getHeight() - knobHeight) * 0.5f;
        juce::Rectangle<float> knobRect(knobX, knobY, knobWidth, knobHeight);

        draggingKnob = knobRect.contains(event.position);
        dragStartValue = getValue();

        if (draggingKnob && event.mods.isShiftDown())
            if (onBypassOversamplingChanged) onBypassOversamplingChanged(true);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!draggingKnob)
            return;

        auto bounds = getLocalBounds().toFloat();
        const float knobWidth = 40.0f;
        float sliderRange = bounds.getWidth() - knobWidth;
        float mouseX = juce::jlimit(0.0f, sliderRange, event.position.x - knobWidth * 0.5f);
        float newNorm = juce::jlimit(0.0f, 1.0f, mouseX / sliderRange);

        setSliderValueFromNorm(newNorm);

        if (onBypassOversamplingChanged)
            onBypassOversamplingChanged(event.mods.isShiftDown());
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        draggingKnob = false;
        if (onBypassOversamplingChanged)
            onBypassOversamplingChanged(false);
    }

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        auto bounds = getLocalBounds().toFloat();
        const float knobWidth = 40.0f;
        float sliderRange = bounds.getWidth() - knobWidth;
        float normValue = (float) valueToProportionOfLength(getValue());
        float knobX = sliderRange * normValue;
        float knobY = (bounds.getHeight() - knobWidth) * 0.5f;
        juce::Rectangle<float> knobRect(knobX, knobY, knobWidth, knobWidth);

        if (knobRect.contains(event.position))
        {
            float newNorm = juce::jlimit(0.0f, 1.0f, normValue + wheel.deltaY * 0.05f);
            setSliderValueFromNorm(newNorm);
        }
    }

    // Optional: callback for special UI actions
    std::function<void(bool)> onBypassOversamplingChanged;

private:

    static constexpr double minCutoff = 20.0;
    static constexpr double maxCutoff = 20000.0;
    bool draggingKnob = false;
    double dragStartValue = 0.0;

    // midpoint of cutoff mapping
    double midpointFrequency = 200.0; // your midpoint variable

    void setSliderValueFromNorm(float newNorm)
    {
        newNorm = juce::jlimit(0.0f, 1.0f, newNorm);
        double newValue;
        if (newNorm < 0.5)
            newValue = minCutoff + (midpointFrequency - minCutoff) * (newNorm / 0.5);
        else
            newValue = midpointFrequency * std::pow(maxCutoff / midpointFrequency, (newNorm - 0.5) / 0.5);

        newValue = juce::jlimit(getMinimum(), getMaximum(), newValue);
        setValue(newValue, juce::sendNotificationSync);
    }

};
