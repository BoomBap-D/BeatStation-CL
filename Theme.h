#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Charcoal + amber palette. Change colours here and the whole UI follows.
namespace Theme
{
    inline const juce::Colour bg      { 0xff17191d };
    inline const juce::Colour panel   { 0xff1f2228 };
    inline const juce::Colour graphBg { 0xff121418 };
    inline const juce::Colour grid    { 0xff2a2e36 };
    inline const juce::Colour tickOff { 0xff3a3f4a };
    inline const juce::Colour text    { 0xffe8ecf2 };
    inline const juce::Colour dim     { 0xff9aa3b2 };
    inline const juce::Colour amber   { 0xfff5a524 };
    inline const juce::Colour green   { 0xff6fd36f };
    inline const juce::Colour red     { 0xffe0568c };
    inline const juce::Colour cream   { 0xffece0c2 };
    inline const juce::Colour bandColours[5] = { juce::Colour (0xffe0568c), juce::Colour (0xfff5a524), juce::Colour (0xff3ad0e0),
                                                 juce::Colour (0xff8f7cf0), juce::Colour (0xff6fd36f) };
}
