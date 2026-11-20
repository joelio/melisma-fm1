# Melisma for the M-VAVE FM-1

*A Merlin Engines instrument.*

Custom firmware that turns the FM-1 pocket synth into a four-piece band you sit in with. You choose how the
music should feel with dials labelled in plain words; the engine writes and plays bass, chords, melody and
pulse; you take any seat and play along on the keys. Nothing is random: the same settings always give the
same music, and a piece travels as a 15-character code.

Unofficial. Not affiliated with M-VAVE. Installing firmware is at your own risk: read
[docs/FLASHING-SAFETY.md](docs/FLASHING-SAFETY.md) first.

## Where things are

| | |
|---|---|
| [docs/PILOTS-NOTES.md](docs/PILOTS-NOTES.md) | The user guide: every control, as the firmware flies today |
| [docs/PLAN.md](docs/PLAN.md) | What comes next: performance, styles, sound, sharing |
| [docs/RESEARCH-NOTES.md](docs/RESEARCH-NOTES.md) | What other FM-1 firmwares and instruments do; style patterns; hardware capabilities |
| [docs/DESIGN.md](docs/DESIGN.md) | What the instrument is and why; what is built so far (section 16) |
| [docs/FLASHING-SAFETY.md](docs/FLASHING-SAFETY.md) | How FM-1s get bricked, how to recover, the install procedure |
| `firmware/src/mel_engine.h` | The engine: word dials in, notes out. Pure, integer-only, no platform |
| `firmware/src/mel_synth.h` | The sound: FM voices and drums, in the audio interrupt |
| `firmware/src/mel_presets.h` | The shipped pieces |
| `firmware/src/melisma.c` | The app: dials, sequencer, seats, keys, screen |
| `assets/splash.svg`, `tools/gen_splash.py` | The boot plate |
| `tests/mel_test.c`, `tests/golden/` | The engine's checks and its reference pieces |
| [docs/DINGHY.md](docs/DINGHY.md), `docs/API.md`, `docs/RULES.md` | The platform underneath (Dinghy) |

## Checking a change

Everything is checked on the computer before it goes near a unit. No hosted CI: the checks are local.

```sh
tools/check.sh           # engine tests, golden pieces, host run (a few seconds)
tools/check.sh --full    # the above, then the device build, the platform's test suite and a boot in the emulator
```

A pre-commit hook runs the quick form (`git config core.hooksPath .githooks`, once per clone).
The device build needs the JieLi toolchain, the AC79 SDK and Docker; `tools/check.sh --full` says what is
missing. Host mode (`tests/run_host.sh`) writes the screens and a WAV to `build/host/`.

## Credits and licence

GPL-3.0-only. Melisma's firmware stands on [Dinghy](https://github.com/zvenson/dinghy) (Sven Trogus), which
stands on Felucca (Leo Kuroshita, Hügelton Instruments) and SLOOP (isod89); see [LICENSING.md](LICENSING.md)
and the file headers. The vocabulary and its mappings come from the Melisma project's own research notes.
