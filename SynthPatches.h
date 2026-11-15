#pragma once
#include <juce_core/juce_core.h>
#include <vector>

// The built-in Keys instruments. They are synthesized live (oscillators, FM, a filter and an envelope), so there are no sample files.
// These are simple approximations meant as a starting palette, not recordings of real instruments.
struct SynthPatch
{
    const char* name = "";
    int waveA = 2, waveB = 2;               // 0 sine, 1 triangle, 2 saw, 3 square, 4 narrow pulse
    float detuneCents = 0.f, levelB = 0.f, octB = 0.f;      // oscillator B: fine detune, level, offset in semitones
    float fmRatio = 0.f, fmIndex = 0.f, fmDecayMs = 300.f;  // FM on oscillator A (sine only): modulator ratio, strength, how fast it fades
    float sub = 0.f, noise = 0.f;
    float vibRate = 0.f, vibCents = 0.f;
    float attMs = 3.f, decMs = 300.f, sus = 1.f, relMs = 150.f;
    float cutoff = 12000.f, res = 0.7f, fltAmt = 0.f, fltDecMs = 300.f, keyTrack = 0.f;   // filter: base cutoff, resonance, envelope octaves, envelope decay, key tracking
    float pitchDrop = 0.f, pitchDropMs = 60.f;              // semitones the pitch falls from at the start (808 style)
    float gain = 0.5f;
    float drawbars[6] = { 0, 0, 0, 0, 0, 0 };               // organ: levels of harmonics 1, 2, 3, 4, 6, 8 (if any are set, oscillator A is replaced by these)
};

inline const std::vector<SynthPatch>& synthPatches()
{
    static const std::vector<SynthPatch> list = []
    {
        std::vector<SynthPatch> v;
        auto add = [&v] (const char* n, auto&& setup) { SynthPatch p; p.name = n; setup (p); v.push_back (p); };

        add ("Soft Piano",      [] (SynthPatch& p) { p.waveA = 1; p.waveB = 2; p.levelB = .3f; p.detuneCents = 3; p.attMs = 2; p.decMs = 900; p.sus = .15f; p.relMs = 300; p.cutoff = 2600; p.fltAmt = 2.5f; p.fltDecMs = 450; p.keyTrack = .6f; p.gain = .6f; });
        add ("Bright Piano",    [] (SynthPatch& p) { p.waveA = 2; p.waveB = 1; p.levelB = .4f; p.detuneCents = 4; p.attMs = 1; p.decMs = 700; p.sus = .15f; p.relMs = 250; p.cutoff = 4500; p.fltAmt = 3; p.fltDecMs = 350; p.keyTrack = .8f; p.gain = .5f; });
        add ("Electric Piano",  [] (SynthPatch& p) { p.waveA = 0; p.fmRatio = 1.f; p.fmIndex = 2.2f; p.fmDecayMs = 350; p.attMs = 2; p.decMs = 1400; p.sus = .3f; p.relMs = 300; p.cutoff = 9000; p.gain = .6f; });
        add ("Wurly Keys",      [] (SynthPatch& p) { p.waveA = 1; p.noise = .02f; p.attMs = 3; p.decMs = 800; p.sus = .3f; p.relMs = 200; p.cutoff = 4200; p.res = 1.f; p.fltAmt = 1.5f; p.fltDecMs = 250; p.gain = .6f; });
        add ("Drawbar Organ",   [] (SynthPatch& p) { p.drawbars[0] = 1.f; p.drawbars[1] = .8f; p.drawbars[2] = .6f; p.drawbars[3] = .4f; p.drawbars[4] = .3f; p.drawbars[5] = .2f; p.attMs = 3; p.sus = 1.f; p.relMs = 40; p.vibRate = 6.f; p.vibCents = 4; p.cutoff = 9000; p.gain = .3f; });
        add ("Church Organ",    [] (SynthPatch& p) { p.drawbars[0] = 1.f; p.drawbars[1] = .9f; p.drawbars[2] = .5f; p.drawbars[3] = .6f; p.drawbars[4] = .4f; p.drawbars[5] = .3f; p.waveB = 0; p.levelB = .5f; p.octB = -12; p.attMs = 25; p.sus = 1.f; p.relMs = 150; p.cutoff = 7000; p.gain = .28f; });
        add ("Warm Pad",        [] (SynthPatch& p) { p.waveA = 2; p.waveB = 2; p.levelB = .8f; p.detuneCents = 9; p.attMs = 600; p.decMs = 800; p.sus = .8f; p.relMs = 900; p.cutoff = 1400; p.res = .8f; p.fltAmt = .5f; p.fltDecMs = 1500; p.gain = .35f; });
        add ("String Ensemble", [] (SynthPatch& p) { p.waveA = 2; p.waveB = 2; p.levelB = .9f; p.detuneCents = 12; p.vibRate = 5.5f; p.vibCents = 6; p.attMs = 350; p.decMs = 400; p.sus = 1.f; p.relMs = 500; p.cutoff = 3200; p.gain = .32f; });
        add ("Analog Brass",    [] (SynthPatch& p) { p.waveA = 2; p.waveB = 2; p.levelB = .8f; p.detuneCents = 6; p.attMs = 70; p.decMs = 150; p.sus = .8f; p.relMs = 120; p.cutoff = 900; p.res = .9f; p.fltAmt = 3; p.fltDecMs = 220; p.keyTrack = .5f; p.gain = .4f; });
        add ("Synth Lead",      [] (SynthPatch& p) { p.waveA = 2; p.waveB = 3; p.levelB = .5f; p.detuneCents = 7; p.vibRate = 5.5f; p.vibCents = 8; p.attMs = 8; p.decMs = 100; p.sus = .8f; p.relMs = 150; p.cutoff = 4000; p.res = 1.5f; p.fltAmt = 1; p.fltDecMs = 300; p.gain = .35f; });
        add ("Square Lead",     [] (SynthPatch& p) { p.waveA = 3; p.waveB = 3; p.levelB = .25f; p.octB = 12; p.attMs = 5; p.sus = .9f; p.relMs = 120; p.cutoff = 5000; p.res = .8f; p.gain = .3f; });
        add ("Pluck",           [] (SynthPatch& p) { p.waveA = 2; p.waveB = 1; p.levelB = .5f; p.attMs = 1; p.decMs = 260; p.sus = 0.f; p.relMs = 120; p.cutoff = 600; p.res = 2.f; p.fltAmt = 4; p.fltDecMs = 200; p.gain = .5f; });
        add ("Glass Bell",      [] (SynthPatch& p) { p.waveA = 0; p.fmRatio = 3.5f; p.fmIndex = 4.f; p.fmDecayMs = 1200; p.attMs = 1; p.decMs = 2000; p.sus = 0.f; p.relMs = 500; p.cutoff = 12000; p.gain = .45f; });
        add ("Marimba",         [] (SynthPatch& p) { p.waveA = 0; p.fmRatio = 4.f; p.fmIndex = 1.5f; p.fmDecayMs = 120; p.attMs = 1; p.decMs = 450; p.sus = 0.f; p.relMs = 100; p.cutoff = 9000; p.gain = .6f; });
        add ("Sub Bass",        [] (SynthPatch& p) { p.waveA = 0; p.waveB = 0; p.levelB = .3f; p.octB = -12; p.attMs = 6; p.decMs = 100; p.sus = 1.f; p.relMs = 120; p.cutoff = 800; p.gain = .8f; });
        add ("Analog Bass",     [] (SynthPatch& p) { p.waveA = 2; p.sub = .6f; p.attMs = 3; p.decMs = 200; p.sus = .6f; p.relMs = 80; p.cutoff = 350; p.res = 2.5f; p.fltAmt = 3.5f; p.fltDecMs = 180; p.keyTrack = .3f; p.gain = .6f; });
        add ("808 Bass",        [] (SynthPatch& p) { p.waveA = 0; p.pitchDrop = 12; p.pitchDropMs = 60; p.attMs = 1; p.decMs = 1500; p.sus = .4f; p.relMs = 250; p.cutoff = 1200; p.gain = .9f; });
        add ("Flute",           [] (SynthPatch& p) { p.waveA = 0; p.noise = .06f; p.vibRate = 5.f; p.vibCents = 10; p.attMs = 80; p.sus = 1.f; p.relMs = 150; p.cutoff = 5000; p.gain = .5f; });
        add ("Choir Pad",       [] (SynthPatch& p) { p.waveA = 2; p.waveB = 2; p.levelB = .8f; p.detuneCents = 8; p.vibRate = 4.5f; p.vibCents = 5; p.attMs = 500; p.sus = 1.f; p.relMs = 800; p.cutoff = 1800; p.res = 3.f; p.gain = .32f; });
        add ("Funk Clav",       [] (SynthPatch& p) { p.waveA = 4; p.waveB = 2; p.levelB = .3f; p.noise = .02f; p.attMs = 1; p.decMs = 220; p.sus = .1f; p.relMs = 80; p.cutoff = 700; p.res = 1.8f; p.fltAmt = 4; p.fltDecMs = 140; p.keyTrack = .7f; p.gain = .5f; });
        add ("G-Funk Whine",    [] (SynthPatch& p) { p.waveA = 2; p.vibRate = 5.8f; p.vibCents = 15; p.attMs = 20; p.sus = 1.f; p.relMs = 200; p.cutoff = 2400; p.res = 1.2f; p.gain = .35f; });
        return v;
    }();
    return list;
}

// Index 0 plays the selected pad's sample pitched across the keyboard; the rest are the synthesized instruments above.
inline juce::StringArray keysInstrumentNames()
{
    juce::StringArray names;
    names.add ("Selected Pad Sample");
    for (const auto& p : synthPatches()) names.add (p.name);
    return names;
}
