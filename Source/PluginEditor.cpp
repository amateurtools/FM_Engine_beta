#if __has_include("JuceHeader.h")
// Projucer build
#include "JuceHeader.h"
#else
// CMake build: include only the modules you need
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"
// ... add more as needed
// #include "BinaryData.h" // Only if you use binary data in this file
#endif

#include "PluginEditor.h"
#include "PluginProcessor.h"

VpmGUIAudioProcessorEditor::VpmGUIAudioProcessorEditor(VPM2AudioProcessor& p)

    : AudioProcessorEditor(p), processor(p) // Use reference, not pointer
{
    setSize(337, 600);

    backgroundImage = juce::ImageFileFormat::loadFrom(BinaryData::background_png, BinaryData::background_pngSize);
    if (!backgroundImage.isValid())
        DBG("Failed to load background image"
        );

    // ============== INSANELY OVERCOMPLICATED SLIDE SWITCH =====================================================

    addAndMakeVisible(slideSwitch);

    auto* algorithmParam = processor.apvts.getParameter("ALGORITHM");
    jassert(algorithmParam != nullptr);
    
    int maxAlgorithmIndex = static_cast<int>(dynamic_cast<juce::AudioParameterChoice*>(algorithmParam)->choices.size()) - 1;

    // Parameter -> Switch
    algorithmAttachment = std::make_unique<juce::ParameterAttachment>(
        *algorithmParam,
        [this, algorithmParam, maxAlgorithmIndex](float normalizedValueFromParameter)
        {
            if (slideSwitch.isCurrentlyDragging())
                return;
    
            float rawAlgorithmValue = algorithmParam->convertFrom0to1(normalizedValueFromParameter);
            int actualIndex = static_cast<int>(std::floor(rawAlgorithmValue + 0.5f));
            int clampedIndex = juce::jlimit(0, maxAlgorithmIndex, actualIndex);
    
            slideSwitch.setPosition(clampedIndex);
        }
    );

    // Switch -> Parameter
    slideSwitch.onPositionChanged = [algorithmParam, maxAlgorithmIndex](int index)
    {
        if (algorithmParam != nullptr)
        {
            int safeIndex = juce::jlimit(0, maxAlgorithmIndex, index);
            float normalizedValue = algorithmParam->convertTo0to1((float)safeIndex);
    
            algorithmParam->beginChangeGesture();
            algorithmParam->setValueNotifyingHost(normalizedValue);
            algorithmParam->endChangeGesture();
        }
    };

    // Set initial position from parameter value
    float initialAlgorithmNormalized = algorithmParam->getValue();
    float initialAlgorithmRaw = algorithmParam->convertFrom0to1(initialAlgorithmNormalized);
    slideSwitch.setPosition(juce::jlimit(0, maxAlgorithmIndex, static_cast<int>(std::round(initialAlgorithmRaw))));


    // --- MOD_DEPTH (continuous) ---
    modDepthDial.setBounds(45, 40, 100, 130);
    modDepthDial.setAngleRange(juce::degreesToRadians(120.0f), juce::degreesToRadians(420.0f));
    modDepthDial.setDragMode(Dial::VerticalDrag);
    addAndMakeVisible(modDepthDial);

    modDepthSlider.setRange(0.0, 1.0, 0.0); // continuous
    modDepthSlider.setVisible(false);
    modDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "MOD_DEPTH", modDepthSlider
    );

    // --- MAX_DELAY_MS (stepped) ---

    maxDelayDial.setBounds(195, 40, 100, 130);
    maxDelayDial.setAngleRange(minAngle, maxAngle);
    maxDelayDial.setDragMode(Dial::VerticalDrag);
    maxDelayDial.setSteppedValues(steppedNormalizedValues); // Use normalized values, NOT ms values
    addAndMakeVisible(maxDelayDial);

    maxDelaySlider.setRange(0, 2, 1); // 0 = 10ms, 1 = 100ms, 2 = 1000ms (indices)
    maxDelaySlider.setVisible(false);
    maxDelayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "MAX_DELAY_MS", maxDelaySlider
    );

    // --- Synchronization ---
    modDepthSlider.onValueChange = [this] {
        float angle = juce::jmap(
            static_cast<float>(modDepthSlider.getValue()),
            0.0f, 1.0f,
            juce::degreesToRadians(120.0f),
            juce::degreesToRadians(420.0f)
        );
        modDepthDial.setAngle(angle);
    };

    modDepthDial.onAngleChanged = [this](int, float angle) {
        float value = juce::jmap(
            angle,
            juce::degreesToRadians(120.0f),
            juce::degreesToRadians(420.0f),
            0.0f, 1.0f
        );
        modDepthSlider.setValue(value, juce::sendNotificationSync);
    };

    // Slider → Dial (index to normalized)
    maxDelaySlider.onValueChange = [this] {
        int idx = static_cast<int>(maxDelaySlider.getValue());
        if (idx >= 0 && idx < dialAngles.size())
            maxDelayDial.setAngle(dialAngles[idx]);
    };

    // When the dial changes (user moves it), set the slider index

    // Dial → Slider (normalized to index)
    maxDelayDial.onAngleChanged = [this](int, float angle) {
        int closestIdx = 0;
        float minDiff = std::abs(angle - dialAngles[0]);
        for (int i = 1; i < dialAngles.size(); ++i)
        {
            float diff = std::abs(angle - dialAngles[i]);
            if (diff < minDiff)
            {
                minDiff = diff;
                closestIdx = i;
            }
        }
        maxDelaySlider.setValue(closestIdx, juce::sendNotificationSync);
    };

    // --- Sensitivity ---
    modDepthDial.setSensitivity(0.005f);
    // maxDelayDial.setSensitivity(0.005f);

    // --- Visual Sync on GUI Show ---
    {
        float value = static_cast<float>(modDepthSlider.getValue());
        float angle = juce::jmap(
            value,
            0.0f, 1.0f,
            juce::degreesToRadians(120.0f),
            juce::degreesToRadians(420.0f)
        );
        modDepthDial.setAngle(angle);
    }
    {
        int idx = static_cast<int>(maxDelaySlider.getValue());
        if (idx >= 0 && idx < dialAngles.size())
            maxDelayDial.setAngle(dialAngles[idx]);
    }

    // ==================== invert switch ================================

    addAndMakeVisible(invertToggle);
    invertAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "INVERT", invertToggle);  
    
    invertLabel.setText("INVERT", juce::dontSendNotification);
    invertLabel.setFont(juce::Font(juce::FontOptions("Arial", 14.0f, juce::Font::bold)
        .withKerningFactor(0.01f) // Positive for more space, negative for less       
    ));
    invertLabel.setColour(juce::Label::textColourId, juce::Colour(170, 170, 170));
    invertLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(invertLabel);

    // ================== PDC TOGGLE ==================================

    addAndMakeVisible(pdcToggle);
    pdcAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "PDC", pdcToggle);

    pdcLabel.setText ("PDC", juce::dontSendNotification);
    pdcLabel.setFont (juce::Font (juce::FontOptions ("Arial", 14.0f, juce::Font::bold)
        .withKerningFactor (0.01f)
    ));
    pdcLabel.setColour (juce::Label::textColourId, juce::Colour (170, 170, 170));
    pdcLabel.setJustificationType (juce::Justification::right);
    addAndMakeVisible (pdcLabel);

    // ================== INTERPOLATION TYPE TOGGLE =========================

    addAndMakeVisible(interpolationTypeToggle);
    interpolationTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "INTERPOLATION_TYPE", interpolationTypeToggle);

    interpolationTypeLabel.setText("LAGRANGE", juce::dontSendNotification);
    interpolationTypeLabel.setFont(juce::Font(juce::FontOptions("Arial", 14.0f, juce::Font::bold)
        .withKerningFactor(0.01f)));
    interpolationTypeLabel.setColour(juce::Label::textColourId, juce::Colour(170, 170, 170));
    interpolationTypeLabel.setJustificationType(juce::Justification::left);
    addAndMakeVisible(interpolationTypeLabel);

    // ================== OVERSAMPLING TOGGLE ==============================

    addAndMakeVisible(oversamplingToggle);
    oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "OVERSAMPLING", oversamplingToggle);

    oversamplingLabel.setText("2X OS", juce::dontSendNotification);
    oversamplingLabel.setFont(juce::Font(juce::FontOptions("Arial", 14.0f, juce::Font::bold)
        .withKerningFactor(0.01f)));
    oversamplingLabel.setColour(juce::Label::textColourId, juce::Colour(170, 170, 170));
    oversamplingLabel.setJustificationType(juce::Justification::left);
    addAndMakeVisible(oversamplingLabel);

    //==============// lpfSlider =======================================================================
    
    addAndMakeVisible(lpfSlider);
    lpfSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "LP_CUTOFF", lpfSlider
    );
    
    lpfSlider.onBypassOversamplingChanged = [this](bool shouldBypass)
    {
        processor.bypassOversampling = shouldBypass;
    };

    //============== timer for the updating of the delay time and range ===================

    startTimerHz(30); // 30 Hz is smooth for UI

}

const std::vector<float> VpmGUIAudioProcessorEditor::dialAngles { 
    VpmGUIAudioProcessorEditor::minAngle, 
    VpmGUIAudioProcessorEditor::midAngle, 
    VpmGUIAudioProcessorEditor::maxAngle 
};
const std::vector<float> VpmGUIAudioProcessorEditor::steppedNormalizedValues { 0.0f, 0.5f, 1.0f };

// draw tic marks for range dial
void drawDialTicMarks(juce::Graphics& g)
{
    // Dial bounding box and center
    const float bboxX = 195.0f, bboxY = 40.0f, bboxW = 100.0f, bboxH = 130.0f;
    const float centerX = bboxX + bboxW * 0.5f;
    const float centerY = bboxY + bboxH * 0.5f;

    // Tic mark radii and angles
    const float innerRadius = 52.0f;
    const float outerRadius = 57.0f;
    const float startAngleDeg = 225.0f;
    const float endAngleDeg = 315.0f;
    const int numTics = 3;

    g.setColour(juce::Colour(125, 125, 125));

    for (int i = 0; i < numTics; ++i)
    {
        // Calculate angle for this tic
        float alpha = juce::jmap<float>(i, 0, numTics - 1, startAngleDeg, endAngleDeg);
        float angleRad = juce::degreesToRadians(alpha);

        float x1 = centerX + innerRadius * std::cos(angleRad);
        float y1 = centerY + innerRadius * std::sin(angleRad);
        float x2 = centerX + outerRadius * std::cos(angleRad);
        float y2 = centerY + outerRadius * std::sin(angleRad);

        g.drawLine(x1, y1, x2, y2, 1.0f); // 1px wide
    }
}

// ==========================================================================

VpmGUIAudioProcessorEditor::~VpmGUIAudioProcessorEditor()
{
    dials.clear(); // Optional safety measure

}

void VpmGUIAudioProcessorEditor::timerCallback()
{
    repaint();
}

void VpmGUIAudioProcessorEditor::paint(juce::Graphics& g)
{
    
    // Use high-quality resampling for sharp downscaling
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

    if (backgroundImage.isValid())
    {
        // Draw the image scaled to exactly fill the component's bounds
        g.drawImage(backgroundImage,
            getLocalBounds().toFloat(),
            juce::RectanglePlacement::stretchToFit,
            false);                                  // don't fillAlphaChannel
    }

    // display the plugin name banner
    g.setFont(juce::Font(juce::FontOptions("Arial", 26.0f, juce::Font::plain)
        .withKerningFactor(0.05f) // Positive for more space, negative for less
    ));
    g.setColour(juce::Colour(170, 170, 170)); // gold
    g.drawText("FM Engine",
                getLocalBounds().removeFromTop(55), // Top banner area
                juce::Justification::centred, true);

    drawDialTicMarks(g);

    // ====================== write the current delay times as knob labels
    auto* modDepthParam = processor.apvts.getRawParameterValue("MOD_DEPTH");

    float maxDelayMs = processor.getMaxDelayMsFromChoice();
    float modDepth   = modDepthParam ? modDepthParam->load() : 0.0f;
    float modAmountMs = maxDelayMs * modDepth;

    // Optionally clamp for display
    modAmountMs = juce::jlimit(0.0f, maxDelayMs, modAmountMs);

    g.setFont(juce::Font(juce::FontOptions("Arial", 14.0f, juce::Font::bold).withKerningFactor(0.01f)));
    g.setColour(juce::Colour(170, 170, 170));

    if (maxDelayMs && modDepthParam)
    {
        juce::String modAmountText = juce::String(modAmountMs, 2) + " ms";
        juce::String maxDelayText  = juce::String(maxDelayMs, 0) + " ms";
        g.drawFittedText(modAmountText, amountLabel.getBounds(), juce::Justification::centred, 1);
        g.drawFittedText(maxDelayText,  rangeLabel.getBounds(),  juce::Justification::centred, 1);
    }
    else
    {
        g.setColour(juce::Colours::red);
        g.drawText("Param error", 10, 10, 100, 20, juce::Justification::left);
    }
}

void VpmGUIAudioProcessorEditor::resized()
{
    amountLabel.setBounds(47, 166, 100, 20); // AMOUNT label
    rangeLabel.setBounds(196, 165, 100, 20);  // RANGE label
    
    slideSwitch.setBounds(138, 216, 60, 20);  // algorithm
    lpfSlider.setBounds(20, 499, 300, 20);    // lpf slider

    invertToggle.setBounds(280, 419, 40, 20); // invert toggle
    invertLabel.setBounds(170, 419, 100, 20); 

    pdcToggle.setBounds(280, 459, 40, 20);    // PDC toggle
    pdcLabel.setBounds(170, 459, 100, 20);

    interpolationTypeToggle.setBounds(20, 419, 40, 20); // interpolation type toggle
    interpolationTypeLabel.setBounds(65, 419, 145, 20); 

    oversamplingToggle.setBounds(20, 459, 40, 20);    // oversampling toggle
    oversamplingLabel.setBounds(65, 459, 145, 20);

}