#pragma once
#include "MasterFx.h"
#include "Theme.h"
#include "CompMath.h"

// Compressor visuals: transfer curve with live dot, scrolling level history with gain-reduction overlay, GR meter.
class MasterCompView : public juce::Component, private juce::Timer
{
public:
    explicit MasterCompView (MasterFx& p) : proc (p) { inH.fill (-100.f); startTimerHz (30); }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        const auto tr = area.removeFromLeft (juce::jmin (area.getHeight(), area.getWidth() * 0.4f));
        area.removeFromLeft (8.f);
        const auto meter = area.removeFromRight (40.f);
        area.removeFromRight (8.f);
        const auto hist = area;
        for (auto r : { tr, hist, meter }) panel (g, r);

        const float thr = v (MfxID::thresh), ratio = v (MfxID::ratio), knee = v (MfxID::knee);
        const float gr = proc.grDb.load();

        // Transfer curve (-60..0 dB on both axes)
        auto mx = [&] (float db) { return tr.getX() + juce::jmap (db, -60.f, 0.f, 0.f, tr.getWidth()); };
        auto my = [&] (float db) { return tr.getBottom() - juce::jmap (db, -60.f, 0.f, 0.f, tr.getHeight()); };
        for (int db = -50; db < 0; db += 10)
        {
            g.setColour (Theme::grid);
            g.drawVerticalLine ((int) mx ((float) db), tr.getY(), tr.getBottom());
            g.drawHorizontalLine ((int) my ((float) db), tr.getX(), tr.getRight());
        }
        g.setColour (Theme::grid.brighter (0.3f));
        g.drawLine (tr.getX(), tr.getBottom(), tr.getRight(), tr.getY(), 1.f);     // unity line
        g.setColour (Theme::amber.withAlpha (0.5f));
        g.drawVerticalLine ((int) mx (thr), tr.getY(), tr.getBottom());            // threshold

        juce::Path curve;
        for (float x = 0.f; x <= tr.getWidth(); x += 2.f)
        {
            const float inDb = juce::jmap (x, 0.f, tr.getWidth(), -60.f, 0.f);
            const float outDb = juce::jlimit (-60.f, 0.f, CompMath::staticCurve (inDb, thr, ratio, knee));
            if (x == 0.f) curve.startNewSubPath (tr.getX(), my (outDb)); else curve.lineTo (tr.getX() + x, my (outDb));
        }
        g.setColour (Theme::amber);
        g.strokePath (curve, juce::PathStrokeType (3.f));

        const float liveIn = juce::jlimit (-60.f, 0.f, proc.inDb.load());
        const float liveOut = juce::jlimit (-60.f, 0.f, liveIn + gr);
        g.setColour (Theme::text);
        g.fillEllipse (mx (liveIn) - 6.f, my (liveOut) - 6.f, 12.f, 12.f);
        g.setColour (Theme::amber);
        g.drawEllipse (mx (liveIn) - 6.f, my (liveOut) - 6.f, 12.f, 12.f, 2.5f);

        // Level history + gain reduction hanging from the top
        juce::Path in, grP;
        in.startNewSubPath (hist.getX(), hist.getBottom());
        grP.startNewSubPath (hist.getX(), hist.getY());
        for (int i = 0; i < histN; ++i)
        {
            const int idx = (head + i) % histN;
            const float x = hist.getX() + hist.getWidth() * (float) i / (float) (histN - 1);
            in.lineTo (x, hist.getBottom() - juce::jmap (juce::jlimit (-60.f, 0.f, inH[(size_t) idx]), -60.f, 0.f, 0.f, hist.getHeight()));
            grP.lineTo (x, hist.getY() + juce::jmap (juce::jlimit (0.f, 24.f, -grH[(size_t) idx]), 0.f, 24.f, 0.f, hist.getHeight() * 0.7f));
        }
        in.lineTo (hist.getRight(), hist.getBottom()); in.closeSubPath();
        grP.lineTo (hist.getRight(), hist.getY());     grP.closeSubPath();
        g.setColour (juce::Colour (0xff3ad0e0).withAlpha (0.30f));  g.fillPath (in);     // the level coming in, in blue so the red gain reduction stands out
        g.setColour (Theme::red.withAlpha (0.65f));    g.fillPath (grP);
        g.setColour (Theme::dim);
        g.setFont (11.f);
        g.drawText ("INPUT / GAIN REDUCTION", hist.toNearestInt().reduced (8, 6), juce::Justification::bottomLeft);

        // GR meter (0 to -24 dB)
        const float f = juce::jlimit (0.f, 1.f, -gr / 24.f);
        g.setColour (Theme::red);
        g.fillRoundedRectangle (meter.getX() + 10.f, meter.getY() + 6.f, meter.getWidth() - 20.f, (meter.getHeight() - 28.f) * f, 3.f);
        g.setColour (Theme::dim);
        g.drawText (juce::String (gr, 1), meter.toNearestInt().removeFromBottom (18), juce::Justification::centred);
    }

private:
    static constexpr int histN = 300;

    static void panel (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (Theme::graphBg);
        g.fillRoundedRectangle (r, 8.f);
        g.setColour (Theme::grid.brighter (0.1f));
        g.drawRoundedRectangle (r.reduced (0.5f), 8.f, 1.f);
    }
    float v (const juce::String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    void timerCallback() override
    {
        inH[(size_t) head] = proc.inDb.load();
        grH[(size_t) head] = proc.grDb.load();
        head = (head + 1) % histN;
        repaint();
    }

    MasterFx& proc;
    std::array<float, histN> inH {}, grH {};
    int head = 0;
};
