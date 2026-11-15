#pragma once
#include "PluginProcessor.h"
#include "Theme.h"
#include "Widgets.h"
#include "MasterEqView.h"
#include "MasterCompView.h"
#include "RoomView.h"
#include "FxViews.h"
#include "FxPresets.h"

// The row of effect tabs across the top of the effects window. It IS the signal chain: sound goes through the tabs left to right.
// Click a tab to open that effect. Click its light to switch it on or off. Drag a tab sideways to move it in the chain.
class ChainStrip : public juce::Component, private juce::Timer
{
public:
    explicit ChainStrip (BeatStationProcessor& p) : proc (p), chain (p.fxChain) { startTimerHz (12); }

    std::function<void (int fx)> onSelect;
    int selected = fxEq;

    void paint (juce::Graphics& g) override
    {
        for (int slot = 0; slot < numFx; ++slot)
        {
            const int fx = chain.getOrder (slot);
            const auto r = chip (slot);
            const auto col = FxInfo::colours[fx];
            const bool on = chain.isOn (fx), sel = fx == selected;
            g.setColour (sel ? col.withAlpha (0.22f) : Theme::panel);
            g.fillRoundedRectangle (r, 6.f);
            g.setColour (sel ? col : Theme::grid.brighter (0.15f));
            g.drawRoundedRectangle (r.reduced (0.5f), 6.f, sel ? 1.8f : 1.f);
            const auto led = juce::Rectangle<float> (10.f, 10.f).withCentre ({ r.getX() + 12.f, r.getCentreY() });
            if (on) { g.setColour (col.withAlpha (0.35f)); g.fillEllipse (led.expanded (3.f)); }
            g.setColour (on ? col : Theme::tickOff);
            g.fillEllipse (led);
            g.setColour (on ? Theme::text : Theme::dim);
            g.setFont (juce::Font (11.5f, juce::Font::bold));
            g.drawText (FxInfo::names[fx], r.withTrimmedLeft (20.f).toNearestInt(), juce::Justification::centred);
            if (slot < numFx - 1)
            {
                g.setColour (Theme::dim);
                g.drawText (">", (int) r.getRight(), (int) r.getY(), gap, (int) r.getHeight(), juce::Justification::centred);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragSlot = slotAt (e.position.x);
        moved = false;
        if (dragSlot < 0) return;
        const int fx = chain.getOrder (dragSlot);
        if (e.position.x - chip (dragSlot).getX() < 22.f)               // the light: on / off
        {
            if (auto* prm = proc.apvts.getParameter (FxInfo::onIds[fx]))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (chain.isOn (fx) ? 0.f : 1.f);
                prm->endChangeGesture();
            }
            dragSlot = -1;
        }
        selected = fx;
        if (onSelect) onSelect (fx);
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override                 // drag sideways: the tab moves in the chain as you go
    {
        if (dragSlot < 0 || std::abs (e.getDistanceFromDragStartX()) < 8) return;
        const int over = juce::jlimit (0, numFx - 1, (int) (e.position.x / ((float) getWidth() / (float) numFx)));
        if (over != dragSlot) { chain.moveModule (dragSlot, over); dragSlot = over; moved = true; repaint(); }
    }

private:
    static constexpr int gap = 12;
    juce::Rectangle<float> chip (int slot) const
    {
        const float w = ((float) getWidth() - (float) (gap * (numFx - 1))) / (float) numFx;
        return { (float) slot * (w + (float) gap), 2.f, w, (float) getHeight() - 4.f };
    }
    int slotAt (float x) const
    {
        for (int s = 0; s < numFx; ++s) if (x >= chip (s).getX() && x <= chip (s).getRight()) return s;
        return -1;
    }
    void timerCallback() override { if (isShowing()) repaint(); }       // presets and automation can switch effects on and off

    BeatStationProcessor& proc;
    FxChain& chain;
    int dragSlot = -1;
    bool moved = false;
};

// The effects window: the chain across the top, one-click presets under it, and the selected effect's screen below.
class FxPanel : public juce::Component, private juce::Timer
{
public:
    std::function<void()> onBack;

    explicit FxPanel (BeatStationProcessor& p) : proc (p), strip (p), eqView (p.masterFx), compView (p.masterFx), roomView (p), modView (p)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &strip, &eqView, &compView, &roomView, &modView, &backBtn, &prevBtn, &nextBtn, &presetBtn,
                                                                  &bFreq, &bGain, &bQ, &thresh, &ratio, &knee, &attack, &release, &makeup, &mix })
            addAndMakeVisible (*c);
        backBtn.onClick = [this] { if (onBack) onBack(); };
        strip.onSelect = [this] (int fx) { select (fx); };
        prevBtn.onClick = [this] { step (-1); };
        nextBtn.onClick = [this] { step (1); };
        presetBtn.onClick = [this] { showPresetMenu(); };
        presetBtn.setColour (juce::TextButton::textColourOffId, Theme::amber);

        thresh.attach (p.apvts, MfxID::thresh);   ratio.attach (p.apvts, MfxID::ratio);     knee.attach (p.apvts, MfxID::knee);
        attack.attach (p.apvts, MfxID::attack);   release.attach (p.apvts, MfxID::release); makeup.attach (p.apvts, MfxID::makeup);
        mix.attach (p.apvts, MfxID::mix);
        for (auto* k : { &thresh, &ratio, &knee, &attack, &release, &makeup, &mix }) k->setAccent (FxInfo::colours[fxComp]);
        eqView.onSelect = [this] (int b) { selectBand (b); };
        selectBand (2);
        select (fxEq);
        startTimerHz (6);
    }

    void setLearnCallback (std::function<void (const juce::String& target)> cb)
    {
        for (auto* k : { &bFreq, &bGain, &bQ, &thresh, &ratio, &knee, &attack, &release, &makeup, &mix })
            k->onLearnMenu = [cb] (const juce::String& id) { cb ("param:" + id); };
        roomView.setLearnCallback (cb);
        modView.learn = cb;
        if (current != fxEq && current != fxComp && current != fxRoom) modView.setModule (current);
    }

    void select (int fx)
    {
        current = fx;
        strip.selected = fx;
        const bool eq = fx == fxEq, comp = fx == fxComp, room = fx == fxRoom, other = ! eq && ! comp && ! room;
        for (auto* c : std::initializer_list<juce::Component*> { &eqView, &bFreq, &bGain, &bQ }) c->setVisible (eq);
        for (auto* c : std::initializer_list<juce::Component*> { &compView, &thresh, &ratio, &knee, &attack, &release, &makeup, &mix }) c->setVisible (comp);
        roomView.setVisible (room);
        modView.setVisible (other);
        if (other) modView.setModule (fx);
        resized();
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto head = r.removeFromTop (38);
        backBtn.setBounds (head.removeFromRight (120).reduced (0, 2));
        head.removeFromRight (10);
        strip.setBounds (head);
        r.removeFromTop (6);
        auto bar = r.removeFromTop (28);
        bar.removeFromLeft (62);                                        // "PRESET" (painted)
        prevBtn.setBounds (bar.removeFromLeft (30));   bar.removeFromLeft (4);
        presetBtn.setBounds (bar.removeFromLeft (340)); bar.removeFromLeft (4);
        nextBtn.setBounds (bar.removeFromLeft (30));
        hintRect = bar.withTrimmedLeft (14);
        r.removeFromTop (8);

        roomView.setBounds (r);
        modView.setBounds (r);
        knobPanel = r.removeFromBottom (110);
        r.removeFromBottom (8);
        eqView.setBounds (r);
        compView.setBounds (r);
        std::vector<LabeledKnob*> eqK { &bFreq, &bGain, &bQ }, compK { &thresh, &ratio, &knee, &attack, &release, &makeup, &mix };
        layoutKnobs (eqK, knobPanel, 70);
        layoutKnobs (compK, knobPanel, 0);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::dim);
        g.setFont (juce::Font (11.f, juce::Font::bold));
        g.drawText ("PRESET", 0, presetBtn.getY(), 58, presetBtn.getHeight(), juce::Justification::centredLeft);
        g.setFont (11.f);
        g.drawFittedText ("Sound goes through the tabs left to right. Drag a tab to move it. Click its light to switch it on or off.",
                          hintRect, juce::Justification::centredLeft, 2);
        if (current != fxEq && current != fxComp) return;
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (knobPanel.toFloat(), 8.f);
        g.setColour (Theme::grid.brighter (0.1f));
        g.drawRoundedRectangle (knobPanel.toFloat().reduced (0.5f), 8.f, 1.f);
        if (current == fxEq)                                            // which EQ band the three knobs are editing
        {
            g.setColour (Theme::dim);
            g.setFont (juce::Font (11.f, juce::Font::bold));
            g.drawText ("BAND", knobPanel.getX() + 12, knobPanel.getCentreY() - 24, 56, 14, juce::Justification::left);
            g.setColour (Theme::bandColours[selBand]);
            g.setFont (juce::Font (28.f, juce::Font::bold));
            g.drawText (juce::String (selBand + 1), knobPanel.getX() + 12, knobPanel.getCentreY() - 10, 56, 32, juce::Justification::left);
        }
    }

private:
    void selectBand (int b)
    {
        selBand = eqView.selected = b;
        bFreq.attach (proc.apvts, MfxID::freq (b));
        bGain.attach (proc.apvts, MfxID::gain (b));
        bQ.attach (proc.apvts, MfxID::q (b));
        for (auto* k : { &bFreq, &bGain, &bQ }) k->setAccent (Theme::bandColours[b]);   // the knob lights take the colour of the band
        repaint();
    }

    int currentPreset() const { return (int) proc.apvts.state.getProperty ("fxPreset", -1); }

    void load (int index)
    {
        applyFxPreset (proc.apvts, proc.fxChain, proc.roomFx, index);
        shownPreset = -2;                                               // refresh the button text on the next tick
        for (int slot = 0; slot < numFx; ++slot)                        // open the first effect the preset uses
            if (proc.fxChain.isOn (proc.fxChain.getOrder (slot))) { select (proc.fxChain.getOrder (slot)); break; }
        strip.repaint();
    }

    void step (int dir)
    {
        const int n = (int) fxPresets().size();
        load (((juce::jmax (-1, currentPreset()) + dir) % n + n) % n);
    }

    void showPresetMenu()
    {
        juce::PopupMenu m;
        const auto& all = fxPresets();
        juce::StringArray cats;
        for (const auto& p : all) cats.addIfNotAlreadyThere (p.category);
        const int cur = currentPreset();
        for (const auto& cat : cats)
        {
            juce::PopupMenu sub;
            for (int i = 0; i < (int) all.size(); ++i)
                if (cat == all[(size_t) i].category) sub.addItem (i + 1, all[(size_t) i].name, true, i == cur);
            m.addSubMenu (cat, sub);
        }
        juce::Component::SafePointer<FxPanel> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [safe] (int r) { if (safe != nullptr && r > 0) safe->load (r - 1); });
    }

    void timerCallback() override
    {
        const int cur = currentPreset();
        if (cur == shownPreset) return;
        shownPreset = cur;
        const auto& all = fxPresets();
        presetBtn.setButtonText (cur >= 0 && cur < (int) all.size() ? juce::String (all[(size_t) cur].name) : juce::String ("Choose a preset..."));
    }

    static void layoutKnobs (std::vector<LabeledKnob*>& knobs, juce::Rectangle<int> area, int leftPad)
    {
        if (area.isEmpty()) return;
        area.removeFromLeft (leftPad);
        area = area.reduced (4, 6);
        const int kw = juce::jmin (110, area.getWidth() / (int) knobs.size());
        area = area.withSizeKeepingCentre (kw * (int) knobs.size(), area.getHeight());
        for (auto* k : knobs) k->setBounds (area.removeFromLeft (kw));
    }

    BeatStationProcessor& proc;
    ChainStrip strip;
    MasterEqView eqView;
    MasterCompView compView;
    RoomView roomView;
    FxModuleView modView;
    juce::TextButton backBtn { "Back to pads" }, prevBtn { "<" }, nextBtn { ">" }, presetBtn { "Choose a preset..." };
    LabeledKnob bFreq { "FREQ", " Hz", Theme::amber }, bGain { "GAIN", " dB", Theme::amber }, bQ { "Q", "", Theme::amber },
                thresh { "THRESH", " dB", Theme::amber }, ratio { "RATIO", ":1", Theme::amber }, knee { "KNEE", " dB", Theme::amber },
                attack { "ATTACK", " ms", Theme::amber }, release { "RELEASE", " ms", Theme::amber },
                makeup { "MAKEUP", " dB", Theme::green }, mix { "MIX", "", Theme::green };
    juce::Rectangle<int> knobPanel, hintRect;
    int current = fxEq, selBand = 2, shownPreset = -2;
};
