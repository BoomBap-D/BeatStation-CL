#pragma once
#include "Theme.h"
#include <juce_audio_utils/juce_audio_utils.h>

// A big playable keyboard that shows two octaves at a time (full-size keys), with arrows and a slider to reach all 88 keys.
class KeyboardPanel : public juce::Component
{
public:
    explicit KeyboardPanel (juce::MidiKeyboardState& state) : keys (state, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
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
        auto bottom = r.removeFromBottom (26);
        left.setBounds (bottom.removeFromLeft (40));
        right.setBounds (bottom.removeFromRight (40));
        slider.setBounds (bottom.reduced (6, 0));
        r.removeFromBottom (4);
        keys.setBounds (r);
        keys.setKeyWidth (juce::jmax (24.f, (float) r.getWidth() / 14.f));   // 14 white keys = two octaves
    }

private:
    juce::MidiKeyboardComponent keys;
    juce::Slider slider;
    juce::TextButton left { "<" }, right { ">" };
};
