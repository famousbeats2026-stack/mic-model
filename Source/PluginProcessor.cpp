#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
FamousBeatsMicModelProcessor::FamousBeatsMicModelProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("FamousBeatsMicModel"),
                 {
                     std::make_unique<juce::AudioParameterBool>(
                         kBypassParam, "Bypass", false),
                     std::make_unique<juce::AudioParameterFloat>(
                         kModelAmountParam, "Model Amount",
                         juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f,
                         juce::AudioParameterFloatAttributes().withLabel("%")),
                     std::make_unique<juce::AudioParameterFloat>(
                         kWarmthParam, "Warmth",
                         juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
                         juce::AudioParameterFloatAttributes().withLabel("%")),
                     std::make_unique<juce::AudioParameterFloat>(
                         kAirParam, "Air",
                         juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f,
                         juce::AudioParameterFloatAttributes().withLabel("%")),
                     std::make_unique<juce::AudioParameterFloat>(
                         kTubeDriveParam, "Tube Drive",
                         juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 30.0f,
                         juce::AudioParameterFloatAttributes().withLabel("%")),
                     std::make_unique<juce::AudioParameterFloat>(
                         kOutputGainParam, "Output",
                         juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f,
                         juce::AudioParameterFloatAttributes().withLabel("dB")),
                 })
{
    bypassParam      = parameters.getRawParameterValue(kBypassParam);
    modelAmountParam = parameters.getRawParameterValue(kModelAmountParam);
    warmthParam      = parameters.getRawParameterValue(kWarmthParam);
    airParam         = parameters.getRawParameterValue(kAirParam);
    tubeDriveParam   = parameters.getRawParameterValue(kTubeDriveParam);
    outputGainParam  = parameters.getRawParameterValue(kOutputGainParam);

    // Start the smoothers at the default parameter values.
    warmthDbS.reset(currentSampleRate, 0.05);
    airDbS.reset(currentSampleRate, 0.05);
    amountS.reset(currentSampleRate, 0.05);
    driveS.reset(currentSampleRate, 0.05);
    outputGainS.reset(currentSampleRate, 0.05);
    warmthDbS.setCurrentAndTargetValue(0.5f * kWarmthMaxDb);
    airDbS.setCurrentAndTargetValue(0.5f * kAirMaxDb);
    amountS.setCurrentAndTargetValue(1.0f);
    driveS.setCurrentAndTargetValue(1.0f + 0.30f * 2.5f);
    outputGainS.setCurrentAndTargetValue(1.0f);
}

FamousBeatsMicModelProcessor::~FamousBeatsMicModelProcessor() = default;

//==============================================================================
void FamousBeatsMicModelProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    const int numChannels = getTotalNumInputChannels();
    modelChannels.clear();
    for (int ch = 0; ch < numChannels; ++ch)
        modelChannels.push_back(std::make_unique<ModelChannel>());

    // Smoother ramp times (50 ms) and re-seed at the live values so a
    // sample-rate change does not jump the gains.
    warmthDbS.reset(sampleRate, 0.05);
    airDbS.reset(sampleRate, 0.05);
    amountS.reset(sampleRate, 0.05);
    driveS.reset(sampleRate, 0.05);
    outputGainS.reset(sampleRate, 0.05);
    warmthDbS.setCurrentAndTargetValue(warmthParam->load() * 0.01f * kWarmthMaxDb);
    airDbS.setCurrentAndTargetValue(airParam->load() * 0.01f * kAirMaxDb);
    amountS.setCurrentAndTargetValue(modelAmountParam->load() * 0.01f);
    driveS.setCurrentAndTargetValue(1.0f + tubeDriveParam->load() * 0.01f * 2.5f);
    outputGainS.setCurrentAndTargetValue(
        std::pow(10.0f, outputGainParam->load() / 20.0f));

    dryBuffer.setSize(numChannels, samplesPerBlock);

    // Force a coefficient rebuild on the first block.
    lastWarmthDb = -999.0f;
    lastAirDb = -999.0f;
    updateCoefficients(warmthDbS.getCurrentValue(), airDbS.getCurrentValue());
}

void FamousBeatsMicModelProcessor::releaseResources()
{
    modelChannels.clear();
}

//==============================================================================
void FamousBeatsMicModelProcessor::updateCoefficients(float warmthDb, float airDb)
{
    if (std::abs(warmthDb - lastWarmthDb) < 0.005f
        && std::abs(airDb - lastAirDb) < 0.005f
        && presenceCoeffs != nullptr)
        return;

    lastWarmthDb = warmthDb;
    lastAirDb = airDb;

    // NOTE: JUCE's makePeakFilter/makeHighShelf take a LINEAR gain
    // factor, not dB — gains must go through decibelsToGain (passing
    // raw dB, e.g. -2.5, makes sqrt() of a negative number and zeroes
    // the signal).
    presenceCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        currentSampleRate, kPresenceFreqHz, kPresenceQ,
        juce::Decibels::decibelsToGain(kPresenceGainDb));
    warmthCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        currentSampleRate, kWarmthFreqHz, kWarmthQ,
        juce::Decibels::decibelsToGain(warmthDb));
    airCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        currentSampleRate, kAirFreqHz, 0.707f,
        juce::Decibels::decibelsToGain(airDb));
    rolloffCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(
        currentSampleRate, kRolloffFreqHz, kRolloffQ);

    for (auto& ch : modelChannels)
    {
        ch->presence.coefficients = presenceCoeffs;
        ch->warmth.coefficients = warmthCoeffs;
        ch->air.coefficients = airCoeffs;
        ch->rolloff.coefficients = rolloffCoeffs;
    }
}

//==============================================================================
float FamousBeatsMicModelProcessor::getModelResponseDb(float freqHz) const
{
    if (presenceCoeffs == nullptr || warmthCoeffs == nullptr
        || airCoeffs == nullptr || rolloffCoeffs == nullptr)
        return 0.0f;

    const double f = juce::jlimit(20.0, 20000.0, (double)freqHz);
    double mag = 1.0;
    mag *= presenceCoeffs->getMagnitudeForFrequency(f, currentSampleRate);
    mag *= warmthCoeffs->getMagnitudeForFrequency(f, currentSampleRate);
    mag *= airCoeffs->getMagnitudeForFrequency(f, currentSampleRate);
    mag *= rolloffCoeffs->getMagnitudeForFrequency(f, currentSampleRate);
    return (float)(20.0 * std::log10(mag + 1e-12));
}

void FamousBeatsMicModelProcessor::getModelResponseCurve(const float* freqs, float* dbs,
                                                         int numPoints) const
{
    for (int i = 0; i < numPoints; ++i)
        dbs[i] = getModelResponseDb(freqs[i]);
}

//==============================================================================
void FamousBeatsMicModelProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Hard bypass: the buffer already holds the dry input, so just
    // leave it alone.
    if (bypassParam->load() > 0.5f)
        return;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if ((int)modelChannels.size() != numChannels)
    {
        // Defensive: prepareToPlay should have sized this, but never
        // process with a mismatched channel count.
        modelChannels.clear();
        for (int ch = 0; ch < numChannels; ++ch)
            modelChannels.push_back(std::make_unique<ModelChannel>());
        lastWarmthDb = -999.0f;
        lastAirDb = -999.0f;
        presenceCoeffs = nullptr; // force rebuild + assignment below
    }

    // Per-block smoothed targets (coefficients update once per block).
    warmthDbS.setTargetValue(warmthParam->load() * 0.01f * kWarmthMaxDb);
    airDbS.setTargetValue(airParam->load() * 0.01f * kAirMaxDb);
    amountS.setTargetValue(modelAmountParam->load() * 0.01f);
    driveS.setTargetValue(1.0f + tubeDriveParam->load() * 0.01f * 2.5f);
    outputGainS.setTargetValue(std::pow(10.0f, outputGainParam->load() / 20.0f));

    const float warmthDb = warmthDbS.getNextValue();
    const float airDb = airDbS.getNextValue();
    updateCoefficients(warmthDb, airDb);
    const float amount = amountS.getNextValue();
    const float drive = driveS.getNextValue();
    const float outGain = outputGainS.getNextValue();

    // Keep the dry copy for the wet/dry mix.
    dryBuffer.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // ---- Model EQ (per channel) ----
    juce::dsp::AudioBlock<float> block(buffer);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& mc = *modelChannels[(size_t)ch];
        auto channelBlock = block.getSingleChannelBlock(ch);
        juce::dsp::ProcessContextReplacing<float> ctx(channelBlock);
        mc.presence.process(ctx);
        mc.warmth.process(ctx);
        mc.air.process(ctx);
        mc.rolloff.process(ctx);
    }

    // ---- Tube stage: asymmetric tanh saturator (6AU6-style warmth) ----
    // Normalised so small signals stay at unity gain: tanh(g*x)/g is
    // ~x for small x, and compresses the peaks as g grows. The negative
    // half uses 0.85x the gain, which generates the even harmonics a
    // real tube stage adds.
    const float gPos = drive;
    const float gNeg = drive * 0.85f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = data[i];
            data[i] = x >= 0.0f ? std::tanh(gPos * x) / gPos
                                : std::tanh(gNeg * x) / gNeg;
        }
    }

    // ---- Wet/dry mix + output gain ----
    const float wet = amount;
    const float dry = 1.0f - amount;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        const auto* dryData = dryBuffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = (data[i] * wet + dryData[i] * dry) * outGain;
    }
}

//==============================================================================
juce::AudioProcessorEditor* FamousBeatsMicModelProcessor::createEditor()
{
    return new PluginEditor(*this);
}

//==============================================================================
// JUCE 8 CMake builds do not generate this for you: the plugin client
// (VST3/AU wrappers) calls createPluginFilter() to build the processor.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FamousBeatsMicModelProcessor();
}

//==============================================================================
void FamousBeatsMicModelProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void FamousBeatsMicModelProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}
