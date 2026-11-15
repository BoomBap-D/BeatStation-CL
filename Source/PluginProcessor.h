#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "SampleLibrary.h"

struct Sample
{
    juce::AudioBuffer<float> data;      // always stereo
    double sampleRate = 44100.0;
    juce::String name;
    float trim = 1.f;                   // the INPUT volume it was loaded with (stays with the sound once it is in the machine)
    bool internal = false;              // built-in kit sounds: these never show on the IN meter
};

struct PadInfo
{
    std::shared_ptr<const Sample> sample;
    float start = 0.f, end = 1.f;       // chop region, 0..1 of the sample
    juce::String file;
};

// A free-form envelope: up to 8 points. t = time in ms from the start of the hit, v = level 0..1. Straight lines between points;
// after the last point the level holds.
struct EnvPoint { float t = 0.f, v = 1.f; };
struct EnvData
{
    int n = 2;
    EnvPoint p[8] = { { 0.f, 1.f }, { 100.f, 1.f } };

    float at (float tMs) const
    {
        if (tMs <= p[0].t) return p[0].v;
        for (int i = 0; i < n - 1; ++i)
            if (tMs < p[i + 1].t)
            {
                const float span = p[i + 1].t - p[i].t;
                return span <= 0.f ? p[i + 1].v : p[i].v + (p[i + 1].v - p[i].v) * (tMs - p[i].t) / span;
            }
        return p[n - 1].v;
    }
};

class BeatStationProcessor : public juce::AudioProcessor, private juce::MidiInputCallback
{
public:
    BeatStationProcessor();
    ~BeatStationProcessor() override { for (auto& m : midiInputs) m->stop(); }

    void prepareToPlay (double sr, int blockSize) override
    {
        sampleRate = sr;
        midiCollector.reset (sr);
        stopCoef = std::pow (0.001f, 1.f / (0.01f * (float) sr));     // 10 ms fade when a sound is cut
        for (auto& v : voices)
        {
            v.active = false;
            v.filt.prepare ({ sr, (juce::uint32) juce::jmax (1, blockSize), 2u });   // done here once so nothing allocates while playing
        }
        preview.active = false;
        for (auto& s : synth) { s.active = false; s.filt.prepare ({ sr, (juce::uint32) juce::jmax (1, blockSize), 1u }); s.env.setSampleRate (sr); }
        inBus.setSize (2, juce::jmax (blockSize * 2, 8192));
        micBus.setSize (2, juce::jmax (blockSize * 2, 8192));
        recBuf.setSize (2, (int) (sr * 60.0) + 16);                    // room for 60 s of recording, made here so nothing allocates while recording
        recLen = 0;
        masterFx.prepare (sr, blockSize);
    }
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        const auto in = l.getMainInputChannelSet();                    // the input is optional: off, mono or stereo
        return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
            && (in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
    }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Beat Station"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- Sampler API (message thread) ----
    bool loadSample (int pad, const juce::File& file, float trim = -1.f);   // trim < 0 = use the INPUT volume knob as it is now
    void clearPad (int pad);                            // removes the sample from the pad
    void clearChops (int pad);                          // keeps the sample on one pad, plays it whole again, empties the pads that shared it
    PadInfo getPad (int pad) const;
    void setRegion (int pad, float start, float end);
    void chopGrid (int pad, int count);                 // equal slices across consecutive pads
    void chopTransients (int pad, int maxSlices);       // slices at detected hits across consecutive pads
    void auditionWhole (int pad) { wholePad = pad; }    // plays the pad's entire sample, ignoring the chop
    EnvData getEnv (int pad, int which) const;          // which: 0 = amplitude, 1 = filter
    void setEnv (int pad, int which, const EnvData& e);

    // ---- Input: the IN meter shows only what comes in from outside (browser auditions, the audio input, samples you loaded) ----
    std::atomic<float> inputGain { 1.f };               // the INPUT volume knob under the IN meter
    void startInputRecording() { recLen = 0; recArmed = true; }
    void stopInputRecording() { recArmed = false; }
    bool commitRecording (int pad);                     // call a moment after stopping: puts what was recorded on the pad (and saves a WAV)
    std::atomic<bool> recArmed { false };
    std::atomic<int> recLen { 0 };                      // frames recorded so far

    // ---- What the keyboard and the pads play ----
    std::atomic<bool> keysDrums { false };              // true = the keyboard plays the drum pads instead of an instrument
    std::atomic<bool> padsInst { false };               // true = the pads play the chosen instrument instead of their samples

    // ---- Turntable deck (single SL-style deck; plays through the same IN/OUT meters and master FX) ----
    bool deckLoadPad (int pad);                         // loads the pad's whole sample; pads that share it become hot cues
    bool deckLoadFile (const juce::File& file);
    std::shared_ptr<const Sample> getDeckSample() const { const juce::SpinLock::ScopedLockType l (deckLock); return deckSample; }
    double getDeckCue (int i) const { return deckCue[(size_t) i].load(); }          // frame position or -1
    void setDeckCue (int i, double framePos) { deckCue[(size_t) i] = framePos; }
    std::atomic<bool> deckMotor { false }, deck45 { false }, deckReverse { false }, deckTouch { false }, deckLoop { true };
    std::atomic<float> deckPitch { 0.f }, deckTouchRate { 0.f }, deckCut { 1.f }, deckGain { 0.8f };   // pitch is a fraction (+0.08 = +8%)
    std::atomic<double> deckPos { 0.0 }, deckSeek { -1.0 };                        // position in frames of the loaded sample

    // ---- Sample browser ----
    SampleLibrary library;
    void previewFile (const juce::File& file);          // decodes in the background, then plays through the master
    void stopPreview() { previewStop = true; }
    std::atomic<float> previewGain { 0.8f };

    // ---- Built-in kits (synthesized drum kits compiled into the plugin) ----
    static juce::StringArray getKitKeys();
    static juce::String kitDisplayName (const juce::String& key);
    int loadKit (const juce::String& key);              // fills all 16 pads, returns how many loaded

    // ---- Step sequencer ----
    static constexpr int numTracks = 4, maxSteps = 32;
    bool getStep (int t, int s) const { return steps[(size_t) (t * maxSteps + s)].load(); }
    void setStep (int t, int s, bool on) { steps[(size_t) (t * maxSteps + s)].store (on); }
    int getPatternLength() const { return pLen->load() < 0.5f ? 16 : 32; }
    void clearPattern();
    void startPlaying() { resetPos = true; playing = true; }
    void stopPlaying() { playing = false; }
    juce::String getTrackName (int t) const { return trackName[(size_t) t]; }     // empty = use the pad's sound name
    void setTrackName (int t, const juce::String& n) { trackName[(size_t) t] = n.trim(); }
    std::atomic<bool> playing { false }, recording { false }, resetPos { false };
    std::atomic<int> currentStep { -1 };                // for the playhead in the GUI

    // ---- MIDI ----
    void rescanMidi();                                  // standalone only: opens every MIDI input it finds
    juce::StringArray midiNames;
    int getPadNote (int pad) const { return padNote[(size_t) pad]; }
    // MIDI Learn: startLearn("param:<id>"), startLearn("pad:<n>") or startLearn("act:play|stop|stopall|rec"), then move/press something.
    void startLearn (const juce::String& target) { const juce::SpinLock::ScopedLockType l (mapLock); learnTarget = target; }
    void cancelLearn() { const juce::SpinLock::ScopedLockType l (mapLock); learnTarget = {}; }
    juce::String getLearnTarget() const { const juce::SpinLock::ScopedLockType l (mapLock); return learnTarget; }
    juce::String describeMapping (const juce::String& target) const;      // "CC 21", "Note 40" or ""
    void clearMapping (const juce::String& target);
    std::atomic<int> mapVersion { 0 };                  // bumps whenever a mapping changes
    std::atomic<int> lastMidi { 0 };                    // the last CC or note received, packed: type<<24 | channel<<16 | number<<8 | value
    void copyPad (int from, int to);                    // gives another pad the same sound and chop

    // ---- Meters: [0] = input, [1] = output; each side has left/right peaks that the GUI reads and resets ----
    std::atomic<float> peak[2][2] {};
    std::atomic<bool> clipped[2] {};

    std::atomic<bool> stopAll { false };                // the STOP button
    std::atomic<int> selectedPad { 0 };                 // the pad that Keys mode plays
    std::atomic<uint32_t> activePads { 0 };             // bit i set = pad i is sounding
    juce::MidiKeyboardState keyboardState;              // the GUI plays pads and keys through this
    juce::AudioProcessorValueTreeState apvts;
    MasterFx masterFx { apvts };                        // EQ then compressor on the final mix

private:
    struct Slot
    {
        mutable juce::SpinLock lock;
        std::shared_ptr<const Sample> sample;
        std::atomic<float> start { 0.f }, end { 1.f };
        juce::String file;
        EnvData ampEnv, fltEnv;
    };
    struct Voice
    {
        bool active = false;
        int pad = -1, age = 0;
        std::shared_ptr<const Sample> s;
        double pos = 0, end = 0, ratio = 1, fadeOutLen = 1, relSamples = 0;
        float gainL = 1.f, gainR = 1.f, relGain = 1.f, relStep = 0.f;
        int note = -1, group = 0, fType = -1;
        bool keys = false, gate = false, stopping = false, releasing = false, autoRelease = true, filtOn = false, fltEnvOn = true;
        float stopGain = 1.f, trim = 1.f;
        bool internal = false;
        EnvData ampEnv, fltEnv;
        juce::dsp::StateVariableTPTFilter<float> filt;
    };
    struct SynthVoice
    {
        bool active = false, stopping = false;
        int note = 0, age = 0;
        float vel = 0.f, stopGain = 1.f, freq = 440.f, phA = 0.f, phB = 0.f, phS = 0.f, phM = 0.f, phV = 0.f;
        const SynthPatch* patch = nullptr;
        juce::ADSR env;
        juce::dsp::StateVariableTPTFilter<float> filt;
    };
    struct MidiMap { bool isCC = true; int channel = 1, number = 0; juce::String target; juce::RangedAudioParameter* param = nullptr; int padParamIdx = -1; };

    void assign (int pad, std::shared_ptr<const Sample> s, const juce::String& file, float start, float end);
    void startVoice (int pad, float velocity, int note, bool keys, float extraSemis, bool noGate, bool whole = false);
    void stopVoice (Voice& v) { v.stopping = true; }                  // quick 10 ms fade (cut / choke / STOP)
    void releaseVoice (Voice& v) { if (! v.releasing) { v.releasing = true; v.relStep = 1.f / (float) juce::jmax (1.0, v.relSamples); } }
    void render (juce::AudioBuffer<float>&, int from, int to);
    void renderPreview (juce::AudioBuffer<float>&);
    void renderDeck (juce::AudioBuffer<float>&, int n);
    void renderSampler (juce::AudioBuffer<float>&, int from, int to);
    void renderSynth (juce::AudioBuffer<float>&, int from, int to);
    void startSynth (int note, float velocity, int patchIndex);
    void releaseSynth (int note) { for (auto& s : synth) if (s.active && ! s.stopping && s.note == note) s.env.noteOff(); }
    bool handleMapping (const juce::MidiMessage& msg);
    void applyLearn (bool isCC, int channel, int number);
    void recordHit (int pad, double stepPosition);
    bool decodeInto (int pad, std::unique_ptr<juce::AudioFormatReader> reader, const juce::String& name, const juce::String& fileKey, float trim, bool internal);
    bool loadBuiltin (int pad, const juce::String& resourceName);
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m) override { midiCollector.addMessageToQueue (m); }

    double sampleRate = 44100.0;
    juce::AudioFormatManager formats;
    std::array<Slot, numPads> slots;
    std::array<Voice, 32> voices;
    std::array<SynthVoice, 12> synth;
    juce::Random noiseRng;
    juce::AudioBuffer<float> inBus;                      // the pad sounds before the envelope and filter: what the IN meter shows
    float *inL = nullptr, *inR = nullptr;
    juce::AudioBuffer<float> micBus, recBuf;             // the audio input for this block, and the recording
    juce::RangedAudioParameter* padParams[numPads][numPadParams] {};
    std::atomic<float>* raw[numPads][numPadParams] {};   // indexed by PadParam
    std::atomic<float> *pKeys = nullptr, *pKeysInst = nullptr;
    std::atomic<float> *pBpm = nullptr, *pSwing = nullptr, *pLen = nullptr, *pTrackPad[numTracks] {}, *pTrackMute[numTracks] {};
    std::array<std::atomic<bool>, numTracks * maxSteps> steps;
    std::array<juce::String, numTracks> trackName;
    double stepPos = 0.0;
    float stopCoef = 0.9985f;
    std::atomic<int> wholePad { -1 };

    juce::SpinLock mapLock;
    std::vector<MidiMap> maps;
    juce::String learnTarget;
    std::array<int, numPads> padNote {};

    struct Preview { bool active = false; std::shared_ptr<const Sample> s; double pos = 0, ratio = 1; } preview;
    juce::SpinLock previewLock;
    std::shared_ptr<const Sample> pendingPreview;
    std::atomic<bool> previewReady { false }, previewStop { false };
    std::atomic<int> previewToken { 0 };

    mutable juce::SpinLock deckLock;
    std::shared_ptr<const Sample> deckSample, dSample;
    std::array<std::atomic<double>, 8> deckCue;
    std::atomic<bool> deckDirty { false };
    double dPos = 0.0;
    float dRate = 0.f, dCutNow = 1.f;

    juce::MidiMessageCollector midiCollector;
    std::vector<std::unique_ptr<juce::MidiInput>> midiInputs;
    juce::ThreadPool previewPool { 1 };                 // declared last so it is shut down first

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeatStationProcessor)
};
