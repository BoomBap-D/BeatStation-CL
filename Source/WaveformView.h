#pragma once
#include "PluginProcessor.h"
#include "Theme.h"

// The sample window for the selected pad.
//  - Drag the two cream handles to trim what the pad plays. Other pads that share this sample show as thin markers.
//  - AMP ENV and FILTER ENV: switch each envelope on or off. When on, its curve is drawn over the sample: drag the dots,
//    double-click to add a dot, double-click or right-click a dot to remove it. The envelope runs across the trimmed region.
//  - Click the sample to hear it (Shift+click plays the whole sample).
//  - Drag from the DRAG tab onto any pad, or onto a sequencer track, to copy this sound there.
class WaveformView : public juce::Component
{
public:
    explicit WaveformView (BeatStationProcessor& p) : proc (p)
    {
        for (auto* b : { &ampBtn, &fltBtn }) { b->setClickingTogglesState (true); addAndMakeVisible (*b); }
    }

    void setPad (int p)
    {
        pad = p;
        ampAtt.reset();
        fltAtt.reset();
        ampAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamID::ampOn (p), ampBtn);
        fltAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamID::fltOn (p), fltBtn);
        reload();
    }

    void reload()
    {
        info = proc.getPad (pad);
        env[0] = proc.getEnv (pad, 0);
        env[1] = proc.getEnv (pad, 1);
        peaks.clear();
        others.clear();
        if (info.sample != nullptr)
        {
            const float* x = info.sample->data.getReadPointer (0);
            const juce::int64 n = info.sample->data.getNumSamples();
            constexpr int cols = 600;
            peaks.assign (cols, 0.f);
            for (int c = 0; c < cols; ++c)
            {
                float pk = 0.f;
                for (juce::int64 i = n * c / cols; i < n * (c + 1) / cols; ++i) pk = juce::jmax (pk, std::abs (x[i]));
                peaks[(size_t) c] = pk;
            }
            for (int i = 0; i < numPads; ++i)
                if (i != pad) { const auto o = proc.getPad (i); if (o.sample == info.sample) others.push_back ({ i, o.start }); }
        }
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6);
        fltBtn.setBounds (r.removeFromRight (96).removeFromBottom (22));
        r.removeFromRight (4);
        ampBtn.setBounds (r.removeFromRight (84).removeFromBottom (22));
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (Theme::graphBg);
        g.fillRoundedRectangle (b, 8.f);
        g.setColour (Theme::grid.brighter (0.1f));
        g.drawRoundedRectangle (b.reduced (0.5f), 8.f, 1.f);

        if (info.sample == nullptr)
        {
            g.setColour (Theme::dim);
            g.setFont (13.f);
            g.drawText ("Drop audio on the pads, or use BROWSE / Load", getLocalBounds(), juce::Justification::centred);
            return;
        }

        const float mid = b.getCentreY(), half = b.getHeight() * 0.40f;
        g.setColour (Theme::amber.withAlpha (0.6f));
        for (size_t c = 0; c < peaks.size(); ++c)
        {
            const float x = b.getX() + b.getWidth() * (float) c / (float) peaks.size();
            g.drawVerticalLine ((int) x, mid - peaks[c] * half, mid + peaks[c] * half);
        }

        const float xs = regionX0(), xe = regionX1();
        g.setColour (juce::Colours::black.withAlpha (0.55f));          // dim everything outside the trimmed region
        g.fillRect (b.getX(), b.getY(), xs - b.getX(), b.getHeight());
        g.fillRect (xe, b.getY(), b.getRight() - xe, b.getHeight());

        g.setFont (11.f);
        for (const auto& o : others)
        {
            const float x = b.getX() + o.second * b.getWidth();
            g.setColour (Theme::dim);
            g.drawVerticalLine ((int) x, b.getY(), b.getBottom());
            g.drawText (juce::String (o.first + 1), (int) x + 3, (int) b.getY() + 3, 20, 12, juce::Justification::left);
        }
        g.setColour (Theme::cream);
        g.fillRect (xs - 1.f, b.getY(), 2.f, b.getHeight());
        g.fillRect (xe - 1.f, b.getY(), 2.f, b.getHeight());
        g.drawText (juce::String (pad + 1), (int) xs + 4, (int) b.getBottom() - 16, 24, 12, juce::Justification::left);

        for (int w = 0; w < 2; ++w)                                    // envelope curves, only for the envelopes that are switched on
        {
            if (! envOn (w)) continue;
            const auto col = w == 0 ? Theme::amber : Theme::green;
            juce::Path curve;
            for (int i = 0; i < env[w].n; ++i)
            {
                const auto pt = nodePos (w, i);
                if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
            }
            const auto last = nodePos (w, env[w].n - 1);
            curve.lineTo (xe, last.y);
            g.setColour (col);
            g.strokePath (curve, juce::PathStrokeType (2.f));
            for (int i = 0; i < env[w].n; ++i)
            {
                const auto pt = nodePos (w, i);
                g.setColour (col.withAlpha (0.35f));
                g.fillEllipse (pt.x - 9.f, pt.y - 9.f, 18.f, 18.f);
                g.setColour (Theme::cream);
                g.fillEllipse (pt.x - 4.5f, pt.y - 4.5f, 9.f, 9.f);
            }
        }

        const auto grab = grabRect();                                  // the drag-to-copy tab
        g.setColour (juce::Colour (0xff262a31));
        g.fillRoundedRectangle (grab, 5.f);
        g.setColour (Theme::dim);
        g.setFont (juce::Font (10.f, juce::Font::bold));
        g.drawText ("DRAG TO PAD", grab.toNearestInt(), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        mode = none;
        if (info.sample == nullptr) return;
        if (grabRect().contains (e.position)) { mode = copy; return; }
        for (int w = 0; w < 2; ++w)                                    // envelope dots come first
            if (envOn (w))
                if (const int i = hitNode (w, e.position); i >= 0)
                {
                    if (e.mods.isPopupMenu()) { removeNode (w, i); return; }
                    mode = envNode; envWhich = w; envIdx = i;
                    return;
                }
        if (std::abs (e.position.x - regionX0()) < 10.f) mode = handleStart;
        else if (std::abs (e.position.x - regionX1()) < 10.f) mode = handleEnd;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (mode == copy)
        {
            if (e.getDistanceFromDragStart() > 6 && ! copying)
                if (auto* dc = juce::DragAndDropContainer::findParentDragContainerFor (this)) { copying = true; dc->startDragging ("padcopy:" + juce::String (pad), this); }
        }
        else if (mode == envNode) moveNode (e.position);
        else if (mode == handleStart || mode == handleEnd)
        {
            const float v = juce::jlimit (0.f, 1.f, e.position.x / (float) getWidth());
            if (mode == handleStart) info.start = juce::jmin (v, info.end - 0.005f);
            else                     info.end = juce::jmax (v, info.start + 0.005f);
            proc.setRegion (pad, info.start, info.end);
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const bool wasClick = (mode == none) && ! e.mouseWasDraggedSinceMouseDown() && info.sample != nullptr && ! e.mods.isPopupMenu();
        mode = none;
        copying = false;
        if (! wasClick) return;
        if (e.mods.isShiftDown()) { proc.auditionWhole (pad); return; }       // Shift+click: the whole sample
        const int note = proc.getPadNote (pad);                               // click: play the pad
        proc.keyboardState.noteOn (10, note, 0.9f);
        juce::Component::SafePointer<WaveformView> safe (this);
        juce::Timer::callAfterDelay (350, [safe, note] { if (safe != nullptr) safe->proc.keyboardState.noteOff (10, note, 0.f); });
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (info.sample == nullptr || grabRect().contains (e.position)) return;
        for (int w = 0; w < 2; ++w) if (envOn (w)) if (const int i = hitNode (w, e.position); i >= 0) { removeNode (w, i); return; }
        int best = -1; float bestD = 1.0e9f;                                  // add a dot to whichever envelope curve is closest
        for (int w = 0; w < 2; ++w)
        {
            if (! envOn (w) || env[w].n >= 8) continue;
            const float d = std::abs (nodeY (valueAt (w, xToT (e.position.x))) - e.position.y);
            if (d < bestD) { bestD = d; best = w; }
        }
        if (best < 0) return;
        const float t = xToT (e.position.x), v = yToV (e.position.y);
        int at = 1;
        while (at < env[best].n && env[best].p[at].t < t) ++at;
        for (int i = env[best].n; i > at; --i) env[best].p[i] = env[best].p[i - 1];
        env[best].p[at] = { t, v };
        ++env[best].n;
        proc.setEnv (pad, best, env[best]);
        repaint();
    }

private:
    enum Mode { none, copy, envNode, handleStart, handleEnd };

    bool envOn (int w) const { return proc.apvts.getRawParameterValue (w == 0 ? ParamID::ampOn (pad) : ParamID::fltOn (pad))->load() > 0.5f; }
    juce::Rectangle<float> grabRect() const { return { 6.f, 6.f, 84.f, 18.f }; }
    float regionX0() const { return info.start * (float) getWidth(); }
    float regionX1() const { return info.end * (float) getWidth(); }

    // the envelope's time axis is the trimmed region (the time that region lasts at normal pitch)
    float regionMs() const
    {
        if (info.sample == nullptr) return 100.f;
        return juce::jmax (10.f, (info.end - info.start) * (float) info.sample->data.getNumSamples() / (float) info.sample->sampleRate * 1000.f);
    }
    float tToX (float t) const { return regionX0() + juce::jlimit (0.f, 1.f, t / regionMs()) * (regionX1() - regionX0()); }
    float xToT (float x) const { return juce::jlimit (0.f, regionMs(), (x - regionX0()) / juce::jmax (1.f, regionX1() - regionX0()) * regionMs()); }
    float nodeY (float v) const { return (float) getHeight() - 6.f - v * ((float) getHeight() - 12.f); }
    float yToV (float y) const { return juce::jlimit (0.f, 1.f, ((float) getHeight() - 6.f - y) / ((float) getHeight() - 12.f)); }
    float valueAt (int w, float t) const { return env[w].at (t); }
    juce::Point<float> nodePos (int w, int i) const { return { tToX (env[w].p[i].t), nodeY (env[w].p[i].v) }; }

    int hitNode (int w, juce::Point<float> pt) const
    {
        int best = -1; float bestD = 12.f;
        for (int i = 0; i < env[w].n; ++i)
            if (const float d = nodePos (w, i).getDistanceFrom (pt); d < bestD) { bestD = d; best = i; }
        return best;
    }

    void moveNode (juce::Point<float> pos)
    {
        auto& e = env[envWhich];
        float t = xToT (pos.x);
        if (envIdx == 0) t = 0.f;
        else t = juce::jlimit (e.p[envIdx - 1].t + 1.f, envIdx < e.n - 1 ? e.p[envIdx + 1].t - 1.f : regionMs(), t);
        e.p[envIdx] = { t, yToV (pos.y) };
        proc.setEnv (pad, envWhich, e);
        repaint();
    }

    void removeNode (int w, int i)
    {
        auto& e = env[w];
        if (e.n <= 2 || i <= 0) return;
        for (int k = i; k < e.n - 1; ++k) e.p[k] = e.p[k + 1];
        --e.n;
        proc.setEnv (pad, w, e);
        repaint();
    }

    BeatStationProcessor& proc;
    int pad = 0, envWhich = 0, envIdx = 0;
    Mode mode = none;
    bool copying = false;
    PadInfo info;
    EnvData env[2];
    std::vector<float> peaks;
    std::vector<std::pair<int, float>> others;
    juce::TextButton ampBtn { "AMP ENV" }, fltBtn { "FILTER ENV" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ampAtt, fltAtt;
};
