#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class BlueLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
        auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto centre = bounds.getCentre();
        auto angle  = startAngle + sliderPos * (endAngle - startAngle);
        const float lineW = radius * 0.18f;
        const float arcR  = radius - lineW * 0.5f;

        juce::Path bg;
        bg.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xffAACAE8));
        g.strokePath (bg, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path val;
        val.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (juce::Colour (0xff3A7CB8));
        g.strokePath (val, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto kr = radius * 0.62f;
        g.setColour (juce::Colour (0xff6CA3D6));
        g.fillEllipse (centre.x - kr, centre.y - kr, kr * 2.0f, kr * 2.0f);
        g.setColour (juce::Colour (0xff2E5F8C));
        g.drawEllipse (centre.x - kr, centre.y - kr, kr * 2.0f, kr * 2.0f, 1.5f);

        juce::Path pointer;
        pointer.addRoundedRectangle (-1.5f, -kr, 3.0f, kr * 0.9f, 1.5f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
        g.setColour (juce::Colour (0xff1B3A5C));
        g.fillPath (pointer);
    }
};

//==============================================================================
class SpectrumDisplay : public juce::Component, private juce::Timer
{
public:
    SpectrumDisplay (VocoduhAudioProcessor& p) : proc (p) { startTimerHz (30); }
    ~SpectrumDisplay() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto full = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xffF2F8FE));
        g.fillRoundedRectangle (full, 14.0f);
        g.setColour (juce::Colour (0xffCBE0F5));
        g.drawRoundedRectangle (full.reduced (0.75f), 14.0f, 1.5f);

        std::array<float, VocoduhAudioProcessor::maxBands> levels {};
        proc.getBandLevels (levels.data());
        const int n = proc.getActiveBands();
        if (n <= 0) return;

        auto plot = full.reduced (12.0f);
        const float gap    = 3.0f;
        const float barW   = (plot.getWidth() - gap * (n - 1)) / (float) n;
        const float corner = juce::jmin (barW * 0.5f, 4.0f);

        for (int i = 0; i < n; ++i)
        {
            const float level = juce::jlimit (0.0f, 1.0f, std::sqrt (levels[(size_t) i] * displayScale));
            peaks[(size_t) i] = juce::jmax (peaks[(size_t) i] * 0.90f, level);

            const float barH = level * plot.getHeight();
            const float x    = plot.getX() + i * (barW + gap);
            const float yTop = plot.getBottom() - barH;

            juce::ColourGradient grad (juce::Colour (0xff8FC4F2), x, yTop,
                                       juce::Colour (0xff3D7FBE), x, plot.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (x, yTop, barW, barH, corner);

            const float peakY = plot.getBottom() - peaks[(size_t) i] * plot.getHeight();
            g.setColour (juce::Colour (0x55FFA6CE));
            g.fillRoundedRectangle (x - 1.5f, peakY - 3.0f, barW + 3.0f, 6.0f, 3.0f);
            g.setColour (juce::Colour (0xffFFB6D5));
            g.fillRoundedRectangle (x, peakY - 1.5f, barW, 3.0f, 1.5f);
        }
    }

private:
    void timerCallback() override { repaint(); }

    VocoduhAudioProcessor& proc;
    std::array<float, VocoduhAudioProcessor::maxBands> peaks {};
    static constexpr float displayScale = 3.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumDisplay)
};

//==============================================================================
class VocoduhAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    VocoduhAudioProcessorEditor (VocoduhAudioProcessor&);
    ~VocoduhAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    VocoduhAudioProcessor& audioProcessor;
    BlueLookAndFeel blueLnf;
    SpectrumDisplay spectrum { audioProcessor };

    juce::Slider gainSlider, attackSlider, releaseSlider, unvoicedSlider, enhanceSlider;
    juce::Label  gainLabel,  attackLabel,  releaseLabel,  unvoicedLabel,  enhanceLabel;
    juce::ComboBox bandsBox;
    juce::Label    bandsLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SliderAttachment> gainAttachment, attackAttachment, releaseAttachment,
                                      unvoicedAttachment, enhanceAttachment;
    std::unique_ptr<ComboAttachment>  bandsAttachment;

    void setupKnob (juce::Slider&, juce::Label&, const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocoduhAudioProcessorEditor)
};
