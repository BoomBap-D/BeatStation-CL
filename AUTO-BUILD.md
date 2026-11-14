# Beat Station Phase 5b — Update on GitHub (browser upload)

Phase 5 goes into the SAME repo as Phase 4: **BoomBap-D/BeatStation-CL**.
The build workflow is already in that repo and in this package, and it
starts by itself on every upload. You never compile anything on your PC.

## Update the repo
1. Unzip this package on your PC and open the folder.
2. On github.com, open **BoomBap-D/BeatStation-CL** → **Add file** →
   **Upload files**.
3. Select ALL of this package's contents (Ctrl+A — including the
   `.github` folder) and drag them into the upload page. Upload the
   CONTENTS, not the outer folder itself. Same-name files are replaced;
   Phase 5b's files are added.
4. **Commit changes.** The build starts by itself.

## Getting your Windows build
1. Repo → **Actions** tab → open the newest run.
2. Wait for it to finish. Green check on **Windows** is the one that
   matters (macOS can cancel on GitHub's side — that's not a code
   failure, it happened on Phase 4's first run too).
3. Open the run → **Artifacts** → download **BeatStation-Windows**.
4. Inside: `Beat Station.exe` (standalone — double-click and play) and
   `Beat Station.vst3` (drop the whole folder into
   `C:\Program Files\Common Files\VST3` for your DAW).

Built on **JUCE 7.0.12** — JUCE 7's Windows deployment target is Vista
onward, so this covers **Windows 7, 8, 8.1, 10 and 11**, including your
8.1 PC. (JUCE 8, which Phase 5 arrived on, is Windows 10 only.)
Notes on what was fixed and ported: see BUILD-NOTES.md.
