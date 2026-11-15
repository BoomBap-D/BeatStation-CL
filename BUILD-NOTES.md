# Beat Station Phase 12 — JUCE 7 Port + Build Notes (Nov 15, 2026)

## What Phase 12 contains (verified against Phase 9, file by file)
Phases 10, 11 and 12 in one zip (no separate 10/11 sent). 12 files
changed plus 8 new: Booth.h, BoothPanel.h, BoothPresets.h, FxChain.h,
FxDsp.h, FxPresets.h, FxViews.h, TracksPanel.h.
- **Phase 10 — effects suite:** the FX window is a chain of 11 effects
  (EQ, COMP, DRIVE, FILTER, CRUSH, PUNCH, CHORUS, DELAY, ROOM, WIDTH,
  LIMITER) — click a tab's light for on/off, drag tabs to reorder,
  order saved with the project. 61 chain presets in 9 categories.
  DRIVE and LIMITER add latency that is not yet reported to a DAW.
- **Phase 11 — multitrack + Vocal Booth:** TRACKS tab with 8 audio
  tracks in time with the beat (arm, REC, mute/solo/volume, WAV takes
  saved and reloaded; tracks 1–4 are Booth tracks). BOOTH page
  (title-bar button): 10 microphone characters + Off (tonal characters
  from published descriptions, not measurements), BURN IN THE MIC,
  vocal chain MIC > GATE > DE-ESS > COMP > EQ > DRIVE > DOUBLER >
  DELAY > REVERB (reorderable), tone graph, meters, 50 vocal presets
  (30 hip-hop). KEYS gets REC TO STEPS. Limits per its README: one
  take per track, one shared Booth chain, no pitch correction (that
  arrives in Phase 12), no latency compensation on takes.
- **Phase 12 — punch + pitch:** punch recording on TRACKS and in the
  Booth (Whole take / Manual / Auto IN-OUT / Match punch — drops in
  by itself when your level matches your last take, join placed at a
  breath; undo/redo of the last take). TUNE pitch correction in the
  Booth (key, scale, speed, amount; per-track tuner, not recorded
  into the take; ~27 ms delay compensated on playback).

## The port (same standard way as Phase 4–9)
- **JUCE 8.0.4 → JUCE 7.0.12.** JUCE 8's deployment target is Windows 10
  only. JUCE 7.0.12's README lists its Windows deployment target as
  Vista onward — this build covers **Windows 7, 8, 8.1, 10 and 11**,
  including D's Windows 8.1 Pro PC. Port surface: 44
  `juce::Font(juce::FontOptions(...))` calls converted to JUCE 7
  `juce::Font(size, style)`.
- Bugs fixed again: missing `juce_audio_devices` (include + CMake
  link); `juce::jmin<juce::int64>` → `std::min`, now in FOUR places
  (a fourth sample loader lives in the new Booth.h).
- The ported Phase 12 tree was diffed against the ported Phase 9 tree:
  the same 20 files differ — nothing else moved.

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
