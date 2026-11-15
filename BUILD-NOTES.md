# Beat Station Phase 7 — JUCE 7 Port + Build Notes (Nov 14, 2026)

## What Phase 7 contains (verified against Phase 5b, file by file)
This zip carries Phase 6 and Phase 7 together (no separate Phase 6 was
sent). 9 files changed plus 1 new file, TurntablePanel.h:
- **Phase 6 — DECK:** a turntable page (DECK button swaps the pad bank
  for it; sequencer/keys/shape/FX stay). Scratch by dragging the
  platter, START/STOP with spin-up/brake, 33/45, REVERSE, LOOP, pitch
  fader with ±8/16/50 range, load a pad's whole sample or any file,
  hot cues from chop points, waveform strip, CUT FADER / HOLD CUT.
  Deck state saving, scratch recorder, second deck, key lock and sync
  are NOT in this phase (per its README — they come next).
- **Phase 7 — input rules:** IN meter = outside sound only (browser
  auditions, audio input while REC is on, loaded samples at their
  incoming level); pad level/pan/envelope/filter move OUT only.
  INPUT VOL knob under the IN meter (double-click = 0 dB; a loaded
  sample keeps the knob's level from load time). REC button under the
  IN meter records up to 60 s onto the selected pad and saves a WAV
  (standalone: untick "Mute audio input" in Options > Audio/MIDI
  Settings; the plugin also gains an optional stereo input).
  INSTRUMENT / DRUM SET switch on the keyboard; KITS / INSTR switch on
  the pads (INSTR = pads play one instrument as 16 notes from C3).
  Browser: Back/Up/Home, click only auditions, double-click no longer
  loads, "Next pad after load" starts off.

## The port (same standard way as Phase 4 / 5 / 5b)
- **JUCE 8.0.4 → JUCE 7.0.12.** JUCE 8's deployment target is Windows 10
  only. JUCE 7.0.12's README lists its Windows deployment target as
  Vista onward — this build covers **Windows 7, 8, 8.1, 10 and 11**,
  including D's Windows 8.1 Pro PC. Port surface: 24
  `juce::Font(juce::FontOptions(...))` calls converted to JUCE 7
  `juce::Font(size, style)` (the new TurntablePanel.h alone had 8).
- Bug fixes: missing `juce_audio_devices` (include + CMake link) was
  back again — fixed. The `jmin<int64>` collision was back, now in
  THREE places (a third sample loader for the deck) — all → `std::min`.
- One bug they fixed themselves: `Slider::TextBoxInside` is now
  `TextBoxLeft`, which exists in JUCE 7 — left as they wrote it.
- The ported Phase 7 tree was diffed against the ported Phase 5b tree:
  exactly the same 9 files + TurntablePanel.h differ — nothing else moved.

## Verification (Linux, Nov 14, 2026)
- JUCE 7.0.12 build result, artefact sizes, dlopen/GetPluginFactory
  and the running-build screenshot are recorded with the delivery in
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
