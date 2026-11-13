#pragma once
#include "PluginProcessor.h"
#include "Theme.h"

// Search a huge sound library, audition sounds before loading them, then load into the selected pad (double-click, Enter,
// the Load button) or drag a row onto any pad.
class SampleBrowser : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    std::function<void (const juce::File&)> onLoad;                    // load this file into the selected pad

    explicit SampleBrowser (BeatStationProcessor& p) : proc (p)
    {
        search.setTextToShowWhenEmpty ("Search sounds, kits, folders...  (e.g. snare dusty)", Theme::dim);
        search.setColour (juce::TextEditor::backgroundColourId, Theme::graphBg);
        search.setColour (juce::TextEditor::textColourId, Theme::text);
        search.setColour (juce::TextEditor::outlineColourId, Theme::grid);
        search.setColour (juce::TextEditor::focusedOutlineColourId, Theme::amber);
        search.onTextChange = [this] { searchDirty = true; };
        list.setModel (this);
        list.setRowHeight (40);
        list.setColour (juce::ListBox::backgroundColourId, Theme::graphBg);
        list.setColour (juce::ListBox::outlineColourId, Theme::grid);
        list.setOutlineThickness (1);

        autoPlay.setToggleState (true, juce::dontSendNotification);
        advance.setToggleState (true, juce::dontSendNotification);
        for (auto* t : { &autoPlay, &advance })
        {
            t->setColour (juce::ToggleButton::textColourId, Theme::text);
            t->setColour (juce::ToggleButton::tickColourId, Theme::amber);
            t->setColour (juce::ToggleButton::tickDisabledColourId, Theme::dim);
        }
        volume.setSliderStyle (juce::Slider::LinearHorizontal);
        volume.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        volume.setRange (0.0, 1.2, 0.01);
        volume.setValue (0.8, juce::dontSendNotification);
        volume.setColour (juce::Slider::trackColourId, Theme::amber.withAlpha (0.5f));
        volume.setColour (juce::Slider::thumbColourId, Theme::amber);
        volume.onValueChange = [this] { proc.previewGain = (float) volume.getValue(); };
        status.setColour (juce::Label::textColourId, Theme::dim);
        status.setFont (12.f);
        info.setColour (juce::Label::textColourId, Theme::text);
        info.setFont (12.f);

        foldersBtn.onClick = [this] { showFolderMenu(); };
        rescanBtn.onClick = [this] { proc.library.rescan(); };
        stopBtn.onClick = [this] { proc.stopPreview(); };
        loadBtn.onClick = [this] { loadSelected(); };

        for (auto* c : std::initializer_list<juce::Component*> { &search, &foldersBtn, &rescanBtn, &autoPlay, &advance, &volume, &stopBtn, &list, &status, &info, &loadBtn })
            addAndMakeVisible (*c);
        setWantsKeyboardFocus (true);
        startTimerHz (10);
        searchDirty = true;
    }

    ~SampleBrowser() override { list.setModel (nullptr); }

    void setTargetPad (int pad) { targetPad = pad; loadBtn.setButtonText ("Load to Pad " + juce::String (pad + 1)); }
    bool advanceAfterLoad() const { return advance.getToggleState(); }

    void resized() override
    {
        auto r = getLocalBounds();
        auto row1 = r.removeFromTop (30);
        rescanBtn.setBounds (row1.removeFromRight (64));  row1.removeFromRight (4);
        foldersBtn.setBounds (row1.removeFromRight (84)); row1.removeFromRight (6);
        search.setBounds (row1);
        r.removeFromTop (6);
        auto row2 = r.removeFromTop (26);
        autoPlay.setBounds (row2.removeFromLeft (92));
        advance.setBounds (row2.removeFromLeft (150));
        stopBtn.setBounds (row2.removeFromRight (56));    row2.removeFromRight (6);
        volume.setBounds (row2.removeFromRight (110));
        r.removeFromTop (6);
        auto bottom = r.removeFromBottom (64);
        r.removeFromBottom (6);
        list.setBounds (r);
        loadBtn.setBounds (bottom.removeFromBottom (30));
        info.setBounds (bottom.removeFromTop (18));
        status.setBounds (bottom);
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::spaceKey) { previewRow (list.getSelectedRow()); return true; }
        if (k == juce::KeyPress::escapeKey) { proc.stopPreview(); return true; }
        return false;
    }

private:
    int getNumRows() override { return results.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, results.size())) return;
        if (selected) { g.setColour (juce::Colour (0xff3a2a0a)); g.fillRect (0, 0, width, height); g.setColour (Theme::amber); g.fillRect (0, 0, 3, height); }
        const juce::File f (results[row]);
        g.setColour (Theme::text);
        g.setFont (juce::Font (13.f, juce::Font::bold));
        g.drawText (f.getFileName(), 10, 3, width - 16, 18, juce::Justification::left);
        g.setColour (Theme::dim);
        g.setFont (11.f);
        g.drawText (f.getParentDirectory().getFullPathName(), 10, 21, width - 16, 15, juce::Justification::centredLeft);   // keeps the end of long paths visible
        g.setColour (Theme::grid);
        g.drawHorizontalLine (height - 1, 0.f, (float) width);
    }

    void selectedRowsChanged (int row) override
    {
        if (juce::isPositiveAndBelow (row, results.size()))
        {
            info.setText (juce::File (results[row]).getFileName(), juce::dontSendNotification);
            if (autoPlay.getToggleState()) previewRow (row);
        }
    }

    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        if (! juce::isPositiveAndBelow (row, results.size())) return;
        const juce::File f (results[row]);
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            m.addItem (1, "Show in folder");
            m.showMenuAsync (juce::PopupMenu::Options(), [f] (int r) { if (r == 1) f.revealToUser(); });
        }
        else if (row == lastPreviewed) previewRow (row);               // clicking the selected row again plays it again
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { loadRow (row); }
    void returnKeyPressed (int row) override { loadRow (row); }

    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override
    {
        if (rows.size() > 0 && juce::isPositiveAndBelow (rows[0], results.size())) return "sample:" + results[rows[0]];
        return {};
    }

    void previewRow (int row)
    {
        if (! juce::isPositiveAndBelow (row, results.size())) return;
        lastPreviewed = row;
        proc.previewFile (juce::File (results[row]));
    }

    void loadRow (int row)
    {
        if (! juce::isPositiveAndBelow (row, results.size()) || ! onLoad) return;
        onLoad (juce::File (results[row]));
    }

    void loadSelected() { loadRow (list.getSelectedRow()); }

    void showFolderMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Add folder...");
        m.addItem (2, "Rescan all folders");
        const auto folders = proc.library.getFolders();
        if (folders.size() > 0) m.addSeparator();
        for (int i = 0; i < folders.size(); ++i) m.addItem (100 + i, "Remove: " + folders[i]);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&foldersBtn), [this] (int r)
        {
            if (r == 1)
            {
                chooser = std::make_unique<juce::FileChooser> ("Choose a folder of sounds");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                      [this] (const juce::FileChooser& fc) { if (fc.getResult().isDirectory()) proc.library.addFolder (fc.getResult()); });
            }
            else if (r == 2) proc.library.rescan();
            else if (r >= 100) proc.library.removeFolder (r - 100);
        });
    }

    void refresh()
    {
        results = proc.library.search (search.getText(), 3000);
        lastPreviewed = -1;
        list.updateContent();
        list.repaint();
    }

    void timerCallback() override
    {
        const bool scanning = proc.library.isScanning();
        const int total = proc.library.size();
        if (searchDirty || (scanning && ++scanTick % 10 == 0) || (wasScanning && ! scanning))   // refresh while scanning, and again when it ends
        {
            searchDirty = false;
            refresh();
        }
        wasScanning = scanning;
        if (total == 0 && ! scanning)
            status.setText ("No sounds yet. Press Folders... and add a folder of samples.", juce::dontSendNotification);
        else
            status.setText ((scanning ? "Scanning...  " : "") + juce::String (total) + " sounds indexed, "
                              + juce::String (results.size()) + (results.size() >= 3000 ? "+ shown (type more words to narrow)" : " shown"),
                            juce::dontSendNotification);
    }

    BeatStationProcessor& proc;
    juce::TextEditor search;
    juce::ListBox list;
    juce::StringArray results;
    juce::TextButton foldersBtn { "Folders..." }, rescanBtn { "Rescan" }, stopBtn { "Stop" }, loadBtn { "Load to Pad 1" };
    juce::ToggleButton autoPlay { "Auto-play" }, advance { "Next pad after load" };
    juce::Slider volume;
    juce::Label status, info;
    std::unique_ptr<juce::FileChooser> chooser;
    int targetPad = 0, lastPreviewed = -1, scanTick = 0;
    bool searchDirty = false, wasScanning = false;
};
