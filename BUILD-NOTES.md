# Beat Station Phase 5 — JUCE 7 Port + Build Notes (Nov 14, 2026)

## What Phase 5 adds (from its README)
- Meters: IN = pad sounds before envelope/filter; OUT = final mix after
  envelopes, filter, synth and the master EQ/compressor.
- MIDI Learn fixed: controllers told apart by channel + number; pad and
  SHAPE knobs follow the selected pad; title bar shows the last MIDI
  message received (e.g. "last: CC 21 ch1 = 64").
- Sample window: AMP ENV / FILTER ENV switches draw the envelope curve
  over the sample; DRAG TO PAD tab copies a sound/chop to a pad or a
  sequencer track; browser drag-and-drop onto pads and tracks.
- Browser: one folder at a time with Back/Forward/Up, Favorites, and
  search across everything added.
- Keys: 21 built-in synthesized instruments in the INSTRUMENT menu.
- FX window: master EQ + compressor on the final mix (EQ / COMP / BOTH).
- File menu: Save/Open Project as .bstation; standalone restores the
  last session by itself.
- 6 new source files: CompMath.h, FxPanel.h, MasterCompView.h,
  MasterEqView.h, MasterFx.h, SynthPatches.h.

## The port (same standard way as Phase 4)
- **JUCE 8.0.4 → JUCE 7.0.12.** JUCE 8's deployment target is Windows 10
  only (JUCE 8 README; BREAKING_CHANGES: no workaround). JUCE 7.0.12's
  own README lists its Windows deployment target as Windows Vista
  onward — so this build covers **Windows 7, 8, 8.1, 10 and 11**,
  including D's Windows 8.1 Pro PC. Port surface: 15
  `juce::Font(juce::FontOptions(...))` calls (Phase 5's new files added
  more) converted to JUCE 7 `juce::Font(size, style)`.
- Phase 5 as sent had **never been compiled** (its README says so for
  the line it continues). The same three Phase 4 bugs were back in the
  new source and are fixed again here:
  1. Missing `juce_audio_devices`: added the include to
     `PluginProcessor.h` and `juce::juce_audio_devices` to CMake.
  2. `SeqPanel.h` BPM/SWING sliders used `juce::Slider::TextBoxInside`,
     which does not exist in any JUCE version → `NoTextBox`. KNOWN
     COSMETIC: those two bars don't show numbers; tempo always shows in
     the top bar. Sliders still work.
  3. Two `juce::jmin<juce::int64>` calls in `PluginProcessor.cpp`
     collide with juce_dsp SIMD → `std::min`.
- CMake warning (pre-existing): auto bundle ID contains spaces
  ("com.Basement Instruments.BeatStation"). Harmless on Windows/Linux.

## Verification (Linux, Nov 14, 2026)
- JUCE 7.0.12 build: clean (EXIT 0). Standalone 11 MB; VST3 `.so`
  11 MB, SDK VST 3.7.8, moduleinfo reads "Beat Station / Basement
  Instruments / Instrument, Sampler".
- VST3 dlopen OK; `GetPluginFactory` present.
- Standalone launched and screenshot-verified running: Trap Kit
  auto-loaded (16 named pads), waveform up, AMP ENV / FILTER ENV
  switches and DRAG TO PAD visible, FX and File buttons in the top
  bar, "MIDI: none found / last: none yet" readout live, STEP SEQ
  starter pattern lit.
- The Windows build itself runs on GitHub (repo BoomBap-D/BeatStation-CL,
  workflow already in place) — download BeatStation-Windows from the
  run and run `Beat Station.exe` on the PC; that is the end-to-end proof.

## Build it (any platform)
Needs CMake 3.22+ and a C++17 compiler.
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
JUCE 7.0.12 downloads automatically (or use the GitHub-Ready package —
see AUTO-BUILD.md).
