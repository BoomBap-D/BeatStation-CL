#pragma once
#include "PluginProcessor.h"
#include "Theme.h"

// A fader with a lit tick trail. Vertical + bipolar = the pitch fader (top is minus, like a real deck).
// Horizontal + unipolar = the cut fader.
class DeckFader : public juce::Component
{
public:
    DeckFader (bool vert, bool bip, float def) : vertical (vert), bipolar (bip), defaultValue (def), value (def) {}

    std::function<void (float)> onChange;

    float getValue() const { return value; }
    void setValue (float v, bool notify)
    {
        v = juce::jlimit (bipolar ? -1.f : 0.f, 1.f, v);
        if (bipolar && std::abs (v) < 0.02f) v = 0.f;
        value = v;
        if (notify && onChange) onChange (value);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const float pad = 14.f;
        const int n = bipolar ? 21 : 11;
        const float a0 = vertical ? b.getY() + pad : b.getX() + pad, a1 = vertical ? b.getBottom() - pad : b.getRight() - pad;
        auto axis = [&] (float f) { return a0 + (a1 - a0) * f; };

        g.setColour (Theme::graphBg);
        if (vertical) g.fillRoundedRectangle (b.getCentreX() - 3.f, a0, 6.f, a1 - a0, 3.f);
        else          g.fillRoundedRectangle (a0, b.getCentreY() - 3.f, a1 - a0, 6.f, 3.f);

        for (int i = 0; i < n; ++i)
        {
            const float f = (float) i / (float) (n - 1);
            const float v = bipolar ? f * 2.f - 1.f : f;
            bool lit;
            if (bipolar) lit = value >= 0.f ? (v >= 0.f && v <= value + 1e-4f) : (v <= 0.f && v >= value - 1e-4f);
            else         lit = v <= value + 1e-4f;
            const float len = (i % 5 == 0) ? 11.f : 6.f;
            g.setColour (lit ? Theme::amber : Theme::tickOff);
            const float p = axis (f);
            if (vertical)
            {
                g.drawLine (b.getCentreX() + 9.f, p, b.getCentreX() + 9.f + len, p, 1.6f);
                g.drawLine (b.getCentreX() - 9.f, p, b.getCentreX() - 9.f - len, p, 1.6f);
            }
            else
            {
                g.drawLine (p, b.getCentreY() - 9.f, p, b.getCentreY() - 9.f - len, 1.6f);
                g.drawLine (p, b.getCentreY() + 9.f, p, b.getCentreY() + 9.f + len, 1.6f);
            }
        }

        const float tf = bipolar ? (value + 1.f) * 0.5f : value;
        const auto thumb = vertical ? juce::Rectangle<float> (30.f, 14.f).withCentre ({ b.getCentreX(), axis (tf) })
                                    : juce::Rectangle<float> (14.f, 30.f).withCentre ({ axis (tf), b.getCentreY() });
        g.setColour (Theme::panel.brighter (0.25f));
        g.fillRoundedRectangle (thumb, 3.f);
        g.setColour (Theme::dim);
        g.drawRoundedRectangle (thumb.reduced (0.5f), 3.f, 1.f);
        g.setColour (Theme::amber);
        if (vertical) g.drawLine (thumb.getX() + 3.f, thumb.getCentreY(), thumb.getRight() - 3.f, thumb.getCentreY(), 2.f);
        else          g.drawLine (thumb.getCentreX(), thumb.getY() + 3.f, thumb.getCentreX(), thumb.getBottom() - 3.f, 2.f);
    }

    void mouseDown (const juce::MouseEvent& e) override { fromMouse (e); }
    void mouseDrag (const juce::MouseEvent& e) override { fromMouse (e); }
    void mouseDoubleClick (const juce::MouseEvent&) override { setValue (defaultValue, true); }

private:
    void fromMouse (const juce::MouseEvent& e)
    {
        const float pad = 14.f;
        const float a0 = vertical ? pad : pad, len = (vertical ? (float) getHeight() : (float) getWidth()) - 2.f * pad;
        const float p = vertical ? e.position.y : e.position.x;
        const float f = juce::jlimit (0.f, 1.f, (p - a0) / juce::jmax (1.f, len));
        setValue (bipolar ? f * 2.f - 1.f : f, true);
    }

    bool vertical, bipolar;
    float defaultValue, value;
};

// One turntable: platter you can grab and scratch, motor, 33/45, pitch fader, reverse, loop, hot cues, cut fader.
// The whole loaded sample is on the record. Hot cues are the chops of the pad it came from.
class TurntablePanel : public juce::Component, public juce::DragAndDropTarget, private juce::Timer
{
public:
    explicit TurntablePanel (BeatStationProcessor& p) : proc (p), pitchFader (true, true, 0.f), cutFader (false, false, 1.f)
    {
        for (auto* b : { &startBtn, &rpm33, &rpm45, &revBtn, &loopBtn, &rangeBtn, &loadPadBtn, &loadFileBtn, &holdBtn })
            addAndMakeVisible (*b);
        addAndMakeVisible (pitchFader);
        addAndMakeVisible (cutFader);

        startBtn.onClick = [this] { proc.deckMotor = ! proc.deckMotor.load(); syncButtons(); };
        rpm33.onClick = [this] { proc.deck45 = false; syncButtons(); };
        rpm45.onClick = [this] { proc.deck45 = true;  syncButtons(); };
        revBtn.onClick = [this] { proc.deckReverse = ! proc.deckReverse.load(); syncButtons(); };
        loopBtn.onClick = [this] { proc.deckLoop = ! proc.deckLoop.load(); syncButtons(); };
        rangeBtn.onClick = [this] { rangeIdx = (rangeIdx + 1) % 3; applyPitch(); syncButtons(); };
        pitchFader.onChange = [this] (float) { applyPitch(); };
        cutFader.onChange = [this] (float) { applyCut(); };
        holdBtn.onStateChange = [this] { holdDown = holdBtn.isDown(); applyCut(); };
        loadPadBtn.onClick = [this] { proc.deckLoadPad (selPad); lastCueSig = -1; repaint(); };
        loadFileBtn.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Choose a sample for the deck", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      if (fc.getResult().existsAsFile()) proc.deckLoadFile (fc.getResult());
                                      repaint();
                                  });
        };

        for (int i = 0; i < 8; ++i)
        {
            cues[(size_t) i] = std::make_unique<CueButton> (*this, i);
            addAndMakeVisible (*cues[(size_t) i]);
        }
        startBtn.setColour (juce::TextButton::textColourOffId, Theme::amber);
        holdBtn.setColour (juce::TextButton::textColourOffId, Theme::red);
        loopBtn.setToggleState (true, juce::dontSendNotification);
        applyPitch();
        syncButtons();
        startTimerHz (30);
    }

    void setSelectedPad (int i)
    {
        selPad = i;
        loadPadBtn.setButtonText ("Load pad " + juce::String (i + 1));
    }

    void resized() override
    {
        startBtn.setBounds (14, 322, 96, 44);
        rpm33.setBounds (14, 14, 46, 28);
        rpm45.setBounds (64, 14, 46, 28);
        revBtn.setBounds (428, 322, 84, 28);
        loopBtn.setBounds (428, 354 - 2, 84, 22);
        pitchFader.setBounds (516, 46, 58, 252);
        rangeBtn.setBounds (516, 306, 58, 28);

        loadPadBtn.setBounds (596, 28, 120, 28);
        loadFileBtn.setBounds (722, 28, 84, 28);
        waveRect = { 596, 66, 404, 96 };
        for (int i = 0; i < 8; ++i)
            cues[(size_t) i]->setBounds (596 + (i % 4) * 102, 194 + (i / 4) * 42, 94, 34);
        cutFader.setBounds (596, 280, 290, 40);
        holdBtn.setBounds (896, 284, 104, 32);
    }

    void paint (juce::Graphics& g) override
    {
        // ---- deck body ----
        const auto body = juce::Rectangle<float> (0.f, 0.f, 580.f, 380.f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff272a31), 0.f, 0.f, juce::Colour (0xff181a1e), 0.f, 380.f, false));
        g.fillRoundedRectangle (body, 14.f);
        g.setColour (Theme::grid.brighter (0.15f));
        g.drawRoundedRectangle (body.reduced (0.5f), 14.f, 1.f);
        g.setColour (Theme::amber.withAlpha (0.5f));
        g.fillRect (18.f, 1.f, 544.f, 2.f);
        for (auto c : { juce::Point<float> (12.f, 12.f), { 568.f, 12.f }, { 12.f, 368.f }, { 568.f, 368.f } })
        {
            g.setColour (Theme::graphBg);
            g.fillEllipse (c.x - 4.f, c.y - 4.f, 8.f, 8.f);
            g.setColour (Theme::tickOff);
            g.drawLine (c.x - 3.f, c.y - 3.f, c.x + 3.f, c.y + 3.f, 1.f);
        }

        const auto sample = proc.getDeckSample();
        const double pos = proc.deckPos.load();
        const double secs = sample != nullptr ? pos / sample->sampleRate : 0.0;
        const float angle = (float) (juce::MathConstants<double>::twoPi * secs / 1.8);
        const float cx = platC.x, cy = platC.y;

        // ---- platter ----
        g.setColour (juce::Colour (0xff0e0f12));
        g.fillEllipse (cx - platR - 6.f, cy - platR - 6.f, (platR + 6.f) * 2.f, (platR + 6.f) * 2.f);
        g.setColour (juce::Colour (0xff3a3e47));
        g.fillEllipse (cx - platR, cy - platR, platR * 2.f, platR * 2.f);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.addTransform (juce::AffineTransform::rotation (angle, cx, cy));

            // strobe-style dots around the rim
            for (int i = 0; i < 72; ++i)
            {
                const float a = juce::MathConstants<float>::twoPi * (float) i / 72.f;
                const float r = platR - 8.f;
                g.setColour (i % 6 == 0 ? Theme::amber : Theme::dim.withAlpha (0.55f));
                g.fillEllipse (cx + std::cos (a) * r - 1.8f, cy + std::sin (a) * r - 1.8f, 3.6f, 3.6f);
            }

            // record
            g.setColour (juce::Colour (0xff0b0b0d));
            g.fillEllipse (cx - recR, cy - recR, recR * 2.f, recR * 2.f);
            for (float r = 58.f; r < recR - 2.f; r += 3.5f)
            {
                g.setColour (juce::Colours::white.withAlpha ((((int) (r / 3.5f)) % 7 == 0) ? 0.10f : 0.035f));
                g.drawEllipse (cx - r, cy - r, r * 2.f, r * 2.f, 0.7f);
            }

            // label
            g.setColour (Theme::amber);
            g.fillEllipse (cx - 52.f, cy - 52.f, 104.f, 104.f);
            g.setColour (juce::Colour (0xff17191d));
            g.setFont (juce::Font (9.f, juce::Font::bold));
            g.drawText ("BASEMENT", (int) cx - 40, (int) cy - 24, 80, 12, juce::Justification::centred);
            g.drawText ("INSTRUMENTS", (int) cx - 40, (int) cy - 12, 80, 12, juce::Justification::centred);
            g.setFont (juce::Font (8.f, juce::Font::plain));
            g.drawText (sample != nullptr ? sample->name : juce::String ("NO RECORD"), (int) cx - 40, (int) cy + 8, 80, 11, juce::Justification::centred);
            g.fillRect (cx - 1.f, cy + 30.f, 2.f, 14.f);                 // a mark so you can see it turn
        }

        // light reflection, does not rotate
        {
            juce::Path w1, w2;
            w1.addPieSegment (cx - recR, cy - recR, recR * 2.f, recR * 2.f, -1.25f, -0.55f, 0.f);
            w2.addPieSegment (cx - recR, cy - recR, recR * 2.f, recR * 2.f, 1.89f, 2.59f, 0.f);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillPath (w1);
            g.fillPath (w2);
        }
        g.setColour (juce::Colour (0xffcfd3da));
        g.fillEllipse (cx - 4.f, cy - 4.f, 8.f, 8.f);

        // ---- tonearm ----
        {
            const juce::Point<float> P (505.f, 52.f), C (cx, cy);
            const float L = 290.f;
            const double len = sample != nullptr ? (double) sample->data.getNumSamples() : 1.0;
            const float prog = sample != nullptr ? (float) juce::jlimit (0.0, 1.0, pos / len) : 0.f;
            const float r = sample != nullptr ? juce::jmap (prog, 0.f, 1.f, recR - 6.f, 62.f) : platR + 24.f;
            const float d = P.getDistanceFrom (C);
            const float x = (d * d + L * L - r * r) / (2.f * d);
            const float h = std::sqrt (juce::jmax (0.f, L * L - x * x));
            const auto u = (C - P) / d;
            const juce::Point<float> nrm (-u.y, u.x);
            auto S = P + u * x + nrm * h;
            if ((P + u * x - nrm * h).y > S.y) S = P + u * x - nrm * h;
            const auto dir = (S - P) / juce::jmax (1.f, S.getDistanceFrom (P));

            g.setColour (juce::Colour (0xff111316));
            g.fillEllipse (P.x - 26.f, P.y - 26.f, 52.f, 52.f);
            g.setColour (Theme::tickOff);
            g.drawEllipse (P.x - 26.f, P.y - 26.f, 52.f, 52.f, 2.f);
            g.setColour (juce::Colour (0xff8d929c));
            g.drawLine (P.x - dir.x * 44.f, P.y - dir.y * 44.f, S.x, S.y, 5.f);            // arm and counterweight side
            g.setColour (juce::Colour (0xff555a63));
            g.fillEllipse (P.x - dir.x * 44.f - 9.f, P.y - dir.y * 44.f - 9.f, 18.f, 18.f);
            g.setColour (juce::Colour (0xffb7bcc5));
            g.fillEllipse (P.x - 7.f, P.y - 7.f, 14.f, 14.f);
            const auto sideN = juce::Point<float> (-dir.y, dir.x);
            juce::Path head;
            head.startNewSubPath (S - dir * 24.f + sideN * 6.f);
            head.lineTo (S + sideN * 7.f);
            head.lineTo (S - sideN * 7.f);
            head.lineTo (S - dir * 24.f - sideN * 6.f);
            head.closeSubPath();
            g.setColour (juce::Colour (0xff2c2f36));
            g.fillPath (head);
            g.setColour (Theme::amber);
            g.fillEllipse (S.x - 3.f, S.y - 3.f, 6.f, 6.f);
        }

        // ---- labels on the deck ----
        g.setFont (juce::Font (10.f, juce::Font::bold));
        g.setColour (Theme::dim);
        g.drawText ("PITCH", 516, 24, 58, 14, juce::Justification::centred);
        g.drawText ("-", 516, 34, 14, 14, juce::Justification::centred);
        g.drawText ("+", 516, 284, 14, 14, juce::Justification::centred);
        g.setFont (juce::Font (13.f, juce::Font::bold));
        g.setColour (Theme::amber);
        const float pc = pitchFader.getValue() * ranges[rangeIdx] * 100.f;
        g.drawText ((pc >= 0.f ? "+" : "") + juce::String (pc, 1) + "%", 506, 338, 76, 16, juce::Justification::centred);
        // motor lamp
        g.setColour (proc.deckMotor.load() ? Theme::amber : Theme::tickOff);
        g.fillEllipse (118.f, 336.f, 10.f, 10.f);

        // ---- right side ----
        g.setColour (Theme::dim);
        g.setFont (juce::Font (11.f, juce::Font::bold));
        g.drawText ("DECK 1", 596, 4, 60, 18, juce::Justification::centredLeft);
        g.setColour (Theme::text);
        g.setFont (13.f);
        g.drawFittedText (sample != nullptr ? sample->name : "empty - load a pad or a file, or drag a sample here", 654, 4, 346, 18, juce::Justification::centredLeft, 1);
        paintWave (g, sample, pos);

        g.setColour (Theme::dim);
        g.setFont (juce::Font (11.f, juce::Font::bold));
        g.drawText ("HOT CUES   (the chops of the pad it came from)", 596, 172, 404, 16, juce::Justification::centredLeft);
        g.drawText ("CUT FADER", 596, 262, 120, 16, juce::Justification::centredLeft);
        g.setFont (11.f);
        g.drawFittedText ("Grab the platter and move it to scratch. Click the waveform to jump. Click a cue to jump to it (right-click to set or clear).\n"
                          "Hold CUT to silence the deck. The whole sample is always on the record, chopped or not.",
                          596, 328, 404, 46, juce::Justification::topLeft, 3);
    }

    // ---- platter and waveform mouse handling ----
    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.position;
        if (waveRect.toFloat().contains (p)) { seekFromX (p.x); draggingWave = true; return; }
        if (p.getDistanceFrom (platC) <= platR && p.getDistanceFrom (platC) > 12.f)
        {
            touching = true;
            smoothRate = 0.f;
            proc.deckTouchRate = 0.f;
            proc.deckTouch = true;
            lastAngle = angleOf (p);
            lastT = lastMoveT = juce::Time::getMillisecondCounterHiRes();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (draggingWave) { seekFromX (e.position.x); return; }
        if (! touching || e.position.getDistanceFrom (platC) < 14.f) return;
        const float a = angleOf (e.position);
        float d = a - lastAngle;
        while (d > juce::MathConstants<float>::pi)  d -= juce::MathConstants<float>::twoPi;
        while (d < -juce::MathConstants<float>::pi) d += juce::MathConstants<float>::twoPi;
        const double t = juce::Time::getMillisecondCounterHiRes();
        const float dt = (float) juce::jmax (1.0, t - lastT) * 0.001f;
        const float rate = juce::jlimit (-14.f, 14.f, d / juce::MathConstants<float>::twoPi / dt * 1.8f);   // 1 revolution = 1.8 s of audio
        smoothRate = 0.5f * smoothRate + 0.5f * rate;
        proc.deckTouchRate = smoothRate;
        lastAngle = a;
        lastT = lastMoveT = t;
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (touching) { touching = false; proc.deckTouch = false; proc.deckTouchRate = 0.f; smoothRate = 0.f; }
        draggingWave = false;
    }

    // ---- drop a sample on the deck: from the browser or from a pad ----
    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        const auto s = d.description.toString();
        return s.startsWith ("sample:") || s.startsWith ("padcopy:");
    }
    void itemDropped (const SourceDetails& d) override
    {
        const auto s = d.description.toString();
        if (s.startsWith ("sample:")) proc.deckLoadFile (juce::File (s.fromFirstOccurrenceOf ("sample:", false, false)));
        else                          proc.deckLoadPad (s.fromFirstOccurrenceOf (":", false, false).getIntValue());
        repaint();
    }

private:
    class CueButton : public juce::Component
    {
    public:
        CueButton (TurntablePanel& o, int i) : owner (o), index (i) {}
        void paint (juce::Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat().reduced (0.5f);
            const double c = owner.proc.getDeckCue (index);
            const bool set = c >= 0.0;
            g.setColour (set ? Theme::amber.withAlpha (isMouseOver() ? 0.32f : 0.2f) : Theme::panel);
            g.fillRoundedRectangle (b, 6.f);
            g.setColour (set ? Theme::amber : Theme::grid.brighter (0.1f));
            g.drawRoundedRectangle (b, 6.f, 1.2f);
            g.setColour (set ? Theme::text : Theme::dim);
            g.setFont (juce::Font (13.f, juce::Font::bold));
            g.drawText ("CUE " + juce::String (index + 1), getLocalBounds().removeFromTop (20).withTrimmedTop (2), juce::Justification::centred);
            g.setFont (10.f);
            g.setColour (Theme::dim);
            const auto s = owner.proc.getDeckSample();
            const juce::String t = (set && s != nullptr) ? juce::String (c / s->sampleRate, 2) + " s" : juce::String ("empty");
            g.drawText (t, getLocalBounds().removeFromBottom (14), juce::Justification::centred);
        }
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu()) { owner.cueMenu (index); return; }
            const double c = owner.proc.getDeckCue (index);
            if (c >= 0.0)
            {
                owner.proc.deckSeek = c;                       // jump, and start the motor if the deck is stopped
                if (! owner.proc.deckMotor.load()) { owner.proc.deckMotor = true; owner.syncButtons(); }
            }
            else owner.proc.setDeckCue (index, owner.proc.deckPos.load());
            repaint();
        }
    private:
        TurntablePanel& owner;
        int index;
    };

    void cueMenu (int i)
    {
        juce::PopupMenu m;
        m.addItem (1, "Set cue " + juce::String (i + 1) + " here");
        m.addItem (2, "Clear cue " + juce::String (i + 1), proc.getDeckCue (i) >= 0.0);
        juce::Component::SafePointer<TurntablePanel> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe, i] (int r)
        {
            if (safe == nullptr) return;
            if (r == 1) safe->proc.setDeckCue (i, safe->proc.deckPos.load());
            else if (r == 2) safe->proc.setDeckCue (i, -1.0);
            safe->repaint();
        });
    }

    void syncButtons()
    {
        const bool m = proc.deckMotor.load();
        startBtn.setButtonText (m ? "STOP" : "START");
        startBtn.setToggleState (m, juce::dontSendNotification);
        rpm33.setToggleState (! proc.deck45.load(), juce::dontSendNotification);
        rpm45.setToggleState (proc.deck45.load(), juce::dontSendNotification);
        revBtn.setToggleState (proc.deckReverse.load(), juce::dontSendNotification);
        loopBtn.setToggleState (proc.deckLoop.load(), juce::dontSendNotification);
        rangeBtn.setButtonText (juce::String::fromUTF8 ("\xc2\xb1") + juce::String ((int) (ranges[rangeIdx] * 100.f)) + "%");
        repaint();
    }
    void applyPitch() { proc.deckPitch = pitchFader.getValue() * ranges[rangeIdx]; repaint(); }
    void applyCut() { proc.deckCut = holdDown ? 0.f : cutFader.getValue(); }

    static float angleOf (juce::Point<float> p) { return std::atan2 (p.y - platC.y, p.x - platC.x); }

    void seekFromX (float x)
    {
        const auto s = proc.getDeckSample();
        if (s == nullptr) return;
        const float f = juce::jlimit (0.f, 1.f, (x - (float) waveRect.getX()) / (float) waveRect.getWidth());
        proc.deckSeek = (double) f * (double) s->data.getNumSamples();
    }

    void rebuildPeaks (const std::shared_ptr<const Sample>& s)
    {
        const int w = waveRect.getWidth();
        if (s == peaksFor && (int) peaks.size() == w) return;
        peaksFor = s;
        peaks.assign ((size_t) juce::jmax (0, w), { 0.f, 0.f });
        if (s == nullptr || w <= 0) return;
        const int n = s->data.getNumSamples();
        const float* a = s->data.getReadPointer (0);
        const float* b = s->data.getReadPointer (1);
        for (int x = 0; x < w; ++x)
        {
            const int i0 = (int) ((juce::int64) n * x / w), i1 = juce::jmax (i0 + 1, (int) ((juce::int64) n * (x + 1) / w));
            const int stride = juce::jmax (1, (i1 - i0) / 64);
            float lo = 0.f, hi = 0.f;
            for (int i = i0; i < i1 && i < n; i += stride) { const float v = 0.5f * (a[i] + b[i]); lo = juce::jmin (lo, v); hi = juce::jmax (hi, v); }
            peaks[(size_t) x] = { lo, hi };
        }
    }

    void paintWave (juce::Graphics& g, const std::shared_ptr<const Sample>& s, double pos)
    {
        const auto r = waveRect.toFloat();
        g.setColour (Theme::graphBg);
        g.fillRoundedRectangle (r, 6.f);
        g.setColour (Theme::grid.brighter (0.1f));
        g.drawRoundedRectangle (r.reduced (0.5f), 6.f, 1.f);
        if (s == nullptr) return;

        const float mid = r.getCentreY(), half = r.getHeight() * 0.45f;
        const float playX = r.getX() + (float) (pos / (double) s->data.getNumSamples()) * r.getWidth();
        for (size_t x = 0; x < peaks.size(); ++x)
        {
            const float px = r.getX() + (float) x;
            g.setColour (px <= playX ? Theme::amber : Theme::amber.withAlpha (0.45f));
            g.drawLine (px, mid - peaks[x].second * half, px, mid - peaks[x].first * half + 1.f, 1.f);
        }
        g.setColour (Theme::text);
        g.drawLine (playX, r.getY() + 2.f, playX, r.getBottom() - 2.f, 1.6f);

        for (int i = 0; i < 8; ++i)
        {
            const double c = proc.getDeckCue (i);
            if (c < 0.0) continue;
            const float x = r.getX() + (float) (c / (double) s->data.getNumSamples()) * r.getWidth();
            g.setColour (Theme::green);
            g.drawLine (x, r.getY() + 2.f, x, r.getBottom() - 2.f, 1.f);
            g.fillRect (x, r.getY() + 2.f, 12.f, 12.f);
            g.setColour (juce::Colour (0xff111316));
            g.setFont (juce::Font (10.f, juce::Font::bold));
            g.drawText (juce::String (i + 1), (int) x, (int) r.getY() + 2, 12, 12, juce::Justification::centred);
        }
    }

    void timerCallback() override
    {
        if (touching && juce::Time::getMillisecondCounterHiRes() - lastMoveT > 40.0)   // hand held still on the record
        {
            smoothRate = 0.f;
            proc.deckTouchRate = 0.f;
        }
        rebuildPeaks (proc.getDeckSample());
        repaint();
    }

    BeatStationProcessor& proc;
    DeckFader pitchFader, cutFader;
    std::array<std::unique_ptr<CueButton>, 8> cues;
    juce::TextButton startBtn { "START" }, rpm33 { "33" }, rpm45 { "45" }, revBtn { "REVERSE" }, loopBtn { "LOOP" }, rangeBtn { "8%" },
                     loadPadBtn { "Load pad 1" }, loadFileBtn { "Load..." }, holdBtn { "HOLD CUT" };
    std::unique_ptr<juce::FileChooser> chooser;

    static constexpr float ranges[3] = { 0.08f, 0.16f, 0.50f };
    static constexpr float platR = 170.f, recR = 152.f;
    static inline const juce::Point<float> platC { 245.f, 190.f };
    juce::Rectangle<int> waveRect { 596, 66, 404, 96 };
    std::shared_ptr<const Sample> peaksFor;
    std::vector<std::pair<float, float>> peaks;

    int selPad = 0, rangeIdx = 0, lastCueSig = -1;
    bool touching = false, draggingWave = false, holdDown = false;
    float lastAngle = 0.f, smoothRate = 0.f;
    double lastT = 0.0, lastMoveT = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TurntablePanel)
};
