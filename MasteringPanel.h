#pragma once
#include "PluginProcessor.h"
#include "Theme.h"
#include "Widgets.h"

// THE MASTERING STATION screen. Left: the meter bridge (loudness against your target, true peak, and how hard each stage is working).
// Top: the seven stages in the order the mix goes through them. Click one for its picture and knobs; click its light to switch it on or off.
class MasteringPanel : public juce::Component, private juce::Timer
{
public:
    std::function<void()> onBack;
    std::function<void (const juce::String& target)> learn;

    explicit MasteringPanel (BeatStationProcessor& p) : proc (p), st (p.mastering)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &backBtn, &onBtn, &prevBtn, &nextBtn, &presetBtn, &targetBox, &matchBtn, &resetBtn, &bounceBtn }) addAndMakeVisible (*c);
        backBtn.onClick = [this] { if (onBack) onBack(); };
        onBtn.setClickingTogglesState (true);
        onAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "mst_on", onBtn);
        targetBox.addItemList (MasterInfo::targetNames, 1);
        targetAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "mst_target", targetBox);
        targetBox.setColour (juce::ComboBox::backgroundColourId, Theme::panel);
        targetBox.setColour (juce::ComboBox::textColourId, Theme::text);
        targetBox.setColour (juce::ComboBox::outlineColourId, Theme::grid);
        targetBox.setColour (juce::ComboBox::arrowColourId, Theme::amber);
        presetBtn.setColour (juce::TextButton::textColourOffId, Theme::amber);
        presetBtn.onClick = [this] { presetMenu(); };
        prevBtn.onClick = [this] { step (-1); };
        nextBtn.onClick = [this] { step (1); };
        resetBtn.onClick = [this] { st.resetMeters = true; };
        matchBtn.onClick = [this]                                         // sets the limiter gain so the song's loudness lands on the target
        {
            const float now = st.lufsI.load();
            if (now < -60.f) { note = "Play the song first so the loudness can be measured, then press MATCH TARGET."; return; }
            setParam ("mst_lim_gain", juce::jlimit (0.f, 18.f, st.v (35) + st.targetLufs() - now));
            setParam ("mst_lim_on", 1.f);
            setParam ("mst_on", 1.f);
            st.resetMeters = true;
            note = "Limiter gain set. Play the song again to check the new reading.";
        };
        bounceBtn.onClick = [this] { bounceMenu(); };
        bounceBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd0221f));
        select (mLimit);
        startTimerHz (24);
    }

    void select (int m)
    {
        current = m;
        knobs.clear();
        struct K { const char* id; const char* title; const char* suffix; };
        std::vector<K> list;
        switch (m)
        {
            case mEq:     list = { { "mst_eq_low", "LOW", " dB" }, { "mst_eq_lowf", "LOW FREQ", " Hz" }, { "mst_eq_mud", "LOW-MID", " dB" }, { "mst_eq_mudf", "L-M FREQ", " Hz" }, { "mst_eq_pres", "PRESENCE", " dB" },
                                   { "mst_eq_presf", "PRES FREQ", " Hz" }, { "mst_eq_air", "AIR", " dB" }, { "mst_eq_airf", "AIR FREQ", " Hz" }, { "mst_eq_sidecut", "SIDE LOW CUT", " Hz" }, { "mst_eq_sideair", "SIDE AIR", " dB" } }; break;
            case mGlue:   list = { { "mst_glue_thresh", "THRESHOLD", " dB" }, { "mst_glue_ratio", "RATIO", ":1" }, { "mst_glue_attack", "ATTACK", " ms" }, { "mst_glue_release", "RELEASE", " ms" },
                                   { "mst_glue_makeup", "MAKEUP", " dB" }, { "mst_glue_schp", "S-C LOW CUT", " Hz" }, { "mst_glue_mix", "MIX", "" } }; break;
            case mMulti:  list = { { "mst_mb_t1", "LOW THRESH", " dB" }, { "mst_mb_r1", "LOW RATIO", ":1" }, { "mst_mb_g1", "LOW GAIN", " dB" }, { "mst_mb_xlow", "SPLIT 1", " Hz" }, { "mst_mb_t2", "MID THRESH", " dB" },
                                   { "mst_mb_r2", "MID RATIO", ":1" }, { "mst_mb_g2", "MID GAIN", " dB" }, { "mst_mb_xhigh", "SPLIT 2", " Hz" }, { "mst_mb_t3", "HIGH THRESH", " dB" }, { "mst_mb_r3", "HIGH RATIO", ":1" }, { "mst_mb_g3", "HIGH GAIN", " dB" } }; break;
            case mColor:  list = { { "mst_col_warmth", "WARMTH", "" }, { "mst_col_air", "AIR", "" } }; break;
            case mImager: list = { { "mst_img_low", "LOW WIDTH", "" }, { "mst_img_xlow", "SPLIT 1", " Hz" }, { "mst_img_mid", "MID WIDTH", "" }, { "mst_img_xhigh", "SPLIT 2", " Hz" }, { "mst_img_high", "HIGH WIDTH", "" } }; break;
            case mClip:   list = { { "mst_clip_ceil", "CEILING", " dB" }, { "mst_clip_soft", "SOFTNESS", "" } }; break;
            case mLimit:  list = { { "mst_lim_gain", "GAIN", " dB" }, { "mst_lim_ceil", "CEILING", " dBTP" }, { "mst_lim_rel", "RELEASE", " ms" }, { "mst_out", "INPUT TRIM", " dB" } }; break;
            default: break;
        }
        for (const auto& k : list)
        {
            auto knob = std::make_unique<LabeledKnob> (k.title, k.suffix, MasterInfo::colours[m]);
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
        backBtn.setBounds (head.removeFromRight (120).reduced (0, 2));  head.removeFromRight (8);
        onBtn.setBounds (head.removeFromRight (130).reduced (0, 2));    head.removeFromRight (10);
        stripRect = head;
        r.removeFromTop (6);
        auto bar = r.removeFromTop (28);
        bar.removeFromLeft (62);
        prevBtn.setBounds (bar.removeFromLeft (28));     bar.removeFromLeft (4);
        presetBtn.setBounds (bar.removeFromLeft (290));  bar.removeFromLeft (4);
        nextBtn.setBounds (bar.removeFromLeft (28));     bar.removeFromLeft (14);
        targetBox.setBounds (bar.removeFromLeft (186));  bar.removeFromLeft (4);
        matchBtn.setBounds (bar.removeFromLeft (124));   bar.removeFromLeft (14);
        resetBtn.setBounds (bar.removeFromLeft (110));   bar.removeFromLeft (4);
        bounceBtn.setBounds (bar.removeFromLeft (110));
        r.removeFromTop (8);
        noteRect = r.removeFromBottom (18);
        bridge = r.removeFromLeft (250);
        r.removeFromLeft (10);
        knobPanel = r.removeFromBottom (118);
        r.removeFromBottom (8);
        vis = r;
        if (! knobs.empty())
        {
            auto k = knobPanel.reduced (4, 6);
            const int kw = juce::jmin (112, k.getWidth() / (int) knobs.size());
            k = k.withSizeKeepingCentre (kw * (int) knobs.size(), k.getHeight());
            for (auto& knob : knobs) knob->setBounds (k.removeFromLeft (kw));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int m = 0; m < numMaster; ++m)
        {
            const auto c = chip (m);
            if (! c.contains (e.position)) continue;
            if (e.position.x - c.getX() < 22.f) setParam (MasterInfo::onIds[m], st.moduleOn (m) ? 0.f : 1.f);
            select (m);
        }
    }

    void paint (juce::Graphics& g) override
    {
        // ---- the chain
        for (int m = 0; m < numMaster; ++m)
        {
            const auto r = chip (m);
            const auto col = MasterInfo::colours[m];
            const bool on = st.moduleOn (m) && st.isOn(), sel = m == current;
            g.setColour (sel ? col.withAlpha (0.22f) : Theme::panel);
            g.fillRoundedRectangle (r, 6.f);
            g.setColour (sel ? col : Theme::grid.brighter (0.15f));
            g.drawRoundedRectangle (r.reduced (0.5f), 6.f, sel ? 1.8f : 1.f);
            const auto led = juce::Rectangle<float> (10.f, 10.f).withCentre ({ r.getX() + 12.f, r.getCentreY() });
            if (on) { g.setColour (col.withAlpha (0.35f)); g.fillEllipse (led.expanded (3.f)); }
            g.setColour (st.moduleOn (m) ? (on ? col : col.withAlpha (0.4f)) : Theme::tickOff);
            g.fillEllipse (led);
            g.setColour (on ? Theme::text : Theme::dim);
            g.setFont (juce::Font (11.5f, juce::Font::bold));
            g.drawText (MasterInfo::names[m], r.withTrimmedLeft (20.f).toNearestInt(), juce::Justification::centred);
            if (m < numMaster - 1) { g.setColour (Theme::dim); g.drawText (">", (int) r.getRight(), (int) r.getY(), 12, (int) r.getHeight(), juce::Justification::centred); }
        }
        g.setColour (Theme::dim);
        g.setFont (juce::Font (11.f, juce::Font::bold));
        g.drawText ("PRESET", 0, presetBtn.getY(), 58, presetBtn.getHeight(), juce::Justification::centredLeft);
        g.setFont (11.f);
        g.setColour (st.isBouncing() ? juce::Colour (0xffff5a4d) : Theme::dim);
        g.drawFittedText (st.isBouncing() ? "BOUNCING to " + st.bounceFile.getFileName() + "   " + juce::String (st.bouncedSeconds(), 1) + " s   -   press Stop when the song has finished"
                                          : (note.isNotEmpty() ? note : juce::String ("The mix goes through the stages left to right. The limiter is always last. Meters work even when the station is off.")),
                          noteRect, juce::Justification::centredLeft, 1);

        for (auto rect : { bridge, vis, knobPanel })
        {
            g.setColour (rect == vis ? Theme::graphBg : Theme::panel);
            g.fillRoundedRectangle (rect.toFloat(), 8.f);
            g.setColour (Theme::grid.brighter (0.1f));
            g.drawRoundedRectangle (rect.toFloat().reduced (0.5f), 8.f, 1.f);
        }
        paintBridge (g);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (vis);
            paintVisual (g, vis.toFloat().reduced (14.f, 12.f).withTrimmedTop (22.f));
        }
        g.setColour (MasterInfo::colours[current]);
        g.setFont (juce::Font (14.f, juce::Font::bold));
        g.drawText (titleFor (current), vis.getX() + 14, vis.getY() + 8, vis.getWidth() - 28, 18, juce::Justification::left);
    }

private:
    static const char* titleFor (int m)
    {
        switch (m)
        {
            case mEq:     return "EQ  -  solid = the whole mix, thin = the sides only";
            case mGlue:   return "GLUE  -  how much the bus compressor is pulling the mix together";
            case mMulti:  return "MULTIBAND  -  low / mid / high, each with its own threshold (line) and squeeze (fill)";
            case mColor:  return "COLOR  -  valve warmth on the lows and mids, new air on top";
            case mImager: return "IMAGER  -  how wide each part of the mix is (narrow in the middle, wide at the edges)";
            case mClip:   return "CLIPPER  -  the tips of the loudest hits are shaved before the limiter";
            default:      return "LIMITER  -  gain held back to keep the true peak under the ceiling";
        }
    }
    juce::Rectangle<float> chip (int m) const
    {
        const float gap = 12.f, w = ((float) stripRect.getWidth() - gap * (float) (numMaster - 1)) / (float) numMaster;
        return { (float) stripRect.getX() + (float) m * (w + gap), (float) stripRect.getY() + 2.f, w, (float) stripRect.getHeight() - 4.f };
    }
    void setParam (const juce::String& id, float v)
    {
        if (auto* p = proc.apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); }
    }
    float pv (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    // ---- the meter bridge
    void paintBridge (juce::Graphics& g)
    {
        auto r = bridge.reduced (12, 10);
        const float I = st.lufsI.load(), S = st.lufsS.load(), M = st.lufsM.load(), target = st.targetLufs();
        g.setColour (Theme::dim);
        g.setFont (juce::Font (11.f, juce::Font::bold));
        g.drawText ("LOUDNESS OF THE WHOLE SONG", r.removeFromTop (14), juce::Justification::left);
        g.setColour (Theme::text);
        g.setFont (juce::Font (44.f, juce::Font::bold));
        g.drawText (I > -70.f ? juce::String (I, 1) : juce::String ("--.-"), r.removeFromTop (50), juce::Justification::centredLeft);
        g.setFont (12.f);
        g.setColour (Theme::dim);
        g.drawText ("LUFS integrated", r.getX() + 150, r.getY() - 26, 90, 14, juce::Justification::left);
        const float diff = I - target;
        g.setColour (I < -70.f ? Theme::dim : (std::abs (diff) <= 0.5f ? Theme::green : (diff > 0.f ? Theme::amber : juce::Colour (0xff5aa9ff))));
        g.setFont (juce::Font (13.f, juce::Font::bold));
        g.drawText (I < -70.f ? juce::String ("play the song to measure it")
                              : (std::abs (diff) <= 0.5f ? juce::String ("on target (") + juce::String (juce::roundToInt (target)) + ")"
                                                         : juce::String (std::abs (diff), 1) + (diff > 0.f ? " LU louder than the target" : " LU quieter than the target")),
                    r.removeFromTop (18), juce::Justification::left);
        r.removeFromTop (8);

        // three bars on one scale, with the target line across them
        auto bars = r.removeFromTop (150);
        auto Y = [&] (float lufs) { return (float) bars.getBottom() - 14.f - juce::jlimit (0.f, 1.f, (lufs + 36.f) / 36.f) * (float) (bars.getHeight() - 20); };
        const char* labels[3] = { "NOW", "3 SEC", "SONG" };
        const float vals[3] = { M, S, I };
        g.setFont (9.f);
        for (int db = -36; db <= 0; db += 6)
        {
            g.setColour (Theme::grid);
            g.drawHorizontalLine ((int) Y ((float) db), (float) bars.getX() + 22.f, (float) bars.getRight());
            g.setColour (Theme::dim);
            g.drawText (juce::String (db), bars.getX(), (int) Y ((float) db) - 5, 20, 10, juce::Justification::right);
        }
        for (int k = 0; k < 3; ++k)
        {
            const auto col = juce::Rectangle<float> ((float) bars.getX() + 34.f + (float) k * 64.f, (float) bars.getY(), 44.f, (float) bars.getHeight() - 14.f);
            const float y = Y (vals[k]);
            g.setColour (Theme::graphBg);
            g.fillRect (col.withTop (Y (0.f)));
            g.setColour (vals[k] > target + 0.5f ? Theme::amber : Theme::green);
            if (vals[k] > -70.f) g.fillRect (col.getX(), y, col.getWidth(), col.getBottom() - y);
            g.setColour (Theme::dim);
            g.drawText (labels[k], (int) col.getX() - 6, bars.getBottom() - 12, 56, 10, juce::Justification::centred);
        }
        g.setColour (Theme::text);
        g.drawHorizontalLine ((int) Y (target), (float) bars.getX() + 22.f, (float) bars.getRight());
        g.drawText ("target " + juce::String (juce::roundToInt (target)), bars.getRight() - 60, (int) Y (target) - 11, 60, 10, juce::Justification::right);
        r.removeFromTop (8);

        // true peak
        const float tp = juce::Decibels::gainToDecibels (st.tpMax.load(), -100.f), ceiling = pv ("mst_lim_ceil");
        g.setColour (Theme::dim);
        g.setFont (juce::Font (11.f, juce::Font::bold));
        g.drawText ("HIGHEST TRUE PEAK", r.removeFromTop (14), juce::Justification::left);
        g.setColour (tp > ceiling + 0.05f ? Theme::red : Theme::text);
        g.setFont (juce::Font (22.f, juce::Font::bold));
        g.drawText (tp > -90.f ? juce::String (tp, 2) + " dBTP" : juce::String ("--"), r.removeFromTop (26), juce::Justification::left);
        g.setFont (11.f);
        g.setColour (tp > ceiling + 0.05f ? Theme::red : Theme::dim);
        g.drawText (tp > ceiling + 0.05f ? "over the ceiling: switch the limiter on" : "streaming services ask for -1 dBTP or lower", r.removeFromTop (14), juce::Justification::left);
        r.removeFromTop (10);

        // how hard each stage is working
        g.setColour (Theme::dim);
        g.setFont (juce::Font (11.f, juce::Font::bold));
        g.drawText ("GAIN BEING HELD BACK", r.removeFromTop (14), juce::Justification::left);
        auto row = [&] (const char* label, float db, juce::Colour col)
        {
            auto line = r.removeFromTop (17);
            g.setColour (Theme::dim);
            g.setFont (10.f);
            g.drawText (label, line.removeFromLeft (52), juce::Justification::left);
            g.setColour (Theme::text);
            g.drawText (juce::String (-std::abs (db), 1), line.removeFromRight (34), juce::Justification::right);
            const auto track = line.reduced (0, 4).toFloat();
            g.setColour (Theme::graphBg);
            g.fillRoundedRectangle (track, 3.f);
            g.setColour (col);
            g.fillRoundedRectangle (track.withWidth (track.getWidth() * juce::jlimit (0.f, 1.f, std::abs (db) / 12.f)), 3.f);
        };
        row ("GLUE", shown[0], MasterInfo::colours[mGlue]);
        row ("MB LOW", shown[1], MasterInfo::colours[mMulti]);
        row ("MB MID", shown[2], MasterInfo::colours[mMulti]);
        row ("MB HIGH", shown[3], MasterInfo::colours[mMulti]);
        row ("CLIPPER", shown[4], MasterInfo::colours[mClip]);
        row ("LIMITER", shown[5], MasterInfo::colours[mLimit]);
    }

    // ---- the picture for the selected stage
    void paintVisual (juce::Graphics& g, juce::Rectangle<float> b)
    {
        const auto col = MasterInfo::colours[current];
        auto X = [&] (float hz) { return b.getX() + b.getWidth() * std::log10 (hz / 20.f) / 3.f; };
        auto freqGrid = [&]
        {
            g.setFont (10.f);
            for (float hz : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
            {
                g.setColour (Theme::grid);
                g.drawVerticalLine ((int) X (hz), b.getY(), b.getBottom());
                g.setColour (Theme::dim);
                g.drawText (hz >= 1000.f ? juce::String ((int) (hz / 1000.f)) + "k" : juce::String ((int) hz), (int) X (hz) + 2, (int) b.getBottom() - 12, 30, 12, juce::Justification::left);
            }
        };
        if (current == mEq)
        {
            freqGrid();
            auto Y = [&] (float db) { return b.getCentreY() - juce::jlimit (-9.f, 9.f, db) / 9.f * b.getHeight() * 0.5f; };
            for (int db : { -6, -3, 0, 3, 6 }) { g.setColour (db == 0 ? Theme::grid.brighter (0.4f) : Theme::grid); g.drawHorizontalLine ((int) Y ((float) db), b.getX(), b.getRight()); }
            auto& e = st.shownEq;
            e.lowF = pv ("mst_eq_lowf"); e.lowDb = pv ("mst_eq_low"); e.mudF = pv ("mst_eq_mudf"); e.mudDb = pv ("mst_eq_mud"); e.presF = pv ("mst_eq_presf"); e.presDb = pv ("mst_eq_pres");
            e.airF = pv ("mst_eq_airf"); e.airDb = pv ("mst_eq_air"); e.sideCut = pv ("mst_eq_sidecut"); e.sideAir = pv ("mst_eq_sideair");
            e.prepare (st.getSampleRate());
            juce::Path all, side;
            for (float x = b.getX(); x <= b.getRight(); x += 2.f)
            {
                const float hz = 20.f * std::pow (10.f, 3.f * (x - b.getX()) / b.getWidth());
                if (x == b.getX()) { all.startNewSubPath (x, Y (e.responseDb (hz))); side.startNewSubPath (x, Y (e.sideResponseDb (hz))); }
                else { all.lineTo (x, Y (e.responseDb (hz))); side.lineTo (x, Y (e.sideResponseDb (hz))); }
            }
            juce::Path fill (all);
            fill.lineTo (b.getRight(), Y (0.f));  fill.lineTo (b.getX(), Y (0.f));  fill.closeSubPath();
            g.setColour (col.withAlpha (0.22f));  g.fillPath (fill);
            g.setColour (juce::Colour (0xff3ad0e0));  g.strokePath (side, juce::PathStrokeType (1.4f));
            g.setColour (col);  g.strokePath (all, juce::PathStrokeType (2.6f));
        }
        else if (current == mMulti)
        {
            freqGrid();
            const float x1 = X (pv ("mst_mb_xlow")), x2 = X (pv ("mst_mb_xhigh"));
            const float edges[4] = { b.getX(), x1, x2, b.getRight() };
            const char* ids[3] = { "mst_mb_t1", "mst_mb_t2", "mst_mb_t3" };
            const char* names[3] = { "LOW", "MID", "HIGH" };
            auto Y = [&] (float db) { return b.getY() + 14.f + juce::jlimit (0.f, 1.f, -db / 50.f) * (b.getHeight() - 30.f); };
            for (int k = 0; k < 3; ++k)
            {
                const auto band = juce::Rectangle<float> (edges[k] + 2.f, b.getY() + 14.f, edges[k + 1] - edges[k] - 4.f, b.getHeight() - 30.f);
                g.setColour (col.withAlpha (0.07f + 0.05f * (float) k));
                g.fillRoundedRectangle (band, 5.f);
                const float gr = std::abs (shown[1 + k]);
                g.setColour (Theme::red.withAlpha (0.55f));
                g.fillRect (band.getX(), band.getY(), band.getWidth(), band.getHeight() * juce::jlimit (0.f, 1.f, gr / 18.f));   // squeeze hangs from the top
                g.setColour (Theme::text);
                g.drawHorizontalLine ((int) Y (pv (ids[k])), band.getX(), band.getRight());
                g.setFont (juce::Font (12.f, juce::Font::bold));
                g.drawText (names[k], band.toNearestInt().removeFromBottom (18), juce::Justification::centred);
                g.setFont (10.f);
                g.drawText ("threshold " + juce::String (juce::roundToInt (pv (ids[k]))) + " dB", (int) band.getX() + 4, (int) Y (pv (ids[k])) + 2, 120, 12, juce::Justification::left);
            }
        }
        else if (current == mGlue || current == mLimit)
        {
            const auto& h = current == mGlue ? histGlue : histLimit;
            for (int k = 1; k < 6; ++k) { g.setColour (Theme::grid); g.drawHorizontalLine ((int) (b.getY() + b.getHeight() * (float) k / 6.f), b.getX(), b.getRight()); }
            juce::Path p;
            p.startNewSubPath (b.getX(), b.getY());
            for (int i = 0; i < histN; ++i)
                p.lineTo (b.getX() + b.getWidth() * (float) i / (float) (histN - 1), b.getY() + juce::jlimit (0.f, 12.f, std::abs (h[(size_t) ((head + i) % histN)])) / 12.f * b.getHeight());
            p.lineTo (b.getRight(), b.getY());
            p.closeSubPath();
            g.setColour (col.withAlpha (0.65f));
            g.fillPath (p);
            g.setColour (Theme::dim);
            g.setFont (10.f);
            g.drawText ("0 dB", (int) b.getX() + 4, (int) b.getY() + 2, 40, 12, juce::Justification::left);
            g.drawText ("-12 dB", (int) b.getX() + 4, (int) b.getBottom() - 14, 50, 12, juce::Justification::left);
            g.setColour (col);
            g.setFont (juce::Font (24.f, juce::Font::bold));
            g.drawText (juce::String (-std::abs (shown[current == mGlue ? 0 : 5]), 1) + " dB", (int) b.getRight() - 170, (int) b.getBottom() - 34, 164, 28, juce::Justification::right);
        }
        else if (current == mClip)
        {
            const float side = juce::jmin (b.getWidth(), b.getHeight());
            const auto q = b.withSizeKeepingCentre (side * 1.5f, side);
            const float c = juce::Decibels::decibelsToGain (pv ("mst_clip_ceil")), knee = pv ("mst_clip_soft") * 0.5f, start = c * (1.f - knee);
            auto PX = [&] (float in) { return q.getX() + in / 1.2f * q.getWidth(); };
            auto PY = [&] (float out) { return q.getBottom() - out / 1.2f * q.getHeight(); };
            g.setColour (Theme::tickOff);
            g.drawLine (PX (0.f), PY (0.f), PX (1.2f), PY (1.2f), 1.f);
            juce::Path p;
            for (int i = 0; i <= 120; ++i)
            {
                const float in = 1.2f * (float) i / 120.f;
                const float out = in <= start ? in : (knee <= 0.f ? c : start + (c - start) * std::tanh ((in - start) / (c - start)));
                if (i == 0) p.startNewSubPath (PX (in), PY (out)); else p.lineTo (PX (in), PY (out));
            }
            g.setColour (col);
            g.strokePath (p, juce::PathStrokeType (2.8f));
            g.setColour (Theme::text.withAlpha (0.6f));
            g.drawHorizontalLine ((int) PY (c), q.getX(), q.getRight());
            g.setFont (11.f);
            g.drawText ("ceiling " + juce::String (pv ("mst_clip_ceil"), 1) + " dB     taking off " + juce::String (shown[4], 1) + " dB", (int) q.getX() + 6, (int) PY (c) - 16, 300, 14, juce::Justification::left);
        }
        else                                                              // COLOR and IMAGER: three bands drawn as bars
        {
            const bool img = current == mImager;
            const char* names[3] = { img ? "LOW" : "WARMTH (lows and mids)", img ? "MID" : "AIR (top)", "HIGH" };
            const float vals[3] = { img ? pv ("mst_img_low") / 2.f : pv ("mst_col_warmth"), img ? pv ("mst_img_mid") / 2.f : pv ("mst_col_air"), img ? pv ("mst_img_high") / 2.f : 0.f };
            const int n = img ? 3 : 2;
            for (int k = 0; k < n; ++k)
            {
                const auto row = juce::Rectangle<float> (b.getX(), b.getY() + 10.f + (float) k * (b.getHeight() - 20.f) / (float) n, b.getWidth(), (b.getHeight() - 20.f) / (float) n - 12.f);
                g.setColour (Theme::panel);
                g.fillRoundedRectangle (row, 6.f);
                const float w = row.getWidth() * juce::jlimit (0.02f, 1.f, vals[k]);
                g.setColour (col.withAlpha (0.75f));
                if (img) g.fillRoundedRectangle (row.withSizeKeepingCentre (w, row.getHeight() - 8.f), 6.f);          // wider bar = wider sound
                else     g.fillRoundedRectangle (row.reduced (0.f, 4.f).withWidth (w), 6.f);
                if (img) { g.setColour (Theme::cream); g.drawVerticalLine ((int) row.getCentreX(), row.getY(), row.getBottom()); }
                g.setColour (Theme::text);
                g.setFont (juce::Font (12.f, juce::Font::bold));
                g.drawText (names[k], row.toNearestInt().reduced (10, 0), juce::Justification::centredLeft);
                if (img) g.drawText (vals[k] < 0.02f ? juce::String ("mono") : juce::String ((int) (vals[k] * 200.f)) + "%", row.toNearestInt().reduced (10, 0), juce::Justification::centredRight);
            }
        }
        if (! st.isOn() || ! st.moduleOn (current))
        {
            g.setColour (Theme::dim);
            g.setFont (12.f);
            g.drawText (! st.isOn() ? "The station is OFF  -  press MASTERING ON" : "This stage is off  -  click its light above", vis.getRight() - 330, vis.getY() + 8, 316, 18, juce::Justification::right);
        }
    }

    int currentPreset() const { return (int) proc.apvts.state.getProperty ("masterPreset", -1); }
    void load (int index) { applyMasterPreset (proc.apvts, index); shownPreset = -2; st.resetMeters = true; note = "Preset loaded. Play the song, then press MATCH TARGET to set the loudness for this mix."; }
    void step (int dir) { const int n = (int) masterPresets().size(); load (((juce::jmax (-1, currentPreset()) + dir) % n + n) % n); }
    void presetMenu()
    {
        juce::PopupMenu m;
        const auto& all = masterPresets();
        juce::StringArray cats;
        for (const auto& p : all) cats.addIfNotAlreadyThere (p.category);
        for (const auto& cat : cats)
        {
            juce::PopupMenu sub;
            for (int i = 0; i < (int) all.size(); ++i) if (cat == all[(size_t) i].category) sub.addItem (i + 1, all[(size_t) i].name, true, i == currentPreset());
            m.addSubMenu (cat, sub);
        }
        juce::Component::SafePointer<MasteringPanel> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [safe] (int r) { if (safe != nullptr && r > 0) safe->load (r - 1); });
    }

    void bounceMenu()
    {
        if (st.isBouncing()) { st.stopBounce(); proc.stopPlaying(); note = "Bounce saved: " + st.bounceFile.getFullPathName(); return; }
        juce::PopupMenu m;
        m.addItem (1, "Bounce the song to a 24-bit WAV (best quality, for distributors)");
        m.addItem (2, "Bounce the song to a 16-bit WAV with dither (CD quality)");
        juce::Component::SafePointer<MasteringPanel> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&bounceBtn), [safe] (int r)
        {
            if (safe == nullptr || r == 0) return;
            const auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Beat Station").getChildFile ("Bounces");
            dir.createDirectory();
            safe->chooser = std::make_unique<juce::FileChooser> ("Save the master as", dir.getChildFile ("Master " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H.%M") + ".wav"), "*.wav");
            const bool sixteen = r == 2;
            safe->chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                                        [safe, sixteen] (const juce::FileChooser& fc)
            {
                if (safe == nullptr || fc.getResult() == juce::File()) return;
                if (safe->st.startBounce (fc.getResult().withFileExtension ("wav"), sixteen))
                {
                    safe->st.resetMeters = true;
                    safe->tail = -1;
                    safe->proc.startPlaying();                            // from the start of the song
                }
                else safe->note = "Could not create that file.";
            });
        });
    }

    void timerCallback() override
    {
        const float now[6] = { st.grGlue.load(), st.grMulti[0].load(), st.grMulti[1].load(), st.grMulti[2].load(), st.clipDb.load(), st.grLimit.load() };
        for (int k = 0; k < 6; ++k) shown[k] = std::abs (now[k]) > std::abs (shown[k]) ? now[k] : shown[k] * 0.85f;
        histGlue[(size_t) head] = now[0];
        histLimit[(size_t) head] = now[5];
        head = (head + 1) % histN;
        if (st.isBouncing() && ! proc.playing.load())                     // the song was stopped: let the reverb and echo tails ring out, then close the file
        {
            if (tail < 0) tail = 60;                                      // 2.5 seconds
            else if (--tail == 0) { st.stopBounce(); note = "Bounce saved: " + st.bounceFile.getFullPathName(); }
        }
        bounceBtn.setButtonText (st.isBouncing() ? "STOP BOUNCE" : "BOUNCE...");
        bounceBtn.setToggleState (st.isBouncing(), juce::dontSendNotification);
        const int cur = currentPreset();
        if (cur != shownPreset)
        {
            shownPreset = cur;
            const auto& all = masterPresets();
            presetBtn.setButtonText (cur >= 0 && cur < (int) all.size() ? juce::String (all[(size_t) cur].name) : juce::String ("Choose a master..."));
        }
        if (isShowing()) repaint();
    }

    static constexpr int histN = 200;
    BeatStationProcessor& proc;
    MasterStation& st;
    juce::TextButton backBtn { "Back" }, onBtn { "MASTERING ON" }, prevBtn { "<" }, nextBtn { ">" }, presetBtn { "Choose a master..." },
                     matchBtn { "MATCH TARGET" }, resetBtn { "RESET METERS" }, bounceBtn { "BOUNCE..." };
    juce::ComboBox targetBox;
    std::vector<std::unique_ptr<LabeledKnob>> knobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> targetAtt;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> stripRect, bridge, vis, knobPanel, noteRect;
    juce::String note;
    std::array<float, histN> histGlue {}, histLimit {};
    float shown[6] {};
    int current = mLimit, head = 0, shownPreset = -2, tail = -1;
};
