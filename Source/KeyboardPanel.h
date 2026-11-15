#pragma once
#include "PluginProcessor.h"
#include "Theme.h"
#include <juce_audio_utils/juce_audio_utils.h>

// A big playable keyboard that shows two octaves at a time (full-size keys), with arrows and a slider to reach all 88 keys.
class KeyboardPanel : public juce::Component
{
public:
    explicit KeyboardPanel (BeatStationProcessor& p) : proc (p), keys (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        instBox.addItemList (keysInstrumentNames(), 1);
        instBox.setColour (juce::ComboBox::backgroundColourId, Theme::panel);
        instBox.setColour (juce::ComboBox::textColourId, Theme::text);
        instBox.setColour (juce::ComboBox::outlineColourId, Theme::grid);
        instBox.setColour (juce::ComboBox::arrowColourId, Theme::amber);
        instAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ParamID::keysInst, instBox);
        instBox.onChange = [this]                                       // choosing an instrument also switches Keys on, so you hear it
        {
            if (! isShowing() || proc.padsInst.load()) return;          // not when a project loads, or when the pads are the ones playing the instrument
            if (auto* prm = proc.apvts.getParameter (ParamID::keysMode)) prm->setValueNotifyingHost (1.f);
        };
        keysOn.setClickingTogglesState (true);
        keysOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamID::keysMode, keysOn);
        addAndMakeVisible (instBox);
        addAndMakeVisible (keysOn);

        // INSTRUMENT / DRUM SET: click the word to switch what the keyboard plays
        kitKeys = BeatStationProcessor::getKitKeys();
        for (int i = 0; i < kitKeys.size(); ++i) kitBox.addItem (BeatStationProcessor::kitDisplayName (kitKeys[i]), i + 1);
        kitBox.setTextWhenNothingSelected ("Choose a drum set...");
        kitBox.setColour (juce::ComboBox::backgroundColourId, Theme::panel);
        kitBox.setColour (juce::ComboBox::textColourId, Theme::text);
        kitBox.setColour (juce::ComboBox::outlineColourId, Theme::grid);
        kitBox.setColour (juce::ComboBox::arrowColourId, Theme::amber);
        kitBox.onChange = [this]
        {
            const int i = kitBox.getSelectedItemIndex();
            if (i >= 0) { proc.loadKit (kitKeys[i]); if (onKitLoaded) onKitLoaded(); }
        };
        srcBtn.setColour (juce::TextButton::textColourOffId, Theme::amber);
        srcBtn.onClick = [this]
        {
            proc.keysDrums = ! proc.keysDrums.load();
            if (auto* prm = proc.apvts.getParameter (ParamID::keysMode)) prm->setValueNotifyingHost (1.f);   // so you hear it straight away
            sync();
        };
        addAndMakeVisible (srcBtn);
        // REC TO STEPS: records what you play here into the step sequencer. It switches Keys on and starts the beat if it is stopped.
        recBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd0221f));
        recBtn.onClick = [this]
        {
            const bool on = ! proc.recording.load();
            proc.recording = on;
            if (on)
            {
                if (auto* prm = proc.apvts.getParameter (ParamID::keysMode)) prm->setValueNotifyingHost (1.f);
                if (! proc.playing.load()) proc.startPlaying();
            }
            shownRec = -1;
            sync();
        };
        addAndMakeVisible (recBtn);
        addChildComponent (kitBox);
        sync();

        keys.setAvailableRange (21, 108);                              // A0 to C8 = 88 keys
        keys.setScrollButtonWidth (0);                                 // we draw our own arrows and slider
        keys.setMidiChannel (1);
        keys.setOctaveForMiddleC (3);
        keys.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xff3b3f48));     // charcoal-gray keys
        keys.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff0e1013));
        keys.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff17191d));
        keys.setColour (juce::MidiKeyboardComponent::textLabelColourId, Theme::cream);
        keys.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Theme::amber.withAlpha (0.85f));
        keys.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, Theme::amber.withAlpha (0.25f));
        addAndMakeVisible (keys);

        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange (21.0, 85.0, 1.0);
        slider.setColour (juce::Slider::trackColourId, Theme::amber.withAlpha (0.5f));
        slider.setColour (juce::Slider::thumbColourId, Theme::amber);
        slider.onValueChange = [this] { keys.setLowestVisibleKey ((int) slider.getValue()); };
        addAndMakeVisible (slider);

        left.onClick = [this] { slider.setValue (juce::jmax (21.0, slider.getValue() - 12.0)); };
        right.onClick = [this] { slider.setValue (juce::jmin (85.0, slider.getValue() + 12.0)); };
        addAndMakeVisible (left);
        addAndMakeVisible (right);
        slider.setValue (36.0, juce::sendNotificationSync);            // starts on the pad keys (36-51)
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (30);
        srcBtn.setBounds (top.removeFromLeft (104));   top.removeFromLeft (8);
        const auto box = top.removeFromLeft (260);     top.removeFromLeft (8);
        instBox.setBounds (box);
        kitBox.setBounds (box);
        keysOn.setBounds (top.removeFromLeft (90));
        top.removeFromLeft (8);
        recBtn.setBounds (top.removeFromLeft (120));
        r.removeFromTop (4);
        auto bottom = r.removeFromBottom (26);
        left.setBounds (bottom.removeFromLeft (40));
        right.setBounds (bottom.removeFromRight (40));
        slider.setBounds (bottom.reduced (6, 0));
        r.removeFromBottom (4);
        keys.setBounds (r);
        keys.setKeyWidth (juce::jmax (24.f, (float) r.getWidth() / 14.f));   // 14 white keys = two octaves
    }

    std::function<void()> onKitLoaded;

    void sync()                                                         // also called by the editor's timer, so a loaded project shows correctly
    {
        const bool drums = proc.keysDrums.load();
        if ((int) proc.recording.load() != shownRec) { shownRec = (int) proc.recording.load(); recBtn.setToggleState (shownRec == 1, juce::dontSendNotification); }
        if ((int) drums == shownDrums) return;
        shownDrums = (int) drums;
        srcBtn.setButtonText (drums ? "DRUM SET" : "INSTRUMENT");
        instBox.setVisible (! drums);
        kitBox.setVisible (drums);
    }

private:
    BeatStationProcessor& proc;
    juce::ComboBox instBox, kitBox;
    juce::StringArray kitKeys;
    juce::TextButton keysOn { "Keys ON" }, srcBtn { "INSTRUMENT" };
    int shownDrums = -1, shownRec = -1;
    juce::TextButton recBtn { "REC TO STEPS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> instAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> keysOnAtt;
    juce::MidiKeyboardComponent keys;
    juce::Slider slider;
    juce::TextButton left { "<" }, right { ">" };
};
