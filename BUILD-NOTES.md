# Beat Station Phase 9 — JUCE 7 Port + Build Notes (Nov 14, 2026)

## What Phase 9 contains (verified against Phase 7, file by file)
Phases 8 and 9 together (no separate Phase 8 was sent). 11 files
changed plus 5 new Room files (RoomEngine.h, RoomFigures.h, RoomFx.h,
RoomPresets.h, RoomView.h):
- **Phase 8 — sequencer overhaul:** tracks are no longer fixed at 4 —
  "+ Track" adds up to 64, the list scrolls; a track plays a pad OR a
  Keys note; recording pads/keys auto-creates their tracks; Length is
  1/2/4/8 bars with bar-step buttons; right-click a track to rename,
  clear or delete; old 4-track projects convert. Centre-zero knobs
  (pan, tune, EQ gain, filter env) light outwards from the top-centre
  line. CLIP lights are glossy red and latch until clicked. Window
  size menu (File): 75%–200% or Fit to screen, saved with the project.
- **Phase 9 — Room Reverb built in:** their Room Reverb 0.1.1 engine
  (24 wall reflections + 8-line diffuse tail, whole-sample prime tail
  delays, new LOW CUT knob) sits in the master chain after EQ/comp as
  FX > ROOM, starting off. All 104 factory rooms with search and
  categories; charcoal/amber look; rapper/singer vector figures
  (RoomFigures.h SVG); drag performer/listener, 3D or plan view,
  HEAR WET, 19 knobs, all MIDI-learnable; settings save with the project.

## The port (same standard way as Phase 4–7)
- **JUCE 8.0.4 → JUCE 7.0.12.** JUCE 8's deployment target is Windows 10
  only. JUCE 7.0.12's README lists its Windows deployment target as
  Vista onward — this build covers **Windows 7, 8, 8.1, 10 and 11**,
  including D's Windows 8.1 Pro PC. Port surface: 27
  `juce::Font(juce::FontOptions(...))` calls converted to JUCE 7
  `juce::Font(size, style)`.
- Bugs fixed again: missing `juce_audio_devices` (include + CMake
  link); three `juce::jmin<juce::int64>` calls → `std::min`.
  `TextBoxLeft` from Phase 7 stands as written (JUCE 7 has it).
- The ported Phase 9 tree was diffed against the ported Phase 7 tree:
  exactly the same files differ — nothing else moved.

## Verification (Linux, Nov 14, 2026)
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
