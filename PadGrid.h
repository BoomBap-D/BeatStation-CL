#pragma once
#include "PluginProcessor.h"
#include "Theme.h"

// 4x4 MPC-style pads (pad 1 bottom-left, pad 13 top-left). Click to play and select, right-click for MIDI Learn,
// drop audio files (or drag a sound from the browser) onto a pad to load it. Dropping several files fills consecutive pads.
class PadGrid : public juce::Component, public juce::FileDragAndDropTarget, public juce::DragAndDropTarget, private juce::Timer
{
public:
    explicit PadGrid (BeatStationProcessor& p) : proc (p) { startTimerHz (30); }

    std::function<void (int)> onSelect, onRightClick;
    std::function<void()> onSamplesChanged;
    int selected = 0;

    void paint (juce::Graphics& g) override
    {
        const uint32_t mask = proc.activePads.load();
        const juce::String learn = proc.getLearnTarget();
        const int learnPad = learn.startsWith ("pad:") ? learn.fromFirstOccurrenceOf (":", false, false).getIntValue() : -1;
        for (int i = 0; i < numPads; ++i)
        {
            const auto r = padRect (i);
            const bool on = (mask >> i) & 1u, sel = (i == selected);
            const auto info = proc.getPad (i);

            if (on)
            {
                g.setColour (Theme::amber.withAlpha (0.25f));
                g.fillRoundedRectangle (r.expanded (4.f), 12.f);
                g.setColour (juce::Colour (0xff3a2a0a));
            }
            else
            {
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c3037), r.getX(), r.getY(),
                                                         juce::Colour (0xff1b1d22), r.getX(), r.getBottom(), false));
            }
            g.fillRoundedRectangle (r, 8.f);
            g.setColour (i == learnPad ? Theme::red : (on ? Theme::amber : (sel ? Theme::cream : juce::Colour (0xff3a3f4a))));
            g.drawRoundedRectangle (r.reduced (0.5f), 8.f, (sel || i == learnPad) ? 2.f : 1.f);

            g.setColour (Theme::dim);
            g.setFont (juce::Font (11.f, juce::Font::bold));
            g.drawText (juce::String (i + 1) + (proc.getPadNote (i) != firstNote + i ? "  (note " + juce::String (proc.getPadNote (i)) + ")" : juce::String()),
                        r.toNearestInt().reduced (8, 6), juce::Justification::topLeft);

            g.setColour (info.sample != nullptr ? Theme::text : Theme::dim.withAlpha (0.6f));
            g.setFont (12.f);
            g.drawFittedText (i == learnPad ? "Press a pad..." : (info.sample != nullptr ? info.sample->name : "Empty"), r.toNearestInt().reduced (8, 6),
                              juce::Justification::bottomLeft, 2);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = hit (e.position);
        if (i < 0) return;
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick (i); return; }
        selected = i;
        if (onSelect) onSelect (i);
        const auto r = padRect (i);
        const float vel = juce::jlimit (0.3f, 1.f, 0.3f + 0.7f * (e.position.y - r.getY()) / r.getHeight());   // lower on the pad = harder
        downNote = proc.getPadNote (i);
        proc.keyboardState.noteOn (10, downNote, vel);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (downNote >= 0) proc.keyboardState.noteOff (10, downNote, 0.f);
        downNote = -1;
    }

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int x, int y) override
    {
        const int start = hit ({ (float) x, (float) y });
        if (start < 0) return;
        for (int k = 0; k < files.size() && start + k < numPads; ++k)
            proc.loadSample (start + k, juce::File (files[k]));
        finishLoad (start);
    }

    bool isInterestedInDragSource (const SourceDetails& d) override { const auto s = d.description.toString(); return s.startsWith ("sample:") || s.startsWith ("padcopy:"); }
    void itemDropped (const SourceDetails& d) override
    {
        const int pad = hit (d.localPosition.toFloat());
        if (pad < 0) return;
        const auto s = d.description.toString();
        if (s.startsWith ("sample:")) proc.loadSample (pad, juce::File (s.fromFirstOccurrenceOf ("sample:", false, false)));        // from the browser
        else if (s.startsWith ("padcopy:")) proc.copyPad (s.fromFirstOccurrenceOf (":", false, false).getIntValue(), pad);          // from the sample window
        finishLoad (pad);
    }

private:
    void finishLoad (int pad)
    {
        selected = pad;
        if (onSelect) onSelect (pad);
        if (onSamplesChanged) onSamplesChanged();
        repaint();
    }

    juce::Rectangle<float> padRect (int i) const
    {
        const float gap = 8.f, w = ((float) getWidth() - 3.f * gap) / 4.f, h = ((float) getHeight() - 3.f * gap) / 4.f;
        const int col = i % 4, row = 3 - i / 4;
        return { (float) col * (w + gap), (float) row * (h + gap), w, h };
    }

    int hit (juce::Point<float> p) const
    {
        for (int i = 0; i < numPads; ++i) if (padRect (i).contains (p)) return i;
        return -1;
    }

    void timerCallback() override { repaint(); }

    BeatStationProcessor& proc;
    int downNote = -1;
};
