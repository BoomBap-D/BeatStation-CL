#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "FxDsp.h"
#include "RoomEngine.h"
#include "FxChain.h"

// ====================================================================== THE VOCAL BOOTH CHAIN
// Vocals go: MIC character -> the eight vocal effects in the order you set -> the main mix (and then the master effects).
// The mic character is applied while you listen, so you can pick a different microphone after the take.
// With BURN IN on, the character is recorded into the take instead and is not applied again on playback.
enum BoothFx { bGate, bDeEss, bComp, bEq, bDrive, bDouble, bDelay, bReverb, numBooth };

namespace BoothInfo
{
    inline const char* const names[numBooth] = { "GATE", "DE-ESS", "COMP", "EQ", "DRIVE", "DOUBLER", "DELAY", "REVERB" };
    inline const char* const keys[numBooth]  = { "gate", "deess", "comp", "eq", "drive", "dbl", "dly", "rev" };
    inline const char* const onIds[numBooth] = { "bth_gate_on", "bth_deess_on", "bth_comp_on", "bth_eq_on", "bth_drive_on", "bth_dbl_on", "bth_dly_on", "bth_rev_on" };
    inline const juce::Colour colours[numBooth] = { juce::Colour (0xff9aa3b2), juce::Colour (0xff3ad0e0), juce::Colour (0xffe0568c), juce::Colour (0xfff5a524),
                                                    juce::Colour (0xffff8a5c), juce::Colour (0xff8f7cf0), juce::Colour (0xff5aa9ff), juce::Colour (0xff6fd36f) };
}

class BoothChain
{
public:
    static constexpr int numP = 26;
    static constexpr const char* const paramIds[numP] = {
        "bth_gate_thresh", "bth_gate_rel", "bth_deess_freq", "bth_deess_thresh", "bth_deess_max",
        "bth_comp_thresh", "bth_comp_ratio", "bth_comp_attack", "bth_comp_release", "bth_comp_makeup",
        "bth_eq_lowcut", "bth_eq_mud", "bth_eq_pres", "bth_eq_air", "bth_drive_amt", "bth_drive_mix",
        "bth_dbl_mix", "bth_dbl_depth", "bth_dly_sync", "bth_dly_fb", "bth_dly_ping", "bth_dly_mix",
        "bth_rev_decay", "bth_rev_predelay", "bth_rev_mix", "bth_out" };

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
        StringArray mics;
        for (const auto& m : fxdsp::micProfiles()) mics.add (m.name);
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { "bth_mic", 1 }, "Booth Microphone", mics, 1));
        b ("bth_burn", "Booth Burn In Mic", false);
        b ("bth_monitor", "Booth Monitor", false);
        b ("bth_tune_on", "Booth Tune On", false);
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { "bth_tune_key", 1 }, "Booth Tune Key", StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { "bth_tune_scale", 1 }, "Booth Tune Scale", StringArray (fxdsp::tune::scaleNames, 6), 0));
        f ("bth_tune_speed", "Booth Tune Speed", 0.f, 400.f, 20.f, 0.4f, 1.f);
        f ("bth_tune_amount", "Booth Tune Amount", 0.f, 1.f, 1.f, 1.f, 0.01f);
        b ("bth_gate_on", "Booth Gate On", false);       f ("bth_gate_thresh", "Booth Gate Threshold", -80.f, -20.f, -50.f, 1.f, 0.5f);   f ("bth_gate_rel", "Booth Gate Release", 20.f, 500.f, 150.f, 0.5f, 1.f);
        b ("bth_deess_on", "Booth De-Ess On", true);     f ("bth_deess_freq", "Booth De-Ess Frequency", 3000.f, 10000.f, 6500.f, 0.6f, 1.f);
        f ("bth_deess_thresh", "Booth De-Ess Threshold", -50.f, -10.f, -30.f, 1.f, 0.5f);   f ("bth_deess_max", "Booth De-Ess Max Cut", 0.f, 12.f, 6.f, 1.f, 0.1f);
        b ("bth_comp_on", "Booth Comp On", true);        f ("bth_comp_thresh", "Booth Comp Threshold", -50.f, 0.f, -20.f, 1.f, 0.5f);     f ("bth_comp_ratio", "Booth Comp Ratio", 1.f, 20.f, 4.f, 0.5f, 0.1f);
        f ("bth_comp_attack", "Booth Comp Attack", 0.1f, 50.f, 3.f, 0.4f, 0.1f);   f ("bth_comp_release", "Booth Comp Release", 10.f, 500.f, 60.f, 0.5f, 1.f);   f ("bth_comp_makeup", "Booth Comp Makeup", 0.f, 24.f, 4.f, 1.f, 0.1f);
        b ("bth_eq_on", "Booth EQ On", true);            f ("bth_eq_lowcut", "Booth EQ Low Cut", 20.f, 300.f, 90.f, 0.5f, 1.f);
        f ("bth_eq_mud", "Booth EQ Mud", -12.f, 6.f, -3.f, 1.f, 0.1f);   f ("bth_eq_pres", "Booth EQ Presence", -6.f, 12.f, 2.5f, 1.f, 0.1f);   f ("bth_eq_air", "Booth EQ Air", -6.f, 12.f, 2.f, 1.f, 0.1f);
        b ("bth_drive_on", "Booth Drive On", false);     f ("bth_drive_amt", "Booth Drive Amount", 0.f, 24.f, 4.f, 1.f, 0.1f);   f ("bth_drive_mix", "Booth Drive Mix", 0.f, 1.f, 0.5f, 1.f, 0.01f);
        b ("bth_dbl_on", "Booth Doubler On", false);     f ("bth_dbl_mix", "Booth Doubler Mix", 0.f, 1.f, 0.3f, 1.f, 0.01f);     f ("bth_dbl_depth", "Booth Doubler Depth", 0.f, 5.f, 1.5f, 1.f, 0.01f);
        b ("bth_dly_on", "Booth Delay On", false);
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { "bth_dly_sync", 1 }, "Booth Delay Sync", boothSyncNames(), 3));
        f ("bth_dly_fb", "Booth Delay Feedback", 0.f, 0.9f, 0.25f, 1.f, 0.01f);   b ("bth_dly_ping", "Booth Delay Ping-Pong", false);   f ("bth_dly_mix", "Booth Delay Mix", 0.f, 1.f, 0.15f, 1.f, 0.01f);
        b ("bth_rev_on", "Booth Reverb On", false);      f ("bth_rev_decay", "Booth Reverb Decay", 0.2f, 5.f, 1.f, 0.5f, 0.01f);
        f ("bth_rev_predelay", "Booth Reverb Pre-delay", 0.f, 120.f, 20.f, 1.f, 1.f);   f ("bth_rev_mix", "Booth Reverb Mix", 0.f, 1.f, 0.15f, 1.f, 0.01f);
        f ("bth_out", "Booth Output", -24.f, 12.f, 0.f, 1.f, 0.1f);
    }
    // the booth delay is always tempo-synced: these are its note lengths (in beats)
    static juce::StringArray boothSyncNames() { return { "1/2", "1/4", "1/4 dotted", "1/8", "1/8 dotted", "1/8 triplet", "1/16" }; }
    static constexpr float syncBeats[7] = { 2.f, 1.f, 1.5f, 0.5f, 0.75f, 1.f / 3.f, 0.25f };

    explicit BoothChain (juce::AudioProcessorValueTreeState& state) : apvts (state)
    {
        for (int i = 0; i < numBooth; ++i) { order[(size_t) i] = i; pOn[i] = apvts.getRawParameterValue (BoothInfo::onIds[i]); }
        for (int i = 0; i < numP; ++i) raw[i] = apvts.getRawParameterValue (paramIds[i]);
        pMic = apvts.getRawParameterValue ("bth_mic");
        pBurn = apvts.getRawParameterValue ("bth_burn");
        pMonitor = apvts.getRawParameterValue ("bth_monitor");
        pTuneOn = apvts.getRawParameterValue ("bth_tune_on");    pTuneKey = apvts.getRawParameterValue ("bth_tune_key");
        pTuneScale = apvts.getRawParameterValue ("bth_tune_scale"); pTuneSpeed = apvts.getRawParameterValue ("bth_tune_speed");
        pTuneAmount = apvts.getRawParameterValue ("bth_tune_amount");
    }

    void prepare (double sr)
    {
        sampleRate = sr;
        mic.prepare (sr);  burnMic.prepare (sr);
        for (int c = 0; c < 2; ++c) { gate[c].prepare (sr); deess[c].prepare (sr); comp[c].prepare (sr); eq[c].prepare (sr); }
        drive.prepare (sr);  doubler.prepare (sr);  delay.prepare (sr);
        reverb.prepare (sr, reverbSettings());
        for (auto& t : tuners) t.prepare (sr);
        for (auto* v : { &bufL, &bufR }) v->assign (maxBlock, 0.f);
    }

    bool monitorOn() const { return pMonitor->load() > 0.5f; }

    // ---- TUNE: pitch correction. It works on one voice at a time, so every vocal track (and the live input) has its own copy,
    // all set to the same key, scale and speed. It comes straight after the microphone, before the rest of the chain.
    static constexpr int numTuners = 9;                                  // 8 tracks + the live input
    bool tuneOn() const { return pTuneOn->load() > 0.5f; }
    int tuneLatency() const { return tuners[0].latency(); }
    void tuneBlock (int slot, float* buf, int n)
    {
        auto& t = tuners[(size_t) juce::jlimit (0, numTuners - 1, slot)];
        t.key = (int) pTuneKey->load();  t.scale = (int) pTuneScale->load();  t.speedMs = pTuneSpeed->load();  t.amount = pTuneAmount->load();
        for (int i = 0; i < n; ++i) buf[i] = t.tick (buf[i]);
        if (t.detectedNote > 0.f) { tuneDetected = t.detectedNote; tuneTarget = t.targetNote; tuneAge = 0; }
        else if (++tuneAge > 20) { tuneDetected = 0.f; tuneTarget = 0.f; }
    }
    std::atomic<float> tuneDetected { 0.f }, tuneTarget { 0.f };          // MIDI note numbers, 0 = no clear note
    bool burnOn() const { return pBurn->load() > 0.5f; }
    int micIndex() const { return (int) pMic->load(); }
    bool isOn (int fx) const { return pOn[fx]->load() > 0.5f; }
    float v (int p) const { return raw[p]->load (std::memory_order_relaxed); }

    // the microphone character for a take that is being burned in (its own copy, so playback is not disturbed)
    float burnTick (float x) { if (burnMic.index() != micIndex()) burnMic.set (micIndex()); return burnMic.tick (x); }

    int getOrder (int slot) const { return order[(size_t) slot].load(); }
    void moveModule (int from, int to)
    {
        if (from == to) return;
        int cur[numBooth];
        for (int i = 0; i < numBooth; ++i) cur[i] = order[(size_t) i].load();
        const int moving = cur[from];
        if (from < to) for (int i = from; i < to; ++i) cur[i] = cur[i + 1];
        else           for (int i = from; i > to; --i) cur[i] = cur[i - 1];
        cur[to] = moving;
        for (int i = 0; i < numBooth; ++i) order[(size_t) i] = cur[i];
    }
    juce::String orderString() const { juce::StringArray s; for (auto& o : order) s.add (BoothInfo::keys[o.load()]); return s.joinIntoString (">"); }
    void setOrderFromString (const juce::String& text)
    {
        std::vector<int> result;
        for (const auto& t : juce::StringArray::fromTokens (text, ">,", ""))
            for (int i = 0; i < numBooth; ++i)
                if (t.trim() == BoothInfo::keys[i] && std::find (result.begin(), result.end(), i) == result.end()) result.push_back (i);
        for (int i = 0; i < numBooth; ++i) if (std::find (result.begin(), result.end(), i) == result.end()) result.push_back (i);
        for (int i = 0; i < numBooth; ++i) order[(size_t) i] = result[(size_t) i];
    }

    // rawIn: vocals that still need the mic character. burnedIn: takes that already have it. The result is ADDED to outL / outR.
    void process (const float* rawIn, const float* burnedIn, float* outL, float* outR, int n, double bpm)
    {
        for (int start = 0; start < n; start += maxBlock)
            processChunk (rawIn + start, burnedIn + start, outL + start, outR + start, juce::jmin (maxBlock, n - start), bpm);
    }

    float micResponseDb (float hz) const { return shown.responseDb (hz); }      // for the screen (message thread copy)
    void refreshShownMic (double sr) { if (shown.index() != micIndex() || shownSr != sr) { shownSr = sr; shown.prepare (sr); shown.set (micIndex()); } }

    std::atomic<float> inLevel { 0.f }, outLevel { 0.f }, compGr { 0.f }, deessCut { 0.f }, gateGain { 1.f };
    juce::AudioProcessorValueTreeState& apvts;

private:
    static constexpr int maxBlock = 4096;

    room::Parameters reverbSettings() const
    {
        room::Parameters p;                                              // a smooth vocal plate-style space
        p.width = 9.f; p.depth = 13.f; p.height = 5.f; p.sx = 0.42f; p.sy = 0.3f; p.lx = 0.6f; p.ly = 0.7f;
        p.decay = v (22); p.predelay = v (23); p.damping = 7500.f; p.absorption = 0.3f; p.diffusion = 1.f; p.spread = 1.f;
        p.wet = 1.f; p.direct = 0.f; p.early = 0.35f; p.late = 0.9f; p.duck = 0.f; p.output = 0.f; p.lowCut = 300.f;   // low cut on the return keeps the reverb out of the low end
        p.wetSolo = true; p.bypass = false; p.subject = room::Singer;
        return p;
    }

    void processChunk (const float* rawIn, const float* burnedIn, float* outL, float* outR, int n, double bpm)
    {
        if (mic.index() != micIndex()) mic.set (micIndex());
        float* L = bufL.data();
        float* R = bufR.data();
        float peak = 0.f;
        for (int i = 0; i < n; ++i)
        {
            const float x = mic.tick (rawIn[i]) + burnedIn[i];
            peak = juce::jmax (peak, std::abs (x));
            L[i] = R[i] = x;
        }
        inLevel = peak;
        if (peak < 1.0e-7f && ++silentBlocks > 400) { outLevel = 0.f; compGr = 0.f; deessCut = 0.f; return; }   // nothing coming in and the tails are long gone: do no work
        if (peak >= 1.0e-7f) silentBlocks = 0;

        for (int slot = 0; slot < numBooth; ++slot)
        {
            const int fx = order[(size_t) slot].load();
            if (pOn[fx]->load() < 0.5f) continue;
            switch (fx)
            {
                case bGate:
                    for (int c = 0; c < 2; ++c) { gate[c].threshDb = v (0); gate[c].releaseMs = v (1); gate[c].update(); }
                    for (int i = 0; i < n; ++i) { L[i] = gate[0].tick (L[i]); R[i] = gate[1].tick (R[i]); }
                    gateGain = gate[0].gainNow;
                    break;
                case bDeEss:
                    for (int c = 0; c < 2; ++c) { deess[c].freq = v (2); deess[c].threshDb = v (3); deess[c].maxCutDb = v (4); deess[c].update(); }
                    for (int i = 0; i < n; ++i) { L[i] = deess[0].tick (L[i]); R[i] = deess[1].tick (R[i]); }
                    deessCut = deess[0].cutNow;
                    break;
                case bComp:
                    for (int c = 0; c < 2; ++c) { comp[c].threshDb = v (5); comp[c].ratio = v (6); comp[c].attackMs = v (7); comp[c].releaseMs = v (8); comp[c].makeupDb = v (9); comp[c].update(); }
                    for (int i = 0; i < n; ++i) { L[i] = comp[0].tick (L[i]); R[i] = comp[1].tick (R[i]); }
                    compGr = comp[0].grNow;
                    break;
                case bEq:
                    for (int c = 0; c < 2; ++c) { eq[c].lowCut = v (10); eq[c].mudDb = v (11); eq[c].presDb = v (12); eq[c].airDb = v (13); eq[c].update(); }
                    for (int i = 0; i < n; ++i) { L[i] = eq[0].tick (L[i]); R[i] = eq[1].tick (R[i]); }
                    break;
                case bDrive:
                    drive.driveDb = v (14); drive.mix = v (15); drive.type = 0; drive.tone = 20000.f; drive.outDb = 0.f;
                    drive.process (L, R, n);
                    break;
                case bDouble:                                            // a widening double: two slow moving delays, a little different left to right
                    doubler.rate = 0.35f; doubler.delayMs = 18.f; doubler.depthMs = v (17); doubler.feedback = 0.f; doubler.mix = v (16) * 0.5f;
                    doubler.process (L, R, n);
                    break;
                case bDelay:
                    delay.timeMs = juce::jlimit (1.f, 2000.f, 60000.f / (float) juce::jmax (20.0, bpm) * syncBeats[juce::jlimit (0, 6, (int) v (18))]);
                    delay.feedback = v (19); delay.pingPong = v (20) > 0.5f; delay.mix = v (21); delay.lowCut = 300.f; delay.highCut = 4500.f;
                    delay.process (L, R, n);
                    break;
                case bReverb:
                {
                    reverb.setParameters (reverbSettings());
                    const float m = v (24);
                    for (int i = 0; i < n; ++i)
                    {
                        const auto w = reverb.process (L[i], R[i]);
                        L[i] += m * w[0];
                        R[i] += m * w[1];
                    }
                    break;
                }
                default: break;
            }
        }
        const float g = juce::Decibels::decibelsToGain (v (25));
        float po = 0.f;
        for (int i = 0; i < n; ++i)
        {
            outL[i] += L[i] * g;
            outR[i] += R[i] * g;
            po = juce::jmax (po, std::abs (L[i] * g));
        }
        outLevel = po;
    }

    std::array<std::atomic<int>, numBooth> order;
    std::atomic<float>* pOn[numBooth] {};
    std::atomic<float>* raw[numP] {};
    std::atomic<float> *pMic = nullptr, *pBurn = nullptr, *pMonitor = nullptr, *pTuneOn = nullptr, *pTuneKey = nullptr, *pTuneScale = nullptr, *pTuneSpeed = nullptr, *pTuneAmount = nullptr;
    std::array<fxdsp::Tuner, numTuners> tuners;
    int tuneAge = 0;
    fxdsp::MicModel mic, burnMic, shown;
    fxdsp::Gate gate[2];  fxdsp::DeEsser deess[2];  fxdsp::VocalComp comp[2];  fxdsp::VocalEq eq[2];
    fxdsp::Drive drive;   fxdsp::Chorus doubler;    fxdsp::Delay delay;
    room::Engine reverb;
    std::vector<float> bufL, bufR;
    double sampleRate = 44100.0, shownSr = 0.0;
    int silentBlocks = 0;
};

// ====================================================================== THE MULTITRACK
// Eight audio tracks that play in time with the beat. Tracks 1-4 start as vocal tracks that go through the Booth; 5-8 go straight to the mix.
// One track is armed at a time. With REC on, the whole pass is captured, and the punch mode decides which part of it is kept:
//   0 Whole take   - everything from where Play started.
//   1 Manual punch - press PUNCH to drop in, press it again to drop out.
//   2 Auto punch   - only between the IN and OUT points you set on the track.
//   3 Match punch  - rap along with the last take. When your level has matched the old take's level for a quarter of a second,
//                    it drops in by itself, starting from the last gap between phrases so the join falls on a breath, not mid-word.
// Whatever is kept replaces just that part of the track (8 ms crossfades at the joins). The rest of the old take stays.
struct TrackClip { juce::AudioBuffer<float> data; double sampleRate = 44100.0; };   // one channel

class AudioTracks
{
public:
    static constexpr int num = 8;
    struct Track
    {
        mutable juce::SpinLock lock;
        std::shared_ptr<const TrackClip> clip, undoClip;
        std::atomic<juce::int64> start { 0 };                            // where the clip begins in the song, in samples
        juce::int64 undoStart = 0;
        bool hasUndo = false;
        std::atomic<float> vol { 0.8f }, pan { 0.f }, peak { 0.f };
        std::atomic<bool> mute { false }, solo { false }, booth { false }, burned { false };
        juce::String name, file;
    };
    std::array<Track, num> tracks;

    std::atomic<int> armed { -1 };                                       // which track records (-1 = none)
    std::atomic<bool> recEnabled { false }, recording { false }, takePending { false };
    std::atomic<juce::int64> songPos { 0 }, songStart { 0 };
    std::atomic<bool> restart { false };
    std::atomic<int> version { 0 };                                      // bumps when a clip changes (for the screens)

    // ---- punch
    std::atomic<int> punchMode { 0 };
    std::atomic<juce::int64> punchIn { -1 }, punchOut { -1 };            // auto punch points, in samples (-1 = not set)
    std::atomic<bool> punchToggle { false };                             // manual punch: set true to drop in / out
    std::atomic<float> matchTolDb { 6.f };                               // match punch: how close the two levels must be
    std::atomic<juce::int64> regionStart { -1 }, regionEnd { -1 };       // what this pass is keeping (for the screen too)
    std::atomic<float> matchLive { 0.f }, matchOld { 0.f };              // the two levels being compared (for the screen)

    AudioTracks()
    {
        static const char* names[num] = { "Lead Vocal", "Double", "Ad-Libs", "Hook", "Audio 5", "Audio 6", "Audio 7", "Audio 8" };
        for (int i = 0; i < num; ++i) { tracks[(size_t) i].name = names[i]; tracks[(size_t) i].booth = i < 4; }
    }

    void prepare (double sr)
    {
        sampleRate = sr;
        for (auto* v : { &rawBus, &burnBus, &trackBuf, &liveBuf }) v->assign (maxBlock, 0.f);
    }

    std::shared_ptr<const TrackClip> getClip (int t) const { const juce::SpinLock::ScopedLockType l (tracks[(size_t) t].lock); return tracks[(size_t) t].clip; }
    bool isPunchedAt (juce::int64 p) const { const auto rs = regionStart.load(), re = regionEnd.load(); return rs >= 0 && p >= rs && (re < 0 || p < re); }

    // ---- message thread ----
    void arm (int t)                                                     // room for a 5 minute pass is made here, never while recording
    {
        if (t >= 0 && take.getNumSamples() == 0) take.setSize (1, (int) (sampleRate * 300.0) + 16);
        armed = t;
    }
    void clearClip (int t)
    {
        auto& tr = tracks[(size_t) t];
        rememberForUndo (t);
        { const juce::SpinLock::ScopedLockType l (tr.lock); tr.clip = nullptr; }
        tr.file = {};
        ++version;
    }
    void setClip (int t, std::shared_ptr<const TrackClip> c, juce::int64 start, const juce::String& file, bool burned)
    {
        auto& tr = tracks[(size_t) t];
        { const juce::SpinLock::ScopedLockType l (tr.lock); tr.clip = std::move (c); }
        tr.start = start;  tr.file = file;  tr.burned = burned;
        ++version;
    }
    bool canUndo (int t) const { return tracks[(size_t) t].hasUndo; }
    void undo (int t)                                                    // puts back what was on the track before the last take (press again to redo)
    {
        auto& tr = tracks[(size_t) t];
        if (! tr.hasUndo) return;
        auto prev = tr.undoClip;
        const auto prevStart = tr.undoStart;
        tr.undoClip = getClip (t);
        tr.undoStart = tr.start.load();
        { const juce::SpinLock::ScopedLockType l (tr.lock); tr.clip = prev; }
        tr.start = prevStart;
        tr.file = prev != nullptr ? saveWav (*prev, t) : juce::String();
        ++version;
    }
    bool loadClipFile (int t, const juce::File& f, juce::int64 start, bool burned, juce::AudioFormatManager& formats)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
        if (reader == nullptr || reader->lengthInSamples <= 0) return false;
        const int len = (int) std::min (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 600.0));
        juce::AudioBuffer<float> tmp ((int) reader->numChannels, len);
        reader->read (&tmp, 0, len, 0, true, true);
        auto c = std::make_shared<TrackClip>();
        c->data.setSize (1, len);
        c->data.copyFrom (0, 0, tmp, 0, 0, len);
        if (tmp.getNumChannels() > 1) { c->data.addFrom (0, 0, tmp, 1, 0, len); c->data.applyGain (0.5f); }   // stereo files are folded to one channel
        c->sampleRate = reader->sampleRate;
        setClip (t, c, start, f.getFullPathName(), burned);
        return true;
    }

    // Call from a timer. When a pass has finished, this joins the kept part into the track and saves it. Returns true if the track changed.
    bool commitTake()
    {
        if (! takePending.exchange (false)) return false;
        const int t = takeTrack, got = juce::jmin (takeLen.load(), take.getNumSamples());
        juce::int64 rs = regionStart.load(), re = regionEnd.load();
        if (t < 0 || rs < 0) return false;                               // never punched in: the track is left exactly as it was
        rs = juce::jmax (rs, takeStart);
        re = juce::jmin (re < 0 ? takeStart + got : re, takeStart + (juce::int64) got);
        if (re - rs < (juce::int64) (sampleRate * 0.05)) return false;

        auto& tr = tracks[(size_t) t];
        const auto old = getClip (t);
        juce::AudioBuffer<float> oldData;                                // the old recording at today's sample rate
        juce::int64 oldStart = tr.start.load(), oldLen = 0;
        if (old != nullptr)
        {
            const double ratio = old->sampleRate / sampleRate;
            oldLen = (juce::int64) ((double) old->data.getNumSamples() / ratio);
            oldData.setSize (1, (int) oldLen);
            const float* src = old->data.getReadPointer (0);
            float* dst = oldData.getWritePointer (0);
            const int last = old->data.getNumSamples() - 1;
            for (int i = 0; i < (int) oldLen; ++i)
            {
                const double p = (double) i * ratio;
                const int idx = juce::jmin (last, (int) p), nxt = juce::jmin (last, idx + 1);
                dst[i] = src[idx] + (src[nxt] - src[idx]) * (float) (p - (double) idx);
            }
        }
        const juce::int64 newStart = old != nullptr ? juce::jmin (oldStart, rs) : rs, newEnd = old != nullptr ? juce::jmax (oldStart + oldLen, re) : re;
        auto c = std::make_shared<TrackClip>();
        c->sampleRate = sampleRate;
        c->data.setSize (1, (int) (newEnd - newStart));
        c->data.clear();
        if (old != nullptr) c->data.copyFrom (0, (int) (oldStart - newStart), oldData, 0, 0, (int) oldLen);
        float* out = c->data.getWritePointer (0);
        const float* tk = take.getReadPointer (0);
        const double fade = juce::jmax (1.0, sampleRate * 0.008);        // 8 ms crossfade at each join
        for (juce::int64 p = rs; p < re; ++p)
        {
            const float w = (float) juce::jmin (1.0, (double) (p - rs + 1) / fade, (double) (re - p) / fade);
            float& o = out[(size_t) (p - newStart)];
            o = o * (1.f - w) + tk[(size_t) (p - takeStart)] * w;
        }
        rememberForUndo (t);
        setClip (t, c, newStart, saveWav (*c, t), takeBurned);
        return true;
    }

    juce::int64 songLength() const                                       // the end of the last clip, in samples
    {
        juce::int64 end = 0;
        for (int t = 0; t < num; ++t)
            if (auto c = getClip (t)) end = juce::jmax (end, tracks[(size_t) t].start.load() + (juce::int64) ((double) c->data.getNumSamples() * sampleRate / c->sampleRate));
        return end;
    }

    // ---- audio thread ----
    // mic: this block's audio input (already at the INPUT volume), or nullptr. Adds the tracks and the booth to out. Returns true if the mic should show on the IN meter.
    bool process (juce::AudioBuffer<float>& out, const float* mic, int n, bool running, juce::int64 hostSample, BoothChain& booth, double bpm)
    {
        n = juce::jmin (n, maxBlock);
        if (running)
        {
            if (hostSample >= 0) songPos = hostSample;
            else if (restart.exchange (false)) songPos = songStart.load();
        }
        const juce::int64 pos = songPos.load();
        float* raw = rawBus.data();
        float* burn = burnBus.data();
        std::fill (raw, raw + n, 0.f);
        std::fill (burn, burn + n, 0.f);
        float* L = out.getWritePointer (0);
        float* R = out.getNumChannels() > 1 ? out.getWritePointer (1) : L;
        const bool tuning = booth.tuneOn();
        const int tuneLat = tuning ? booth.tuneLatency() : 0;

        // ---- the recording pass
        const int arm = armed.load();
        const bool want = running && recEnabled.load() && arm >= 0 && mic != nullptr && take.getNumSamples() > 0;
        if (want && ! recording.load())
        {
            const bool hasOld = getClip (arm) != nullptr;
            takeLen = 0;  takeStart = pos;  takeTrack = arm;
            takeBurned = hasOld ? tracks[(size_t) arm].burned.load() : booth.burnOn();   // a punch into an existing take is recorded the same way that take was
            passMode = punchMode.load();
            if (passMode == 3 && ! hasOld) passMode = 0;                  // nothing to match against yet: record the whole take
            regionStart = -1;  regionEnd = -1;
            if (passMode == 0) regionStart = pos;
            if (passMode == 2) { regionStart = punchIn.load() >= 0 ? punchIn.load() : pos; regionEnd = punchOut.load() > regionStart.load() ? punchOut.load() : -1; }
            punchToggle = false;
            envLive = envOld = slowLive = slowOld = 0.f;  matchRun = 0;  quietPos = liveQuiet = matchStartPos = pos;
            recording = true;
        }
        if (recording.load())
        {
            if (! want)                                                   // the pass is over (Stop, or REC switched off)
            {
                if (regionStart.load() >= 0 && regionEnd.load() < 0) regionEnd = pos;
                recording = false;  takePending = true;  recEnabled = false;
            }
            else
            {
                const int at = takeLen.load(), room = take.getNumSamples() - at, len = juce::jmin (n, room);
                float* dst = take.getWritePointer (0, at);
                if (takeBurned) for (int i = 0; i < len; ++i) dst[i] = booth.burnTick (mic[i]);
                else            for (int i = 0; i < len; ++i) dst[i] = mic[i];
                takeLen = at + len;
                if (passMode == 1 && punchToggle.exchange (false))
                {
                    if (regionStart.load() < 0) regionStart = pos;
                    else if (regionEnd.load() < 0) regionEnd = pos;
                }
                if (len < n) { if (regionStart.load() >= 0 && regionEnd.load() < 0) regionEnd = pos + len; recording = false; takePending = true; recEnabled = false; }   // 5 minutes: full
            }
        }
        const bool pass = recording.load();

        // ---- monitoring: hear yourself through the booth (use headphones)
        if (mic != nullptr && arm >= 0 && booth.monitorOn())
        {
            float* live = liveBuf.data();
            std::copy (mic, mic + n, live);
            if (tracks[(size_t) arm].booth.load())
            {
                if (tuning) booth.tuneBlock (num, live, n);
                for (int i = 0; i < n; ++i) raw[i] += live[i];
            }
            else for (int i = 0; i < n; ++i) { L[i] += live[i]; if (R != L) R[i] += live[i]; }
        }

        // ---- playback
        bool anySolo = false;
        for (auto& t : tracks) anySolo = anySolo || t.solo.load();
        const float envC = 1.f - std::exp (-1.f / (0.03f * (float) sampleRate)), slowC = 1.f - std::exp (-1.f / (0.2f * (float) sampleRate));
        const int matchNeed = (int) (sampleRate * 0.25);
        for (int ti = 0; ti < num; ++ti)
        {
            auto& t = tracks[(size_t) ti];
            float pk = 0.f;
            const bool isTake = pass && ti == takeTrack;
            if (running && ((! t.mute.load() && (! anySolo || t.solo.load())) || isTake))
            {
                const juce::SpinLock::ScopedTryLockType lock (t.lock);
                if (lock.isLocked() && t.clip != nullptr)
                {
                    const float* d = t.clip->data.getReadPointer (0);
                    const int len = t.clip->data.getNumSamples();
                    const double ratio = t.clip->sampleRate / sampleRate;
                    const bool audible = ! t.mute.load() && (! anySolo || t.solo.load());
                    const float g = audible ? t.vol.load() : 0.f, pan = t.pan.load();
                    const float gl = g * std::cos ((pan + 1.f) * 0.25f * juce::MathConstants<float>::pi) * 1.41421356f,
                                gr = g * std::sin ((pan + 1.f) * 0.25f * juce::MathConstants<float>::pi) * 1.41421356f;
                    const bool toBooth = t.booth.load(), burned = t.burned.load(), tuneThis = toBooth && tuning;
                    const juce::int64 startPos = t.start.load();
                    const int ahead = tuneThis ? tuneLat : 0;                 // read early by the tuner's delay, so tuned vocals stay in time
                    float* tb = trackBuf.data();
                    const float tol = matchTolDb.load();
                    for (int i = 0; i < n; ++i)
                    {
                        const juce::int64 sp = pos + i;
                        const double p = (double) (sp + ahead - startPos) * ratio;
                        float s = 0.f;
                        if (p >= 0.0 && p < (double) (len - 1)) { const int idx = (int) p; s = d[idx] + (d[idx + 1] - d[idx]) * (float) (p - (double) idx); }

                        if (isTake)
                        {
                            if (passMode == 3 && regionStart.load() < 0)      // match punch: compare your level with the old take's level
                            {
                                const float ao = std::abs (s), al = std::abs (mic[i]);
                                envOld += envC * (ao - envOld);      envLive += envC * (al - envLive);       // quick: finds the gaps between phrases
                                slowOld += slowC * (ao - slowOld);   slowLive += slowC * (al - slowLive);    // slow: the overall loudness that gets compared
                                if (envOld < 0.004f) quietPos = sp;                                          // a gap in the old take
                                if (envLive < 0.004f) liveQuiet = sp;                                        // you were not rapping yet
                                const bool both = slowOld > 0.008f && slowLive > 0.008f;
                                if (both && std::abs (20.f * std::log10 (slowLive / slowOld)) < tol) { if (matchRun++ == 0) matchStartPos = sp; }
                                else matchRun = juce::jmax (0, matchRun - 1);
                                if (matchRun >= matchNeed)
                                {
                                    // drop in at the gap before this phrase, unless you came in later than that: then drop in where you started
                                    juce::int64 at = (matchStartPos - quietPos < (juce::int64) (sampleRate * 1.5)) ? quietPos : matchStartPos;
                                    if (liveQuiet - at > (juce::int64) (sampleRate * 0.15)) at = juce::jmin (liveQuiet, matchStartPos);
                                    else if (liveQuiet < at && at - liveQuiet < (juce::int64) (sampleRate * 0.3)) at = liveQuiet;   // you started a touch early, in the gap: keep that too
                                    regionStart = juce::jmax (at, takeStart);
                                }
                            }
                            if (isPunchedAt (sp)) s = 0.f;                    // punched in: the old take makes way
                        }
                        tb[i] = s * g;
                        pk = juce::jmax (pk, std::abs (tb[i]));
                    }
                    if (tuneThis) booth.tuneBlock (ti, tb, n);
                    if (toBooth) { float* bus = burned ? burn : raw; for (int i = 0; i < n; ++i) bus[i] += tb[i]; }   // the booth output is centred; pan applies to the straight tracks
                    else for (int i = 0; i < n; ++i) { L[i] += tb[i] * (g > 0.f ? gl / g : 0.f); if (R != L) R[i] += tb[i] * (g > 0.f ? gr / g : 0.f); }
                }
            }
            if (ti == arm && mic != nullptr) for (int i = 0; i < n; ++i) pk = juce::jmax (pk, std::abs (mic[i]));   // the armed track shows the input level
            t.peak = juce::jmax (pk, t.peak.load() * 0.9f);
        }
        matchLive = slowLive;
        matchOld = slowOld;

        booth.process (raw, burn, L, R != L ? R : L, n, bpm);
        if (running) songPos = pos + n;
        return arm >= 0 && mic != nullptr;
    }

private:
    void rememberForUndo (int t)
    {
        auto& tr = tracks[(size_t) t];
        tr.undoClip = getClip (t);
        tr.undoStart = tr.start.load();
        tr.hasUndo = true;
    }
    juce::String saveWav (const TrackClip& c, int t) const
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                             .getChildFile ("Basement Instruments").getChildFile ("Beat Station").getChildFile ("Recordings");
        dir.createDirectory();
        const auto file = dir.getChildFile ("Track" + juce::String (t + 1) + "-" + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".wav");
        if (auto out = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
        {
            juce::WavAudioFormat wav;
            if (auto* rawWriter = wav.createWriterFor (out.get(), c.sampleRate, 1, 24, {}, 0))
            {
                out.release();
                std::unique_ptr<juce::AudioFormatWriter> writer (rawWriter);
                if (writer->writeFromAudioSampleBuffer (c.data, 0, c.data.getNumSamples())) return file.getFullPathName();
            }
        }
        return {};
    }

    static constexpr int maxBlock = 16384;
    std::vector<float> rawBus, burnBus, trackBuf, liveBuf;
    juce::AudioBuffer<float> take;
    std::atomic<int> takeLen { 0 };
    juce::int64 takeStart = 0, quietPos = 0, liveQuiet = 0, matchStartPos = 0;
    int takeTrack = -1, passMode = 0, matchRun = 0;
    float envLive = 0.f, envOld = 0.f, slowLive = 0.f, slowOld = 0.f;
    bool takeBurned = false;
    double sampleRate = 44100.0;
};
