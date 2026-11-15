#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "RoomPresets.h"

// Room Reverb inside Beat Station: the room::Engine from the stand-alone Room Reverb plugin, on the master chain after the EQ and compressor.
// It starts switched off. All its controls are normal parameters ("room_..."), so they save with the project and can be MIDI-learned.
namespace RoomID
{
    constexpr int numFloat = 23;
    // same order as room::Parameters is filled in settings()
    inline const char* const ids[numFloat] = { "room_width", "room_depth", "room_height", "room_sx", "room_sy", "room_sz", "room_lx", "room_ly", "room_lz",
                                               "room_decay", "room_predelay", "room_damping", "room_absorption", "room_diffusion", "room_spread",
                                               "room_wet", "room_direct", "room_early", "room_late", "room_duck", "room_output", "room_orientation", "room_lowcut" };
    inline const juce::String subject = "room_subject", on = "room_on", wetSolo = "room_wetsolo";
}

class RoomFx
{
public:
    static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& l)
    {
        using namespace juce;
        auto add = [&l] (const char* id, const char* title, float lo, float hi, float value, float skew, int kind)   // kind: 0 plain, 1 percent, 2 whole number
        {
            NormalisableRange<float> r (lo, hi);
            r.skew = skew;
            auto attrs = AudioParameterFloatAttributes()
                .withStringFromValueFunction ([kind] (float v, int) { return kind == 1 ? String (v * 100.f, 0) + "%" : String (v, kind == 2 ? 0 : 2); })
                .withValueFromStringFunction ([kind] (const String& t) { return t.getFloatValue() / (kind == 1 ? 100.f : 1.f); });
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, title, r, value, attrs));
        };
        add ("room_width", "Room Width (m)", 2.f, 25.f, 6.f, 1.f, 0);          add ("room_depth", "Room Depth (m)", 2.f, 35.f, 8.f, 1.f, 0);
        add ("room_height", "Room Height (m)", 2.f, 12.f, 3.f, 1.f, 0);
        add ("room_sx", "Room Source X", 0.05f, 0.95f, 0.25f, 1.f, 1);         add ("room_sy", "Room Source Y", 0.05f, 0.95f, 0.25f, 1.f, 1);
        add ("room_sz", "Room Source Height (m)", 0.2f, 3.f, 1.5f, 1.f, 0);
        add ("room_lx", "Room Listener X", 0.05f, 0.95f, 0.8f, 1.f, 1);        add ("room_ly", "Room Listener Y", 0.05f, 0.95f, 0.45f, 1.f, 1);
        add ("room_lz", "Room Listener Height (m)", 0.5f, 2.5f, 1.5f, 1.f, 0);
        add ("room_decay", "Room Decay (s)", 0.15f, 8.f, 1.2f, 0.45f, 0);      add ("room_predelay", "Room Pre-delay (ms)", 0.f, 120.f, 15.f, 1.f, 0);
        add ("room_damping", "Room Tail Tone (Hz)", 300.f, 16000.f, 6500.f, 0.4f, 2);
        add ("room_absorption", "Room Wall Absorption", 0.05f, 0.95f, 0.35f, 1.f, 1);
        add ("room_diffusion", "Room Diffusion", 0.f, 1.f, 0.8f, 1.f, 1);      add ("room_spread", "Room Stereo Width", 0.f, 1.f, 1.f, 1.f, 1);
        add ("room_wet", "Room Amount", 0.f, 1.f, 0.25f, 1.f, 1);              add ("room_direct", "Room Direct Level", 0.f, 1.f, 1.f, 1.f, 1);
        add ("room_early", "Room Early Reflections", 0.f, 1.f, 0.65f, 1.f, 1); add ("room_late", "Room Late Tail", 0.f, 1.f, 0.65f, 1.f, 1);
        add ("room_duck", "Room Ducking", 0.f, 1.f, 0.3f, 1.f, 1);             add ("room_output", "Room Output (dB)", -24.f, 12.f, 0.f, 1.f, 0);
        add ("room_orientation", "Room Source Facing (deg)", -180.f, 180.f, 0.f, 1.f, 2);
        add ("room_lowcut", "Room Low Cut (Hz)", 20.f, 600.f, 90.f, 0.5f, 2);
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { RoomID::subject, 1 }, "Room Subject",
                                                       StringArray { "Rapper", "Singer", "Drum Kit", "Tuba", "Acoustic Guitar", "Electric Guitar", "Piano", "Synth / Keys" }, 0));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { RoomID::on, 1 }, "Room On", false));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { RoomID::wetSolo, 1 }, "Room Hear Wet Only", false));
    }

    explicit RoomFx (juce::AudioProcessorValueTreeState& state) : apvts (state)
    {
        for (int i = 0; i < RoomID::numFloat; ++i) raw[i] = apvts.getRawParameterValue (RoomID::ids[i]);
        pSubject = apvts.getRawParameterValue (RoomID::subject);
        pOn = apvts.getRawParameterValue (RoomID::on);
        pSolo = apvts.getRawParameterValue (RoomID::wetSolo);
        (void) room::factoryPresets();                                  // built here, never on the audio side
    }

    room::Parameters settings() const
    {
        room::Parameters p;
        float v[RoomID::numFloat];
        for (int i = 0; i < RoomID::numFloat; ++i) v[i] = raw[i]->load (std::memory_order_relaxed);
        p.width = v[0]; p.depth = v[1]; p.height = v[2]; p.sx = v[3]; p.sy = v[4]; p.sz = v[5]; p.lx = v[6]; p.ly = v[7]; p.lz = v[8];
        p.decay = v[9]; p.predelay = v[10]; p.damping = v[11]; p.absorption = v[12]; p.diffusion = v[13]; p.spread = v[14];
        p.wet = v[15]; p.direct = v[16]; p.early = v[17]; p.late = v[18]; p.duck = v[19]; p.output = v[20]; p.orientation = v[21]; p.lowCut = v[22];
        p.subject = (int) pSubject->load();
        p.bypass = pOn->load() < 0.5f;
        p.wetSolo = pSolo->load() > 0.5f;
        return room::sanitise (p);
    }

    void prepare (double sr)
    {
        sampleRate = sr;
        engine.prepare (sr, settings());
        idleSamples = 0;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const auto p = settings();
        const int n = buffer.getNumSamples();
        if (p.bypass)                                                   // off: let it fade out, then stop working altogether
        {
            if (idleSamples > (int) sampleRate) { wetLevel.store (0.f); return; }
            idleSamples += n;
        }
        else
        {
            if (idleSamples > (int) sampleRate) engine.prepare (sampleRate, p);   // switched back on: start from silence (same buffer sizes, nothing is allocated)
            idleSamples = 0;
        }
        engine.setParameters (p);
        float* L = buffer.getWritePointer (0);
        float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
        for (int i = 0; i < n; ++i)
        {
            const auto y = engine.process (L[i], R != nullptr ? R[i] : L[i], R == nullptr);
            L[i] = y[0];
            if (R != nullptr) R[i] = y[1];
        }
        wetLevel.store (juce::jmax (engine.wetMeter[0].envelope, engine.wetMeter[1].envelope));
    }

    // ---- message thread ----
    void setValue (const juce::String& id, float v)
    {
        if (auto* prm = apvts.getParameter (id)) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); prm->endChangeGesture(); }
    }

    void loadPreset (int index)
    {
        const auto& presets = room::factoryPresets();
        if (index < 0 || index >= (int) presets.size()) return;
        const auto& p = presets[(size_t) index].values;
        const float v[22] = { p.width, p.depth, p.height, p.sx, p.sy, p.sz, p.lx, p.ly, p.lz, p.decay, p.predelay, p.damping, p.absorption, p.diffusion, p.spread,
                              p.wet, p.direct, p.early, p.late, p.duck, p.output, p.orientation };
        for (int i = 0; i < 22; ++i) setValue (RoomID::ids[i], v[i]);
        setValue (RoomID::subject, (float) p.subject);
        setValue (RoomID::on, 1.f);                                     // picking a room switches the reverb on so you hear it
        apvts.state.setProperty ("roomPreset", index, nullptr);
    }
    int selectedPreset() const { return (int) apvts.state.getProperty ("roomPreset", -1); }

    void selectMaterial (int m)
    {
        const float absorb[] = { 0.7f, 0.55f, 0.25f, 0.12f, 0.35f }, tone[] = { 3000.f, 4500.f, 8500.f, 12000.f, 6500.f };
        m = juce::jlimit (0, 4, m);
        setValue ("room_absorption", absorb[m]);
        setValue ("room_damping", tone[m]);
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::atomic<float> wetLevel { 0.f };

private:
    room::Engine engine;
    std::atomic<float>* raw[RoomID::numFloat] {};
    std::atomic<float> *pSubject = nullptr, *pOn = nullptr, *pSolo = nullptr;
    double sampleRate = 44100.0;
    int idleSamples = 0;
};
