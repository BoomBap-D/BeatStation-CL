#pragma once
#include "PluginProcessor.h"
#include "Theme.h"
#include "Widgets.h"
#include "EnvelopeEditor.h"

// The SHAPE drawer: one free-form envelope editor that you switch between AMPLITUDE and FILTER, plus the filter controls.
// The filter starts as an open low-pass: turn CUTOFF down and you hear it straight away, even on a sound that is already playing.
class ShapePanel : public juce::Component
{
public:
    explicit ShapePanel (BeatStationProcessor& p) : proc (p), editor (p)
    {
        addAndMakeVisible (editor);
        ampTab.onClick = [this] { which = 0; applyTarget(); };
        fltTab.onClick = [this] { which = 1; applyTarget(); };
        addAndMakeVisible (ampTab);
        addAndMakeVisible (fltTab);
        static const char* names[] = { "Off", "Low-pass", "High-pass", "Band-pass" };
        for (int i = 0; i < 4; ++i)
        {
            typeBtn[i].setButtonText (names[i]);
            typeBtn[i].onClick = [this, i] { setType (i); };
            addAndMakeVisible (typeBtn[i]);
        }
        for (auto* k : { &cutoff, &res, &amount, &fade }) addAndMakeVisible (*k);
    }

    void setLearnCallback (std::function<void (const juce::String&)> cb) { for (auto* k : { &cutoff, &res, &amount, &fade }) k->onLearnMenu = cb; }

    void setPad (int p)
    {
        pad = p;
        applyTarget();
        cutoff.attach (proc.apvts, ParamID::fCut (p));
        res.attach (proc.apvts, ParamID::fRes (p));
        amount.attach (proc.apvts, ParamID::fAmt (p));
        fade.attach (proc.apvts, ParamID::rel (p));
        refreshButtons();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto left = r.removeFromLeft (560);
        auto tabs = left.removeFromTop (28);
        ampTab.setBounds (tabs.removeFromLeft (110));  tabs.removeFromLeft (6);
        fltTab.setBounds (tabs.removeFromLeft (90));
        left.removeFromTop (4);
        editor.setBounds (left);
        r.removeFromLeft (14);
        auto top = r.removeFromTop (34);
        for (auto& b : typeBtn) { b.setBounds (top.removeFromLeft (80)); top.removeFromLeft (3); }
        r.removeFromTop (6);
        const int kw = r.getWidth() / 4;
        for (auto* k : { &cutoff, &res, &amount, &fade }) k->setBounds (r.removeFromLeft (kw).withSizeKeepingCentre (80, 104));
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::dim);
        g.setFont (11.f);
        g.drawFittedText ("AMPLITUDE = loudness over time.  FILTER = how the cutoff moves over time; ENV AMT sets how far (in octaves, + opens, - closes).  FADE = tail length at the end of the chop.",
                          getLocalBounds().removeFromBottom (44).removeFromRight (326), juce::Justification::topLeft, 3);
    }

private:
    void applyTarget()
    {
        editor.setTarget (pad, which, which == 0 ? Theme::amber : Theme::green);
        ampTab.setToggleState (which == 0, juce::dontSendNotification);
        fltTab.setToggleState (which == 1, juce::dontSendNotification);
    }

    void setType (int i)
    {
        auto* prm = proc.apvts.getParameter (ParamID::fType (pad));
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
        refreshButtons();
    }

    void refreshButtons()
    {
        const int t = (int) proc.apvts.getRawParameterValue (ParamID::fType (pad))->load();
        for (int i = 0; i < 4; ++i) typeBtn[i].setToggleState (i == t, juce::dontSendNotification);
    }

    BeatStationProcessor& proc;
    EnvelopeEditor editor;
    juce::TextButton ampTab { "AMPLITUDE" }, fltTab { "FILTER" }, typeBtn[4];
    LabeledKnob cutoff { "CUTOFF", " Hz", Theme::green }, res { "RESO", "", Theme::green }, amount { "ENV AMT", " oct", Theme::green }, fade { "FADE", " ms", Theme::amber };
    int pad = 0, which = 0;
};
