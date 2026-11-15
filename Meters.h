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
        if (side == 0)                                                 // the IN meter carries the INPUT volume knob and the record button
        {
            vol.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            vol.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            vol.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            vol.setRange (-30.0, 12.0, 0.1);
            vol.setDoubleClickReturnValue (true, 0.0);
            vol.setValue (0.0, juce::dontSendNotification);
            vol.setColour (juce::Slider::rotarySliderFillColourId, Theme::amber);
            vol.onValueChange = [this] { proc.inputGain = juce::Decibels::decibelsToGain ((float) vol.getValue()); repaint(); };
            addAndMakeVisible (vol);
        }
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
        clipRect = juce::Rectangle<int> ((getWidth() - 36) / 2, 25, 36, 17);
        const auto cr = clipRect.toFloat();
        if (clip)                                                      // glossy red, stays on until you click it
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff5a4d), cr.getCentreX(), cr.getY(), juce::Colour (0xffa30f14), cr.getCentreX(), cr.getBottom(), false));
            g.fillRoundedRectangle (cr, 4.f);
            g.setColour (juce::Colours::white.withAlpha (0.38f));
            g.fillRoundedRectangle (cr.reduced (2.f, 1.5f).withHeight (cr.getHeight() * 0.4f), 3.f);
            g.setColour (juce::Colour (0xffff9a90));
            g.drawRoundedRectangle (cr.reduced (0.5f), 4.f, 1.f);
        }
        else
        {
            g.setColour (juce::Colour (0xff3a2430));
            g.fillRoundedRectangle (cr, 4.f);
        }
        g.setColour (clip ? juce::Colours::white : Theme::dim);
        g.setFont (juce::Font (9.f, juce::Font::bold));
        g.drawText ("CLIP", clipRect, juce::Justification::centred);

        const int top = 48, bottom = getHeight() - footer, segs = 40, barW = 14, gap = 4;
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

    void paintOverChildren (juce::Graphics& g) override
    {
        if (side != 0) return;
        g.setColour (Theme::dim);
        g.setFont (juce::Font (9.f, juce::Font::bold));
        g.drawText ("VOL", 0, getHeight() - footer + 6, getWidth(), 10, juce::Justification::centred);
        g.setColour (Theme::text);
        const float db = (float) vol.getValue();
        g.drawText ((db > 0.f ? "+" : "") + juce::String (db, 1), 0, recRect.getY() - 14, getWidth(), 10, juce::Justification::centred);

        // record button: blends in when off, glossy red when recording
        const auto r = recRect.toFloat();
        if (recOn)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff5a4d), r.getCentreX(), r.getY(), juce::Colour (0xffa30f14), r.getCentreX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 6.f);
            g.setColour (juce::Colours::white.withAlpha (0.38f));
            g.fillRoundedRectangle (r.reduced (3.f, 2.f).withHeight (r.getHeight() * 0.38f), 4.f);      // the shine
            g.setColour (juce::Colour (0xffff9a90));
            g.drawRoundedRectangle (r.reduced (0.5f), 6.f, 1.f);
            g.setColour (juce::Colours::white);
        }
        else
        {
            g.setColour (Theme::panel.brighter (0.06f));
            g.fillRoundedRectangle (r, 6.f);
            g.setColour (Theme::grid.brighter (0.15f));
            g.drawRoundedRectangle (r.reduced (0.5f), 6.f, 1.f);
            g.setColour (Theme::dim);
        }
        g.setFont (juce::Font (10.f, juce::Font::bold));
        g.drawText ("REC", recRect, juce::Justification::centred);
    }

    void resized() override
    {
        recRect = { 3, getHeight() - 32, getWidth() - 6, 26 };
        vol.setBounds ((getWidth() - 38) / 2, getHeight() - footer + 16, 38, 38);
    }

    std::function<void()> onRecorded;                                  // called after a recording has landed on the selected pad

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (side == 0 && recRect.contains (e.getPosition()))
        {
            if (! recOn) { proc.startInputRecording(); recOn = true; }
            else finishRecording();
            repaint();
            return;
        }
        if (clipRect.contains (e.getPosition())) proc.clipped[side] = false;
        hold[0] = hold[1] = -100.f;                                    // any click also resets the peak-hold lines
    }

private:
    void finishRecording()
    {
        recOn = false;
        proc.stopInputRecording();
        juce::Component::SafePointer<MeterStrip> safe (this);         // a short wait so the audio side has finished writing
        juce::Timer::callAfterDelay (150, [safe]
        {
            if (safe == nullptr) return;
            if (safe->proc.commitRecording (safe->proc.selectedPad.load())) { if (safe->onRecorded) safe->onRecorded(); }
            else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Nothing was recorded",
                     "No audio came in. In the standalone app, open Options > Audio/MIDI Settings, pick your input, and untick \"Mute audio input\". "
                     "In a DAW, route audio into the plugin's input.");
        });
    }

    void timerCallback() override
    {
        if (side == 0)
        {
            if (recOn && ! proc.recArmed.load()) finishRecording();    // it stopped itself after 60 s
            const float db = juce::Decibels::gainToDecibels (proc.inputGain.load(), -30.f);
            if (std::abs (db - (float) vol.getValue()) > 0.06f) vol.setValue (db, juce::dontSendNotification);   // follows a loaded project
        }
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
    juce::Rectangle<int> clipRect, recRect;
    juce::Slider vol;
    bool recOn = false;
    static constexpr int footer = 104;                                 // room under the bars (both meters, so the bars line up)
};
