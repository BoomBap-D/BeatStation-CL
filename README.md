# Beat Station - phase 1: sampler + 16 pads (VST3 instrument + standalone)

## Build
Needs CMake 3.22+ and a C++17 compiler.

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

JUCE 8.0.4 downloads automatically. You get a VST3 instrument (copied to your VST3 folder) and a Standalone app.

## Try it
1. Run the Standalone app.
2. Drag a drum sample (wav/aif/flac/mp3/ogg) onto a pad. Drag several files at once to fill consecutive pads.
3. Click pads to play them. Click lower on a pad for harder hits. In a DAW the pads answer MIDI notes 36-51.
4. Select a pad, press Load, then Chop 4/8/16 (equal slices) or Chop at hits (slices at detected transients).
   The slices spread over the following pads. Drag the two handles in the waveform to trim a pad's region.
5. Per pad: Level, Pan, Tune (+/-12 semitones, changes pitch and speed like an MPC) and Decay (5000 ms = off).

Samples are remembered by file path in your project/preset, so keep the files where they are.

## What's next
- Banks B, C, D (32+ pads), choke groups (open/closed hat), one-shot vs gate, reverse, per-pad filter.
- The 4-track step sequencer linked to the pads, then the keyboard, sound bank browser, multitrack, effects.
- Replace the SpinLock/shared_ptr sample swap with a lock-free handoff before release.



## Phase 1.1 changes
- STOP button (and MIDI "all notes off") fades out everything that is playing.
- Per-pad Mode: Cut (default - hitting a pad again restarts it instead of doubling up), Layer, Gate (stops when you release).
- Choke groups: give pads the same CHOKE number (1-8) and they cut each other (closed/open hat). Pad 0 = no group.
- MIDI: the Standalone app now opens every connected MIDI input by itself (press "Rescan MIDI" after plugging one in).
  Don't also enable the same keyboard in the app's Options menu or notes will double. In a DAW, route MIDI to the plugin as usual.
- On-screen keyboard (clickable, and your computer keys work when it has focus). Notes 36-51 play pads 1-16.
- Keys mode: every note plays the selected pad's sample pitched across the keyboard (C3 = original pitch), polyphonic.
  On-screen pad clicks and anything on MIDI channel 10 still play the pads.

Status: written as a starting point, not yet compiled or tested. Expect small fixes on the first build.

## Phase 2: built-in kits + step sequencer
- **Kits:** four synthesized drum kits are compiled into the plugin (Trap, Boom Bap, Drill, G-Funk), 16 sounds each (kick, snare,
  clap, hats, rim, perc, two 808s, tom, shaker, stab, pluck, bell, FX). Pick one from the menu at the top - it fills all 16 pads.
  The Trap kit loads on startup. They are plain starter sounds made from scratch (no samples), so you can test everything without
  hunting for files; drop your own audio on any pad to replace a sound. To regenerate or edit them, see Kits/ (wav files named
  kit_NN_Name.wav; add more kits by dropping in files with the same naming and re-running CMake).
- **Step sequencer:** open the STEP SEQ drawer. 4 tracks x 16 steps (Length 32 adds a second page), tempo, swing, Play/Stop.
  Each track has a Pad menu (which pad it plays) and a mute button. A starter beat is loaded so Play makes sound right away.
  Click or drag across steps to paint them. In a DAW it follows the host transport and tempo.
- **REC:** press Play, press REC, then hit pads (on screen, MIDI keyboard, notes 36-51): hits land on the nearest step of the track
  that uses that pad. If a pad is not on any track, pick it in a track's Pad menu first.
- Patterns, tempo, kit/pad choices and the rest are saved with your project.

## Phase 3: envelopes and filter per pad (SHAPE drawer)
- Open the **SHAPE** tab. For the selected pad you get an **amp envelope** (Attack, Decay, Sustain, Release), a **filter**
  (Off / Low-pass / High-pass / Band-pass with Cutoff and Reso), and a **filter envelope** whose **ENV AMT** knob says how many octaves
  the cutoff sweeps (+ opens, - closes). Drag the dots on the curves, or use the knobs.
- The old Decay knob is gone: use the amp envelope instead. For a punchy hit, set Sustain low and Decay short. Sustain 100% plays the
  whole chop. On Cut/Layer pads the Release is applied automatically as the chop ends, so tails fade smoothly; on Gate pads it
  starts when you let go of the key.
- Try it: Filter = Low-pass, Cutoff ~400 Hz, ENV AMT +4, filter Decay ~250 ms, filter Sustain 0 = a classic "wow" pluck.
- Company name is now Basement Instruments (plugin company, window title). Plugin manufacturer code changed to "Bsmt".

## Phase 4 (feedback round)
- **Sample browser (BROWSE tab, right of the pads):** press Folders..., add a folder of sounds (millions of files are fine; it indexes in the
  background and remembers the index between runs). Type several words to search (all must appear in the file path, so folder names
  count). Click a result or use the up/down arrows to hear it before loading. Double-click or press Enter to load it into the selected
  pad, or drag a row onto any pad. Space replays, Esc stops. "Next pad after load" moves on so you can fill a kit quickly.
- **MIDI Learn:** right-click any knob, any pad, or the Play/Stop/STOP/REC buttons, choose MIDI Learn, then move the knob or press the
  key/pad/button on your controller. Right-click again to clear. Pads can be assigned any note. Mappings are saved with the project.
- **Clear Chops** keeps the sample (on the first pad that had it, whole again) and empties the other slices. **Play Full** plays the whole
  sample even when it is chopped. **Clear Pad** removes the sample.
- **Sequencer names:** click a track name to hear the sound; right-click to rename it.
- **Keyboard:** two octaves at a time with big charcoal keys, arrows and a slider to reach all 88 keys.
- **SHAPE:** one free-form envelope editor with AMPLITUDE and FILTER tabs. Drag dots anywhere, double-click to add a dot (up to 8),
  double-click a dot to remove it. The filter now starts as an open low-pass; turning Cutoff down is heard immediately, even on a
  sound that is already ringing. FADE sets the tail length.
- **Meters:** IN on the left, OUT on the right. Segments, a peak-hold line per channel (stays until you click the meter), and a red CLIP
  box (click it to clear). IN shows audio-input level in the Standalone app once your audio input is enabled (it is not played back yet).
- **Transport:** Play and Stop in the top bar, tempo shown next to them.
- **Audio cutouts:** found and fixed one likely cause - filters were being set up while a sound started, which can allocate memory on the
  audio thread. They are now prepared in advance. Build in Release mode (a Debug build is much slower and can crackle).
- Coming next: mix mode (a small fader and meter on every pad). Per-pad LEVEL and PAN already exist (knobs under EDIT).

## Phase 5 (second feedback round)
- **Meters:** IN now shows the loaded pad sounds before the envelope and filter; OUT shows the final result after the envelopes, filter,
  synth and the master EQ/compressor.
- **MIDI Learn fixed:** controllers are now told apart by channel and number (knobs that share a CC number on different channels no
  longer overwrite each other). Pad knobs (level, pan, tune, choke) and the SHAPE knobs follow the selected pad, so a learned knob
  always moves the pad you are looking at. The title bar shows the last MIDI message received, e.g. "last: CC 21 ch1 = 64".
- **Sample window:** AMP ENV and FILTER ENV switches in the sample window turn each envelope on or off; when on, the curve is drawn over
  the sample (drag dots, double-click to add or remove). Click the sample to hear it (Shift+click = whole sample). Drag the DRAG TO PAD
  tab onto another pad or onto a sequencer track to copy the sound and chop there.
- **Drag and drop:** sounds from the browser and copies from the sample window can be dropped on a pad or on a sequencer track (it loads
  into that track's pad).
- **Browser:** shows one folder at a time (double-click a folder to go in), Back / Forward / Up, Favorites (folders or single sounds,
  saved between runs), and the search box still searches everything you added. Shift+click loads a sound.
- **Keys:** pick one of 21 built-in instruments from the INSTRUMENT menu above the keyboard (piano, electric piano, organs, pads,
  strings, brass, leads, plucks, bells, basses, flute, clav...). Choosing one switches Keys on. They are synthesized live and are simple
  approximations, not recordings. "Selected Pad Sample" plays the pad's sound across the keyboard.
- **FX window (top bar, FX):** a master EQ and compressor on the final mix, the same processing as the stand-alone EQ/Comp plugin. Show EQ,
  COMP or BOTH side by side; each has its own on/off switch. The compressor starts off.
- **File menu:** Save Project... and Open Project... write and read a .bstation file (pads, chops, envelopes, sequence, MIDI mappings).
  The stand-alone app also restores your last session by itself. A "save before closing?" prompt needs a custom stand-alone wrapper.

## Phase 5b (small update to phase 5)
- Right-click a pad: **Load sound...** (next to MIDI Learn).
- Right-click a sound in the browser: **Load to the selected pad** and **Load to pad...** (pick any of the 16).
- Nothing else changed from phase 5.

## Phase 6 - the turntable (DECK)

New **DECK** button in the title bar (next to PADS). DECK swaps the pad bank for a single turntable. The sequencer, keys, shape drawer and master FX stay where they are.
The deck is an original Basement Instruments design (charcoal and amber) with the functions of a classic direct-drive deck. It is not a copy of any real product's look.

- **Platter**: grab it with the mouse and move it in a circle to scratch. One revolution is 1.8 s of audio at 33. Hold it still and the sound stops, let go and it spins back up to speed.
- **START / STOP**: motor with a short spin-up and a short brake. **33 / 45**, **REVERSE**, **LOOP** (on by default, off = plays once and goes silent at the end).
- **Pitch fader**: top is minus, bottom is plus (like a real deck). Double-click to reset to zero. The range button cycles +/-8, 16, 50 percent. The ticks light up from the centre to the fader.
- **Loading**: "Load pad N" puts the selected pad's WHOLE sample on the record, even if it is chopped. "Load..." opens any audio file. You can also drag a sample from the browser, or a pad's waveform, onto the deck.
- **Hot cues**: when a record comes from a pad, the chop starts of every pad that shares that sample become cues 1-8. Click a cue to jump there (the motor starts if it was stopped). An empty cue sets itself to the current position when clicked. Right-click a cue to set or clear it.
- **Waveform strip**: click or drag to jump. Cue flags are drawn on it.
- **CUT FADER** and **HOLD CUT**: the deck's output gain with ~1.5 ms smoothing, for chops and transforms.
- The deck goes through the same IN meter, master EQ/compressor and OUT meter as everything else.

Not in this phase (next): saving the deck state in the project file, scratch recorder, second deck and DJ mixer, key lock, sync.

Also fixed: SeqPanel used `Slider::TextBoxInside`, which does not exist in JUCE 8. It is now `TextBoxLeft` (bar sliders draw their text inside anyway).

## Phase 7 - input rules, record button, instrument / drum switches, browser fixes

**IN and OUT meters**
- IN shows only sound that comes in from outside: a browser audition, the audio input while REC is on, and samples you loaded (at the level they came in at).
- Turning a pad's level, pan, envelope or filter changes OUT only. IN stays where it was.
- Internal sounds show on OUT only: the built-in drum kits and the Keys instruments.
- A browser audition passes straight through, so IN and OUT match.

**INPUT volume knob (VOL, under the IN meter)**
- Sets the level of what comes in. Turn it down while auditioning a loud sample and both meters drop.
- When you load a sample, the knob's level at that moment stays with that sound. Double-click the knob for 0 dB.
- The browser's own volume slider is gone: this knob replaces it.

**REC button (bottom of the IN meter)**
- Blends in when off, glossy red when on. Click to start, click again to stop. The recording (up to 60 s) lands on the selected pad and is saved as a WAV in your user data folder under Basement Instruments/Beat Station/Recordings.
- Standalone: JUCE mutes the audio input by default. Open Options > Audio/MIDI Settings, choose your input and untick "Mute audio input". The input is not played back through the speakers while recording, so there is no feedback.
- The plugin now has an optional stereo input, so in a DAW you can route audio into it.

**INSTRUMENT / DRUM SET on the keyboard**
- Click the word INSTRUMENT (left of the dropdown in KEYS). It changes to DRUM SET, the dropdown lists the drum kits, and every key plays a pad (the 16 pads repeat up the keyboard).

**KITS / INSTR on the pads**
- The button left of the kit dropdown at the top. KITS = pads play their samples. INSTR = the dropdown lists the instruments and the 16 pads play that instrument as 16 notes going up from C3. The sequencer follows the same switch.

**Browser**
- "< Back" (where you just were), "Up" (one folder up) and a new "Home" (your main folders).
- Clicking a sound only auditions it, however fast you click. Double-click opens folders and no longer loads sounds. To load: the Load button, Enter, right-click, Shift+click, or drag onto a pad.
- "Next pad after load" now starts switched off.

## Phase 8 - sequencer overhaul, pan knob, clip lights, window size

**Step sequencer**
- Tracks are no longer fixed at 4. Use "+ Track" to add one (up to 64). The list scrolls.
- A track plays a pad OR one key of the Keys instrument. Click the box at the left of a row: Pads > Pad 1-16, or Keys > the note.
- Recording the keyboard: switch Keys ON, press Play and REC, and play. Each key you play gets its own track ("Key C3", "Key E3"...), made automatically.
- Recording pads: with REC on, any pad you hit is recorded. If that pad has no track yet, one is made for it automatically.
- "Length" is one button: 1, 2, 4 or 8 bars (16 to 128 steps). The "<" and ">" buttons next to it move between bars.
- Right-click a track name: rename, clear its steps, or delete the track. Click the name to hear it. M mutes.
- Old projects (4 tracks x 32 steps) open and are converted.
- Key tracks play whatever instrument is chosen in KEYS at the time (one instrument for all key tracks for now).

**Knobs**: any knob with zero in the middle (pan, tune, EQ gain, filter envelope amount) now shows a line at the top centre and lights outwards from it: left when turned left, right when turned right, nothing lit in the middle.

**CLIP lights**: glossy red with the word CLIP, and they stay on until you click them.

**Window size**: File > Window size: 75% to 200%, or "Fit to screen". The whole interface scales. The size is saved with the project.

## Phase 9 - Room Reverb built in

Your Room Reverb 0.1.1 is now part of Beat Station: FX > **ROOM** tab. It sits on the master chain after the EQ and compressor and starts switched off.

- **Same engine** (`RoomEngine.h`: 24 wall reflections + 8-line diffuse tail), with these changes for a cleaner, more exact sound:
  - the eight tail delays are whole-sample, all-prime lengths: no shared resonances and no interpolation blur once a setting has settled
  - early reflections land on exact samples, so they keep their full treble
  - new **LOW CUT** knob (20-600 Hz, default 90 Hz) on the reverb only, so the low end of the mix stays tight
  - reflections are recalculated only when a control moves (less CPU)
  - when ROOM is off it fades out and then uses no CPU
- **All 104 factory rooms** are in the list on the left, with search and categories. Picking one switches the reverb on.
- **The look** now matches Beat Station: charcoal and amber, the vintage knobs with lit ticks, the same panels and fonts.
- **Rapper and singer** are new shaded vector figures (clothing, cap, chain, mic, mic stand, hair) instead of the block figures. They are drawings, not photos, and stay sharp at any window size. They live in `RoomFigures.h` as SVG, so they are easy to swap for other artwork later.
- Drag the performer or the listener in the picture. 3D view or top-down plan. Surface shortcuts, HEAR WET and all 19 knobs are there, and every knob can be MIDI-learned (right-click).
- All room settings are saved with the project.

Checked here: the ported engine was run at 44.1, 48 and 96 kHz on six presets. Output stayed finite and the measured decay followed the DECAY setting (for example 2.80 s set, 2.68 s measured).
