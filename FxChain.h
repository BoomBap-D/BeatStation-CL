#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MasterFx.h"
#include "RoomFx.h"
#include "FxDsp.h"

// The effects chain on the output: eleven effects, each with its own on/off switch, in an order you choose.
// Sound goes through them left to right, then to the OUT meter.
enum FxId { fxEq, fxComp, fxDrive, fxFilter, fxCrush, fxTransient, fxChorus, fxDelay, fxRoom, fxWidth, fxLimiter, numFx };

namespace FxInfo
{
    inline const char* const names[numFx]  = { "EQ", "COMP", "DRIVE", "FILTER", "CRUSH", "PUNCH", "CHORUS", "DELAY", "ROOM", "WIDTH", "LIMITER" };
    inline const char* const keys[numFx]   = { "eq", "comp", "drive", "filter", "crush", "trans", "chorus", "delay", "room", "width", "limiter" };   // used by presets
    inline const char* const onIds[numFx]  = { "mfx_eq_on", "mfx_comp_on", "fx_drive_on", "fx_filt_on", "fx_crush_on", "fx_trans_on", "fx_chor_on",
                                               "fx_dly_on", "room_on", "fx_wid_on", "fx_lim_on" };
    inline const juce::Colour colours[numFx] = { juce::Colour (0xfff5a524), juce::Colour (0xffe0568c), juce::Colour (0xffff8a5c), juce::Colour (0xff3ad0e0),
                                                 juce::Colour (0xff9bd35a), juce::Colour (0xffece0c2), juce::Colour (0xff8f7cf0), juce::Colour (0xff5aa9ff),
                                                 juce::Colour (0xfff5a524), juce::Colour (0xff6fd36f), juce::Colour (0xffff5a4d) };
    inline const juce::StringArray syncNames { "Free", "1/2", "1/4", "1/4 dotted", "1/8", "1/8 dotted", "1/8 triplet", "1/16", "1/16 triplet" };
    inline const float syncBeats[9] = { 0.f, 2.f, 1.f, 1.5f, 0.5f, 0.75f, 1.f / 3.f, 0.25f, 1.f / 6.f };
}

class FxChain
{
public:
    static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& l)
    {
        using namespace juce;
        auto f = [&l] (const char* id, const char* name, float lo, float hi, float def, float skew = 1.f, float step = 0.f)
        {
            NormalisableRange<float> r (lo, hi, step);
            r.skew = skew;
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def));
        };
        auto b = [&l] (const char* id, const char* name, bool def) { l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
        auto c = [&l] (const char* id, const char* name, StringArray items, int def) { l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };

        b ("fx_drive_on", "Drive On", false);
        f ("fx_drive_amt", "Drive Amount", 0.f, 36.f, 6.f, 1.f, 0.1f);
        c ("fx_drive_type", "Drive Type", { "Tape", "Tube", "Clip" }, 0);
        f ("fx_drive_tone", "Drive Tone", 1000.f, 20000.f, 20000.f, 0.35f, 1.f);
        f ("fx_drive_mix", "Drive Mix", 0.f, 1.f, 1.f, 1.f, 0.01f);
        f ("fx_drive_out", "Drive Output", -24.f, 12.f, 0.f, 1.f, 0.1f);

        b ("fx_filt_on", "Filter On", false);
        c ("fx_filt_type", "Filter Type", { "Low-pass", "High-pass", "Band-pass", "Notch" }, 0);
        f ("fx_filt_cut", "Filter Cutoff", 20.f, 20000.f, 2000.f, 0.25f, 0.1f);
        f ("fx_filt_res", "Filter Resonance", 0.5f, 12.f, 0.71f, 0.5f, 0.01f);
        c ("fx_filt_slope", "Filter Slope", { "12 dB", "24 dB" }, 1);
        f ("fx_filt_rate", "Filter LFO Rate", 0.05f, 10.f, 0.5f, 0.4f, 0.01f);
        f ("fx_filt_depth", "Filter LFO Depth", 0.f, 4.f, 0.f, 1.f, 0.01f);

        b ("fx_crush_on", "Crush On", false);
        f ("fx_crush_rate", "Crush Sample Rate", 1000.f, 48000.f, 26040.f, 0.4f, 1.f);
        f ("fx_crush_bits", "Crush Bits", 4.f, 16.f, 12.f, 1.f, 0.1f);
        f ("fx_crush_cut", "Crush Filter", 500.f, 20000.f, 12000.f, 0.35f, 1.f);
        f ("fx_crush_mix", "Crush Mix", 0.f, 1.f, 1.f, 1.f, 0.01f);

        b ("fx_trans_on", "Punch On", false);
        f ("fx_trans_attack", "Punch Attack", -100.f, 100.f, 0.f, 1.f, 1.f);
        f ("fx_trans_sustain", "Punch Sustain", -100.f, 100.f, 0.f, 1.f, 1.f);
        f ("fx_trans_out", "Punch Output", -24.f, 12.f, 0.f, 1.f, 0.1f);

        b ("fx_chor_on", "Chorus On", false);
        f ("fx_chor_rate", "Chorus Rate", 0.05f, 8.f, 0.6f, 0.4f, 0.01f);
        f ("fx_chor_depth", "Chorus Depth", 0.f, 10.f, 3.f, 1.f, 0.01f);
        f ("fx_chor_delay", "Chorus Delay", 1.f, 30.f, 12.f, 0.5f, 0.01f);
        f ("fx_chor_fb", "Chorus Feedback", 0.f, 0.9f, 0.f, 1.f, 0.01f);
        f ("fx_chor_mix", "Chorus Mix", 0.f, 1.f, 0.5f, 1.f, 0.01f);

        b ("fx_dly_on", "Delay On", false);
        f ("fx_dly_time", "Delay Time", 1.f, 2000.f, 375.f, 0.4f, 0.1f);
        c ("fx_dly_sync", "Delay Sync", FxInfo::syncNames, 4);
        f ("fx_dly_fb", "Delay Feedback", 0.f, 0.95f, 0.35f, 1.f, 0.01f);
        b ("fx_dly_ping", "Delay Ping-Pong", false);
        f ("fx_dly_lowcut", "Delay Low Cut", 20.f, 1000.f, 120.f, 0.4f, 1.f);
        f ("fx_dly_highcut", "Delay High Cut", 1000.f, 20000.f, 6000.f, 0.4f, 1.f);
        f ("fx_dly_mix", "Delay Mix", 0.f, 1.f, 0.25f, 1.f, 0.01f);

        b ("fx_wid_on", "Width On", false);
        f ("fx_wid_width", "Width", 0.f, 2.f, 1.f, 1.f, 0.01f);
        f ("fx_wid_mono", "Width Mono Below", 20.f, 500.f, 20.f, 0.4f, 1.f);
        f ("fx_wid_out", "Width Output", -24.f, 12.f, 0.f, 1.f, 0.1f);

        b ("fx_lim_on", "Limiter On", false);
        f ("fx_lim_gain", "Limiter Gain", 0.f, 24.f, 0.f, 1.f, 0.1f);
        f ("fx_lim_ceil", "Limiter Ceiling", -12.f, 0.f, -0.3f, 1.f, 0.1f);
        f ("fx_lim_rel", "Limiter Release", 10.f, 1000.f, 120.f, 0.4f, 1.f);
    }

    FxChain (juce::AudioProcessorValueTreeState& state, MasterFx& m, RoomFx& r) : apvts (state), master (m), room (r)
    {
        for (int i = 0; i < numFx; ++i) { order[(size_t) i] = i; pOn[i] = apvts.getRawParameterValue (FxInfo::onIds[i]); }
        for (int i = 0; i < numP; ++i) raw[i] = apvts.getRawParameterValue (paramIds[i]);
    }

    void prepare (double sr, int blockSize)
    {
        sampleRate = sr;
        juce::ignoreUnused (blockSize);
        drive.prepare (sr);  filter.prepare (sr);  crush.prepare (sr);  trans.prepare (sr);
        chorus.prepare (sr); delay.prepare (sr);   width.prepare (sr);  limiter.prepare (sr);
        for (auto& w : wasOn) w = false;
    }

    // ---- order (message thread writes, audio thread reads; only atomics) ----
    int getOrder (int slot) const { return order[(size_t) slot].load(); }
    void moveModule (int fromSlot, int toSlot)
    {
        if (fromSlot == toSlot) return;
        int cur[numFx];
        for (int i = 0; i < numFx; ++i) cur[i] = order[(size_t) i].load();
        const int moving = cur[fromSlot];
        if (fromSlot < toSlot) for (int i = fromSlot; i < toSlot; ++i) cur[i] = cur[i + 1];
        else                   for (int i = fromSlot; i > toSlot; --i) cur[i] = cur[i - 1];
        cur[toSlot] = moving;
        for (int i = 0; i < numFx; ++i) order[(size_t) i] = cur[i];
    }
    juce::String orderString() const
    {
        juce::StringArray s;
        for (auto& o : order) s.add (FxInfo::keys[o.load()]);
        return s.joinIntoString (">");
    }
    void setOrderFromString (const juce::String& text)                  // anything not named keeps its usual place after the named ones
    {
        std::vector<int> result;
        for (const auto& t : juce::StringArray::fromTokens (text, ">,", ""))
            for (int i = 0; i < numFx; ++i)
                if (t.trim() == FxInfo::keys[i] && std::find (result.begin(), result.end(), i) == result.end()) result.push_back (i);
        for (int i = 0; i < numFx; ++i) if (std::find (result.begin(), result.end(), i) == result.end()) result.push_back (i);
        for (int i = 0; i < numFx; ++i) order[(size_t) i] = result[(size_t) i];
    }
    bool isOn (int fx) const { return pOn[fx]->load() > 0.5f; }

    float delayMsNow() const
    {
        const int sync = (int) v (23 /* dly_sync */);
        return sync > 0 ? juce::jlimit (1.f, 2000.f, 60000.f / juce::jmax (20.f, bpm.load()) * FxInfo::syncBeats[sync]) : v (24 /* dly_time */);
    }

    void process (juce::AudioBuffer<float>& buffer, double tempo)
    {
        bpm = (float) tempo;
        if (buffer.getNumChannels() < 2) { master.process (buffer); room.process (buffer); return; }
        const int n = buffer.getNumSamples();
        float* L = buffer.getWritePointer (0);
        float* R = buffer.getWritePointer (1);

        for (int slot = 0; slot < numFx; ++slot)
        {
            const int fx = order[(size_t) slot].load();
            const bool on = pOn[fx]->load() > 0.5f;
            const bool fresh = on && ! wasOn[fx];                       // just switched on: start from silence, not from old echoes
            wasOn[fx] = on;
            switch (fx)
            {
                case fxEq:    master.processEq (buffer);   break;       // these three look after their own on/off
                case fxComp:  master.processComp (buffer); break;
                case fxRoom:  room.process (buffer);       break;
                case fxDrive:
                    if (! on) break;
                    if (fresh) drive.prepare (sampleRate);
                    drive.driveDb = v (0 /* drive_amt */);  drive.type = (int) v (1 /* drive_type */);  drive.tone = v (2 /* drive_tone */);
                    drive.mix = v (3 /* drive_mix */);      drive.outDb = v (4 /* drive_out */);
                    drive.process (L, R, n);
                    driveIn = drive.inPeak;
                    break;
                case fxFilter:
                    if (! on) break;
                    if (fresh) filter.prepare (sampleRate);
                    filter.type = (int) v (5 /* filt_type */);  filter.cutoff = v (6 /* filt_cut */);   filter.q = v (7 /* filt_res */);
                    filter.slope = (int) v (8 /* filt_slope */); filter.lfoRate = v (9 /* filt_rate */); filter.lfoDepth = v (10 /* filt_depth */);
                    filter.process (L, R, n);
                    filterCut = filter.liveCutoff;
                    break;
                case fxCrush:
                    if (! on) break;
                    if (fresh) crush.prepare (sampleRate);
                    crush.rate = v (11 /* crush_rate */);  crush.bits = v (12 /* crush_bits */);  crush.cutoff = v (13 /* crush_cut */);  crush.mix = v (14 /* crush_mix */);
                    crush.process (L, R, n);
                    break;
                case fxTransient:
                    if (! on) { transGain = 0.f; break; }
                    if (fresh) trans.prepare (sampleRate);
                    trans.attack = v (15 /* trans_attack */) * 0.01f;  trans.sustain = v (16 /* trans_sustain */) * 0.01f;  trans.outDb = v (17 /* trans_out */);
                    trans.process (L, R, n);
                    transGain = trans.gainDbNow;
                    transLevel = trans.levelNow;
                    break;
                case fxChorus:
                    if (! on) break;
                    if (fresh) chorus.prepare (sampleRate);
                    chorus.rate = v (18 /* chor_rate */);  chorus.depthMs = v (19 /* chor_depth */);  chorus.delayMs = v (20 /* chor_delay */);
                    chorus.feedback = v (21 /* chor_fb */); chorus.mix = v (22 /* chor_mix */);
                    chorus.process (L, R, n);
                    chorusPhase = chorus.phaseNow;
                    break;
                case fxDelay:
                    if (! on) break;
                    if (fresh) delay.prepare (sampleRate);
                    delay.timeMs = delayMsNow();   delay.feedback = v (25 /* dly_fb */);      delay.pingPong = v (26 /* dly_ping */) > 0.5f;
                    delay.lowCut = v (27 /* dly_lowcut */); delay.highCut = v (28 /* dly_highcut */); delay.mix = v (29 /* dly_mix */);
                    delay.process (L, R, n);
                    break;
                case fxWidth:
                    if (! on) break;
                    if (fresh) width.prepare (sampleRate);
                    width.width = v (30 /* wid_width */);  width.monoBelow = v (31 /* wid_mono */);  width.outDb = v (32 /* wid_out */);
                    width.process (L, R, n);
                    widthCorr = width.correlation;
                    break;
                case fxLimiter:
                    if (! on) { limGr = 0.f; break; }
                    if (fresh) limiter.prepare (sampleRate);
                    limiter.gainDb = v (33 /* lim_gain */);  limiter.ceilingDb = v (34 /* lim_ceil */);  limiter.releaseMs = v (35 /* lim_rel */);
                    limiter.process (L, R, n);
                    limGr = limiter.grDb;
                    limIn = limiter.inPeak;
                    limOut = limiter.outPeak;
                    break;
                default: break;
            }
        }

        for (int i = 0; i < n; i += 6)                                  // a thin slice of the final output for the stereo scope
        {
            scopeL[(size_t) scopePos] = L[i];
            scopeR[(size_t) scopePos] = R[i];
            scopePos = (scopePos + 1) % scopeSize;
        }
    }

    // ---- read by the screens ----
    static constexpr int scopeSize = 512;
    std::array<std::atomic<float>, scopeSize> scopeL {}, scopeR {};
    std::atomic<float> driveIn { 0.f }, filterCut { 2000.f }, transGain { 0.f }, transLevel { 0.f }, chorusPhase { 0.f },
                       widthCorr { 1.f }, limGr { 0.f }, limIn { 0.f }, limOut { 0.f }, bpm { 92.f };
    juce::AudioProcessorValueTreeState& apvts;

    static constexpr int numP = 36;
    static constexpr const char* const paramIds[numP] = { "fx_drive_amt", "fx_drive_type", "fx_drive_tone", "fx_drive_mix", "fx_drive_out", "fx_filt_type", "fx_filt_cut", "fx_filt_res", "fx_filt_slope", "fx_filt_rate", "fx_filt_depth", "fx_crush_rate", "fx_crush_bits", "fx_crush_cut", "fx_crush_mix", "fx_trans_attack", "fx_trans_sustain", "fx_trans_out", "fx_chor_rate", "fx_chor_depth", "fx_chor_delay", "fx_chor_fb", "fx_chor_mix", "fx_dly_sync", "fx_dly_time", "fx_dly_fb", "fx_dly_ping", "fx_dly_lowcut", "fx_dly_highcut", "fx_dly_mix", "fx_wid_width", "fx_wid_mono", "fx_wid_out", "fx_lim_gain", "fx_lim_ceil", "fx_lim_rel" };
    float v (int p) const { return raw[p]->load (std::memory_order_relaxed); }   // the index is the position in paramIds

private:
    MasterFx& master;
    RoomFx& room;
    std::array<std::atomic<int>, numFx> order;
    std::atomic<float>* pOn[numFx] {};
    bool wasOn[numFx] {};
    fxdsp::Drive drive;  fxdsp::Filter filter;  fxdsp::Crush crush;  fxdsp::Transient trans;
    fxdsp::Chorus chorus; fxdsp::Delay delay;   fxdsp::Width width;  fxdsp::Limiter limiter;
    double sampleRate = 44100.0;
    int scopePos = 0;
    std::atomic<float>* raw[numP] {};
};
