#pragma once
#include "PluginProcessor.h"
#include "VintageLookAndFeel.h"
#include "Widgets.h"
#include "PadGrid.h"
#include "WaveformView.h"
#include "SeqPanel.h"
#include "ShapePanel.h"
#include "FxPanel.h"
#include "KeyboardPanel.h"
#include "Meters.h"
#include "SampleBrowser.h"

class BeatStationEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit BeatStationEditor (BeatStationProcessor&);
    ~BeatStationEditor() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void selectPad (int pad);
    void refreshAfterEdit();
    void updateButtons();
    void updateDrawer();
    void updateRightMode();
    void updateView();
    void showFileMenu();
    void showLearnMenu (const juce::String& target);
    void timerCallback() override;
    bool isStandalone() const { return proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone; }

    BeatStationProcessor& proc;
    VintageLookAndFeel lnf;
    MeterStrip inMeter, outMeter;
    PadGrid pads;
    WaveformView wave;
    SampleBrowser browser;
    SequencerPanel seq;
    ShapePanel shape;
    KeyboardPanel keyboard;
    FxPanel fx;

    juce::TextButton loadBtn { "Load..." }, clearBtn { "Clear Pad" }, clearChopsBtn { "Clear Chops" }, fullBtn { "Play Full" },
                     modeBtn { "Mode: Cut" }, keysBtn { "Keys" },
                     chop4 { "Chop 4" }, chop8 { "Chop 8" }, chop16 { "Chop 16" }, chopHits { "Chop at hits" },
                     stopBtn { "STOP" }, midiBtn { "MIDI" }, fxBtn { "FX" }, fileBtn { "File" }, playTop { "Play" }, stopTop { "Stop" },
                     editTab { "EDIT" }, browseTab { "BROWSE" }, keysTab { "KEYS" }, seqTab { "STEP SEQ" }, shapeTab { "SHAPE" };
    LabeledKnob level { "LEVEL", " dB", Theme::amber }, pan { "PAN", "", Theme::green }, tune { "TUNE", " st", Theme::amber },
                choke { "CHOKE", "", Theme::amber };
    juce::ComboBox kitBox;
    juce::StringArray kitKeys;
    std::unique_ptr<juce::FileChooser> chooser;
    int selPad = 0, drawer = 1, rightMode = 0, lastMapVersion = -1;   // drawer: 0 keys, 1 step seq, 2 shape.  rightMode: 0 edit, 1 browse
    bool wasLearning = false, fxMode = false;
    int lastMidiSeen = 0;
    juce::String bpmText;
    juce::Rectangle<int> knobPanel, titleRect, bannerRect;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeatStationEditor)
};
