#pragma once
#include "PluginProcessor.h"
#include "Theme.h"

// Shows the selected pad's sample, the region it plays (drag the two handles), and where the other pads
// that share this sample start, so after a chop you can see every slice.
class WaveformView : public juce::Component
{
public:
    explicit WaveformView (BeatStationProcessor& p) : proc (p) {}

    void setPad (int p) { pad = p; reload(); }

    void reload()
    {
        info = proc.getPad (pad);
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
            g.drawText ("Drop audio files on the pads, or press Load", getLocalBounds(), juce::Justification::centred);
            return;
        }

        const float mid = b.getCentreY(), half = b.getHeight() * 0.42f;
        g.setColour (Theme::amber.withAlpha (0.85f));
        for (size_t c = 0; c < peaks.size(); ++c)
        {
            const float x = b.getX() + b.getWidth() * (float) c / (float) peaks.size();
            g.drawVerticalLine ((int) x, mid - peaks[c] * half, mid + peaks[c] * half);
        }

        const float xs = b.getX() + info.start * b.getWidth(), xe = b.getX() + info.end * b.getWidth();
        g.setColour (juce::Colours::black.withAlpha (0.55f));                  // dim everything outside the region
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
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (info.sample == nullptr) return;
        const float xs = info.start * (float) getWidth(), xe = info.end * (float) getWidth();
        drag = std::abs (e.position.x - xs) < 12.f ? 1 : (std::abs (e.position.x - xe) < 12.f ? 2 : 0);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag == 0) return;
        const float v = juce::jlimit (0.f, 1.f, e.position.x / (float) getWidth());
        if (drag == 1) info.start = juce::jmin (v, info.end - 0.005f);
        else           info.end = juce::jmax (v, info.start + 0.005f);
        proc.setRegion (pad, info.start, info.end);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override { drag = 0; }

private:
    BeatStationProcessor& proc;
    int pad = 0, drag = 0;
    PadInfo info;
    std::vector<float> peaks;
    std::vector<std::pair<int, float>> others;
};
