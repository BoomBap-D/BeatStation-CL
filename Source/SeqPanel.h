#pragma once
#include "PluginProcessor.h"
#include "Theme.h"

// One row of the step sequencer: [source] [M] [name] [16 steps of the bar being shown].
// The source is a pad or one key of the Keys instrument. Click the source box to change it.
// Click the name to hear it. Right-click the name to rename or delete the track. Click or drag across the steps to paint them.
class TrackRow : public juce::Component, public juce::DragAndDropTarget
{
public:
    TrackRow (BeatStationProcessor& p, int t) : proc (p), track (t) {}

    int page = 0;                                                       // which bar is shown
    std::function<void (int pad)> onLoaded;

    static juce::Colour trackColour (int t)
    {
        static const juce::Colour c[8] = { Theme::red, Theme::amber, Theme::cream, Theme::green,
                                           juce::Colour (0xff3ad0e0), juce::Colour (0xff8f7cf0), juce::Colour (0xffff8a5c), juce::Colour (0xff9bd35a) };
        return c[t % 8];
    }
    static juce::String srcName (int src)
    {
        return src < BeatStationProcessor::keySrc ? "Pad " + juce::String (src + 1)
                                                  : "Key " + juce::MidiMessage::getMidiNoteName (src - BeatStationProcessor::keySrc, true, true, 3);
    }

    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        const auto s = d.description.toString();
        return proc.getTrackSrc (track) < BeatStationProcessor::keySrc && (s.startsWith ("sample:") || s.startsWith ("padcopy:"));
    }
    void itemDropped (const SourceDetails& d) override                  // drop a sound on a pad track: it loads into that pad
    {
        const int pad = proc.getTrackSrc (track);
        const auto s = d.description.toString();
        if (s.startsWith ("sample:")) proc.loadSample (pad, juce::File (s.fromFirstOccurrenceOf ("sample:", false, false)));
        else proc.copyPad (s.fromFirstOccurrenceOf (":", false, false).getIntValue(), pad);
        if (onLoaded) onLoaded (pad);
    }

    juce::Rectangle<float> cell (int s) const
    {
        const float x0 = (float) stepsX, gap = 4.f, w = ((float) getWidth() - x0 - 15.f * gap) / 16.f;
        return { x0 + (float) s * (w + gap), 0.f, w, (float) getHeight() };
    }

    void paint (juce::Graphics& g) override
    {
        const int src = proc.getTrackSrc (track);
        const bool muted = proc.getTrackMute (track);
        const auto col = trackColour (track);

        // source box
        g.setColour (Theme::graphBg);
        g.fillRoundedRectangle (srcRect.toFloat(), 5.f);
        g.setColour (Theme::grid.brighter (0.1f));
        g.drawRoundedRectangle (srcRect.toFloat().reduced (0.5f), 5.f, 1.f);
        g.setColour (Theme::text);
        g.setFont (12.f);
        g.drawFittedText (srcName (src), srcRect.reduced (5, 0), juce::Justification::centredLeft, 1);

        // mute
        g.setColour (muted ? Theme::amber : Theme::panel);
        g.fillRoundedRectangle (muteRect.toFloat(), 5.f);
        g.setColour (muted ? Theme::amber : Theme::grid.brighter (0.2f));
        g.drawRoundedRectangle (muteRect.toFloat().reduced (0.5f), 5.f, 1.f);
        g.setColour (muted ? juce::Colour (0xff111316) : Theme::dim);
        g.setFont (juce::Font (12.f, juce::Font::bold));
        g.drawText ("M", muteRect, juce::Justification::centred);

        // name
        juce::String text = proc.getTrackName (track);
        if (text.isEmpty())
        {
            if (src < BeatStationProcessor::keySrc) { const auto info = proc.getPad (src); text = info.sample != nullptr ? info.sample->name : "-"; }
            else text = "Keys instrument";
        }
        if (down) { g.setColour (Theme::amber.withAlpha (0.25f)); g.fillRoundedRectangle (nameRect.toFloat(), 5.f); }
        g.setColour (col);
        g.fillEllipse ((float) nameRect.getX() + 2.f, (float) getHeight() * 0.5f - 4.f, 8.f, 8.f);
        g.setColour (down ? Theme::text : Theme::dim);
        g.setFont (12.f);
        g.drawFittedText (text, nameRect.withTrimmedLeft (14), juce::Justification::centredLeft, 1);

        // steps
        const int cur = proc.currentStep.load(), len = proc.getPatternLength();
        for (int s = 0; s < 16; ++s)
        {
            const int abs = page * 16 + s;
            if (abs >= len) break;
            const auto r = cell (s);
            g.setColour (proc.getStep (track, abs) ? (muted ? col.withAlpha (0.4f) : col) : ((s / 4) % 2 == 0 ? juce::Colour (0xff2f333c) : juce::Colour (0xff262a31)));
            g.fillRoundedRectangle (r, 5.f);
            g.setColour (juce::Colour (0xff3a3f4a));
            g.drawRoundedRectangle (r.reduced (0.5f), 5.f, 1.f);
            if (abs == cur) { g.setColour (Theme::cream); g.drawRoundedRectangle (r.reduced (1.f), 5.f, 2.f); }
        }
    }

    void resized() override
    {
        srcRect = { 0, 0, 76, getHeight() };
        muteRect = { 80, 0, 34, getHeight() };
        nameRect = { 118, 0, 112, getHeight() };
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.getPosition();
        painting = false;
        if (srcRect.contains (p)) { showSourceMenu(); return; }
        if (muteRect.contains (p)) { proc.setTrackMute (track, ! proc.getTrackMute (track)); repaint(); return; }
        if (nameRect.contains (p))
        {
            if (e.mods.isPopupMenu()) { showNameMenu(); return; }
            const int src = proc.getTrackSrc (track);                    // click = hear it
            if (src < BeatStationProcessor::keySrc) { ch = 10; note = proc.getPadNote (src); }
            else { ch = 15; note = src - BeatStationProcessor::keySrc; }
            proc.keyboardState.noteOn (ch, note, 0.9f);
            down = true;
            repaint();
            return;
        }
        int s;
        if (hitStep (e.position, s)) { painting = true; paintValue = ! proc.getStep (track, page * 16 + s); proc.setStep (track, page * 16 + s, paintValue); repaint(); }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        int s;
        if (painting && hitStep (e.position, s)) { proc.setStep (track, page * 16 + s, paintValue); repaint(); }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (down) { proc.keyboardState.noteOff (ch, note, 0.f); down = false; repaint(); }
    }

private:
    bool hitStep (juce::Point<float> p, int& s) const
    {
        const int len = proc.getPatternLength();
        for (s = 0; s < 16 && page * 16 + s < len; ++s)
            if (cell (s).expanded (2.f, 0.f).contains (p)) return true;
        return false;
    }

    void showSourceMenu()
    {
        juce::PopupMenu m, padMenu, keyMenu;
        const int cur = proc.getTrackSrc (track);
        for (int i = 0; i < numPads; ++i)
        {
            const auto info = proc.getPad (i);
            padMenu.addItem (1 + i, "Pad " + juce::String (i + 1) + (info.sample != nullptr ? "  -  " + info.sample->name : juce::String()), true, cur == i);
        }
        for (int n = 24; n <= 96; ++n)                                   // C1 to C7
            keyMenu.addItem (1000 + n, juce::MidiMessage::getMidiNoteName (n, true, true, 3), true, cur == BeatStationProcessor::keySrc + n);
        m.addSubMenu ("Pads", padMenu);
        m.addSubMenu ("Keys (plays the Keys instrument)", keyMenu);
        juce::Component::SafePointer<TrackRow> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (srcRect)), [safe] (int r)
        {
            if (safe == nullptr || r == 0) return;
            safe->proc.setTrackSrc (safe->track, r >= 1000 ? BeatStationProcessor::keySrc + (r - 1000) : r - 1);
            safe->repaint();
        });
    }

    void showNameMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Rename...");
        m.addItem (2, "Use the sound's name");
        m.addSeparator();
        m.addItem (3, "Clear this track's steps");
        m.addItem (4, "Delete this track");
        juce::Component::SafePointer<TrackRow> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r)
        {
            if (safe == nullptr) return;
            if (r == 1) safe->rename();
            else if (r == 2) { safe->proc.setTrackName (safe->track, {}); safe->repaint(); }
            else if (r == 3) { for (int s = 0; s < BeatStationProcessor::maxSteps; ++s) safe->proc.setStep (safe->track, s, false); safe->repaint(); }
            else if (r == 4) safe->proc.removeTrack (safe->track);      // the panel rebuilds its rows on the next timer tick
        });
    }

    void rename()
    {
        auto* aw = new juce::AlertWindow ("Rename track", "Name this track (leave empty to use the sound's name):", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", proc.getTrackName (track));
        aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<TrackRow> safe (this);
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([safe, aw] (int result)
        {
            if (result == 1 && safe != nullptr) { safe->proc.setTrackName (safe->track, aw->getTextEditorContents ("name")); safe->repaint(); }
        }), true);
    }

    BeatStationProcessor& proc;
    int track, note = 36, ch = 10;
    static constexpr int stepsX = 238;
    bool down = false, painting = false, paintValue = true;
    juce::Rectangle<int> srcRect, muteRect, nameRect;
};

// The whole step-sequencer drawer: transport, tempo, swing, REC, bars, and a scrolling list of tracks (up to 64).
// REC while the sequencer plays: every pad or key you hit is written to its own track. A track is made automatically if there is none yet.
class SequencerPanel : public juce::Component, private juce::Timer
{
public:
    std::function<void (const juce::String& target)> onLearnMenu;   // right-click Play / Stop / REC to MIDI-learn them
    std::function<void (int pad)> onLoaded;                         // a sound was dropped onto a track (it loads into that track's pad)

    explicit SequencerPanel (BeatStationProcessor& p)
        : proc (p), bpm (juce::Slider::LinearBar, juce::Slider::TextBoxLeft), swing (juce::Slider::LinearBar, juce::Slider::TextBoxLeft)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &playBtn, &stopBtn, &recBtn, &clearBtn, &barsBtn, &prevBtn, &nextBtn, &addBtn, &bpm, &swing, &view })
            addAndMakeVisible (*c);
        view.setViewedComponent (&rows, false);
        view.setScrollBarsShown (true, false);
        view.setScrollBarThickness (10);

        for (auto* s : { &bpm, &swing })
        {
            s->setColour (juce::Slider::trackColourId, Theme::amber.withAlpha (0.35f));
            s->setColour (juce::Slider::backgroundColourId, Theme::graphBg);
            s->setColour (juce::Slider::textBoxTextColourId, Theme::text);
            s->setColour (juce::Slider::textBoxOutlineColourId, Theme::grid);
        }
        bpmAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamID::bpm, bpm);
        swingAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamID::swing, swing);

        auto leftClick = [] { return ! juce::ModifierKeys::currentModifiers.isPopupMenu(); };   // a right-click must not also press the button
        playBtn.onClick = [this, leftClick] { if (leftClick()) proc.startPlaying(); };
        stopBtn.onClick = [this, leftClick] { if (leftClick()) proc.stopPlaying(); };
        recBtn.onClick = [this, leftClick]                               // REC also starts the beat if it is stopped: nothing records while stopped
        {
            if (! leftClick()) return;
            proc.recording = ! proc.recording.load();
            if (proc.recording.load() && ! proc.playing.load()) proc.startPlaying();
        };
        clearBtn.onClick = [this] { proc.clearPattern(); rows.repaint(); };
        barsBtn.onClick = [this]                                          // one button: pick how many bars the pattern has
        {
            juce::PopupMenu m;
            for (int b : { 1, 2, 4, 8 }) m.addItem (b, juce::String (b) + (b == 1 ? " bar  (16 steps)" : " bars  (" + juce::String (b * 16) + " steps)"), true, proc.getBars() == b);
            juce::Component::SafePointer<SequencerPanel> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&barsBtn), [safe] (int r) { if (safe != nullptr && r > 0) safe->proc.setBars (r); });
        };
        prevBtn.onClick = [this] { setPage (page - 1); };
        nextBtn.onClick = [this] { setPage (page + 1); };
        addBtn.onClick = [this]                                           // a new track on the first pad that has no track yet
        {
            int src = 0;
            for (int i = 0; i < numPads; ++i) if (proc.findTrack (i) < 0) { src = i; break; }
            proc.addTrack (src);
            scrollToEnd = true;
        };
        for (auto* b : { &playBtn, &stopBtn, &recBtn }) b->addMouseListener (this, false);
        rebuild();
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
        addBtn.setBounds (head.removeFromRight (78));   head.removeFromRight (10);
        nextBtn.setBounds (head.removeFromRight (30));
        pageRect = head.removeFromRight (74);
        prevBtn.setBounds (head.removeFromRight (30));  head.removeFromRight (10);
        barsBtn.setBounds (head.removeFromRight (110));

        r.removeFromTop (8);
        view.setBounds (r);
        layoutRows();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::dim);
        g.setFont (juce::Font (11.f, juce::Font::bold));
        g.drawText ("BPM", bpm.getX() - 34, bpm.getY(), 32, bpm.getHeight(), juce::Justification::centredRight);
        g.drawText ("SWING", swing.getX() - 48, swing.getY(), 46, swing.getHeight(), juce::Justification::centredRight);
        g.setColour (Theme::text);
        g.setFont (12.f);
        g.drawText ("Bar " + juce::String (page + 1) + " of " + juce::String (proc.getBars()), pageRect, juce::Justification::centred);
    }

private:
    static constexpr int rowH = 34, rowGap = 5;

    void setPage (int p)
    {
        page = juce::jlimit (0, proc.getBars() - 1, p);
        for (auto& r : rowList) r->page = page;
        repaint();
    }

    void rebuild()
    {
        rowList.clear();
        const int n = proc.getNumTracks();
        for (int t = 0; t < n; ++t)
        {
            auto row = std::make_unique<TrackRow> (proc, t);
            row->page = page;
            row->onLoaded = [this] (int pad) { if (onLoaded) onLoaded (pad); };
            rows.addAndMakeVisible (*row);
            rowList.push_back (std::move (row));
        }
        shownVersion = proc.tracksVersion.load();
        layoutRows();
    }

    void layoutRows()
    {
        const int w = juce::jmax (100, view.getWidth() - 14), n = (int) rowList.size();
        rows.setSize (w, juce::jmax (1, n * (rowH + rowGap)));
        for (int t = 0; t < n; ++t) rowList[(size_t) t]->setBounds (0, t * (rowH + rowGap), w, rowH);
    }

    void timerCallback() override
    {
        playBtn.setToggleState (proc.playing.load(), juce::dontSendNotification);
        recBtn.setToggleState (proc.recording.load(), juce::dontSendNotification);
        if (proc.tracksVersion.load() != shownVersion)
        {
            const bool grew = proc.getNumTracks() > (int) rowList.size();
            rebuild();
            if (grew || scrollToEnd) view.setViewPosition (0, juce::jmax (0, rows.getHeight() - view.getHeight()));   // show the new track
            scrollToEnd = false;
        }
        const int bars = proc.getBars();
        if (bars != shownBars) { shownBars = bars; barsBtn.setButtonText ("Length: " + juce::String (bars) + (bars == 1 ? " bar" : " bars")); }
        if (page >= bars) setPage (bars - 1);
        prevBtn.setEnabled (page > 0);
        nextBtn.setEnabled (page < bars - 1);
        rows.repaint();
        repaint (pageRect);
    }

    BeatStationProcessor& proc;
    juce::Viewport view;
    juce::Component rows;
    std::vector<std::unique_ptr<TrackRow>> rowList;
    juce::TextButton playBtn { "Play" }, stopBtn { "Stop" }, recBtn { "REC" }, clearBtn { "Clear" }, barsBtn { "Length: 1 bar" },
                     prevBtn { "<" }, nextBtn { ">" }, addBtn { "+ Track" };
    juce::Slider bpm, swing;
    juce::Rectangle<int> pageRect;
    int page = 0, shownVersion = -1, shownBars = -1;
    bool scrollToEnd = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAtt, swingAtt;
};
