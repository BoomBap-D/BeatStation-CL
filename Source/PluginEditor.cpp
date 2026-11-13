#include "PluginEditor.h"

BeatStationEditor::BeatStationEditor (BeatStationProcessor& p)
    : AudioProcessorEditor (&p), proc (p), inMeter (p, 0, "IN"), outMeter (p, 1, "OUT"), pads (p), wave (p), browser (p), seq (p), shape (p),
      keyboard (p.keyboardState)
{
    setLookAndFeel (&lnf);
    for (auto* c : std::initializer_list<juce::Component*> { &inMeter, &outMeter, &pads, &wave, &browser, &seq, &shape, &keyboard,
                                                              &loadBtn, &clearBtn, &clearChopsBtn, &fullBtn, &modeBtn, &keysBtn,
                                                              &chop4, &chop8, &chop16, &chopHits, &stopBtn, &playTop, &stopTop,
                                                              &editTab, &browseTab, &keysTab, &seqTab, &shapeTab, &kitBox,
                                                              &level, &pan, &tune, &choke })
        addAndMakeVisible (*c);
    if (isStandalone()) addAndMakeVisible (midiBtn);
    stopBtn.setColour (juce::TextButton::textColourOffId, Theme::red);

    kitKeys = BeatStationProcessor::getKitKeys();
    for (int i = 0; i < kitKeys.size(); ++i) kitBox.addItem (BeatStationProcessor::kitDisplayName (kitKeys[i]), i + 1);
    kitBox.setTextWhenNothingSelected ("Load a kit...");
    kitBox.setSelectedItemIndex (juce::jmax (0, kitKeys.indexOf ("trap")), juce::dontSendNotification);
    kitBox.setColour (juce::ComboBox::backgroundColourId, Theme::panel);
    kitBox.setColour (juce::ComboBox::textColourId, Theme::text);
    kitBox.setColour (juce::ComboBox::outlineColourId, Theme::grid);
    kitBox.setColour (juce::ComboBox::arrowColourId, Theme::amber);
    kitBox.onChange = [this]
    {
        const int i = kitBox.getSelectedItemIndex();
        if (i >= 0) { proc.loadKit (kitKeys[i]); refreshAfterEdit(); }
    };

    pads.onSelect = [this] (int i) { selectPad (i); };
    pads.onSamplesChanged = [this] { refreshAfterEdit(); };
    pads.onRightClick = [this] (int i) { showLearnMenu ("pad:" + juce::String (i)); };

    // MIDI Learn: right-click any knob, pad, or transport button
    auto knobMenu = [this] (const juce::String& id) { showLearnMenu ("param:" + id); };
    for (auto* k : { &level, &pan, &tune, &choke }) k->onLearnMenu = knobMenu;
    shape.setLearnCallback (knobMenu);
    seq.onLearnMenu = [this] (const juce::String& target) { showLearnMenu (target); };
    for (auto* b : { &playTop, &stopTop, &stopBtn }) b->addMouseListener (this, false);

    browser.onLoad = [this] (const juce::File& f)
    {
        proc.loadSample (selPad, f);
        const int next = selPad + 1;
        refreshAfterEdit();
        if (browser.advanceAfterLoad() && next < numPads) selectPad (next);
    };

    auto leftClick = [] { return ! juce::ModifierKeys::currentModifiers.isPopupMenu(); };
    loadBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a sample for pad " + juce::String (selPad + 1), juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile()) { proc.loadSample (selPad, f); refreshAfterEdit(); }
                              });
    };
    clearBtn.onClick = [this] { proc.clearPad (selPad); refreshAfterEdit(); };
    clearChopsBtn.onClick = [this]
    {
        const auto s = proc.getPad (selPad).sample;
        int home = selPad;                                              // the pad that keeps the whole sample
        for (int i = 0; i < numPads; ++i) if (proc.getPad (i).sample == s) { home = i; break; }
        proc.clearChops (selPad);
        selectPad (home);
        refreshAfterEdit();
    };
    fullBtn.onClick = [this] { proc.auditionWhole (selPad); };
    chop4.onClick    = [this] { proc.chopGrid (selPad, 4);  refreshAfterEdit(); };
    chop8.onClick    = [this] { proc.chopGrid (selPad, 8);  refreshAfterEdit(); };
    chop16.onClick   = [this] { proc.chopGrid (selPad, 16); refreshAfterEdit(); };
    chopHits.onClick = [this] { proc.chopTransients (selPad, 16); refreshAfterEdit(); };
    stopBtn.onClick  = [this, leftClick] { if (leftClick()) proc.stopAll = true; };
    playTop.onClick  = [this, leftClick] { if (! leftClick()) return; if (proc.playing.load()) proc.stopPlaying(); else proc.startPlaying(); };
    stopTop.onClick  = [this, leftClick] { if (leftClick()) proc.stopPlaying(); };
    midiBtn.onClick  = [this] { proc.rescanMidi(); repaint(); };
    editTab.onClick   = [this] { rightMode = 0; updateRightMode(); };
    browseTab.onClick = [this] { rightMode = 1; updateRightMode(); };
    keysTab.onClick  = [this] { drawer = 0; updateDrawer(); };
    seqTab.onClick   = [this] { drawer = 1; updateDrawer(); };
    shapeTab.onClick = [this] { drawer = 2; updateDrawer(); };

    modeBtn.onClick = [this]                                        // cycles Cut -> Layer -> Gate
    {
        auto* prm = proc.apvts.getParameter (ParamID::mode (selPad));
        const int m = (int) proc.apvts.getRawParameterValue (ParamID::mode (selPad))->load();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) ((m + 1) % 3)));
        updateButtons();
    };
    keysBtn.onClick = [this]
    {
        auto* prm = proc.apvts.getParameter (ParamID::keysMode);
        prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.f : 1.f);
        updateButtons();
    };

    selectPad (0);
    updateDrawer();
    updateRightMode();
    setWantsKeyboardFocus (true);
    setSize (1032, 730);
    startTimerHz (15);
}

void BeatStationEditor::updateRightMode()
{
    const bool edit = rightMode == 0;
    for (auto* c : std::initializer_list<juce::Component*> { &wave, &loadBtn, &clearBtn, &clearChopsBtn, &fullBtn, &modeBtn, &keysBtn,
                                                              &chop4, &chop8, &chop16, &chopHits, &level, &pan, &tune, &choke })
        c->setVisible (edit);
    browser.setVisible (! edit);
    editTab.setToggleState (edit, juce::dontSendNotification);
    browseTab.setToggleState (! edit, juce::dontSendNotification);
    repaint();
}

void BeatStationEditor::updateDrawer()
{
    keyboard.setVisible (drawer == 0);
    seq.setVisible (drawer == 1);
    shape.setVisible (drawer == 2);
    keysTab.setToggleState (drawer == 0, juce::dontSendNotification);
    seqTab.setToggleState (drawer == 1, juce::dontSendNotification);
    shapeTab.setToggleState (drawer == 2, juce::dontSendNotification);
}

void BeatStationEditor::updateButtons()
{
    static const char* names[] = { "Mode: Cut", "Mode: Layer", "Mode: Gate" };
    modeBtn.setButtonText (names[juce::jlimit (0, 2, (int) proc.apvts.getRawParameterValue (ParamID::mode (selPad))->load())]);
    keysBtn.setToggleState (proc.apvts.getRawParameterValue (ParamID::keysMode)->load() > 0.5f, juce::dontSendNotification);
}

void BeatStationEditor::selectPad (int i)
{
    selPad = pads.selected = i;
    proc.selectedPad = i;
    level.attach (proc.apvts, ParamID::level (i));
    pan.attach (proc.apvts, ParamID::pan (i));
    tune.attach (proc.apvts, ParamID::tune (i));
    choke.attach (proc.apvts, ParamID::group (i));
    wave.setPad (i);
    shape.setPad (i);
    browser.setTargetPad (i);
    updateButtons();
    repaint();
}

void BeatStationEditor::refreshAfterEdit() { wave.reload(); pads.repaint(); repaint(); }

void BeatStationEditor::showLearnMenu (const juce::String& target)
{
    juce::PopupMenu m;
    const auto mapped = proc.describeMapping (target);
    const bool isPad = target.startsWith ("pad:");
    m.addItem (1, isPad ? "MIDI Learn (hit a pad or key)" : "MIDI Learn");
    if (isPad || mapped.isNotEmpty())
        m.addItem (2, isPad ? "Reset to the default note (now " + mapped + ")" : "Clear MIDI mapping (" + mapped + ")");
    juce::Component::SafePointer<BeatStationEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, target] (int r)
    {
        if (safe == nullptr) return;
        if (r == 1) safe->proc.startLearn (target);
        else if (r == 2) safe->proc.clearMapping (target);
        safe->repaint();
    });
}

void BeatStationEditor::mouseDown (const juce::MouseEvent& e)
{
    if (proc.getLearnTarget().isNotEmpty() && bannerRect.contains (e.getEventRelativeTo (this).getPosition())) { proc.cancelLearn(); repaint(); return; }
    if (! e.mods.isPopupMenu()) return;
    if (e.eventComponent == &playTop) showLearnMenu ("act:play");
    else if (e.eventComponent == &stopTop) showLearnMenu ("act:stop");
    else if (e.eventComponent == &stopBtn) showLearnMenu ("act:stopall");
}

bool BeatStationEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey && proc.getLearnTarget().isNotEmpty()) { proc.cancelLearn(); repaint(); return true; }
    return false;
}

void BeatStationEditor::timerCallback()
{
    const bool learning = proc.getLearnTarget().isNotEmpty();
    const int version = proc.mapVersion.load();
    const bool playing = proc.playing.load();
    const juce::String text = juce::String (proc.apvts.getRawParameterValue (ParamID::bpm)->load(), 1) + " BPM";
    if (learning != wasLearning || version != lastMapVersion || text != bpmText || playTop.getToggleState() != playing)
    {
        wasLearning = learning;
        lastMapVersion = version;
        bpmText = text;
        playTop.setToggleState (playing, juce::dontSendNotification);
        repaint();
    }
}

void BeatStationEditor::resized()
{
    auto full = getLocalBounds();
    inMeter.setBounds (full.removeFromLeft (46).reduced (3, 8));
    outMeter.setBounds (full.removeFromRight (46).reduced (3, 8));
    auto r = full.reduced (20);

    auto title = r.removeFromTop (44);
    titleRect = title;
    stopBtn.setBounds (title.removeFromRight (90).withSizeKeepingCentre (90, 34));
    title.removeFromRight (8);
    midiBtn.setBounds (title.removeFromRight (110).withSizeKeepingCentre (110, 30));
    kitBox.setBounds (titleRect.getX() + 230, titleRect.getY() + 8, 190, 28);
    playTop.setBounds (titleRect.getX() + 430, titleRect.getY() + 5, 60, 34);
    stopTop.setBounds (titleRect.getX() + 494, titleRect.getY() + 5, 60, 34);
    bannerRect = { getWidth() / 2 - 300, 4, 600, 32 };

    auto main = r.removeFromTop (380);
    pads.setBounds (main.removeFromLeft (380));
    main.removeFromLeft (20);
    auto tabs = main.removeFromTop (28);
    editTab.setBounds (tabs.removeFromLeft (90));   tabs.removeFromLeft (6);
    browseTab.setBounds (tabs.removeFromLeft (100));
    main.removeFromTop (4);
    browser.setBounds (main);

    auto g = main;
    wave.setBounds (g.removeFromTop (118));
    g.removeFromTop (6);
    auto row1 = g.removeFromTop (28);
    loadBtn.setBounds (row1.removeFromLeft (72));        row1.removeFromLeft (4);
    clearBtn.setBounds (row1.removeFromLeft (80));       row1.removeFromLeft (4);
    clearChopsBtn.setBounds (row1.removeFromLeft (100)); row1.removeFromLeft (4);
    fullBtn.setBounds (row1.removeFromLeft (84));
    g.removeFromTop (4);
    auto row2 = g.removeFromTop (28);
    chop4.setBounds (row2.removeFromLeft (70));     row2.removeFromLeft (4);
    chop8.setBounds (row2.removeFromLeft (70));     row2.removeFromLeft (4);
    chop16.setBounds (row2.removeFromLeft (76));    row2.removeFromLeft (4);
    chopHits.setBounds (row2.removeFromLeft (110));
    g.removeFromTop (4);
    auto row3 = g.removeFromTop (28);
    modeBtn.setBounds (row3.removeFromLeft (120));  row3.removeFromLeft (4);
    keysBtn.setBounds (row3.removeFromLeft (70));
    g.removeFromTop (6);
    knobPanel = g.removeFromTop (100);
    auto k = knobPanel.reduced (6, 6);
    const int kw = k.getWidth() / 4;
    for (auto* c : { &level, &pan, &tune, &choke }) c->setBounds (k.removeFromLeft (kw).withSizeKeepingCentre (84, k.getHeight()));

    r.removeFromTop (10);
    auto dtabs = r.removeFromTop (34);
    keysTab.setBounds (dtabs.removeFromLeft (90));   dtabs.removeFromLeft (6);
    seqTab.setBounds (dtabs.removeFromLeft (110));   dtabs.removeFromLeft (6);
    shapeTab.setBounds (dtabs.removeFromLeft (90));
    r.removeFromTop (8);
    keyboard.setBounds (r);
    seq.setBounds (r);
    shape.setBounds (r);
}

void BeatStationEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);
    g.setColour (Theme::dim);
    g.setFont (juce::Font (11.f, juce::Font::bold));
    g.drawText ("BASEMENT INSTRUMENTS", titleRect.getX(), titleRect.getY() + 2, 220, 14, juce::Justification::centredLeft);
    g.setColour (Theme::amber);
    g.setFont (juce::Font (16.f, juce::Font::bold));
    g.drawText ("BEAT STATION", titleRect.getX(), titleRect.getY() + 16, 220, 22, juce::Justification::centredLeft);

    g.setColour (Theme::text);
    g.setFont (juce::Font (13.f, juce::Font::bold));
    g.drawText (bpmText, titleRect.getX() + 560, titleRect.getY() + 8, 90, 28, juce::Justification::centredLeft);

    if (isStandalone())
    {
        g.setColour (Theme::dim);
        g.setFont (12.f);
        g.drawFittedText ("MIDI in: " + (proc.midiNames.isEmpty() ? juce::String ("none found") : proc.midiNames.joinIntoString (", ")),
                          titleRect.getX() + 650, titleRect.getY() + 8, titleRect.getWidth() - 650 - 210, 28, juce::Justification::centredRight, 1);
    }

    if (rightMode == 0)
    {
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (knobPanel.toFloat(), 8.f);
        g.setColour (Theme::grid.brighter (0.1f));
        g.drawRoundedRectangle (knobPanel.toFloat().reduced (0.5f), 8.f, 1.f);
        const auto info = proc.getPad (selPad);
        g.setColour (Theme::dim);
        g.setFont (12.f);
        g.drawText ("PAD " + juce::String (selPad + 1) + (info.sample != nullptr ? "  -  " + info.sample->name : "  -  empty") + "     Choke 0 = off.  Right-click a knob or pad for MIDI Learn.",
                    knobPanel.getX(), knobPanel.getBottom() + 4, 500, 16, juce::Justification::left);
    }

    const auto learn = proc.getLearnTarget();
    if (learn.isNotEmpty())
    {
        g.setColour (Theme::red);
        g.fillRoundedRectangle (bannerRect.toFloat(), 8.f);
        g.setColour (juce::Colour (0xff17191d));
        g.setFont (juce::Font (13.f, juce::Font::bold));
        const juce::String what = learn.startsWith ("pad:") ? "press a pad or key for pad " + juce::String (learn.fromFirstOccurrenceOf (":", false, false).getIntValue() + 1)
                                : learn.startsWith ("act:") ? "press a button or key for " + learn.fromFirstOccurrenceOf (":", false, false).toUpperCase()
                                                            : "move a knob or fader on your controller";
        g.drawText ("MIDI LEARN: " + what + "   (click here or press Esc to cancel)", bannerRect, juce::Justification::centred);
    }
}
