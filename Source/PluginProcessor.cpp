#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <KitData.h>

static juce::dsp::StateVariableTPTFilterType typeFor (int ft)
{
    return ft == 2 ? juce::dsp::StateVariableTPTFilterType::highpass
         : ft == 3 ? juce::dsp::StateVariableTPTFilterType::bandpass
                   : juce::dsp::StateVariableTPTFilterType::lowpass;
}

BeatStationProcessor::BeatStationProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    formats.registerBasicFormats();
    for (int p = 0; p < numPads; ++p)
    {
        auto gp = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
        raw[p][pLevel] = gp (ParamID::level (p));   raw[p][pPan] = gp (ParamID::pan (p));     raw[p][pTune] = gp (ParamID::tune (p));
        raw[p][pMode] = gp (ParamID::mode (p));     raw[p][pGroup] = gp (ParamID::group (p)); raw[p][pRel] = gp (ParamID::rel (p));
        raw[p][pFType] = gp (ParamID::fType (p));   raw[p][pFCut] = gp (ParamID::fCut (p));   raw[p][pFRes] = gp (ParamID::fRes (p));
        raw[p][pFAmt] = gp (ParamID::fAmt (p));
        raw[p][pAmpOn] = gp (ParamID::ampOn (p));   raw[p][pFltOn] = gp (ParamID::fltOn (p));
        auto gq = [this] (const juce::String& id) { return apvts.getParameter (id); };         // the parameters themselves, for MIDI learn
        padParams[p][pLevel] = gq (ParamID::level (p));   padParams[p][pPan] = gq (ParamID::pan (p));     padParams[p][pTune] = gq (ParamID::tune (p));
        padParams[p][pMode] = gq (ParamID::mode (p));     padParams[p][pGroup] = gq (ParamID::group (p)); padParams[p][pRel] = gq (ParamID::rel (p));
        padParams[p][pFType] = gq (ParamID::fType (p));   padParams[p][pFCut] = gq (ParamID::fCut (p));   padParams[p][pFRes] = gq (ParamID::fRes (p));
        padParams[p][pFAmt] = gq (ParamID::fAmt (p));     padParams[p][pAmpOn] = gq (ParamID::ampOn (p)); padParams[p][pFltOn] = gq (ParamID::fltOn (p));
        padNote[(size_t) p] = firstNote + p;
        slots[(size_t) p].fltEnv.p[1] = { 400.f, 0.f };                                       // filter envelope starts as a falling curve
    }
    for (auto& cue : deckCue) cue = -1.0;
    pKeys = apvts.getRawParameterValue (ParamID::keysMode);
    pKeysInst = apvts.getRawParameterValue (ParamID::keysInst);
    pBpm = apvts.getRawParameterValue (ParamID::bpm);
    pSwing = apvts.getRawParameterValue (ParamID::swing);
    pLen = apvts.getRawParameterValue (ParamID::seqLen);
    for (int t = 0; t < numTracks; ++t)
    {
        pTrackPad[t] = apvts.getRawParameterValue (ParamID::trackPad (t));
        pTrackMute[t] = apvts.getRawParameterValue (ParamID::trackMute (t));
    }
    for (auto& s : steps) s.store (false);

    loadKit ("trap");                                                 // so there is something to play right away
    for (int s : { 0, 6, 10 })                 setStep (0, s, true);  // a simple starter beat: kick,
    for (int s : { 4, 12 })                    setStep (1, s, true);  // snare,
    for (int s : { 0, 2, 4, 6, 8, 10, 12, 14 }) setStep (2, s, true); // hats,
    for (int s : { 0, 10 })                    setStep (3, s, true);  // 808

    if (wrapperType == wrapperType_Standalone) rescanMidi();
}

void BeatStationProcessor::rescanMidi()
{
    for (auto& m : midiInputs) m->stop();
    midiInputs.clear();
    midiNames.clear();
    for (const auto& d : juce::MidiInput::getAvailableDevices())
        if (auto in = juce::MidiInput::openDevice (d.identifier, this))
        {
            in->start();
            midiInputs.push_back (std::move (in));
            midiNames.add (d.name);
        }
}

// ---------------------------------------------------------------- sample management
void BeatStationProcessor::assign (int pad, std::shared_ptr<const Sample> s, const juce::String& file, float start, float end)
{
    auto& slot = slots[(size_t) pad];
    {
        const juce::SpinLock::ScopedLockType l (slot.lock);
        slot.sample = std::move (s);
        slot.file = file;
    }
    slot.start = start;
    slot.end = end;
}

bool BeatStationProcessor::decodeInto (int pad, std::unique_ptr<juce::AudioFormatReader> reader, const juce::String& name, const juce::String& fileKey, float trim, bool internal)
{
    if (reader == nullptr || reader->lengthInSamples <= 0) return false;
    const int len = (int) std::min (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60.0));   // cap at 60 s
    auto s = std::make_shared<Sample>();
    s->data.setSize (2, len);
    reader->read (&s->data, 0, len, 0, true, true);
    if (reader->numChannels == 1) s->data.copyFrom (1, 0, s->data, 0, 0, len);
    s->sampleRate = reader->sampleRate;
    s->name = name;
    s->trim = trim;
    s->internal = internal;
    assign (pad, s, fileKey, 0.f, 1.f);
    return true;
}

bool BeatStationProcessor::loadSample (int pad, const juce::File& file, float trim)
{
    return decodeInto (pad, std::unique_ptr<juce::AudioFormatReader> (formats.createReaderFor (file)),
                       file.getFileNameWithoutExtension(), file.getFullPathName(), trim < 0.f ? inputGain.load() : trim, false);
}

// Resource file names look like "trap_01_Kick.wav": kit, pad number, sound name (hyphen = space).
static bool parseKitResource (const char* resName, juce::String& kit, int& index, juce::String& sound)
{
    const char* original = KitData::getNamedResourceOriginalFilename (resName);
    if (original == nullptr) return false;
    const auto parts = juce::StringArray::fromTokens (juce::String (original).upToLastOccurrenceOf (".", false, false), "_", "");
    if (parts.size() < 3) return false;
    kit = parts[0];
    index = parts[1].getIntValue();
    sound = parts[2].replaceCharacter ('-', ' ');
    return true;
}

bool BeatStationProcessor::loadBuiltin (int pad, const juce::String& resourceName)
{
    int size = 0;
    const char* data = KitData::getNamedResource (resourceName.toRawUTF8(), size);
    juce::String kit, sound;
    int index = 0;
    if (data == nullptr || ! parseKitResource (resourceName.toRawUTF8(), kit, index, sound)) return false;
    return decodeInto (pad, std::unique_ptr<juce::AudioFormatReader> (formats.createReaderFor (std::make_unique<juce::MemoryInputStream> (data, (size_t) size, false))),
                       sound, "builtin:" + resourceName, 1.f, true);
}

juce::StringArray BeatStationProcessor::getKitKeys()
{
    juce::StringArray keys;
    for (int i = 0; i < KitData::namedResourceListSize; ++i)
    {
        juce::String kit, sound;
        int index = 0;
        if (parseKitResource (KitData::namedResourceList[i], kit, index, sound)) keys.addIfNotAlreadyThere (kit);
    }
    return keys;
}

juce::String BeatStationProcessor::kitDisplayName (const juce::String& key)
{
    if (key == "g-funk") return "G-Funk Kit";
    auto words = juce::StringArray::fromTokens (key, "-", "");
    for (auto& w : words) w = w.substring (0, 1).toUpperCase() + w.substring (1);
    return words.joinIntoString (" ") + " Kit";
}

int BeatStationProcessor::loadKit (const juce::String& key)
{
    int count = 0;
    for (int i = 0; i < KitData::namedResourceListSize; ++i)
    {
        juce::String kit, sound;
        int index = 0;
        if (parseKitResource (KitData::namedResourceList[i], kit, index, sound) && kit == key && index >= 1 && index <= numPads)
            if (loadBuiltin (index - 1, KitData::namedResourceList[i])) ++count;
    }
    return count;
}

void BeatStationProcessor::clearPattern() { for (auto& s : steps) s.store (false); }
void BeatStationProcessor::clearPad (int pad) { assign (pad, nullptr, {}, 0.f, 1.f); }

void BeatStationProcessor::clearChops (int pad)
{
    const auto src = getPad (pad);
    if (src.sample == nullptr) return;
    int home = pad;                                                   // the lowest-numbered pad sharing this sample keeps it
    for (int i = 0; i < numPads; ++i) if (getPad (i).sample == src.sample) { home = i; break; }
    for (int i = 0; i < numPads; ++i) if (i != home && getPad (i).sample == src.sample) clearPad (i);
    setRegion (home, 0.f, 1.f);
}

PadInfo BeatStationProcessor::getPad (int pad) const
{
    const auto& slot = slots[(size_t) pad];
    PadInfo info;
    {
        const juce::SpinLock::ScopedLockType l (slot.lock);
        info.sample = slot.sample;
        info.file = slot.file;
    }
    info.start = slot.start.load();
    info.end = slot.end.load();
    return info;
}

void BeatStationProcessor::setRegion (int pad, float start, float end)
{
    slots[(size_t) pad].start = start;
    slots[(size_t) pad].end = end;
}

EnvData BeatStationProcessor::getEnv (int pad, int which) const
{
    const auto& slot = slots[(size_t) pad];
    const juce::SpinLock::ScopedLockType l (slot.lock);
    return which == 0 ? slot.ampEnv : slot.fltEnv;
}

void BeatStationProcessor::setEnv (int pad, int which, const EnvData& e)
{
    auto& slot = slots[(size_t) pad];
    const juce::SpinLock::ScopedLockType l (slot.lock);
    (which == 0 ? slot.ampEnv : slot.fltEnv) = e;
}

void BeatStationProcessor::chopGrid (int pad, int count)
{
    const auto src = getPad (pad);
    if (src.sample == nullptr) return;
    for (int k = 0; k < count && pad + k < numPads; ++k)
        assign (pad + k, src.sample, src.file,
                src.start + (src.end - src.start) * (float) k / (float) count,
                src.start + (src.end - src.start) * (float) (k + 1) / (float) count);
}

void BeatStationProcessor::chopTransients (int pad, int maxSlices)
{
    const auto src = getPad (pad);
    if (src.sample == nullptr) return;
    const auto& d = src.sample->data;
    const int n = d.getNumSamples(), i0 = (int) (src.start * (float) n), i1 = (int) (src.end * (float) n), hop = 512;

    std::vector<float> env;                                           // short-window RMS envelope
    for (int i = i0; i + hop <= i1; i += hop)
    {
        double sum = 0.0;
        for (int j = 0; j < hop; ++j) { const float v = d.getSample (0, i + j); sum += (double) v * v; }
        env.push_back (std::sqrt ((float) (sum / hop)));
    }
    if (env.size() < 4) return;

    std::vector<float> flux (env.size(), 0.f);                        // positive energy jumps = hits
    float maxFlux = 0.f;
    for (size_t k = 1; k < env.size(); ++k) { flux[k] = juce::jmax (0.f, env[k] - env[k - 1]); maxFlux = juce::jmax (maxFlux, flux[k]); }

    struct Cand { size_t idx; float f; };
    std::vector<Cand> cands;
    for (size_t k = 1; k + 1 < env.size(); ++k)
        if (flux[k] > 0.2f * maxFlux && flux[k] >= flux[k - 1] && flux[k] >= flux[k + 1]) cands.push_back ({ k, flux[k] });
    std::sort (cands.begin(), cands.end(), [] (const Cand& a, const Cand& b) { return a.f > b.f; });

    std::vector<size_t> picked;                                       // strongest hits, at least ~90 ms apart
    for (auto& c : cands)
    {
        if ((int) picked.size() >= maxSlices - 1) break;
        bool ok = true;
        for (auto p : picked) if ((c.idx > p ? c.idx - p : p - c.idx) < 8) ok = false;
        if (ok) picked.push_back (c.idx);
    }
    std::sort (picked.begin(), picked.end());

    std::vector<float> bounds { src.start };
    for (auto p : picked) bounds.push_back ((float) (i0 + (int) (p - 1) * hop) / (float) n);
    bounds.push_back (src.end);
    for (size_t k = 0; k + 1 < bounds.size() && pad + (int) k < numPads; ++k)
        assign (pad + (int) k, src.sample, src.file, bounds[k], bounds[k + 1]);
}

// ---------------------------------------------------------------- browser preview
void BeatStationProcessor::previewFile (const juce::File& file)
{
    const int token = ++previewToken;
    previewStop = true;                                               // cut whatever is playing right now
    previewPool.addJob ([this, file, token]
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr || reader->lengthInSamples <= 0) return;
        const int len = (int) std::min (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 30.0));   // first 30 s
        auto s = std::make_shared<Sample>();
        s->data.setSize (2, len);
        reader->read (&s->data, 0, len, 0, true, true);
        if (reader->numChannels == 1) s->data.copyFrom (1, 0, s->data, 0, 0, len);
        s->sampleRate = reader->sampleRate;
        s->name = file.getFileNameWithoutExtension();
        if (token != previewToken.load()) return;                     // a newer click replaced this one
        { const juce::SpinLock::ScopedLockType l (previewLock); pendingPreview = s; }
        previewReady = true;
    });
}

void BeatStationProcessor::renderPreview (juce::AudioBuffer<float>& buffer)
{
    if (! preview.active || preview.s == nullptr) return;
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const float* d0 = preview.s->data.getReadPointer (0);
    const float* d1 = preview.s->data.getReadPointer (1);
    const double len = (double) preview.s->data.getNumSamples();
    const float g = previewGain.load() * inputGain.load();            // a browser audition is a sound coming in: the INPUT volume sets its level
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        if (preview.pos >= len - 1.0) { preview.active = false; break; }
        const int idx = (int) preview.pos;
        const float fr = (float) (preview.pos - (double) idx);
        const float pl = (d0[idx] + (d0[idx + 1] - d0[idx]) * fr) * g, pr = (d1[idx] + (d1[idx + 1] - d1[idx]) * fr) * g;
        if (inL != nullptr) { inL[i] += pl; inR[i] += pr; }           // nothing changes it on the way through, so IN and OUT match
        L[i] += pl;
        if (R != nullptr) R[i] += pr;
        preview.pos += preview.ratio;
    }
}

// ---------------------------------------------------------------- MIDI learn
void BeatStationProcessor::copyPad (int from, int to)
{
    if (from == to) return;
    const auto src = getPad (from);
    if (src.sample == nullptr) return;
    assign (to, src.sample, src.file, src.start, src.end);
    setEnv (to, 0, getEnv (from, 0));
    setEnv (to, 1, getEnv (from, 1));
}

juce::String BeatStationProcessor::describeMapping (const juce::String& target) const
{
    const juce::SpinLock::ScopedLockType l (mapLock);
    if (target.startsWith ("pad:")) return "Note " + juce::String (padNote[(size_t) target.fromFirstOccurrenceOf (":", false, false).getIntValue()]);
    for (const auto& m : maps) if (m.target == target) return (m.isCC ? "CC " : "Note ") + juce::String (m.number) + " ch " + juce::String (m.channel);
    return {};
}

void BeatStationProcessor::clearMapping (const juce::String& target)
{
    {
        const juce::SpinLock::ScopedLockType l (mapLock);
        if (target.startsWith ("pad:"))
        {
            const int i = target.fromFirstOccurrenceOf (":", false, false).getIntValue();
            padNote[(size_t) i] = firstNote + i;
        }
        else
            maps.erase (std::remove_if (maps.begin(), maps.end(), [&] (const MidiMap& m) { return m.target == target; }), maps.end());
    }
    ++mapVersion;
}

void BeatStationProcessor::applyLearn (bool isCC, int channel, int number)
{
    // called with mapLock held
    const juce::String target = learnTarget;
    if (target.startsWith ("pad:"))
    {
        if (isCC) return;                                             // pads learn from notes only; keep waiting
        const int i = target.fromFirstOccurrenceOf (":", false, false).getIntValue();
        const int old = padNote[(size_t) i];
        for (int j = 0; j < numPads; ++j) if (j != i && padNote[(size_t) j] == number) padNote[(size_t) j] = old;   // swap, so no two pads share a note
        padNote[(size_t) i] = number;
    }
    else
    {
        if ((target.startsWith ("param:") || target.startsWith ("sel:")) && ! isCC) return;     // knobs learn from controllers only
        // a controller is identified by its channel AND number, so knobs that share a number on different channels stay separate
        maps.erase (std::remove_if (maps.begin(), maps.end(),
                                    [&] (const MidiMap& m) { return m.target == target || (m.isCC == isCC && m.channel == channel && m.number == number); }), maps.end());
        MidiMap m;
        m.isCC = isCC;
        m.channel = channel;
        m.number = number;
        m.target = target;
        if (target.startsWith ("param:")) m.param = apvts.getParameter (target.fromFirstOccurrenceOf (":", false, false));
        if (target.startsWith ("sel:")) m.padParamIdx = target.fromFirstOccurrenceOf (":", false, false).getIntValue();   // follows whichever pad is selected
        maps.push_back (m);
    }
    learnTarget = {};
    ++mapVersion;
}

bool BeatStationProcessor::handleMapping (const juce::MidiMessage& msg)
{
    const bool isCC = msg.isController(), isNote = msg.isNoteOn();
    if (! isCC && ! isNote) return false;
    const juce::SpinLock::ScopedTryLockType l (mapLock);
    if (! l.isLocked()) return false;
    const int number = isCC ? msg.getControllerNumber() : msg.getNoteNumber(), channel = msg.getChannel();
    if (isCC && number >= 120) return false;                          // "all notes off" and friends are not mappable

    if (learnTarget.isNotEmpty())
    {
        const bool waiting = learnTarget.startsWith ("pad:") ? ! isNote : ((learnTarget.startsWith ("param:") || learnTarget.startsWith ("sel:")) ? ! isCC : false);
        if (waiting) return false;                                    // wrong kind of message for this target
        applyLearn (isCC, channel, number);
        return true;
    }

    for (const auto& m : maps)
        if (m.isCC == isCC && m.channel == channel && m.number == number)
        {
            const float value = isCC ? (float) msg.getControllerValue() / 127.f : 1.f;
            if (m.param != nullptr) { if (isCC) m.param->setValueNotifyingHost (value); }
            else if (m.padParamIdx >= 0)
            {
                if (isCC) if (auto* pp = padParams[selectedPad.load()][m.padParamIdx]) pp->setValueNotifyingHost (value);
            }
            else if (! isCC || msg.getControllerValue() >= 64)        // actions fire on a note-on or when a button sends a high value
            {
                if (m.target == "act:play")         { if (playing.load()) stopPlaying(); else startPlaying(); }
                else if (m.target == "act:stop")    stopPlaying();
                else if (m.target == "act:stopall") stopAll = true;
                else if (m.target == "act:rec")     recording = ! recording.load();
            }
            return true;
        }
    return false;
}

// ---------------------------------------------------------------- audio
void BeatStationProcessor::startVoice (int pad, float velocity, int note, bool keys, float extraSemis, bool noGate, bool whole)
{
    std::shared_ptr<const Sample> smp;
    EnvData ae, fe;
    auto& slot = slots[(size_t) pad];
    {
        const juce::SpinLock::ScopedTryLockType l (slot.lock);
        if (! l.isLocked()) return;                                   // GUI is swapping this pad right now: skip the hit
        smp = slot.sample;
        ae = slot.ampEnv;
        fe = slot.fltEnv;
    }
    if (smp == nullptr) return;

    const double len = (double) smp->data.getNumSamples();
    const double start = whole ? 0.0 : (double) slot.start.load() * len;
    const double end = whole ? len : juce::jmin (len, (double) slot.end.load() * len);
    if (end - start < 4.0) return;

    auto P = [&] (int idx) { return raw[pad][idx]->load(); };
    const int mode = (int) P (pMode), group = (int) P (pGroup);
    if (! keys && ! whole)                                            // Cut mode and choke groups fade the old sound out
        for (auto& c : voices)
            if (c.active && ! c.stopping && ! c.keys && ((mode == 0 && c.pad == pad) || (group > 0 && c.group == group)))
                stopVoice (c);

    Voice* v = nullptr;
    for (auto& c : voices) if (! c.active) { v = &c; break; }
    if (v == nullptr) { v = &voices[0]; for (auto& c : voices) if (c.age > v->age) v = &c; }   // steal the oldest

    const float semis = P (pTune) + extraSemis, pan = P (pPan);
    const float gain = juce::Decibels::decibelsToGain (P (pLevel)) * velocity;
    const float angle = (pan + 1.f) * juce::MathConstants<float>::pi * 0.25f;

    v->active = true;  v->pad = pad;  v->age = 0;  v->s = smp;
    v->trim = smp->trim;  v->internal = smp->internal;
    v->note = note;    v->keys = keys; v->gate = (mode == 2) && ! noGate && ! whole; v->group = group;
    v->stopping = false; v->stopGain = 1.f; v->releasing = false; v->relGain = 1.f; v->relStep = 0.f;
    v->autoRelease = ! v->gate;                                       // one-shots fade themselves out as the chop ends
    v->pos = start;    v->end = end;
    v->ratio = smp->sampleRate / sampleRate * std::pow (2.0, (double) semis / 12.0);
    v->fadeOutLen = juce::jmax (1.0, 0.003 * sampleRate * v->ratio);
    v->relSamples = (double) P (pRel) * 0.001 * sampleRate;
    v->gainL = gain * std::cos (angle) * 1.4142f;                     // equal-power pan, unity at centre
    v->gainR = gain * std::sin (angle) * 1.4142f;
    v->ampEnv = P (pAmpOn) > 0.5f ? ae : EnvData();                   // an envelope that is switched off is just flat
    v->fltEnv = fe;
    v->fltEnvOn = P (pFltOn) > 0.5f;
    v->fType = -1;                                                    // forces the filter to be set up on the first control tick
    v->filtOn = false;
    v->filt.reset();
}

void BeatStationProcessor::renderSampler (juce::AudioBuffer<float>& buffer, int from, int to)
{
    if (to <= from) return;
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const float fadeInLen = (float) (0.001 * sampleRate), maxCut = (float) (sampleRate * 0.45);
    const float msPerSample = (float) (1000.0 / sampleRate);

    for (auto& v : voices)
    {
        if (! v.active) continue;
        const float* d0 = v.s->data.getReadPointer (0);
        const float* d1 = v.s->data.getReadPointer (1);
        for (int i = from; i < to; ++i)
        {
            if (v.pos >= v.end - 1.0) { v.active = false; break; }
            if (! v.releasing && v.autoRelease && (v.end - v.pos) / v.ratio <= v.relSamples)
                releaseVoice (v);                                     // start the fade early enough that it finishes as the chop ends

            const float tMs = (float) v.age * msPerSample;
            float aenv = v.ampEnv.at (tMs);
            if (v.releasing)
            {
                v.relGain -= v.relStep;
                if (v.relGain <= 0.f) { v.active = false; break; }
                aenv *= v.relGain;
            }

            const int idx = (int) v.pos;
            const float fr = (float) (v.pos - (double) idx);
            float l = (d0[idx] + (d0[idx + 1] - d0[idx]) * fr) * v.trim;
            float r = (d1[idx] + (d1[idx + 1] - d1[idx]) * fr) * v.trim;
            // IN meter: a sample that came in from outside shows at the level it came in at, whatever the pad's level, pan, envelope or filter do.
            // Built-in kit sounds are internal, so they only show on OUT.
            if (inL != nullptr && ! v.internal) { inL[i] += l; inR[i] += r; }

            if ((v.age & 15) == 0)                                    // every 16 samples: read the filter knobs live, so turning them is heard at once
            {
                const int ft = (int) raw[v.pad][pFType]->load();
                const float cut = juce::jlimit (20.f, maxCut, raw[v.pad][pFCut]->load() * std::pow (2.f, raw[v.pad][pFAmt]->load() * (v.fltEnvOn ? v.fltEnv.at (tMs) : 0.f)));
                if (ft != v.fType) { v.fType = ft; v.filt.setType (typeFor (ft)); }
                v.filt.setCutoffFrequency (cut);
                v.filt.setResonance (raw[v.pad][pFRes]->load());
                v.filtOn = ft > 0 && ! (ft == 1 && cut >= 18000.f);   // a fully open low-pass is skipped
            }
            if (v.filtOn)
            {
                l = v.filt.processSample (0, l);
                r = v.filt.processSample (1, r);
            }

            const float fadeIn = v.age < (int) fadeInLen ? (float) v.age / fadeInLen : 1.f;
            const float fadeOut = juce::jmin (1.f, (float) ((v.end - v.pos) / v.fadeOutLen));
            float g = aenv * fadeIn * fadeOut;
            if (v.stopping)
            {
                v.stopGain *= stopCoef;
                if (v.stopGain < 0.001f) { v.active = false; break; }
                g *= v.stopGain;
            }
            L[i] += l * v.gainL * g;
            if (R != nullptr) R[i] += r * v.gainR * g;
            v.pos += v.ratio;
            ++v.age;
        }
    }
}

static float polyBlep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

static float oscSample (int wave, float ph, float dt)
{
    switch (wave)
    {
        case 0:  return std::sin (juce::MathConstants<float>::twoPi * ph);
        case 1:  return 1.f - 4.f * std::abs (ph - 0.5f);
        case 2:  return 2.f * ph - 1.f - polyBlep (ph, dt);
        case 3:  { float v = ph < 0.5f ? 1.f : -1.f; v += polyBlep (ph, dt); float p2 = ph + 0.5f;  if (p2 >= 1.f) p2 -= 1.f; return v - polyBlep (p2, dt); }
        default: { float v = ph < 0.25f ? 1.f : -1.f; v += polyBlep (ph, dt); float p2 = ph + 0.75f; if (p2 >= 1.f) p2 -= 1.f; return v - polyBlep (p2, dt); }
    }
}

void BeatStationProcessor::startSynth (int note, float velocity, int patchIndex)
{
    const auto& patches = synthPatches();
    if (! juce::isPositiveAndBelow (patchIndex, (int) patches.size())) return;
    SynthVoice* v = nullptr;
    for (auto& c : synth) if (! c.active) { v = &c; break; }
    if (v == nullptr) { v = &synth[0]; for (auto& c : synth) if (c.age > v->age) v = &c; }
    const SynthPatch& p = patches[(size_t) patchIndex];
    v->active = true;  v->stopping = false;  v->stopGain = 1.f;  v->note = note;  v->age = 0;  v->vel = velocity;  v->patch = &p;
    v->freq = 440.f * std::pow (2.f, ((float) note - 69.f) / 12.f);
    v->phA = v->phB = v->phS = v->phM = v->phV = 0.f;
    v->env.setParameters ({ p.attMs * 0.001f, p.decMs * 0.001f, p.sus, p.relMs * 0.001f });
    v->env.noteOn();
    v->filt.reset();
    v->filt.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    v->filt.setResonance (p.res);
    v->filt.setCutoffFrequency (p.cutoff);
}

void BeatStationProcessor::renderSynth (juce::AudioBuffer<float>& buffer, int from, int to)
{
    if (to <= from) return;
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const float sr = (float) sampleRate, maxCut = sr * 0.45f, twoPi = juce::MathConstants<float>::twoPi;
    static const float harm[6] = { 1.f, 2.f, 3.f, 4.f, 6.f, 8.f };

    for (auto& v : synth)
    {
        if (! v.active) continue;
        const SynthPatch& p = *v.patch;
        const bool additive = (p.drawbars[0] + p.drawbars[1] + p.drawbars[2] + p.drawbars[3] + p.drawbars[4] + p.drawbars[5]) > 0.f;
        const float detRatio = std::exp2 (p.detuneCents / 1200.f), octRatio = std::exp2 (p.octB / 12.f);

        for (int i = from; i < to; ++i)
        {
            const float tMs = (float) v.age * 1000.f / sr;
            float f = v.freq;
            if (p.pitchDrop != 0.f) f *= std::exp2 (p.pitchDrop * std::exp (-tMs / juce::jmax (1.f, p.pitchDropMs)) / 12.f);
            if (p.vibCents > 0.f)
            {
                v.phV += p.vibRate / sr;  if (v.phV >= 1.f) v.phV -= 1.f;
                f *= std::exp2 (std::sin (twoPi * v.phV) * p.vibCents / 1200.f);
            }
            const float dtA = f / sr;

            float mix;
            if (additive)
            {
                mix = 0.f;
                for (int k = 0; k < 6; ++k) mix += p.drawbars[k] * std::sin (twoPi * harm[k] * v.phA);
                mix *= 0.35f;
            }
            else if (p.fmIndex > 0.f && p.waveA == 0)
            {
                const float idx = p.fmIndex * std::exp (-tMs / juce::jmax (1.f, p.fmDecayMs));
                mix = std::sin (twoPi * v.phA + idx * std::sin (twoPi * v.phM));
                v.phM += dtA * p.fmRatio;  if (v.phM >= 1.f) v.phM -= 1.f;
            }
            else mix = oscSample (p.waveA, v.phA, dtA);
            v.phA += dtA;  if (v.phA >= 1.f) v.phA -= 1.f;

            if (p.levelB > 0.f)
            {
                const float dtB = dtA * detRatio * octRatio;
                mix += p.levelB * oscSample (p.waveB, v.phB, dtB);
                v.phB += dtB;  if (v.phB >= 1.f) v.phB -= 1.f;
            }
            if (p.sub > 0.f) { mix += p.sub * std::sin (twoPi * v.phS); v.phS += dtA * 0.5f;  if (v.phS >= 1.f) v.phS -= 1.f; }
            if (p.noise > 0.f) mix += p.noise * (noiseRng.nextFloat() * 2.f - 1.f);

            const float aenv = v.env.getNextSample();
            if (! v.env.isActive()) { v.active = false; break; }

            if ((v.age & 15) == 0)                                    // filter cutoff: base, key tracking, and a falling envelope
            {
                const float fe = std::exp (-tMs / juce::jmax (1.f, p.fltDecMs));
                v.filt.setCutoffFrequency (juce::jlimit (30.f, maxCut, p.cutoff * std::pow (v.freq / 261.63f, p.keyTrack) * std::exp2 (p.fltAmt * fe)));
            }
            float g = v.vel * p.gain;
            if (v.stopping)
            {
                v.stopGain *= stopCoef;
                if (v.stopGain < 0.001f) { v.active = false; break; }
                g *= v.stopGain;
            }
            const float out = v.filt.processSample (0, mix * aenv) * g;
            L[i] += out;
            if (R != nullptr) R[i] += out;
            ++v.age;
        }
    }
}

void BeatStationProcessor::render (juce::AudioBuffer<float>& buffer, int from, int to)
{
    renderSampler (buffer, from, to);
    renderSynth (buffer, from, to);
}

// ---------------------------------------------------------------- recording from the audio input
bool BeatStationProcessor::commitRecording (int pad)
{
    const int len = juce::jmin (recLen.load(), recBuf.getNumSamples());
    if (recArmed.load() || len < (int) (sampleRate * 0.05) || pad < 0 || pad >= numPads) return false;

    auto s = std::make_shared<Sample>();
    s->data.setSize (2, len);
    s->data.copyFrom (0, 0, recBuf, 0, 0, len);
    s->data.copyFrom (1, 0, recBuf, 1, 0, len);
    s->sampleRate = sampleRate;
    s->name = "Rec " + juce::Time::getCurrentTime().formatted ("%H.%M.%S");

    // saved as a WAV so the project can find it again
    const auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                         .getChildFile ("Basement Instruments").getChildFile ("Beat Station").getChildFile ("Recordings");
    dir.createDirectory();
    const auto file = dir.getChildFile ("Rec-" + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".wav");
    juce::String key = "rec:" + s->name;
    if (auto out = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
    {
        juce::WavAudioFormat wav;
        if (auto* raw = wav.createWriterFor (out.get(), sampleRate, 2, 24, {}, 0))
        {
            out.release();                                            // the writer owns the stream now
            std::unique_ptr<juce::AudioFormatWriter> writer (raw);
            if (writer->writeFromAudioSampleBuffer (s->data, 0, len)) key = file.getFullPathName();
        }
    }
    assign (pad, s, key, 0.f, 1.f);
    recLen = 0;
    return true;
}

// ---------------------------------------------------------------- turntable deck
bool BeatStationProcessor::deckLoadPad (int pad)
{
    const auto info = getPad (pad);
    if (info.sample == nullptr) return false;
    {
        const juce::SpinLock::ScopedLockType l (deckLock);
        deckSample = info.sample;
    }
    for (auto& c : deckCue) c = -1.0;
    int k = 0;
    const double len = (double) info.sample->data.getNumSamples();
    for (int p = 0; p < numPads && k < 8; ++p)                       // pads that share this sample: their chop starts become hot cues
    {
        const auto o = getPad (p);
        if (o.sample == info.sample) deckCue[(size_t) k++] = (double) o.start * len;
    }
    if (k == 0) deckCue[0] = 0.0;
    deckSeek = 0.0;
    deckDirty = true;
    return true;
}

bool BeatStationProcessor::deckLoadFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0) return false;
    const int len = (int) std::min (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60.0));
    auto s = std::make_shared<Sample>();
    s->data.setSize (2, len);
    reader->read (&s->data, 0, len, 0, true, true);
    if (reader->numChannels == 1) s->data.copyFrom (1, 0, s->data, 0, 0, len);
    s->sampleRate = reader->sampleRate;
    s->name = file.getFileNameWithoutExtension();
    s->trim = inputGain.load();
    {
        const juce::SpinLock::ScopedLockType l (deckLock);
        deckSample = s;
    }
    for (auto& c : deckCue) c = -1.0;
    deckCue[0] = 0.0;
    deckSeek = 0.0;
    deckDirty = true;
    return true;
}

// One revolution = 1.8 s of audio at 33 1/3 rpm. 45 rpm plays 1.35x faster. The rate follows the motor (spin-up / brake) or the hand.
void BeatStationProcessor::renderDeck (juce::AudioBuffer<float>& buffer, int n)
{
    if (deckDirty.exchange (false))
    {
        const juce::SpinLock::ScopedTryLockType l (deckLock);
        if (l.isLocked()) { dSample = deckSample; dPos = 0.0; }
        else deckDirty = true;
    }
    if (dSample == nullptr) { deckPos = 0.0; return; }

    const double seek = deckSeek.exchange (-1.0);
    const double len = (double) dSample->data.getNumSamples();
    if (seek >= 0.0) dPos = juce::jlimit (0.0, len - 1.0, seek);

    const bool touching = deckTouch.load();
    const float pitch = deckPitch.load();
    float target;
    if (touching) target = deckTouchRate.load();
    else
    {
        target = deck45.load() ? 1.35f : 1.f;
        target *= 1.f + pitch;
        if (deckReverse.load()) target = -target;
        if (! deckMotor.load()) target = 0.f;
    }
    const float tc = touching ? 0.004f : (target == 0.f ? 0.35f : 0.25f);
    const float alpha = 1.f - std::exp (-1.f / (tc * (float) sampleRate));
    const float cutAlpha = 1.f - std::exp (-1.f / (0.0015f * (float) sampleRate));
    const float cutTarget = deckCut.load();
    const float gain = deckGain.load();
    const double srRatio = dSample->sampleRate / sampleRate;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const float* d0 = dSample->data.getReadPointer (0);
    const float* d1 = dSample->data.getReadPointer (1);
    const int total = dSample->data.getNumSamples();
    const float dTrim = dSample->trim;
    const bool dInternal = dSample->internal;

    auto hermite = [] (float y0, float y1, float y2, float y3, float t)
    {
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    };

    for (int i = 0; i < n; ++i)
    {
        dRate += (target - dRate) * alpha;
        dCutNow += (cutTarget - dCutNow) * cutAlpha;
        dPos += (double) dRate * srRatio;
        if (dPos >= len || dPos < 0.0)
        {
            if (deckLoop.load()) { dPos = std::fmod (dPos, len); if (dPos < 0.0) dPos += len; }
            else { dPos = juce::jlimit (0.0, len - 1.0, dPos); continue; }
        }
        const int idx = (int) dPos;
        const float fr = (float) (dPos - (double) idx);
        const int i0 = juce::jmax (0, idx - 1), i2 = juce::jmin (total - 1, idx + 1), i3 = juce::jmin (total - 1, idx + 2);
        const float sl = hermite (d0[i0], d0[idx], d0[i2], d0[i3], fr) * dTrim;
        const float sr = hermite (d1[i0], d1[idx], d1[i2], d1[i3], fr) * dTrim;
        if (inL != nullptr && ! dInternal) { inL[i] += sl; inR[i] += sr; }
        L[i] += sl * dCutNow * gain;
        if (R != nullptr) R[i] += sr * dCutNow * gain;
    }
    deckPos = dPos;
}

void BeatStationProcessor::recordHit (int pad, double stepPosition)
{
    const int len = getPatternLength();
    const int idx = (((int) std::floor (stepPosition + 0.5)) % len + len) % len;   // nearest step
    for (int t = 0; t < numTracks; ++t)
        if ((int) pTrackPad[t]->load() == pad) setStep (t, idx, true);
}

void BeatStationProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    auto raise = [] (std::atomic<float>& a, float v) { float c = a.load(); while (v > c && ! a.compare_exchange_weak (c, v)) {} };
    // ---- audio input (microphone / line in): kept aside before the output is cleared
    const int inCh = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels());
    const bool haveMic = inCh > 0 && micBus.getNumSamples() >= n;
    if (haveMic)
    {
        const float ig = inputGain.load();
        micBus.copyFrom (0, 0, buffer, 0, 0, n);
        micBus.copyFrom (1, 0, buffer, juce::jmin (1, inCh - 1), 0, n);
        micBus.applyGain (0, n, ig);
    }
    buffer.clear();
    inL = inR = nullptr;                                              // the IN meter collects only what comes in from outside
    if (inBus.getNumSamples() >= n) { inBus.clear (0, n); inL = inBus.getWritePointer (0); inR = inBus.getWritePointer (1); }
    if (recArmed.load() && haveMic)                                   // REC: the input shows on the IN meter and is written to the recording
    {
        if (inL != nullptr) { inBus.addFrom (0, 0, micBus, 0, 0, n); inBus.addFrom (1, 0, micBus, 1, 0, n); }
        const int at = recLen.load(), room = recBuf.getNumSamples() - at, take = juce::jmin (n, room);
        if (take > 0)
        {
            recBuf.copyFrom (0, at, micBus, 0, 0, take);
            recBuf.copyFrom (1, at, micBus, 1, 0, take);
            recLen = at + take;
        }
        if (take < n) recArmed = false;                               // full (60 s): stops itself
    }

    midiCollector.removeNextBlockOfMessages (midi, n);                // hardware MIDI keyboards (standalone)
    keyboardState.processNextMidiBuffer (midi, 0, n, true);           // pads and keys clicked on screen

    if (stopAll.exchange (false))
    {
        for (auto& v : voices) if (v.active) stopVoice (v);
        for (auto& sv : synth) if (sv.active) sv.stopping = true;
        preview.active = false;
    }
    if (previewStop.exchange (false)) preview.active = false;
    if (previewReady.exchange (false))
    {
        const juce::SpinLock::ScopedTryLockType l (previewLock);
        if (l.isLocked() && pendingPreview != nullptr)
        {
            preview.s = pendingPreview;
            preview.pos = 0.0;
            preview.ratio = preview.s->sampleRate / sampleRate;
            preview.active = true;
        }
        else previewReady = true;                                     // try again next block
    }
    if (const int w = wholePad.exchange (-1); w >= 0)                 // "Play Full": the whole sample, ignoring the chop
        startVoice (w, 0.9f, padNote[(size_t) w], false, 0.f, true, true);

    // ---- Step sequencer: turns lit steps into note-ons (MIDI channel 16) at the right sample offsets.
    // Inside a DAW it follows the host transport and tempo; otherwise it runs from the Play button.
    double bpm = pBpm->load(), hostStep = 0.0;
    bool hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (pos->getIsPlaying())
            {
                hostPlaying = true;
                hostStep = pos->getPpqPosition().orFallback (0.0) * 4.0;
                if (auto hb = pos->getBpm()) bpm = *hb;
            }
    const bool running = hostPlaying || playing.load();
    const double sps = bpm / 60.0 * 4.0 / sampleRate;                 // steps (16th notes) per sample
    double blockStart = stepPos;
    if (running)
    {
        if (hostPlaying) stepPos = hostStep;
        else if (resetPos.exchange (false)) stepPos = 0.0;
        blockStart = stepPos;

        const int len = getPatternLength();
        const double s1 = blockStart + n * sps, shift = 2.0 * pSwing->load() / 100.0 - 1.0;   // swing delays every second step
        for (int k = (int) std::floor (blockStart) - 1; k <= (int) std::ceil (s1) + 1; ++k)
        {
            const double t = (double) k + ((k & 1) ? shift : 0.0);
            if (t < blockStart || t >= s1) continue;
            const int off = juce::jlimit (0, n - 1, (int) ((t - blockStart) / sps));
            const int idx = ((k % len) + len) % len;
            currentStep.store (idx);
            for (int tr = 0; tr < numTracks; ++tr)
                if (steps[(size_t) (tr * maxSteps + idx)].load() && pTrackMute[tr]->load() < 0.5f)
                    midi.addEvent (juce::MidiMessage::noteOn (16, padNote[(size_t) (int) pTrackPad[tr]->load()], (juce::uint8) 100), off);
        }
        stepPos = s1;
        if (! hostPlaying && stepPos >= 1024.0) stepPos -= 1024.0;
    }
    else currentStep.store (-1);

    // ---- MIDI and sequencer events
    const bool keysMode = pKeys->load() > 0.5f;
    const int inst = (int) pKeysInst->load();                         // 0 = the selected pad's sample, 1+ = a synthesized instrument
    int pos = 0;
    for (const auto meta : midi)
    {
        render (buffer, pos, meta.samplePosition);
        pos = meta.samplePosition;
        const auto msg = meta.getMessage();

        if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            for (auto& v : voices) if (v.active) stopVoice (v);
            for (auto& sv : synth) if (sv.active) sv.stopping = true;
            continue;
        }
        if (msg.isController()) lastMidi = (1 << 24) | (msg.getChannel() << 16) | (msg.getControllerNumber() << 8) | msg.getControllerValue();
        else if (msg.isNoteOn() && msg.getChannel() != 16) lastMidi = (2 << 24) | (msg.getChannel() << 16) | (msg.getNoteNumber() << 8) | (int) msg.getVelocity();
        if (handleMapping (msg)) continue;                            // MIDI Learn: assigned controls, or the message that was just learned

        const int note = msg.getNoteNumber(), ch = msg.getChannel();
        const bool seqNote = (ch == 16);
        const bool keysOn = keysMode && ch != 10 && ! seqNote;        // channel 10 and the sequencer always play the pads
        const bool keysAsDrums = keysOn && keysDrums.load();          // DRUM SET: every key plays a pad (16 pads repeat up the keyboard)
        const bool keysTrigger = keysOn && ! keysAsDrums;
        int pad = -1;
        if (keysAsDrums) pad = (((note - 36) % numPads) + numPads) % numPads;
        else for (int i = 0; i < numPads; ++i) if (padNote[(size_t) i] == note) { pad = i; break; }
        const bool padAsInst = pad >= 0 && ! keysTrigger && ! keysAsDrums && padsInst.load();   // INSTRUMENT on the pads: 16 notes from C3

        if (msg.isNoteOn())
        {
            if (keysTrigger && inst > 0)
                startSynth (note, msg.getFloatVelocity(), inst - 1);
            else if (keysTrigger)
                startVoice (selectedPad.load(), msg.getFloatVelocity(), note, true, (float) (note - 60), false);
            else if (padAsInst)
                startSynth (48 + pad, msg.getFloatVelocity(), juce::jmax (0, inst - 1));
            else if (pad >= 0)
            {
                startVoice (pad, msg.getFloatVelocity(), note, false, 0.f, seqNote);
                if (! seqNote && running && recording.load())            // REC: hits you play land on the nearest step
                    recordHit (pad, blockStart + (double) meta.samplePosition * sps);
            }
        }
        else if (msg.isNoteOff())
        {
            if (keysTrigger && inst > 0) releaseSynth (note);
            if (padAsInst) releaseSynth (48 + pad);
            for (auto& v : voices)                                    // Gate mode pads fade out when the key is let go
                if (v.active && v.gate && ! v.stopping && v.note == note && v.keys == keysTrigger) releaseVoice (v);
        }
    }
    render (buffer, pos, n);
    renderPreview (buffer);
    renderDeck (buffer, n);
    if (inL != nullptr)
    {
        const float l = inBus.getMagnitude (0, 0, n), r = inBus.getMagnitude (1, 0, n);
        raise (peak[0][0], l);  raise (peak[0][1], r);
        if (juce::jmax (l, r) >= 0.999f) clipped[0] = true;
    }

    masterFx.process (buffer);                                        // master EQ and compressor, then the OUT meter

    const float oL = buffer.getMagnitude (0, 0, n), oR = buffer.getNumChannels() > 1 ? buffer.getMagnitude (1, 0, n) : oL;
    raise (peak[1][0], oL);  raise (peak[1][1], oR);
    if (juce::jmax (oL, oR) >= 0.999f) clipped[1] = true;

    uint32_t mask = 0;
    for (auto& v : voices) if (v.active) mask |= (1u << v.pad);
    activePads.store (mask);
}

// ---------------------------------------------------------------- state
static juce::String envToString (const EnvData& e)
{
    juce::StringArray s;
    for (int i = 0; i < e.n; ++i) s.add (juce::String (e.p[i].t, 1) + ":" + juce::String (e.p[i].v, 3));
    return s.joinIntoString (",");
}

static EnvData envFromString (const juce::String& str, const EnvData& fallback)
{
    EnvData e;
    const auto parts = juce::StringArray::fromTokens (str, ",", "");
    int n = 0;
    for (const auto& part : parts)
    {
        if (n >= 8) break;
        e.p[n].t = part.upToFirstOccurrenceOf (":", false, false).getFloatValue();
        e.p[n].v = juce::jlimit (0.f, 1.f, part.fromFirstOccurrenceOf (":", false, false).getFloatValue());
        ++n;
    }
    if (n < 2) return fallback;
    e.n = n;
    return e;
}

void BeatStationProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    for (const char* name : { "PADS", "SEQ", "ENVS", "MIDIMAP", "NAMES" }) state.removeChild (state.getChildWithName (name), nullptr);

    juce::ValueTree pads ("PADS"), envs ("ENVS");
    for (int i = 0; i < numPads; ++i)
    {
        const auto info = getPad (i);
        if (info.file.isNotEmpty())
        {
            juce::ValueTree p ("PAD");
            p.setProperty ("i", i, nullptr);
            p.setProperty ("file", info.file, nullptr);
            p.setProperty ("start", info.start, nullptr);
            p.setProperty ("end", info.end, nullptr);
            if (info.sample != nullptr) p.setProperty ("trim", info.sample->trim, nullptr);
            pads.appendChild (p, nullptr);
        }
        juce::ValueTree e ("ENV");
        e.setProperty ("i", i, nullptr);
        e.setProperty ("amp", envToString (getEnv (i, 0)), nullptr);
        e.setProperty ("flt", envToString (getEnv (i, 1)), nullptr);
        envs.appendChild (e, nullptr);
    }
    state.appendChild (pads, nullptr);
    state.appendChild (envs, nullptr);

    state.setProperty ("inGain", inputGain.load(), nullptr);
    state.setProperty ("keysDrums", keysDrums.load(), nullptr);
    state.setProperty ("padsInst", padsInst.load(), nullptr);

    juce::String bits;
    for (auto& s : steps) bits += s.load() ? "1" : "0";
    juce::ValueTree seq ("SEQ");
    seq.setProperty ("steps", bits, nullptr);
    state.appendChild (seq, nullptr);

    juce::ValueTree names ("NAMES");
    for (int t = 0; t < numTracks; ++t) names.setProperty ("t" + juce::String (t), trackName[(size_t) t], nullptr);
    state.appendChild (names, nullptr);

    juce::ValueTree midiTree ("MIDIMAP");
    {
        const juce::SpinLock::ScopedLockType l (mapLock);
        juce::StringArray notes;
        for (auto n : padNote) notes.add (juce::String (n));
        midiTree.setProperty ("padNotes", notes.joinIntoString (","), nullptr);
        for (const auto& m : maps)
        {
            juce::ValueTree e ("MAP");
            e.setProperty ("cc", m.isCC, nullptr);
            e.setProperty ("ch", m.channel, nullptr);
            e.setProperty ("num", m.number, nullptr);
            e.setProperty ("target", m.target, nullptr);
            midiTree.appendChild (e, nullptr);
        }
    }
    state.appendChild (midiTree, nullptr);

    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void BeatStationProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    inputGain = (float) apvts.state.getProperty ("inGain", 1.0f);
    keysDrums = (bool) apvts.state.getProperty ("keysDrums", false);
    padsInst = (bool) apvts.state.getProperty ("padsInst", false);

    const auto padsTree = apvts.state.getChildWithName ("PADS");
    if (padsTree.isValid())
    {
        for (int i = 0; i < numPads; ++i) clearPad (i);
        std::map<juce::String, std::shared_ptr<const Sample>> cache;  // chopped pads share one decoded sample
        for (auto p : padsTree)
        {
            const int i = p["i"];
            const juce::String path = p["file"].toString();
            if (i < 0 || i >= numPads) continue;
            if (cache.find (path) == cache.end())
            {
                const bool ok = path.startsWith ("builtin:") ? loadBuiltin (i, path.fromFirstOccurrenceOf (":", false, false))
                                                             : loadSample (i, juce::File (path), p.hasProperty ("trim") ? (float) p["trim"] : 1.f);
                if (! ok) continue;                                   // file moved or missing: pad stays empty
                cache[path] = getPad (i).sample;
            }
            assign (i, cache[path], path, (float) p["start"], (float) p["end"]);
        }
    }

    for (auto e : apvts.state.getChildWithName ("ENVS"))
    {
        const int i = e["i"];
        if (i < 0 || i >= numPads) continue;
        setEnv (i, 0, envFromString (e["amp"].toString(), getEnv (i, 0)));
        setEnv (i, 1, envFromString (e["flt"].toString(), getEnv (i, 1)));
    }

    const auto seq = apvts.state.getChildWithName ("SEQ");
    if (seq.isValid())
    {
        const juce::String bits = seq["steps"].toString();
        for (int i = 0; i < (int) steps.size() && i < bits.length(); ++i) steps[(size_t) i].store (bits[i] == '1');
    }

    const auto names = apvts.state.getChildWithName ("NAMES");
    for (int t = 0; t < numTracks; ++t) trackName[(size_t) t] = names.getProperty ("t" + juce::String (t)).toString();

    const auto midiTree = apvts.state.getChildWithName ("MIDIMAP");
    if (midiTree.isValid())
    {
        const juce::SpinLock::ScopedLockType l (mapLock);
        const auto notes = juce::StringArray::fromTokens (midiTree["padNotes"].toString(), ",", "");
        for (int i = 0; i < numPads && i < notes.size(); ++i) padNote[(size_t) i] = notes[i].getIntValue();
        maps.clear();
        for (auto e : midiTree)
        {
            MidiMap m;
            m.isCC = (bool) e["cc"];
            m.channel = e.hasProperty ("ch") ? (int) e["ch"] : 1;
            m.number = e["num"];
            m.target = e["target"].toString();
            if (m.target.startsWith ("param:")) m.param = apvts.getParameter (m.target.fromFirstOccurrenceOf (":", false, false));
            if (m.target.startsWith ("sel:")) m.padParamIdx = m.target.fromFirstOccurrenceOf (":", false, false).getIntValue();
            maps.push_back (m);
        }
    }
    ++mapVersion;
}

juce::AudioProcessorEditor* BeatStationProcessor::createEditor() { return new BeatStationEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BeatStationProcessor(); }
