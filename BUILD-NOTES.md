# Beat Station — JUCE 7 Port + First Build Notes (Nov 13, 2026)

## What changed and why
- **JUCE 8.0.4 → JUCE 7.0.12.** JUCE 8's deployment target is Windows 10 only
  (JUCE 8 README; its BREAKING_CHANGES say there is no workaround). JUCE 7.0.12
  is the last generation that targets Windows 8.1, and a JUCE 7 build also runs
  on Windows 10/11. This is the same port BoomBap received, done so the
  Windows build runs on D's Windows 8.1 Pro PC. Port surface: 11
  `juce::Font(juce::FontOptions(...))` calls converted to JUCE 7
  `juce::Font(size, style)`; nothing else in the code was JUCE 8-only.
- The original Phase 4 source had **never been compiled** (its README said so).
  Three bugs were found and fixed on the first build:
  1. `PluginProcessor` uses `juce::MidiInput` / `MidiInputCallback` /
     `MidiMessageCollector` (the standalone's hardware-MIDI input) but the
     project never included or linked `juce_audio_devices`. Added the include
     to `PluginProcessor.h` and `juce::juce_audio_devices` to CMake.
  2. `SeqPanel.h` constructed the BPM/SWING sliders with
     `juce::Slider::TextBoxInside`, which does not exist in any JUCE version.
     Changed to `NoTextBox`. KNOWN COSMETIC: the STEP SEQ drawer's BPM and
     SWING bars no longer show their numbers inside the bar; the tempo is
     always shown in the top bar (e.g. "92.0 BPM"). Sliders still work.
  3. Two `juce::jmin<juce::int64>` calls (sample-length caps in
     `PluginProcessor.cpp`) collide with `juce_dsp`'s SIMD `jmin` and fail to
     compile. Replaced with `std::min`.
- CMake warning (pre-existing): the auto bundle ID contains spaces
  ("com.Basement Instruments.BeatStation"). Harmless on Windows/Linux;
  set an explicit BUNDLE_ID before any macOS release build.

## Verification (Linux, Nov 13, 2026)
- JUCE 8.0.4 build of the fixed source: clean (EXIT 0) — proved the bug fixes.
- JUCE 7.0.12 build: clean (EXIT 0). Standalone 11 MB; VST3 `.so` 9.9 MB,
  SDK VST 3.7.8, moduleinfo reads "Beat Station / Basement Instruments /
  Instrument, Sampler".
- VST3 dlopen OK; `GetPluginFactory` present.
- Standalone launched and screenshot-verified running: Trap Kit loaded on
  startup (16 named pads), waveform shown, STEP SEQ drawer open with the
  starter pattern lit, MIDI Learn hint visible.

## Build it (any platform)
Needs CMake 3.22+ and a C++17 compiler.
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
JUCE 7.0.12 downloads automatically (or the GitHub-Ready package builds
itself on GitHub — see AUTO-BUILD.md in that package).
