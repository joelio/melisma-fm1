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
| KNOB3 | Style: free, club, street, funk, island, breeze, fiesta, lounge, roadhouse, motor, rush, drift | Feel: straight, swung, waltz, lilting, odd |
| KNOB4 | Shape: loop, rise, fall, arch, question and answer | Length: 4 to 32 bars |
| SELECT | Key (turn below C for AUTO) | Key layout: degrees, snapped, chromatic |
| ALGORITHM | Variation: another piece from the same words (0 to 1023) | Tempo, within what the energy or style allows. All the way down: the engine's own |
| PRESETS | The shipped pieces, 38 of them | Output level, −6 to +6 |
| MASTER | Volume | |
| FX, SCL, ENV, LFO | The four parts: bass, chords, melody, pulse. **Tap:** the part drops out, or comes back, at the next bar. **Hold half a second:** the seat is yours | That part alone, for as long as you hold it |
| GLO + a part's button | That part's next sound (five each for bass, chords, melody) | |
| HOME + a part's button | Hold that part exactly as it is while you change everything else | |
| SEQ + a part's button | That part's next way of playing (see section 9) | |
| ARP | A fill to the end of this bar. Held: a build, for as long as you hold it | |
| SEQ | Held, or tapped to stay: the keys become the panel (section 9) | |
| PLAY | Start and stop | |
| REC | Keep what you play as a looping phrase | Snap what you record to the beat, or keep your own timing |
| HOME + REC | Undo the last recording pass. Held for a second: clear the whole phrase | |
| SAVE | Keep everything now. Held: the piece's code, large | |
| OCT−, OCT+ | Your octave | |

A dial turned while the band is playing takes effect at the next bar line, so the music never stumbles.

## 3. Starting

Switch on. The Merlin Engines mark shows, then the Melisma plate, then the main screen: four words, which are
the piece. Press **PLAY**. That is all that is needed to make music.

## 4. The words

- **Mood** chooses the scale. *Bright* is major, *sad* is minor, and the others sit between and beyond.
- **Energy** moves three things together: how many notes, how high, how fast.
- **Style** is the band's rhythmic language (see below). *Free* is the engine writing everything itself.
- **Feel** (EDIT + KNOB3) is how the bar is counted: four straight, four swung, three (waltz), six (lilting),
  seven (odd).
- **Shape** is what the piece does over its length: stay level, build, ease off, build then ease, or ask a
  question and answer it.
- **Boost** pushes the whole piece harder. It goes to eleven.
- **Variation** gives a different piece from the same words. Note the number and you can always come back.

### The styles

A style gives the drums their pattern and, where the idiom has one, the bass its line and the chords their
rhythm. Whatever a style does not specify, the engine writes as before, and the melody is always the engine's or
yours. Many styles have a second form when the feel is *swung*.

| Style | Straight feel | Swung feel | Tempo |
|---|---|---|---|
| FREE | The engine's own writing | The same, swung | By energy, 60 to 170 |
| CLUB | House: kick on every beat, a tick on every offbeat | Two-step garage | 118 to 138 |
| STREET | Boom-bap: loud snare on two and four, lazy kicks, one held chord | The same | 84 to 96 |
| FUNK | Funk: a heavy first beat, ghosted snares, an opening hat | New Orleans second line | 90 to 120 |
| ISLAND | Reggae one drop: nothing on the first beat, chords on the offbeats | The same | 65 to 85 |
| BREEZE | Bossa nova: quiet and even, a rim-click figure, a bass that arrives early | The same | 60 to 75 |
| FIESTA | Salsa: the clave, a bass that avoids the first beat, a piano figure | The same | 80 to 110 |
| LOUNGE | Jazz: ride cymbal, a walking bass, chords with sevenths | The same | 110 to 200 |
| ROADHOUSE | Country train beat: brushes, root and fifth | Shuffle blues | 70 to 130 |
| MOTOR | Motorik: it never varies, and takes no fills | The same | 120 to 150 |
| RUSH | Drum and bass: fast drums over a slow bass and a held chord | The same | 160 to 175 |
| DRIFT | Ambient: no drums at all | The same | 60 to 90 |

- **Energy** moves the tempo through the style's own range, and thins the drums when low: at *still* only kick
  and snare remain, at *calm* half the hats. It never adds notes the style does not have.
- A style applies when the feel is straight or swung. In waltz, lilting and odd the band plays free, and the
  style's label reads "in 4 only".
- The ways of playing in section 9 work inside a style too.

Only patterns that could be checked against a source, or are standard beyond doubt, are included.
`RESEARCH-NOTES.md` lists them and the ones left out.

## 5. Seats

Each seat button's light tells you who is playing that part:

| Light | Meaning |
|---|---|
| Lit | The engine plays it |
| Blinking | You play it. This is your seat |
| Glowing dimly | Held: frozen as it was |
| Dark | Off |

**Tap** a part's button and it drops out at the next bar; tap again and it comes back. **Hold** the button for
half a second and that seat is yours: the engine stops playing it and the keys do. Tap it to hand it back. You
can only sit in one seat at a time. With no seat taken, the keys play over the engine's melody.

Hold **EDIT** and a part's button to hear that part alone for as long as you hold it.

- **Melody seat:** single notes.
- **Bass seat:** the same keys, two octaves down.
- **Chords seat:** one key plays the whole chord built on that note.
- **Pulse seat:** the keys are the five drums (thump, snap, tick, rim, open hat), repeating along the keyboard.

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
with the piece. Press REC and play again to add to it.

- **HOME + REC** undoes the last pass. Hold the two for a second to clear the whole phrase.
- **EDIT + REC** switches snapping. With it on (the default), what you record is moved to the nearest beat or
  half-beat of the piece's feel, so a loose take comes back tight. With it off, your timing is kept as played.

The phrase is kept as steps of the scale, not as fixed pitches. Turn the mood or the key afterwards and the
phrase moves with the band, still with no wrong notes. The REC light glows when a phrase exists.

## 9. Performing: fills, sections, scenes and the panel

**A fill.** Tap **ARP** and the band plays a fill to the end of the bar: the drums roll, the bass walks into the
next chord, the chords leave room. Hold ARP and the band builds for as long as you hold it.

**The panel.** Hold **SEQ** and the keys stop playing notes and become a panel of choices. Tap SEQ instead and
the panel stays up until you tap it again (its light shows). The key lights show what can be pressed, and what
is chosen is lit brightly. The top of the screen shows the section and how each part is playing.

Black keys, left to right:

| Keys | What they choose |
|---|---|
| The first five | The section: **INTRO** (chords and pulse only), **A** (the piece as dialled), **B** (more of everything, further from home), **BREAK** (chords and melody only), **ENDING** (one last bar on the home chord, and the band stops) |
| The next four | A new way of playing for the bass, the chords, the melody, the pulse |
| The next one | **BACK**: the piece as it was before your last choice here. Press again to go forward |
| The last one | **ROLL**: another variation of the same words. Its number is shown, so you can return to it |

A section starts at the next bar, and the band plays a fill to get there.

White keys, left to right:

| Keys | What they choose |
|---|---|
| The first eight | **Scenes.** Hold **HOME** and press one to keep the whole piece there. Press it alone to bring that piece back at the next bar |
| The last eight | How the chords are played: **BLOCK**, **OPEN**, **CLIMB**, **FALL**, **WAVE**, **BROKEN**, **ROLLED**, **OFFBEAT** |

CLIMB, FALL and WAVE are arpeggios: one note of the chord at a time, upwards, downwards, or up and back.

The bass has eight ways too: WRITTEN, ROOTS, FIFTHS, OCTAVES, WALK, TRIAD, PEDAL (the home note under
everything) and PUSH (it arrives early). The melody and the pulse have eight ideas each, numbered.

None of this is lost: every choice is part of the piece, kept in its code and in a scene.

## 10. Shipped pieces, the screen, and the code

Turn **PRESETS** to step through the shipped pieces. The name shows at the bottom of the screen. A preset is
only the dials in a position, so anything you do after loading one is yours. Several leave the melody seat open
for you.

The screen, from the top: the four words (the one you last turned is highlighted). The shape of the piece as a
line, with the bar being played marked. Four lanes showing where each part strikes in this bar; a dashed lane is
your seat. Then the key, tempo, variation and bar count, and at the bottom the piece's code.

**Hold SAVE** to see the code filling the whole screen, in three groups of six, for reading out or photographing.

## 11. What is kept

Your piece, scenes, octave, level and phrase are kept in two ways:

- **Tap SAVE** and everything is kept at once, even while the band plays. The sound stutters for a fraction of a
  second while it writes.
- Left alone, it is all kept about two seconds after you **stop** the band.

The **SAVE light glows** whenever something has not been kept yet. If it is glowing, tap SAVE (or stop the band
and wait a moment) before you switch off.

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
