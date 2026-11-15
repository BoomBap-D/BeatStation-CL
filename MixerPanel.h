#pragma once
#include "PluginProcessor.h"
#include "Theme.h"

// THE MIXER: one channel for every sound source, and the master channel on the right.
//   16 pads | Keys | Deck | 8 audio tracks | Booth | MASTER
// Each channel: name, pan (drag sideways, double-click = centre), M (mute), S (solo), fader with level meter (double-click = 0 dB).
// The master channel has the output meters with peak hold, a pair of VU meters, the master fader, MONO and DIM.
class MixerPanel : public juce::Component, private juce::Timer
{
public:
    std::function<void()> onBack;
    std::function<void()> onMastering;

    explicit MixerPanel (BeatStationProcessor& p) : proc (p)
    {
        addAndMakeVisible (backBtn);
        backBtn.onClick = [this] { if (onBack) onBack(); };
        build();
        startTimerHz (30);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto head = r.removeFromTop (30);
        backBtn.setBounds (head.removeFromRight (120));
        headRect = head;
        r.removeFromTop (6);
        masterRect = r.removeFromRight (150);
        r.removeFromRight (10);
        area = r;
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::text);
        g.setFont (juce::Font (15.f, juce::Font::bold));
        auto head = headRect;
        g.drawText ("MIXER", head.removeFromLeft (70), juce::Justification::centredLeft);
        g.setColour (Theme::dim);
        g.setFont (11.f);
        g.drawFittedText (hover >= 0 ? strips[(size_t) hover].longName() + "    " + juce::String (strips[(size_t) hover].getDb(), 1) + " dB"
                                     : juce::String ("Drag a fader. Drag the small bar above it to pan. M mutes, S solos. Double-click a fader for 0 dB, or a pan bar for centre."),
                          head, juce::Justification::centredLeft, 1);

        g.setColour (Theme::panel);
        g.fillRoundedRectangle (area.toFloat(), 8.f);
        g.setColour (Theme::grid.brighter (0.1f));
        g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 8.f, 1.f);

        // section labels and dividers
        const char* sections[5] = { "PADS", "KEYS", "DECK", "AUDIO TRACKS", "BOOTH" };
        const int first[6] = { 0, 16, 17, 18, 26, 27 };
        for (int s = 0; s < 5; ++s)
        {
            const auto a = stripRect (first[s]), b = stripRect (first[s + 1] - 1);
            g.setColour (Theme::dim);
            g.setFont (juce::Font (10.f, juce::Font::bold));
            g.drawText (sections[s], a.getX(), area.getY() + 4, b.getRight() - a.getX(), 12, juce::Justification::centred);
            if (s > 0) { g.setColour (Theme::grid.brighter (0.2f)); g.drawVerticalLine (a.getX() - 3, (float) area.getY() + 6.f, (float) area.getBottom() - 6.f); }
        }
        for (int i = 0; i < (int) strips.size(); ++i) paintStrip (g, i);
        paintMaster (g);
    }

    void mouseMove (const juce::MouseEvent& e) override { const int h = stripAt (e.getPosition()); if (h != hover) { hover = h; repaint (0, 0, getWidth(), 30); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint (0, 0, getWidth(), 30); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        drag = none;
        const auto p = e.getPosition();
        if (masterRect.contains (p))
        {
            if (monoRect.contains (p)) proc.masterMono = ! proc.masterMono.load();
            else if (dimRect.contains (p)) proc.masterDim = ! proc.masterDim.load();
            else if (clipRect.contains (p)) proc.clipped[1] = false;
            else if (mastRect.contains (p)) { if (onMastering) onMastering(); }
            else if (masterFader.expanded (10, 6).contains (p)) { drag = masterFad; gesture (true); setMasterFromY (p.y); }
            repaint();
            return;
        }
        dragStrip = stripAt (p);
        if (dragStrip < 0) return;
        auto& s = strips[(size_t) dragStrip];
        const auto r = stripRect (dragStrip);
        if (panRect (r).contains (p)) { drag = pan; s.setPan (panFromX (r, p.x)); }
        else if (muteRect (r).contains (p)) s.setMute (! s.getMute());
        else if (soloRect (r).contains (p)) { if (s.setSolo) s.setSolo (! s.getSolo()); }
        else if (faderRect (r).expanded (4, 6).contains (p)) { drag = fader; if (s.gesture) s.gesture (true); s.setDb (dbFromY (faderRect (r), p.y)); }
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag == masterFad) setMasterFromY (e.y);
        else if (dragStrip >= 0 && drag == fader) strips[(size_t) dragStrip].setDb (dbFromY (faderRect (stripRect (dragStrip)), e.y));
        else if (dragStrip >= 0 && drag == pan) strips[(size_t) dragStrip].setPan (panFromX (stripRect (dragStrip), e.x));
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (drag == masterFad) gesture (false);
        if (drag == fader && dragStrip >= 0 && strips[(size_t) dragStrip].gesture) strips[(size_t) dragStrip].gesture (false);
        drag = none;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const auto p = e.getPosition();
        if (masterFader.expanded (10, 6).contains (p)) { gesture (true); setMaster (0.f); gesture (false); }
        const int i = stripAt (p);
        if (i >= 0)
        {
            const auto r = stripRect (i);
            if (panRect (r).contains (p)) strips[(size_t) i].setPan (0.f);
            else if (faderRect (r).expanded (4, 6).contains (p)) strips[(size_t) i].setDb (0.f);
        }
        repaint();
    }

private:
    struct Strip
    {
        juce::String shortName;
        juce::Colour colour;
        std::function<juce::String()> longName;
        std::function<float()> getDb, getPan, takePeak;
        std::function<void (float)> setDb, setPan;
        std::function<bool()> getMute, getSolo;
        std::function<void (bool)> setMute, setSolo;                     // setSolo empty = this channel has no solo
        std::function<void (bool)> gesture;                              // for parameters a DAW can automate
        float meter = 0.f, hold = 0.f;
        bool hasPan = true;
    };

    static constexpr float minDb = -60.f, maxDb = 6.f;

    void build()
    {
        for (int i = 0; i < numPads; ++i)                                // ---- the 16 pads
        {
            Strip s;
            s.shortName = juce::String (i + 1);
            s.colour = Theme::amber;
            s.longName = [this, i] { const auto info = proc.getPad (i); return "Pad " + juce::String (i + 1) + (info.sample != nullptr ? "  -  " + info.sample->name : juce::String ("  (empty)")); };
            auto* lvl = proc.apvts.getParameter (ParamID::level (i));
            auto* pn = proc.apvts.getParameter (ParamID::pan (i));
            s.getDb = [this, i] { return proc.apvts.getRawParameterValue (ParamID::level (i))->load(); };
            s.setDb = [lvl] (float db) { lvl->setValueNotifyingHost (lvl->convertTo0to1 (juce::jlimit (minDb, maxDb, db))); };
            s.getPan = [this, i] { return proc.apvts.getRawParameterValue (ParamID::pan (i))->load(); };
            s.setPan = [pn] (float v) { pn->beginChangeGesture(); pn->setValueNotifyingHost (pn->convertTo0to1 (v)); pn->endChangeGesture(); };
            s.gesture = [lvl] (bool begin) { if (begin) lvl->beginChangeGesture(); else lvl->endChangeGesture(); };
            s.getMute = [this, i] { return proc.padMute[(size_t) i].load(); };   s.setMute = [this, i] (bool v) { proc.padMute[(size_t) i] = v; };
            s.getSolo = [this, i] { return proc.padSolo[(size_t) i].load(); };   s.setSolo = [this, i] (bool v) { proc.padSolo[(size_t) i] = v; };
            s.takePeak = [this, i] { return proc.padPeak[(size_t) i].exchange (0.f); };
            strips.push_back (std::move (s));
        }
        {                                                                // ---- keys
            Strip s;
            s.shortName = "KEY";  s.colour = juce::Colour (0xff8f7cf0);
            s.longName = [] { return juce::String ("Keys instrument (also the key tracks of the sequencer)"); };
            s.getDb = [this] { return juce::Decibels::gainToDecibels (proc.keysVol.load(), minDb); };
            s.setDb = [this] (float db) { proc.keysVol = db <= minDb + 0.05f ? 0.f : juce::Decibels::decibelsToGain (juce::jlimit (minDb, maxDb, db)); };
            s.getPan = [this] { return proc.keysPan.load(); };   s.setPan = [this] (float v) { proc.keysPan = v; };
            s.getMute = [this] { return proc.keysMute.load(); }; s.setMute = [this] (bool v) { proc.keysMute = v; };
            s.getSolo = [this] { return proc.keysSolo.load(); }; s.setSolo = [this] (bool v) { proc.keysSolo = v; };
            s.takePeak = [this] { return proc.keysPeak.exchange (0.f); };
            strips.push_back (std::move (s));
        }
        {                                                                // ---- deck
            Strip s;
            s.shortName = "DECK";  s.colour = Theme::cream;  s.hasPan = false;
            s.longName = [] { return juce::String ("Turntable deck"); };
            s.getDb = [this] { return juce::Decibels::gainToDecibels (proc.deckGain.load(), minDb); };
            s.setDb = [this] (float db) { proc.deckGain = db <= minDb + 0.05f ? 0.f : juce::Decibels::decibelsToGain (juce::jlimit (minDb, maxDb, db)); };
            s.getPan = [] { return 0.f; };   s.setPan = [] (float) {};
            s.getMute = [this] { return proc.deckMute.load(); }; s.setMute = [this] (bool v) { proc.deckMute = v; };
            s.getSolo = [this] { return proc.deckSolo.load(); }; s.setSolo = [this] (bool v) { proc.deckSolo = v; };
            s.takePeak = [this] { return proc.deckPeak.exchange (0.f); };
            strips.push_back (std::move (s));
        }
        for (int i = 0; i < AudioTracks::num; ++i)                       // ---- the 8 audio tracks
        {
            Strip s;
            s.shortName = "T" + juce::String (i + 1);
            s.colour = juce::Colour (0xff5aa9ff);
            s.longName = [this, i] { auto& t = proc.tracks.tracks[(size_t) i]; return "Track " + juce::String (i + 1) + "  -  " + t.name + (t.booth.load() ? "  (through the Booth: pan has no effect)" : juce::String()); };
            s.getDb = [this, i] { return juce::Decibels::gainToDecibels (proc.tracks.tracks[(size_t) i].vol.load(), minDb); };
            s.setDb = [this, i] (float db) { proc.tracks.tracks[(size_t) i].vol = db <= minDb + 0.05f ? 0.f : juce::Decibels::decibelsToGain (juce::jlimit (minDb, maxDb, db)); };
            s.getPan = [this, i] { return proc.tracks.tracks[(size_t) i].pan.load(); };
            s.setPan = [this, i] (float v) { proc.tracks.tracks[(size_t) i].pan = v; };
            s.getMute = [this, i] { return proc.tracks.tracks[(size_t) i].mute.load(); };  s.setMute = [this, i] (bool v) { proc.tracks.tracks[(size_t) i].mute = v; };
            s.getSolo = [this, i] { return proc.tracks.tracks[(size_t) i].solo.load(); };  s.setSolo = [this, i] (bool v) { proc.tracks.tracks[(size_t) i].solo = v; };
            s.takePeak = [this, i] { return proc.tracks.tracks[(size_t) i].peak.load(); };
            strips.push_back (std::move (s));
        }
        {                                                                // ---- the booth return
            Strip s;
            s.shortName = "BTH";  s.colour = Theme::green;  s.hasPan = false;
            s.longName = [] { return juce::String ("Booth output (all vocal tracks after the mic and vocal chain)"); };
            auto* out = proc.apvts.getParameter ("bth_out");
            s.getDb = [this] { return proc.apvts.getRawParameterValue ("bth_out")->load(); };
            s.setDb = [out] (float db) { out->setValueNotifyingHost (out->convertTo0to1 (juce::jlimit (-24.f, maxDb, db))); };
            s.gesture = [out] (bool begin) { if (begin) out->beginChangeGesture(); else out->endChangeGesture(); };
            s.getPan = [] { return 0.f; };   s.setPan = [] (float) {};
            s.getMute = [this] { return boothMuted; };
            s.setMute = [this, out] (bool v)                                 // mute = the booth output pulled all the way down, and put back after
            {
                if (v == boothMuted) return;
                boothMuted = v;
                if (v) boothWas = proc.apvts.getRawParameterValue ("bth_out")->load();
                out->beginChangeGesture(); out->setValueNotifyingHost (out->convertTo0to1 (v ? -24.f : boothWas)); out->endChangeGesture();
            };
            s.getSolo = [] { return false; };
            s.takePeak = [this] { return proc.booth.outLevel.load(); };
            strips.push_back (std::move (s));
        }
    }

    // ---- geometry
    juce::Rectangle<int> stripRect (int i) const
    {
        const int n = (int) strips.size();
        const float w = (float) (area.getWidth() - 12) / (float) n;
        return { area.getX() + 6 + (int) ((float) i * w), area.getY() + 18, (int) w - 1, area.getHeight() - 24 };
    }
    static juce::Rectangle<int> nameRect (juce::Rectangle<int> r)  { return r.removeFromTop (16); }
    static juce::Rectangle<int> panRect (juce::Rectangle<int> r)   { return r.withTrimmedTop (20).removeFromTop (14).reduced (1, 0); }
    static juce::Rectangle<int> muteRect (juce::Rectangle<int> r)  { return r.withTrimmedTop (38).removeFromTop (18).reduced (1, 0); }
    static juce::Rectangle<int> soloRect (juce::Rectangle<int> r)  { return r.withTrimmedTop (58).removeFromTop (18).reduced (1, 0); }
    static juce::Rectangle<int> faderRect (juce::Rectangle<int> r) { return r.withTrimmedTop (84).withTrimmedBottom (16); }
    static float dbFromY (juce::Rectangle<int> f, int y) { return juce::jmap (juce::jlimit (0.f, 1.f, (float) (f.getBottom() - y) / (float) f.getHeight()), minDb, maxDb); }
    static float yFromDb (juce::Rectangle<int> f, float db) { return (float) f.getBottom() - juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, 0.f, 1.f) * (float) f.getHeight(); }
    static float panFromX (juce::Rectangle<int> r, int x) { const auto p = panRect (r); const float v = juce::jlimit (-1.f, 1.f, 2.f * (float) (x - p.getX()) / (float) p.getWidth() - 1.f); return std::abs (v) < 0.08f ? 0.f : v; }
    int stripAt (juce::Point<int> p) const { for (int i = 0; i < (int) strips.size(); ++i) if (stripRect (i).contains (p)) return i; return -1; }

    static void smallButton (juce::Graphics& g, juce::Rectangle<int> r, const char* text, bool on, juce::Colour col, bool enabled = true)
    {
        g.setColour (on ? col : Theme::graphBg);
        g.fillRoundedRectangle (r.toFloat(), 3.f);
        g.setColour (on ? col : Theme::grid.brighter (0.2f));
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 3.f, 1.f);
        g.setColour (on ? juce::Colour (0xff111316) : (enabled ? Theme::dim : Theme::tickOff));
        g.setFont (juce::Font (10.f, juce::Font::bold));
        g.drawText (text, r, juce::Justification::centred);
    }

    static void drawFader (juce::Graphics& g, juce::Rectangle<int> f, float db, float meterDb, float holdDb, juce::Colour col, bool wide)
    {
        const int mw = wide ? 10 : 6;
        const auto slot = juce::Rectangle<float> ((float) f.getX() + 3.f, (float) f.getY(), 4.f, (float) f.getHeight());
        const auto meter = juce::Rectangle<float> ((float) f.getRight() - (float) mw - 1.f, (float) f.getY(), (float) mw, (float) f.getHeight());
        g.setColour (Theme::graphBg);
        g.fillRoundedRectangle (slot, 2.f);
        g.fillRect (meter);
        const float zero = yFromDb (f, 0.f);
        g.setColour (Theme::dim.withAlpha (0.6f));
        g.drawHorizontalLine ((int) zero, (float) f.getX(), (float) f.getRight());         // 0 dB
        // meter: green, amber over -12, red over -3
        const float top = yFromDb (f, meterDb);
        auto seg = [&] (float fromDb, float toDb, juce::Colour c)
        {
            const float y0 = juce::jmax (top, yFromDb (f, toDb)), y1 = yFromDb (f, fromDb);
            if (y1 > y0) { g.setColour (c); g.fillRect (meter.getX(), y0, meter.getWidth(), y1 - y0); }
        };
        seg (minDb, -12.f, Theme::green);  seg (-12.f, -3.f, Theme::amber);  seg (-3.f, maxDb, Theme::red);
        if (holdDb > minDb + 1.f) { g.setColour (Theme::text); g.fillRect (meter.getX(), yFromDb (f, holdDb), meter.getWidth(), 1.5f); }
        // cap
        const float y = yFromDb (f, db);
        const auto cap = juce::Rectangle<float> ((float) f.getX() - 1.f, y - 8.f, (float) (f.getWidth() - mw - 3), 16.f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5b616d), 0.f, cap.getY(), juce::Colour (0xff23262c), 0.f, cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 3.f);
        g.setColour (juce::Colour (0xff0f1114));
        g.drawRoundedRectangle (cap, 3.f, 1.f);
        g.setColour (col);
        g.fillRect (cap.getX() + 2.f, y - 1.f, cap.getWidth() - 4.f, 2.f);
    }

    void paintStrip (juce::Graphics& g, int i)
    {
        auto& s = strips[(size_t) i];
        const auto r = stripRect (i);
        if (i == hover) { g.setColour (Theme::text.withAlpha (0.04f)); g.fillRoundedRectangle (r.toFloat().expanded (1.f, 0.f), 4.f); }
        g.setColour (s.colour);
        g.setFont (juce::Font (10.f, juce::Font::bold));
        g.drawText (s.shortName, nameRect (r), juce::Justification::centred);

        const auto pr = panRect (r).toFloat();                           // pan: a bar that fills from the centre towards the side
        g.setColour (Theme::graphBg);
        g.fillRoundedRectangle (pr, 3.f);
        if (s.hasPan)
        {
            const float v = s.getPan(), cx = pr.getCentreX(), px = cx + v * pr.getWidth() * 0.5f;
            g.setColour (s.colour.withAlpha (0.8f));
            g.fillRect (juce::jmin (cx, px), pr.getY() + 2.f, juce::jmax (1.5f, std::abs (px - cx)), pr.getHeight() - 4.f);
            g.setColour (Theme::cream);
            g.drawVerticalLine ((int) cx, pr.getY(), pr.getBottom());
        }
        smallButton (g, muteRect (r), "M", s.getMute(), Theme::amber);
        smallButton (g, soloRect (r), "S", s.getSolo(), Theme::green, (bool) s.setSolo);
        drawFader (g, faderRect (r), s.getDb(), juce::Decibels::gainToDecibels (s.meter, minDb), juce::Decibels::gainToDecibels (s.hold, minDb), s.colour, false);
        g.setColour (Theme::dim);
        g.setFont (9.f);
        const float db = s.getDb();
        g.drawText (db <= minDb + 0.05f ? juce::String ("off") : juce::String (db, db <= -10.f ? 0 : 1), juce::Rectangle<int> (r).removeFromBottom (12), juce::Justification::centred);
    }

    // ---- master
    void setMaster (float db) { if (auto* m = proc.apvts.getParameter ("master_vol")) m->setValueNotifyingHost (m->convertTo0to1 (juce::jlimit (minDb, maxDb, db))); }
    void setMasterFromY (int y) { setMaster (dbFromY (masterFader, y)); }
    void gesture (bool begin) { if (auto* m = proc.apvts.getParameter ("master_vol")) { if (begin) m->beginChangeGesture(); else m->endChangeGesture(); } }

    static void drawVu (juce::Graphics& g, juce::Rectangle<float> r, float level, const char* label)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff3e6c4), r.getX(), r.getY(), juce::Colour (0xffd9c79a), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 5.f);
        g.setColour (juce::Colour (0xff111316));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.f, 1.5f);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (r.reduced (1.5f).toNearestInt());                 // nothing is drawn outside the dial
        const juce::Point<float> pivot (r.getCentreX(), r.getBottom() - 4.f);
        const float radius = juce::jmin (r.getHeight() - 16.f, r.getWidth() * 0.72f), a0 = -0.7f, a1 = 0.7f;   // a wide, shallow arc, like a real VU
        auto angleFor = [&] (float vuDb) { return juce::jmap (juce::jlimit (-20.f, 3.f, vuDb), -20.f, 3.f, a0, a1); };
        auto at = [&] (float a, float rad) { return juce::Point<float> (pivot.x + std::sin (a) * rad, pivot.y - std::cos (a) * rad); };
        juce::Path arc;                                                       // the scale line: black, red from 0 VU up
        arc.addCentredArc (pivot.x, pivot.y, radius - 4.f, radius - 4.f, 0.f, a0, angleFor (0.f), true);
        g.setColour (juce::Colour (0xff111316));
        g.strokePath (arc, juce::PathStrokeType (1.f));
        juce::Path red;
        red.addCentredArc (pivot.x, pivot.y, radius - 4.f, radius - 4.f, 0.f, angleFor (0.f), a1, true);
        g.setColour (juce::Colour (0xffc0241c));
        g.strokePath (red, juce::PathStrokeType (2.f));
        g.setFont (juce::Font (7.5f, juce::Font::bold));
        for (int db : { -20, -10, -5, 0, 3 })
        {
            const float a = angleFor ((float) db);
            g.setColour (db >= 0 ? juce::Colour (0xffc0241c) : juce::Colour (0xff111316));
            g.drawLine ({ at (a, radius - 7.f), at (a, radius - 1.f) }, 1.f);
            const auto t = at (a, radius - 13.f);
            g.drawText (juce::String (std::abs (db)), (int) t.x - 8, (int) t.y - 4, 16, 8, juce::Justification::centred);
        }
        // 0 VU sits at -18 dBFS average level, the usual studio line-up
        const float a = angleFor (juce::Decibels::gainToDecibels (level, -60.f) + 18.f);
        g.setColour (juce::Colour (0xff111316));
        g.drawLine ({ pivot, at (a, radius) }, 1.4f);
        g.fillEllipse (pivot.x - 3.f, pivot.y - 3.f, 6.f, 6.f);
        g.setFont (juce::Font (8.f, juce::Font::bold));
        g.drawText (juce::String ("VU ") + label, (int) r.getX() + 4, (int) r.getY() + 2, 30, 10, juce::Justification::left);
    }

    void paintMaster (juce::Graphics& g)
    {
        auto r = masterRect;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2f36), 0.f, (float) r.getY(), juce::Colour (0xff1b1d21), 0.f, (float) r.getBottom(), false));
        g.fillRoundedRectangle (r.toFloat(), 8.f);
        g.setColour (Theme::amber);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.f, 1.4f);
        r = r.reduced (8, 6);
        g.setFont (juce::Font (13.f, juce::Font::bold));
        g.drawText ("MASTER", r.removeFromTop (18), juce::Justification::centred);

        auto vuRow = r.removeFromTop (78);                               // the VU pair
        drawVu (g, vuRow.removeFromLeft (vuRow.getWidth() / 2).reduced (2, 2).toFloat(), vuShown[0], "L");
        drawVu (g, vuRow.reduced (2, 2).toFloat(), vuShown[1], "R");
        r.removeFromTop (4);

        clipRect = r.removeFromTop (18).reduced (20, 0);                 // CLIP: stays lit until clicked
        const bool clip = proc.clipped[1].load();
        if (clip)
        {
            const auto cr = clipRect.toFloat();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff5a4d), cr.getCentreX(), cr.getY(), juce::Colour (0xffa30f14), cr.getCentreX(), cr.getBottom(), false));
            g.fillRoundedRectangle (cr, 4.f);
            g.setColour (juce::Colours::white.withAlpha (0.38f));
            g.fillRoundedRectangle (cr.reduced (2.f, 1.5f).withHeight (cr.getHeight() * 0.4f), 3.f);
        }
        else { g.setColour (juce::Colour (0xff3a2430)); g.fillRoundedRectangle (clipRect.toFloat(), 4.f); }
        g.setColour (clip ? juce::Colours::white : Theme::dim);
        g.setFont (juce::Font (10.f, juce::Font::bold));
        g.drawText ("CLIP", clipRect, juce::Justification::centred);
        r.removeFromTop (4);

        auto buttons = r.removeFromBottom (74);
        mastRect = buttons.removeFromBottom (26);
        buttons.removeFromBottom (4);
        auto row = buttons.removeFromBottom (22);
        monoRect = row.removeFromLeft (row.getWidth() / 2).reduced (2, 0);
        dimRect = row.reduced (2, 0);
        smallButton (g, monoRect, "MONO", proc.masterMono.load(), Theme::amber);
        smallButton (g, dimRect, "DIM", proc.masterDim.load(), Theme::amber);
        const bool mOn = proc.mastering.isOn();                          // the switch into the mastering station; lit while the station is on
        g.setColour (mOn ? Theme::amber : Theme::graphBg);
        g.fillRoundedRectangle (mastRect.toFloat(), 4.f);
        g.setColour (Theme::amber);
        g.drawRoundedRectangle (mastRect.toFloat().reduced (0.5f), 4.f, 1.f);
        g.setColour (mOn ? juce::Colour (0xff111316) : Theme::amber);
        g.setFont (juce::Font (10.f, juce::Font::bold));
        g.drawFittedText ("MASTERING STATION", mastRect, juce::Justification::centred, 1);
        g.setColour (Theme::dim);
        g.setFont (9.f);
        const float lufs = proc.mastering.lufsS.load();
        g.drawText (lufs > -70.f ? juce::String (lufs, 1) + " LUFS" : juce::String ("-- LUFS"), mastRect.getX(), mastRect.getY() - 40, mastRect.getWidth(), 10, juce::Justification::centred);

        const float db = proc.apvts.getRawParameterValue ("master_vol")->load();
        g.setColour (Theme::text);
        g.setFont (juce::Font (12.f, juce::Font::bold));
        g.drawText (db <= minDb + 0.05f ? juce::String ("off") : juce::String (db, 1) + " dB", r.removeFromBottom (16), juce::Justification::centred);
        g.setColour (Theme::dim);
        g.setFont (9.f);
        g.drawText ("peak " + juce::String (juce::Decibels::gainToDecibels (juce::jmax (mHold[0], mHold[1]), minDb), 1) + " dB", r.removeFromBottom (12), juce::Justification::centred);

        // fader in the middle with a wide meter either side
        r.removeFromTop (4);
        const int mw = 14;
        auto left = r.removeFromLeft (mw + 16), right = r.removeFromRight (mw + 16);
        masterFader = r.reduced (10, 8);
        auto bigMeter = [&] (juce::Rectangle<int> a, int ch, const char* label)
        {
            g.setColour (Theme::dim);
            g.setFont (9.f);
            g.drawText (label, a.removeFromBottom (10), juce::Justification::centred);
            const auto f = a.withSizeKeepingCentre (mw, a.getHeight() - 16);
            g.setColour (Theme::graphBg);
            g.fillRect (f);
            const int segs = 36;
            const float lvl = juce::Decibels::gainToDecibels (mMeter[ch], minDb), hold = juce::Decibels::gainToDecibels (mHold[ch], minDb);
            for (int k = 0; k < segs; ++k)
            {
                const float segDb = juce::jmap ((float) k / (float) (segs - 1), minDb, maxDb);
                const auto sr = juce::Rectangle<float> ((float) f.getX(), (float) f.getBottom() - (float) (k + 1) * (float) f.getHeight() / (float) segs + 1.f, (float) mw, (float) f.getHeight() / (float) segs - 1.5f);
                const auto c = segDb > -3.f ? Theme::red : segDb > -12.f ? Theme::amber : Theme::green;
                g.setColour (lvl >= segDb ? c : c.withAlpha (0.12f));
                g.fillRect (sr);
            }
            if (hold > minDb + 1.f) { g.setColour (Theme::text); g.fillRect ((float) f.getX(), yFromDb (f, hold), (float) mw, 2.f); }
        };
        bigMeter (left, 0, "L");
        bigMeter (right, 1, "R");
        drawFader (g, masterFader.withTrimmedRight (-12), db, minDb, minDb, Theme::amber, true);
    }

    void timerCallback() override
    {
        if (! isShowing()) return;
        for (auto& s : strips)
        {
            const float p = s.takePeak();
            s.meter = juce::jmax (p, s.meter * 0.86f);
            s.hold = p >= s.hold ? p : s.hold * 0.985f;
        }
        // The OUT meter beside the window already takes the output peak, so the master reads it from the slow VU plus its own hold.
        for (int ch = 0; ch < 2; ++ch)
        {
            vuShown[ch] += 0.35f * (proc.vu[ch].load() - vuShown[ch]);
            const float p = proc.masterPeak[ch].exchange (0.f);
            mMeter[ch] = juce::jmax (p, mMeter[ch] * 0.86f);
            mHold[ch] = p >= mHold[ch] ? p : mHold[ch] * 0.992f;
        }
        repaint();
    }

    enum Drag { none, fader, pan, masterFad };
    BeatStationProcessor& proc;
    std::vector<Strip> strips;
    juce::TextButton backBtn { "Back to pads" };
    juce::Rectangle<int> headRect, area, masterRect, masterFader, monoRect, dimRect, clipRect, mastRect;
    int hover = -1, dragStrip = -1;
    Drag drag = none;
    float vuShown[2] {}, mMeter[2] {}, mHold[2] {}, boothWas = 0.f;
    bool boothMuted = false;
};
