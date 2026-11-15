#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "MasterDsp.h"

// THE MASTERING STATION: the last thing the whole mix goes through, after the master fader.
//   EQ > GLUE > MULTIBAND > COLOR > IMAGER > CLIPPER > LIMITER   (the limiter is always last)
// The loudness (LUFS) and true-peak meters always run, even with the station switched off, so you can see where the mix stands.
enum MasterStage { mEq, mGlue, mMulti, mColor, mImager, mClip, mLimit, numMaster };

namespace MasterInfo
{
    inline const char* const names[numMaster] = { "EQ", "GLUE", "MULTIBAND", "COLOR", "IMAGER", "CLIPPER", "LIMITER" };
    inline const char* const keys[numMaster]  = { "eq", "glue", "mb", "col", "img", "clip", "lim" };
    inline const char* const onIds[numMaster] = { "mst_eq_on", "mst_glue_on", "mst_mb_on", "mst_col_on", "mst_img_on", "mst_clip_on", "mst_lim_on" };
    inline const juce::Colour colours[numMaster] = { juce::Colour (0xfff5a524), juce::Colour (0xffe0568c), juce::Colour (0xff3ad0e0), juce::Colour (0xffff8a5c),
                                                     juce::Colour (0xff6fd36f), juce::Colour (0xffece0c2), juce::Colour (0xffff5a4d) };
    inline const juce::StringArray targetNames { "Streaming  -14 LUFS", "Apple Music  -16 LUFS", "Loud hip-hop  -9 LUFS", "Club  -7 LUFS" };
    inline const float targetLufs[4] = { -14.f, -16.f, -9.f, -7.f };
}

class MasterStation
{
public:
    static constexpr int numP = 41;
    static constexpr const char* const ids[numP] = {
        "mst_eq_lowf", "mst_eq_low", "mst_eq_mudf", "mst_eq_mud", "mst_eq_presf", "mst_eq_pres", "mst_eq_airf", "mst_eq_air", "mst_eq_sidecut", "mst_eq_sideair",   // 0-9
        "mst_glue_thresh", "mst_glue_ratio", "mst_glue_attack", "mst_glue_release", "mst_glue_makeup", "mst_glue_schp", "mst_glue_mix",                              // 10-16
        "mst_mb_xlow", "mst_mb_xhigh", "mst_mb_t1", "mst_mb_t2", "mst_mb_t3", "mst_mb_r1", "mst_mb_r2", "mst_mb_r3", "mst_mb_g1", "mst_mb_g2", "mst_mb_g3",           // 17-27
        "mst_col_warmth", "mst_col_air",                                                                                                                                // 28-29
        "mst_img_low", "mst_img_mid", "mst_img_high",                                                                                                                   // 30-32
        "mst_clip_ceil", "mst_clip_soft",                                                                                                                               // 33-34
        "mst_lim_gain", "mst_lim_ceil", "mst_lim_rel",                                                                                                                  // 35-37
        "mst_img_xlow", "mst_img_xhigh", "mst_out" };                                                                                                                   // 38-40

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
        b ("mst_on", "Mastering On", false);
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { "mst_target", 1 }, "Mastering Target", MasterInfo::targetNames, 0));
        b ("mst_eq_on", "Master EQ On", true);
        f ("mst_eq_lowf", "Master EQ Low Freq", 40.f, 200.f, 80.f, 0.6f, 1.f);          f ("mst_eq_low", "Master EQ Low", -6.f, 6.f, 0.f, 1.f, 0.1f);
        f ("mst_eq_mudf", "Master EQ Low-Mid Freq", 150.f, 600.f, 300.f, 0.6f, 1.f);    f ("mst_eq_mud", "Master EQ Low-Mid", -6.f, 6.f, 0.f, 1.f, 0.1f);
        f ("mst_eq_presf", "Master EQ Presence Freq", 1500.f, 6000.f, 3000.f, 0.6f, 1.f); f ("mst_eq_pres", "Master EQ Presence", -6.f, 6.f, 0.f, 1.f, 0.1f);
        f ("mst_eq_airf", "Master EQ Air Freq", 6000.f, 16000.f, 12000.f, 0.6f, 1.f);   f ("mst_eq_air", "Master EQ Air", -6.f, 6.f, 0.f, 1.f, 0.1f);
        f ("mst_eq_sidecut", "Master EQ Side Low Cut", 20.f, 300.f, 20.f, 0.5f, 1.f);   f ("mst_eq_sideair", "Master EQ Side Air", -6.f, 6.f, 0.f, 1.f, 0.1f);
        b ("mst_glue_on", "Master Glue On", false);
        f ("mst_glue_thresh", "Master Glue Threshold", -40.f, 0.f, -14.f, 1.f, 0.1f);   f ("mst_glue_ratio", "Master Glue Ratio", 1.f, 6.f, 2.f, 0.6f, 0.1f);
        f ("mst_glue_attack", "Master Glue Attack", 0.1f, 60.f, 30.f, 0.5f, 0.1f);      f ("mst_glue_release", "Master Glue Release", 50.f, 1200.f, 250.f, 0.5f, 1.f);
        f ("mst_glue_makeup", "Master Glue Makeup", 0.f, 12.f, 0.f, 1.f, 0.1f);         f ("mst_glue_schp", "Master Glue Side-chain Low Cut", 20.f, 250.f, 90.f, 0.6f, 1.f);
        f ("mst_glue_mix", "Master Glue Mix", 0.f, 1.f, 1.f, 1.f, 0.01f);
        b ("mst_mb_on", "Master Multiband On", false);
        f ("mst_mb_xlow", "Master Multiband Low Split", 60.f, 400.f, 120.f, 0.6f, 1.f); f ("mst_mb_xhigh", "Master Multiband High Split", 1000.f, 8000.f, 2500.f, 0.6f, 1.f);
        for (int k = 1; k <= 3; ++k)
        {
            static const char* band[3] = { "Low", "Mid", "High" };
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { "mst_mb_t" + String (k), 1 }, String ("Master Multiband ") + band[k - 1] + " Threshold", NormalisableRange<float> (-50.f, 0.f, 0.1f), -18.f));
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { "mst_mb_r" + String (k), 1 }, String ("Master Multiband ") + band[k - 1] + " Ratio", NormalisableRange<float> (1.f, 8.f, 0.1f), 2.f));
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { "mst_mb_g" + String (k), 1 }, String ("Master Multiband ") + band[k - 1] + " Gain", NormalisableRange<float> (-6.f, 6.f, 0.1f), 0.f));
        }
        b ("mst_col_on", "Master Color On", false);
        f ("mst_col_warmth", "Master Warmth", 0.f, 1.f, 0.3f, 1.f, 0.01f);              f ("mst_col_air", "Master Air", 0.f, 1.f, 0.3f, 1.f, 0.01f);
        b ("mst_img_on", "Master Imager On", false);
        f ("mst_img_low", "Master Width Low", 0.f, 2.f, 0.5f, 1.f, 0.01f);              f ("mst_img_mid", "Master Width Mid", 0.f, 2.f, 1.f, 1.f, 0.01f);
        f ("mst_img_high", "Master Width High", 0.f, 2.f, 1.2f, 1.f, 0.01f);
        f ("mst_img_xlow", "Master Imager Low Split", 80.f, 500.f, 250.f, 0.6f, 1.f);   f ("mst_img_xhigh", "Master Imager High Split", 1500.f, 8000.f, 4000.f, 0.6f, 1.f);
        b ("mst_clip_on", "Master Clipper On", false);
        f ("mst_clip_ceil", "Master Clipper Ceiling", -6.f, 0.f, -1.f, 1.f, 0.1f);      f ("mst_clip_soft", "Master Clipper Softness", 0.f, 1.f, 0.5f, 1.f, 0.01f);
        b ("mst_lim_on", "Master Limiter On", true);
        f ("mst_lim_gain", "Master Limiter Gain", 0.f, 18.f, 0.f, 1.f, 0.1f);           f ("mst_lim_ceil", "Master Limiter Ceiling", -3.f, 0.f, -1.f, 1.f, 0.1f);
        f ("mst_lim_rel", "Master Limiter Release", 20.f, 1000.f, 150.f, 0.5f, 1.f);
        f ("mst_out", "Mastering Input Trim", -12.f, 12.f, 0.f, 1.f, 0.1f);
    }

    explicit MasterStation (juce::AudioProcessorValueTreeState& state) : apvts (state)
    {
        for (int i = 0; i < numP; ++i) raw[i] = apvts.getRawParameterValue (ids[i]);
        for (int i = 0; i < numMaster; ++i) pOn[i] = apvts.getRawParameterValue (MasterInfo::onIds[i]);
        pMain = apvts.getRawParameterValue ("mst_on");
        pTarget = apvts.getRawParameterValue ("mst_target");
    }
    ~MasterStation() { stopBounce(); }

    void prepare (double sr, int blockSize)
    {
        sampleRate = sr;
        eq.prepare (sr); glue.prepare (sr); multi.prepare (sr); color.prepare (sr); imager.prepare (sr); clip.prepare (sr); limiter.prepare (sr);
        loud.prepare (sr);
        tp[0].reset(); tp[1].reset();
        ditherBuf.setSize (2, juce::jmax (blockSize * 2, 8192));
    }

    bool isOn() const { return pMain->load() > 0.5f; }
    bool moduleOn (int m) const { return pOn[m]->load() > 0.5f; }
    float v (int p) const { return raw[p]->load (std::memory_order_relaxed); }
    float targetLufs() const { return MasterInfo::targetLufs[juce::jlimit (0, 3, (int) pTarget->load())]; }
    double getSampleRate() const { return sampleRate; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2) return;
        const int n = buffer.getNumSamples();
        float* L = buffer.getWritePointer (0);
        float* R = buffer.getWritePointer (1);

        if (isOn())
        {
            const float trim = juce::Decibels::decibelsToGain (v (40));
            if (std::abs (trim - 1.f) > 1.0e-4f) buffer.applyGain (0, n, trim);
            if (moduleOn (mEq))
            {
                eq.lowF = v (0); eq.lowDb = v (1); eq.mudF = v (2); eq.mudDb = v (3); eq.presF = v (4); eq.presDb = v (5); eq.airF = v (6); eq.airDb = v (7); eq.sideCut = v (8); eq.sideAir = v (9);
                eq.update();
                eq.process (L, R, n);
            }
            if (moduleOn (mGlue))
            {
                glue.threshDb = v (10); glue.ratio = v (11); glue.attackMs = v (12); glue.releaseMs = v (13); glue.makeupDb = v (14); glue.scHp = v (15); glue.mix = v (16);
                glue.process (L, R, n);
                grGlue = glue.grNow;
            } else grGlue = 0.f;
            if (moduleOn (mMulti))
            {
                multi.xLow = v (17); multi.xHigh = v (18);
                for (int b = 0; b < 3; ++b) { multi.thresh[b] = v (19 + b); multi.ratio[b] = v (22 + b); multi.gain[b] = v (25 + b); }
                multi.process (L, R, n);
                for (int b = 0; b < 3; ++b) grMulti[b] = multi.grNow[b];
            } else for (auto& g : grMulti) g = 0.f;
            if (moduleOn (mColor)) { color.warmth = v (28); color.air = v (29); color.process (L, R, n); }
            if (moduleOn (mImager))
            {
                imager.width[0] = v (30); imager.width[1] = v (31); imager.width[2] = v (32); imager.xLow = v (38); imager.xHigh = v (39);
                imager.process (L, R, n);
            }
            if (moduleOn (mClip)) { clip.ceilingDb = v (33); clip.soft = v (34); clip.process (L, R, n); clipDb = clip.clippedDb; } else clipDb = 0.f;
            if (moduleOn (mLimit)) { limiter.gainDb = v (35); limiter.ceilingDb = v (36); limiter.releaseMs = v (37); limiter.process (L, R, n); grLimit = limiter.grDb; } else grLimit = 0.f;
        }
        else { grGlue = 0.f; grLimit = 0.f; clipDb = 0.f; for (auto& g : grMulti) g = 0.f; }

        // ---- meters (always)
        if (resetMeters.exchange (false)) { loud.reset(); tpMax = 0.f; }
        loud.process (L, R, n);
        float pk = tpMax.load();
        for (int i = 0; i < n; ++i) pk = juce::jmax (pk, tp[0].tick (L[i]), tp[1].tick (R[i]));
        tpMax = pk;
        lufsM = loud.momentary;  lufsS = loud.shortTerm;  lufsI = loud.integrated;

        // ---- bounce to a file: exactly what leaves the station
        if (writing.load())
        {
            const juce::ScopedTryLock lock (writerLock);
            if (lock.isLocked() && writer != nullptr)
            {
                if (bounce16 && ditherBuf.getNumSamples() >= n)           // 16-bit files get dither; what you hear is not changed
                {
                    float* dl = ditherBuf.getWritePointer (0);
                    float* dr = ditherBuf.getWritePointer (1);
                    for (int i = 0; i < n; ++i) { dl[i] = dither[0].tick (L[i]); dr[i] = dither[1].tick (R[i]); }
                    const float* chans[2] = { dl, dr };
                    writer->write (chans, n);
                }
                else writer->write (buffer.getArrayOfReadPointers(), n);
                bounced += n;
            }
        }
    }

    // ---- bounce (message thread). The audio is handed to a background thread that writes the file, so recording never stalls the sound.
    bool startBounce (const juce::File& file, bool sixteenBit)
    {
        stopBounce();
        file.deleteFile();
        auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream());
        if (stream == nullptr) return false;
        juce::WavAudioFormat wav;
        auto* w = wav.createWriterFor (stream.get(), sampleRate, 2, sixteenBit ? 16 : 24, {}, 0);
        if (w == nullptr) return false;
        stream.release();
        if (! thread.isThreadRunning()) thread.startThread();
        auto tw = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (w, thread, 65536);
        {
            const juce::ScopedLock lock (writerLock);
            bounce16 = sixteenBit;
            bounced = 0;
            writer = std::move (tw);
        }
        bounceFile = file;
        writing = true;
        return true;
    }
    void stopBounce()
    {
        writing = false;
        std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> old;
        { const juce::ScopedLock lock (writerLock); old = std::move (writer); }
        old.reset();                                                     // finishes writing and closes the file
    }
    bool isBouncing() const { return writing.load(); }
    double bouncedSeconds() const { return (double) bounced.load() / sampleRate; }
    juce::File bounceFile;

    // ---- read by the screen
    std::atomic<float> lufsM { -100.f }, lufsS { -100.f }, lufsI { -100.f }, tpMax { 0.f }, grGlue { 0.f }, grLimit { 0.f }, clipDb { 0.f };
    std::atomic<float> grMulti[3] {};
    std::atomic<bool> resetMeters { false };
    juce::AudioProcessorValueTreeState& apvts;
    fxdsp::MasterEq shownEq;                                             // a copy for drawing the curve (message thread only)

private:
    std::atomic<float>* raw[numP] {};
    std::atomic<float>* pOn[numMaster] {};
    std::atomic<float> *pMain = nullptr, *pTarget = nullptr;
    fxdsp::MasterEq eq;  fxdsp::GlueComp glue;  fxdsp::Multiband multi;  fxdsp::Color color;  fxdsp::Imager imager;  fxdsp::Clipper clip;  fxdsp::MasterLimiter limiter;
    fxdsp::LoudnessMeter loud;
    fxdsp::TruePeak tp[2];
    fxdsp::Dither dither[2];
    juce::AudioBuffer<float> ditherBuf;
    juce::TimeSliceThread thread { "Beat Station bounce" };
    juce::CriticalSection writerLock;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
    std::atomic<bool> writing { false };
    std::atomic<juce::int64> bounced { 0 };
    bool bounce16 = false;
    double sampleRate = 44100.0;
};

// ---------------------------------------------------------------- one-click masters
// Recipe words: +eq +glue +mb +col +img +clip +lim switch a stage on (everything else off); name=value sets a control (without "mst_"); target=N picks the loudness target.
// The limiter gain in a preset is a starting point: play the song, then press MATCH TARGET to set it for this mix.
struct MasterPreset { const char* name; const char* category; const char* recipe; };

inline const std::vector<MasterPreset>& masterPresets()
{
    static const std::vector<MasterPreset> p = {
    { "Clean Slate (limiter only at -1 dBTP)", "Start Here", "+lim lim_ceil=-1 target=0" },
    { "Streaming Master - Clean", "Start Here", "+eq +glue +lim eq_low=0.5 eq_air=1 eq_sidecut=90 glue_thresh=-14 glue_ratio=1.5 glue_attack=30 glue_release=300 lim_gain=3 lim_ceil=-1 target=0" },
    { "Loud Hip-Hop Master", "Start Here", "+eq +glue +col +img +clip +lim eq_low=1.5 eq_mud=-1 eq_air=1.5 eq_sidecut=120 glue_thresh=-14 glue_ratio=2 glue_schp=120 col_warmth=0.3 col_air=0.3 img_low=0.2 img_high=1.2 clip_ceil=-1.5 clip_soft=0.4 lim_gain=7 lim_ceil=-1 lim_rel=80 target=2" },

    { "Trap - Loud, Wide, Heavy 808", "Hip-Hop", "+eq +glue +mb +col +img +clip +lim eq_lowf=55 eq_low=2 eq_mud=-1.5 eq_pres=1 eq_air=2 eq_sidecut=130 glue_thresh=-13 glue_ratio=2 glue_attack=30 glue_release=200 glue_schp=140 mb_t1=-16 mb_r1=3 mb_t3=-20 mb_r3=2 col_warmth=0.25 col_air=0.4 img_low=0 img_mid=1 img_high=1.35 clip_ceil=-1.5 clip_soft=0.3 lim_gain=8 lim_ceil=-1 lim_rel=60 target=2" },
    { "Drill - Sliding 808, Cold Top", "Hip-Hop", "+eq +glue +mb +img +clip +lim eq_lowf=50 eq_low=2.5 eq_mud=-2 eq_pres=1.5 eq_air=1 eq_sidecut=150 glue_thresh=-13 glue_ratio=2.5 glue_schp=150 mb_t1=-15 mb_r1=3.5 mb_g1=0.5 img_low=0 img_high=1.2 clip_ceil=-1.5 clip_soft=0.25 lim_gain=7 lim_ceil=-1 lim_rel=70 target=2" },
    { "Boom Bap - Warm And Punchy", "Hip-Hop", "+eq +glue +col +clip +lim eq_lowf=75 eq_low=1.5 eq_mud=-0.5 eq_airf=9000 eq_air=-1 eq_sidecut=100 glue_thresh=-15 glue_ratio=2.5 glue_attack=30 glue_release=250 glue_schp=90 col_warmth=0.55 col_air=0.1 clip_ceil=-2 clip_soft=0.6 lim_gain=5 lim_ceil=-1 lim_rel=180 target=2" },
    { "West Coast - Bounce And Shine", "Hip-Hop", "+eq +glue +col +img +lim eq_lowf=60 eq_low=2 eq_pres=1 eq_air=1.5 eq_sidecut=110 glue_thresh=-14 glue_ratio=3 glue_attack=25 glue_release=180 glue_schp=100 col_warmth=0.3 col_air=0.35 img_low=0.2 img_high=1.25 lim_gain=6 lim_ceil=-1 lim_rel=120 target=2" },
    { "Southern - Thick Low End", "Hip-Hop", "+eq +glue +mb +col +lim eq_lowf=50 eq_low=2.5 eq_mud=-1 eq_air=1 eq_sidecut=120 glue_thresh=-14 glue_ratio=2 glue_schp=130 mb_t1=-18 mb_r1=3 mb_g1=1 col_warmth=0.45 lim_gain=6 lim_ceil=-1 lim_rel=120 target=2" },
    { "Melodic Trap - Glossy", "Hip-Hop", "+eq +glue +col +img +clip +lim eq_low=1.5 eq_mud=-1.5 eq_air=2.5 eq_sideair=1.5 eq_sidecut=120 glue_thresh=-15 glue_ratio=2 glue_schp=120 col_warmth=0.2 col_air=0.55 img_low=0.1 img_high=1.45 clip_ceil=-1.5 lim_gain=7 lim_ceil=-1 lim_rel=90 target=2" },
    { "Lo-Fi - Mellow And Rounded", "Hip-Hop", "+eq +glue +col +lim eq_low=1 eq_airf=8000 eq_air=-3 glue_thresh=-18 glue_ratio=3 glue_attack=40 glue_release=350 col_warmth=0.7 col_air=0 lim_gain=3 lim_ceil=-1 lim_rel=250 target=0" },
    { "Old School - Hard Drums", "Hip-Hop", "+eq +glue +clip +lim eq_low=1 eq_pres=1.5 eq_airf=10000 eq_air=0.5 glue_thresh=-16 glue_ratio=4 glue_attack=20 glue_release=150 glue_schp=80 clip_ceil=-2.5 clip_soft=0.2 lim_gain=5 lim_ceil=-1 target=2" },

    { "R&B - Smooth And Open", "Other Styles", "+eq +glue +col +img +lim eq_low=1 eq_pres=-0.5 eq_air=2 eq_sideair=1 eq_sidecut=90 glue_thresh=-16 glue_ratio=1.8 glue_attack=40 glue_release=400 col_warmth=0.3 col_air=0.35 img_low=0.3 img_high=1.3 lim_gain=4 lim_ceil=-1 lim_rel=250 target=0" },
    { "Pop - Bright And Polished", "Other Styles", "+eq +glue +mb +col +img +lim eq_low=0.5 eq_mud=-1 eq_pres=1 eq_air=2.5 eq_sidecut=100 glue_thresh=-14 glue_ratio=2 mb_t2=-20 mb_r2=1.8 col_air=0.45 col_warmth=0.15 img_high=1.3 lim_gain=6 lim_ceil=-1 lim_rel=120 target=0" },
    { "Afrobeats / Dancehall - Clean Punch", "Other Styles", "+eq +glue +img +clip +lim eq_low=1.5 eq_pres=1 eq_air=1.5 eq_sidecut=110 glue_thresh=-14 glue_ratio=2 glue_schp=110 img_low=0.2 img_high=1.25 clip_ceil=-1.5 lim_gain=6 lim_ceil=-1 target=0" },
    { "Club System - Mono-Safe And Loud", "Other Styles", "+eq +glue +img +clip +lim eq_lowf=45 eq_low=2.5 eq_mud=-1 eq_sidecut=180 glue_thresh=-13 glue_ratio=2 glue_schp=150 img_low=0 img_mid=0.9 img_high=1 clip_ceil=-1 clip_soft=0.3 lim_gain=9 lim_ceil=-0.5 lim_rel=70 target=3" },

    { "Fix - Mix Is Too Dark", "Problem Solvers", "+eq +col +lim eq_mud=-1.5 eq_pres=1.5 eq_air=3 col_air=0.5 col_warmth=0 lim_ceil=-1 target=0" },
    { "Fix - Mix Is Too Harsh", "Problem Solvers", "+eq +mb +col +lim eq_presf=3500 eq_pres=-2 eq_airf=9000 eq_air=-1.5 mb_t3=-24 mb_r3=3 col_warmth=0.4 col_air=0 lim_ceil=-1 target=0" },
    { "Fix - Muddy Low Mids", "Problem Solvers", "+eq +mb +lim eq_mudf=280 eq_mud=-3 eq_low=0.5 mb_t2=-22 mb_r2=2 lim_ceil=-1 target=0" },
    { "Fix - Bass Is Out Of Control", "Problem Solvers", "+eq +mb +img +lim eq_sidecut=160 mb_xlow=140 mb_t1=-22 mb_r1=4 img_low=0 lim_ceil=-1 target=0" },
    { "Fix - Too Narrow", "Problem Solvers", "+eq +img +lim eq_sideair=2 eq_sidecut=110 img_low=0.3 img_mid=1.15 img_high=1.6 lim_ceil=-1 target=0" },
    { "Gentle Glue Only", "Problem Solvers", "+glue +lim glue_thresh=-14 glue_ratio=1.5 glue_attack=30 glue_release=300 glue_schp=90 lim_ceil=-1 target=0" },
    };
    return p;
}

inline void applyMasterPreset (juce::AudioProcessorValueTreeState& apvts, int index)
{
    const auto& all = masterPresets();
    if (index < 0 || index >= (int) all.size()) return;
    auto setTo = [&apvts] (const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (value)); p->endChangeGesture(); return true; }
        return false;
    };
    for (auto* base : apvts.processor.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (base))
            if (p->paramID.startsWith ("mst_") && p->paramID != "mst_on") { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
    for (int i = 0; i < numMaster; ++i) setTo (MasterInfo::onIds[i], 0.f);
    for (const auto& tok : juce::StringArray::fromTokens (juce::String (all[(size_t) index].recipe), " ", ""))
    {
        if (tok.isEmpty()) continue;
        if (tok.startsWithChar ('+')) { for (int i = 0; i < numMaster; ++i) if (tok.substring (1) == MasterInfo::keys[i]) setTo (MasterInfo::onIds[i], 1.f); continue; }
        const auto key = tok.upToFirstOccurrenceOf ("=", false, false);
        if (! setTo ("mst_" + key, tok.fromFirstOccurrenceOf ("=", false, false).getFloatValue())) jassertfalse;
    }
    setTo ("mst_on", 1.f);                                              // choosing a master switches the station on
    apvts.state.setProperty ("masterPreset", index, nullptr);
}
