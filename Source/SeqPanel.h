#pragma once
#include "PluginProcessor.h"
#include "Theme.h"

// 4 tracks x 16 steps. Click or drag to paint steps on and off. The cream outline is the playhead.
class StepGrid : public juce::Component, public juce::DragAndDropTarget
{
public:
    explicit StepGrid (BeatStationProcessor& p) : proc (p) {}
    int page = 0;
    std::function<void (int track, const juce::String& description)> onDropped;

    bool isInterestedInDragSource (const SourceDetails& d) override { const auto s = d.description.toString(); return s.startsWith ("sample:") || s.startsWith ("padcopy:"); }
    void itemDropped (const SourceDetails& d) override
    {
        for (int t = 0; t < BeatStationProcessor::numTracks; ++t)
            if (cell (t, 0).withWidth ((float) getWidth()).expanded (0.f, 3.f).contains (d.localPosition.toFloat()))
            {
                if (onDropped) onDropped (t, d.description.toString());
                return;
            }
    }                                                   // 0 = steps 1-16, 1 = steps 17-32

    static juce::Colour trackColour (int t)
    {
        static const juce::Colour c[4] = { Theme::red, Theme::amber, Theme::cream, Theme::green };
        return c[t];
    }

    juce::Rectangle<float> cell (int t, int s) const
    {
        const float gap = 4.f, rowGap = 6.f;
        const float w = ((float) getWidth() - 15.f * gap) / 16.f, h = ((float) getHeight() - 3.f * rowGap) / 4.f;
        return { (float) s * (w + gap), (float) t * (h + rowGap), w, h };
    }

    void paint (juce::Graphics& g) override
    {
        const int cur = proc.currentStep.load(), visible = juce::jlimit (0, 16, proc.getPatternLength() - page * 16);
        for (int t = 0; t < BeatStationProcessor::numTracks; ++t)
            for (int s = 0; s < visible; ++s)
            {
                const auto r = cell (t, s);
                const int abs = page * 16 + s;
                g.setColour (proc.getStep (t, abs) ? trackColour (t) : ((s / 4) % 2 == 0 ? juce::Colour (0xff2f333c) : juce::Colour (0xff262a31)));
                g.fillRoundedRectangle (r, 5.f);
                g.setColour (juce::Colour (0xff3a3f4a));
                g.drawRoundedRectangle (r.reduced (0.5f), 5.f, 1.f);
                if (abs == cur)
                {
                    g.setColour (Theme::cream);
                    g.drawRoundedRectangle (r.reduced (1.f), 5.f, 2.f);
                }
            }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        int t, s;
        if (hit (e.position, t, s)) { paintValue = ! proc.getStep (t, page * 16 + s); proc.setStep (t, page * 16 + s, paintValue); repaint(); }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        int t, s;
        if (hit (e.position, t, s)) { proc.setStep (t, page * 16 + s, paintValue); repaint(); }
    }

private:
    bool hit (juce::Point<float> p, int& t, int& s) const
    {
        const int visible = juce::jlimit (0, 16, proc.getPatternLength() - page * 16);
        for (t = 0; t < BeatStationProcessor::numTracks; ++t)
            for (s = 0; s < visible; ++s)
                if (cell (t, s).expanded (2.f).contains (p)) return true;
        return false;
    }

    BeatStationProcessor& proc;
    bool paintValue = true;
};

// The name next to a track. It shows your own name if you gave one, otherwise the name of the sound on the pad.
// Click it to hear the sound (it plays the pad). Right-click to rename.
class TrackLabel : public juce::Component, public juce::DragAndDropTarget
{
public:
    TrackLabel (BeatStationProcessor& p, int t) : proc (p), track (t) {}
    std::function<void (const juce::String& description)> onDropped;
    bool isInterestedInDragSource (const SourceDetails& d) override { const auto s = d.description.toString(); return s.startsWith ("sample:") || s.startsWith ("padcopy:"); }
    void itemDropped (const SourceDetails& d) override { if (onDropped) onDropped (d.description.toString()); }

    void paint (juce::Graphics& g) override
    {
        const int pad = padIndex();
        const auto info = proc.getPad (pad);
        juce::String text = proc.getTrackName (track);
        if (text.isEmpty()) text = info.sample != nullptr ? info.sample->name : "-";

        if (down) { g.setColour (Theme::amber.withAlpha (0.25f)); g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.f); }
        g.setColour (StepGrid::trackColour (track));
        g.fillEllipse (2.f, (float) getHeight() * 0.5f - 4.f, 8.f, 8.f);
        g.setColour (down ? Theme::text : Theme::dim);
        g.setFont (12.f);
        g.drawFittedText (text, 14, 0, getWidth() - 14, getHeight(), juce::Justification::centredLeft, 1);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            m.addItem (1, "Rename...");
            m.addItem (2, "Use the sound's name");
            juce::Component::SafePointer<TrackLabel> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r)
            {
                if (safe == nullptr) return;
                if (r == 1) safe->rename();
                else if (r == 2) { safe->proc.setTrackName (safe->track, {}); safe->repaint(); }
            });
            return;
        }
        note = proc.getPadNote (padIndex());                          // click = hear it: this plays the pad
        proc.keyboardState.noteOn (10, note, 0.9f);
        down = true;
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (! down) return;
        proc.keyboardState.noteOff (10, note, 0.f);
        down = false;
        repaint();
    }

private:
    int padIndex() const { return (int) proc.apvts.getRawParameterValue (ParamID::trackPad (track))->load(); }

    void rename()
    {
        auto* aw = new juce::AlertWindow ("Rename track", "Name this track (leave empty to use the sound's name):", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", proc.getTrackName (track));
        aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<TrackLabel> safe (this);
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([safe, aw] (int result)
        {
            if (result == 1 && safe != nullptr) { safe->proc.setTrackName (safe->track, aw->getTextEditorContents ("name")); safe->repaint(); }
        }), true);
    }

    BeatStationProcessor& proc;
    int track, note = 36;
    bool down = false;
};

// The whole step-sequencer drawer: transport, tempo, swing, REC, and one row per track (pad menu + mute + name + steps).
class SequencerPanel : public juce::Component, private juce::Timer
{
public:
    std::function<void (const juce::String& target)> onLearnMenu;   // right-click Play / Stop / REC to MIDI-learn them
    std::function<void (int pad)> onLoaded;                         // a sound was dropped onto a track (it loads into that track's pad)

    explicit SequencerPanel (BeatStationProcessor& p)
        : proc (p), grid (p),
          bpm (juce::Slider::LinearBar, juce::Slider::TextBoxLeft), swing (juce::Slider::LinearBar, juce::Slider::TextBoxLeft)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &playBtn, &stopBtn, &recBtn, &clearBtn, &lenBtn, &pageBtn, &bpm, &swing, &grid })
            addAndMakeVisible (*c);

        grid.onDropped = [this] (int t, const juce::String& d) { handleDrop (t, d); };
        for (auto* s : { &bpm, &swing })
        {
            s->setColour (juce::Slider::trackColourId, Theme::amber.withAlpha (0.35f));
            s->setColour (juce::Slider::backgroundColourId, Theme::graphBg);
            s->setColour (juce::Slider::textBoxTextColourId, Theme::text);
            s->setColour (juce::Slider::textBoxOutlineColourId, Theme::grid);
        }
        bpmAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamID::bpm, bpm);
        swingAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamID::swing, swing);

        juce::StringArray padNames;
        for (int i = 0; i < numPads; ++i) padNames.add ("Pad " + juce::String (i + 1));
        for (int t = 0; t < BeatStationProcessor::numTracks; ++t)
        {
            label[t] = std::make_unique<TrackLabel> (p, t);
            label[t]->onDropped = [this, t] (const juce::String& d) { handleDrop (t, d); };
            addAndMakeVisible (*label[t]);
            padBox[t].addItemList (padNames, 1);
            padBox[t].setColour (juce::ComboBox::backgroundColourId, Theme::graphBg);
            padBox[t].setColour (juce::ComboBox::textColourId, Theme::text);
            padBox[t].setColour (juce::ComboBox::outlineColourId, Theme::grid);
            padBox[t].setColour (juce::ComboBox::arrowColourId, Theme::dim);
            muteBtn[t].setButtonText ("M");
            muteBtn[t].setClickingTogglesState (true);
            addAndMakeVisible (padBox[t]);
            addAndMakeVisible (muteBtn[t]);
            padAtt[t] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ParamID::trackPad (t), padBox[t]);
            muteAtt[t] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamID::trackMute (t), muteBtn[t]);
        }

        auto leftClick = [] { return ! juce::ModifierKeys::currentModifiers.isPopupMenu(); };   // a right-click must not also press the button
        playBtn.onClick = [this, leftClick] { if (leftClick()) proc.startPlaying(); };
        stopBtn.onClick = [this, leftClick] { if (leftClick()) proc.stopPlaying(); };
        recBtn.onClick = [this, leftClick] { if (leftClick()) proc.recording = ! proc.recording.load(); };
        clearBtn.onClick = [this] { proc.clearPattern(); grid.repaint(); };
        lenBtn.onClick = [this]
        {
            auto* prm = proc.apvts.getParameter (ParamID::seqLen);
            prm->setValueNotifyingHost (prm->getValue() < 0.5f ? 1.f : 0.f);
        };
        pageBtn.onClick = [this] { if (proc.getPatternLength() == 32) grid.page = 1 - grid.page; };
        for (auto* b : { &playBtn, &stopBtn, &recBtn }) b->addMouseListener (this, false);
        startTimerHz (30);
    }

    ~SequencerPanel() override { for (auto* b : { &playBtn, &stopBtn, &recBtn }) b->removeMouseListener (this); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu() || ! onLearnMenu) return;
        if (e.eventComponent == &playBtn) onLearnMenu ("act:play");
        else if (e.eventComponent == &stopBtn) onLearnMenu ("act:stop");
        else if (e.eventComponent == &recBtn) onLearnMenu ("act:rec");
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto head = r.removeFromTop (34);
        playBtn.setBounds (head.removeFromLeft (64));  head.removeFromLeft (4);
        stopBtn.setBounds (head.removeFromLeft (64));  head.removeFromLeft (4);
        recBtn.setBounds (head.removeFromLeft (60));   head.removeFromLeft (14);
        head.removeFromLeft (34);                                    // "BPM" label (painted)
        bpm.setBounds (head.removeFromLeft (80).withSizeKeepingCentre (80, 28)); head.removeFromLeft (10);
        head.removeFromLeft (48);                                    // "SWING" label (painted)
        swing.setBounds (head.removeFromLeft (70).withSizeKeepingCentre (70, 28));
        clearBtn.setBounds (head.removeFromRight (60)); head.removeFromRight (4);
        pageBtn.setBounds (head.removeFromRight (96));  head.removeFromRight (4);
        lenBtn.setBounds (head.removeFromRight (96));

        r.removeFromTop (8);
        auto left = r.removeFromLeft (230);
        r.removeFromLeft (8);
        grid.setBounds (r);
        const float rowGap = 6.f, rowH = ((float) left.getHeight() - 3.f * rowGap) / 4.f;
        for (int t = 0; t < BeatStationProcessor::numTracks; ++t)
        {
            const int y = left.getY() + (int) ((float) t * (rowH + rowGap));
            padBox[t].setBounds (left.getX(), y, 76, (int) rowH);
            muteBtn[t].setBounds (left.getX() + 80, y, 34, (int) rowH);
            label[t]->setBounds (left.getX() + 118, y, left.getWidth() - 118, (int) rowH);
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::dim);
        g.setFont (juce::Font (11.f, juce::Font::bold));
        g.drawText ("BPM", bpm.getX() - 34, bpm.getY(), 32, bpm.getHeight(), juce::Justification::centredRight);
        g.drawText ("SWING", swing.getX() - 48, swing.getY(), 46, swing.getHeight(), juce::Justification::centredRight);
    }

private:
    void handleDrop (int track, const juce::String& description)
    {
        const int pad = (int) proc.apvts.getRawParameterValue (ParamID::trackPad (track))->load();
        if (description.startsWith ("sample:")) proc.loadSample (pad, juce::File (description.fromFirstOccurrenceOf ("sample:", false, false)));
        else if (description.startsWith ("padcopy:")) proc.copyPad (description.fromFirstOccurrenceOf (":", false, false).getIntValue(), pad);
        if (onLoaded) onLoaded (pad);
    }

    void timerCallback() override
    {
        playBtn.setToggleState (proc.playing.load(), juce::dontSendNotification);
        recBtn.setToggleState (proc.recording.load(), juce::dontSendNotification);
        const int len = proc.getPatternLength();
        lenBtn.setButtonText (len == 16 ? "Length 16" : "Length 32");
        if (len == 16) grid.page = 0;
        pageBtn.setEnabled (len == 32);
        pageBtn.setButtonText (grid.page == 0 ? "Steps 1-16" : "Steps 17-32");
        grid.repaint();
        for (auto& l : label) l->repaint();
    }

    BeatStationProcessor& proc;
    StepGrid grid;
    juce::TextButton playBtn { "Play" }, stopBtn { "Stop" }, recBtn { "REC" }, clearBtn { "Clear" }, lenBtn { "Length 16" }, pageBtn { "Steps 1-16" };
    juce::Slider bpm, swing;
    juce::ComboBox padBox[BeatStationProcessor::numTracks];
    juce::TextButton muteBtn[BeatStationProcessor::numTracks];
    std::unique_ptr<TrackLabel> label[BeatStationProcessor::numTracks];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAtt, swingAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> padAtt[BeatStationProcessor::numTracks];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAtt[BeatStationProcessor::numTracks];
};
