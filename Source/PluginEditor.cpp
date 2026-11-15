#include "PluginEditor.h"

BeatStationEditor::BeatStationEditor (BeatStationProcessor& p)
    : AudioProcessorEditor (&p), proc (p), inMeter (p, 0, "IN"), outMeter (p, 1, "OUT"), pads (p), wave (p), browser (p), seq (p), shape (p),
      keyboard (p), fx (p), turntable (p)
{
    setLookAndFeel (&lnf);
    for (auto* c : std::initializer_list<juce::Component*> { &inMeter, &outMeter, &pads, &wave, &browser, &seq, &shape, &keyboard, &fx, &turntable, &padsModeBtn, &deckModeBtn, &padSrcBtn, &fxBtn, &fileBtn,
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

    // KITS / INSTR: what the pads play. KITS = their samples. INSTR = the chosen instrument, 16 notes going up from C3.
    padInstBox.addItemList (keysInstrumentNames(), 1);
    padInstBox.setColour (juce::ComboBox::backgroundColourId, Theme::panel);
    padInstBox.setColour (juce::ComboBox::textColourId, Theme::text);
    padInstBox.setColour (juce::ComboBox::outlineColourId, Theme::grid);
    padInstBox.setColour (juce::ComboBox::arrowColourId, Theme::amber);
    padInstAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ParamID::keysInst, padInstBox);
    addChildComponent (padInstBox);
    padSrcBtn.setColour (juce::TextButton::textColourOffId, Theme::amber);
    padSrcBtn.onClick = [this] { proc.padsInst = ! proc.padsInst.load(); syncPadSource(); };
    keyboard.onKitLoaded = [this] { refreshAfterEdit(); };
    inMeter.onRecorded = [this] { refreshAfterEdit(); };
    syncPadSource();

    pads.onSelect = [this] (int i) { selectPad (i); };
    pads.onSamplesChanged = [this] { refreshAfterEdit(); };
    pads.onRightClick = [this] (int i) { showLearnMenu ("pad:" + juce::String (i)); };

    // MIDI Learn: right-click any knob, pad, or transport button
    // the pad knobs follow whichever pad is selected, so a MIDI-learned knob always moves the pad you are looking at
    level.onLearnMenu = [this] (const juce::String&) { showLearnMenu ("sel:" + juce::String (pLevel)); };
    pan.onLearnMenu   = [this] (const juce::String&) { showLearnMenu ("sel:" + juce::String (pPan)); };
    tune.onLearnMenu  = [this] (const juce::String&) { showLearnMenu ("sel:" + juce::String (pTune)); };
    choke.onLearnMenu = [this] (const juce::String&) { showLearnMenu ("sel:" + juce::String (pGroup)); };
    shape.setLearnCallback ([this] (const juce::String& t) { showLearnMenu (t); });
    fx.setLearnCallback ([this] (const juce::String& t) { showLearnMenu (t); });
    seq.onLoaded = [this] (int pad) { refreshAfterEdit(); selectPad (pad); };
    browser.onLoadToPad = [this] (const juce::File& f, int pad) { proc.loadSample (pad, f); selectPad (pad); refreshAfterEdit(); };
    fx.onBack = [this] { fxMode = false; updateView(); };
    fxBtn.onClick = [this] { fxMode = ! fxMode; updateView(); };
    padsModeBtn.onClick = [this] { deckMode = false; fxMode = false; updateView(); };
    deckModeBtn.onClick = [this] { deckMode = true;  fxMode = false; updateView(); };
    fileBtn.onClick = [this] { showFileMenu(); };
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
    updateView();
    setWantsKeyboardFocus (true);
    setSize (1132, 730);
    startTimerHz (15);
}

void BeatStationEditor::syncPadSource()
{
    const int inst = proc.padsInst.load() ? 1 : 0;
    if (inst == shownPadsInst) return;
    shownPadsInst = inst;
    padSrcBtn.setButtonText (inst ? "INSTR" : "KITS");
    kitBox.setVisible (! inst);
    padInstBox.setVisible (inst);
}

void BeatStationEditor::updateRightMode()
{
    const bool edit = rightMode == 0;
    const bool padView = ! deckMode;
    for (auto* c : std::initializer_list<juce::Component*> { &wave, &loadBtn, &clearBtn, &clearChopsBtn, &fullBtn, &modeBtn, &keysBtn,
                                                              &chop4, &chop8, &chop16, &chopHits, &level, &pan, &tune, &choke })
        c->setVisible (edit && ! fxMode && padView);
    browser.setVisible (! edit && ! fxMode && padView);
    editTab.setToggleState (edit, juce::dontSendNotification);
    browseTab.setToggleState (! edit, juce::dontSendNotification);
    repaint();
}

void BeatStationEditor::updateView()
{
    for (auto* c : std::initializer_list<juce::Component*> { &pads, &editTab, &browseTab }) c->setVisible (! fxMode && ! deckMode);
    for (auto* c : std::initializer_list<juce::Component*> { &keysTab, &seqTab, &shapeTab }) c->setVisible (! fxMode);
    turntable.setVisible (deckMode && ! fxMode);
    padsModeBtn.setToggleState (! deckMode, juce::dontSendNotification);
    deckModeBtn.setToggleState (deckMode, juce::dontSendNotification);
    fx.setVisible (fxMode);
    fxBtn.setToggleState (fxMode, juce::dontSendNotification);
    updateRightMode();
    updateDrawer();
    repaint();
}

void BeatStationEditor::showFileMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Save Project...");
    m.addItem (2, "Open Project...");
    juce::Component::SafePointer<BeatStationEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fileBtn), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto* ed = safe.getComponent();
        if (r == 1)
        {
            ed->chooser = std::make_unique<juce::FileChooser> ("Save project", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Untitled.bstation"), "*.bstation");
            ed->chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                                      [safe] (const juce::FileChooser& fc)
                                      {
                                          if (safe == nullptr || fc.getResult() == juce::File()) return;
                                          juce::MemoryBlock mb;
                                          safe->proc.getStateInformation (mb);
                                          fc.getResult().withFileExtension ("bstation").replaceWithData (mb.getData(), mb.getSize());
                                      });
        }
        else
        {
            ed->chooser = std::make_unique<juce::FileChooser> ("Open project", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.bstation");
            ed->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                      [safe] (const juce::FileChooser& fc)
                                      {
                                          if (safe == nullptr || ! fc.getResult().existsAsFile()) return;
                                          juce::MemoryBlock mb;
                                          if (! fc.getResult().loadFileAsData (mb)) return;
                                          safe->proc.setStateInformation (mb.getData(), (int) mb.getSize());
                                          safe->selectPad (safe->selPad);
                                          safe->refreshAfterEdit();
                                      });
        }
    });
}

void BeatStationEditor::updateDrawer()
{
    keyboard.setVisible (drawer == 0 && ! fxMode);
    seq.setVisible (drawer == 1 && ! fxMode);
    shape.setVisible (drawer == 2 && ! fxMode);
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
    turntable.setSelectedPad (i);
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
    if (isPad) m.addItem (3, "Load sound...");                         // right-click a pad to load a sound into it
    juce::Component::SafePointer<BeatStationEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, target] (int r)
    {
        if (safe == nullptr) return;
        if (r == 1) safe->proc.startLearn (target);
        else if (r == 2) safe->proc.clearMapping (target);
        else if (r == 3)
        {
            const int pad = target.fromFirstOccurrenceOf (":", false, false).getIntValue();
            safe->chooser = std::make_unique<juce::FileChooser> ("Choose a sample for pad " + juce::String (pad + 1), juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
            safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                        [safe, pad] (const juce::FileChooser& fc)
                                        {
                                            if (safe == nullptr || ! fc.getResult().existsAsFile()) return;
                                            safe->proc.loadSample (pad, fc.getResult());
                                            safe->selectPad (pad);
                                            safe->refreshAfterEdit();
                                        });
        }
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
    keyboard.sync();
    syncPadSource();
    const bool learning = proc.getLearnTarget().isNotEmpty();
    const int version = proc.mapVersion.load(), lm = proc.lastMidi.load();
    const bool playing = proc.playing.load();
    if (learning != wasLearning || version != lastMapVersion || lm != lastMidiSeen || playTop.getToggleState() != playing)
    {
        wasLearning = learning;
        lastMapVersion = version;
        lastMidiSeen = lm;
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
    title.removeFromRight (6);
    fileBtn.setBounds (title.removeFromRight (56).withSizeKeepingCentre (56, 30));
    title.removeFromRight (4);
    midiBtn.setBounds (title.removeFromRight (60).withSizeKeepingCentre (60, 30));
    padSrcBtn.setBounds (titleRect.getX() + 150, titleRect.getY() + 8, 56, 28);
    kitBox.setBounds (titleRect.getX() + 210, titleRect.getY() + 8, 150, 28);
    padInstBox.setBounds (kitBox.getBounds());
    playTop.setBounds (titleRect.getX() + 368, titleRect.getY() + 5, 52, 34);
    stopTop.setBounds (titleRect.getX() + 424, titleRect.getY() + 5, 52, 34);
    fxBtn.setBounds (titleRect.getX() + 484, titleRect.getY() + 5, 44, 34);
    padsModeBtn.setBounds (titleRect.getX() + 540, titleRect.getY() + 5, 56, 34);
    deckModeBtn.setBounds (titleRect.getX() + 600, titleRect.getY() + 5, 56, 34);
    fx.setBounds (r);                                                   // the master EQ/compressor window covers everything under the title
    bannerRect = { getWidth() / 2 - 300, 4, 600, 32 };

    auto main = r.removeFromTop (380);
    turntable.setBounds (main);
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
    g.drawText ("BASEMENT INSTRUMENTS", titleRect.getX(), titleRect.getY() + 2, 148, 14, juce::Justification::centredLeft);
    g.setColour (Theme::amber);
    g.setFont (juce::Font (16.f, juce::Font::bold));
    g.drawText ("BEAT STATION", titleRect.getX(), titleRect.getY() + 16, 148, 22, juce::Justification::centredLeft);

    {
        g.setColour (Theme::dim);
        g.setFont (11.f);
        const int lm = proc.lastMidi.load();
        juce::String last = "none yet";
        if (lm != 0) last = ((lm >> 24) == 1 ? "CC " : "Note ") + juce::String ((lm >> 8) & 255) + " ch" + juce::String ((lm >> 16) & 255) + " = " + juce::String (lm & 255);
        const juce::String devices = isStandalone() ? (proc.midiNames.isEmpty() ? juce::String ("MIDI: none found") : "MIDI: " + proc.midiNames.joinIntoString (", "))
                                                    : juce::String ("MIDI from the host");
        g.drawFittedText (devices, titleRect.getX() + 664, titleRect.getY() + 4, 126, 16, juce::Justification::centredLeft, 1);
        g.drawFittedText ("last: " + last, titleRect.getX() + 664, titleRect.getY() + 22, 126, 16, juce::Justification::centredLeft, 1);
    }

    if (rightMode == 0 && ! fxMode && ! deckMode)
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
