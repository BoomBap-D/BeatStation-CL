#pragma once
#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

// A slider that tells its owner when it is right-clicked (used for MIDI Learn).
class LearnSlider : public juce::Slider
{
public:
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
        juce::Slider::mouseDown (e);
    }
};

// A knob with its name and live value underneath, bound to a parameter. Right-click it for MIDI Learn.
class LabeledKnob : public juce::Component
{
public:
    std::function<void (const juce::String& paramId)> onLearnMenu;

    LabeledKnob (const juce::String& title, const juce::String& unitSuffix, juce::Colour accent) : suffix (unitSuffix)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.onValueChange = [this] { refresh(); };
        slider.onRightClick = [this] { if (onLearnMenu && paramId.isNotEmpty()) onLearnMenu (paramId); };

        name.setText (title, juce::dontSendNotification);
        name.setJustificationType (juce::Justification::centred);
        name.setFont (juce::Font (11.f, juce::Font::bold));
        name.setColour (juce::Label::textColourId, Theme::dim);
        name.setInterceptsMouseClicks (false, false);
        value.setJustificationType (juce::Justification::centred);
        value.setFont (juce::Font (13.f, juce::Font::bold));
        value.setColour (juce::Label::textColourId, Theme::text);
        value.setInterceptsMouseClicks (false, false);

        addAndMakeVisible (slider);
        addAndMakeVisible (name);
        addAndMakeVisible (value);
    }

    void attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
    {
        attachment.reset();
        paramId = id;
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider);
        // a knob whose range is the same either side of zero (pan, tune, gain) lights outwards from the centre
        slider.getProperties().set ("bipolar", slider.getMinimum() < 0.0 && std::abs (slider.getMinimum() + slider.getMaximum()) < 1.0e-6);
        refresh();
    }

    void setAccent (juce::Colour c) { slider.setColour (juce::Slider::rotarySliderFillColourId, c); slider.repaint(); }   // the colour of the lit ticks

    void resized() override
    {
        auto b = getLocalBounds();
        name.setBounds (b.removeFromBottom (14));
        value.setBounds (b.removeFromBottom (16));
        slider.setBounds (b);
    }

private:
    void refresh() { value.setText (slider.getTextFromValue (slider.getValue()) + suffix, juce::dontSendNotification); }

    LearnSlider slider;
    juce::Label name, value;
    juce::String suffix, paramId;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};
