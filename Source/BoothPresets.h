#pragma once
#include "Booth.h"

// One-click vocal chains for the Booth. Recipe words:
//   mic=N                      microphone character (0 = off, 1-10 as listed in the Booth)
//   +gate +deess +comp +eq +drive +dbl +dly +rev     switch that effect on (everything else is off)
//   order=a>b>c                the order of the chain
//   comp=threshold/ratio/attack/release/makeup       eq=lowcut/mud/presence/air
//   name=value                 any other control, by its parameter name without "bth_"
// Starting points follow common engineering practice: low cut around 80-100 Hz, a 2-4 dB dip in the 250-450 Hz boxy area,
// about 4:1 fast compression for rap, de-essing in the 5-8 kHz range, a 2-3 dB presence lift at 3-5 kHz, a short plate and a 1/8 or 1/4 note delay.
struct BoothPreset { const char* name; const char* category; const char* recipe; };

inline const std::vector<BoothPreset>& boothPresets()
{
    static const std::vector<BoothPreset> p = {
    // ================= HIP-HOP (30)
    { "Modern Rap Lead - Clean And Bright", "Hip-Hop: Leads", "mic=1 +deess +comp +eq +rev comp=-22/4/3/60/5 eq=90/-3/2.5/3 deess_freq=6500 deess_max=5 rev_decay=0.9 rev_mix=0.08" },
    { "Trap Lead - Upfront", "Hip-Hop: Leads", "mic=1 +gate +deess +comp +eq +dly +rev gate_thresh=-52 comp=-24/6/2/50/6 eq=100/-3.5/3/4 deess_max=6 dly_sync=3 dly_fb=0.2 dly_mix=0.08 rev_decay=0.8 rev_mix=0.07" },
    { "Trap Lead - Dark And Heavy", "Hip-Hop: Leads", "mic=3 +deess +comp +eq +drive +rev comp=-24/6/3/60/6 eq=90/-2.5/2/1 drive_amt=5 drive_mix=0.4 rev_decay=1.1 rev_mix=0.1" },
    { "Boom Bap Lead - Dry And Warm", "Hip-Hop: Leads", "mic=3 +deess +comp +eq +drive comp=-20/4/5/80/4 eq=85/-2.5/2/0.5 drive_amt=6 drive_mix=0.5 deess_max=4" },
    { "Boom Bap Lead - 90s Grit", "Hip-Hop: Leads", "mic=8 +comp +eq +drive order=eq>comp>drive comp=-22/5/4/70/5 eq=100/-2/3/-1 drive_amt=9 drive_mix=0.6" },
    { "Drill Lead - Tight And Cold", "Hip-Hop: Leads", "mic=10 +gate +deess +comp +eq +rev gate_thresh=-48 gate_rel=90 comp=-24/6/2/45/6 eq=100/-3.5/3.5/2.5 deess_max=6 rev_decay=0.6 rev_predelay=10 rev_mix=0.09" },
    { "West Coast Lead - Smooth And Present", "Hip-Hop: Leads", "mic=2 +deess +comp +eq +dbl +rev comp=-20/4/4/70/4 eq=90/-2.5/2.5/2 dbl_mix=0.15 dbl_depth=1 rev_decay=0.9 rev_mix=0.08" },
    { "Southern Lead - Big And Thick", "Hip-Hop: Leads", "mic=3 +deess +comp +eq +drive +dly comp=-22/5/4/70/5 eq=80/-2/2.5/1.5 drive_amt=5 drive_mix=0.4 dly_sync=1 dly_fb=0.15 dly_mix=0.07" },
    { "Aggressive Lead - In Your Face", "Hip-Hop: Leads", "mic=8 +gate +deess +comp +eq +drive gate_thresh=-46 comp=-26/8/1.5/40/7 eq=110/-3/4/1 drive_amt=8 drive_mix=0.5 deess_max=6" },
    { "Deep Voice Lead - Weight With Clarity", "Hip-Hop: Leads", "mic=5 +deess +comp +eq +rev comp=-21/4/5/80/4 eq=75/-4/3/2.5 rev_decay=1 rev_mix=0.07" },
    { "High Voice Lead - Tame The Top", "Hip-Hop: Leads", "mic=7 +deess +comp +eq +rev deess_freq=7000 deess_thresh=-34 deess_max=8 comp=-21/4/4/70/4 eq=110/-2/1/-1 rev_decay=0.9 rev_mix=0.08" },
    { "Fast Flow - Every Syllable", "Hip-Hop: Leads", "mic=10 +gate +deess +comp +eq gate_thresh=-50 gate_rel=70 comp=-24/5/1/35/6 eq=100/-3.5/4/2 deess_max=5" },

    { "Ad-Libs - Wide And Wet", "Hip-Hop: Ad-Libs & Doubles", "mic=1 +comp +eq +dbl +dly +rev comp=-24/5/3/60/4 eq=180/-3/3/3 dbl_mix=0.45 dbl_depth=2.5 dly_sync=4 dly_fb=0.3 dly_ping=1 dly_mix=0.22 rev_decay=1.6 rev_mix=0.22 out=-4" },
    { "Ad-Libs - Telephone Pop-Outs", "Hip-Hop: Ad-Libs & Doubles", "mic=8 +comp +eq +drive +dly comp=-26/8/1/40/6 eq=300/-6/8/-6 drive_amt=14 drive_mix=0.8 dly_sync=3 dly_fb=0.25 dly_mix=0.15 out=-5" },
    { "Ad-Libs - Far Back Shouts", "Hip-Hop: Ad-Libs & Doubles", "mic=4 +comp +eq +rev comp=-22/4/5/80/3 eq=200/-3/1/-1 rev_decay=2.2 rev_predelay=30 rev_mix=0.4 out=-6" },
    { "Doubles - Tight Under The Lead", "Hip-Hop: Ad-Libs & Doubles", "mic=2 +deess +comp +eq +dbl deess_max=7 comp=-26/6/2/50/4 eq=140/-4/1/-1 dbl_mix=0.3 dbl_depth=1.2 out=-6" },
    { "Stacks - Thick Gang Vocals", "Hip-Hop: Ad-Libs & Doubles", "mic=3 +comp +eq +dbl +rev comp=-24/5/4/70/4 eq=120/-3/2/1 dbl_mix=0.6 dbl_depth=3 rev_decay=1.2 rev_mix=0.14 out=-3" },
    { "Whisper Layer - Air Only", "Hip-Hop: Ad-Libs & Doubles", "mic=6 +deess +comp +eq +dbl deess_max=8 comp=-30/6/2/60/8 eq=280/-6/2/8 dbl_mix=0.4 dbl_depth=2 out=-6" },

    { "Melodic Rap - Shiny Hook", "Hip-Hop: Hooks & Melodic", "mic=1 +deess +comp +eq +dbl +dly +rev comp=-23/4/4/70/5 eq=100/-3/2.5/4.5 dbl_mix=0.2 dbl_depth=1.5 dly_sync=1 dly_fb=0.25 dly_mix=0.12 rev_decay=1.6 rev_predelay=30 rev_mix=0.16" },
    { "Melodic Trap - Dreamy Wash", "Hip-Hop: Hooks & Melodic", "mic=5 +deess +comp +eq +dbl +dly +rev comp=-23/4/5/90/5 eq=110/-3/2/4 dbl_mix=0.3 dbl_depth=2.2 dly_sync=2 dly_fb=0.4 dly_ping=1 dly_mix=0.18 rev_decay=2.6 rev_predelay=40 rev_mix=0.24" },
    { "Sung Hook - Big And Lifted", "Hip-Hop: Hooks & Melodic", "mic=5 +deess +comp +eq +dbl +rev comp=-22/4/5/90/5 eq=95/-2.5/2.5/4 dbl_mix=0.35 dbl_depth=2 rev_decay=1.8 rev_predelay=35 rev_mix=0.2" },
    { "Chant Hook - Crowd Energy", "Hip-Hop: Hooks & Melodic", "mic=8 +comp +eq +drive +dbl +rev comp=-26/6/2/50/5 eq=120/-3/3.5/1 drive_amt=6 drive_mix=0.5 dbl_mix=0.6 dbl_depth=3.5 rev_decay=1.4 rev_mix=0.18" },
    { "R&B Rap Blend - Silky Verse", "Hip-Hop: Hooks & Melodic", "mic=5 +deess +comp +eq +dly +rev comp=-21/3/8/120/4 eq=90/-2.5/1.5/4 dly_sync=3 dly_fb=0.2 dly_mix=0.08 rev_decay=1.5 rev_predelay=30 rev_mix=0.14" },
    { "Emo Rap - Close And Raw", "Hip-Hop: Hooks & Melodic", "mic=4 +deess +comp +eq +drive +rev comp=-24/5/3/70/5 eq=100/-2/2.5/2 drive_amt=6 drive_mix=0.45 rev_decay=1.3 rev_mix=0.15" },
    { "Hum And Gibberish Sketch (melody first)", "Hip-Hop: Hooks & Melodic", "mic=2 +comp +eq +rev comp=-22/3/10/120/4 eq=90/-2/1.5/1.5 rev_decay=1.2 rev_mix=0.14" },

    { "Lo-Fi Verse - Dusty Tape", "Hip-Hop: Character", "mic=9 +comp +eq +drive order=drive>eq>comp comp=-22/4/8/120/4 eq=120/-1/1/-5 drive_amt=12 drive_mix=0.7" },
    { "Old School Party Mic", "Hip-Hop: Character", "mic=8 +comp +eq +dly +rev comp=-20/4/5/90/4 eq=110/-2/3/-2 dly_sync=3 dly_fb=0.3 dly_mix=0.14 rev_decay=1 rev_mix=0.14" },
    { "Radio Voice Intro", "Hip-Hop: Character", "mic=8 +gate +comp +eq +drive comp=-28/10/1/40/8 eq=250/-5/6/-6 drive_amt=10 drive_mix=0.7 gate_thresh=-45" },
    { "Dub Throw Verse End", "Hip-Hop: Character", "mic=2 +comp +eq +dly +rev order=comp>eq>dly>rev comp=-22/4/4/70/4 eq=100/-3/2.5/2 dly_sync=2 dly_fb=0.6 dly_ping=1 dly_mix=0.35 rev_decay=2 rev_mix=0.15" },
    { "Spoken Intro - Intimate", "Hip-Hop: Character", "mic=7 +gate +deess +comp +eq +rev gate_thresh=-55 comp=-24/3/10/150/5 eq=70/-2/1.5/1.5 rev_decay=0.7 rev_mix=0.06" },

    // ================= OTHER STYLES (20)
    { "R&B Lead - Silk", "R&B & Soul", "mic=5 +deess +comp +eq +dly +rev comp=-21/3/10/140/4 eq=85/-2.5/2/4.5 deess_max=5 dly_sync=1 dly_fb=0.2 dly_mix=0.08 rev_decay=1.8 rev_predelay=35 rev_mix=0.18" },
    { "R&B Lead - Modern Bright", "R&B & Soul", "mic=1 +deess +comp +eq +dbl +rev comp=-22/4/5/90/5 eq=95/-3/2/5 dbl_mix=0.15 dbl_depth=1.2 rev_decay=1.5 rev_predelay=30 rev_mix=0.15" },
    { "Soul - Warm Tube", "R&B & Soul", "mic=3 +deess +comp +eq +drive +rev comp=-20/3/12/160/4 eq=80/-2/1.5/1 drive_amt=4 drive_mix=0.4 rev_decay=1.6 rev_mix=0.16" },
    { "Neo-Soul - Close And Soft", "R&B & Soul", "mic=7 +deess +comp +eq +rev comp=-22/3/15/180/4 eq=75/-1.5/1/1.5 rev_decay=1.2 rev_mix=0.12" },
    { "Harmony Stack - Smooth Bed", "R&B & Soul", "mic=5 +deess +comp +eq +dbl +rev deess_max=8 comp=-26/5/5/100/4 eq=150/-4/0/3 dbl_mix=0.5 dbl_depth=2.5 rev_decay=2.2 rev_mix=0.25 out=-5" },

    { "Pop Lead - Radio Ready", "Pop", "mic=1 +deess +comp +eq +dbl +dly +rev comp=-23/4/4/70/6 eq=100/-3/3/5 deess_max=6 dbl_mix=0.12 dbl_depth=1 dly_sync=1 dly_fb=0.2 dly_mix=0.08 rev_decay=1.4 rev_predelay=30 rev_mix=0.13" },
    { "Pop Hook - Wide And Glossy", "Pop", "mic=6 +deess +comp +eq +dbl +rev comp=-24/5/4/70/6 eq=110/-3/3/5.5 dbl_mix=0.45 dbl_depth=2.4 rev_decay=1.9 rev_predelay=35 rev_mix=0.2" },
    { "Ballad - Long And Tender", "Pop", "mic=5 +deess +comp +eq +rev comp=-21/3/12/180/4 eq=80/-2/1.5/3.5 rev_decay=2.8 rev_predelay=45 rev_mix=0.24" },
    { "Indie Pop - Breath And Air", "Pop", "mic=6 +deess +comp +eq +dbl +rev comp=-26/4/6/100/7 eq=120/-3/1.5/6 dbl_mix=0.25 dbl_depth=1.8 rev_decay=1.6 rev_mix=0.16" },

    { "Rock Lead - Cut Through Guitars", "Rock & Alternative", "mic=8 +comp +eq +drive +dly comp=-24/6/3/60/6 eq=110/-2.5/4.5/1 drive_amt=7 drive_mix=0.5 dly_sync=3 dly_fb=0.2 dly_mix=0.1" },
    { "Alt Rock - Gritty Slap", "Rock & Alternative", "mic=4 +comp +eq +drive +dly comp=-24/5/4/70/5 eq=100/-2/3/0 drive_amt=10 drive_mix=0.6 dly_sync=6 dly_fb=0.1 dly_mix=0.2" },
    { "Screamer - Controlled Power", "Rock & Alternative", "mic=8 +gate +comp +eq +drive gate_thresh=-42 comp=-28/10/1/50/6 eq=130/-3/3/-2 drive_amt=8 drive_mix=0.5" },

    { "Dancehall Lead - Sharp And Loud", "Reggae, Dancehall & Afrobeats", "mic=10 +deess +comp +eq +dly +rev comp=-24/6/2/50/6 eq=100/-3/3.5/3 dly_sync=3 dly_fb=0.25 dly_mix=0.12 rev_decay=1 rev_mix=0.1" },
    { "Reggae - Dub Echo Voice", "Reggae, Dancehall & Afrobeats", "mic=3 +comp +eq +dly +rev order=comp>eq>dly>rev comp=-21/4/6/100/4 eq=90/-2/2/1 dly_sync=2 dly_fb=0.55 dly_mix=0.28 rev_decay=1.8 rev_mix=0.16" },
    { "Afrobeats Lead - Smooth Bounce", "Reggae, Dancehall & Afrobeats", "mic=1 +deess +comp +eq +dbl +dly +rev comp=-22/4/5/80/5 eq=95/-3/2.5/4 dbl_mix=0.18 dbl_depth=1.4 dly_sync=4 dly_fb=0.25 dly_ping=1 dly_mix=0.12 rev_decay=1.3 rev_mix=0.13" },

    { "Gospel Lead - Power And Room", "Gospel, Country, Jazz & Spoken", "mic=3 +deess +comp +eq +rev comp=-20/3/10/150/4 eq=80/-2/2.5/2.5 rev_decay=2.2 rev_predelay=30 rev_mix=0.2" },
    { "Country Lead - Honest And Clear", "Gospel, Country, Jazz & Spoken", "mic=2 +deess +comp +eq +dly +rev comp=-21/3/10/140/4 eq=90/-2.5/2.5/2.5 dly_sync=6 dly_fb=0.1 dly_mix=0.1 rev_decay=1.3 rev_mix=0.12" },
    { "Jazz - Ribbon Crooner", "Gospel, Country, Jazz & Spoken", "mic=9 +comp +eq +rev comp=-18/2.5/20/200/3 eq=60/-1/1.5/2 rev_decay=1.6 rev_predelay=25 rev_mix=0.16" },
    { "Podcast And Voice-Over", "Gospel, Country, Jazz & Spoken", "mic=8 +gate +deess +comp +eq gate_thresh=-50 comp=-22/4/5/100/5 eq=80/-2/2.5/1 deess_max=5" },
    { "Clean Reference (mic only)", "Gospel, Country, Jazz & Spoken", "mic=2" },
    };
    return p;
}

inline void applyBoothPreset (juce::AudioProcessorValueTreeState& apvts, BoothChain& booth, int index)
{
    const auto& all = boothPresets();
    if (index < 0 || index >= (int) all.size()) return;
    auto setTo = [&apvts] (const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (value)); p->endChangeGesture(); return true; }
        return false;
    };
    for (auto* base : apvts.processor.getParameters())                  // back to defaults, but leave Burn In, Monitor and TUNE as the user set them
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (base))
            if (p->paramID.startsWith ("bth_") && p->paramID != "bth_burn" && p->paramID != "bth_monitor" && ! p->paramID.startsWith ("bth_tune_"))   // TUNE belongs to the song (its key), so presets leave it alone
            { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
    for (int i = 0; i < numBooth; ++i) setTo (BoothInfo::onIds[i], 0.f);
    booth.setOrderFromString ({});

    for (const auto& tok : juce::StringArray::fromTokens (juce::String (all[(size_t) index].recipe), " ", ""))
    {
        if (tok.isEmpty()) continue;
        if (tok.startsWithChar ('+'))
        {
            for (int i = 0; i < numBooth; ++i) if (tok.substring (1) == BoothInfo::keys[i]) setTo (BoothInfo::onIds[i], 1.f);
            continue;
        }
        const auto key = tok.upToFirstOccurrenceOf ("=", false, false), val = tok.fromFirstOccurrenceOf ("=", false, false);
        const auto parts = juce::StringArray::fromTokens (val, "/", "");
        if (key == "order") booth.setOrderFromString (val);
        else if (key == "comp" && parts.size() == 5)
        {
            setTo ("bth_comp_thresh", parts[0].getFloatValue());   setTo ("bth_comp_ratio", parts[1].getFloatValue());   setTo ("bth_comp_attack", parts[2].getFloatValue());
            setTo ("bth_comp_release", parts[3].getFloatValue());  setTo ("bth_comp_makeup", parts[4].getFloatValue());
        }
        else if (key == "eq" && parts.size() == 4)
        {
            setTo ("bth_eq_lowcut", parts[0].getFloatValue());  setTo ("bth_eq_mud", parts[1].getFloatValue());
            setTo ("bth_eq_pres", parts[2].getFloatValue());    setTo ("bth_eq_air", parts[3].getFloatValue());
        }
        else if (! setTo ("bth_" + key, val.getFloatValue())) jassertfalse;
    }
    apvts.state.setProperty ("boothPreset", index, nullptr);
}
