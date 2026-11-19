#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// Gold, shiny LookAndFeel: metallic gold gradients, gloss highlights,
// black text on gold, white text on black. (Same style as the other
// Famous Beats Studio plugins.)
class GoldLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GoldLookAndFeel();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override;
    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override
    {
        const auto& props = button.getProperties();
        const float size = props.contains("fontSize")
            ? (float)(double)props["fontSize"]
            : juce::jmin(14.0f, buttonHeight * 0.4f);
        return juce::Font(juce::FontOptions(size, juce::Font::bold));
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isMouseOverButton, bool isButtonDown) override;

    static juce::ColourGradient goldGradient(float x0, float y0, float x1, float y1);
    static juce::ColourGradient goldRadial(float cx, float cy, float radius);

    static const juce::Colour gold;
    static const juce::Colour goldLight;
    static const juce::Colour goldDark;
    static const juce::Colour black;
    static const juce::Colour panel;
    static const juce::Colour cream;
};

//==============================================================================
// Live model-curve display: draws the processor's actual difference-EQ
// response (TLM 103 -> C-800G) on a log-frequency / dB grid. Not
// draggable — the Warmth and Air knobs move the curve.
class ModelCurveComponent : public juce::Component, private juce::Timer
{
public:
    explicit ModelCurveComponent(FamousBeatsMicModelProcessor& p);
    ~ModelCurveComponent() override = default;

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override { repaint(); }

    float freqToX(float freq) const;
    float dbToY(float db) const;

    FamousBeatsMicModelProcessor& processor;

    static constexpr int kNumPoints = 160;
    static constexpr float kMinDb = -6.0f;
    static constexpr float kMaxDb = 6.0f;
};

//==============================================================================
// Black-and-gold editor: title, live model curve, five knobs
// (Model Amount / Warmth / Air / Tube Drive / Output) and Bypass.
class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(FamousBeatsMicModelProcessor&);
    ~PluginEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void makeKnob(juce::Slider& knob, juce::Label& label, const juce::String& text,
                  const char* paramId);

    FamousBeatsMicModelProcessor& processor;
    GoldLookAndFeel goldLaf;

    juce::Label titleLabel;
    juce::Label brandLabel;
    juce::Label fromToLabel;
    juce::Label noteLabel;

    juce::TextButton bypassButton;

    ModelCurveComponent curve;

    juce::Slider amountKnob; juce::Label amountLabel;
    juce::Slider warmthKnob; juce::Label warmthLabel;
    juce::Slider airKnob;    juce::Label airLabel;
    juce::Slider driveKnob;  juce::Label driveLabel;
    juce::Slider outputKnob; juce::Label outputLabel;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
