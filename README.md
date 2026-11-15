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

## Phase 10 - the effects suite

The FX window is now a full effects chain on the output, with one-click presets.

**The chain (the row of tabs at the top)**
- Eleven effects: EQ, COMP, DRIVE, FILTER, CRUSH, PUNCH, CHORUS, DELAY, ROOM, WIDTH, LIMITER.
- Sound goes through the tabs left to right, then to the OUT meter.
- Click a tab to open that effect's screen. Click the light on a tab to switch the effect on or off. Drag a tab sideways to move it in the chain. The order is saved with the project.

**The new effects** (`FxDsp.h`, plain C++ so each one can be tested on its own)
- **DRIVE** - Tape, Tube or Clip saturation. Runs at twice the sample rate with anti-aliasing on top. Screen: the input/output curve with a dot riding it.
- **FILTER** - low-pass, high-pass, band-pass, notch; 12 or 24 dB; resonance; an LFO for sweeps. Screen: the response curve, moving with the LFO.
- **CRUSH** - old-sampler sound: sample rate, bit depth and an output filter. Screen: a smooth wave and the stepped wave it becomes.
- **PUNCH** - transient shaper: Attack makes hits harder or softer, Sustain lengthens or tightens tails. Screen: scrolling level with the gain it applies.
- **CHORUS** - two moving delays per side; short delay + feedback gives a flanger. Screen: the left and right movement.
- **DELAY** - tempo-synced or free, ping-pong, repeats get darker and thinner each time round. Screen: each repeat as a bar, left above and right below, against the beat.
- **WIDTH** - stereo width, and "Mono below" to keep the low end in the centre. Screen: stereo scope and a mono-safety bar.
- **LIMITER** - 3 ms look-ahead, the output never passes the ceiling. Screen: level in, the ceiling, and how much is being held back.

**Presets (61)** - the PRESET bar under the tabs: click the name for the menu, or use < and >.
Start Here, Drums, 808 & Bass, Sampling & Lo-Fi, Vocals, Keys & Melody, Mix Bus, Mastering, Creative & DJ.
A preset sets which effects are on, their order and every control. They are plain text recipes in `FxPresets.h`, so adding more is one line each.

**EQ and compressor colours**
- EQ: the spectrum is drawn in the band colours from low to high; each band's boost or cut is filled in its own colour; the three knobs light up in the colour of the band you picked.
- Compressor: incoming level in blue, gain reduction in red, knobs in the compressor's colour.

**Tested here** (offline, at 48 kHz):
- DRIVE: aliasing from a hard-driven 5 kHz tone is 73 to 133 dB below the tone (a plain waveshaper measures about 20 dB). Flat within 0.2 dB to 20 kHz when not driving.
- LIMITER: output never exceeded the ceiling with up to +24 dB of gain, noise bursts and spikes; untouched below the ceiling.
- DELAY: echo times exact to the sample; ping-pong alternates sides; maximum feedback cannot run away.
- FILTER slopes, CRUSH transparency at full settings, WIDTH at 0 (true mono) and 1 (unchanged), PUNCH directions: all as designed.

**Notes**
- The chain is on the whole output for now. Per-pad and per-track effects come with the mixer.
- DRIVE adds 23 samples of delay and LIMITER adds 3 ms when they are on. This is not reported to a DAW yet.
- The EQ and compressor each have their own tab now; the old side-by-side "BOTH" view is gone.

## Phase 11 - multitrack audio and the Vocal Booth

**Keyboard into the step sequencer (fix)**
- Recording only happens while the beat is playing, and before this build REC did not start it. Now REC starts the beat if it is stopped.
- The KEYS tab has its own **REC TO STEPS** button: it switches Keys on, starts the beat and records each key you play onto its own track.

**TRACKS tab (next to KEYS, STEP SEQ, SHAPE)** - eight audio tracks that play in time with the beat
- Click a track's round light to arm it, press **REC** (the beat starts and the take begins), press Stop to end the take. REC switches itself off after each take.
- The armed track shows the input level, and the IN meter shows the input while a track is armed.
- Each track: M (mute), S (solo), volume (drag; double-click resets), and the recording drawn against time.
- Click in a recording area to choose where Play starts from (green line).
- Right-click a name: rename, clear, load an audio file, or switch "goes through the Booth" on or off.
- **MONITOR** lets you hear the input through the Booth while recording. Use headphones or it will feed back.
- Takes are saved as WAV files (user data folder > Basement Instruments > Beat Station > Recordings) and are reloaded with the project.
- Tracks 1-4 (Lead Vocal, Double, Ad-Libs, Hook) start as Booth tracks. Tracks 5-8 go straight to the mix.

**BOOTH (button in the title bar)** - the vocal page
- **10 microphone characters** plus Off. Each one sets low-end weight, where the presence lift sits, how much air is on top, and how much valve colour:
  800 Air (Sony C-800G style), 87 Studio (Neumann U 87), 47 Tube (U 47), 67 Tube (U 67), 251 Silk (ELA M 251), C12 Open (AKG C12),
  49 Velvet (M 49), 7B Broadcast (Shure SM7B), 44 Ribbon (RCA 44), Reference Modern (Manley Reference Cardioid).
  They are tonal characters built from published descriptions of those microphones, not measurements of them, and they assume a fairly neutral microphone on your side.
- The character is added **while you listen**, so you can record first and choose a different microphone afterwards.
- **BURN IN THE MIC**: when lit, the character is recorded into the take ("MIC BURNED IN" shows on it) and is not applied again on playback.
- **The vocal chain**: MIC > GATE > DE-ESS > COMP > EQ > DRIVE > DOUBLER > DELAY > REVERB > the mix. Click a tab for its knobs, click its light for on/off, drag a tab to reorder. The order is saved with the project.
- **Tone graph**: the microphone curve (amber), the vocal EQ (green) and both together (white). The drawn curve was checked against the measured sound (within 0.02 dB).
- **Meters**: voice in, gate, de-ess, compression, booth out.
- **50 vocal chain presets, 30 of them hip-hop**: Leads (12), Ad-Libs & Doubles (6), Hooks & Melodic (7), Character (5); then R&B & Soul, Pop, Rock & Alternative, Reggae/Dancehall/Afrobeats, Gospel/Country/Jazz/Spoken.
- The four vocal tracks are shown at the bottom of the Booth with their own REC / Play / Stop. They are the same tracks 1-4 as in the TRACKS tab.
- The Booth's output joins the main mix and then goes through the master effects chain.

**Limits in this build**
- One recording per track (a new take replaces the old one). No takes list, punch-in, editing or moving clips yet.
- The Booth is one chain shared by all Booth tracks, so they all get the same microphone and effects.
- One compressor stage (many rap chains use two), and no pitch correction (Auto-Tune style) yet.
- No latency compensation: on a slow audio setup a take can land slightly late.
- Standalone: the audio input is muted by default. Options > Audio/MIDI Settings: choose your input and untick "Mute audio input".
- The window is 60 pixels wider (1192) to fit the BOOTH button.

## Phase 12 - punch recording and pitch correction

**Punch recording** (TRACKS tab and the Booth: the "punch" dropdown next to MONITOR)
Every pass is captured from the moment Play starts. The punch mode decides which part of it is kept. Only that part of the track is replaced
(8 ms crossfades at the joins); the rest of the old take stays.
- **Whole take** - everything from where Play started.
- **Manual punch** - REC rolls the song and you hear the old take. Press **PUNCH** to drop in, press it again to drop out.
- **Auto punch (IN / OUT)** - right-click the armed track's recording area: "Set punch IN here", "Set punch OUT here". REC then replaces only between them. The points are drawn in red.
- **Match punch (my last take)** - exclusive to Beat Station. Press REC and rap along with your last take. It compares your loudness with the old take's.
  When the two have stayed within 6 dB of each other for a quarter of a second it drops in by itself, and it places the join at the gap before
  that phrase (a breath), not in the middle of a word. If you came in later than the start of the phrase, the join goes where you came in.
  If you never rap, nothing is replaced. The header shows "LISTENING... you -20 dB / last take -22 dB" and then "PUNCHED IN".
- **Undo the last take** - right-click a recording area. Works after any take, punch or clear (choose it again to redo).
- A punch into an existing take is recorded the same way that take was (mic burned in or not), so the two halves match.

**TUNE (pitch correction)** - Booth, second tab, straight after MIC
- Finds the note being sung and moves the voice to the nearest note of the key and scale you choose.
- **KEY** (C to B), **SCALE** (Chromatic, Major, Minor, Major pentatonic, Minor pentatonic, Blues), **SPEED** (0 ms = the hard, robotic snap; 100-300 ms = natural), **AMOUNT**.
- The screen shows the note it is moving you to in large type, how many cents sharp or flat you are, and a scrolling picture: grey = what you sang, colour = where it was moved to, against the notes of the scale.
- Every vocal track has its own tuner (it can only follow one voice at a time), all sharing the same key, scale and speed.
- It is not recorded into the take: change the key or switch it off later. Vocal chain presets leave TUNE alone, because the key belongs to the song.
- It delays the voice by about 27 ms. Recorded tracks are played early by that amount so they stay in time. The live monitor signal is 27 ms late while TUNE is on.

**Tested here** (offline)
- TUNE on voice-like test tones at 44.1, 48 and 96 kHz: output landed within about 1 cent of the target note in every case (e.g. 225 Hz in -> 220.02 Hz out;
  270 Hz in C major -> 261.62 Hz, skipping C#). Level changed by less than 0.1 dB. With Amount at 0 the output is a clean delayed copy.
  Breath and noise pass through at the same level. Cost: about 1.5% of one CPU core per voice.
- Match punch detector, simulated on five cases: joining at the start of a phrase (same level; 4 dB louder and early; 3 dB quieter), joining mid-phrase, and never rapping.
  It placed the join at the phrase start in the first three, where the performer came in for the fourth, and left the take alone in the fifth.
- These were synthetic signals. Real voices (breathy, raspy, very fast) will need tuning by ear.

## Phase 13 - the mixer

**MIXER button** (title bar, next to BOOTH). It covers the main area like FX and BOOTH; press it again or "Back to pads" to leave.

**Channels**, left to right: the 16 pads, Keys, Deck, the 8 audio tracks, the Booth output, then the MASTER channel.
- Each channel: name, pan bar (drag sideways; double-click = centre), **M** mute, **S** solo, a fader with a level meter and peak-hold line (double-click = 0 dB), and its level in dB.
- Hover over a channel to see its full name (the pad's sample, the track's name) at the top.
- **Faders and pans now act live**, including on notes that are already ringing. Before this build a pad's level and pan were fixed at the moment it was hit.
- **Solo works across everything**: solo a pad and the keys, deck and audio tracks go quiet, and the other way round.
- The pad faders are the same control as the LEVEL and PAN knobs on the pad page, so they move together and can be MIDI-learned.
- Mutes, the keys level and pan, and the deck level are saved with the project. Solos are not (they reset to off).

**MASTER channel**
- A pair of VU meters (0 VU = -18 dBFS average, the usual studio line-up), L and R output meters with peak hold, the peak reading in dB, and a CLIP light that stays on until clicked.
- **Master fader** (new parameter "Master Volume", after all the effects). It glides, so moving it never clicks.
- **MONO**: hear the mix on one speaker to check nothing disappears. **DIM**: 20 dB quieter while you talk.
- The **MASTERING STATION** switch is in place on the master channel but is not active yet: that is the next build.

**Not in this build**: per-channel EQ, compressor and effect sends; grouping; the Deck and the Booth have no pan; the Booth channel has mute but no solo (solo the vocal tracks instead).

## Phase 14 - the Mastering Station

**How to get there**: MIXER > the **MASTERING STATION** switch on the master channel. "Back" returns to the mixer. The switch lights up while the station is on, and the master channel now shows the loudness in LUFS.

**Where it sits**: everything > master effects chain > master fader > **mastering station** > MONO / DIM (listening only) > output.

**The chain** (fixed order, the limiter always last). Click a stage for its picture and knobs; click its light to switch it on or off.
1. **EQ** - four broad bands on the whole mix, plus two moves on the sides only: SIDE LOW CUT (keeps the bass and 808 in the centre) and SIDE AIR.
2. **GLUE** - a gentle stereo bus compressor. Its side-chain low cut stops the kick and 808 from making the whole mix pump.
3. **MULTIBAND** - low / mid / high compressor, each band with threshold, ratio and gain, and two movable split points.
4. **COLOR** - valve-style warmth on the lows and mids, and new "air" harmonics on top.
5. **IMAGER** - stereo width for low / mid / high separately (0 = mono, 100% = unchanged, 200% = extra wide).
6. **CLIPPER** - shaves the tips of the loudest hits before the limiter, at twice the sample rate.
7. **LIMITER** - 3 ms look-ahead, working on the true (between-sample) peak. Default ceiling -1 dBTP.

**The meter bridge** (left side, always running, even with the station off)
- **Loudness in LUFS** to the ITU BS.1770 / EBU R128 standard: NOW (0.4 s), 3 SEC, and SONG (integrated, with the standard's two gates).
- **Target**: Streaming -14, Apple Music -16, Loud hip-hop -9, Club -7. The bridge says how many LU louder or quieter than the target the song is.
- **Highest true peak** in dBTP (8x oversampled), red if it is over the ceiling.
- **Gain being held back** by the glue, each multiband band, the clipper and the limiter.
- **RESET METERS** starts the song measurement again.

**One-click workflow**
1. Pick a master from PRESET (21: Start Here, Hip-Hop, Other Styles, Problem Solvers). That switches the station on.
2. Play the song through once.
3. Press **MATCH TARGET**: it sets the limiter gain so the song's loudness lands on the target you chose. Play again to check.
4. **BOUNCE...**: choose 24-bit, or 16-bit with dither. The song plays from the start and is written to a WAV (Documents > Beat Station > Bounces by default).
   Press Stop when the song ends; it keeps writing for 2.5 seconds so reverb and echo tails are not cut, then closes the file.

**Tested here** (offline, at 44.1, 48 and 96 kHz)
- Loudness meter: the standard's check tone (1 kHz stereo sine at -23 dBFS) read -22.99 to -23.01 LUFS on all three readings. Gating ignored a near-silent half.
- True-peak meter: a tone whose samples only reach -3.01 dB but whose real peak is 0 dB read -0.04 dB.
- Limiter at a -1.0 dBTP ceiling with 0, +8 and +16 dB of gain on a harsh test signal: the output true peak (checked with a separate 16x reconstruction) was -1.03 to -1.25 dBTP. It never went over.
- Band split put back together: flat to 0.000 dB. Glue ratio correct; with the side-chain low cut at 150 Hz a loud 45 Hz tone caused 0.0 dB of reduction.
- Multiband squeezed only its own band (a 5 kHz tone moved 0.01 dB while the low band was compressed 16 dB). Imager made out-of-phase bass mono and widened the top as set.
- Clipper held a 0 dB tone to -3.02 dB at a -3 dB ceiling and left quiet sound untouched. 16-bit dither noise floor: -96.3 dBFS.
- The whole chain plus meters costs about 5% of one CPU core.

**Limits in this build**
- The stage order is fixed (EQ ... LIMITER). The FX window's chain is still reorderable.
- No reference-track A/B yet, no MP3/AAC preview, no loudness-range or album tools.
- The bounce is in real time (it plays the song) and is WAV only.
- The limiter aims 0.4 dB under the ceiling to guarantee the true peak, so a master set to -1 dBTP usually measures around -1.1 to -1.3.
- The station adds about 4 ms of delay when the clipper and limiter are on. A DAW is not told about this yet.
- Preset values are engineering starting points and have not been tuned by ear.

## Phase 15 - fixes found while photographing every screen for the manual

Every screen was built and run for real (Linux, JUCE 8.0.4) to take the manual's pictures. That turned up these, all fixed and re-checked by photographing again:

- **Number readouts**: knob values could show long decimals ("999.9999390 Hz", "50.658 cents", "26.75 ms"). Values now show 0 decimals from 100 up, 1 decimal from 10 up, 2 below that.
- **TUNE screen**: with several vocal tracks the reading jumped between them. It now shows the live input while monitoring, otherwise the armed track (or the first track with a note). The picture no longer smears when the note jumps more than 5 semitones.
- **Mixer VU meters**: the needle and scale could draw outside the dial. Redrawn as a wide, shallow arc kept inside the dial, with a red zone from 0 VU.
- **Deck**: the CUT FADER label was hidden under the cue buttons and the PITCH label under the tonearm. Tonearm, cues, cut fader and hint text were moved apart.
- **Keyboard** now opens at C1 (where bass lines live) instead of C0.
- **FX DELAY screen**: LEFT/RIGHT labels no longer sit under the dry bar.
- **SHAPE drawer**: the help text has its own strip and no longer covers the knob names.
- **Build fixes** (Linux/macOS compilers): `jmin<juce::int64>` / `jmax<juce::int64>` replaced with `std::min` / `std::max`; `PluginProcessor.h` now includes `juce_audio_devices` for `MidiInput`.

The manual's screenshots are in `manual-shots/`.
