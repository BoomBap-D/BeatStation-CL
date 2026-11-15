#pragma once
#include "PluginProcessor.h"
#include "Theme.h"

// A free-form envelope editor. Drag any dot anywhere (left-right = time, up-down = level). Double-click empty space to add a
// dot (up to 8), double-click or right-click a dot to remove it. The same editor shapes the amplitude and the filter.
class EnvelopeEditor : public juce::Component
{
public:
    explicit EnvelopeEditor (BeatStationProcessor& p) : proc (p)
    {
        addAndMakeVisible (rangeBtn);
        rangeBtn.onClick = [this] { rangeIdx = (rangeIdx + 1) % 5; fitToRange(); updateRangeText(); repaint(); };
        updateRangeText();
    }

    void setTarget (int padIndex, int whichEnv, juce::Colour c)
    {
        pad = padIndex;
        which = whichEnv;
        col = c;
        env = proc.getEnv (pad, which);
        repaint();
    }

    void resized() override { rangeBtn.setBounds (getWidth() - 110, 4, 100, 22); }

    void paint (juce::Graphics& g) override
    {
        const auto full = getLocalBounds().toFloat();
        g.setColour (Theme::graphBg);
        g.fillRoundedRectangle (full, 8.f);
        g.setColour (Theme::grid.brighter (0.1f));
        g.drawRoundedRectangle (full.reduced (0.5f), 8.f, 1.f);

        const auto b = plot();
        g.setFont (11.f);
        for (int i = 0; i <= 4; ++i)
        {
            const float x = b.getX() + b.getWidth() * (float) i / 4.f;
            g.setColour (Theme::grid);
            g.drawVerticalLine ((int) x, b.getY(), b.getBottom());
            g.setColour (Theme::dim);
            g.drawText (juce::String (juce::roundToInt (range() * (float) i / 4.f)) + " ms", (int) x - 30, (int) b.getBottom() + 3, 60, 14, juce::Justification::centred);
        }
        for (float v : { 0.f, 0.5f, 1.f })
        {
            g.setColour (Theme::grid);
            g.drawHorizontalLine ((int) vToY (v), b.getX(), b.getRight());
        }

        juce::Path curve;
        for (int i = 0; i < env.n; ++i)
        {
            const auto pt = nodePos (i);
            if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
        }
        const auto last = nodePos (env.n - 1);
        juce::Path fill (curve);
        fill.lineTo (b.getRight(), last.y);                           // after the last dot the level holds
        fill.lineTo (b.getRight(), b.getBottom());
        fill.lineTo (b.getX(), b.getBottom());
        fill.closeSubPath();
        g.setColour (col.withAlpha (0.14f));
        g.fillPath (fill);
        g.setColour (col);
        g.strokePath (curve, juce::PathStrokeType (2.4f));
        g.setColour (col.withAlpha (0.5f));
        g.drawLine (last.x, last.y, b.getRight(), last.y, 1.5f);

        for (int i = 0; i < env.n; ++i)
        {
            const auto pt = nodePos (i);
            g.setColour (col.withAlpha (0.3f));
            g.fillEllipse (pt.x - 10.f, pt.y - 10.f, 20.f, 20.f);
            g.setColour (Theme::cream);
            g.fillEllipse (pt.x - 5.f, pt.y - 5.f, 10.f, 10.f);
        }

        g.setColour (Theme::dim);
        g.drawText ("Double-click to add a dot. Double-click a dot to remove it.", 12, 6, 340, 16, juce::Justification::left);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        drag = hitNode (e.position);
        if (e.mods.isPopupMenu() && drag > 0) { removeNode (drag); drag = -1; }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag < 0) return;
        float t = xToT (e.position.x);
        const float v = yToV (e.position.y);
        if (drag == 0) t = 0.f;                                       // the first dot always sits at the start of the hit
        else t = juce::jlimit (env.p[drag - 1].t + 1.f, drag < env.n - 1 ? env.p[drag + 1].t - 1.f : range(), t);
        env.p[drag] = { t, v };
        proc.setEnv (pad, which, env);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override { drag = -1; }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int h = hitNode (e.position);
        if (h > 0) { removeNode (h); return; }
        if (h == 0 || env.n >= 8) return;
        const float t = xToT (e.position.x), v = yToV (e.position.y);
        int at = 1;
        while (at < env.n && env.p[at].t < t) ++at;
        for (int i = env.n; i > at; --i) env.p[i] = env.p[i - 1];
        env.p[at] = { t, v };
        ++env.n;
        proc.setEnv (pad, which, env);
        repaint();
    }

private:
    static constexpr float ranges[5] = { 250.f, 500.f, 1000.f, 2000.f, 4000.f };

    float range() const { return ranges[rangeIdx]; }
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().withTrimmedTop (30.f).withTrimmedBottom (20.f).reduced (16.f, 4.f); }
    float tToX (float t) const { const auto b = plot(); return b.getX() + juce::jlimit (0.f, 1.f, t / range()) * b.getWidth(); }
    float xToT (float x) const { const auto b = plot(); return juce::jlimit (0.f, range(), (x - b.getX()) / b.getWidth() * range()); }
    float vToY (float v) const { const auto b = plot(); return b.getBottom() - v * b.getHeight(); }
    float yToV (float y) const { const auto b = plot(); return juce::jlimit (0.f, 1.f, (b.getBottom() - y) / b.getHeight()); }
    juce::Point<float> nodePos (int i) const { return { tToX (env.p[i].t), vToY (env.p[i].v) }; }

    int hitNode (juce::Point<float> pt) const
    {
        int best = -1; float bestD = 14.f;
        for (int i = 0; i < env.n; ++i)
            if (const float d = nodePos (i).getDistanceFrom (pt); d < bestD) { bestD = d; best = i; }
        return best;
    }

    void removeNode (int i)
    {
        if (env.n <= 2 || i <= 0) return;
        for (int k = i; k < env.n - 1; ++k) env.p[k] = env.p[k + 1];
        --env.n;
        proc.setEnv (pad, which, env);
        repaint();
    }

    void fitToRange()
    {
        for (int i = 1; i < env.n; ++i) env.p[i].t = juce::jlimit (env.p[i - 1].t + 1.f, range(), env.p[i].t);
        proc.setEnv (pad, which, env);
    }

    void updateRangeText() { rangeBtn.setButtonText ("Range " + juce::String (juce::roundToInt (range())) + " ms"); }

    BeatStationProcessor& proc;
    EnvData env;
    juce::Colour col { Theme::amber };
    juce::TextButton rangeBtn;
    int pad = 0, which = 0, drag = -1, rangeIdx = 2;
};
