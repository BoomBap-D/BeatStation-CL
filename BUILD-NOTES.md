# Beat Station Phase 15 — JUCE 7 Port + Build Notes (Nov 15, 2026)

## What Phase 15 contains (verified against Phase 13, file by file)
Phases 14 and 15 in one zip. 15 files changed plus 3 new:
MasterDsp.h, Mastering.h, MasteringPanel.h. The zip also carries
`manual-shots/` — the manual's screenshots (kept in the Source
package; not needed to build, so left out of the GitHub-Ready one).
- **Phase 14 — the Mastering Station** (MIXER > MASTERING STATION
  switch on the master channel; the switch now works and lights):
  chain EQ > GLUE > MULTIBAND > COLOR > IMAGER > CLIPPER > LIMITER
  (fixed order, limiter last, true-peak, −1 dBTP default). Meter
  bridge: LUFS loudness (BS.1770/EBU R128 — NOW / 3 SEC / SONG),
  targets (Streaming −14, Apple −16, Loud hip-hop −9, Club −7),
  highest true peak, per-stage gain reduction, RESET METERS.
  21 presets; MATCH TARGET sets limiter gain to hit the target;
  BOUNCE writes the song to WAV (24-bit, or 16-bit with dither).
  Limits per its README: fixed stage order, real-time WAV-only
  bounce, no reference A/B or MP3 preview yet, ~4 ms added delay
  not reported to a DAW.
- **Phase 15 — fixes found while photographing every screen for
  the manual:** number readouts no longer show long decimals; TUNE
  screen no longer jumps between tracks or smears; mixer VU needles
  redrawn inside the dial; Deck CUT FADER/PITCH labels uncovered;
  keyboard opens at C1; FX DELAY labels fixed; SHAPE help text got
  its own strip.

## The port (same standard way as Phase 4–13)
- **JUCE 8.0.4 → JUCE 7.0.12.** JUCE 8's deployment target is Windows 10
  only. JUCE 7.0.12's README lists its Windows deployment target as
  Vista onward — this build covers **Windows 7, 8, 8.1, 10 and 11**,
  including D's Windows 8.1 Pro PC. Port surface: 66
  `juce::Font(juce::FontOptions(...))` calls converted to JUCE 7
  `juce::Font(size, style)`.
- **They fixed two of the three recurring bugs themselves this
  time**: `jmin<juce::int64>`/`jmax<juce::int64>` are now
  `std::min`/`std::max`, and `PluginProcessor.h` includes
  `juce_audio_devices`. The CMake link line still omits
  `juce_audio_devices`, so the port adds it there as before.
- The ported Phase 15 tree was diffed against the ported Phase 13
  tree: the same 18 source files differ — nothing else moved.

## Verification (Linux, Nov 15, 2026)
- JUCE 7.0.12 build result, artefact sizes, dlopen/GetPluginFactory
  and the running-build screenshots are recorded with the delivery in
  the VST WORK chat.
- The Windows build runs on GitHub (repo BoomBap-D/BeatStation-CL) —
  download BeatStation-Windows from the run and run `Beat Station.exe`
  on the PC; that is the end-to-end proof.

## Build it (any platform)
Needs CMake 3.22+ and a C++17 compiler.
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
JUCE 7.0.12 downloads automatically (or use the GitHub-Ready package —
see AUTO-BUILD.md).
