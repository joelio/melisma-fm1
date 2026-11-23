# Licensing

Melisma is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`, full text in
`LICENSE`). It is built on Dinghy, and what follows first is Dinghy's own statement, which still applies to the
platform; Melisma's own parts and sources are listed after it.

## Dinghy

Dinghy is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`, full text in
`LICENSE`). You may use, study, change and share it. If you give someone a firmware built from it, give them its
complete source under the same licence too.

## Who made what

| | |
| --- | --- |
| The platform: `firmware/hal/`, `firmware/loader/`, `crt0.S`, `app.ld`, `firmware/src/` (`usb.c`, `ota.c`, `recovery.c`, `lcd.c`, `gfx.c`, `libc.c`, `bootguard.h`, `uac_fir.h`), `tools/` | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments ([Felucca](https://github.com/hugelton/Felucca)), with the changes of [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) |
| The SMK-37 board layer: `firmware/hal/smk_input.h`, `fm1_flash_map.h`, the SMK-37 parts of `fm1_cc.h`, `fm1_flash.h`, `fm1_lcd_hw.h`, `lcd.c`, `ota.c`, `bootguard.h`, `firmware/loader/`, `tools/fm1pkg_make.py`, `tools/fm1_install.py`, `tools/build.py`; `firmware/src/smk_wheel.h`; `tests/ota_test.c`, `ldr_test.c`, `install_test.py`, `smk_wheel_test.c`, `smk_pkg_test.py` | Copyright (C) 2026 M. Geisler (genkigenki/makkuro), from [genkigenki/sloop-fm1](https://github.com/genkigenki/sloop-fm1) branch `smk37` (the reference port of the [fm1-to-smk37](https://github.com/genkigenki/fm1-to-smk37) kit) |
| The TRS MIDI IN `firmware/src/midi_uart.c` | Felucca's, as sloopDX carries it, with the ring read by content from Felucca [Salt] 0.9-salt14 by ChanceTheMaker (found by keremimo) |
| The update loader's screen: `firmware/loader/ldr_screen.c`, `tests/ldr_screen_test.c` | Copyright (C) 2026 zvenson (from OberVave, the Firmware Hub's look); the idea, a loader that draws, its 5 x 7 letters and the order of things from [Skiff](https://github.com/illuminus5/skiff) by illuminus5, GPL-3.0 |
| From OberVave: the storage (blobs, slots), `LCD_IDLE`, the SMK-37 wiring of `platform.c`, `store.h`, the pad event queue in `smk_input.h`, `tests/smk_flash_test.c`, `tests/run_tests.sh`, the CI workflow | Copyright (C) 2026 zvenson |
| The wiring `platform.c`, the host mode `tests/`, the platform docs (`docs/DINGHY.md`, `API.md`, `RULES.md`, `MAKE-IT-YOURS.md`, `SMK37.md`) | zvenson |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Terminus font 8x16 (ter-u16n), rendered into `firmware/gen/felucca_font.h` | SIL OFL 1.1 | `assets/fonts/Terminus-LICENSE.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## Melisma's own parts

| | |
| --- | --- |
| The instrument: `firmware/src/melisma.c`, `mel_engine.h`, `mel_synth.h`, `mel_presets.h`; `tests/mel_test.c`, `tests/synth_test.c`, `tests/golden/`, `tests/scripts/`; `tools/check.sh`, `tools/shots.sh`, `tools/gen_splash.py`; the docs other than the platform's | Copyright (C) 2026 Merlin Engines (github.com/joelio), GPL-3.0-only |
| The Merlin Engines mark and the Melisma plate: `assets/logo/`, `assets/merlin-engines.svg`, `assets/splash.svg`, `assets/logo-takes/`, `firmware/gen/mel_splash.h`, `docs/images/` | Copyright (C) 2026 Merlin Engines. They mark this project; a fork should use its own |

## What Melisma's sound follows

`mel_synth.h` was written for this project, in integer arithmetic. Its designs follow published open-source work,
credited here with thanks:

| What | From | Licence |
| --- | --- | --- |
| The band-limited sawtooth, the state-variable filter, feedback FM | Braids by Emilie Gillet (`pichenettes/eurorack`: `braids/analog_oscillator.cc`, `braids/svf.h`, `braids/digital_oscillator.cc`) | MIT |
| The reverb's layout and delay lengths | Rings by Emilie Gillet (`rings/dsp/fx/reverb.h`), after Griesinger | MIT |
| The chorus's taps and rates | Plaits by Emilie Gillet (`plaits/dsp/fx/ensemble.h`) | MIT |
| What an analogue voice and a lead need | Felucca-Engines by Leo Kuroshita, Hügelton Instruments (`hugelton/Felucca-Engines`) | MIT |

The MIT licence of those works: "Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following
conditions: The above copyright notice and this permission notice shall be included in all copies or substantial
portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND." (Copyright (c) 2012 to
2020 Emilie Gillet; Copyright (c) 2026 Leo Kuroshita.)

The style patterns are common musical practice; `docs/RESEARCH-NOTES.md` lists where each was checked.

## Dinghy's logo

Dinghy's own logo (the designburg signet) is not part of this repository: it marks Dinghy itself.

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments; "M-VAVE", "FM-1", "SMK-37" and "MKE-P37"
belong to their owners. Neither Dinghy nor Melisma is made or endorsed by them.
