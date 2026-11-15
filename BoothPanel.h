#pragma once
#include "PluginProcessor.h"
#include "Theme.h"
#include "Widgets.h"
#include "TracksPanel.h"
#include "BoothPresets.h"

// The vocal chain as a row of tabs: MIC first (always), then the eight vocal effects in the order the voice goes through them.
// Click a tab to see its knobs, click its light to switch it on or off, drag a tab sideways to move it.
class BoothStrip : public juce::Component, private juce::Timer
{
public:
    static juce::Colour tuneColour() { return juce::Colour (0xffd68cff); }
    explicit BoothStrip (BeatStationProcessor& p) : proc (p), booth (p.booth) { startTimerHz (12); }
    std::function<void (int fx)> onSelect;
    int selected = bComp;

    void paint (juce::Graphics& g) override
    {
        const auto mic = juce::Rectangle<float> (0.f, 2.f, micW, (float) getHeight() - 4.f);
        g.setColour (Theme::amber.withAlpha (0.2f));
        g.fillRoundedRectangle (mic, 6.f);
        g.setColour (Theme::amber);
        g.drawRoundedRectangle (mic.reduced (0.5f), 6.f, 1.4f);
        g.setFont (juce::Font (11.5f, juce::Font::bold));
        g.drawText ("MIC", mic.toNearestInt(), juce::Justification::centred);
        g.setColour (Theme::dim);
        g.drawText (">", (int) micW, 0, gap, getHeight(), juce::Justification::centred);
        {
            const auto r = tuneChip();                                   // TUNE: always second, straight after the microphone
            const bool on = booth.tuneOn(), sel = selected == numBooth;
            g.setColour (sel ? tuneColour().withAlpha (0.22f) : Theme::panel);
            g.fillRoundedRectangle (r, 6.f);
            g.setColour (sel ? tuneColour() : Theme::grid.brighter (0.15f));
            g.drawRoundedRectangle (r.reduced (0.5f), 6.f, sel ? 1.8f : 1.f);
            const auto led = juce::Rectangle<float> (10.f, 10.f).withCentre ({ r.getX() + 12.f, r.getCentreY() });
            if (on) { g.setColour (tuneColour().withAlpha (0.35f)); g.fillEllipse (led.expanded (3.f)); }
            g.setColour (on ? tuneColour() : Theme::tickOff);
            g.fillEllipse (led);
            g.setColour (on ? Theme::text : Theme::dim);
            g.drawText ("TUNE", r.withTrimmedLeft (20.f).toNearestInt(), juce::Justification::centred);
            g.setColour (Theme::dim);
            g.drawText (">", (int) r.getRight(), 0, gap, getHeight(), juce::Justification::centred);
        }
        for (int slot = 0; slot < numBooth; ++slot)
        {
            const int fx = booth.getOrder (slot);
            const auto r = chip (slot);
            const auto col = BoothInfo::colours[fx];
            const bool on = booth.isOn (fx), sel = fx == selected;
            g.setColour (sel ? col.withAlpha (0.22f) : Theme::panel);
            g.fillRoundedRectangle (r, 6.f);
            g.setColour (sel ? col : Theme::grid.brighter (0.15f));
            g.drawRoundedRectangle (r.reduced (0.5f), 6.f, sel ? 1.8f : 1.f);
            const auto led = juce::Rectangle<float> (10.f, 10.f).withCentre ({ r.getX() + 12.f, r.getCentreY() });
            if (on) { g.setColour (col.withAlpha (0.35f)); g.fillEllipse (led.expanded (3.f)); }
            g.setColour (on ? col : Theme::tickOff);
            g.fillEllipse (led);
            g.setColour (on ? Theme::text : Theme::dim);
            g.drawText (BoothInfo::names[fx], r.withTrimmedLeft (20.f).toNearestInt(), juce::Justification::centred);
            g.setColour (Theme::dim);
            g.drawText (">", (int) r.getRight(), 0, gap, getHeight(), juce::Justification::centred);
        }
        g.setColour (Theme::dim);
        g.drawText ("MIX", getWidth() - 34, 0, 34, getHeight(), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (tuneChip().contains (e.position))
        {
            if (e.position.x - tuneChip().getX() < 22.f)
                if (auto* prm = proc.apvts.getParameter ("bth_tune_on")) { prm->beginChangeGesture(); prm->setValueNotifyingHost (booth.tuneOn() ? 0.f : 1.f); prm->endChangeGesture(); }
            selected = numBooth;
            dragSlot = -1;
            if (onSelect) onSelect (numBooth);
            repaint();
            return;
        }
        dragSlot = slotAt (e.position.x);
        if (dragSlot < 0) return;
        const int fx = booth.getOrder (dragSlot);
        if (e.position.x - chip (dragSlot).getX() < 22.f)
        {
            if (auto* prm = proc.apvts.getParameter (BoothInfo::onIds[fx])) { prm->beginChangeGesture(); prm->setValueNotifyingHost (booth.isOn (fx) ? 0.f : 1.f); prm->endChangeGesture(); }
            dragSlot = -1;
        }
        selected = fx;
        if (onSelect) onSelect (fx);
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragSlot < 0 || std::abs (e.getDistanceFromDragStartX()) < 8) return;
        for (int s = 0; s < numBooth; ++s)
            if (e.position.x >= chip (s).getX() && e.position.x <= chip (s).getRight() && s != dragSlot) { booth.moveModule (dragSlot, s); dragSlot = s; repaint(); break; }
    }

private:
    static constexpr float micW = 50.f, tuneW = 70.f;
    static constexpr int gap = 12;
    juce::Rectangle<float> tuneChip() const { return { micW + (float) gap, 2.f, tuneW, (float) getHeight() - 4.f }; }
    juce::Rectangle<float> chip (int slot) const
    {
        const float x0 = micW + tuneW + 2.f * (float) gap, w = ((float) getWidth() - x0 - 34.f - (float) (gap * numBooth)) / (float) numBooth;
        return { x0 + (float) slot * (w + (float) gap), 2.f, w, (float) getHeight() - 4.f };
    }
    int slotAt (float x) const { for (int s = 0; s < numBooth; ++s) if (x >= chip (s).getX() && x <= chip (s).getRight()) return s; return -1; }
    void timerCallback() override { if (isShowing()) repaint(); }
    BeatStationProcessor& proc;
    BoothChain& booth;
    int dragSlot = -1;
};

// THE BOOTH: where vocals are recorded and shaped. Pick a microphone character, a one-click vocal chain, and record onto the vocal tracks below.
class BoothPanel : public juce::Component, private juce::Timer
{
public:
    std::function<void()> onBack;
    std::function<void (const juce::String& target)> learn;

    explicit BoothPanel (BeatStationProcessor& p) : proc (p), booth (p.booth), strip (p), vocalTracks (p, 0, 4)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &strip, &backBtn, &prevBtn, &nextBtn, &presetBtn, &burnBtn, &vocalTracks, &outKnob }) addAndMakeVisible (*c);
        backBtn.onClick = [this] { if (onBack) onBack(); };
        strip.onSelect = [this] (int fx) { select (fx); };
        prevBtn.onClick = [this] { step (-1); };
        nextBtn.onClick = [this] { step (1); };
        presetBtn.onClick = [this] { presetMenu(); };
        presetBtn.setColour (juce::TextButton::textColourOffId, Theme::amber);
        burnBtn.setClickingTogglesState (true);
        burnBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd0221f));
        burnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "bth_burn", burnBtn);
        outKnob.attach (p.apvts, "bth_out");
        select (bComp);
        startTimerHz (24);
    }

    void select (int fx)
    {
        current = fx;
        strip.selected = fx;
        knobs.clear();
        struct K { const char* id; const char* title; const char* suffix; };
        std::vector<K> list;
        switch (fx)
        {
            case bGate:   list = { { "bth_gate_thresh", "THRESHOLD", " dB" }, { "bth_gate_rel", "RELEASE", " ms" } }; break;
            case bDeEss:  list = { { "bth_deess_freq", "FREQUENCY", " Hz" }, { "bth_deess_thresh", "THRESHOLD", " dB" }, { "bth_deess_max", "MAX CUT", " dB" } }; break;
            case bComp:   list = { { "bth_comp_thresh", "THRESHOLD", " dB" }, { "bth_comp_ratio", "RATIO", ":1" }, { "bth_comp_attack", "ATTACK", " ms" }, { "bth_comp_release", "RELEASE", " ms" }, { "bth_comp_makeup", "MAKEUP", " dB" } }; break;
            case bEq:     list = { { "bth_eq_lowcut", "LOW CUT", " Hz" }, { "bth_eq_mud", "MUD 320", " dB" }, { "bth_eq_pres", "PRESENCE 4k", " dB" }, { "bth_eq_air", "AIR 11k", " dB" } }; break;
            case bDrive:  list = { { "bth_drive_amt", "DRIVE", " dB" }, { "bth_drive_mix", "MIX", "" } }; break;
            case bDouble: list = { { "bth_dbl_mix", "AMOUNT", "" }, { "bth_dbl_depth", "SPREAD", " ms" } }; break;
            case bDelay:  list = { { "bth_dly_sync", "NOTE", "" }, { "bth_dly_fb", "REPEATS", "" }, { "bth_dly_ping", "PING-PONG", "" }, { "bth_dly_mix", "MIX", "" } }; break;
            case numBooth: list = { { "bth_tune_key", "KEY", "" }, { "bth_tune_scale", "SCALE", "" }, { "bth_tune_speed", "SPEED", " ms" }, { "bth_tune_amount", "AMOUNT", "" } }; break;
            case bReverb: list = { { "bth_rev_decay", "DECAY", " s" }, { "bth_rev_predelay", "PRE-DELAY", " ms" }, { "bth_rev_mix", "MIX", "" } }; break;
            default: break;
        }
        for (const auto& k : list)
        {
            auto knob = std::make_unique<LabeledKnob> (k.title, k.suffix, colourOf (fx));
            knob->attach (proc.apvts, k.id);
            if (learn) { auto cb = learn; knob->onLearnMenu = [cb] (const juce::String& id) { cb ("param:" + id); }; }
            addAndMakeVisible (*knob);
            knobs.push_back (std::move (knob));
        }
        resized();
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto head = r.removeFromTop (38);
        backBtn.setBounds (head.removeFromRight (120).reduced (0, 2));
        head.removeFromRight (10);
        strip.setBounds (head);
        r.removeFromTop (6);
        auto bar = r.removeFromTop (28);
        bar.removeFromLeft (62);
        prevBtn.setBounds (bar.removeFromLeft (30));    bar.removeFromLeft (4);
        presetBtn.setBounds (bar.removeFromLeft (330)); bar.removeFromLeft (4);
        nextBtn.setBounds (bar.removeFromLeft (30));    bar.removeFromLeft (16);
        burnBtn.setBounds (bar.removeFromLeft (150));
        hintRect = bar.withTrimmedLeft (12);
        r.removeFromTop (8);

        vocalTracks.setBounds (r.removeFromBottom (28 + 4 + 4 * 34));
        r.removeFromBottom (8);
        knobPanel = r.removeFromBottom (108);
        r.removeFromBottom (8);
        micList = r.removeFromLeft (236);
        r.removeFromLeft (10);
        meters = r.removeFromRight (150);
        r.removeFromRight (10);
        graph = r;

        auto k = knobPanel.reduced (6, 6);
        outKnob.setBounds (k.removeFromRight (100));
        if (! knobs.empty())
        {
            const int kw = juce::jmin (120, k.getWidth() / (int) knobs.size());
            k = k.withSizeKeepingCentre (kw * (int) knobs.size(), k.getHeight());
            for (auto& knob : knobs) knob->setBounds (k.removeFromLeft (kw));
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::dim);
        g.setFont (juce::Font (11.f, juce::Font::bold));
        g.drawText ("PRESET", 0, presetBtn.getY(), 58, presetBtn.getHeight(), juce::Justification::centredLeft);
        g.setFont (11.f);
        g.drawFittedText (burnBtn.getToggleState() ? "BURN IN is on: the mic character is recorded into the take and cannot be changed afterwards."
                                                   : "The mic character is added while you listen. Record now, choose a different microphone later.",
                          hintRect, juce::Justification::centredLeft, 2);
        for (auto rect : { micList, graph, meters, knobPanel })
        {
            g.setColour (rect == graph ? Theme::graphBg : Theme::panel);
            g.fillRoundedRectangle (rect.toFloat(), 8.f);
            g.setColour (Theme::grid.brighter (0.1f));
            g.drawRoundedRectangle (rect.toFloat().reduced (0.5f), 8.f, 1.f);
        }

        // ---- microphone list
        const auto& mics = fxdsp::micProfiles();
        const int cur = booth.micIndex();
        g.setFont (juce::Font (11.f, juce::Font::bold));
        g.setColour (Theme::dim);
        g.drawText ("MICROPHONE", micList.getX() + 10, micList.getY() + 6, 200, 14, juce::Justification::left);
        for (int i = 0; i < (int) mics.size(); ++i)
        {
            const auto row = micRow (i);
            if (i == cur)
            {
                g.setColour (Theme::amber.withAlpha (0.18f));
                g.fillRoundedRectangle (row.toFloat(), 4.f);
                g.setColour (Theme::amber);
                g.fillRect (row.getX(), row.getY() + 3, 3, row.getHeight() - 6);
            }
            g.setColour (i == cur ? Theme::amber : Theme::text);
            g.setFont (12.f);
            g.drawText (mics[(size_t) i].name, row.withTrimmedLeft (10), juce::Justification::centredLeft);
        }

        if (current == numBooth) drawTune (g);
        else
        {
        // ---- tone graph: the microphone (amber), the vocal EQ (green), and both together (white)
        const auto b = graph.toFloat().reduced (12.f, 10.f).withTrimmedTop (40.f);
        auto X = [&] (float hz) { return b.getX() + b.getWidth() * std::log10 (hz / 20.f) / 3.f; };
        auto Y = [&] (float db) { return b.getCentreY() - juce::jlimit (-12.f, 12.f, db) / 12.f * b.getHeight() * 0.5f; };
        g.setFont (10.f);
        for (float hz : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
        {
            g.setColour (Theme::grid);
            g.drawVerticalLine ((int) X (hz), b.getY(), b.getBottom());
            g.setColour (Theme::dim);
            g.drawText (hz >= 1000.f ? juce::String ((int) (hz / 1000.f)) + "k" : juce::String ((int) hz), (int) X (hz) + 2, (int) b.getBottom() - 12, 30, 12, juce::Justification::left);
        }
        for (int db : { -12, -6, 0, 6, 12 })
        {
            g.setColour (db == 0 ? Theme::grid.brighter (0.4f) : Theme::grid);
            g.drawHorizontalLine ((int) Y ((float) db), b.getX(), b.getRight());
        }
        shownEq.lowCut = pv ("bth_eq_lowcut"); shownEq.mudDb = pv ("bth_eq_mud"); shownEq.presDb = pv ("bth_eq_pres"); shownEq.airDb = pv ("bth_eq_air");
        shownEq.prepare (proc.getSR());
        const bool eqOn = booth.isOn (bEq);
        juce::Path micP, eqP, sumP;
        for (float x = b.getX(); x <= b.getRight(); x += 2.f)
        {
            const float hz = 20.f * std::pow (10.f, 3.f * (x - b.getX()) / b.getWidth());
            const float m = booth.micResponseDb (hz), e = eqOn ? shownEq.responseDb (hz) : 0.f;
            if (x == b.getX()) { micP.startNewSubPath (x, Y (m)); eqP.startNewSubPath (x, Y (e)); sumP.startNewSubPath (x, Y (m + e)); }
            else { micP.lineTo (x, Y (m)); eqP.lineTo (x, Y (e)); sumP.lineTo (x, Y (m + e)); }
        }
        juce::Path fill (micP);
        fill.lineTo (b.getRight(), Y (0.f));
        fill.lineTo (b.getX(), Y (0.f));
        fill.closeSubPath();
        g.setColour (Theme::amber.withAlpha (0.2f));
        g.fillPath (fill);
        if (eqOn) { g.setColour (Theme::green.withAlpha (0.8f)); g.strokePath (eqP, juce::PathStrokeType (1.5f)); }
        g.setColour (Theme::amber);
        g.strokePath (micP, juce::PathStrokeType (2.4f));
        g.setColour (Theme::text);
        g.strokePath (sumP, juce::PathStrokeType (1.6f));

        const auto& m = mics[(size_t) juce::jlimit (0, (int) mics.size() - 1, cur)];
        g.setColour (Theme::amber);
        g.setFont (juce::Font (15.f, juce::Font::bold));
        g.drawText (m.name, graph.getX() + 12, graph.getY() + 8, 220, 18, juce::Justification::left);
        g.setColour (Theme::dim);
        g.setFont (11.f);
        if (cur > 0) g.drawText (juce::String ("in the style of the ") + m.inspiredBy, graph.getX() + 12, graph.getY() + 27, 320, 14, juce::Justification::left);
        g.setColour (Theme::text);
        g.drawFittedText (m.note, graph.getX() + 240, graph.getY() + 8, graph.getWidth() - 252, 32, juce::Justification::topRight, 2);
        g.setColour (Theme::dim);
        g.setFont (10.f);
        g.drawText ("amber = microphone   green = vocal EQ   white = both", (int) b.getX(), (int) b.getY() + 2, 320, 12, juce::Justification::left);
        }

        // ---- meters
        auto mr = meters.reduced (10, 8);
        auto bar = [&] (const char* label, float frac, juce::Colour col, const juce::String& value)
        {
            auto row = mr.removeFromTop (38);
            g.setColour (Theme::dim);
            g.setFont (juce::Font (10.f, juce::Font::bold));
            g.drawText (label, row.removeFromTop (13), juce::Justification::left);
            g.setColour (Theme::text);
            g.drawText (value, row.getX(), row.getY() - 13, row.getWidth(), 13, juce::Justification::right);
            auto track = row.removeFromTop (12).toFloat();
            g.setColour (Theme::graphBg);
            g.fillRoundedRectangle (track, 3.f);
            g.setColour (col);
            g.fillRoundedRectangle (track.withWidth (track.getWidth() * juce::jlimit (0.f, 1.f, frac)), 3.f);
        };
        const float inDb = juce::Decibels::gainToDecibels (lvIn, -60.f), outDb = juce::Decibels::gainToDecibels (lvOut, -60.f);
        bar ("VOICE IN", (inDb + 60.f) / 60.f, Theme::amber, juce::String (juce::roundToInt (inDb)) + " dB");
        bar ("GATE", booth.isOn (bGate) ? 1.f - booth.gateGain.load() : 0.f, BoothInfo::colours[bGate], booth.isOn (bGate) && booth.gateGain.load() < 0.5f ? "closed" : "open");
        bar ("DE-ESS", booth.isOn (bDeEss) ? booth.deessCut.load() / 12.f : 0.f, BoothInfo::colours[bDeEss], "-" + juce::String (booth.isOn (bDeEss) ? booth.deessCut.load() : 0.f, 1) + " dB");
        bar ("COMPRESSION", booth.isOn (bComp) ? -booth.compGr.load() / 18.f : 0.f, BoothInfo::colours[bComp], juce::String (booth.isOn (bComp) ? booth.compGr.load() : 0.f, 1) + " dB");
        bar ("BOOTH OUT", (outDb + 60.f) / 60.f, Theme::green, juce::String (juce::roundToInt (outDb)) + " dB");

        g.setColour (colourOf (current));
        g.setFont (juce::Font (11.f, juce::Font::bold));
        g.drawText (current == numBooth ? "TUNE" : BoothInfo::names[current], knobPanel.getX() + 12, knobPanel.getY() + 6, 100, 14, juce::Justification::left);
    }

    void mouseDown (const juce::MouseEvent& e) override                 // choose a microphone
    {
        for (int i = 0; i < (int) fxdsp::micProfiles().size(); ++i)
            if (micRow (i).contains (e.getPosition()))
                if (auto* prm = proc.apvts.getParameter ("bth_mic")) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i)); prm->endChangeGesture(); repaint(); }
    }

private:
    static juce::Colour colourOf (int fx) { return fx == numBooth ? BoothStrip::tuneColour() : BoothInfo::colours[fx]; }

    // The TUNE screen: the note you are singing, the note it is moving you to, how far off you are, and both drawn over the last few seconds.
    void drawTune (juce::Graphics& g)
    {
        const auto col = BoothStrip::tuneColour();
        const float det = booth.tuneOn() ? booth.tuneDetected.load() : 0.f, tgt = booth.tuneOn() ? booth.tuneTarget.load() : 0.f;
        auto area = graph.reduced (12, 10);
        auto left = area.removeFromLeft (190);
        area.removeFromLeft (10);

        g.setColour (Theme::dim);
        g.setFont (juce::Font (11.f, juce::Font::bold));
        g.drawText ("PITCH CORRECTION", left.removeFromTop (16), juce::Justification::left);
        g.setColour (col);
        g.setFont (juce::Font (64.f, juce::Font::bold));
        g.drawText (tgt > 0.f ? juce::MidiMessage::getMidiNoteName ((int) tgt, true, true, 3) : juce::String ("--"), left.removeFromTop (76), juce::Justification::centredLeft);
        const float cents = det > 0.f ? (det - tgt) * 100.f : 0.f;
        g.setColour (Theme::text);
        g.setFont (13.f);
        g.drawText (det > 0.f ? "you are " + juce::String (juce::roundToInt (std::abs (cents))) + " cents " + (cents >= 0.f ? "sharp" : "flat") : juce::String (booth.tuneOn() ? "waiting for a note..." : "TUNE is off"),
                    left.removeFromTop (18), juce::Justification::left);
        left.removeFromTop (8);
        auto bar = left.removeFromTop (16).toFloat();                    // how far off the note: flat on the left, sharp on the right
        g.setColour (Theme::tickOff);
        g.fillRoundedRectangle (bar, 4.f);
        g.setColour (Theme::text.withAlpha (0.6f));
        g.drawVerticalLine ((int) bar.getCentreX(), bar.getY() - 2.f, bar.getBottom() + 2.f);
        if (det > 0.f)
        {
            const float x = bar.getCentreX() + juce::jlimit (-50.f, 50.f, cents) / 50.f * bar.getWidth() * 0.5f;
            g.setColour (std::abs (cents) < 10.f ? Theme::green : col);
            g.fillRoundedRectangle (juce::jmin (x, bar.getCentreX()), bar.getY(), std::abs (x - bar.getCentreX()), bar.getHeight(), 4.f);
            g.fillEllipse (x - 6.f, bar.getCentreY() - 6.f, 12.f, 12.f);
        }
        g.setColour (Theme::dim);
        g.setFont (10.f);
        g.drawText ("flat", left.removeFromTop (14), juce::Justification::left);
        const int key = (int) pv ("bth_tune_key"), scale = (int) pv ("bth_tune_scale");
        static const char* keyNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        g.setColour (Theme::text);
        g.setFont (12.f);
        g.drawFittedText (juce::String ("Key of ") + keyNames[key] + ", " + fxdsp::tune::scaleNames[scale] + ".  Speed 0 = hard snap, 100-300 = natural.  Delay " +
                          juce::String (juce::roundToInt (1000.0 * booth.tuneLatency() / proc.getSR())) + " ms.", left, juce::Justification::topLeft, 4);

        // history: grey = what you sang, colour = where it was moved to, against the notes of the scale
        const auto b = area.toFloat();
        float centre = lastCentre;
        for (int i = histN - 1; i >= 0; --i) { const float v = histTgt[(size_t) ((histHead + i) % histN)]; if (v > 0.f) { centre = v; break; } }
        lastCentre += (std::abs (centre - lastCentre) > 5.f ? 1.f : 0.15f) * (centre - lastCentre);   // jumps straight to a new register, glides within it
        const float lo = lastCentre - 7.f, hi = lastCentre + 7.f;
        auto Y = [&] (float note) { return b.getBottom() - (note - lo) / (hi - lo) * b.getHeight(); };
        for (int n = (int) std::ceil (lo); n <= (int) hi; ++n)
        {
            const bool in = fxdsp::tune::allowed (n, key, scale);
            g.setColour (in ? Theme::grid.brighter (0.25f) : Theme::grid.withAlpha (0.5f));
            g.drawHorizontalLine ((int) Y ((float) n), b.getX() + 30.f, b.getRight());
            g.setColour (in ? Theme::text : Theme::tickOff);
            g.setFont (10.f);
            g.drawText (juce::MidiMessage::getMidiNoteName (n, true, true, 3), (int) b.getX(), (int) Y ((float) n) - 6, 28, 12, juce::Justification::left);
        }
        juce::Path sung, tuned;
        bool sOn = false, tOn = false;
        for (int i = 0; i < histN; ++i)
        {
            const size_t idx = (size_t) ((histHead + i) % histN);
            const float x = b.getX() + 30.f + (b.getWidth() - 30.f) * (float) i / (float) (histN - 1);
            const float d = histDet[idx], t = histTgt[idx];
            if (d > lo && d < hi) { if (sOn) sung.lineTo (x, Y (d)); else sung.startNewSubPath (x, Y (d)); sOn = true; } else sOn = false;
            if (t > lo && t < hi) { if (tOn) tuned.lineTo (x, Y (t)); else tuned.startNewSubPath (x, Y (t)); tOn = true; } else tOn = false;
        }
        g.setColour (Theme::dim);
        g.strokePath (sung, juce::PathStrokeType (1.6f));
        g.setColour (col);
        g.strokePath (tuned, juce::PathStrokeType (3.f));
    }

    float pv (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    juce::Rectangle<int> micRow (int i) const { return { micList.getX() + 6, micList.getY() + 24 + i * 23, micList.getWidth() - 12, 22 }; }
    int currentPreset() const { return (int) proc.apvts.state.getProperty ("boothPreset", -1); }
    void load (int index)
    {
        applyBoothPreset (proc.apvts, booth, index);
        shownPreset = -2;
        for (int slot = 0; slot < numBooth; ++slot) if (booth.isOn (booth.getOrder (slot))) { select (booth.getOrder (slot)); break; }
    }
    void step (int dir) { const int n = (int) boothPresets().size(); load (((juce::jmax (-1, currentPreset()) + dir) % n + n) % n); }
    void presetMenu()
    {
        juce::PopupMenu m;
        const auto& all = boothPresets();
        juce::StringArray cats;
        for (const auto& p : all) cats.addIfNotAlreadyThere (p.category);
        for (const auto& cat : cats)
        {
            juce::PopupMenu sub;
            for (int i = 0; i < (int) all.size(); ++i) if (cat == all[(size_t) i].category) sub.addItem (i + 1, all[(size_t) i].name, true, i == currentPreset());
            m.addSubMenu (cat, sub);
        }
        juce::Component::SafePointer<BoothPanel> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [safe] (int r) { if (safe != nullptr && r > 0) safe->load (r - 1); });
    }
    void timerCallback() override
    {
        if (! isShowing()) return;
        booth.refreshShownMic (proc.getSR());
        lvIn = juce::jmax (booth.inLevel.load(), lvIn * 0.85f);
        lvOut = juce::jmax (booth.outLevel.load(), lvOut * 0.85f);
        histDet[(size_t) histHead] = booth.tuneOn() ? booth.tuneDetected.load() : 0.f;
        histTgt[(size_t) histHead] = booth.tuneOn() ? booth.tuneTarget.load() : 0.f;
        histHead = (histHead + 1) % histN;
        const int cur = currentPreset();
        if (cur != shownPreset)
        {
            shownPreset = cur;
            const auto& all = boothPresets();
            presetBtn.setButtonText (cur >= 0 && cur < (int) all.size() ? juce::String (all[(size_t) cur].name) : juce::String ("Choose a vocal chain..."));
        }
        repaint();
    }

    BeatStationProcessor& proc;
    BoothChain& booth;
    BoothStrip strip;
    TrackList vocalTracks;
    juce::TextButton backBtn { "Back to pads" }, prevBtn { "<" }, nextBtn { ">" }, presetBtn { "Choose a vocal chain..." }, burnBtn { "BURN IN THE MIC" };
    LabeledKnob outKnob { "BOOTH OUT", " dB", Theme::green };
    std::vector<std::unique_ptr<LabeledKnob>> knobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> burnAtt;
    fxdsp::VocalEq shownEq;
    juce::Rectangle<int> micList, graph, meters, knobPanel, hintRect;
    int current = bComp, shownPreset = -2;
    float lvIn = 0.f, lvOut = 0.f, lastCentre = 57.f;
    static constexpr int histN = 180;
    std::array<float, histN> histDet {}, histTgt {};
    int histHead = 0;
};
