#pragma once
#include "Theme.h"

// Vintage pointer knob: charcoal body, chrome ring, cream wedge pointer.
// The tick scale around it lights up in the knob's accent colour from minimum up to the pointer,
// with a soft glow, and the tick under the pointer fades in smoothly as the value moves.
class VintageLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VintageLookAndFeel()
    {
        setColour (juce::TextButton::textColourOffId, Theme::dim);
        setColour (juce::TextButton::textColourOnId, juce::Colour (0xff111316));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.f);
        const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const float angle = startAngle + pos * (endAngle - startAngle);

        // Tick scale with lit trail
        constexpr int numTicks = 21;
        const bool bipolar = (bool) slider.getProperties().getWithDefault ("bipolar", false);
        for (int i = 0; i < numTicks; ++i)
        {
            const float t = (float) i / (float) (numTicks - 1);
            const float a = startAngle + t * (endAngle - startAngle);
            const bool major = (i % 5 == 0);
            const juce::Point<float> dir (std::sin (a), -std::cos (a));
            const juce::Line<float> tick (c + dir * (r * (major ? 0.76f : 0.82f)), c + dir * (r * 0.95f));
            // Normal knobs light up from the minimum. Centre-zero knobs (pan, tune, gain) light outwards from the top:
            // nothing lit in the middle, lights to the left going left, lights to the right going right.
            float lit;
            if (! bipolar) lit = juce::jlimit (0.f, 1.f, pos * (float) (numTicks - 1) - (float) i + 1.f);
            else if (pos > 0.5f) lit = t > 0.5f ? juce::jlimit (0.f, 1.f, (pos - t) * (float) (numTicks - 1) + 1.f) : 0.f;
            else                 lit = t < 0.5f ? juce::jlimit (0.f, 1.f, (t - pos) * (float) (numTicks - 1) + 1.f) : 0.f;
            if (bipolar && std::abs (pos - 0.5f) < 0.004f) lit = 0.f;

            if (lit > 0.f)
            {
                g.setColour (accent.withAlpha (0.30f * lit));
                g.drawLine (tick, 5.f);                      // glow
            }
            const bool centreMark = bipolar && i == numTicks / 2;         // the line at the top that marks the centre
            g.setColour (centreMark ? Theme::cream : Theme::tickOff.interpolatedWith (accent, lit));
            g.drawLine (tick, major ? 2.4f : 1.8f);
        }

        // Body
        const float br = r * 0.60f;
        juce::ColourGradient body (juce::Colour (0xff3b3f48), c.x - br * 0.4f, c.y - br * 0.5f,
                                   juce::Colour (0xff0f1114), c.x + br * 0.6f, c.y + br * 0.7f, true);
        g.setGradientFill (body);
        g.fillEllipse (c.x - br, c.y - br, br * 2.f, br * 2.f);
        g.setColour (juce::Colour (0xffc9ccd3));
        g.drawEllipse (c.x - br, c.y - br, br * 2.f, br * 2.f, 1.8f);

        // Cream wedge pointer
        const float len = r * 0.66f, wid = r * 0.26f;
        juce::Path p;
        p.startNewSubPath (-wid * 0.5f, 0.f);
        p.lineTo (-wid * 0.16f, -len);
        p.lineTo ( wid * 0.16f, -len);
        p.lineTo ( wid * 0.5f, 0.f);
        p.closeSubPath();
        const auto xf = juce::AffineTransform::rotation (angle).translated (c.x, c.y);
        g.setColour (Theme::cream);
        g.fillPath (p, xf);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.strokePath (p, juce::PathStrokeType (1.f), xf);
        g.setColour (juce::Colour (0xff0b0c0f));
        g.fillEllipse (c.x - r * 0.1f, c.y - r * 0.1f, r * 0.2f, r * 0.2f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool) override
    {
        const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        g.setColour (on ? Theme::amber : (over ? Theme::panel.brighter (0.15f) : Theme::panel));
        g.fillRoundedRectangle (r, 6.f);
        g.setColour (on ? Theme::amber : Theme::grid.brighter (0.2f));
        g.drawRoundedRectangle (r, 6.f, 1.f);
    }
};
