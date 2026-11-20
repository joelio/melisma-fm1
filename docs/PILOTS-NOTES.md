# Pilot's Notes: Melisma Mark I

*Merlin Engines. For the M-VAVE FM-1 airframe.*

These notes describe the firmware as it flies today. Anything not described here is not fitted yet; see
[PLAN.md](PLAN.md) for what is coming.

## 1. What it is

Melisma is a four-piece band you sit in with. You say how the music should feel, in plain words, and the engine
writes and plays the parts: **bass, chords, melody, pulse**. You can take any one of those seats and play it
yourself on the keys. Nothing is random: the same settings always give the same music.

## 2. Controls

| Control | What it does | With EDIT held |
|---|---|---|
| KNOB1 | Mood: bright, warm, wistful, sad, dark, dreamy, tense | Colour: the scale, chosen apart from the mood |
| KNOB2 | Energy: still, calm, moving, driving, frantic | Boost: 0 to 11 |
| KNOB3 | Feel: straight, swung, waltz, lilting, odd | Tempo, within the energy's range (AUTO lets the engine choose) |
| KNOB4 | Shape: loop, rise, fall, arch, question and answer | Length: 4 to 32 bars |
| SELECT | Key (turn below C for AUTO) | Key layout: degrees, snapped, chromatic |
| ALGORITHM | Variation: another piece from the same words (0 to 1023) | Who follows whom (stored, not yet acted on) |
| PRESETS | The shipped pieces, 24 of them | Output level, −6 to +6 |
| MASTER | Volume | |
| FX, SCL, ENV, LFO | The four seats: bass, chords, melody, pulse | |
| GLO + a seat button | That part's next sound (five each for bass, chords, melody) | |
| HOME + a seat button | Hold that part exactly as it is while you change everything else | |
| PLAY | Start and stop | |
| REC | Keep what you play as a looping phrase | |
| HOME + REC | Clear the phrase | |
| OCT−, OCT+ | Your octave | |

A dial turned while the band is playing takes effect at the next bar line, so the music never stumbles.

## 3. Starting

Switch on. The Merlin Engines mark shows, then the Melisma plate, then the main screen: four words, which are
the piece. Press **PLAY**. That is all that is needed to make music.

## 4. The words

- **Mood** chooses the scale. *Bright* is major, *sad* is minor, and the others sit between and beyond.
- **Energy** moves three things together: how many notes, how high, how fast.
- **Feel** is how the bar is counted: four straight, four swung, three (waltz), six (lilting), seven (odd).
- **Shape** is what the piece does over its length: stay level, build, ease off, build then ease, or ask a
  question and answer it.
- **Boost** pushes the whole piece harder. It goes to eleven.
- **Variation** gives a different piece from the same words. Note the number and you can always come back.

## 5. Seats

Each seat button's light tells you who is playing that part:

| Light | Meaning |
|---|---|
| Lit | The engine plays it |
| Blinking | You play it. This is your seat |
| Glowing dimly | Held: frozen as it was |
| Dark | Off |

Press a seat button to take that seat. Press it again to switch the part off, and again to hand it back to the
engine. You can only sit in one seat at a time. With no seat taken, the keys play over the engine's melody.

- **Melody seat:** single notes.
- **Bass seat:** the same keys, two octaves down.
- **Chords seat:** one key plays the whole chord built on that note.
- **Pulse seat:** the keys are the three drums, repeating along the keyboard.

## 6. The keys, and wrong notes

The key lights show what fits: **home notes are lit, other notes of the scale glow, anything else is dark.**

Three layouts (EDIT + SELECT), which you can change while playing:

| Layout | What the keys do |
|---|---|
| Degrees | Every key is the next note of the scale. There are no wrong notes to find |
| Snapped | A normal piano. A key outside the scale plays the nearest note that fits |
| Chromatic | A normal piano with no help |

## 7. Sounds

Hold **GLO** and press a seat button to step to that part's next sound. The screen names it for a moment.

## 8. Recording a phrase

Press **REC** (it starts the band if it was stopped) and play. Press **REC** again and what you played loops
with the piece. Press REC and play again to add to it. **HOME + REC** clears it.

The phrase is kept as steps of the scale, not as fixed pitches. Turn the mood or the key afterwards and the
phrase moves with the band, still with no wrong notes. The REC light glows when a phrase exists.

## 9. Shipped pieces

Turn **PRESETS**. The name shows at the bottom of the screen. A preset is only the dials in a position, so
anything you do after loading one is yours. Several leave the melody seat open for you.

## 10. The screen

From the top: the four words (the one you last turned is highlighted). The shape of the piece as a line, with
the bar being played marked. Four lanes showing where each part strikes in this bar; a dashed lane is your seat.
Then the key, tempo, variation and bar count, and at the bottom the piece's 15-character code.

## 11. What is kept

Your piece, octave, level and phrase are saved about two seconds after you **stop** the band. They are not saved
while it plays, because writing to memory interrupts the sound. Stop before switching off if you want to keep
a change.

## 12. Too quiet?

Hold **EDIT** and turn **PRESETS** to raise the output level. The built-in speaker cannot reproduce the bass
part's low notes; headphones or a line out give the full band.

## 13. Emergencies

| Situation | Action |
|---|---|
| The instrument misbehaves | Switch off and on |
| It will not start properly | Hold **OCT−** while switching on: the updater, no sound. Install again from there |
| It will not start at all | Hold **REC** while switching on: download mode. Use M-Flasher to install a firmware or restore a backup |
| You want the recovery menu | Hold **PLAY** while switching on |
| You want the original M-VAVE synth back | In M-Flasher, restore the full-flash backup taken before the first install |

Full detail, with sources: [FLASHING-SAFETY.md](FLASHING-SAFETY.md).

## 14. Known limits of the Mark I

- The share code is shown but cannot yet be typed in.
- No reverb or other effects; the sound is dry.
- The keys do not yet sense how hard they are played.
- MIDI clock is not followed.
- At *frantic* the parts are dense and land on top of each other more than they should.
