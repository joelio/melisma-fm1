# Design: Melisma for the M-VAVE FM-1

Status: agreed on 20 November 2026. What is built so far: section 16.
Name: Melisma, a Merlin Engines instrument.

## 1. What it is

A standalone firmware that turns the FM-1 into a four-piece band you sit in with.
You choose how the music should feel using dials labelled with plain words; the
engine writes and plays the parts; you take any seat in the band and play along
on the keys. Every piece can be replayed, saved and shared.

It is a new instrument built from the ideas in Melisma (`~/src/work/music-thingy`),
not a port of its code. Melisma's vocabulary tables and research are the
specification; its Rust is a reference to read.

## 2. Principles

1. **Outcomes, not mechanisms.** Controls are named by what you hear (brighter,
   busier), never by theory. Theory names appear only where you ask for them.
2. **Wrong notes are a choice, not an accident.** By default the keys only give
   notes that fit. You can loosen or remove that help at any time, on the
   device, while playing.
3. **Nothing is lost.** No random numbers. The same settings always produce the
   same music, and everything you pass through can be recalled.
4. **Doing nothing sounds good.** Power on, press PLAY, music. Every dial has a
   default that works.
5. **Experiment on the device.** Anything about how notes are chosen is a
   setting you can flip mid-jam, not a build-time decision.
6. **It looks like an instrument.** The screen is an aero-engine panel, not a
   menu system.

## 3. The hardware we have

| Part | What it gives us |
|---|---|
| 27 keys, F3 to G5 | Chromatic mini keyboard. Each key has an LED with a bright and a dim level. |
| 7 endless encoders with detents | SELECT, ALGORITHM, KNOB1 to KNOB4, PRESETS. Stepped, so they suit word dials. |
| 14 buttons, each with an LED | OCT−, OCT+, FX, SEL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, PLAY, REC |
| MASTER knob | Volume (handled by the platform). |
| Screen | 240 × 240, 16-bit colour, flicker-free drawing. |
| Sound | Stereo, 44.1 kHz, 256-frame blocks (5.8 ms each). |
| Memory | 568 KB code and tables, 96 KB RAM, 336 KB pool for big buffers. |
| Storage | 4 KB settings, eight 4 KB blobs, eight 16 KB slots. |
| MIDI | USB in and out, TRS in, clock and transport. |
| USB audio | The FM-1 appears as a stereo audio input to a computer or phone. |

Not assumed: velocity from the FM-1's own keys. Dinghy treats them as on/off;
felucca-bsp has experimental velocity sensing. See Risks and unknowns.

## 4. The band

Four roles, as in Melisma: **bass, chords, melody, pulse**.

Each role is in one of four states, shown on its button LED:

| State | Meaning |
|---|---|
| Engine | The engine plays this part. |
| You | You play this part on the keys. The engine's version is silent. |
| Held | The part is frozen as it is now, while other dials keep changing the rest. |
| Off | Silent. |

The default is the engine playing bass, chords and pulse, with you on melody.

## 5. The dials

Seven encoders, seven word dials. Each turn of a detent moves one word. The
change takes effect at the next bar line, so the music never stumbles.

| Encoder | Dial | Words |
|---|---|---|
| KNOB1 | Mood | bright, warm, wistful, sad, dark, dreamy, tense |
| KNOB2 | Energy | still, calm, moving, driving, frantic |
| KNOB3 | Feel | straight, swung, waltz, lilting, odd |
| KNOB4 | Shape | loop, rise, fall, arch, question and answer |
| SELECT | Key | auto, C, D♭, D, E♭, E, F, F♯, G, A♭, A, B♭, B |
| ALGORITHM | Variation | 0 to 1023: a different piece from the same words |
| PRESETS | Logbook | Scrolls through history and saved pieces (section 8) |

Holding EDIT gives each encoder a second dial:

| Encoder + EDIT | Dial | Values |
|---|---|---|
| KNOB1 | Colour | auto, bright, warm, wistful, sad, dark, dreamy, tense, aching |
| KNOB2 | Boost | 0 to 11 |
| KNOB3 | Tempo trim | slower or faster within the energy's range |
| KNOB4 | Length | 4, 8, 12, 16, 24, 32 bars |
| SELECT | Key layout | degrees, piano snapped, chromatic (section 6) |
| ALGORITHM | Follow | you follow the band, the band follows you (section 6) |

What the words do, taken from Melisma's `docs/accessible.md`:

- **Mood** picks the scale (bright is major, sad is minor, and so on). Colour
  overrides the scale without changing the mood, so the two can be explored
  apart.
- **Energy** moves note density, register and tempo together.
- **Feel** picks the metre and groove: 4/4, swung 4/4, 3/4, 6/8, 7/8.
- **Shape** is what the piece does over its length: stay level, build, release.
- **Boost** pushes the piece harder, up to eleven.

The research behind these mappings (mode for valence and attack rate for
arousal; tempo-dependent swing; the KTH phrase arch and punctuation rules;
emotional cues for articulation and brightness) carries over as fixed-point
tables.

Two lessons from Melisma's backlog are requirements here:

- **Each role has its own lane in the bar**, so parts do not all strike on the
  same instant.
- **A shape must be audible.** A rise has to get noticeably louder and brighter,
  not just denser on paper.

## 6. Playing

### Key layouts (switchable while playing)

| Layout | What the 27 keys do | Trade-off |
|---|---|---|
| Degrees | Each key is the next note of the scale. No wrong notes exist. Nearly four octaves. | Does not feel like a piano. |
| Piano snapped | Normal piano layout. A key outside the scale plays the nearest note that fits. | Some neighbouring keys play the same note. |
| Chromatic | Normal piano, no help. | Wrong notes are possible. |

The key LEDs show what fits: home note bright, other scale notes dim, and in
chromatic layout the notes outside the scale are dark.

### Follow

- **You follow the band:** the engine's chord progression leads. In the snapped
  and degree layouts, the notes on offer can narrow to the current chord's
  notes on strong beats (a setting, so it can be compared by ear).
- **The band follows you:** when you hold the bass or chords seat, the engine
  re-harmonises the other parts around what you play.

### Per seat

- **Melody:** single notes or small chords over the band.
- **Bass:** low register, one note at a time.
- **Chords:** one key plays the whole chord built on that scale degree.
- **Pulse:** keys trigger the pulse's sounds; timing can snap to the groove.

Your playing gets the same expression treatment as the engine's parts: dynamics
and brightness follow the shape of the piece.

OCT− and OCT+ move your register (lower, higher), bounded by the seat's
sensible range.

## 7. Sound

Four voices, one designed for each role. All fixed-point, sized to fit well
inside half the 5.8 ms audio block.

| Role | Character | Polyphony (starting guess) |
|---|---|---|
| Bass | Round, simple, strong fundamental | 1 |
| Chords | Soft-attack pad or electric-piano style FM | 4 to 6 |
| Melody | Clear lead, responds most to expression | 2 to 4 |
| Pulse | Short ticks and thumps, synthesised | 3 |

Each role offers a small set of sounds chosen with FX + its role button. The
expression layer drives the sound directly: brightness, attack time and level
follow mood, energy and the shape's tension. A shared reverb or delay gives
space.

Polyphony and effects are guesses until measured on the device (section 13).

## 8. Never losing anything

### Determinism

A piece is a pure function of its settings. There is no random number
generator: where variety is needed, the engine hashes the musical position and
the variation number. The same settings give the same music on any device
running the same engine version.

### Logbook

The PRESETS encoder scrolls a single list:

- **History:** the last 64 pieces you passed through, newest first, kept in
  flash so it survives power-off.
- **Saved:** pieces you pressed SAVE on, with a name.

A piece's settings are 9 bytes, so both lists are cheap. Scrolling the logbook
auditions each piece at the next bar.

### Hold

Pressing a role's button with HOME freezes that part. Held parts keep their own
copy of the settings they were made from, so a held bass survives any amount of
dial turning and is saved with the piece.

### Capture

REC records what you play on the keys as a phrase, in time with the piece. It
loops with the piece, is saved with it, and can be overdubbed or cleared.
A phrase stores about four bytes a note, so a 4 KB blob holds around a thousand
notes.

### Saving

SAVE writes the current piece (settings, held parts, captured phrase, name) to
the logbook. Writing flash interrupts the sound for about 50 ms per sector, so
writes happen at a quiet moment or when stopped, never in a loop.

## 9. Sharing

### The code

A piece's settings pack into 68 bits:

| Field | Bits |
|---|---|
| Engine version (the Mark) | 4 |
| Mood, energy, feel, shape | 3 each, 12 total |
| Colour, key | 4 each, 8 total |
| Length | 3 |
| Variation | 10 |
| Four role states | 2 each, 8 total |
| Four role sounds | 3 each, 12 total |
| Key layout, follow | 3 |
| Tempo trim, boost | 4 each, 8 total |

With a 5-bit check value that is 73 bits, or **15 characters** in a 32-symbol
alphabet with no look-alikes (no 0, 1, I or O), shown in groups of five, for
example `K7QM2-XVD9F-4TRW8`. The check value rejects typos.

### The share screen

Shows the piece three ways:

1. **In words:** "wistful, driving, swung, arch, in D, variation 417". Anyone
   can dial this in by hand.
2. **As the code**, for copying.
3. **Entering a code:** an encoder picks each character; the words fill in as
   you go and the piece plays when the check passes.

### What the code does not carry

Held parts and captured phrases are too big for a short code. Those travel by
link or small file through the companion page (section 11). The short code
always still works for the generated piece underneath.

### Versions

The code carries the engine version. Each released version's tables are frozen,
so an old code always plays the same music. If a code comes from a newer
version than the device has, the device says so instead of playing something
different.

## 10. The look

A black instrument panel at night: the Melisma web panel's engineering-drawing
style, inverted for a small backlit screen.

- Hairlines, condensed capitals, one accent colour. No gradients, no rounded
  corners, no icons.
- Each role has its own colour (the four from the Melisma piano roll), used for
  its button LED, its lane on screen and its notes.
- A condensed caps bitmap font is added to the firmware (Dinghy ships only a
  16 px and a 32 px font). Tables live in flash and cost no RAM.

Screens:

```
BOOT: the maker's plate            MAIN: the panel
+------------------------+        +------------------------+
|                        |        | WISTFUL      DRIVING   |
|     MERLIN ENGINES     |        | SWUNG        ARCH      |
|  TYPE ....... MELISMA  |        |   ( speed )  ( bars )  |
|  MARK ............. I  |        |    needle     needle   |
|  SERIAL ...... <code>  |        | D   VAR 417            |
|                        |        | BASS CHORDS MELODY PULSE|
+------------------------+        +------------------------+

LOGBOOK                            SHARE
+------------------------+        +------------------------+
| > wistful driving  D   |        | wistful, driving,      |
|   sad calm waltz   A   |        | swung, arch, in D,     |
|   HEBRIDEAN  (saved)   |        | variation 417          |
|   dark odd rise    E   |        |                        |
|                        |        |   K7QM2-XVD9F-4TRW8    |
+------------------------+        +------------------------+
```

- **Main:** the four dial words (mood, energy, feel, shape) large; two needle gauges with limit marks
  (speed, and position in the piece in bars); key and variation; a four-lane
  strip showing each role's state and when it strikes.
- A dial being turned shows its word large for a moment, then settles back.
- **Boost at eleven** takes the needle past the limit mark.

Every screen is rendered to a PNG in host mode, so the look is reviewed on the
Mac before it reaches the device.

## 11. Companion web page (later)

A single static page, Chrome or Edge, talking to the FM-1 over USB MIDI:

- Open a link containing a code and send the piece to the device.
- Back up and restore the logbook.
- Share held parts and captured phrases as a link or file.
- Install the firmware.

Not needed for the instrument to work. The device is complete on its own.

## 12. What we build on

- **Dinghy** (GPL-3.0) for the platform: updates, USB rescue, MIDI, USB audio,
  panel, screen, storage, host mode, safety checks. Already cloned and running
  in host mode on this Mac. It supports both FM-1 flash chips.
- **Consequence:** the firmware is GPL-3.0. Melisma itself is unaffected.
- **Language:** C, fixed-point, no 64-bit division, no heap. Dinghy's build
  refuses the things known to brick units.
- **Testing:** the engine is a pure function, so host mode can render any code
  to a WAV and a note list. Golden files pin each version's output, which is
  also what makes the version freeze in section 9 enforceable.

## 13. Risks and unknowns

| Unknown | Why it matters | How we find out |
|---|---|---|
| Processor budget | Decides polyphony and whether reverb fits. | Measure on the device with `fm1_ticks()`. Host mode cannot tell us. |
| Key velocity on the FM-1 | Without it, your playing has no touch dynamics; expression comes from the shape only. | Test felucca-bsp's sensing on the device. |
| Regeneration time | A dial turn re-plans the piece; it must finish before the next bar. | Measure; plan in the main loop, never the audio interrupt. |
| "Band follows you" | Re-harmonising live is the least proven idea here. | Prototype in host mode with scripted input before committing. |
| First install | Single flash bank, no recovery button. | Full flash backup first; charged battery; Dinghy's rescue path. |

## 14. Build order (after this design is agreed)

1. **Engine in host mode:** dials to notes for all four roles, golden files,
   WAV out through placeholder sine voices.
2. **Codes and logbook:** pack, unpack, check value, history, save.
3. **Screens:** plate, main panel, logbook, share, as PNGs.
4. **Sound:** the four voices and expression, by ear in host mode.
5. **Playing:** seats, the three key layouts, key LEDs, capture, hold.
6. **On the device:** toolchain, first install, measure the budget, tune.
7. **Companion page.**

## 15. Decisions

Agreed, 10 November 2026:

| Question | Decision |
|---|---|
| Name | Melisma. |
| Licence | GPL-3.0. |
| Other firmwares | Not a concern. The unit runs this firmware; compatibility with other community firmwares is out of scope. |

Left to Claude's judgement, and easy to change later because each is a setting
or a constant:

| Question | Default chosen | Why |
|---|---|---|
| Default seat | You on melody; engine on bass, chords and pulse. | Pressing a key does something musical straight away. |
| Sound choice in the code | Included. With the style and each part's way of playing added (stage 1 of docs/PLAN.md), the code is now 18 characters: 84 bits and a check. | A shared jam should sound like the one that was shared. |
| History depth | 64 pieces. | Costs under 600 bytes. |
| MIDI clock | Follow it when present. | Lets the FM-1 sit in with other gear. |
| Button assignments | Settled once the unit is in hand. | Depends on how the panel feels to play. |

## 16. Build status (20 November 2026)

Built and tested on the Mac, not yet installed on the unit:

| Part | State |
|---|---|
| Engine (`firmware/src/mel_engine.h`) | Dials to notes for all four roles, share codes, the three key layouts. About 11 million checks in `tests/mel_test.c`; five reference pieces pinned note for note in `tests/golden/`. |
| Sound (`firmware/src/mel_synth.h`) | Two-operator FM for bass, chords and melody (three sounds each), three synthesised drums. No reverb yet. |
| App (`firmware/src/melisma.c`) | Word dials, EDIT layer, play and stop, role seats and hold, your keys, key lights, settings kept across power-off, boot plate. |
| Screen | Boot plate from `assets/splash.svg`; a first main screen (four words, the shape, the bar's lanes, key, tempo, variation, the code). The needle gauges are not drawn yet. |
| Device image | Builds (95 KB of 568 KB, 21 KB of 96 KB RAM), passes Dinghy's build checks and test suite, and boots, plays and saves in the fm1-emu emulator on the newer flash chip. |

Button assignments, now the panel is known: FX, SEL, ENV and LFO (the top row's
first four) are bass, chords, melody and pulse; EDIT is the second layer; HOME
with a role button holds it; PLAY starts and stops.

Added the same day after the first install on the unit: an output stage 8 dB
louder with a limiter, an output level trim on the device (EDIT + PRESETS,
plus or minus 9 dB), 24 shipped pieces on the PRESETS dial
(`firmware/src/mel_presets.h`), and five sounds for each pitched role, chosen
with GLO + the role's button.

Room: the image is 97 KB of 568 KB and uses 21 KB of 96 KB of RAM. A shipped
piece is 22 bytes and a sound is 12, so presets and sounds are limited by
taste, not space.

Not built yet: the logbook and history, the share screen and entering a code,
capture (REC), following MIDI clock, "the band
follows you" (the setting is stored but does nothing), the gauges, reverb, and
the interrupted-write protection from `docs/FLASHING-SAFETY.md`.

Measured but not judged: nobody has listened to it yet. Levels, timing and
note correctness are checked by tests; whether it sounds good is not.
At `frantic` more than half the strikes land with two other parts, which is
dense by the vocabulary's own definition and wants tuning by ear.

### Added 20 November 2026: performance (stage 1 of docs/PLAN.md)

Built, and described for the player in docs/PILOTS-NOTES.md:
- Parts drop out and come back on the bar line with one tap; holding takes the seat; EDIT + a part solos it.
- Fills (ARP), written by the engine: the playing bar is made again and taken up mid-bar by the interrupt.
- Sections (INTRO, A, B, BREAK, ENDING), scenes (eight, kept with the settings), BACK and ROLL, on the panel:
  SEQ turns the keys into choices.
- Each part has eight ways of playing: the chords' manners (block, open, three arpeggios, broken, rolled, offbeat),
  the bass's (written, roots, fifths, octaves, walk, triad, pedal, push), and numbered ideas for melody and pulse.
  They are in the piece and its code, which grew to 18 characters. Pieces made before are unchanged note for note.
- REC: undo of the last pass, and snapping to the beat.
- SAVE: keeps everything on demand, while playing if asked; held, it shows the code large. Its light glows when
  something is unsaved.
- A load meter (EDIT held, bottom right): the longest audio block of the last two seconds, as a percentage.

Not built from stage 1: recording a whole performance as a take; the hidden engineering page.

### Added 20 November 2026: styles (stage 2 of docs/PLAN.md)

- A style dial (KNOB3; feel moved to EDIT + KNOB3, tempo to EDIT + ALGORITHM): FREE and eleven styles, each a
  groove in the engine (`MEL_GROOVE` in mel_engine.h) with a straight and, for three of them, a different swung form.
- A groove gives drum rows and, where the research was sure of them, a bass row and a chord rhythm; anything else
  is written by the engine as before. Left out as unverified: afrobeat, baião, bembé, Balkan drum voicings, trap,
  the salsa kick, and the chord rhythms of house, funk, second line and two-step (the engine writes those).
- Energy sets the tempo within the style's range and thins the drums; it never adds notes.
- Sevenths for LOUNGE and BREEZE as rootless chords (third, fifth, seventh).
- Two more drum sounds: a rim click and an open hat (the closed tick chokes it).
- Fourteen style presets (38 in all).

Not built from stage 2: a major chord on the fifth for FIESTA in minor moods; the 12-bar form for the shuffle;
the larger 808/909 kit (clap, toms, cowbell).

### Added 22 November 2026: instruments and space (steps 1 and 2 of docs/SOUND-PLAN.md)

- mel_synth.h rewritten. A voice is an instrument from one list (`SYN_INST`; its number is what a piece keeps, so
  the list only grows at the end). Two kinds so far: FM (now with feedback and a slightly sharp modulator) and
  analogue (two band-limited sawtooths or a pulse, a sub-octave, drive, a state-variable filter run twice a frame
  with its own envelope). Flags give one-note-at-a-time with glide, unison, and a filter that follows loudness.
- Every voice: vibrato after 0.3 s, a slow drift of a few cents, a place between the two sides.
- A shared chorus (three wandering taps) and a room (four all-passes and two cross-fed loops, sized and damped
  for five rooms; its arithmetic rounds towards zero so the tail dies to silence). The bass stays out of the room.
- GLO held is the line-up page. The piece gained `room`, and instrument numbers of 6 bits: the code is 21
  characters. Pieces kept by earlier builds are renumbered when loaded, not lost.
- tests/synth_test.c measures every instrument (tuning, level, peaks, a clean ending, silence afterwards) and the
  rooms, and can write a WAV of each. tools/check.sh --full now times the heaviest band in the emulator from a
  test build: 33% of a block on average, 44% at worst, at 240 MHz.
- The training flight has a step for the line-up, and says how to leave it on every screen.

Still to come from the sound plan: plucked and struck instruments, organs and electric pianos, flute, reed and
voice, the tempo delay, and the speaker-aware bass.

### Added 23 November 2026: more instruments, easier playing, and the first public version

- 70 more instruments on the two engines (94 in all); instrument numbers are 7 bits in the code. 26 more shipped
  pieces (64).
- EASY, the new default key layout: every key is the next of five notes of the scale that sound well over
  anything (the pentatonic that fits the mood).
- Time lock (EDIT + PLAY): a key pressed off the beat waits for the next division; one near enough sounds at once.
- A level for each part (HOME + a knob), kept on the instrument, not in the piece.
- HOME + ARP: a whole new piece. Choosing an instrument at rest plays it.
- HOME + SAVE: a code typed in on the instrument (the keys are the letters).
- The settings grew; those kept by earlier builds are read at their old size and carried over.
- Docs for publication: README, INSTALL, QUICK-REFERENCE, CHANGELOG, pictures taken from the firmware
  (`tools/shots.sh`), SVG logos (`tools/gen_logos.py`), `fmhub.json`, and a release workflow that calls the FM-1
  community's shared build.
