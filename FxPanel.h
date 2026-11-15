#pragma once
#include "PluginProcessor.h"
#include "Theme.h"
#include "Widgets.h"
#include "MasterEqView.h"
#include "MasterCompView.h"
#include "RoomView.h"

// The master effects window: an EQ and a compressor on the final mix. Show the EQ, the compressor, or both side by side.
// Each has its own on/off switch (lit = on). The compressor starts off.
class FxPanel : public juce::Component
{
public:
    std::function<void()> onBack;

    explicit FxPanel (BeatStationProcessor& p) : proc (p), eqView (p.masterFx), compView (p.masterFx), roomView (p)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &eqView, &compView, &roomView, &eqTab, &compTab, &bothTab, &roomTab, &eqOnBtn, &compOnBtn, &backBtn,
                                                                  &bFreq, &bGain, &bQ, &thresh, &ratio, &knee, &attack, &release, &makeup, &mix })
            addAndMakeVisible (*c);
        eqOnBtn.setClickingTogglesState (true);
        compOnBtn.setClickingTogglesState (true);
        eqOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, MfxID::eqOn, eqOnBtn);
        compOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, MfxID::compOn, compOnBtn);

        eqTab.onClick = [this] { setMode (0); };
        compTab.onClick = [this] { setMode (1); };
        bothTab.onClick = [this] { setMode (2); };
        roomTab.onClick = [this] { setMode (3); };
        backBtn.onClick = [this] { if (onBack) onBack(); };

        thresh.attach (p.apvts, MfxID::thresh);   ratio.attach (p.apvts, MfxID::ratio);     knee.attach (p.apvts, MfxID::knee);
        attack.attach (p.apvts, MfxID::attack);   release.attach (p.apvts, MfxID::release); makeup.attach (p.apvts, MfxID::makeup);
        mix.attach (p.apvts, MfxID::mix);
        eqView.onSelect = [this] (int b) { selectBand (b); };
        selectBand (2);
        setMode (2);
    }

    void setLearnCallback (std::function<void (const juce::String& target)> cb)
    {
        for (auto* k : { &bFreq, &bGain, &bQ, &thresh, &ratio, &knee, &attack, &release, &makeup, &mix })
            k->onLearnMenu = [cb] (const juce::String& id) { cb ("param:" + id); };
        roomView.setLearnCallback (cb);
    }

    void setMode (int m)
    {
        mode = m;
        eqTab.setToggleState (m == 0, juce::dontSendNotification);
        compTab.setToggleState (m == 1, juce::dontSendNotification);
        bothTab.setToggleState (m == 2, juce::dontSendNotification);
        roomTab.setToggleState (m == 3, juce::dontSendNotification);
        roomView.setVisible (m == 3);
        const bool showEq = (m == 0 || m == 2), showComp = (m == 1 || m == 2);
        for (auto* c : std::initializer_list<juce::Component*> { &eqView, &bFreq, &bGain, &bQ }) c->setVisible (showEq);
        for (auto* c : std::initializer_list<juce::Component*> { &compView, &thresh, &ratio, &knee, &attack, &release, &makeup, &mix }) c->setVisible (showComp);
        resized();
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto head = r.removeFromTop (34);
        eqTab.setBounds (head.removeFromLeft (70));    head.removeFromLeft (4);
        compTab.setBounds (head.removeFromLeft (80));  head.removeFromLeft (4);
        bothTab.setBounds (head.removeFromLeft (80));  head.removeFromLeft (4);
        roomTab.setBounds (head.removeFromLeft (80));  head.removeFromLeft (20);
        eqOnBtn.setBounds (head.removeFromLeft (84));  head.removeFromLeft (4);
        compOnBtn.setBounds (head.removeFromLeft (100));
        backBtn.setBounds (head.removeFromRight (140));
        r.removeFromTop (8);

        eqPanel = compPanel = {};
        roomView.setBounds (r);
        if (mode == 3) return;                                         // the room reverb fills the window
        const int knobH = 110;
        if (mode == 2)
        {
            auto left = r.removeFromLeft (r.getWidth() / 2 - 6);
            r.removeFromLeft (12);
            eqPanel = left.removeFromBottom (knobH);   left.removeFromBottom (8);
            compPanel = r.removeFromBottom (knobH);    r.removeFromBottom (8);
            eqView.setBounds (left);
            compView.setBounds (r);
        }
        else
        {
            auto panel = r.removeFromBottom (knobH);
            r.removeFromBottom (8);
            (mode == 0 ? eqPanel : compPanel) = panel;
            (mode == 0 ? static_cast<juce::Component&> (eqView) : static_cast<juce::Component&> (compView)).setBounds (r);
        }
        std::vector<LabeledKnob*> eqK { &bFreq, &bGain, &bQ }, compK { &thresh, &ratio, &knee, &attack, &release, &makeup, &mix };
        layoutKnobs (eqK, eqPanel, 70);
        layoutKnobs (compK, compPanel, 0);
    }

    void paint (juce::Graphics& g) override
    {
        for (auto* rect : { &eqPanel, &compPanel })
        {
            if (rect->isEmpty()) continue;
            g.setColour (Theme::panel);
            g.fillRoundedRectangle (rect->toFloat(), 8.f);
            g.setColour (Theme::grid.brighter (0.1f));
            g.drawRoundedRectangle (rect->toFloat().reduced (0.5f), 8.f, 1.f);
        }
        if (! eqPanel.isEmpty())                                       // which EQ band the three knobs are editing
        {
            g.setColour (Theme::dim);
            g.setFont (juce::Font (11.f, juce::Font::bold));
            g.drawText ("BAND", eqPanel.getX() + 12, eqPanel.getCentreY() - 24, 56, 14, juce::Justification::left);
            g.setColour (Theme::bandColours[selBand]);
            g.setFont (juce::Font (28.f, juce::Font::bold));
            g.drawText (juce::String (selBand + 1), eqPanel.getX() + 12, eqPanel.getCentreY() - 10, 56, 32, juce::Justification::left);
        }
    }

private:
    void selectBand (int b)
    {
        selBand = eqView.selected = b;
        bFreq.attach (proc.apvts, MfxID::freq (b));
        bGain.attach (proc.apvts, MfxID::gain (b));
        bQ.attach (proc.apvts, MfxID::q (b));
        repaint();
    }

    static void layoutKnobs (std::vector<LabeledKnob*>& knobs, juce::Rectangle<int> area, int leftPad)
    {
        if (area.isEmpty()) return;
        area.removeFromLeft (leftPad);
        area = area.reduced (4, 6);
        const int kw = juce::jmin (84, area.getWidth() / (int) knobs.size());
        area = area.withSizeKeepingCentre (kw * (int) knobs.size(), area.getHeight());
        for (auto* k : knobs) k->setBounds (area.removeFromLeft (kw));
    }

    BeatStationProcessor& proc;
    MasterEqView eqView;
    MasterCompView compView;
    RoomView roomView;
    juce::TextButton eqTab { "EQ" }, compTab { "COMP" }, bothTab { "BOTH" }, roomTab { "ROOM" }, eqOnBtn { "EQ ON" }, compOnBtn { "COMP ON" }, backBtn { "Back to pads" };
    LabeledKnob bFreq { "FREQ", " Hz", Theme::amber }, bGain { "GAIN", " dB", Theme::amber }, bQ { "Q", "", Theme::amber },
                thresh { "THRESH", " dB", Theme::amber }, ratio { "RATIO", ":1", Theme::amber }, knee { "KNEE", " dB", Theme::amber },
                attack { "ATTACK", " ms", Theme::amber }, release { "RELEASE", " ms", Theme::amber },
                makeup { "MAKEUP", " dB", Theme::green }, mix { "MIX", "", Theme::green };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> eqOnAtt, compOnAtt;
    juce::Rectangle<int> eqPanel, compPanel;
    int mode = 2, selBand = 2;
};
