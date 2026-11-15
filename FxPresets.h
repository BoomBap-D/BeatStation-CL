#pragma once
#include "FxChain.h"

// One-click effect chains. Each line is: name, category, recipe.
// Recipe words:  +eq +comp +drive +filter +crush +trans +chorus +delay +room +width +limiter   switch that effect on (everything else is off)
//                order=a>b>c        the order the sound goes through them (anything not named follows in the usual order)
//                b1=freq/gain/q     EQ band 1-5 (1 = low shelf, 5 = high shelf)
//                c=threshold/ratio/attack/release/makeup      compressor
//                room=N             start from factory room number N
//                name=value         any other control, by its parameter name without the "fx_" (for example drive_amt=12)
struct FxPreset { const char* name; const char* category; const char* recipe; };

inline const std::vector<FxPreset>& fxPresets()
{
    static const std::vector<FxPreset> p = {
    // ---------------- Start Here
    { "Clean Slate (everything off)", "Start Here", "" },
    { "Instant Polish", "Start Here", "+eq +comp +limiter b1=70/1.5/0.7 b2=300/-1.5/1.2 b5=10000/2/0.7 c=-16/2.5/15/150/1.5 lim_gain=2 lim_ceil=-0.3" },
    { "Loud And Punchy", "Start Here", "+eq +comp +trans +limiter b1=60/2/0.7 b4=3500/1.5/1 b5=11000/2/0.7 c=-18/3/20/120/2 trans_attack=25 lim_gain=5 lim_ceil=-0.3 lim_rel=80" },
    { "Warm And Dusty", "Start Here", "+eq +drive +crush +comp b1=90/2/0.7 b5=8000/-3/0.7 drive_amt=8 drive_type=0 drive_tone=9000 crush_rate=26040 crush_bits=12 crush_cut=9000 c=-16/3/25/180/1" },

    // ---------------- Drums
    { "Kick - Deep Punch", "Drums", "+eq +trans +comp +limiter b1=55/4/0.9 b2=320/-4/1.4 b4=3200/2.5/1.2 trans_attack=35 trans_sustain=-15 c=-14/4/25/90/2 lim_ceil=-0.5" },
    { "Kick - Boom Bap Thump", "Drums", "+eq +drive +comp b1=80/3.5/0.8 b2=400/-3/1.2 b5=7000/-4/0.7 drive_amt=7 drive_type=0 drive_tone=8000 c=-15/3.5/30/140/1.5" },
    { "Kick - Trap Click", "Drums", "+eq +trans +drive +limiter b1=50/3/0.9 b2=250/-3/1.3 b4=4500/4/1.2 trans_attack=50 trans_sustain=-25 drive_amt=5 drive_type=2 lim_ceil=-0.5" },
    { "Snare - Crack", "Drums", "+eq +trans +comp b2=200/2/1 b3=900/-2.5/1.5 b4=5000/3.5/1 b5=11000/2/0.7 trans_attack=40 c=-16/4/8/110/2" },
    { "Snare - Fat Room", "Drums", "+eq +comp +drive +room b2=180/3/1 b4=3500/2/1.2 c=-18/5/12/160/3 drive_amt=6 drive_type=1 room=32 room_wet=0.18" },
    { "Clap - Wide Splash", "Drums", "+eq +room +width b1=150/-6/0.7 b4=4000/2.5/1 b5=10000/2/0.7 room=33 room_wet=0.22 wid_width=1.4 wid_mono=200" },
    { "Hi-Hats - Crisp", "Drums", "+eq +trans +width b1=300/-12/0.7 b4=7000/2/1 b5=12000/3/0.7 trans_attack=20 trans_sustain=-30 wid_width=1.25" },
    { "Hi-Hats - Dark Pocket", "Drums", "+eq +crush +room b1=250/-12/0.7 b5=8000/-4/0.7 crush_rate=22000 crush_bits=12 crush_cut=8000 room=35 room_wet=0.12" },
    { "Drum Bus - Glue", "Drums", "+eq +comp +drive +limiter b1=60/1.5/0.7 b5=10000/1.5/0.7 c=-14/2/30/100/1 cknee=10 drive_amt=3 drive_type=0 lim_ceil=-0.3" },
    { "Drum Bus - Smash (Parallel)", "Drums", "+comp +drive +eq order=comp>drive>eq c=-30/10/2/60/10 cmix=0.45 drive_amt=6 drive_type=0 drive_mix=0.5 b1=70/2/0.7 b5=9000/2/0.7" },
    { "Break - Warehouse", "Drums", "+eq +comp +room +limiter b1=80/2/0.7 b3=600/-2/1.2 c=-18/4/10/90/3 room=40 room_wet=0.2 lim_ceil=-0.3" },
    { "Percussion - Tight And Dry", "Drums", "+eq +trans b1=120/-8/0.7 b4=5000/2/1 trans_attack=30 trans_sustain=-45" },

    // ---------------- 808 & Bass
    { "808 - Clean Sub", "808 & Bass", "+eq +comp +width +limiter b1=45/3/0.8 b2=250/-3/1.2 b5=6000/-6/0.7 c=-12/3/40/200/1 wid_width=0 lim_ceil=-0.5" },
    { "808 - Distorted Growl", "808 & Bass", "+drive +eq +comp +limiter order=drive>eq>comp>limiter drive_amt=18 drive_type=1 drive_tone=6000 drive_mix=0.7 b1=50/2/0.8 b3=900/3/1 c=-10/4/20/150/0 lim_ceil=-0.5" },
    { "808 - Phone Speaker Proof", "808 & Bass", "+drive +eq +limiter drive_amt=14 drive_type=0 drive_tone=4000 drive_mix=0.6 b1=40/-2/0.7 b2=180/3/1 b3=700/3/1.2 lim_gain=2 lim_ceil=-0.5" },
    { "Bass - Round And Warm", "808 & Bass", "+eq +comp +drive +width b1=70/2.5/0.8 b3=800/-2/1.2 b5=5000/-5/0.7 c=-16/4/25/160/2 drive_amt=5 drive_type=1 wid_width=0.3 wid_mono=150" },

    // ---------------- Sampling & Lo-Fi
    { "SP-1200 Grit", "Sampling & Lo-Fi", "+crush +drive +eq order=drive>crush>eq crush_rate=26040 crush_bits=12 crush_cut=11000 drive_amt=6 drive_type=0 b1=80/2/0.7 b5=9000/2/0.7" },
    { "S950 Smooth Filter", "Sampling & Lo-Fi", "+crush +filter +eq order=crush>filter>eq crush_rate=30000 crush_bits=12 crush_cut=14000 filt_type=0 filt_cut=5500 filt_res=1.1 filt_slope=1 b1=90/2/0.7" },
    { "Dusty Vinyl Sample", "Sampling & Lo-Fi", "+eq +drive +crush +chorus +comp b1=70/-4/0.7 b2=250/2/1 b5=7000/-5/0.7 drive_amt=7 drive_type=1 drive_tone=7000 crush_rate=24000 crush_bits=11 crush_cut=8000 chor_rate=0.35 chor_depth=0.6 chor_delay=8 chor_mix=0.2 c=-18/3/20/200/2" },
    { "Cassette Tape", "Sampling & Lo-Fi", "+drive +chorus +eq +comp drive_amt=10 drive_type=0 drive_tone=6500 chor_rate=0.8 chor_depth=0.9 chor_delay=6 chor_mix=0.3 b1=60/-3/0.7 b5=6000/-4/0.7 c=-20/3/30/250/3" },
    { "Lo-Fi Beat Tape", "Sampling & Lo-Fi", "+eq +crush +drive +chorus +comp +room b1=80/2/0.7 b5=5500/-6/0.7 crush_rate=18000 crush_bits=10 crush_cut=6000 drive_amt=6 drive_type=0 chor_rate=0.4 chor_depth=1.2 chor_delay=9 chor_mix=0.25 c=-20/4/25/220/3 room=91 room_wet=0.12" },
    { "8-Bit Crunch", "Sampling & Lo-Fi", "+crush +limiter crush_rate=9000 crush_bits=6 crush_cut=9000 lim_ceil=-1" },
    { "Low-Pass The Sample (bass only)", "Sampling & Lo-Fi", "+filter +comp filt_type=0 filt_cut=220 filt_res=0.9 filt_slope=1 c=-18/4/30/180/4" },
    { "High-Pass The Sample (make room for drums)", "Sampling & Lo-Fi", "+filter +eq +width filt_type=1 filt_cut=280 filt_res=0.71 filt_slope=1 b4=3000/1.5/1 wid_width=1.3" },

    // ---------------- Vocals
    { "Rap Lead - Upfront", "Vocals", "+eq +comp +drive +room +limiter b1=90/-12/0.7 b2=300/-2.5/1.3 b4=4000/2.5/1 b5=11000/3/0.7 c=-20/4/6/90/4 drive_amt=3 drive_type=1 room=12 room_wet=0.12 lim_ceil=-0.5" },
    { "Rap Lead - Boom Bap Dry", "Vocals", "+eq +comp +drive +room b1=100/-12/0.7 b2=250/-2/1.2 b3=1500/1.5/1 b5=9000/1.5/0.7 c=-18/5/10/120/4 drive_amt=5 drive_type=0 drive_tone=10000 room=13 room_wet=0.1" },
    { "Trap Lead - Bright And Tight", "Vocals", "+eq +comp +delay +room +limiter b1=110/-12/0.7 b2=350/-3/1.4 b4=5000/3/1 b5=12000/4/0.7 c=-22/6/3/70/5 dly_sync=4 dly_fb=0.2 dly_mix=0.1 dly_highcut=4000 room=14 room_wet=0.14 lim_ceil=-0.5" },
    { "Drill Vocal - Concrete", "Vocals", "+eq +comp +drive +room b1=100/-12/0.7 b2=280/-3/1.3 b4=3500/3/1.1 c=-22/6/4/80/5 drive_amt=4 drive_type=2 room=15 room_wet=0.16" },
    { "Ad-Libs - Wide And Far", "Vocals", "+eq +delay +room +width b1=200/-12/0.7 b3=1000/-2/1 b5=10000/3/0.7 dly_sync=5 dly_fb=0.3 dly_ping=1 dly_mix=0.22 room=18 room_wet=0.3 wid_width=1.5" },
    { "Hook - Big And Lifted", "Vocals", "+eq +comp +chorus +delay +room b1=100/-12/0.7 b4=4500/2/1 b5=12000/3.5/0.7 c=-20/4/8/110/4 chor_rate=0.5 chor_depth=1.5 chor_delay=14 chor_mix=0.18 dly_sync=2 dly_fb=0.25 dly_mix=0.12 room=19 room_wet=0.22" },
    { "R&B Vocal - Silky", "Vocals", "+eq +comp +chorus +room b1=90/-12/0.7 b2=250/-2/1.2 b5=13000/4/0.7 c=-22/3/12/160/4 chor_rate=0.4 chor_depth=1 chor_delay=16 chor_mix=0.12 room=21 room_wet=0.25" },
    { "Telephone Voice", "Vocals", "+filter +drive +eq order=eq>filter>drive b1=300/-12/0.7 b5=3500/-12/0.7 filt_type=2 filt_cut=1400 filt_res=1.2 filt_slope=0 drive_amt=14 drive_type=2 drive_tone=4000" },

    // ---------------- Keys & Melody
    { "Keys - Warm Rhodes", "Keys & Melody", "+eq +drive +chorus +room b1=80/1.5/0.7 b3=2500/-1.5/1 drive_amt=5 drive_type=1 chor_rate=0.7 chor_depth=2 chor_delay=14 chor_mix=0.3 room=66 room_wet=0.2" },
    { "Piano - Soul Sample Feel", "Keys & Melody", "+eq +crush +drive +room b1=100/-3/0.7 b2=400/2/1 b5=7000/-4/0.7 crush_rate=28000 crush_bits=12 crush_cut=9000 drive_amt=5 drive_type=0 room=22 room_wet=0.2" },
    { "Pad - Wide And Dreamy", "Keys & Melody", "+eq +chorus +delay +room +width b1=150/-6/0.7 b5=11000/2/0.7 chor_rate=0.3 chor_depth=4 chor_delay=18 chor_mix=0.45 dly_sync=3 dly_fb=0.4 dly_ping=1 dly_mix=0.2 room=67 room_wet=0.35 wid_width=1.6 wid_mono=180" },
    { "Synth Lead - West Coast Whine", "Keys & Melody", "+eq +drive +delay +room b1=200/-8/0.7 b4=3000/2/1 drive_amt=6 drive_type=1 dly_sync=4 dly_fb=0.3 dly_mix=0.18 dly_highcut=5000 room=68 room_wet=0.15" },
    { "Pluck - Trap Bells", "Keys & Melody", "+eq +delay +room +width b1=250/-10/0.7 b5=12000/3/0.7 dly_sync=5 dly_fb=0.45 dly_ping=1 dly_mix=0.25 room=25 room_wet=0.3 wid_width=1.4" },
    { "Guitar Loop - Moody", "Keys & Melody", "+eq +filter +chorus +room b1=120/-6/0.7 filt_type=0 filt_cut=4500 filt_res=0.8 filt_slope=0 chor_rate=0.5 chor_depth=2.5 chor_delay=12 chor_mix=0.3 room=55 room_wet=0.3" },

    // ---------------- Mix Bus
    { "Mix Bus - Gentle Glue", "Mix Bus", "+eq +comp b1=50/1/0.7 b5=12000/1/0.7 c=-12/1.8/30/150/0.5 cknee=12" },
    { "Mix Bus - Boom Bap Weight", "Mix Bus", "+eq +drive +comp +width b1=70/2/0.7 b2=350/-1.5/1 b5=9000/1/0.7 drive_amt=4 drive_type=0 drive_tone=14000 c=-14/2.5/30/200/1 wid_width=0.9 wid_mono=120" },
    { "Mix Bus - Trap Wide And Bright", "Mix Bus", "+eq +comp +width +limiter b1=45/2/0.8 b2=300/-2/1.2 b5=12000/2.5/0.7 c=-14/2/20/100/1 wid_width=1.3 wid_mono=140 lim_ceil=-0.3" },
    { "Mix Bus - West Coast Bounce", "Mix Bus", "+eq +comp +drive +width b1=60/2.5/0.8 b4=3000/1.5/1 b5=11000/2/0.7 c=-15/3/25/120/1.5 drive_amt=3 drive_type=1 wid_width=1.15" },
    { "Mix Bus - R&B Smooth", "Mix Bus", "+eq +comp +room +width b1=60/1.5/0.7 b3=2500/-1/1 b5=13000/2/0.7 c=-16/2/30/250/1 room=70 room_wet=0.08 wid_width=1.2 wid_mono=120" },

    // ---------------- Mastering
    { "Master - Clean Streaming", "Mastering", "+eq +comp +limiter b1=35/-2/0.7 b5=12000/1/0.7 c=-12/1.6/30/200/0.5 cknee=12 lim_gain=3 lim_ceil=-1 lim_rel=200" },
    { "Master - Loud Trap", "Mastering", "+eq +comp +drive +width +limiter b1=45/2/0.8 b2=280/-1.5/1.2 b5=11000/2/0.7 c=-14/2/20/100/1 drive_amt=3 drive_type=2 drive_mix=0.5 wid_width=1.15 wid_mono=130 lim_gain=7 lim_ceil=-0.3 lim_rel=60" },
    { "Master - Drill Heavy Low End", "Mastering", "+eq +comp +width +limiter b1=50/3/0.9 b2=220/-2/1.2 b4=4000/1.5/1 c=-13/2.5/25/90/1 wid_width=1.1 wid_mono=160 lim_gain=6 lim_ceil=-0.3 lim_rel=70" },
    { "Master - Boom Bap Warm", "Mastering", "+eq +drive +comp +limiter b1=75/2/0.7 b5=8500/-1.5/0.7 drive_amt=5 drive_type=0 drive_tone=15000 c=-14/2.5/30/220/1 lim_gain=4 lim_ceil=-0.5 lim_rel=180" },
    { "Master - Lo-Fi Mellow", "Mastering", "+eq +crush +drive +comp +limiter b1=70/1.5/0.7 b5=6500/-4/0.7 crush_rate=30000 crush_bits=13 crush_cut=10000 drive_amt=5 drive_type=0 c=-16/3/30/250/2 lim_gain=3 lim_ceil=-1" },
    { "Master - Club System", "Mastering", "+eq +comp +width +limiter b1=40/3/0.9 b2=150/-1.5/1.2 b5=10000/1.5/0.7 c=-12/2/30/120/1 wid_width=1 wid_mono=180 lim_gain=6 lim_ceil=-0.3 lim_rel=90" },
    { "Master - Just A Safety Limiter", "Mastering", "+limiter lim_gain=0 lim_ceil=-0.3 lim_rel=120" },

    // ---------------- Creative & DJ
    { "DJ Filter - Slow Sweep", "Creative & DJ", "+filter filt_type=0 filt_cut=1200 filt_res=2.5 filt_slope=1 filt_rate=0.12 filt_depth=3" },
    { "DJ Filter - High-Pass Build", "Creative & DJ", "+filter +room filt_type=1 filt_cut=600 filt_res=3 filt_slope=1 room=88 room_wet=0.25" },
    { "Dub Echo Throw", "Creative & DJ", "+delay +filter +room order=filter>delay>room dly_sync=3 dly_fb=0.65 dly_ping=1 dly_lowcut=300 dly_highcut=3000 dly_mix=0.45 filt_type=1 filt_cut=120 filt_slope=0 room=79 room_wet=0.15" },
    { "Jet Flanger", "Creative & DJ", "+chorus chor_rate=0.15 chor_depth=1.8 chor_delay=2.2 chor_fb=0.8 chor_mix=0.5" },
    { "Radio In The Next Room", "Creative & DJ", "+filter +crush +room +width filt_type=2 filt_cut=900 filt_res=0.9 filt_slope=0 crush_rate=12000 crush_bits=10 crush_cut=5000 room=91 room_wet=0.45 wid_width=0.3" },
    { "Underwater", "Creative & DJ", "+filter +chorus +room filt_type=0 filt_cut=500 filt_res=2 filt_slope=1 filt_rate=0.25 filt_depth=0.8 chor_rate=0.3 chor_depth=5 chor_delay=20 chor_mix=0.5 room=81 room_wet=0.4" },
    { "Stadium Shout", "Creative & DJ", "+eq +delay +room +limiter b1=150/-6/0.7 b4=3000/3/1 dly_time=140 dly_sync=0 dly_fb=0.3 dly_mix=0.3 room=94 room_wet=0.4 lim_ceil=-0.5" },
    };
    return p;
}

// Applies a preset: resets every effect control to its default, then follows the recipe. Message thread only.
inline void applyFxPreset (juce::AudioProcessorValueTreeState& apvts, FxChain& chain, RoomFx& room, int index)
{
    const auto& all = fxPresets();
    if (index < 0 || index >= (int) all.size()) return;

    auto setTo = [&apvts] (const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (value)); p->endChangeGesture(); return true; }
        return false;
    };
    for (auto* base : apvts.processor.getParameters())                  // back to defaults: all fx_ and mfx_ controls, and the room switched off
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (base))
            if (p->paramID.startsWith ("fx_") || p->paramID.startsWith ("mfx_")) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
    for (int i = 0; i < numFx; ++i) setTo (FxInfo::onIds[i], 0.f);
    setTo ("room_wetsolo", 0.f);
    chain.setOrderFromString ({});

    for (const auto& tok : juce::StringArray::fromTokens (juce::String (all[(size_t) index].recipe), " ", ""))
    {
        if (tok.isEmpty()) continue;
        if (tok.startsWithChar ('+'))
        {
            for (int i = 0; i < numFx; ++i) if (tok.substring (1) == FxInfo::keys[i]) setTo (FxInfo::onIds[i], 1.f);
            continue;
        }
        const auto key = tok.upToFirstOccurrenceOf ("=", false, false), val = tok.fromFirstOccurrenceOf ("=", false, false);
        const auto parts = juce::StringArray::fromTokens (val, "/", "");
        if (key == "order") chain.setOrderFromString (val);
        else if (key == "room") room.loadPreset (val.getIntValue());   // also switches the room on
        else if (key.length() == 2 && key[0] == 'b' && parts.size() == 3)
        {
            const int band = key.substring (1).getIntValue() - 1;
            if (band >= 0 && band < MfxID::numBands) { setTo (MfxID::freq (band), parts[0].getFloatValue()); setTo (MfxID::gain (band), parts[1].getFloatValue()); setTo (MfxID::q (band), parts[2].getFloatValue()); }
        }
        else if (key == "c" && parts.size() == 5)
        {
            setTo (MfxID::thresh, parts[0].getFloatValue());   setTo (MfxID::ratio, parts[1].getFloatValue());   setTo (MfxID::attack, parts[2].getFloatValue());
            setTo (MfxID::release, parts[3].getFloatValue());  setTo (MfxID::makeup, parts[4].getFloatValue());
        }
        else if (key == "cknee") setTo (MfxID::knee, val.getFloatValue());
        else if (key == "cmix")  setTo (MfxID::mix, val.getFloatValue());
        else if (! (key.startsWith ("room_") ? setTo (key, val.getFloatValue()) : setTo ("fx_" + key, val.getFloatValue())))
            jassertfalse;                                               // a recipe names a control that does not exist
    }
    apvts.state.setProperty ("fxPreset", index, nullptr);
}
