#pragma once
#include "PluginProcessor.h"
#include "Theme.h"

// A tall stereo meter made of lit segments (left and right side by side). The thin line on each bar holds the highest level reached
// until you click the meter. The box under the label turns red if the signal clipped; click it to clear.
class MeterStrip : public juce::Component, private juce::Timer
{
public:
    MeterStrip (BeatStationProcessor& p, int sideIndex, juce::String label) : proc (p), side (sideIndex), name (std::move (label))
    {
        level[0] = level[1] = hold[0] = hold[1] = -100.f;
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.f);
        g.setColour (Theme::dim);
        g.setFont (juce::Font (12.f, juce::Font::bold));
        g.drawText (name, 0, 6, getWidth(), 16, juce::Justification::centred);

        const bool clip = proc.clipped[side].load();
        clipRect = juce::Rectangle<int> ((getWidth() - 32) / 2, 26, 32, 14);
        g.setColour (clip ? Theme::red : juce::Colour (0xff3a2430));
        g.fillRoundedRectangle (clipRect.toFloat(), 3.f);
        g.setColour (clip ? juce::Colours::white : Theme::dim);
        g.setFont (juce::Font (9.f, juce::Font::bold));
        g.drawText ("CLIP", clipRect, juce::Justification::centred);

        const int top = 48, bottom = getHeight() - 10, segs = 40, barW = 14, gap = 4;
        const float segH = (float) (bottom - top) / (float) segs;
        const int x0 = (getWidth() - 2 * barW - gap) / 2;
        for (int ch = 0; ch < 2; ++ch)
        {
            const int x = x0 + ch * (barW + gap);
            for (int s = 0; s < segs; ++s)
            {
                const float db = -60.f + ((float) s + 0.5f) * 60.f / (float) segs;
                const bool lit = db <= level[ch];
                const juce::Colour c = db > -3.f ? Theme::red : (db > -12.f ? Theme::amber : Theme::green);
                g.setColour (lit ? c : juce::Colour (0xff20242b));
                g.fillRect ((float) x, (float) bottom - (float) (s + 1) * segH + 1.f, (float) barW, segH - 2.f);
            }
            if (hold[ch] > -60.f)
            {
                const float y = (float) bottom - juce::jmap (hold[ch], -60.f, 0.f, 0.f, (float) (bottom - top));
                g.setColour (Theme::cream);
                g.fillRect ((float) x - 2.f, juce::jmax ((float) top, y) - 1.f, (float) barW + 4.f, 2.f);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (clipRect.contains (e.getPosition())) proc.clipped[side] = false;
        hold[0] = hold[1] = -100.f;                                    // any click also resets the peak-hold lines
    }

private:
    void timerCallback() override
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            const float p = proc.peak[side][ch].exchange (0.f);
            const float db = p > 1.0e-5f ? 20.f * std::log10 (p) : -100.f;
            level[ch] = juce::jmax (db, level[ch] - 1.8f);              // falls about 54 dB per second
            hold[ch] = juce::jmax (hold[ch], db);
        }
        repaint();
    }

    BeatStationProcessor& proc;
    int side;
    juce::String name;
    float level[2], hold[2];
    juce::Rectangle<int> clipRect;
};
