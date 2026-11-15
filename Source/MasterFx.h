#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "CompMath.h"

namespace MfxID
{
    constexpr int numBands = 5;     // band 1 = low shelf, band 5 = high shelf, the rest are bells
    inline juce::String freq (int b) { return "mfx_b" + juce::String (b + 1) + "_freq"; }
    inline juce::String gain (int b) { return "mfx_b" + juce::String (b + 1) + "_gain"; }
    inline juce::String q    (int b) { return "mfx_b" + juce::String (b + 1) + "_q"; }
    inline const juce::String thresh = "mfx_c_thresh", ratio = "mfx_c_ratio", knee = "mfx_c_knee", attack = "mfx_c_attack",
                              release = "mfx_c_release", makeup = "mfx_c_makeup", mix = "mfx_c_mix", eqOn = "mfx_eq_on", compOn = "mfx_comp_on";
}

// The master EQ and compressor: the same processing as the stand-alone EQ/Comp plugin, built into the Beat Station.
// It works on the final mix: everything you play goes through the EQ, then the compressor, then out. Each section has its own
// on/off switch. The compressor starts switched off, so nothing changes until you turn it on.
class MasterFx
{
public:
    using Coeffs = juce::dsp::IIR::Coefficients<float>::Ptr;
    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder;

    static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& l)
    {
        using namespace juce;
        const float defFreq[MfxID::numBands] = { 60.f, 250.f, 1000.f, 4000.f, 12000.f };
        for (int b = 0; b < MfxID::numBands; ++b)
        {
            const bool shelf = (b == 0 || b == MfxID::numBands - 1);
            const String n = "Master EQ Band " + String (b + 1);
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { MfxID::freq (b), 1 }, n + " Freq", NormalisableRange<float> (20.f, 20000.f, 0.f, 0.25f), defFreq[b]));
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { MfxID::gain (b), 1 }, n + " Gain", NormalisableRange<float> (-18.f, 18.f, 0.1f), 0.f));
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { MfxID::q (b), 1 },    n + " Q",    NormalisableRange<float> (0.1f, 18.f, 0.01f, 0.4f), shelf ? 0.71f : 1.f));
        }
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { MfxID::thresh, 1 },  "Master Comp Threshold", NormalisableRange<float> (-60.f, 0.f, 0.1f), -18.f));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { MfxID::ratio, 1 },   "Master Comp Ratio",     NormalisableRange<float> (1.f, 20.f, 0.1f, 0.5f), 4.f));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { MfxID::knee, 1 },    "Master Comp Knee",      NormalisableRange<float> (0.f, 24.f, 0.1f), 6.f));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { MfxID::attack, 1 },  "Master Comp Attack",    NormalisableRange<float> (0.1f, 200.f, 0.1f, 0.4f), 8.f));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { MfxID::release, 1 }, "Master Comp Release",   NormalisableRange<float> (10.f, 1000.f, 1.f, 0.4f), 120.f));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { MfxID::makeup, 1 },  "Master Comp Makeup",    NormalisableRange<float> (-12.f, 24.f, 0.1f), 0.f));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { MfxID::mix, 1 },     "Master Comp Mix",       NormalisableRange<float> (0.f, 1.f, 0.01f), 1.f));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { MfxID::eqOn, 1 },   "Master EQ On",   true));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { MfxID::compOn, 1 }, "Master Comp On", false));
    }

    explicit MasterFx (juce::AudioProcessorValueTreeState& state) : apvts (state)
    {
        for (int b = 0; b < MfxID::numBands; ++b)
        {
            raw[b][0] = apvts.getRawParameterValue (MfxID::freq (b));
            raw[b][1] = apvts.getRawParameterValue (MfxID::gain (b));
            raw[b][2] = apvts.getRawParameterValue (MfxID::q (b));
        }
        pThresh = apvts.getRawParameterValue (MfxID::thresh);   pRatio = apvts.getRawParameterValue (MfxID::ratio);
        pKnee = apvts.getRawParameterValue (MfxID::knee);       pAttack = apvts.getRawParameterValue (MfxID::attack);
        pRelease = apvts.getRawParameterValue (MfxID::release); pMakeup = apvts.getRawParameterValue (MfxID::makeup);
        pMix = apvts.getRawParameterValue (MfxID::mix);         pEqOn = apvts.getRawParameterValue (MfxID::eqOn);
        pCompOn = apvts.getRawParameterValue (MfxID::compOn);
    }

    void prepare (double sr, int blockSize)
    {
        sampleRate = sr;
        const juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, blockSize), 2u };
        for (auto& f : eq) { f.prepare (spec); f.reset(); }
        for (auto& l : last) l[0] = l[1] = l[2] = -1000.f;            // force a coefficient refresh
        grState = 0.f;
        fifoIndex = 0;
    }

    Coeffs makeBandCoeffs (int b, double sr) const
    {
        using C = juce::dsp::IIR::Coefficients<float>;
        const float f = juce::jlimit (20.f, (float) (sr * 0.45), raw[b][0]->load());
        const float g = juce::Decibels::decibelsToGain (raw[b][1]->load());
        const float q = raw[b][2]->load();
        if (b == 0)                      return C::makeLowShelf (sr, f, q, g);
        if (b == MfxID::numBands - 1)    return C::makeHighShelf (sr, f, q, g);
        return C::makePeakFilter (sr, f, q, g);
    }

    double getSR() const { return sampleRate; }

    void process (juce::AudioBuffer<float>& buffer) { processEq (buffer); processComp (buffer); }

    void processEq (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = buffer.getNumChannels(), n = buffer.getNumSamples();

        for (int i = 0; i < n; ++i)                                   // spectrum analyser tap (what is going into the master EQ)
        {
            float s = 0.f;
            for (int ch = 0; ch < numCh; ++ch) s += buffer.getReadPointer (ch)[i];
            pushSample (s / (float) juce::jmax (1, numCh));
        }

        if (pEqOn->load() > 0.5f)
        {
            for (int b = 0; b < MfxID::numBands; ++b)                 // only rebuild a band's filter when one of its parameters changed
            {
                const float now[3] = { raw[b][0]->load(), raw[b][1]->load(), raw[b][2]->load() };
                if (now[0] != last[b][0] || now[1] != last[b][1] || now[2] != last[b][2])
                {
                    *eq[(size_t) b].state = *makeBandCoeffs (b, sampleRate);
                    last[b][0] = now[0]; last[b][1] = now[1]; last[b][2] = now[2];
                }
            }
            juce::dsp::AudioBlock<float> block (buffer);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            for (auto& f : eq) f.process (ctx);
        }
    }

    void processComp (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = buffer.getNumChannels(), n = buffer.getNumSamples();
        if (pCompOn->load() < 0.5f)                                   // compressor bypass
        {
            inDb.store (juce::Decibels::gainToDecibels (buffer.getMagnitude (0, n), -100.f));
            grDb.store (0.f);
            grState = 0.f;
            return;
        }

        const float thr = pThresh->load(), ratio = pRatio->load(), knee = pKnee->load(), makeup = pMakeup->load(), mix = pMix->load();
        const float aC = std::exp (-1.f / (0.001f * pAttack->load() * (float) sampleRate));
        const float rC = std::exp (-1.f / (0.001f * pRelease->load() * (float) sampleRate));
        float blockPeak = 0.f, blockGr = 0.f;
        for (int i = 0; i < n; ++i)
        {
            float peak = 0.f;
            for (int ch = 0; ch < numCh; ++ch) peak = juce::jmax (peak, std::abs (buffer.getReadPointer (ch)[i]));
            blockPeak = juce::jmax (blockPeak, peak);
            const float lvl = juce::Decibels::gainToDecibels (peak, -100.f);
            const float gr = CompMath::staticCurve (lvl, thr, ratio, knee) - lvl;      // <= 0
            const float coef = gr < grState ? aC : rC;
            grState = coef * grState + (1.f - coef) * gr;
            blockGr = juce::jmin (blockGr, grState);
            const float gain = juce::Decibels::decibelsToGain (grState + makeup);
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = buffer.getWritePointer (ch);
                const float dry = d[i];
                d[i] = dry + mix * (dry * gain - dry);
            }
        }
        inDb.store (juce::Decibels::gainToDecibels (blockPeak, -100.f));
        grDb.store (blockGr);
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::atomic<float> inDb { -100.f }, grDb { 0.f };                 // read by the compressor display
    std::array<float, fftSize * 2> fftData {};                        // read by the EQ spectrum display
    std::atomic<bool> fftReady { false };

private:
    void pushSample (float s)
    {
        if (fifoIndex == fftSize)
        {
            if (! fftReady.load())
            {
                std::fill (fftData.begin(), fftData.end(), 0.f);
                std::copy (fifo.begin(), fifo.end(), fftData.begin());
                fftReady = true;
            }
            fifoIndex = 0;
        }
        fifo[(size_t) fifoIndex++] = s;
    }

    double sampleRate = 44100.0;
    std::array<juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>, MfxID::numBands> eq;
    std::atomic<float>* raw[MfxID::numBands][3] {};
    std::atomic<float> *pThresh {}, *pRatio {}, *pKnee {}, *pAttack {}, *pRelease {}, *pMakeup {}, *pMix {}, *pEqOn {}, *pCompOn {};
    float last[MfxID::numBands][3] {};
    float grState = 0.f;
    std::array<float, fftSize> fifo {};
    int fifoIndex = 0;
};
