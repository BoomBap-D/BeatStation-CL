#pragma once
#include "PluginProcessor.h"
#include "Theme.h"

// Browse your sounds like a file manager: you see one folder at a time (open a folder to go into it), with Back / Forward / Up,
// favorites, and a search box that looks through everything you added. Audition before loading: click a sound or use the arrows.
// Load into the selected pad with double-click, Enter, Shift+click or the Load button, or drag a sound onto any pad or sequencer track.
class SampleBrowser : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    std::function<void (const juce::File&)> onLoad;                    // load this file into the selected pad
    std::function<void (const juce::File&, int pad)> onLoadToPad;      // load this file into a specific pad (right-click menu)

    explicit SampleBrowser (BeatStationProcessor& p) : proc (p)
    {
        search.setTextToShowWhenEmpty ("Search everything...  (e.g. snare dusty)", Theme::dim);
        search.setColour (juce::TextEditor::backgroundColourId, Theme::graphBg);
        search.setColour (juce::TextEditor::textColourId, Theme::text);
        search.setColour (juce::TextEditor::outlineColourId, Theme::grid);
        search.setColour (juce::TextEditor::focusedOutlineColourId, Theme::amber);
        search.onTextChange = [this] { searchDirty = true; };
        list.setModel (this);
        list.setRowHeight (36);
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

        backBtn.onClick = [this] { if (histPos > 0) { --histPos; showDir (history[(size_t) histPos]); } };
        fwdBtn.onClick = [this] { if (histPos + 1 < (int) history.size()) { ++histPos; showDir (history[(size_t) histPos]); } };
        upBtn.onClick = [this] { goUp(); };
        favBtn.onClick = [this] { showFavoritesMenu(); };
        foldersBtn.onClick = [this] { showFolderMenu(); };
        stopBtn.onClick = [this] { proc.stopPreview(); };
        loadBtn.onClick = [this] { loadRow (list.getSelectedRow()); };

        for (auto* c : std::initializer_list<juce::Component*> { &backBtn, &fwdBtn, &upBtn, &favBtn, &foldersBtn, &search, &stopBtn, &autoPlay, &advance, &volume, &list, &status, &loadBtn })
            addAndMakeVisible (*c);
        history.push_back (juce::File());
        histPos = 0;
        setWantsKeyboardFocus (true);
        startTimerHz (10);
        searchDirty = true;
    }

    ~SampleBrowser() override { list.setModel (nullptr); }

    void setTargetPad (int pad) { loadBtn.setButtonText ("Load to Pad " + juce::String (pad + 1)); }
    bool advanceAfterLoad() const { return advance.getToggleState(); }

    void resized() override
    {
        auto r = getLocalBounds();
        auto row1 = r.removeFromTop (28);
        backBtn.setBounds (row1.removeFromLeft (30));    row1.removeFromLeft (3);
        fwdBtn.setBounds (row1.removeFromLeft (30));     row1.removeFromLeft (3);
        upBtn.setBounds (row1.removeFromLeft (40));      row1.removeFromLeft (6);
        foldersBtn.setBounds (row1.removeFromRight (82)); row1.removeFromRight (4);
        favBtn.setBounds (row1.removeFromRight (84));    row1.removeFromRight (6);
        pathArea = row1;
        r.removeFromTop (4);
        auto row2 = r.removeFromTop (28);
        stopBtn.setBounds (row2.removeFromRight (56));   row2.removeFromRight (6);
        search.setBounds (row2);
        r.removeFromTop (3);
        auto row3 = r.removeFromTop (24);
        autoPlay.setBounds (row3.removeFromLeft (92));
        advance.setBounds (row3.removeFromLeft (150));
        volume.setBounds (row3);
        r.removeFromTop (4);
        auto bottom = r.removeFromBottom (48);
        r.removeFromBottom (4);
        list.setBounds (r);
        loadBtn.setBounds (bottom.removeFromBottom (28));
        status.setBounds (bottom);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::dim);
        g.setFont (12.f);
        juce::String text = currentDir == juce::File() ? "Your library  (add folders with Folders...)" : currentDir.getFullPathName();
        const int maxChars = juce::jmax (8, pathArea.getWidth() / 7);
        if (text.length() > maxChars) text = juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6")) + text.substring (text.length() - maxChars + 1);   // keep the end of the path visible
        g.drawText (text, pathArea, juce::Justification::centredLeft);
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::spaceKey) { previewRow (list.getSelectedRow()); return true; }
        if (k == juce::KeyPress::escapeKey) { proc.stopPreview(); return true; }
        if (k == juce::KeyPress::backspaceKey) { goUp(); return true; }
        return false;
    }

private:
    struct Entry { juce::File file; bool isDir; };

    static bool isAudio (const juce::File& f)
    {
        const auto e = f.getFileExtension().toLowerCase();
        return e == ".wav" || e == ".aif" || e == ".aiff" || e == ".flac" || e == ".mp3" || e == ".ogg";
    }

    // ---- navigation
    void showDir (const juce::File& dir)
    {
        currentDir = dir;
        search.setText ({}, false);
        searchDirty = true;
        repaint();
    }

    void goTo (const juce::File& dir)
    {
        if (dir == currentDir && search.isEmpty()) return;
        history.resize ((size_t) histPos + 1);                         // going somewhere new forgets the forward history
        history.push_back (dir);
        histPos = (int) history.size() - 1;
        showDir (dir);
    }

    void goUp()
    {
        if (currentDir == juce::File()) return;
        const auto roots = proc.library.getFolders();
        goTo (roots.contains (currentDir.getFullPathName()) ? juce::File() : currentDir.getParentDirectory());
    }

    // ---- the list
    int getNumRows() override { return entries.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, entries.size())) return;
        const auto& e = entries[row];
        if (selected) { g.setColour (juce::Colour (0xff3a2a0a)); g.fillRect (0, 0, width, height); g.setColour (Theme::amber); g.fillRect (0, 0, 3, height); }

        int x = 10;
        if (e.isDir)
        {
            juce::Path tri;                                            // a small folder arrow
            tri.addTriangle (12.f, (float) height * 0.5f - 5.f, 12.f, (float) height * 0.5f + 5.f, 20.f, (float) height * 0.5f);
            g.setColour (Theme::amber);
            g.fillPath (tri);
            x = 28;
        }
        g.setColour (Theme::text);
        g.setFont (juce::Font (13.f, e.isDir ? juce::Font::bold : juce::Font::plain));
        g.drawText (e.file.getFileName(), x, 2, width - x - 8, 18, juce::Justification::left);
        g.setColour (Theme::dim);
        g.setFont (11.f);
        const juce::String sub = e.isDir ? "folder"
                               : (inSearch ? e.file.getParentDirectory().getFullPathName()
                                           : e.file.getFileExtension().substring (1).toUpperCase() + "  -  " + juce::String (e.file.getSize() / 1024) + " KB");
        g.drawText (sub, x, 19, width - x - 8, 14, juce::Justification::centredLeft);
        g.setColour (Theme::grid);
        g.drawHorizontalLine (height - 1, 0.f, (float) width);
    }

    void selectedRowsChanged (int row) override
    {
        if (juce::isPositiveAndBelow (row, entries.size()) && ! entries[row].isDir && autoPlay.getToggleState()) previewRow (row);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        if (! juce::isPositiveAndBelow (row, entries.size())) return;
        const auto entry = entries[row];
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m, padMenu;
            if (! entry.isDir)
            {
                m.addItem (4, "Load to the selected pad");
                for (int i = 0; i < numPads; ++i) padMenu.addItem (200 + i, "Pad " + juce::String (i + 1));
                m.addSubMenu ("Load to pad...", padMenu);
                m.addSeparator();
            }
            m.addItem (1, entry.isDir ? "Add folder to favorites" : "Add sound to favorites");
            if (inSearch && ! entry.isDir) m.addItem (2, "Open its folder here");
            if (! entry.isDir) m.addItem (3, "Show in the system file browser");
            m.showMenuAsync (juce::PopupMenu::Options(), [this, entry] (int r)
            {
                if (r == 1) proc.library.addFavorite (entry.file.getFullPathName());
                else if (r == 2) goTo (entry.file.getParentDirectory());
                else if (r == 3) entry.file.revealToUser();
                else if (r == 4) { if (onLoad) onLoad (entry.file); }
                else if (r >= 200) { if (onLoadToPad) onLoadToPad (entry.file, r - 200); }
            });
        }
        else if (! entry.isDir && e.mods.isShiftDown()) loadRow (row);        // Shift+click loads
        else if (! entry.isDir && row == lastPreviewed) previewRow (row);      // clicking the selected sound again plays it again
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { activateRow (row); }
    void returnKeyPressed (int row) override { activateRow (row); }

    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override
    {
        if (rows.size() > 0 && juce::isPositiveAndBelow (rows[0], entries.size()) && ! entries[rows[0]].isDir) return "sample:" + entries[rows[0]].file.getFullPathName();
        return {};
    }

    void activateRow (int row)
    {
        if (! juce::isPositiveAndBelow (row, entries.size())) return;
        if (entries[row].isDir) goTo (entries[row].file); else loadRow (row);
    }

    void previewRow (int row)
    {
        if (! juce::isPositiveAndBelow (row, entries.size()) || entries[row].isDir) return;
        lastPreviewed = row;
        proc.previewFile (entries[row].file);
    }

    void loadRow (int row)
    {
        if (juce::isPositiveAndBelow (row, entries.size()) && ! entries[row].isDir && onLoad) onLoad (entries[row].file);
    }

    // ---- menus
    void showFavoritesMenu()
    {
        juce::PopupMenu m, remove;
        const auto favs = proc.library.getFavorites();
        const int sel = list.getSelectedRow();
        if (currentDir != juce::File()) m.addItem (1, "Add this folder");
        if (juce::isPositiveAndBelow (sel, entries.size()) && ! entries[sel].isDir) m.addItem (2, "Add the selected sound");
        if (favs.size() > 0) m.addSeparator();
        for (int i = 0; i < favs.size(); ++i)
        {
            const juce::File f (favs[i]);
            m.addItem (100 + i, juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85 ")) + f.getFileName() + (f.isDirectory() ? "  (folder)" : ""));
            remove.addItem (1000 + i, f.getFileName());
        }
        if (favs.size() > 0) m.addSubMenu ("Remove a favorite", remove);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&favBtn), [this, favs] (int r)
        {
            if (r == 1) proc.library.addFavorite (currentDir.getFullPathName());
            else if (r == 2) proc.library.addFavorite (entries[list.getSelectedRow()].file.getFullPathName());
            else if (r >= 1000) proc.library.removeFavorite (favs[r - 1000]);
            else if (r >= 100)
            {
                const juce::File f (favs[r - 100]);
                if (f.isDirectory()) goTo (f);
                else if (f.existsAsFile()) { pendingSelect = f; goTo (f.getParentDirectory()); }
            }
        });
    }

    void showFolderMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Add folder...");
        m.addItem (2, "Rescan the search index");
        const auto folders = proc.library.getFolders();
        if (folders.size() > 0) m.addSeparator();
        for (int i = 0; i < folders.size(); ++i) m.addItem (100 + i, "Remove: " + folders[i]);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&foldersBtn), [this] (int r)
        {
            if (r == 1)
            {
                chooser = std::make_unique<juce::FileChooser> ("Choose a folder of sounds");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                      [this] (const juce::FileChooser& fc) { if (fc.getResult().isDirectory()) { proc.library.addFolder (fc.getResult()); searchDirty = true; } });
            }
            else if (r == 2) proc.library.rescan();
            else if (r >= 100) { proc.library.removeFolder (r - 100); searchDirty = true; }
        });
    }

    // ---- rebuild the list: search results, the library roots, or the contents of the current folder (one level only)
    void refresh()
    {
        entries.clear();
        const auto query = search.getText().trim();
        inSearch = query.isNotEmpty();
        if (inSearch)
        {
            for (const auto& path : proc.library.search (query, 3000)) entries.add ({ juce::File (path), false });
        }
        else if (currentDir == juce::File())
        {
            for (const auto& path : proc.library.getFolders()) entries.add ({ juce::File (path), true });
        }
        else
        {
            std::vector<Entry> folders, files;
            for (const auto& item : juce::RangedDirectoryIterator (currentDir, false, "*", juce::File::findFilesAndDirectories))
            {
                if (item.isHidden()) continue;
                const auto f = item.getFile();
                if (item.isDirectory()) folders.push_back ({ f, true });
                else if (isAudio (f)) files.push_back ({ f, false });
            }
            auto byName = [] (const Entry& a, const Entry& b) { return a.file.getFileName().compareNatural (b.file.getFileName()) < 0; };
            std::sort (folders.begin(), folders.end(), byName);
            std::sort (files.begin(), files.end(), byName);
            for (auto& e : folders) entries.add (e);
            for (auto& e : files) entries.add (e);
        }
        lastPreviewed = -1;
        list.updateContent();
        list.deselectAllRows();
        list.scrollToEnsureRowIsOnscreen (0);
        if (pendingSelect != juce::File())                            // coming from a favorite sound: select it
        {
            for (int i = 0; i < entries.size(); ++i) if (entries[i].file == pendingSelect) { list.selectRow (i); break; }
            pendingSelect = juce::File();
        }
        list.repaint();
    }

    void timerCallback() override
    {
        const bool scanning = proc.library.isScanning();
        if (searchDirty || (scanning && inSearch && ++scanTick % 10 == 0) || (wasScanning && ! scanning && inSearch))
        {
            searchDirty = false;
            refresh();
        }
        wasScanning = scanning;
        const int total = proc.library.size();
        juce::String text;
        if (proc.library.getFolders().isEmpty()) text = "No sounds yet. Press Folders... and add a folder of samples.";
        else if (inSearch) text = juce::String (entries.size()) + (entries.size() >= 3000 ? "+" : "") + " results from " + juce::String (total) + " sounds" + (scanning ? "  (still scanning)" : "");
        else text = juce::String (entries.size()) + " items here  -  " + juce::String (total) + " sounds indexed for search" + (scanning ? "  (scanning)" : "");
        status.setText (text, juce::dontSendNotification);
    }

    BeatStationProcessor& proc;
    juce::TextEditor search;
    juce::ListBox list;
    juce::Array<Entry> entries;
    std::vector<juce::File> history;
    juce::File currentDir, pendingSelect;
    juce::Rectangle<int> pathArea;
    juce::TextButton backBtn { "<" }, fwdBtn { ">" }, upBtn { "Up" }, favBtn { "Favorites" }, foldersBtn { "Folders..." }, stopBtn { "Stop" }, loadBtn { "Load to Pad 1" };
    juce::ToggleButton autoPlay { "Auto-play" }, advance { "Next pad after load" };
    juce::Slider volume;
    juce::Label status;
    std::unique_ptr<juce::FileChooser> chooser;
    int histPos = 0, lastPreviewed = -1, scanTick = 0;
    bool searchDirty = false, wasScanning = false, inSearch = false;
};
