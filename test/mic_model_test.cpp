// Test harness: feed generated audio through the Famous Beats Mic Model
// (TLM 103 -> C-800G difference EQ + 6AU6-style tube stage) and report
// measured results.
//
//   1. Instantiation + prepareToPlay (no crash)
//   2. Silence in -> silence out (no output, no NaN)
//   3. White noise at default settings (no clipping, no NaN)
//   4. Difference curve: sine gain at 250 / 1000 / 5500 / 12000 /
//      17000 Hz must follow the processor's own analytic response
//      within 0.7 dB, and the analytic curve must have the intended
//      shape (warmth up, presence dip, air recovery, HF rolloff)
//   5. Warmth and Air knobs move their bands (0 vs 100%)
//   6. Tube drive: 2nd harmonic appears at drive 100, peak stays
//      <= 1.0, no NaN, output differs from drive 0
//   7. Bypass: output == input
//   8. Model Amount 0: output == input
//
// Usage: mic_model_test
// Exit code 0 = all tests passed, 1 = any test failed.

#include "../Source/PluginProcessor.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <functional>
#include <random>

namespace
{
    constexpr double kSr = 48000.0; // Corey's rate (Ableton on his Mac)
    constexpr int kBlock = 512;

    int gFailures = 0;
    int gChecks = 0;

    void check(bool ok, const char* what)
    {
        ++gChecks;
        if (!ok)
        {
            ++gFailures;
            std::printf("  FAIL: %s\n", what);
        }
    }

    double db(double rms) { return 20.0 * std::log10(rms + 1e-12); }

    void setParam(FamousBeatsMicModelProcessor& p, const char* id, float value)
    {
        auto* param = p.getParametersState().getParameter(id);
        param->setValueNotifyingHost(param->convertTo0to1(value));
    }

    void setDefaults(FamousBeatsMicModelProcessor& p)
    {
        setParam(p, FamousBeatsMicModelProcessor::kBypassParam, 0.0f);
        setParam(p, FamousBeatsMicModelProcessor::kModelAmountParam, 100.0f);
        setParam(p, FamousBeatsMicModelProcessor::kWarmthParam, 50.0f);
        setParam(p, FamousBeatsMicModelProcessor::kAirParam, 50.0f);
        setParam(p, FamousBeatsMicModelProcessor::kTubeDriveParam, 0.0f);
        setParam(p, FamousBeatsMicModelProcessor::kOutputGainParam, 0.0f);
    }

    // Render `seconds` of audio through the processor in 512-sample
    // blocks. `gen(t, channel)` returns the input sample. Returns the
    // full stereo output.
    juce::AudioBuffer<float> render(FamousBeatsMicModelProcessor& proc, double seconds,
                                    const std::function<float(double, int)>& gen)
    {
        const int total = (int)(seconds * kSr);
        juce::AudioBuffer<float> out(2, total);
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> block(2, kBlock);
        int done = 0;
        while (done < total)
        {
            const int n = juce::jmin(kBlock, total - done);
            // Resize BEFORE filling, with avoidReallocating=true: the
            // buffer keeps its 512-sample storage and only the reported
            // size changes, so the filled samples survive. (Resizing
            // after filling with avoidReallocating=false reallocates
            // and discards the data on the final partial block.)
            block.setSize(2, n, false, false, true);
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* data = block.getWritePointer(ch);
                for (int i = 0; i < n; ++i)
                    data[i] = gen((done + i) / kSr, ch);
            }
            proc.processBlock(block, midi);
            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom(ch, done, block, ch, 0, n);
            done += n;
        }
        return out;
    }

    double regionRms(const juce::AudioBuffer<float>& buf, double t0, double t1)
    {
        const int s0 = (int)(t0 * kSr), s1 = (int)(t1 * kSr);
        double sum = 0.0;
        int n = 0;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const auto* data = buf.getReadPointer(ch);
            for (int i = s0; i < s1 && i < buf.getNumSamples(); ++i)
            {
                sum += (double)data[i] * data[i];
                ++n;
            }
        }
        return n ? std::sqrt(sum / n) : 0.0;
    }

    float regionPeak(const juce::AudioBuffer<float>& buf, double t0, double t1)
    {
        const int s0 = (int)(t0 * kSr), s1 = (int)(t1 * kSr);
        float peak = 0.0f;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const auto* data = buf.getReadPointer(ch);
            for (int i = s0; i < s1 && i < buf.getNumSamples(); ++i)
                peak = juce::jmax(peak, std::abs(data[i]));
        }
        return peak;
    }

    int countNan(const juce::AudioBuffer<float>& buf)
    {
        int n = 0;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const auto* data = buf.getReadPointer(ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
                if (!std::isfinite(data[i]))
                    ++n;
        }
        return n;
    }

    // Goertzel magnitude of one channel at a frequency (steady-state
    // region only). Used to measure harmonic content.
    double goertzelMag(const juce::AudioBuffer<float>& buf, double freq,
                       double t0, double t1, int channel = 0)
    {
        const int s0 = (int)(t0 * kSr), s1 = (int)(t1 * kSr);
        const double w = 2.0 * M_PI * freq / kSr;
        const double coeff = 2.0 * std::cos(w);
        double s1v = 0.0, s2v = 0.0;
        const auto* data = buf.getReadPointer(channel);
        int n = 0;
        for (int i = s0; i < s1 && i < buf.getNumSamples(); ++i)
        {
            const double s0v = (double)data[i] + coeff * s1v - s2v;
            s2v = s1v;
            s1v = s0v;
            ++n;
        }
        const double power = s2v * s2v + s1v * s1v - coeff * s1v * s2v;
        return n ? std::sqrt(power) / n : 0.0;
    }

    // Sine gain in dB through the processor at one frequency (drive 0,
    // amount 100). Measures the steady-state region only.
    double sineGainDb(FamousBeatsMicModelProcessor& p, double freqHz, double amp = 0.2)
    {
        const double w = 2.0 * M_PI * freqHz;
        auto out = render(p, 1.0, [&](double t, int) {
            return (float)(amp * std::sin(w * t));
        });
        const double outRms = regionRms(out, 0.5, 1.0);
        const double inRms = amp / std::sqrt(2.0);
        return 20.0 * std::log10(outRms / inRms + 1e-12);
    }
}

//==============================================================================
int main()
{
    std::printf("Famous Beats Mic Model — test harness\n");
    std::printf("Sample rate %.0f Hz, block %d\n\n", kSr, kBlock);

    // ---- 1. Instantiation ----
    {
        std::printf("[1] Instantiate + prepare\n");
        FamousBeatsMicModelProcessor p;
        check(p.getName() == "Famous Beats Mic Model", "plugin name");
        check(p.getTotalNumInputChannels() == 2, "stereo input bus");
        check(p.getTotalNumOutputChannels() == 2, "stereo output bus");
        check(!p.acceptsMidi() && !p.producesMidi(), "no MIDI");
        p.prepareToPlay(kSr, kBlock);
        std::printf("  ok\n");
    }

    // ---- 2. Silence ----
    {
        std::printf("[2] Silence in -> silence out\n");
        FamousBeatsMicModelProcessor p;
        setDefaults(p);
        p.prepareToPlay(kSr, kBlock);
        auto out = render(p, 1.0, [](double, int) { return 0.0f; });
        check(regionPeak(out, 0.0, 1.0) == 0.0f, "silence stays silent");
        check(countNan(out) == 0, "no NaN on silence");
        std::printf("  peak %.1e, NaN %d\n", regionPeak(out, 0.0, 1.0), countNan(out));
    }

    // ---- 3. White noise, default settings (drive 30) ----
    {
        std::printf("[3] White noise, defaults (drive 30)\n");
        FamousBeatsMicModelProcessor p;
        setDefaults(p);
        setParam(p, FamousBeatsMicModelProcessor::kTubeDriveParam, 30.0f);
        p.prepareToPlay(kSr, kBlock);
        std::mt19937 rng(12345);
        std::uniform_real_distribution<float> dist(-0.3f, 0.3f);
        auto out = render(p, 2.0, [&](double, int) { return dist(rng); });
        const float peak = regionPeak(out, 0.0, 2.0);
        check(peak <= 1.0f, "no clipping on noise");
        check(countNan(out) == 0, "no NaN on noise");
        check(regionRms(out, 0.5, 2.0) > 0.01, "noise passes through");
        std::printf("  peak %.3f, rms %.3f (%.1f dB), NaN %d\n",
                    peak, regionRms(out, 0.5, 2.0), db(regionRms(out, 0.5, 2.0)), countNan(out));
    }

    // ---- 4. Difference curve ----
    {
        std::printf("[4] Difference curve (defaults: warmth 50, air 50, drive 0)\n");
        FamousBeatsMicModelProcessor p;
        setDefaults(p);
        p.prepareToPlay(kSr, kBlock);

        // Analytic shape from the processor's own coefficients.
        const float r250  = p.getModelResponseDb(250.0f);
        const float r1000 = p.getModelResponseDb(1000.0f);
        const float r5500 = p.getModelResponseDb(5500.0f);
        const float r12k  = p.getModelResponseDb(12000.0f);
        const float r17k  = p.getModelResponseDb(17000.0f);
        const float r20k  = p.getModelResponseDb(20000.0f);
        std::printf("  analytic: 250 Hz %+0.2f dB, 1 kHz %+0.2f dB, 5.5 kHz %+0.2f dB,\n"
                    "            12 kHz %+0.2f dB, 17 kHz %+0.2f dB, 20 kHz %+0.2f dB\n",
                    r250, r1000, r5500, r12k, r17k, r20k);
        check(r250 >= 0.8f && r250 <= 2.2f, "warmth lift at 250 Hz (+1.5 dB expected)");
        check(std::abs(r1000) <= 0.75f, "flat at 1 kHz");
        check(r5500 >= -3.2f && r5500 <= -1.8f, "presence tamed at 5.5 kHz (-2.5 dB expected)");
        check(r12k > r5500 + 0.5f, "air recovers above the presence dip");
        check(r17k < r12k - 0.5f, "HF rolloff above 16 kHz");
        check(r20k < r12k - 1.5f, "response falls toward 18 kHz limit");

        // Measured sine gains must track the analytic curve.
        const double freqs[] = { 250.0, 1000.0, 5500.0, 12000.0, 17000.0 };
        const float analytic[] = { r250, r1000, r5500, r12k, r17k };
        for (int i = 0; i < 5; ++i)
        {
            const double measured = sineGainDb(p, freqs[i]);
            std::printf("  sine %6.0f Hz: measured %+0.2f dB vs analytic %+0.2f dB\n",
                        freqs[i], measured, analytic[i]);
            char msg[128];
            std::snprintf(msg, sizeof(msg), "sine gain tracks curve at %.0f Hz", freqs[i]);
            check(std::abs(measured - analytic[i]) <= 0.7, msg);
        }
    }

    // ---- 5. Warmth and Air knobs move their bands ----
    {
        std::printf("[5] Warmth / Air knob range\n");
        FamousBeatsMicModelProcessor p;
        setDefaults(p);
        setParam(p, FamousBeatsMicModelProcessor::kWarmthParam, 0.0f);
        setParam(p, FamousBeatsMicModelProcessor::kAirParam, 0.0f);
        p.prepareToPlay(kSr, kBlock);
        const float warm0 = p.getModelResponseDb(250.0f);
        const float air0 = p.getModelResponseDb(12000.0f);

        setParam(p, FamousBeatsMicModelProcessor::kWarmthParam, 100.0f);
        setParam(p, FamousBeatsMicModelProcessor::kAirParam, 100.0f);
        p.prepareToPlay(kSr, kBlock);
        const float warm100 = p.getModelResponseDb(250.0f);
        const float air100 = p.getModelResponseDb(12000.0f);
        std::printf("  warmth 0%% -> %+0.2f dB @250 Hz, 100%% -> %+0.2f dB\n", warm0, warm100);
        std::printf("  air    0%% -> %+0.2f dB @12 kHz, 100%% -> %+0.2f dB\n", air0, air100);
        check(warm100 - warm0 >= 2.0f, "Warmth 0->100 lifts 250 Hz by >= 2 dB");
        check(air100 - air0 >= 1.5f, "Air 0->100 lifts 12 kHz by >= 1.5 dB");
    }

    // ---- 6. Tube drive: 2nd harmonic, no clipping ----
    {
        std::printf("[6] Tube drive\n");
        const double w = 2.0 * M_PI * 1000.0;

        FamousBeatsMicModelProcessor p0;
        setDefaults(p0);
        setParam(p0, FamousBeatsMicModelProcessor::kTubeDriveParam, 0.0f);
        p0.prepareToPlay(kSr, kBlock);
        auto out0 = render(p0, 1.0, [&](double t, int) {
            return (float)(0.5 * std::sin(w * t));
        });

        FamousBeatsMicModelProcessor p100;
        setDefaults(p100);
        setParam(p100, FamousBeatsMicModelProcessor::kTubeDriveParam, 100.0f);
        p100.prepareToPlay(kSr, kBlock);
        auto out100 = render(p100, 1.0, [&](double t, int) {
            return (float)(0.5 * std::sin(w * t));
        });

        const double fund = goertzelMag(out100, 1000.0, 0.5, 1.0);
        const double second = goertzelMag(out100, 2000.0, 0.5, 1.0);
        const double secondDb = 20.0 * std::log10(second / (fund + 1e-12));
        std::printf("  drive 100: fundamental %.3f, 2nd harmonic %+.1f dB rel.\n",
                    fund, secondDb);
        check(secondDb > -40.0, "2nd harmonic present at drive 100 (tube warmth)");
        check(regionPeak(out100, 0.0, 1.0) <= 1.0f, "no clipping at drive 100");
        check(countNan(out100) == 0, "no NaN at drive 100");

        // Outputs must differ (drive actually does something).
        double diffRms = 0.0;
        {
            const auto* a = out0.getReadPointer(0);
            const auto* b = out100.getReadPointer(0);
            double sum = 0.0;
            const int n = out0.getNumSamples();
            for (int i = (int)(0.5 * kSr); i < n; ++i)
            {
                const double d = (double)b[i] - a[i];
                sum += d * d;
            }
            diffRms = std::sqrt(sum / (n - (int)(0.5 * kSr)));
        }
        std::printf("  drive 0 vs 100 output difference RMS %.3f\n", diffRms);
        check(diffRms > 0.01, "drive changes the sound");
    }

    // ---- 7. Bypass ----
    {
        std::printf("[7] Bypass\n");
        FamousBeatsMicModelProcessor p;
        setDefaults(p);
        setParam(p, FamousBeatsMicModelProcessor::kBypassParam, 1.0f);
        p.prepareToPlay(kSr, kBlock);
        auto out = render(p, 1.0, [](double t, int) {
            return (float)(0.3 * std::sin(2.0 * M_PI * 440.0 * t));
        });
        double maxErr = 0.0;
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto* data = out.getReadPointer(ch);
            for (int i = 0; i < out.getNumSamples(); ++i)
            {
                const float in = (float)(0.3 * std::sin(2.0 * M_PI * 440.0 * i / kSr));
                maxErr = juce::jmax(maxErr, (double)std::abs(data[i] - in));
            }
        }
        std::printf("  max bypass error %.2e\n", maxErr);
        check(maxErr < 1e-6, "bypass passes input untouched");
    }

    // ---- 8. Model Amount 0 ----
    {
        std::printf("[8] Model Amount 0\n");
        FamousBeatsMicModelProcessor p;
        setDefaults(p);
        setParam(p, FamousBeatsMicModelProcessor::kModelAmountParam, 0.0f);
        p.prepareToPlay(kSr, kBlock);
        auto out = render(p, 1.0, [](double t, int) {
            return (float)(0.3 * std::sin(2.0 * M_PI * 440.0 * t));
        });
        double maxErr = 0.0;
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto* data = out.getReadPointer(ch);
            for (int i = 0; i < out.getNumSamples(); ++i)
            {
                const float in = (float)(0.3 * std::sin(2.0 * M_PI * 440.0 * i / kSr));
                maxErr = juce::jmax(maxErr, (double)std::abs(data[i] - in));
            }
        }
        std::printf("  max amount-0 error %.2e\n", maxErr);
        check(maxErr < 1e-5, "amount 0 passes input untouched");
        check(countNan(out) == 0, "no NaN at amount 0");
    }

    std::printf("\n%d checks, %d failed\n", gChecks, gFailures);
    if (gFailures == 0)
        std::printf("ALL TESTS PASSED\n");
    return gFailures == 0 ? 0 : 1;
}
