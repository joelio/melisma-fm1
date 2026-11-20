# Research notes, 20 November 2026

Condensed from five parallel research runs. Each fact is as reliable as its source; the confidence tags are the
researchers' own. Nothing here has been tried on our unit unless it says so.

## 1. Performance features (from other FM-1 firmwares and comparable instruments)

Gestures proven on this panel:

| Feature | Who does it, and how |
|---|---|
| Sections queued to the bar, with a fill before the change | Polly: HOME + black key 1–5; sections Intro, A, B, Fill AB, Fill BA, Ending, with fallbacks when one is empty (`charlesvestal/fm1-polly`, `app/dsp/arr.c`: `bar_line`, `resolve`). JamTrack: two parts, the bar before a switch becomes a fill (`Philip-Bos/jamtrack-fm1`, `app/dsp/seq.h`: `bar_start`). Yamaha arrangers: Main A–D with auto fill. |
| Fill on demand | Groove OS: hold FILL. SLOOP: GLO + key 9 (while held) or key 10 (the whole next bar). |
| Mutes and solo that land on the bar line | Groove OS: tap lands on the next line, hold a quarter-second to solo, hold HOME to stage several that land on release. X0X: black keys 1–5. |
| Loop record | Groove OS: REC with one-bar count-in, overdub by default, quantise 1/16. Melodee: tap REC arms; hold REC writes the last bars you played. SLOOP: hold about 2 s clears. |
| Undo, redo, clear | HOME + REC undo and HOME + PLAY redo on Groove OS, X0X and Polly: three firmwares share the convention. Groove OS: hold HOME + REC 1.5 s to clear. |
| Scenes | Groove OS: black key launches on the next bar, HOME + black key captures. |
| Reroll and keep | X0X: OCT+ writes a new line, OCT− mutates about a quarter. Tessera: ROLL. Tuna: short REC locks the phrase. |
| Dial throws that snap back | Groove OS: K1–K4 throws, HOME + SEL glides back over a bar. Felucca: hold LFO + turn is momentary. |
| A performance kept as a song | Melodee "Take jam"; MIN-1 stores the seed and each command with its tick and replays note for note. |

Poor fits for an engine that writes the notes: step entry, parameter locks, per-step probability, pattern
chaining by number.

Complaints seen in other projects' issue trackers, to avoid:
- No way to play along, or generation that cannot be switched off (fm1-dub #9, #10; fm1-amb #1; nachtfahrt #3).
- No way to set your own chord progression (pocketband #6; PERF-1 #2, 27 comments).
- Saves failing on the newer flash chip (fm1-amb #10, fm1-fomni #4, CoralFlower #7).
- Auto-save overwriting work (fm1-fomni #8).
- USB audio level tied to the master knob (fm1-polly #2, jamtrack-fm1 #1).
- A dial that regenerates a part and discards hand edits (Pocket Band's guide warns of it).

## 2. The closest relatives, and what to take from them

| Firmware | Licence | Worth taking |
|---|---|---|
| Pocket Band (`elpabl0/pocketband`) | GPL-3.0-only | **Energy as a priority threshold**: each step of each part has a rank, and a step plays if its rank clears the energy (`pb_brain.c`: `pb_prio`, `pb_wt`, `pb_gen_drums`). A fit score for rerolls (`pb_fit`). Six-letter share codes. A stage screen with a sprite per band member. |
| JamTrack (`Philip-Bos/jamtrack-fm1`) | MIT | Bass pattern strings with approach and passing notes (`styles.h` `BSTYLES`, `seq.h` `play_bass`): `R` root, `P` passing, `A` approach. Voice-led comping (`theory.h` `voice_chord`). Each style has 4/4, 3/4 and 6/8 variants. Uses float timing. |
| Polly (`charlesvestal/fm1-polly`) | GPL-3.0-only; drum data CC BY 4.0 | Chord recognition from held keys (`chords.c` `chord_recognize`). The section queue with fallbacks (`arr.c`). |
| RAKT (`chasedeanda/rakt-fm1`) | GPL-3.0-only | Parts that react to each other: bass locks to kick (`cgs_kick_bass`), melody answers the gaps in the chords (`cgs_gap_reply`) in `cf_studio.c`. |
| LOF-1 (`RustyPawz/fm1-HYP1`, `lof1/`) | GPL-3.0-only | The band follows the player's melody: score the seven chords against held and recent notes, change only when the winner beats the current chord by a margin (`amb.c` `follow`). Float. |
| Tessera (`Rastapopolix/tessera`) | GPL-3.0-only | Stateless seeded form: AB to AABC over phrases, every choice a hash of seed, phrase and salt (`form_chain.c`). |
| regent-4 (`davidpryan/regent-4`) | MPL-2.0 | White keys as scale degrees, held black keys as chord colours; chord changes "at once / on the beat / at the bar". |
| Blues Box (`provostprovost/blues`) | GPL-3.0 | A 12-bar chord grid on screen; a variation value that unlocks sevenths, sixths and walk-ups. |

Index to watch: https://fm1-editor.com/firmware/ (423 entries, a "Generative" filter, sorted by update).

## 3. Sound and effects

Budget: at 360 MHz there are about 8,160 processor cycles a sample; at 240 MHz, 5,440. Published costs on this chip:
SLOOP's drum synth "~50–60 integer ops per voice and sample"; X0X's 8-line reverb about 615 cycles a sample (7.5%
of a core); a 4-comb reverb about 435 (5%). Moving a 55 KB hot path from flash into RAM cut one firmware's
peak block time by 6–35% and stopped its dropouts. Watch peaks, not means. Keep 64-bit multiplies out of
per-sample loops (a chip erratum at 360 MHz).

Drum recipes (SLOOP's 808 and 909 kits, `tools/gen_drumkits.py`, GPL-3.0-only): a tone with an exponential pitch
drop, a 1–2 ms click, a noise layer, one filter.

| Sound | Tone | Noise and filter |
|---|---|---|
| Kick 808 | sine 50 Hz, +12 semitones falling over 32 ms, decay 820 ms | click only |
| Kick 909 | sine 52 Hz, +30 st over 24 ms, decay 340 ms, some drive | white 6 ms, low-pass 5 kHz |
| Snare 808 | sine 185 Hz, +3 st over 14 ms, decay 120 ms, partial ×1.78 | white 170 ms, high-pass 1.7 kHz |
| Snare 909 | triangle 190 Hz, +7 st over 15 ms, decay 110 ms, partial ×1.6 | white 220 ms, high-pass 1 kHz |
| Clap | none | bursts at 0, 9.5, 19 ms, then a 230 ms tail; band-pass 1,150 Hz |
| Closed hat | six squares at 205.3, 304.4, 369.6, 522.7, 540, 800 Hz | band-pass 9.2 kHz, high-pass 6.5 kHz, decay 42 ms |
| Open hat | the same source | hold 20 ms, decay 380 ms |
| Toms | sine 98 / 147 Hz, +6 st over 60 ms, decay 420 / 360 ms | white 30 ms, low-pass 2 kHz |
| Rimshot | triangle 1,700 Hz, decay 22 ms, partial ×1.47 | 6 ms noise; band-pass 2.6 kHz |
| Cowbell | two squares 540 Hz and ×1.48, decay 290 ms | band-pass 2.6 kHz |
| Shaker | none | white, decay 55 ms, band-pass 7.2 kHz |

FM patches (standard recipes, not verified; index in radians, peak → sustain):

| Patch | Carrier : modulator | Index | Envelope |
|---|---|---|---|
| Electric piano | 1:1 | 2 → 0.4 over 600 ms | decay 1.5–3 s |
| Round bass | 1:1 | 3–5 → 0.5 over 150–250 ms | decay 400 ms |
| Bell | 1:1.4 or 1:3.5 | 5–10 → 0 | decay 3–8 s |
| Brass | 1:1 | 0 → 4–5 in 50–80 ms, sustain 3 | index follows level |
| Organ | 1:1 or 1:2 | 0.5–1.5, static | instant, no decay |
| Marimba / kalimba | 1:4 / about 1:5.4 | 2 → 0 in 30–60 ms | decay 300–600 ms |
| Pluck / clav | 1:3 | 3–4 → 0.5 over 300 ms | decay 500 ms |
| Pad | 1:1 or 1:2 | 0.5–1.5, slow LFO | attack 300–800 ms, two voices ±6 cents |

Cheapest upgrade: feedback on the modulator (average its last two outputs), for saw-like basses, brass and strings.

Effects other firmwares already ship in integer code we may reuse (GPL-3.0): a four-line FDN reverb in 19 KB
(SLOOP, sloopDX, zp12); chorus, ping-pong delay and a plate reverb in OberVave's `firmware/src/obv_fx.h`; ducking
from the kick (SLOOP: one multiply per bus). Felucca-Engines (MIT) has anti-aliased saw and pulse oscillators
and a state-variable filter in fixed point.

Not reusable: X0X's break generator (no licence), Melodee's Casio and Prophet factory presets.

## 4. What the hardware can do that we are not using

| Capability | State | Notes |
|---|---|---|
| **Key velocity and pressure** | Working on real units, no modification (Velo, CTL-1, MONK-1, ChoralRoot, Felucca 1.8) | One contact per key; the ADC reads the carbon pill's resistance just after landing. 20 keys (F3–C5) get pressure, the top 7 velocity by another route. Code: `hugelton/felucca-bsp` `boards/fm1/fm1_input.h`, `fm1_adc.h` (MPL-2.0). Notes sound a few milliseconds later; the scan takes over the ADC. |
| **Headphone-jack detect** | Working (PB5, 0 = plugged; bounces about 150 ms) | `boards/fm1/fm1_jack.h`, 13 lines, MPL-2.0. |
| **The speaker** | Mutes itself when headphones go in | By SLOOP's author it plays "nothing below ~100 Hz" (a statement, not a measurement). SLOOP and Felucca cut below about 110 Hz on the speaker; Felucca's BASS+ adds harmonics at 250 Hz–1 kHz. |
| Second processor core | Working in several firmwares, but some units misread memory on it | The reported failures match our set-up (m-bootloader, nothing raising the supply rails). Leave it until we need it; if used, a check word on every job, a timeout and a fallback to one core. |
| Processor clock | 360 MHz under the stock V15 bootloader; sources conflict on what m-bootloader leaves | Measure on our unit: read the low byte of `0x10014` (6 = 360, 2 = 320) or time a loop. |
| Bluetooth MIDI | Working in fm1-base's own stack | Licence for a GPL firmware "probably yes", not confirmed. |
| USB audio in, USB MIDI host | In felucca-bsp | Host needs external power. |
| Microphone or line in | None found | Samplers record over USB. |
| Battery level | ADC channel 3 | Uncalibrated. |
| MIDI clock | In from USB and TRS; out over USB only | The real sample rate is about 44,117.6 Hz, so a clock counted in nominal samples runs fast: time it from the 24 MHz timer. |
| Measuring our own load | TIMER4 (`fm1_ticks`) | Dinghy's rule: stay under 2.9 ms of the 5.8 ms block. The key scan alone takes about 7.5% of a core. |
| More storage | Free in the Felucca-family map: 164 KB at `0xB7000`, 16 KB at `0x93000`, 52 KB at `0xE7000` | Never `0xE0000–0xE4FFF` (update staging) or `0xE6000` (where stock stages its loader). |

## 5. Styles

Grid: 16 steps to a bar unless noted. `X` accent, `x` hit, `o` soft, `.` rest. Bass: `R` root, `5` fifth, `8`
octave, `N` the next chord's root played early, `a` approach note, `t` chord tone or scale step.
Tags: [V] checked against a source, [M] from memory and probably right, [?] unsure: listen before trusting.

```
HOUSE 118-128                       K x...x...x...x... [M]   S ....x.......x... [M]   H ..X...X...X...X. [M]
                                    B ..R...8...R...8. [M]   C x..x..x...x..x.. [?]
BOOM-BAP 84-96, swing 58-62%        K x......x..x..... [M]   S ....X.......X... [M]   H x.x.x.x.x.x.x.x. [M]
                                    B R......R..R..... [M]   C x............... [M]
FUNK 96-116                         K x.x.......x..x.. [M]   S ....X..o.o.oX..o [M]   H xxxxxxxXxxxxxXxx [M]
                                    B R..R..8...R..57. [?]   C ..x..x....x..x.. [?]
REGGAE ONE DROP 65-85               K ....x.......x... [V]   S ....x.......x... [V]   H x.X.x.X.x.X.x.X. [M]
                                    B R.R...5.....3.5. [?]   C ..x...x...x...x. [M]
   steppers / dub                   K x.x.x.x.x.x.x.x. [V]
BOSSA NOVA 60-75 (two written bars) K x..xx..xx..xx..x [V]   S x..x..x...x..x.. [V]   H xxxxxxxxxxxxxxxx [V]
                                    B R..55..NR..55..N [M]   C x..x..x...x..x.. [?]
BAIAO 90-120                        K x..x..x.x..x..x. [V shape]  S ......x.......x. [?]  H xxXxxxXxxxXxxxXx [?]
                                    B R..R..5.R..R..5. [?]   C x.x.x.x.x.x.x.x. [V]
SALSA 80-110 (2-3 clave)            S ..x.x...x..x..x. [V]   H x.x.xx.xx.xx.x.x [M]   K ...x..x....x..x. [?]
                                    B ...5..N....5..N. [M]   C x.xx.x.x.x.x.x.x [M]
SECOND LINE 90-120, swing 55-60%    S XooXooXoXooXooXo [V]   K x.......x.....x. [V]   H ....x.......x... [M]
                                    B R.......5.....a. [M]   C ..x...x...x...x. [?]
SHUFFLE BLUES (12 steps) 70-130     H x.xx.xx.xx.x [M]   S ...X.....X.. [M]   K x.....x..... [M]
                                    B R.R3.35.56.6 [M]   C x.xx.xx.xx.x [M]    form: I I I I / IV IV I I / V IV I V
JAZZ SWING (12 steps) 110-200       H x..x.xx..x.x [M]   S ...x.....x.. [M]   K o..o..o..o.. [M]
                                    B R..t..t..a.. [M]   C x....x...... [M]    sevenths essential
DRUM AND BASS 160-175               K x.........x..... [M]   S ....X.......X... [M]   H x.x.x.x.x.x.x.x. [M]
   amen bars 1-2                    K x.x.......xx.... [V]   S ....X..o.o..X..o [V]
                                    B R.........R..... [M]   C a held pad
2-STEP GARAGE 128-138, swing ~58%   K x.o.......x..... [V]   S ....X..o.o..X... [V]   H ..Xo..Xo..Xo..Xo [V]
                                    B ..x..xx.x..x.x.x [V]
TRAP 130-150 (feels half)           K x......x..x..... [?]   S ........X....... [M]   H x.x.x.x.x.x.x.x. [M] with rolls
MOTORIK 120-150                     K x.x...x.x.x...x. [V]   S ....x.......x... [V]   H x.x.x.x.x.x.x.x. [V]
                                    B R.R.R.R.R.R.R.R. [M]   C x.x.x.x.x.x.x.x. [M]    no fills, ever
TRAIN BEAT 90-130                   S ooXoooXoooXoooXo [V]   K x...x...x...x... [V]
                                    B R...5...R...5... [M]   C ..x...x...x...x. [M]
WALTZ (12 steps = 3 beats)          K x........... [M]   S ....x...x... [M]   B R........... [M]   C ....x...x... [M]
BEMBE 6/8 (12 steps) 100-140        H x.x.xx.x.x.x [V]   K x.....x..... [?]   S ...x.....x.. [?]   C x...x...x... [M]
BALKAN 7/8 (14 steps), 2+2+3        K x.......x..... [?]   S ....x.......x. [?]   H x.x.x.x.x.x.x. [?]
AFROBEAT 100-130                    K X....o..X....o.. [?]   H x.xxx.xxx.xxx.xx [?]   no snare found: do not encode blind
AMBIENT 60-90                       no drums; one bass note a bar or two; held, overlapping chords
```

What makes each recognisable: house, the kick on every beat and a tick on every offbeat. Boom-bap, a loud
snare on 2 and 4 with lazy kicks. Funk, the heavy "one" and ghosted snares. One drop, no kick on beat 1 and the
offbeat chord. Bossa, quiet even ticks and a bass that arrives early. Salsa, the clave, and a bass that avoids
beat 1. Shuffle and swing, long-short triplets and (for jazz) four even bass notes. Drum and bass, fast drums
over slow bass. 2-step, no kick on 2, 3 or 4. Trap, one snare on beat 3 and hat rolls. Motorik, it never varies.

Harmony the engine cannot do yet: jazz, bossa, blues and garage need a seventh on the chord; salsa in minor and
Balkan need a major chord on the fifth.

A proposed style dial in plain words, with feel still choosing the metre:

| Word | Straight | Swung |
|---|---|---|
| Free | today's evenly spread engine | the same |
| Club | house | 2-step garage |
| Street | boom-bap | trap |
| Funk | funk | second line |
| Island | one drop (steppers when driving or frantic) | one drop |
| Breeze | bossa (samba when driving or frantic) | bossa |
| Fiesta | salsa | baião |
| Lounge | jazz swing | jazz swing |
| Roadhouse | train beat | shuffle blues |
| Motor | motorik | motorik |
| Rush | drum and bass | drum and bass |
| Drift | ambient | ambient |

From the arranger keyboards and Band-in-a-Box, worth copying: a one-bar fill before each phrase boundary and
whenever the energy changes; a verse/chorus switch; bass approach notes computed from the next chord; a
per-style "play early" amount; an ending that hits beat 1 and holds.
