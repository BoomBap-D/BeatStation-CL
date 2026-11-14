#pragma once
#include <juce_core/juce_core.h>

// A searchable index of every audio file under the folders you add. It scans in the background, remembers the folders and
// the index between runs, and searches by typing several words (all must appear somewhere in the file path, so folder names
// like "808" or "snare" or "kits" count too). Built to cope with very large libraries.
class SampleLibrary : private juce::Thread
{
public:
    SampleLibrary() : juce::Thread ("Sample scanner")
    {
        dir().createDirectory();
        folders = juce::StringArray::fromLines (dir().getChildFile ("folders.txt").loadFileAsString());
        folders.removeEmptyStrings();
        files = juce::StringArray::fromLines (dir().getChildFile ("index.txt").loadFileAsString());
        files.removeEmptyStrings();
        favorites = juce::StringArray::fromLines (dir().getChildFile ("favorites.txt").loadFileAsString());
        favorites.removeEmptyStrings();
    }

    ~SampleLibrary() override { stopThread (5000); }

    juce::StringArray getFolders() const { const juce::ScopedLock sl (lock); return folders; }
    int size() const { const juce::ScopedLock sl (lock); return files.size(); }

    // Favorites: folders or single sounds you want to jump back to. Saved between runs.
    juce::StringArray getFavorites() const { const juce::ScopedLock sl (lock); return favorites; }
    void addFavorite (const juce::String& path) { { const juce::ScopedLock sl (lock); favorites.addIfNotAlreadyThere (path); } saveFavorites(); }
    void removeFavorite (const juce::String& path) { { const juce::ScopedLock sl (lock); favorites.removeString (path); } saveFavorites(); }
    bool isScanning() const { return isThreadRunning(); }

    void addFolder (const juce::File& f)
    {
        { const juce::ScopedLock sl (lock); folders.addIfNotAlreadyThere (f.getFullPathName()); }
        saveFolders();
        rescan();
    }

    void removeFolder (int index)
    {
        { const juce::ScopedLock sl (lock); if (juce::isPositiveAndBelow (index, folders.size())) folders.remove (index); }
        saveFolders();
        rescan();
    }

    void rescan() { stopThread (5000); startThread(); }

    juce::StringArray search (const juce::String& query, int maxResults) const
    {
        juce::StringArray out;
        const auto tokens = juce::StringArray::fromTokens (query.trim(), " ", "\"");
        const juce::ScopedLock sl (lock);
        for (const auto& f : files)
        {
            bool ok = true;
            for (const auto& t : tokens) if (! f.containsIgnoreCase (t)) { ok = false; break; }
            if (ok) { out.add (f); if (out.size() >= maxResults) break; }
        }
        return out;
    }

private:
    static juce::File dir()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Basement Instruments").getChildFile ("Beat Station");
    }

    void saveFavorites() { juce::StringArray f; { const juce::ScopedLock sl (lock); f = favorites; } dir().getChildFile ("favorites.txt").replaceWithText (f.joinIntoString ("\n")); }
    void saveFolders() { juce::StringArray f; { const juce::ScopedLock sl (lock); f = folders; } dir().getChildFile ("folders.txt").replaceWithText (f.joinIntoString ("\n")); }

    void run() override
    {
        { const juce::ScopedLock sl (lock); files.clear(); }
        const auto roots = getFolders();
        juce::StringArray chunk;
        for (const auto& root : roots)
            for (const auto& entry : juce::RangedDirectoryIterator (juce::File (root), true, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg", juce::File::findFiles))
            {
                if (threadShouldExit()) return;
                chunk.add (entry.getFile().getFullPathName());
                if (chunk.size() >= 5000) { const juce::ScopedLock sl (lock); files.addArray (chunk); chunk.clear(); }   // results appear while scanning
            }
        juce::StringArray all;
        { const juce::ScopedLock sl (lock); files.addArray (chunk); all = files; }
        dir().getChildFile ("index.txt").replaceWithText (all.joinIntoString ("\n"));
    }

    mutable juce::CriticalSection lock;
    juce::StringArray folders, files, favorites;
};
