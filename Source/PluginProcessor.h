#pragma once

// Explicit module includes (rather than the generated JuceHeader.h) so
// the test harness can build the processor without the plugin client's
// generated headers.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <atomic>
#include <cmath>

//==============================================================================
// Famous Beats Mic Model — makes a Neumann TLM 103 sound like a Sony
// C-800G tube condenser, from the two mics' PUBLISHED response curves.
//
//   Source (Neumann TLM 103, K 103 capsule, from Neumann's own spec):
//     flat up to ~5 kHz, then a wide flat +4 dB presence boost to 20 kHz.
//   Target (Sony C-800G, 6AU6 tube, K67-type capsule, from Sony's spec):
//     20 Hz-18 kHz; gentle high-end emphasis 1-10 kHz; warmth in the
//     lower mids from the tube + transformer; smooth top end that rolls
//     off above ~16 kHz.
//
// Difference EQ (the "model", per channel, fixed core + two user knobs):
//   1. Presence tamer: peaking cut -2.5 dB at 5.5 kHz, Q 0.7 — takes the
//      edge off the TLM 103's broad presence shelf.
//   2. Warmth: peaking boost at 250 Hz, Q 0.8 — 0 to +3 dB (Warmth knob).
//   3. Air: high shelf at 12 kHz — 0 to +4 dB (Air knob).
//   4. HF rolloff: 2nd-order lowpass at 16 kHz, Q 0.707 — the C-800G's
//      response ends at 18 kHz, so the extreme top gently falls away.
//
// Tube stage: an asymmetric tanh saturator emulating the 6AU6 tube's
// even-harmonic warmth. Drive 0-100% maps to a pre-gain of 1.0-3.5 with
// normalisation, so small signals stay at unity gain and only the peaks
// get the tube treatment. The negative half uses a slightly lower gain
// (0.85x) so even harmonics are generated, like a real tube stage.
//
// Signal flow: input -> model EQ -> tube -> wet/dry mix (Model Amount)
// -> output gain. Bypass passes the input through untouched. Zero
// latency (all IIR + waveshaper, no lookahead).
//
// Honest limits (also in README.md): this is a CHARACTER model built
// from the manufacturers' published curves, not a measurement of Corey's
// actual TLM 103 in his room, and not a clone of Sony's microphone.
// How close it sounds depends on his voice, mic distance, and room.
class FamousBeatsMicModelProcessor : public juce::AudioProcessor
{
public:
    FamousBeatsMicModelProcessor();
    ~FamousBeatsMicModelProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Famous Beats Mic Model"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParametersState() { return parameters; }

    // ---- Model curve (for the editor's display and the tests) ----
    // Combined magnitude response of the four model EQ bands in dB at a
    // frequency, from the current filter coefficients. Does NOT include
    // the tube stage (level-dependent) or the wet/dry mix.
    float getModelResponseDb(float freqHz) const;
    // Same, for many frequencies at once (coefficients read once).
    void getModelResponseCurve(const float* freqs, float* dbs, int numPoints) const;

    // ---- Parameter IDs ----
    static constexpr const char* kBypassParam      = "bypass";
    static constexpr const char* kModelAmountParam = "modelAmount"; // 0..100 %, default 100
    static constexpr const char* kWarmthParam      = "warmth";      // 0..100 %, default 50
    static constexpr const char* kAirParam         = "air";         // 0..100 %, default 50
    static constexpr const char* kTubeDriveParam   = "tubeDrive";   // 0..100 %, default 30
    static constexpr const char* kOutputGainParam  = "outputGain";  // dB, -12..+12

    // ---- Model constants (the difference curve, from the published
    // response curves — see the class comment above) ----
    static constexpr float kPresenceFreqHz = 5500.0f;
    static constexpr float kPresenceGainDb = -2.5f;
    static constexpr float kPresenceQ      = 0.7f;
    static constexpr float kWarmthFreqHz   = 250.0f;
    static constexpr float kWarmthQ        = 0.8f;
    static constexpr float kWarmthMaxDb    = 3.0f;   // at Warmth = 100 %
    static constexpr float kAirFreqHz      = 12000.0f;
    static constexpr float kAirMaxDb       = 4.0f;   // at Air = 100 %
    static constexpr float kRolloffFreqHz  = 16000.0f;
    static constexpr float kRolloffQ       = 0.707f;

private:
    // Recompute the four bands' coefficients from the current (smoothed)
    // Warmth / Air values. Called once per block, and on prepareToPlay.
    void updateCoefficients(float warmthDb, float airDb);

    juce::AudioProcessorValueTreeState parameters;

    // Cached parameter pointers.
    std::atomic<float>* bypassParam = nullptr;
    std::atomic<float>* modelAmountParam = nullptr;
    std::atomic<float>* warmthParam = nullptr;
    std::atomic<float>* airParam = nullptr;
    std::atomic<float>* tubeDriveParam = nullptr;
    std::atomic<float>* outputGainParam = nullptr;

    // Model EQ: one IIR filter per band, per channel. IIR::Filter is
    // neither copyable nor movable (it owns a HeapBlock), so channels
    // are held by unique_ptr.
    struct ModelChannel
    {
        juce::dsp::IIR::Filter<float> presence, warmth, air, rolloff;
    };
    std::vector<std::unique_ptr<ModelChannel>> modelChannels;

    // The shared coefficients (same for every channel) — the response
    // curve API reads these, so it stays true even mid-block.
    juce::dsp::IIR::Coefficients<float>::Ptr presenceCoeffs;
    juce::dsp::IIR::Coefficients<float>::Ptr warmthCoeffs;
    juce::dsp::IIR::Coefficients<float>::Ptr airCoeffs;
    juce::dsp::IIR::Coefficients<float>::Ptr rolloffCoeffs;

    double currentSampleRate = 48000.0;
    float lastWarmthDb = -999.0f; // forces a coefficient update on first block
    float lastAirDb = -999.0f;

    // Smoothed per-block scalars (no zipper noise on knob moves).
    juce::SmoothedValue<float> warmthDbS, airDbS, amountS, driveS, outputGainS;

    juce::AudioBuffer<float> dryBuffer; // reused for the wet/dry mix

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FamousBeatsMicModelProcessor)
};
