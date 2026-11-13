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
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), false)     // optional: the standalone app enables it
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
        padNote[(size_t) p] = firstNote + p;
        slots[(size_t) p].fltEnv.p[1] = { 400.f, 0.f };                                       // filter envelope starts as a falling curve
    }
    pKeys = apvts.getRawParameterValue (ParamID::keysMode);
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

bool BeatStationProcessor::decodeInto (int pad, std::unique_ptr<juce::AudioFormatReader> reader, const juce::String& name, const juce::String& fileKey)
{
    if (reader == nullptr || reader->lengthInSamples <= 0) return false;
    const int len = (int) std::min (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60.0));   // cap at 60 s
    auto s = std::make_shared<Sample>();
    s->data.setSize (2, len);
    reader->read (&s->data, 0, len, 0, true, true);
    if (reader->numChannels == 1) s->data.copyFrom (1, 0, s->data, 0, 0, len);
    s->sampleRate = reader->sampleRate;
    s->name = name;
    assign (pad, s, fileKey, 0.f, 1.f);
    return true;
}

bool BeatStationProcessor::loadSample (int pad, const juce::File& file)
{
    return decodeInto (pad, std::unique_ptr<juce::AudioFormatReader> (formats.createReaderFor (file)),
                       file.getFileNameWithoutExtension(), file.getFullPathName());
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
                       sound, "builtin:" + resourceName);
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
    const float g = previewGain.load();
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        if (preview.pos >= len - 1.0) { preview.active = false; break; }
        const int idx = (int) preview.pos;
        const float fr = (float) (preview.pos - (double) idx);
        L[i] += (d0[idx] + (d0[idx + 1] - d0[idx]) * fr) * g;
        if (R != nullptr) R[i] += (d1[idx] + (d1[idx + 1] - d1[idx]) * fr) * g;
        preview.pos += preview.ratio;
    }
}

// ---------------------------------------------------------------- MIDI learn
juce::String BeatStationProcessor::describeMapping (const juce::String& target) const
{
    const juce::SpinLock::ScopedLockType l (mapLock);
    if (target.startsWith ("pad:")) return "Note " + juce::String (padNote[(size_t) target.fromFirstOccurrenceOf (":", false, false).getIntValue()]);
    for (const auto& m : maps) if (m.target == target) return (m.isCC ? "CC " : "Note ") + juce::String (m.number);
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

void BeatStationProcessor::applyLearn (bool isCC, int number)
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
        if (target.startsWith ("param:") && ! isCC) return;           // knobs learn from controllers only
        maps.erase (std::remove_if (maps.begin(), maps.end(),
                                    [&] (const MidiMap& m) { return m.target == target || (m.isCC == isCC && m.number == number); }), maps.end());
        MidiMap m;
        m.isCC = isCC;
        m.number = number;
        m.target = target;
        if (target.startsWith ("param:")) m.param = apvts.getParameter (target.fromFirstOccurrenceOf (":", false, false));
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
    const int number = isCC ? msg.getControllerNumber() : msg.getNoteNumber();
    if (isCC && number >= 120) return false;                          // "all notes off" and friends are not mappable

    if (learnTarget.isNotEmpty())
    {
        const bool waiting = learnTarget.startsWith ("pad:") ? ! isNote : (learnTarget.startsWith ("param:") ? ! isCC : false);
        if (waiting) return false;                                    // wrong kind of message for this target
        applyLearn (isCC, number);
        return true;
    }

    for (const auto& m : maps)
        if (m.isCC == isCC && m.number == number)
        {
            if (m.param != nullptr) { if (isCC) m.param->setValueNotifyingHost ((float) msg.getControllerValue() / 127.f); }
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
    v->note = note;    v->keys = keys; v->gate = (mode == 2) && ! noGate && ! whole; v->group = group;
    v->stopping = false; v->stopGain = 1.f; v->releasing = false; v->relGain = 1.f; v->relStep = 0.f;
    v->autoRelease = ! v->gate;                                       // one-shots fade themselves out as the chop ends
    v->pos = start;    v->end = end;
    v->ratio = smp->sampleRate / sampleRate * std::pow (2.0, (double) semis / 12.0);
    v->fadeOutLen = juce::jmax (1.0, 0.003 * sampleRate * v->ratio);
    v->relSamples = (double) P (pRel) * 0.001 * sampleRate;
    v->gainL = gain * std::cos (angle) * 1.4142f;                     // equal-power pan, unity at centre
    v->gainR = gain * std::sin (angle) * 1.4142f;
    v->ampEnv = ae;
    v->fltEnv = fe;
    v->fType = -1;                                                    // forces the filter to be set up on the first control tick
    v->filtOn = false;
    v->filt.reset();
}

void BeatStationProcessor::render (juce::AudioBuffer<float>& buffer, int from, int to)
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
            float l = d0[idx] + (d0[idx + 1] - d0[idx]) * fr;
            float r = d1[idx] + (d1[idx + 1] - d1[idx]) * fr;

            if ((v.age & 15) == 0)                                    // every 16 samples: read the filter knobs live, so turning them is heard at once
            {
                const int ft = (int) raw[v.pad][pFType]->load();
                const float cut = juce::jlimit (20.f, maxCut, raw[v.pad][pFCut]->load() * std::pow (2.f, raw[v.pad][pFAmt]->load() * v.fltEnv.at (tMs)));
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
    const int inCh = getTotalNumInputChannels();
    if (inCh > 0)                                                     // input meter (the audio input is not played back yet)
    {
        const float l = buffer.getMagnitude (0, 0, n), r = inCh > 1 ? buffer.getMagnitude (1, 0, n) : l;
        raise (peak[0][0], l);  raise (peak[0][1], r);
        if (juce::jmax (l, r) >= 0.999f) clipped[0] = true;
    }
    buffer.clear();

    midiCollector.removeNextBlockOfMessages (midi, n);                // hardware MIDI keyboards (standalone)
    keyboardState.processNextMidiBuffer (midi, 0, n, true);           // pads and keys clicked on screen

    if (stopAll.exchange (false))
    {
        for (auto& v : voices) if (v.active) stopVoice (v);
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
    int pos = 0;
    for (const auto meta : midi)
    {
        render (buffer, pos, meta.samplePosition);
        pos = meta.samplePosition;
        const auto msg = meta.getMessage();

        if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            for (auto& v : voices) if (v.active) stopVoice (v);
            continue;
        }
        if (handleMapping (msg)) continue;                            // MIDI Learn: assigned controls, or the message that was just learned

        const int note = msg.getNoteNumber(), ch = msg.getChannel();
        const bool seqNote = (ch == 16);
        const bool keysTrigger = keysMode && ch != 10 && ! seqNote;   // channel 10 and the sequencer always play the pads
        int pad = -1;
        for (int i = 0; i < numPads; ++i) if (padNote[(size_t) i] == note) { pad = i; break; }

        if (msg.isNoteOn())
        {
            if (keysTrigger)
                startVoice (selectedPad.load(), msg.getFloatVelocity(), note, true, (float) (note - 60), false);
            else if (pad >= 0)
            {
                startVoice (pad, msg.getFloatVelocity(), note, false, 0.f, seqNote);
                if (! seqNote && running && recording.load())            // REC: hits you play land on the nearest step
                    recordHit (pad, blockStart + (double) meta.samplePosition * sps);
            }
        }
        else if (msg.isNoteOff())
        {
            for (auto& v : voices)                                    // Gate mode pads fade out when the key is let go
                if (v.active && v.gate && ! v.stopping && v.note == note && v.keys == keysTrigger) releaseVoice (v);
        }
    }
    render (buffer, pos, n);
    renderPreview (buffer);

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
                                                             : loadSample (i, juce::File (path));
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
            m.number = e["num"];
            m.target = e["target"].toString();
            if (m.target.startsWith ("param:")) m.param = apvts.getParameter (m.target.fromFirstOccurrenceOf (":", false, false));
            maps.push_back (m);
        }
    }
    ++mapVersion;
}

juce::AudioProcessorEditor* BeatStationProcessor::createEditor() { return new BeatStationEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BeatStationProcessor(); }
