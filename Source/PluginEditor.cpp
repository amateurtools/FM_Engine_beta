#if __has_include("JuceHeader.h")
#include "JuceHeader.h"
#else
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"
#endif

#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
// Custom LookAndFeel for rotating PNG knobs
class RotaryKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RotaryKnobLookAndFeel()
    {
        // Load the single knob PNG from BinaryData
        knobImage = juce::ImageCache::getFromMemory(BinaryData::knob_png, BinaryData::knob_pngSize);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                         float sliderPosProportional, float rotaryStartAngle,
                         float rotaryEndAngle, juce::Slider& /*slider*/) override
    {
        if (!knobImage.isValid())
        {
            // Fallback: draw a simple circle if image fails to load
            g.setColour(juce::Colours::grey);
            g.fillEllipse(x, y, width, height);
            return;
        }

        // Calculate rotation angle based on slider position
        float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        
        // Create transform for rotation around center
        auto transform = juce::AffineTransform::rotation(angle, 
                                                         knobImage.getWidth() * 0.5f, 
                                                         knobImage.getHeight() * 0.5f);
        
        // Draw the rotated knob image
        g.drawImageTransformed(knobImage, 
                              transform.translated(x, y),
                              false);
    }

private:
    juce::Image knobImage;
};

//==============================================================================
// Custom LookAndFeel for 4-position stepped knob
class SteppedKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SteppedKnobLookAndFeel()
    {
        knobImage = juce::ImageCache::getFromMemory(BinaryData::knob_png, BinaryData::knob_pngSize);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                         float sliderPosProportional, float rotaryStartAngle,
                         float rotaryEndAngle, juce::Slider&) override
    {
        if (!knobImage.isValid())
        {
            g.setColour(juce::Colours::grey);
            g.fillEllipse(x, y, width, height);
            return;
        }

        // Snap to one of 4 positions: 0, 0.333, 0.666, 1.0
        float snappedPos;
        if (sliderPosProportional < 0.25f)
            snappedPos = 0.0f;           // Position 0 (1ms)
        else if (sliderPosProportional < 0.5f)
            snappedPos = 0.333f;         // Position 1 (10ms)
        else if (sliderPosProportional < 0.75f)
            snappedPos = 0.666f;         // Position 2 (100ms)
        else
            snappedPos = 1.0f;           // Position 3 (500ms)

        // Calculate rotation angle for the snapped position
        float angle = rotaryStartAngle + snappedPos * (rotaryEndAngle - rotaryStartAngle);

        // Create transform for rotation around center
        auto transform = juce::AffineTransform::rotation(angle, knobImage.getWidth() * 0.5f, 
                                                        knobImage.getHeight() * 0.5f);

        // Draw the rotated knob image
        g.drawImageTransformed(knobImage, transform.translated(x, y), false);
    }

private:
    juce::Image knobImage;
};


//==============================================================================
FmEngineAudioProcessorEditor::FmEngineAudioProcessorEditor(FmEngineAudioProcessor& p)
    : AudioProcessorEditor(p), processor(p)
{
    setSize(337, 600);

    // Load background image
    backgroundImage = juce::ImageFileFormat::loadFrom(BinaryData::background_png, 
                                                     BinaryData::background_pngSize);
    if (backgroundImage.isValid())
        backgroundImage = juce::SoftwareImageType().convert(backgroundImage);
    else
        DBG("Failed to load background image");

    // Set custom look and feel for knobs
    rotaryKnobLookAndFeel = std::make_unique<RotaryKnobLookAndFeel>();
    steppedKnobLookAndFeel = std::make_unique<SteppedKnobLookAndFeel>();
    
    //==========================================================================
    // MOD DEPTH KNOB (Continuous)
    //==========================================================================
    modDepthSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    modDepthSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    modDepthSlider.setRange(0.0, 1.0, 0.0);
    modDepthSlider.setRotaryParameters(juce::degreesToRadians(120.0f), 
                                       juce::degreesToRadians(420.0f), 
                                       true); // stopAtEnd = true
    modDepthSlider.setLookAndFeel(rotaryKnobLookAndFeel.get());
    addAndMakeVisible(modDepthSlider);
    
    modDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "MOD_DEPTH", modDepthSlider);

    //==========================================================================
    // MAX DELAY KNOB (Stepped - 4 positions: 210, 250, 290, 330)
    maxDelaySlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    maxDelaySlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

    // Use NormalisableRange for explicit stepped behavior
    juce::NormalisableRange<double> steppedRange(0.0, 3.0, 1.0);
    maxDelaySlider.setNormalisableRange(steppedRange);

    maxDelaySlider.setRotaryParameters(juce::degreesToRadians(210.0f), 
                                    juce::degreesToRadians(330.0f), 
                                    true);
    maxDelaySlider.setLookAndFeel(steppedKnobLookAndFeel.get());
    addAndMakeVisible(maxDelaySlider);

    maxDelayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "MAX_DELAY_MS", maxDelaySlider);




    //==========================================================================
    // ALGORITHM SWITCH (3-position)
    //==========================================================================
    addAndMakeVisible(slideSwitch);
    
    auto* algorithmParam = processor.apvts.getParameter("ALGORITHM");
    jassert(algorithmParam != nullptr);
    
    int maxAlgorithmIndex = static_cast<int>(
        dynamic_cast<juce::AudioParameterChoice*>(algorithmParam)->choices.size()) - 1;

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

    float initialAlgorithmNormalized = algorithmParam->getValue();
    float initialAlgorithmRaw = algorithmParam->convertFrom0to1(initialAlgorithmNormalized);
    slideSwitch.setPosition(juce::jlimit(0, maxAlgorithmIndex, 
                                        static_cast<int>(std::round(initialAlgorithmRaw))));

    //==========================================================================
    // TOGGLE SWITCHES
    //==========================================================================
    addAndMakeVisible(swapToggle);
    swapAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "SWAP", swapToggle);
    
    addAndMakeVisible(predelayToggle);
    predelayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "PREDELAY", predelayToggle);
    
    addAndMakeVisible(limiterToggle);
    limiterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "LIMITER", limiterToggle);
    
    addAndMakeVisible(oversamplingToggle);
    oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "OVERSAMPLING", oversamplingToggle);

    //==========================================================================
    // LABELS
    //==========================================================================
    swapLabel.setText("Swap", juce::dontSendNotification);
    swapLabel.setFont(juce::Font(juce::FontOptions("Arial", 14.0f, juce::Font::bold)));
    swapLabel.setColour(juce::Label::textColourId, juce::Colour(170, 170, 170));
    swapLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(swapLabel);

    predelayLabel.setText("PDC", juce::dontSendNotification);
    predelayLabel.setFont(juce::Font(juce::FontOptions("Arial", 14.0f, juce::Font::bold)));
    predelayLabel.setColour(juce::Label::textColourId, juce::Colour(170, 170, 170));
    predelayLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(predelayLabel);

    limiterLabel.setText("Limit", juce::dontSendNotification);
    limiterLabel.setFont(juce::Font(juce::FontOptions("Arial", 14.0f, juce::Font::bold)));
    limiterLabel.setColour(juce::Label::textColourId, juce::Colour(170, 170, 170));
    limiterLabel.setJustificationType(juce::Justification::left);
    addAndMakeVisible(limiterLabel);

    oversamplingLabel.setText("2X OS", juce::dontSendNotification);
    oversamplingLabel.setFont(juce::Font(juce::FontOptions("Arial", 14.0f, juce::Font::bold)));
    oversamplingLabel.setColour(juce::Label::textColourId, juce::Colour(170, 170, 170));
    oversamplingLabel.setJustificationType(juce::Justification::left);
    addAndMakeVisible(oversamplingLabel);

    //==========================================================================
    // LPF SLIDER
    //==========================================================================
    addAndMakeVisible(lpfSlider);
    lpfSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "LP_CUTOFF", lpfSlider);
    
    lpfSlider.onBypassOversamplingChanged = [this](bool shouldBypass)
    {
        processor.bypassOversampling = shouldBypass;
    };

    //==========================================================================
    // Collect all GUI components for hide/show functionality
    //==========================================================================
    guiComponents = {
        &modDepthSlider,
        &maxDelaySlider,
        &slideSwitch,
        &swapToggle,
        &predelayToggle,
        &limiterToggle,
        &oversamplingToggle,
        &lpfSlider,
        &swapLabel,
        &predelayLabel,
        &limiterLabel,
        &oversamplingLabel
    };

    startTimerHz(30); // 30 Hz refresh for parameter display
}

FmEngineAudioProcessorEditor::~FmEngineAudioProcessorEditor()
{
    // Reset look and feel before components are destroyed
    modDepthSlider.setLookAndFeel(nullptr);
    maxDelaySlider.setLookAndFeel(nullptr);
}

void FmEngineAudioProcessorEditor::timerCallback()
{
    repaint();
}

void FmEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Use medium-quality resampling for better compatibility
    g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);

    if (backgroundImage.isValid())
    {
        g.drawImage(backgroundImage,
                   getLocalBounds().toFloat(),
                   juce::RectanglePlacement::stretchToFit,
                   false);
    }

    // Title
    g.setFont(juce::Font(juce::FontOptions("Arial", 26.0f, juce::Font::bold)));
    g.setColour(juce::Colour(170, 170, 170));
    g.drawText("FM Engine",
              getLocalBounds().removeFromTop(55),
              juce::Justification::centred, true);

    // Draw tic marks for range dial
    drawDialTicMarks(g);

    // Display current delay times as knob labels
    auto* modDepthParam = processor.apvts.getRawParameterValue("MOD_DEPTH");
    float maxDelayMs = processor.getMaxDelayMsFromChoice();
    float modDepth = modDepthParam ? modDepthParam->load() : 0.0f;
    float modAmountMs = maxDelayMs * modDepth;
    modAmountMs = juce::jlimit(0.0f, maxDelayMs, modAmountMs);

    g.setFont(juce::Font(juce::FontOptions("Arial", 14.0f, juce::Font::bold)));
    g.setColour(juce::Colour(170, 170, 170));

    if (maxDelayMs && modDepthParam)
    {
        juce::String modAmountText = juce::String(modAmountMs, 2) + " ms";
        juce::String maxDelayText = juce::String(maxDelayMs, 0) + " ms";
        g.drawFittedText(modAmountText, 47, 166, 100, 20, juce::Justification::centred, 1);
        g.drawFittedText(maxDelayText, 196, 165, 100, 20, juce::Justification::centred, 1);
    }

    // Hidden control panel (info screen)
    if (controlPanelVisible)
    {
        g.setColour(juce::Colour::fromRGBA(42, 42, 42, 255));
        g.fillRect(getLocalBounds());

        g.setFont(juce::Font(juce::FontOptions("DejaVu Sans Mono", 16.0f, juce::Font::plain)));
        g.setColour(juce::Colour(204, 204, 204));

        juce::String infoText = 
            "      FM Engine - Ver. 071525      \n"
            "   (c)2014-2025 AmateurTools DSP   \n"
            "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n"
            "                                   \n"
            "Arbitrary-input frequency modulator\n"
            "with auxiliary sidechain input     \n"
            "with additional self-mod mono mode.\n"
            "(Expects Stereo and SC inputs, but \n"
            "works with just the main inputs.)  \n"
            "                                   \n"
            "Upper Right dial sets delay time.  \n"
            "Upper Left dial attenuates that.   \n"
            "                                   \n"
            "Alg 1: stereo in, L <- R           \n"
            "Alg 2: summed in <- summed SC      \n"
            "Alg 3: stereo in <- stereo SC      \n"
            "                                   \n"
            "LIMIT  Adds limitations to i/o     \n"
            "                                   \n"
            "SWAP   Swaps the Carrier/Modulator \n"
            "                                   \n"
            "OS2X   2x Oversampling             \n"
            "                                   \n"
            "PDC    Secures timing, but adds    \n"
            "       latency to the DAW.         \n"
            "                                   \n"
            "LPF    (with SHIFT+DRAG preview)   \n"
            "                                   \n"
            "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n"
            "                                   \n"
            "Thanks: JUCE, VST Steinberg GmBh,  \n"
            "  BedroomProducersBlog,            \n"
            "Concept inspired by: Autechre,     \n"
            "Beta Tester: Erik Lagerwall        \n";

        g.drawFittedText(infoText,
                        getLocalBounds().reduced(20),
                        juce::Justification::centredLeft,
                        35);
    }
}

void FmEngineAudioProcessorEditor::drawDialTicMarks(juce::Graphics& g)
{
    // Dial bounding box and center
    const float bboxX = 195.0f, bboxY = 40.0f, bboxW = 100.0f, bboxH = 130.0f;
    const float centerX = bboxX + bboxW * 0.5f;
    const float centerY = bboxY + bboxH * 0.5f;

    // Tic mark radii and angles
    const float innerRadius = 52.0f;
    const float outerRadius = 57.0f;
    const float startAngleDeg = 210.0f;
    const float endAngleDeg = 330.0f;
    const int numTics = 4;


    g.setColour(juce::Colour(125, 125, 125));

    for (int i = 0; i < numTics; ++i)
    {
        float alpha = juce::jmap<float>(i, 0, numTics - 1, startAngleDeg, endAngleDeg);
        float angleRad = juce::degreesToRadians(alpha);

        float x1 = centerX + innerRadius * std::cos(angleRad);
        float y1 = centerY + innerRadius * std::sin(angleRad);
        float x2 = centerX + outerRadius * std::cos(angleRad);
        float y2 = centerY + outerRadius * std::sin(angleRad);

        g.drawLine(x1, y1, x2, y2, 2.0f);
    }
}

void FmEngineAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    if (sandwichIconBounds.contains(e.getPosition()))
    {
        controlPanelVisible = !controlPanelVisible;

        for (auto* comp : guiComponents)
            comp->setVisible(!controlPanelVisible);

        repaint();
        return;
    }

    if (controlPanelVisible && !sandwichIconBounds.contains(e.getPosition()))
    {
        controlPanelVisible = false;
        for (auto* comp : guiComponents)
            comp->setVisible(true);
        repaint();
        return;
    }
}

void FmEngineAudioProcessorEditor::resized()
{
    // Position knobs (100x100 each, centered in their areas)
    modDepthSlider.setBounds(45, 40, 100, 100);
    maxDelaySlider.setBounds(195, 40, 100, 100);
    
    slideSwitch.setBounds(138, 216, 60, 20);
    lpfSlider.setBounds(20, 499, 300, 20);

    swapToggle.setBounds(280, 419, 40, 20);
    swapLabel.setBounds(175, 419, 100, 20);

    predelayToggle.setBounds(280, 459, 40, 20);
    predelayLabel.setBounds(175, 459, 100, 20);

    limiterToggle.setBounds(20, 419, 40, 20);
    limiterLabel.setBounds(65, 419, 145, 20);

    oversamplingToggle.setBounds(20, 459, 40, 20);
    oversamplingLabel.setBounds(65, 459, 145, 20);
}