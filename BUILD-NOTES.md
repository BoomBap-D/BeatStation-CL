# Beat Station Phase 5b — JUCE 7 Port + Build Notes (Nov 14, 2026)

## What 5b changes (verified against Phase 5, file by file)
A small update — only two source files differ from Phase 5:
- **Right-click a pad → "Load sound..."** (next to MIDI Learn) opens a
  file picker and loads the chosen sample into that pad.
- **Right-click a sound in the browser → "Load to the selected pad"**,
  or **"Load to pad..."** with a submenu to pick any of the 16 pads.
- Nothing else changed from Phase 5 (README says the same, and the
  diff confirms it: PluginEditor.cpp + SampleBrowser.h only).

## The port (same standard way as Phase 4 / Phase 5)
- **JUCE 8.0.4 → JUCE 7.0.12.** JUCE 8's deployment target is Windows 10
  only. JUCE 7.0.12's own README lists its Windows deployment target as
  Windows Vista onward — so this build covers **Windows 7, 8, 8.1, 10
  and 11**, including D's Windows 8.1 Pro PC. Port surface: 15
  `juce::Font(juce::FontOptions(...))` calls converted to JUCE 7
  `juce::Font(size, style)`.
- The same three never-compiled bugs were back and are fixed again:
  1. Missing `juce_audio_devices` (include in `PluginProcessor.h` +
     link in CMake).
  2. `SeqPanel.h` BPM/SWING sliders used `juce::Slider::TextBoxInside`,
     which does not exist in any JUCE version → `NoTextBox`. KNOWN
     COSMETIC: those bars don't show numbers; tempo shows in the top
     bar. Sliders still work.
  3. Two `juce::jmin<juce::int64>` calls in `PluginProcessor.cpp` →
     `std::min`.
- The ported 5b tree was diffed against the ported Phase 5 tree: only
  the two 5b files differ — nothing else moved.

## Verification (Linux, Nov 14, 2026)
- JUCE 7.0.12 build: clean (EXIT 0). Standalone and VST3 sizes,
  dlopen/GetPluginFactory, and a screenshot from the running build are
  recorded with the delivery in the VST WORK chat.
- The Windows build runs on GitHub (repo BoomBap-D/BeatStation-CL,
  workflow already in place) — download BeatStation-Windows from the
  run and run `Beat Station.exe` on the PC; that is the end-to-end proof.

## Build it (any platform)
Needs CMake 3.22+ and a C++17 compiler.
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
JUCE 7.0.12 downloads automatically (or use the GitHub-Ready package —
see AUTO-BUILD.md).
