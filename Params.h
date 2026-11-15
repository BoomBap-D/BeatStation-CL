#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SynthPatches.h"
#include "MasterFx.h"
#include "RoomFx.h"

constexpr int numPads = 16;      // phase 1: one bank. Banks B, C, D... come next.
constexpr int firstNote = 36;    // default MIDI note of pad 1 (the usual MPC layout, 36-51); each pad can be MIDI-learned to another note

enum PadParam { pLevel, pPan, pTune, pMode, pGroup, pRel, pFType, pFCut, pFRes, pFAmt, pAmpOn, pFltOn, numPadParams };

namespace ParamID
{
    inline juce::String level (int p) { return "p" + juce::String (p + 1) + "_level"; }
    inline juce::String pan   (int p) { return "p" + juce::String (p + 1) + "_pan"; }
    inline juce::String tune  (int p) { return "p" + juce::String (p + 1) + "_tune"; }
    inline juce::String mode  (int p) { return "p" + juce::String (p + 1) + "_mode"; }    // 0 Cut, 1 Layer, 2 Gate
    inline juce::String group (int p) { return "p" + juce::String (p + 1) + "_group"; }   // choke group, 0 = none
    inline juce::String rel   (int p) { return "p" + juce::String (p + 1) + "_rel"; }     // fade-out time (ms)
    inline juce::String fType (int p) { return "p" + juce::String (p + 1) + "_ft"; }      // 0 Off, 1 Low-pass, 2 High-pass, 3 Band-pass
    inline juce::String fCut  (int p) { return "p" + juce::String (p + 1) + "_fc"; }
    inline juce::String fRes  (int p) { return "p" + juce::String (p + 1) + "_fq"; }
    inline juce::String fAmt  (int p) { return "p" + juce::String (p + 1) + "_fe"; }      // how many octaves the filter envelope moves the cutoff
    inline juce::String ampOn (int p) { return "p" + juce::String (p + 1) + "_ao"; }      // amplitude envelope on/off
    inline juce::String fltOn (int p) { return "p" + juce::String (p + 1) + "_fo"; }      // filter envelope on/off
    inline const juce::String keysMode = "keys_mode", keysInst = "keys_inst";
    inline const juce::String bpm = "bpm", swing = "swing", seqLen = "seq_len";
    inline juce::String trackPad  (int t) { return "t" + juce::String (t + 1) + "_pad"; }
    inline juce::String trackMute (int t) { return "t" + juce::String (t + 1) + "_mute"; }
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    for (int p = 0; p < numPads; ++p)
    {
        const String n = "Pad " + String (p + 1);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::level (p), 1 }, n + " Level", NormalisableRange<float> (-60.f, 6.f, 0.1f), 0.f));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::pan (p), 1 },   n + " Pan",   NormalisableRange<float> (-1.f, 1.f, 0.01f), 0.f));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::tune (p), 1 },  n + " Tune",  NormalisableRange<float> (-12.f, 12.f, 0.01f), 0.f));
        // Cut: hitting the pad again restarts it. Layer: hits stack up. Gate: sound fades out when you let go.
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::mode (p), 1 }, n + " Mode", StringArray { "Cut", "Layer", "Gate" }, 0));
        // Pads sharing a choke group number (1-8) cut each other, e.g. closed and open hat.
        l.add (std::make_unique<AudioParameterInt> (ParameterID { ParamID::group (p), 1 }, n + " Choke", 0, 8, 0));
        // The amplitude and filter ENVELOPES are free-form point lists edited in the SHAPE tab and saved with the project.
        // Fade = how long the sound takes to fade out at the end of the chop (or when a Gate pad is released).
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::rel (p), 1 }, n + " Fade", NormalisableRange<float> (1.f, 5000.f, 0.1f, 0.35f), 30.f));
        // The filter starts as a fully open low-pass, so turning Cutoff down is heard straight away.
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::fType (p), 1 }, n + " Filter", StringArray { "Off", "Low-pass", "High-pass", "Band-pass" }, 1));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::fCut (p), 1 }, n + " Cutoff",    NormalisableRange<float> (20.f, 20000.f, 0.1f, 0.25f), 20000.f));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::fRes (p), 1 }, n + " Resonance", NormalisableRange<float> (0.5f, 12.f, 0.01f, 0.5f), 0.7f));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::fAmt (p), 1 }, n + " Filter Env Amount", NormalisableRange<float> (-6.f, 6.f, 0.01f), 0.f));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::ampOn (p), 1 }, n + " Amp Envelope On", true));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::fltOn (p), 1 }, n + " Filter Envelope On", false));
    }
    // Keys mode: MIDI notes play the selected pad's sample pitched across the keyboard (C3 = original pitch).
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::keysMode, 1 }, "Keys Mode", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::keysInst, 1 }, "Keys Instrument", keysInstrumentNames(), 1));

    // Step sequencer: tempo, swing, pattern length, and which pad each of the 4 tracks plays
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::bpm, 1 },   "BPM",   NormalisableRange<float> (40.f, 220.f, 0.1f), 92.f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::swing, 1 }, "Swing", NormalisableRange<float> (50.f, 75.f, 0.1f), 50.f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::seqLen, 1 }, "Pattern Length", StringArray { "16", "32" }, 0));
    StringArray padNames;
    for (int p = 0; p < numPads; ++p) padNames.add ("Pad " + String (p + 1));
    const int defaultPad[4] = { 0, 2, 4, 8 };                          // kick, snare, closed hat, 808
    for (int t = 0; t < 4; ++t)
    {
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::trackPad (t), 1 },  "Track " + String (t + 1) + " Pad", padNames, defaultPad[t]));
        l.add (std::make_unique<AudioParameterBool>   (ParameterID { ParamID::trackMute (t), 1 }, "Track " + String (t + 1) + " Mute", false));
    }
    MasterFx::addParameters (l);                                       // the master EQ and compressor
    RoomFx::addParameters (l);                                         // the room reverb
    return l;
}
