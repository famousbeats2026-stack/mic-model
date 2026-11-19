#include "PluginEditor.h"

//==============================================================================
// GoldLookAndFeel
const juce::Colour GoldLookAndFeel::gold(0xffd4af37);
const juce::Colour GoldLookAndFeel::goldLight(0xfff6d47c);
const juce::Colour GoldLookAndFeel::goldDark(0xff9a7b1e);
const juce::Colour GoldLookAndFeel::black(0xff0a0a0a);
const juce::Colour GoldLookAndFeel::panel(0xff141414);
const juce::Colour GoldLookAndFeel::cream(0xfff5e6c8);

GoldLookAndFeel::GoldLookAndFeel()
{
    setColour(juce::Slider::rotarySliderFillColourId, gold);
    setColour(juce::Slider::rotarySliderOutlineColourId, goldDark);
    setColour(juce::Slider::thumbColourId, gold);
    setColour(juce::Slider::textBoxTextColourId, cream);
    setColour(juce::Slider::textBoxBackgroundColourId, panel);
    setColour(juce::Slider::textBoxOutlineColourId, goldDark);
    setColour(juce::TextButton::buttonColourId, panel);
    setColour(juce::TextButton::buttonOnColourId, gold);
    setColour(juce::TextButton::textColourOffId, gold);
    setColour(juce::TextButton::textColourOnId, black);
}

juce::ColourGradient GoldLookAndFeel::goldGradient(float x0, float y0, float x1, float y1)
{
    juce::ColourGradient g(goldLight, x0, y0, goldDark, x1, y1, false);
    g.addColour(0.5, gold);
    return g;
}

juce::ColourGradient GoldLookAndFeel::goldRadial(float cx, float cy, float radius)
{
    juce::ColourGradient g(goldLight, cx - radius * 0.4f, cy - radius * 0.5f,
                           goldDark, cx + radius * 0.5f, cy + radius * 0.6f, true);
    g.addColour(0.55, gold);
    return g;
}

void GoldLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float rotaryStartAngle,
                                       float rotaryEndAngle, juce::Slider&)
{
    const float radius = juce::jmin(width / 2.0f, height / 2.0f) - 4.0f;
    const float centreX = x + width * 0.5f;
    const float centreY = y + height * 0.5f;
    const float rx = centreX - radius;
    const float ry = centreY - radius;
    const float rw = radius * 2.0f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Background arc.
    juce::Path bgArc;
    bgArc.addArc(rx, ry, rw, rw, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(goldDark.withAlpha(0.4f));
    g.strokePath(bgArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));

    // Gold value arc.
    juce::Path valueArc;
    valueArc.addArc(rx, ry, rw, rw, rotaryStartAngle, angle, true);
    g.setColour(gold);
    g.strokePath(valueArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Metallic knob body.
    const float bodyR = radius - 7.0f;
    g.setGradientFill(goldRadial(centreX, centreY, bodyR));
    g.fillEllipse(centreX - bodyR, centreY - bodyR, bodyR * 2.0f, bodyR * 2.0f);

    // Gloss highlight.
    g.setColour(juce::Colours::white.withAlpha(0.38f));
    g.fillEllipse(centreX - bodyR * 0.55f, centreY - bodyR * 0.78f,
                  bodyR * 0.7f, bodyR * 0.45f);

    // Pointer line.
    juce::Path pointer;
    const float pointerLength = bodyR * 0.8f;
    pointer.addRectangle(-1.5f, -bodyR, 3.0f, pointerLength);
    g.setColour(black);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centreX, centreY));

    // Rim.
    g.setColour(goldDark);
    g.drawEllipse(centreX - bodyR, centreY - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);
}

void GoldLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                           const juce::Colour& /*backgroundColour*/,
                                           bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    const float corner = 6.0f;
    const bool isOn = button.getToggleState();

    if (isOn)
    {
        g.setGradientFill(goldGradient(bounds.getX(), bounds.getY(),
                                       bounds.getX(), bounds.getBottom()));
        g.fillRoundedRectangle(bounds, corner);
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.fillRoundedRectangle(bounds.getX(), bounds.getY(),
                               bounds.getWidth(), bounds.getHeight() * 0.45f, corner);
    }
    else
    {
        juce::ColourGradient grad(juce::Colour(0xff1c1a14), bounds.getX(), bounds.getY(),
                                  black, bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bounds, corner);
        g.setColour(isMouseOverButton ? gold : goldDark);
        g.drawRoundedRectangle(bounds, corner, isMouseOverButton ? 2.0f : 1.0f);
    }

    if (isButtonDown)
    {
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.fillRoundedRectangle(bounds, corner);
    }
}

//==============================================================================
// ModelCurveComponent
ModelCurveComponent::ModelCurveComponent(FamousBeatsMicModelProcessor& p)
    : processor(p)
{
    startTimerHz(15); // live curve: knob moves and automation show up
}

float ModelCurveComponent::freqToX(float freq) const
{
    const double l = std::log10(juce::jlimit(20.0f, 20000.0f, freq));
    const double l0 = std::log10(20.0), l1 = std::log10(20000.0);
    const double t = (l - l0) / (l1 - l0);
    return 34.0f + (float)t * ((float)getWidth() - 44.0f);
}

float ModelCurveComponent::dbToY(float db) const
{
    const float t = juce::jlimit(kMinDb, kMaxDb, db);
    return 8.0f + (1.0f - (t - kMinDb) / (kMaxDb - kMinDb)) * ((float)getHeight() - 28.0f);
}

void ModelCurveComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    juce::ColourGradient bg(juce::Colour(0xff171310), area.getX(), area.getY(),
                            juce::Colour(0xff050505), area.getX(), area.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(juce::Colour(0xff6b5a26));
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    const float left = 34.0f, right = (float)getWidth() - 10.0f;
    const float top = 8.0f, bottom = (float)getHeight() - 20.0f;

    // Grid: log frequency lines + dB lines.
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    const float freqMarks[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    for (float f : freqMarks)
    {
        const float x = freqToX(f);
        g.setColour(juce::Colour(0xff2a2517));
        g.drawVerticalLine((int)x, top, bottom);
        g.setColour(GoldLookAndFeel::cream.withAlpha(0.55f));
        juce::String label = f >= 1000 ? juce::String(f / 1000.0f, 0) + "k" : juce::String((int)f);
        g.drawText(label, (int)x - 14, (int)bottom + 2, 28, 12, juce::Justification::centred);
    }
    const float dbMarks[] = { -6, -3, 0, 3, 6 };
    for (float dbv : dbMarks)
    {
        const float y = dbToY(dbv);
        g.setColour(std::abs(dbv) < 0.01f ? GoldLookAndFeel::gold.withAlpha(0.7f)
                                : juce::Colour(0xff2a2517));
        g.drawHorizontalLine((int)y, left, right);
        g.setColour(GoldLookAndFeel::cream.withAlpha(0.55f));
        g.drawText((dbv > 0 ? "+" : "") + juce::String((int)dbv), 2, (int)y - 6, 28, 12,
                   juce::Justification::centredLeft);
    }

    // Response curve (computed from the real filter coefficients).
    float freqs[kNumPoints], dbs[kNumPoints];
    for (int i = 0; i < kNumPoints; ++i)
        freqs[i] = 20.0f * (float)std::pow(1000.0, (double)i / (kNumPoints - 1));
    processor.getModelResponseCurve(freqs, dbs, kNumPoints);

    auto point = [&](int i) -> juce::Point<float>
    {
        return { freqToX(freqs[i]), dbToY(dbs[i]) };
    };

    const float zeroY = dbToY(0.0f);
    juce::Path fillPath;
    fillPath.startNewSubPath(point(0));
    for (int i = 1; i < kNumPoints; ++i)
        fillPath.lineTo(point(i));
    fillPath.lineTo(point(kNumPoints - 1).x, zeroY);
    fillPath.lineTo(point(0).x, zeroY);
    fillPath.closeSubPath();
    g.setColour(GoldLookAndFeel::gold.withAlpha(0.14f));
    g.fillPath(fillPath);

    juce::Path curvePath;
    curvePath.startNewSubPath(point(0));
    for (int i = 1; i < kNumPoints; ++i)
        curvePath.lineTo(point(i));
    g.setColour(GoldLookAndFeel::gold.withAlpha(0.35f));
    g.strokePath(curvePath, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
    g.setColour(GoldLookAndFeel::goldLight);
    g.strokePath(curvePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    // Header.
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.setColour(GoldLookAndFeel::gold);
    g.drawText("MODEL CURVE  —  WHAT THE PLUGIN DOES TO YOUR TLM 103",
               (int)left, (int)top + 2, (int)(right - left), 14,
               juce::Justification::centredLeft);
}

//==============================================================================
// PluginEditor
PluginEditor::PluginEditor(FamousBeatsMicModelProcessor& p)
    : AudioProcessorEditor(&p), processor(p), curve(p)
{
    setLookAndFeel(&goldLaf);
    setSize(720, 470);

    titleLabel.setText("FAMOUS BEATS MIC MODEL", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(28.0f, juce::Font::bold)));
    titleLabel.setColour(juce::Label::textColourId, GoldLookAndFeel::gold);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    brandLabel.setText("FAMOUS BEATS STUDIO  •  TUBE CONDENSER CHARACTER MODEL",
                       juce::dontSendNotification);
    brandLabel.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    brandLabel.setColour(juce::Label::textColourId, GoldLookAndFeel::cream);
    brandLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(brandLabel);

    fromToLabel.setText("FROM:  NEUMANN TLM 103   →   TO:  SONY C-800G",
                        juce::dontSendNotification);
    fromToLabel.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    fromToLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    fromToLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(fromToLabel);

    addAndMakeVisible(curve);

    makeKnob(amountKnob, amountLabel, "MODEL AMOUNT",
             FamousBeatsMicModelProcessor::kModelAmountParam);
    makeKnob(warmthKnob, warmthLabel, "WARMTH",
             FamousBeatsMicModelProcessor::kWarmthParam);
    makeKnob(airKnob, airLabel, "AIR",
             FamousBeatsMicModelProcessor::kAirParam);
    makeKnob(driveKnob, driveLabel, "TUBE DRIVE",
             FamousBeatsMicModelProcessor::kTubeDriveParam);
    makeKnob(outputKnob, outputLabel, "OUTPUT",
             FamousBeatsMicModelProcessor::kOutputGainParam);

    bypassButton.setButtonText("BYPASS");
    bypassButton.setClickingTogglesState(true);
    bypassButton.getProperties().set("fontSize", 14.0f);
    addAndMakeVisible(bypassButton);

    noteLabel.setText("Character model from the published response curves — not a "
                      "measurement of your mic, and not a clone of Sony's mic.",
                      juce::dontSendNotification);
    noteLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
    noteLabel.setColour(juce::Label::textColourId, GoldLookAndFeel::cream.withAlpha(0.7f));
    noteLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(noteLabel);

    buttonAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.getParametersState(), FamousBeatsMicModelProcessor::kBypassParam, bypassButton));
}

void PluginEditor::makeKnob(juce::Slider& knob, juce::Label& label, const juce::String& text,
                            const char* paramId)
{
    knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
    knob.setColour(juce::Slider::rotarySliderFillColourId, GoldLookAndFeel::gold);
    knob.setColour(juce::Slider::rotarySliderOutlineColourId, GoldLookAndFeel::goldDark);
    knob.setColour(juce::Slider::thumbColourId, GoldLookAndFeel::gold);
    knob.setColour(juce::Slider::textBoxTextColourId, GoldLookAndFeel::cream);
    knob.setColour(juce::Slider::textBoxBackgroundColourId, GoldLookAndFeel::panel);
    addAndMakeVisible(knob);

    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);

    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParametersState(), paramId, knob));
}

void PluginEditor::paint(juce::Graphics& g)
{
    // Shiny black background: vertical gradient with a gold top edge.
    juce::ColourGradient bg(juce::Colour(0xff171208), 0.0f, 0.0f,
                            juce::Colour(0xff050505), 0.0f, (float)getHeight(), false);
    g.setGradientFill(bg);
    g.fillAll();
    g.setColour(GoldLookAndFeel::gold);
    g.fillRect(0, 0, getWidth(), 2);
    g.setColour(GoldLookAndFeel::goldDark);
    g.drawRect(getLocalBounds(), 2);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(12);
    titleLabel.setBounds(area.removeFromTop(34));
    brandLabel.setBounds(area.removeFromTop(16));
    fromToLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(4);
    curve.setBounds(area.removeFromTop(168));
    area.removeFromTop(10);

    // Knob row: five knobs + bypass button on the right.
    auto knobRow = area.removeFromTop(110);
    const int knobW = (knobRow.getWidth() - 100) / 5;
    auto placeKnob = [&](juce::Slider& k, juce::Label& l)
    {
        auto cell = knobRow.removeFromLeft(knobW);
        k.setBounds(cell.removeFromTop(84).withSizeKeepingCentre(84, 84));
        l.setBounds(cell.removeFromTop(18));
    };
    placeKnob(amountKnob, amountLabel);
    placeKnob(warmthKnob, warmthLabel);
    placeKnob(airKnob, airLabel);
    placeKnob(driveKnob, driveLabel);
    placeKnob(outputKnob, outputLabel);
    bypassButton.setBounds(knobRow.removeFromLeft(100).reduced(6, 30));

    area.removeFromTop(6);
    noteLabel.setBounds(area.removeFromTop(16));
}
