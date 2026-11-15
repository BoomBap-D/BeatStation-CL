#pragma once
#include "PluginProcessor.h"
#include "Theme.h"
#include "Widgets.h"

// The screen for one effect: a picture of what it is doing on top, its knobs underneath.
// Used for DRIVE, FILTER, CRUSH, PUNCH, CHORUS, DELAY, WIDTH and LIMITER (EQ, COMP and ROOM have their own screens).
class FxModuleView : public juce::Component, private juce::Timer
{
public:
    explicit FxModuleView (BeatStationProcessor& p) : proc (p), chain (p.fxChain)
    {
        histA.fill (0.f);
        histB.fill (-100.f);
        startTimerHz (30);
    }

    std::function<void (const juce::String& target)> learn;

    void setModule (int id)
    {
        fx = id;
        knobs.clear();
        struct K { const char* id; const char* title; const char* suffix; };
        std::vector<K> list;
        switch (fx)
        {
            case fxDrive:     list = { { "fx_drive_amt", "DRIVE", " dB" }, { "fx_drive_type", "TYPE", "" }, { "fx_drive_tone", "TONE", " Hz" }, { "fx_drive_mix", "MIX", "" }, { "fx_drive_out", "OUTPUT", " dB" } }; break;
            case fxFilter:    list = { { "fx_filt_type", "TYPE", "" }, { "fx_filt_cut", "CUTOFF", " Hz" }, { "fx_filt_res", "RESONANCE", "" }, { "fx_filt_slope", "SLOPE", "" }, { "fx_filt_rate", "LFO RATE", " Hz" }, { "fx_filt_depth", "LFO DEPTH", " oct" } }; break;
            case fxCrush:     list = { { "fx_crush_rate", "SAMPLE RATE", " Hz" }, { "fx_crush_bits", "BITS", "" }, { "fx_crush_cut", "FILTER", " Hz" }, { "fx_crush_mix", "MIX", "" } }; break;
            case fxTransient: list = { { "fx_trans_attack", "ATTACK", " %" }, { "fx_trans_sustain", "SUSTAIN", " %" }, { "fx_trans_out", "OUTPUT", " dB" } }; break;
            case fxChorus:    list = { { "fx_chor_rate", "RATE", " Hz" }, { "fx_chor_depth", "DEPTH", " ms" }, { "fx_chor_delay", "DELAY", " ms" }, { "fx_chor_fb", "FEEDBACK", "" }, { "fx_chor_mix", "MIX", "" } }; break;
            case fxDelay:     list = { { "fx_dly_sync", "SYNC", "" }, { "fx_dly_time", "TIME", " ms" }, { "fx_dly_fb", "FEEDBACK", "" }, { "fx_dly_ping", "PING-PONG", "" }, { "fx_dly_lowcut", "LOW CUT", " Hz" }, { "fx_dly_highcut", "HIGH CUT", " Hz" }, { "fx_dly_mix", "MIX", "" } }; break;
            case fxWidth:     list = { { "fx_wid_width", "WIDTH", "" }, { "fx_wid_mono", "MONO BELOW", " Hz" }, { "fx_wid_out", "OUTPUT", " dB" } }; break;
            case fxLimiter:   list = { { "fx_lim_gain", "GAIN", " dB" }, { "fx_lim_ceil", "CEILING", " dB" }, { "fx_lim_rel", "RELEASE", " ms" } }; break;
            default: break;
        }
        const auto col = fx >= 0 && fx < numFx ? FxInfo::colours[fx] : Theme::amber;
        for (const auto& k : list)
        {
            auto knob = std::make_unique<LabeledKnob> (k.title, k.suffix, col);
            knob->attach (proc.apvts, k.id);
            if (learn) { auto cb = learn; knob->onLearnMenu = [cb] (const juce::String& id) { cb ("param:" + id); }; }
            addAndMakeVisible (*knob);
            knobs.push_back (std::move (knob));
        }
        histA.fill (0.f);
        histB.fill (-100.f);
        resized();
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        knobPanel = r.removeFromBottom (118);
        r.removeFromBottom (8);
        vis = r;
        if (knobs.empty()) return;
        auto k = knobPanel.reduced (6, 6);
        const int kw = juce::jmin (120, k.getWidth() / (int) knobs.size());
        k = k.withSizeKeepingCentre (kw * (int) knobs.size(), k.getHeight());
        for (auto& knob : knobs) knob->setBounds (k.removeFromLeft (kw));
    }

    void paint (juce::Graphics& g) override
    {
        if (fx < 0) return;
        const auto col = FxInfo::colours[fx];
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (knobPanel.toFloat(), 8.f);
        g.setColour (Theme::grid.brighter (0.1f));
        g.drawRoundedRectangle (knobPanel.toFloat().reduced (0.5f), 8.f, 1.f);

        const auto full = vis.toFloat();
        g.setColour (Theme::graphBg);
        g.fillRoundedRectangle (full, 8.f);
        g.setColour (Theme::grid.brighter (0.1f));
        g.drawRoundedRectangle (full.reduced (0.5f), 8.f, 1.f);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (vis);
        const auto b = full.reduced (14.f, 12.f).withTrimmedTop (22.f);

        switch (fx)
        {
            case fxDrive:     drawDrive (g, b, col);     break;
            case fxFilter:    drawFilter (g, b, col);    break;
            case fxCrush:     drawCrush (g, b, col);     break;
            case fxTransient: drawTransient (g, b, col); break;
            case fxChorus:    drawChorus (g, b, col);    break;
            case fxDelay:     drawDelay (g, b, col);     break;
            case fxWidth:     drawWidth (g, b, col);     break;
            case fxLimiter:   drawLimiter (g, b, col);   break;
            default: break;
        }

        g.setColour (col);
        g.setFont (juce::Font (14.f, juce::Font::bold));
        g.drawText (titleFor (fx), vis.getX() + 14, vis.getY() + 8, 400, 18, juce::Justification::left);
        if (! chain.isOn (fx))
        {
            g.setColour (Theme::dim);
            g.setFont (12.f);
            g.drawText ("OFF  -  click the light on its tab above to switch it on", vis.getRight() - 420, vis.getY() + 8, 406, 18, juce::Justification::right);
        }
    }

private:
    static const char* titleFor (int fx)
    {
        switch (fx)
        {
            case fxDrive:     return "DRIVE  -  what goes in (across) against what comes out (up)";
            case fxFilter:    return "FILTER  -  which frequencies get through";
            case fxCrush:     return "CRUSH  -  a smooth wave after the old-sampler treatment";
            case fxTransient: return "PUNCH  -  level (bars) and what the effect adds or takes away (line)";
            case fxChorus:    return "CHORUS  -  how the two sides move against each other";
            case fxDelay:     return "DELAY  -  the repeats over time";
            case fxWidth:     return "WIDTH  -  stereo picture of the output";
            case fxLimiter:   return "LIMITER  -  level in, and how much is being held back";
            default:          return "";
        }
    }

    float pv (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    static void grid (juce::Graphics& g, juce::Rectangle<float> b, int nx, int ny)
    {
        g.setColour (Theme::grid);
        for (int i = 0; i <= nx; ++i) g.drawVerticalLine ((int) (b.getX() + b.getWidth() * (float) i / (float) nx), b.getY(), b.getBottom());
        for (int i = 0; i <= ny; ++i) g.drawHorizontalLine ((int) (b.getY() + b.getHeight() * (float) i / (float) ny), b.getX(), b.getRight());
    }

    // ---------------- DRIVE: the transfer curve, with a dot riding it at the level of the sound coming in
    void drawDrive (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour col)
    {
        const float side = juce::jmin (area.getWidth(), area.getHeight());
        const auto b = area.withSizeKeepingCentre (side * 1.6f, side);
        grid (g, b, 8, 4);
        const int type = (int) pv ("fx_drive_type");
        const float pre = juce::Decibels::decibelsToGain (pv ("fx_drive_amt")), comp = 1.f / std::sqrt (juce::jmax (1.f, pre));
        auto X = [&] (float in) { return b.getCentreX() + in * b.getWidth() * 0.5f; };
        auto Y = [&] (float out) { return b.getCentreY() - juce::jlimit (-1.f, 1.f, out) * b.getHeight() * 0.5f; };
        auto f = [&] (float in) { return fxdsp::Drive::shape (type, in * pre) * comp; };

        g.setColour (Theme::tickOff);
        g.drawLine (X (-1.f), Y (-1.f), X (1.f), Y (1.f), 1.f);          // straight line = no effect
        juce::Path curve, fill;
        for (int i = 0; i <= 200; ++i)
        {
            const float in = -1.f + 2.f * (float) i / 200.f;
            if (i == 0) curve.startNewSubPath (X (in), Y (f (in))); else curve.lineTo (X (in), Y (f (in)));
        }
        fill = curve;
        fill.lineTo (X (1.f), Y (0.f));
        fill.lineTo (X (-1.f), Y (0.f));
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.35f), 0.f, b.getY(), col.withAlpha (0.04f), 0.f, b.getCentreY(), false));
        g.fillPath (fill);
        g.setColour (col);
        g.strokePath (curve, juce::PathStrokeType (2.8f));

        const float in = chain.isOn (fx) ? juce::jlimit (0.f, 1.f, chain.driveIn.load()) : 0.f;
        level = juce::jmax (in, level * 0.88f);
        for (float sgn : { -1.f, 1.f })
        {
            g.setColour (Theme::text);
            g.fillEllipse (X (sgn * level) - 5.f, Y (f (sgn * level)) - 5.f, 10.f, 10.f);
            g.setColour (col);
            g.drawEllipse (X (sgn * level) - 5.f, Y (f (sgn * level)) - 5.f, 10.f, 10.f, 2.f);
        }
    }

    // ---------------- FILTER: the response curve; the cutoff line moves with the LFO
    void drawFilter (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour col)
    {
        auto X = [&] (float hz) { return b.getX() + b.getWidth() * std::log10 (hz / 20.f) / 3.f; };
        auto Y = [&] (float db) { return b.getY() + b.getHeight() * (18.f - juce::jlimit (-48.f, 18.f, db)) / 66.f; };
        g.setFont (11.f);
        for (float hz : { 20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f })
        {
            g.setColour (Theme::grid);
            g.drawVerticalLine ((int) X (hz), b.getY(), b.getBottom());
            g.setColour (Theme::dim);
            g.drawText (hz >= 1000.f ? juce::String ((int) (hz / 1000.f)) + "k" : juce::String ((int) hz), (int) X (hz) + 3, (int) b.getBottom() - 15, 40, 14, juce::Justification::left);
        }
        for (int db : { 12, 0, -12, -24, -36 })
        {
            g.setColour (db == 0 ? Theme::grid.brighter (0.3f) : Theme::grid);
            g.drawHorizontalLine ((int) Y ((float) db), b.getX(), b.getRight());
            g.setColour (Theme::dim);
            g.drawText (juce::String (db), (int) b.getX() + 4, (int) Y ((float) db) - 14, 30, 14, juce::Justification::left);
        }
        const int type = (int) pv ("fx_filt_type"), slope = (int) pv ("fx_filt_slope");
        const float q0 = pv ("fx_filt_res"), q = slope == 1 ? std::sqrt (juce::jmax (0.5f, q0)) * 0.84f : q0;
        const float fc = chain.isOn (fx) ? chain.filterCut.load() : pv ("fx_filt_cut");
        auto mag = [&] (float hz)
        {
            const float w = hz / fc, den = std::sqrt ((1.f - w * w) * (1.f - w * w) + (w / q) * (w / q)) + 1.0e-9f;
            const float m = type == 0 ? 1.f / den : type == 1 ? w * w / den : type == 2 ? (w / q) / den : std::abs (1.f - w * w) / den;
            return juce::Decibels::gainToDecibels (slope == 1 ? m * m : m, -100.f);
        };
        juce::Path curve;
        for (float x = b.getX(); x <= b.getRight(); x += 2.f)
        {
            const float hz = 20.f * std::pow (10.f, 3.f * (x - b.getX()) / b.getWidth());
            if (x == b.getX()) curve.startNewSubPath (x, Y (mag (hz))); else curve.lineTo (x, Y (mag (hz)));
        }
        juce::Path fill (curve);
        fill.lineTo (b.getRight(), b.getBottom());
        fill.lineTo (b.getX(), b.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.38f), 0.f, b.getY(), col.withAlpha (0.03f), 0.f, b.getBottom(), false));
        g.fillPath (fill);
        g.setColour (col);
        g.strokePath (curve, juce::PathStrokeType (2.8f));
        g.setColour (Theme::text.withAlpha (0.7f));
        g.drawVerticalLine ((int) X (juce::jlimit (20.f, 20000.f, fc)), b.getY(), b.getBottom());
        g.setFont (12.f);
        g.drawText (juce::String ((int) fc) + " Hz", (int) X (juce::jlimit (20.f, 20000.f, fc)) + 6, (int) b.getY() + 2, 90, 14, juce::Justification::left);
    }

    // ---------------- CRUSH: a clean wave (faint) and the stepped wave it becomes
    void drawCrush (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour col)
    {
        grid (g, b, 8, 4);
        const float rate = pv ("fx_crush_rate"), bits = pv ("fx_crush_bits");
        const float levels = std::pow (2.f, juce::jlimit (2.f, 16.f, bits) - 1.f);
        const float cycles = 2.f, toneHz = 500.f;                        // the picture shows two cycles of a 500 Hz wave
        const float stepsPerCycle = rate / toneHz;
        auto Y = [&] (float v) { return b.getCentreY() - v * b.getHeight() * 0.42f; };
        juce::Path clean, stepped;
        float held = 0.f;
        int lastStep = -1;
        for (float x = 0.f; x <= b.getWidth(); x += 1.f)
        {
            const float ph = cycles * x / b.getWidth();
            const float v = std::sin (juce::MathConstants<float>::twoPi * ph);
            const int step = (int) std::floor (ph * stepsPerCycle);
            if (step != lastStep) { lastStep = step; held = bits < 15.99f ? std::round (v * levels) / levels : v; }
            if (x == 0.f) { clean.startNewSubPath (b.getX(), Y (v)); stepped.startNewSubPath (b.getX(), Y (held)); }
            else { clean.lineTo (b.getX() + x, Y (v)); stepped.lineTo (b.getX() + x, Y (held)); }
        }
        g.setColour (Theme::dim.withAlpha (0.5f));
        g.strokePath (clean, juce::PathStrokeType (1.4f));
        juce::Path fill (stepped);
        fill.lineTo (b.getRight(), b.getCentreY());
        fill.lineTo (b.getX(), b.getCentreY());
        fill.closeSubPath();
        g.setColour (col.withAlpha (0.18f));
        g.fillPath (fill);
        g.setColour (col);
        g.strokePath (stepped, juce::PathStrokeType (2.4f));
        g.setColour (Theme::text);
        g.setFont (12.f);
        g.drawText (juce::String ((int) rate) + " Hz   " + juce::String (bits, 1) + " bit   (" + juce::String ((int) (2.f * levels)) + " levels)",
                    (int) b.getX() + 6, (int) b.getBottom() - 18, 320, 14, juce::Justification::left);
    }

    // ---------------- PUNCH: scrolling level, with the gain the effect applies drawn over it
    void drawTransient (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour col)
    {
        grid (g, b, 10, 4);
        const float mid = b.getCentreY();
        juce::Path gainLine;
        for (int i = 0; i < histN; ++i)
        {
            const int idx = (head + i) % histN;
            const float x = b.getX() + b.getWidth() * (float) i / (float) (histN - 1);
            const float lv = juce::jmap (juce::jlimit (-60.f, 0.f, histB[(size_t) idx]), -60.f, 0.f, 0.f, b.getHeight());
            g.setColour (Theme::dim.withAlpha (0.28f));
            g.fillRect (x, b.getBottom() - lv, b.getWidth() / (float) histN + 0.5f, lv);
            const float y = mid - juce::jlimit (-18.f, 18.f, histA[(size_t) idx]) / 18.f * b.getHeight() * 0.5f;
            if (i == 0) gainLine.startNewSubPath (x, y); else gainLine.lineTo (x, y);
        }
        g.setColour (Theme::grid.brighter (0.4f));
        g.drawHorizontalLine ((int) mid, b.getX(), b.getRight());
        juce::Path fill (gainLine);
        fill.lineTo (b.getRight(), mid);
        fill.lineTo (b.getX(), mid);
        fill.closeSubPath();
        g.setColour (col.withAlpha (0.22f));
        g.fillPath (fill);
        g.setColour (col);
        g.strokePath (gainLine, juce::PathStrokeType (2.2f));
        g.setColour (Theme::dim);
        g.setFont (11.f);
        g.drawText ("+18 dB", (int) b.getRight() - 60, (int) b.getY() + 2, 56, 12, juce::Justification::right);
        g.drawText ("-18 dB", (int) b.getRight() - 60, (int) b.getBottom() - 14, 56, 12, juce::Justification::right);
    }

    // ---------------- CHORUS: the moving delay of each side, with a dot showing where it is now
    void drawChorus (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour col)
    {
        grid (g, b, 8, 3);
        const float base = pv ("fx_chor_delay"), dep = juce::jmin (pv ("fx_chor_depth"), base - 0.5f);
        const float phase = chain.isOn (fx) ? chain.chorusPhase.load() : 0.f;
        auto Y = [&] (float ms) { return b.getBottom() - juce::jlimit (0.f, 40.f, ms) / 40.f * b.getHeight(); };
        const juce::Colour side[2] = { col, Theme::amber };
        for (int c = 0; c < 2; ++c)
        {
            juce::Path p;
            for (float x = 0.f; x <= b.getWidth(); x += 2.f)
            {
                const float ph = 2.f * x / b.getWidth() + 0.25f * (float) c;
                const float ms = base + dep * std::sin (juce::MathConstants<float>::twoPi * ph);
                if (x == 0.f) p.startNewSubPath (b.getX(), Y (ms)); else p.lineTo (b.getX() + x, Y (ms));
            }
            g.setColour (side[c]);
            g.strokePath (p, juce::PathStrokeType (2.4f));
            const float dotX = b.getX() + b.getWidth() * std::fmod (phase, 1.f) * 0.5f;
            const float ms = base + dep * std::sin (juce::MathConstants<float>::twoPi * (phase + 0.25f * (float) c));
            g.setColour (Theme::text);
            g.fillEllipse (dotX - 5.f, Y (ms) - 5.f, 10.f, 10.f);
            g.setColour (side[c]);
            g.drawEllipse (dotX - 5.f, Y (ms) - 5.f, 10.f, 10.f, 2.f);
            g.setFont (juce::Font (12.f, juce::Font::bold));
            g.drawText (c == 0 ? "LEFT" : "RIGHT", (int) b.getRight() - 60, (int) b.getY() + 4 + c * 16, 56, 14, juce::Justification::right);
        }
        g.setColour (Theme::dim);
        g.setFont (11.f);
        g.drawText ("40 ms", (int) b.getX() + 4, (int) b.getY() + 2, 50, 12, juce::Justification::left);
        g.drawText ("0 ms", (int) b.getX() + 4, (int) b.getBottom() - 14, 50, 12, juce::Justification::left);
    }

    // ---------------- DELAY: each repeat as a bar. Left side above the line, right side below.
    void drawDelay (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour col)
    {
        const float time = chain.delayMsNow(), fb = pv ("fx_dly_fb"), mix = pv ("fx_dly_mix");
        const bool ping = pv ("fx_dly_ping") > 0.5f;
        const float span = juce::jmax (1000.f, time * 8.5f);
        const float mid = b.getCentreY(), half = b.getHeight() * 0.44f;
        auto X = [&] (float ms) { return b.getX() + 20.f + (b.getWidth() - 40.f) * ms / span; };

        const float beat = 60000.f / juce::jmax (20.f, chain.bpm.load());
        g.setFont (11.f);
        for (int k = 0; (float) k * beat <= span; ++k)                   // one line per beat at the current tempo
        {
            g.setColour (Theme::grid);
            g.drawVerticalLine ((int) X ((float) k * beat), b.getY(), b.getBottom());
            g.setColour (Theme::dim);
            g.drawText ("beat " + juce::String (k + 1), (int) X ((float) k * beat) + 3, (int) b.getBottom() - 14, 60, 12, juce::Justification::left);
        }
        g.setColour (Theme::grid.brighter (0.4f));
        g.drawHorizontalLine ((int) mid, b.getX(), b.getRight());
        g.setColour (Theme::dim);
        g.drawText ("LEFT", (int) b.getX() + 2, (int) b.getY() + 2, 40, 12, juce::Justification::left);
        g.drawText ("RIGHT", (int) b.getX() + 2, (int) b.getBottom() - 30, 40, 12, juce::Justification::left);

        g.setColour (Theme::cream);                                      // the dry sound
        g.fillRoundedRectangle (X (0.f) - 4.f, mid - half, 8.f, half * 2.f, 3.f);
        float amp = juce::jmax (0.04f, mix);
        for (int k = 1; (float) k * time <= span && amp > 0.012f; ++k)
        {
            const float x = X ((float) k * time), h = half * juce::jmin (1.f, amp);
            const bool left = ! ping || (k % 2 == 1), right = ! ping || (k % 2 == 0);
            g.setColour (col.withAlpha (juce::jmax (0.25f, 1.f - 0.07f * (float) k)));
            if (left)  g.fillRoundedRectangle (x - 4.f, mid - h, 8.f, h, 3.f);
            if (right) g.fillRoundedRectangle (x - 4.f, mid, 8.f, h, 3.f);
            amp *= fb;
        }
        g.setColour (Theme::text);
        g.setFont (12.f);
        const int sync = (int) pv ("fx_dly_sync");
        g.drawText (juce::String (time, 0) + " ms" + (sync > 0 ? "   (" + FxInfo::syncNames[sync] + " at " + juce::String (chain.bpm.load(), 1) + " BPM)" : juce::String()),
                    (int) b.getRight() - 320, (int) b.getY() + 2, 316, 14, juce::Justification::right);
    }

    // ---------------- WIDTH: the stereo scope (straight up = mono, spread sideways = wide) and the mono-safety bar
    void drawWidth (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour col)
    {
        auto b = area;
        const auto bar = b.removeFromBottom (26.f);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.48f;
        const auto c = b.getCentre();
        g.setColour (Theme::grid);
        g.drawEllipse (c.x - r, c.y - r, 2.f * r, 2.f * r, 1.f);
        g.drawEllipse (c.x - r * 0.5f, c.y - r * 0.5f, r, r, 1.f);
        g.drawLine (c.x - r * 0.707f, c.y - r * 0.707f, c.x + r * 0.707f, c.y + r * 0.707f, 1.f);
        g.drawLine (c.x + r * 0.707f, c.y - r * 0.707f, c.x - r * 0.707f, c.y + r * 0.707f, 1.f);
        g.setColour (Theme::dim);
        g.setFont (juce::Font (11.f, juce::Font::bold));
        g.drawText ("L", (int) (c.x - r * 0.707f) - 18, (int) (c.y - r * 0.707f) - 14, 14, 12, juce::Justification::centred);
        g.drawText ("R", (int) (c.x + r * 0.707f) + 4, (int) (c.y - r * 0.707f) - 14, 14, 12, juce::Justification::centred);
        g.drawText ("MONO", (int) c.x - 20, (int) (c.y - r) - 2, 40, 12, juce::Justification::centred);
        g.setColour (col.withAlpha (0.75f));
        for (int i = 0; i < FxChain::scopeSize; ++i)
        {
            const float l = chain.scopeL[(size_t) i].load (std::memory_order_relaxed), rr = chain.scopeR[(size_t) i].load (std::memory_order_relaxed);
            const float x = juce::jlimit (-1.f, 1.f, (rr - l) * 0.707f), y = juce::jlimit (-1.f, 1.f, (l + rr) * 0.707f);
            g.fillEllipse (c.x + x * r - 1.5f, c.y - y * r - 1.5f, 3.f, 3.f);
        }
        // mono-safety: +1 is fine on one speaker, below 0 things start to disappear in mono
        const float corr = chain.isOn (fx) ? juce::jlimit (-1.f, 1.f, chain.widthCorr.load()) : 1.f;
        const auto track = bar.reduced (60.f, 8.f);
        g.setColour (Theme::tickOff);
        g.fillRoundedRectangle (track, 4.f);
        const float cx = track.getCentreX(), px = cx + corr * track.getWidth() * 0.5f;
        g.setColour (corr >= 0.f ? Theme::green : Theme::red);
        g.fillRoundedRectangle (juce::jmin (cx, px), track.getY(), std::abs (px - cx), track.getHeight(), 4.f);
        g.setColour (Theme::dim);
        g.drawText ("-1", (int) track.getX() - 26, (int) track.getY() - 2, 22, 14, juce::Justification::right);
        g.drawText ("+1  mono-safe", (int) track.getRight() + 4, (int) track.getY() - 2, 90, 14, juce::Justification::left);
    }

    // ---------------- LIMITER: level coming in (bars), the ceiling, and the gain held back (red, from the top)
    void drawLimiter (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour col)
    {
        grid (g, b, 10, 4);
        auto Y = [&] (float db) { return b.getY() + b.getHeight() * juce::jlimit (0.f, 1.f, (6.f - db) / 42.f); };   // +6 dB at the top, -36 dB at the bottom
        juce::Path gr;
        gr.startNewSubPath (b.getX(), b.getY());
        for (int i = 0; i < histN; ++i)
        {
            const int idx = (head + i) % histN;
            const float x = b.getX() + b.getWidth() * (float) i / (float) (histN - 1);
            const float yl = Y (histB[(size_t) idx]);
            g.setColour (Theme::amber.withAlpha (0.30f));
            g.fillRect (x, yl, b.getWidth() / (float) histN + 0.5f, b.getBottom() - yl);
            gr.lineTo (x, b.getY() + juce::jlimit (0.f, 24.f, -histA[(size_t) idx]) / 24.f * b.getHeight() * 0.6f);
        }
        gr.lineTo (b.getRight(), b.getY());
        gr.closeSubPath();
        g.setColour (col.withAlpha (0.7f));
        g.fillPath (gr);
        const float ceil = pv ("fx_lim_ceil");
        g.setColour (Theme::text);
        g.drawHorizontalLine ((int) Y (ceil), b.getX(), b.getRight());
        g.setFont (12.f);
        g.drawText ("ceiling " + juce::String (ceil, 1) + " dB", (int) b.getX() + 6, (int) Y (ceil) + 3, 140, 14, juce::Justification::left);
        g.setFont (juce::Font (22.f, juce::Font::bold));
        g.setColour (col);
        g.drawText (juce::String (chain.isOn (fx) ? chain.limGr.load() : 0.f, 1) + " dB", (int) b.getRight() - 160, (int) b.getBottom() - 34, 154, 26, juce::Justification::right);
        g.setFont (11.f);
        g.setColour (Theme::dim);
        g.drawText ("held back", (int) b.getRight() - 160, (int) b.getBottom() - 48, 154, 12, juce::Justification::right);
    }

    void timerCallback() override
    {
        if (! isShowing() || fx < 0) return;
        if (fx == fxTransient)
        {
            histA[(size_t) head] = chain.isOn (fx) ? chain.transGain.load() : 0.f;
            histB[(size_t) head] = chain.isOn (fx) ? juce::Decibels::gainToDecibels (chain.transLevel.load(), -100.f) : -100.f;
            head = (head + 1) % histN;
        }
        else if (fx == fxLimiter)
        {
            histA[(size_t) head] = chain.isOn (fx) ? chain.limGr.load() : 0.f;
            histB[(size_t) head] = chain.isOn (fx) ? juce::Decibels::gainToDecibels (chain.limIn.load(), -100.f) : -100.f;
            head = (head + 1) % histN;
        }
        repaint (vis);
    }

    static constexpr int histN = 240;
    BeatStationProcessor& proc;
    FxChain& chain;
    int fx = -1, head = 0;
    float level = 0.f;
    std::array<float, histN> histA {}, histB {};
    std::vector<std::unique_ptr<LabeledKnob>> knobs;
    juce::Rectangle<int> vis, knobPanel;
};
