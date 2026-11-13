# Beat Station — Auto-Build (GitHub)

This package builds itself on GitHub. You never compile anything on your PC.

## One-time setup
1. In GitHub Desktop (signed in as BoomBap-D), create a NEW repository named
   **BeatStation** (separate from the BoomBap repo). Do not initialize it
   with a README.
2. Copy ALL of this package's contents — including the hidden `.github`
   folder — into the new repo's folder on your PC.
3. Commit ("Beat Station JUCE 7 first build") and Publish/Push.

## Getting your Windows build
1. Open the BeatStation repo on github.com → **Actions** tab.
2. Wait for the run to finish (~10–20 minutes). Green check on **Windows**
   is the one that matters.
3. Open the run → **Artifacts** → download **BeatStation-Windows**.
4. Inside: `Beat Station.exe` (standalone — double-click and play) and
   `Beat Station.vst3` (drop the whole folder into
   `C:\Program Files\Common Files\VST3` for your DAW).

Built on **JUCE 7.0.12**, so it runs on your Windows 8.1 PC (and 10/11).
macOS builds are unsigned — first open is right-click → Open.
Notes on what was fixed and ported: see BUILD-NOTES.md.
