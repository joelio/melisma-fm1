# Sound research, 22 November 2026

Two research runs, condensed: a survey of the synth engines other FM-1 firmwares run on this chip (read from
their source), and instrument-modelling recipes that fit our budget in integer arithmetic. Tags are the
researchers': [V] checked against a source or code, [U] commonly cited but not checked, [E] an estimate.
Nothing here has been heard or measured on our unit.

## 1. Why our voice sounds thin

Today a voice is one sine modulating one sine, an index envelope and an amplitude envelope. The causes, in order:

1. The spectrum is frozen: every partial rises and falls together. Real instruments' partials start and die on
   their own schedules.
2. The pitch is perfect: no vibrato, drift, beating or detune.
3. No attack transient (pick, hammer, breath, mallet), which carries much of what makes an instrument recognisable.
4. The spectrum has the same shape at every pitch, with no fixed body resonance: harsh high, dull low.
5. Velocity changes loudness and a little index, not character.
6. Mono and dry.

Cheapest fixes, best value first:

| Fix | How | Cost per voice per sample |
|---|---|---|
| Reverb and chorus on the mix | shared (section 5) | none per voice |
| Velocity and key scaling of the index | at note-on; cap the index so sidebands stay under 18 kHz | none |
| Modulator feedback | `m = sin(phase_m + B * (y1 + y2) / 2)`, B 0.6 to 1.4: towards a sawtooth | 3 operations |
| Detuned modulator | ratio 1.000 to 1.002–1.004: the sidebands beat slowly | none |
| Delayed vibrato | 5–6 Hz, ±15–25 cents, 300 ms delay, 400 ms fade-in [E] | block rate |
| Slow drift | low-passed noise near 0.3 Hz, ±3 cents per voice [E] | block rate |
| Second carrier | an octave or fifth away, or +4–7 cents, at −6 to −12 dB | 8 |
| Attack noise | filtered noise, 5–20 ms, at −20 dB | 6, in the attack only |
| Filter with its own envelope | state-variable filter | 10 |
| Stereo spread | a fixed pan per voice, ±30% | 4 |

Also check the sine table: 1024 entries without interpolation has a noise floor near −44 dB (audible grit); with
linear interpolation, about −106 dB.

## 2. Engines already running on this chip

| Engine | Where | Licence | Maths | Sound | Cost per voice |
|---|---|---|---|---|---|
| ANALOG | `hugelton/Felucca-Engines` `src/eng_analog.c`, `src/dsp.c` | MIT | integer, 32-bit only | two polyBLEP oscillators, detune, drive, filter with envelope: leads, basses, brass, strings | 3–6% [E] |
| PHASE | same, `src/eng_phase.c` | MIT | integer, 32-bit | Casio CZ-style phase distortion: brass, resonant leads, bells, organ | 3–6% [E] |
| WHEEL | same, `src/eng_wheel.c` | MIT | integer, 32-bit | tonewheel organ: nine drawbars, percussion, key click, rotary | 2–5% [E] |
| TRIO | same, `src/eng_trio.c` | MIT | integer, 32-bit | three oscillators, sync, ring: sync leads, ring bells | 3–6% [E] |
| DIGITAL (4-op FM) | `hugelton/Felucca` `firmware/src/eng_digital.c` | GPL-3.0 | integer, no 64-bit in the legacy render | Rhodes, Wurli, clav, marimba, kalimba, harp | the cheapest pitched engine there |
| PHYS | `hugelton/Felucca` `firmware/src/phys_dsp.c` | MIT parts | integer but ~48 64-bit multiplies a sample | modal bars, bells, plucked strings | ~13% measured on a unit; 3 voices |
| STRINGS | `isod89/clipper-fm1` `eng_strings.c` | GPL-3.0 | integer | string machine, nine saws a voice | 2–3× ANALOG [E] |
| Obervave voice | `zvenson/obervave` `obv_dsp.h`, `obv_engine.h` | GPL-3.0 | integer; the ladder filter uses a 64-bit multiply | two oscillators, sync, cross-mod, SVF or ladder, drift | not measured |

Felucca-Engines is the one codebase with no 64-bit arithmetic anywhere, so it matches our constraints exactly.
The cost percentages are estimates converted from simulator instruction counts, not device measurements.

What makes the best-regarded leads full, from their code: two saws 12 cents apart or unison voices spread ±40
cents with random start phases; a resonant filter with its own envelope; drive before the filter; vibrato that
fades in; glide; stereo spread; chorus, delay and reverb sends.

Problems others reported, to avoid:
- Clicks at note end from a high-pass that lifts DC, and on retrigger when filter state is reset.
- Grainy fades from missing dither (Melodee fixed it with TPDF dither).
- Overload with four tracks and effects on; reverb is the costly one.
- Second-core crashes on some units; never change the clock.
- FM aliasing on high notes until the index was capped.

## 3. Building blocks, in fixed point

Formats: audio Q15 in an `int32_t`; coefficients Q15; phase a `uint32_t` (2^32 = one cycle; increment =
f × 97391.5 at 44.1 kHz). Envelopes, LFOs and filter coefficients every 16–32 frames.

**FM with feedback** [V, the Braids `RenderFeedbackFm` pattern]:
```
fb = (y1 + y2) >> 1
m  = SIN(phase_m + ((fb * Kfb) << 4))      Kfb = B * 1304
y  = SIN(phase_c + ((m  * Ki ) << 4))      Ki  = index_in_radians * 1304
y2 = y1; y1 = m
```

**State-variable filter** [V, `braids/svf.h`], about 10 operations:
```
lp += (f * bp) >> 15;  clip lp
hp  = x - lp - ((q * bp) >> 15)
bp += (f * hp) >> 15;  clip bp
```
`f = 2 sin(pi fc / fs)` from a table by pitch; `q = 1/Q`; keep fc under about 7 kHz, or run it twice a sample.

**Band-limited sawtooth** [V, `braids/analog_oscillator.cc` `RenderSaw`], about 10 operations, 25 on a wrap:
```
this = next; next = 0; phase += inc
if (phase < inc) { t = phase / (inc >> 16); this -= t*t >> 18; u = 65535 - t; next += u*u >> 18 }
next += phase >> 17;  out = (this - 16384) << 1
```
A pulse is the difference of two such saws offset by the width.

**Pitfalls:** round towards zero inside feedback loops (a plain shift rounds down and leaves DC and limit
cycles); keep a low-cutoff one-pole's state in Q23; a slow exponential decay in Q15 turns linear at the bottom,
so hold amplitude in Q31 or apply the decay once per 16 frames; free a voice from its envelope, not its signal.

## 4. Instrument models

**Plucked string** (Karplus-Strong with the Jaffe-Smith extensions) [V, J. O. Smith; Jaffe & Smith 1983]. A noise
burst through a dynamics low-pass and a pick-position comb into a delay line; the loop is delay → loss filter →
tuning all-pass. About 22 operations; 2 KB a voice (4 KB for bass).
```
y = d[(w - N) & M]
t = g0*y + g1*yp;  yp = y            g0 = rho(1-S), g1 = rho*S
l = round_to_zero(t >> 15)
a = ((C * (l - apy)) >> 15) + apx;  apx = l;  apy = a      C = (1-D)/(1+D), D in 0.2 .. 1.2
d[w] = sat16(a + excitation);  w = (w + 1) & M
```
Period `P = fs/f0`; `N = floor(P - S - 0.2)`; `D = P - S - N`. Without the all-pass an integer delay is up to 8.6
cents out at A440 and 40 cents at 2 kHz. `rho = 10^(-3 / (f0 * T60))` per note. Subtract the burst's mean and add
a DC blocker. Braids' `RenderPlucked` avoids the tuning problem by reading a fixed line at a variable rate.

| Instrument | Pick position | S | T60 at C3 | Burst bandwidth | Notes |
|---|---|---|---|---|---|
| Nylon guitar | 0.18 | 0.45 | 2.5 s | 1.5–4 kHz | |
| Steel guitar | 0.10 | 0.25 | 4 s | 3–9 kHz | stagger chord notes 8–20 ms |
| Harp | 0.45 | 0.4 | 5 s | 1–2 kHz | |
| Pizzicato | 0.2 | 0.5 | 0.4 s | 1–2 kHz | body band-pass near 300 Hz |
| Upright bass | 0.22 | 0.5 | 1.2 s | 0.6–1.2 kHz | 5 ms thump |
| Electric bass | 0.12 | 0.35 | 3 s | | pickup comb at 0.2 N, soft clip |

(All [E].)

**Struck bars and bells**: a sum of decaying sines, as Braids' `RenderStruckBell` [V]. About 7 operations a mode;
three or four modes for mallets, six for bells; a 3–8 ms low-passed thump at −18 dB; mallet hardness scales the
upper modes.

| Instrument | Frequency ratios | Decay per mode [E] |
|---|---|---|
| Marimba | 1, 4, 10 [V] | 1.0, 0.3, 0.08 s |
| Vibraphone | 1, 4, 10 [U] | 5, 1.2, 0.3 s; tremolo 4–6 Hz on the first |
| Xylophone | 1, 3, 6 [U] | 0.5, 0.15, 0.05 s |
| Glockenspiel | 1, 2.756, 5.404, 8.933 [V] | 3, 1.5, 0.7, 0.3 s |
| Kalimba | 1, 6.267, 17.55 [V] | 2, 0.2, 0.05 s |
| Tubular bell | 2.756, 5.404, 8.933, 13.34, 18.64, with f1 = 0.224 × the note [V, U] | 8 s, shorter upward |

**Electric piano**: the DX7 E.PIANO 1 decoded from the factory bank [V]: a tine pair (modulator ratio 14, fast
decay, strongly velocity-sensitive) and a body pair (1:1), carriers a few cents apart. As four operators [E]:
tine index 0.5 × velocity², decay 150 ms; body index 1.2 → 0.4 over 1.5 s, feedback 0.3; decays shorten 3% a
semitone. A cheaper alternative [E]: a sine plus a weak octave through `y = x + a x²` (a Rhodes "bark") or
`x - x³/3` (Wurlitzer), with stereo tremolo.

**Organ**: one 2048-entry table built from the drawbars when the sound is chosen (footages are 0.5, 1.5, 1, 2, 3,
4, 5, 6, 8 times the note [U]; about 3 dB a drawbar step), one interpolated read a sample. Key click: start at a
random phase with a 3 ms noise burst. Percussion: the second or third harmonic decaying over 0.25–1 s. Rotary
speaker on the bus [E]: split at 800 Hz; horn 0.8 / 6.7 Hz with a 1 ms ± 0.4 ms delay and tremolo, sides opposed;
drum 0.7 / 5.8 Hz tremolo only.

**Strings and pads**: two band-limited saws ±6–8 cents (or a pulse with its width swept at 0.5–1 Hz) into the
filter at 1.5–4 kHz; attack 150–400 ms, release 0.5–1.5 s. The string-machine sound is the ensemble chorus below.

**Brass and reeds**: a filtered saw whose cutoff follows the loudness, attack 40–70 ms, pitch starting 20–30 cents
flat [E]; or FM 1:1 with the index following the amplitude and some feedback. A clarinet waveguide exists in
integer code (Braids `RenderBlown`, about 30 operations) but is fussy: it squeaks or fails to speak outside a
narrow range. A cheap flute: sine, a little octave, band-passed breath noise and a chiff.

**A full lead** [E]: one note at a time; no retrigger on legato; glide 60–120 ms; two saws 5 cents apart plus a
square an octave down; filter near 1.2 kHz with two octaves of envelope and some resonance; soft clip before the
filter; vibrato 5.5 Hz ±20 cents after 300 ms.

**Voice-like**: Braids `RenderVosim` (integer) [V], about 30 operations.

## 5. Shared effects

| Effect | Design | Memory | Cost per frame [E] |
|---|---|---|---|
| Ensemble chorus | Plaits `fx/ensemble.h` [V]: three taps a side around 4 ms, LFOs 0.75 Hz (deep) and 6.57 Hz (shallow), 120° apart | 2 KB | ~60 operations |
| Reverb | Rings `fx/reverb.h` [V]: four input all-passes (150, 214, 319, 527) and a two-branch tank (2182, 2690, 4501; 2525, 2197, 6312 at 48 kHz) | 43 KB, or ~22 KB at half rate | ~80, or ~45 at half rate |
| Tempo delay | mono ping-pong, low-pass in the feedback | up to 88 KB for 1 s | ~15 |
| Rotary | for organs only | small | ~35 |

Together about 3% of the processor.

## 6. Directly portable integer code (MIT)

- `pichenettes/eurorack` `braids/digital_oscillator.cc`: `RenderPlucked`, `RenderStruckBell`, `RenderBlown`,
  `RenderFluted`, `RenderVosim`, `RenderFeedbackFm`, `RenderSawSwarm`; `braids/analog_oscillator.cc`:
  `RenderSaw`, `RenderSquare`; `braids/svf.h`; `stmlib/utils/dsp.h` for the BLEP helpers.
- `hugelton/Felucca-Engines` `src/dsp.c`: `osc_saw`, `osc_pulse`, `blep`, `tsvf_coef`, `tsvf_lp`, `softclip`.
- Plaits and Rings effects are float but short; their constants are the ones above.

## Sources

- Braids: https://github.com/pichenettes/eurorack/tree/master/braids
- Plaits ensemble: https://github.com/pichenettes/eurorack/blob/master/plaits/dsp/fx/ensemble.h
- Rings reverb: https://github.com/pichenettes/eurorack/blob/master/rings/dsp/fx/reverb.h
- Felucca-Engines: https://github.com/hugelton/Felucca-Engines
- Felucca: https://github.com/hugelton/Felucca
- Clipper: https://github.com/isod89/clipper-fm1
- Obervave: https://github.com/zvenson/obervave
- J. O. Smith, Extended Karplus-Strong: https://ccrma.stanford.edu/~jos/pasp/Extended_Karplus_Strong_Algorithm.html
- DX7 factory bank: https://homepages.abdn.ac.uk/d.j.benson/pages/html/dx7.html
- Yamaha, the structure of the marimba: https://www.yamaha.com/en/musical_instrument_guide/marimba/mechanism/mechanism003.html
