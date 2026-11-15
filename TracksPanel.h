#pragma once
#include "PluginProcessor.h"
#include "Theme.h"

// One audio track: [arm] [name + level] [M] [S] [volume] [the recording, drawn against time].
// Click the round light to arm the track for recording. Right-click the name for rename / clear / load a file / Booth on-off.
// Click in the recording area to choose where Play starts from.
class AudioTrackRow : public juce::Component
{
public:
    AudioTrackRow (BeatStationProcessor& p, int t) : proc (p), index (t) {}

    static constexpr int laneX = 262;
    static double viewSeconds (BeatStationProcessor& p) { return juce::jmax (30.0, (double) p.tracks.songLength() / p.getSR() + 10.0); }

    void paint (juce::Graphics& g) override
    {
        auto& tr = proc.tracks.tracks[(size_t) index];
        const bool armed = proc.tracks.armed.load() == index, rec = armed && proc.tracks.recording.load();
        const auto col = tr.booth.load() ? Theme::amber : juce::Colour (0xff5aa9ff);
        const float h = (float) getHeight();

        g.setColour (armed ? juce::Colour (0xffff5a4d) : Theme::tickOff);          // arm light
        g.fillEllipse (4.f, h * 0.5f - 7.f, 14.f, 14.f);
        if (armed) { g.setColour (juce::Colours::white.withAlpha (0.4f)); g.fillEllipse (7.f, h * 0.5f - 5.f, 6.f, 4.f); }

        g.setColour (armed ? Theme::text : Theme::dim);
        g.setFont (12.f);
        g.drawFittedText (tr.name, 24, 0, 92, getHeight() - 5, juce::Justification::centredLeft, 1);
        const float lv = juce::jlimit (0.f, 1.f, juce::jmap (juce::Decibels::gainToDecibels (tr.peak.load(), -60.f), -60.f, 0.f, 0.f, 1.f));
        g.setColour (Theme::tickOff);
        g.fillRect (24.f, h - 5.f, 92.f, 3.f);
        g.setColour (lv > 0.97f ? Theme::red : Theme::green);
        g.fillRect (24.f, h - 5.f, 92.f * lv, 3.f);

        button (g, muteRect, "M", tr.mute.load(), Theme::amber);
        button (g, soloRect, "S", tr.solo.load(), Theme::green);

        g.setColour (Theme::graphBg);                                               // volume
        g.fillRoundedRectangle (volRect.toFloat(), 4.f);
        g.setColour (col.withAlpha (0.45f));
        g.fillRoundedRectangle (volRect.toFloat().withWidth ((float) volRect.getWidth() * juce::jlimit (0.f, 1.f, tr.vol.load() / 1.25f)), 4.f);
        g.setColour (Theme::text);
        g.setFont (10.f);
        g.drawText (juce::String (juce::Decibels::gainToDecibels (tr.vol.load(), -60.f), 1) + " dB", volRect, juce::Justification::centred);

        const auto lane = laneRect().toFloat();                                      // the recording
        g.setColour (Theme::graphBg);
        g.fillRoundedRectangle (lane, 4.f);
        const double view = viewSeconds (proc), sr = proc.getSR();
        if (const auto clip = proc.tracks.getClip (index))
        {
            rebuildPeaks (clip);
            const double startS = (double) tr.start.load() / sr, lenS = (double) clip->data.getNumSamples() / clip->sampleRate;
            const float x0 = lane.getX() + (float) (startS / view) * lane.getWidth(), w = (float) (lenS / view) * lane.getWidth();
            g.setColour (col.withAlpha (tr.mute.load() ? 0.08f : 0.16f));
            g.fillRoundedRectangle (x0, lane.getY() + 1.f, w, lane.getHeight() - 2.f, 3.f);
            g.setColour (tr.mute.load() ? col.withAlpha (0.4f) : col);
            const float mid = lane.getCentreY(), half = lane.getHeight() * 0.46f;
            for (size_t i = 0; i < peaks.size(); ++i)
            {
                const float x = x0 + w * (float) i / (float) peaks.size();
                if (x > lane.getRight()) break;
                g.drawVerticalLine ((int) x, mid - peaks[i] * half, mid + peaks[i] * half + 1.f);
            }
            if (tr.burned.load()) { g.setColour (Theme::text); g.setFont (9.f); g.drawText ("MIC BURNED IN", (int) x0 + 4, (int) lane.getY() + 1, 90, 10, juce::Justification::left); }
        }
        auto xAt = [&] (juce::int64 samples) { return lane.getX() + (float) ((double) samples / sr / view) * lane.getWidth(); };
        if (armed && proc.tracks.punchMode.load() == 2)                             // auto punch: the IN and OUT points
        {
            const auto in = proc.tracks.punchIn.load(), out = proc.tracks.punchOut.load();
            if (in >= 0 && out > in) { g.setColour (juce::Colour (0xffff5a4d).withAlpha (0.12f)); g.fillRect (juce::Rectangle<float> (xAt (in), lane.getY(), xAt (out) - xAt (in), lane.getHeight()).getIntersection (lane)); }
            g.setColour (juce::Colour (0xffff5a4d));
            g.setFont (9.f);
            if (in >= 0)  { g.drawVerticalLine ((int) xAt (in), lane.getY(), lane.getBottom());  g.drawText ("IN", (int) xAt (in) + 2, (int) lane.getBottom() - 10, 20, 9, juce::Justification::left); }
            if (out >= 0) { g.drawVerticalLine ((int) xAt (out), lane.getY(), lane.getBottom()); g.drawText ("OUT", (int) xAt (out) - 22, (int) lane.getBottom() - 10, 20, 9, juce::Justification::right); }
        }
        if (rec && proc.tracks.regionStart.load() >= 0)                             // the part being kept, in red
        {
            const auto rs = proc.tracks.regionStart.load(), re = proc.tracks.regionEnd.load(), now = proc.tracks.songPos.load();
            const auto end = re >= 0 ? juce::jmin (re, now) : now;
            g.setColour (juce::Colour (0xffff5a4d).withAlpha (0.4f));
            if (end > rs) g.fillRect (juce::Rectangle<float> (xAt (rs), lane.getY(), xAt (end) - xAt (rs), lane.getHeight()).getIntersection (lane));
        }
        const float px = lane.getX() + (float) ((double) proc.tracks.songPos.load() / sr / view) * lane.getWidth();
        if (px <= lane.getRight()) { g.setColour (Theme::cream); g.drawVerticalLine ((int) px, lane.getY(), lane.getBottom()); }
        const float sx = lane.getX() + (float) ((double) proc.tracks.songStart.load() / sr / view) * lane.getWidth();
        g.setColour (Theme::green.withAlpha (0.7f));
        g.drawVerticalLine ((int) sx, lane.getY(), lane.getBottom());
    }

    void resized() override
    {
        muteRect = { 122, 3, 24, getHeight() - 6 };
        soloRect = { 150, 3, 24, getHeight() - 6 };
        volRect = { 180, 5, 74, getHeight() - 10 };
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto& tr = proc.tracks.tracks[(size_t) index];
        const auto p = e.getPosition();
        dragVol = false;
        if (p.x < 22) { proc.tracks.arm (proc.tracks.armed.load() == index ? -1 : index); }
        else if (p.x < 118) { if (e.mods.isPopupMenu()) nameMenu(); else proc.tracks.arm (index); }
        else if (muteRect.contains (p)) tr.mute = ! tr.mute.load();
        else if (soloRect.contains (p)) tr.solo = ! tr.solo.load();
        else if (volRect.contains (p)) { dragVol = true; setVol (e); }
        else if (laneRect().contains (p)) { if (e.mods.isPopupMenu()) laneMenu (e.x); else setStart (e); }
        if (auto* parent = getParentComponent()) parent->repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (dragVol) { setVol (e); repaint(); } else if (laneRect().contains (e.getPosition())) { setStart (e); repaint(); } }
    void mouseDoubleClick (const juce::MouseEvent& e) override { if (volRect.contains (e.getPosition())) { proc.tracks.tracks[(size_t) index].vol = 0.8f; repaint(); } }

private:
    juce::Rectangle<int> laneRect() const { return { laneX, 2, juce::jmax (10, getWidth() - laneX - 2), getHeight() - 4 }; }
    void setVol (const juce::MouseEvent& e) { proc.tracks.tracks[(size_t) index].vol = juce::jlimit (0.f, 1.25f, 1.25f * (float) (e.x - volRect.getX()) / (float) volRect.getWidth()); }
    void setStart (const juce::MouseEvent& e)                           // where Play starts from
    {
        const auto lane = laneRect();
        const double secs = juce::jlimit (0.0, 1.0, (double) (e.x - lane.getX()) / (double) lane.getWidth()) * viewSeconds (proc);
        proc.tracks.songStart = (juce::int64) (secs * proc.getSR());
        if (! proc.playing.load()) proc.tracks.songPos = proc.tracks.songStart.load();
    }
    void laneMenu (int x)                                                // right-click in the recording area: punch points and undo
    {
        const auto lane = laneRect();
        const auto at = (juce::int64) (juce::jlimit (0.0, 1.0, (double) (x - lane.getX()) / (double) lane.getWidth()) * viewSeconds (proc) * proc.getSR());
        juce::PopupMenu m;
        m.addItem (1, "Set punch IN here");
        m.addItem (2, "Set punch OUT here");
        m.addItem (3, "Clear the punch points", proc.tracks.punchIn.load() >= 0 || proc.tracks.punchOut.load() >= 0);
        m.addSeparator();
        m.addItem (4, "Undo the last take on this track", proc.tracks.canUndo (index));
        juce::Component::SafePointer<AudioTrackRow> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe, at] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& tk = safe->proc.tracks;
            if (r == 1) { tk.punchIn = at; if (tk.punchOut.load() <= at) tk.punchOut = -1; tk.punchMode = 2; tk.arm (safe->index); }
            else if (r == 2) { tk.punchOut = at; if (tk.punchIn.load() >= at) tk.punchIn = -1; tk.punchMode = 2; tk.arm (safe->index); }
            else if (r == 3) { tk.punchIn = -1; tk.punchOut = -1; }
            else if (r == 4) tk.undo (safe->index);
            if (auto* parent = safe->getParentComponent()) parent->repaint();
        });
    }
    static void button (juce::Graphics& g, juce::Rectangle<int> r, const char* text, bool on, juce::Colour col)
    {
        g.setColour (on ? col : Theme::panel);
        g.fillRoundedRectangle (r.toFloat(), 4.f);
        g.setColour (on ? col : Theme::grid.brighter (0.2f));
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 4.f, 1.f);
        g.setColour (on ? juce::Colour (0xff111316) : Theme::dim);
        g.setFont (juce::Font (11.f, juce::Font::bold));
        g.drawText (text, r, juce::Justification::centred);
    }
    void rebuildPeaks (const std::shared_ptr<const TrackClip>& clip)
    {
        if (clip == peaksFor) return;
        peaksFor = clip;
        const int n = clip->data.getNumSamples(), cols = 600;
        peaks.assign ((size_t) cols, 0.f);
        const float* d = clip->data.getReadPointer (0);
        for (int x = 0; x < cols; ++x)
        {
            const int i0 = (int) ((juce::int64) n * x / cols), i1 = juce::jmax (i0 + 1, (int) ((juce::int64) n * (x + 1) / cols)), stride = juce::jmax (1, (i1 - i0) / 48);
            float m = 0.f;
            for (int i = i0; i < i1 && i < n; i += stride) m = juce::jmax (m, std::abs (d[i]));
            peaks[(size_t) x] = juce::jmin (1.f, m);
        }
    }
    void nameMenu()
    {
        auto& tr = proc.tracks.tracks[(size_t) index];
        juce::PopupMenu m;
        m.addItem (1, "Rename...");
        m.addItem (2, "Goes through the Booth (mic and vocal chain)", true, tr.booth.load());
        m.addItem (3, "Load an audio file onto this track...");
        m.addSeparator();
        m.addItem (4, "Clear this track's recording", proc.tracks.getClip (index) != nullptr);
        juce::Component::SafePointer<AudioTrackRow> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r)
        {
            if (safe == nullptr) return;
            auto& t = safe->proc.tracks.tracks[(size_t) safe->index];
            if (r == 2) t.booth = ! t.booth.load();
            else if (r == 4) safe->proc.tracks.clearClip (safe->index);
            else if (r == 3)
            {
                safe->chooser = std::make_unique<juce::FileChooser> ("Choose audio for this track", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
                safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
                {
                    if (safe != nullptr && fc.getResult().existsAsFile())
                        safe->proc.tracks.loadClipFile (safe->index, fc.getResult(), safe->proc.tracks.songStart.load(), true, safe->proc.getFormats());
                });
            }
            else if (r == 1)
            {
                auto* aw = new juce::AlertWindow ("Rename track", "Track name:", juce::MessageBoxIconType::NoIcon);
                aw->addTextEditor ("name", t.name);
                aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
                aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                aw->enterModalState (true, juce::ModalCallbackFunction::create ([safe, aw] (int result)
                {
                    if (result == 1 && safe != nullptr) { safe->proc.tracks.tracks[(size_t) safe->index].name = aw->getTextEditorContents ("name"); safe->repaint(); }
                }), true);
            }
            safe->repaint();
        });
    }

    BeatStationProcessor& proc;
    int index;
    bool dragVol = false;
    juce::Rectangle<int> muteRect, soloRect, volRect;
    std::shared_ptr<const TrackClip> peaksFor;
    std::vector<float> peaks;
    std::unique_ptr<juce::FileChooser> chooser;
};

// A set of audio track rows with the transport for recording. Used for all 8 tracks (TRACKS tab) and for the vocal tracks in the Booth.
class TrackList : public juce::Component, private juce::Timer
{
public:
    TrackList (BeatStationProcessor& p, int firstTrack, int count) : proc (p)
    {
        for (int i = 0; i < count; ++i)
        {
            rows.push_back (std::make_unique<AudioTrackRow> (p, firstTrack + i));
            holder.addAndMakeVisible (*rows.back());
        }
        view.setViewedComponent (&holder, false);
        view.setScrollBarsShown (true, false);
        view.setScrollBarThickness (10);
        for (auto* c : std::initializer_list<juce::Component*> { &recBtn, &playBtn, &stopBtn, &monBtn, &punchBox, &punchBtn, &view }) addAndMakeVisible (*c);
        punchBox.addItemList ({ "Whole take", "Manual punch", "Auto punch (IN / OUT)", "Match punch (my last take)" }, 1);
        punchBox.setColour (juce::ComboBox::backgroundColourId, Theme::panel);
        punchBox.setColour (juce::ComboBox::textColourId, Theme::text);
        punchBox.setColour (juce::ComboBox::outlineColourId, Theme::grid);
        punchBox.setColour (juce::ComboBox::arrowColourId, Theme::amber);
        punchBox.onChange = [this] { proc.tracks.punchMode = punchBox.getSelectedItemIndex(); };
        punchBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd0221f));
        punchBtn.onClick = [this] { proc.tracks.punchToggle = true; };
        monBtn.setClickingTogglesState (true);
        monAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "bth_monitor", monBtn);
        recBtn.onClick = [this]                                           // REC: arms a track if none is, and starts the song so the take begins straight away
        {
            auto& t = proc.tracks;
            if (t.recEnabled.load()) { t.recEnabled = false; return; }
            if (t.armed.load() < 0) t.arm (first());
            t.recEnabled = true;
            if (! proc.playing.load()) proc.startPlaying();
        };
        playBtn.onClick = [this] { proc.startPlaying(); };
        stopBtn.onClick = [this] { proc.stopPlaying(); };
        startTimerHz (24);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto head = r.removeFromTop (28);
        recBtn.setBounds (head.removeFromLeft (64));   head.removeFromLeft (4);
        playBtn.setBounds (head.removeFromLeft (56));  head.removeFromLeft (4);
        stopBtn.setBounds (head.removeFromLeft (56));  head.removeFromLeft (10);
        monBtn.setBounds (head.removeFromLeft (150));  head.removeFromLeft (10);
        punchBox.setBounds (head.removeFromLeft (196)); head.removeFromLeft (4);
        punchBtn.setBounds (head.removeFromLeft (70));  head.removeFromLeft (10);
        infoRect = head;
        r.removeFromTop (4);
        view.setBounds (r);
        const int w = juce::jmax (100, view.getWidth() - 12);
        holder.setSize (w, (int) rows.size() * 34);
        for (size_t i = 0; i < rows.size(); ++i) rows[i]->setBounds (0, (int) i * 34, w, 30);
    }

    void paint (juce::Graphics& g) override
    {
        const double secs = (double) proc.tracks.songPos.load() / proc.getSR();
        g.setColour (Theme::text);
        g.setFont (juce::Font (13.f, juce::Font::bold));
        auto info = infoRect;
        g.drawText (juce::String ((int) secs / 60) + ":" + juce::String (std::fmod (secs, 60.0), 1).paddedLeft ('0', 4), info.removeFromLeft (64), juce::Justification::centredLeft);
        auto& tk = proc.tracks;
        const int mode = tk.punchMode.load();
        const bool pass = tk.recording.load(), in = pass && tk.isPunchedAt (tk.songPos.load());
        juce::String text;
        if (in) text = "PUNCHED IN - recording";
        else if (pass && mode == 3) text = "LISTENING for a match...   you " + juce::String (juce::roundToInt (juce::Decibels::gainToDecibels (tk.matchLive.load(), -60.f)))
                                           + " dB   last take " + juce::String (juce::roundToInt (juce::Decibels::gainToDecibels (tk.matchOld.load(), -60.f))) + " dB";
        else if (pass) text = mode == 1 ? "Rolling - press PUNCH to drop in" : "Rolling - waiting for the IN point";
        else if (mode == 0) text = "Arm a track (click its light), then REC. Stop ends the take. Right-click a recording for punch points and undo.";
        else if (mode == 1) text = "Manual punch: REC rolls the song, PUNCH drops in, PUNCH again drops out. Only that part is replaced.";
        else if (mode == 2) text = "Auto punch: right-click the armed track's recording to set IN and OUT, then REC. Only between them is replaced.";
        else text = "Match punch: REC, then rap along with your last take. When your level matches it, it drops in at the start of that phrase.";
        g.setColour (in ? juce::Colour (0xffff5a4d) : Theme::dim);
        g.setFont (in ? juce::Font (12.f, juce::Font::bold) : juce::Font (11.f));
        g.drawFittedText (text, info, juce::Justification::centredLeft, 2);
    }

private:
    int first() const { return 0; }
    void timerCallback() override
    {
        if (! isShowing()) return;
        recBtn.setToggleState (proc.tracks.recEnabled.load(), juce::dontSendNotification);
        recBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd0221f));
        playBtn.setToggleState (proc.playing.load(), juce::dontSendNotification);
        if (punchBox.getSelectedItemIndex() != proc.tracks.punchMode.load()) punchBox.setSelectedItemIndex (proc.tracks.punchMode.load(), juce::dontSendNotification);
        punchBtn.setVisible (proc.tracks.punchMode.load() == 1);
        punchBtn.setToggleState (proc.tracks.recording.load() && proc.tracks.isPunchedAt (proc.tracks.songPos.load()), juce::dontSendNotification);
        holder.repaint();
        repaint (0, 0, getWidth(), 28);
    }

    BeatStationProcessor& proc;
    juce::Viewport view;
    juce::Component holder;
    std::vector<std::unique_ptr<AudioTrackRow>> rows;
    juce::TextButton recBtn { "REC" }, playBtn { "Play" }, stopBtn { "Stop" }, monBtn { "MONITOR (headphones)" }, punchBtn { "PUNCH" };
    juce::ComboBox punchBox;
    juce::Rectangle<int> infoRect;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monAtt;
};
