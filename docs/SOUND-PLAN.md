# Sound plan: real instruments for the band

Agreed with Joel on 22 November 2026, on condition that it is documented and in the tutorial. **Steps 1 and 2 are built** (the end of [DESIGN.md](DESIGN.md) says what they became). It replaces "Stage 3: Sound" in [PLAN.md](PLAN.md). The research
behind it is in [SOUND-RESEARCH.md](SOUND-RESEARCH.md). Nothing here is built yet.

## The problem

Every sound today is the same recipe (one sine bending another) with different numbers, played dry, with no
movement. So the fifteen sounds are shades of one thin timbre, and the way to choose one (GLO + a part's
button) is hidden.

## What changes

1. **Five kinds of voice instead of one**, each a different way of making sound:

   | Kind | How it makes sound | Good for |
   |---|---|---|
   | Analogue | two detuned sawtooth or pulse oscillators, a sub-oscillator, a resonant filter with its own envelope, drive | leads, synth basses, pads, brass, strings |
   | Plucked | a modelled string: a burst of noise ringing in a tuned delay line | guitars, harp, pizzicato, upright and electric bass, clav |
   | Struck | a handful of decaying partials at a bar's or bell's own ratios, with a mallet thump | marimba, vibes, glockenspiel, kalimba, tubular bell |
   | Organ | drawbars summed into one waveform, key click, percussion | jazz, rock and pipe organ |
   | FM, improved | today's voice with feedback, a second pair for the electric piano's tine, velocity shaping | electric pianos, bells, FM bass and brass |

2. **Life in every voice**: vibrato that fades in on held notes, slow drift, a little detune, harder playing
   sounding brighter and not just louder, each voice placed across the stereo field.

3. **Space on the whole band**: a chorus, a small reverb and a tempo delay, shared by all parts, with one dial
   for how much.

4. **A lead that plays like a lead**: one note at a time with glide between joined notes, so a melody sings
   instead of chiming.

5. **A bass you can hear on the speaker**: the unit knows when headphones are in. On the speaker the bass gets
   harmonics an octave up and the lows the speaker cannot play are removed.

## The instruments (34)

**Bass**: Fingered Bass, Upright, Moog Bass, Acid, FM Bass, Reese.

**Keys, chords and pads**: Tine Piano, Suitcase Piano, Wurli, Jazz Organ, Rock Organ, Pipe Organ, Nylon Guitar,
Steel Guitar, Harp, Pizzicato, Clav, Marimba, Vibes, Glockenspiel, Kalimba, Tubular Bell, FM Bell, String Machine,
Analogue Pad, Choir.

**Lead**: Mono Lead, Unison Saw, Square Lead, Synth Brass, FM Brass, Flute, Reed, Vox Lead.

Any part may use any instrument: a marimba bass line or an organ lead is allowed.

## Choosing sounds on the unit

Hold **GLO** and the screen becomes the band's line-up, in the same big words as the main screen:

| While GLO is held | Turns |
|---|---|
| KNOB1 | the bass's instrument |
| KNOB2 | the chords' instrument |
| KNOB3 | the melody's instrument |
| KNOB4 | the room: dry, close, room, hall, far |

The name of each instrument is on the screen as you turn. The instruments and the room are part of the piece, so
they are in its code and its scenes. The code grows from 18 to 21 characters.

The training flight gains a step for this.

## How it fits the processor

- All of it is integer arithmetic with no 64-bit multiplies, written from two open-source bodies of integer code
  we are licensed to use (Mutable Instruments' Braids, and Felucca-Engines, both MIT).
- Estimated worst case: twelve voices at about a fifth of the processor, effects about 3%. These are estimates.
  The load meter already on the unit (hold EDIT, bottom right) will tell us the truth at each step.
- The reverb needs 22 to 43 KB of memory, the delay up to 88 KB. We have about 65 KB of ordinary memory free and
  a further 300 KB pool.
- The second processor core is not used.

## Order of work

| Step | What you get | Why this order |
|---|---|---|
| 1 | Life and space: vibrato, drift, detune, feedback, velocity shaping, stereo, chorus and reverb, on the sounds that exist. The GLO line-up page. | The biggest improvement for the least risk; nothing is replaced yet |
| 2 | The analogue voice: Mono Lead, Unison Saw, Square Lead, Synth Brass, Moog Bass, Acid, Reese, Analogue Pad, String Machine | It answers the thin lead directly |
| 3 | Plucked and struck: the guitars, harp, basses, clav, mallets and bells | The most different timbres from what you have |
| 4 | Organs and electric pianos | |
| 5 | Flute, reed, choir and vox; the tempo delay; the speaker-aware bass | The fussiest models last |

Each step ends with a package to install and listen to.

## What I cannot do

I cannot hear. Every instrument will be checked by measurement (tuning to the cent, decay times, harmonic
content, level, no clipping, no clicks at note ends), and built from published recipes, but whether a marimba
sounds like a marimba is for your ears. Expect a round of tuning by ear after each step: tell me which sounds
are wrong and how, and I will adjust them.

## Decisions

1. Is the order right? If the lead matters most, steps 1 and 2 can be one package.
2. Is GLO + the four knobs acceptable for choosing instruments and the room?
3. Is the list missing anything you particularly want (a piano, a specific synth sound, a sitar)?
4. May the share code grow to 21 characters?
