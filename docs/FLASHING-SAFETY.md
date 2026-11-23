# Flashing safety: research before the first install

Researched 20 November 2026. Updated the same day after the backup: see section 2.
Numbers in square brackets refer to the sources at the end.

## 1. Bottom line

- **A permanent brick is very unlikely.** The chip has a download mode in
  mask ROM that no firmware or bootloader can erase [1][6][7]. Every known
  failure has a way back.
- **A "dark" unit is a real risk, and it has happened to people.** An
  interrupted firmware write can leave an FM-1 with a black screen and no USB
  device at all. Software cannot reach it then; a small hardware dongle
  (about £7) can [3][7][8].
- **The usual install method is the riskier one.** The normal USB-MIDI update
  takes one to two minutes and is unprotected for almost all of it [3]. Writing
  through download mode with M-Flasher takes about ten seconds and verifies
  every sector [4][5].
- **Our base (Dinghy) has no protection against an interrupted write.** It
  writes in address order and has no boot stub (checked in this repo's
  `firmware/loader/ldr_core.c`). Other firmwares have added that protection
  [9]; we should too, because development means many installs.

## 2. The unit this was developed on

| Fact | Value | How known |
|---|---|---|
| Firmware before | Stock V15 (`FM-1_015`), app checksum OK | M-Flasher |
| Flash chip | **`0xB36014`** (the newer UC25HQ80) | M-Flasher |
| Bootloader | **m-bootloader 2.4**, installed before the first custom firmware | M-Flasher |

Two full backups were taken first: one of the untouched unit, one after m-bootloader was installed. They differ
only in sectors 0 to 3 (the 16 KiB boot area), which is what a bootloader-only install should change.

Confirmed on the unit after that install: stock V15 starts normally, PLAY at power-on opens the recovery menu,
and REC at power-on reaches download mode. Dinghy 0.5's official package (release checksum and build attestation
verified) was the first custom firmware installed, then Melisma.

## 3. How FM-1s get bricked

| Cause | What happens | Evidence |
|---|---|---|
| Write interrupted: cable pulled, computer sleeps or shuts down | Black screen. Sometimes the unit lands in ROM download mode (recoverable by software); sometimes nothing on USB at all | Two reports in one thread: a cable pulled mid-install left no USB device; a laptop hibernating mid-install left the unit in download mode, recovered by software [8] |
| Write interrupted by the FM-1 itself (battery brown-out suspected) | Black screen, no USB in any mode. Key holds, waiting and restarts did nothing | Flash dump of the affected unit: 33 sectors new, 110 old, loader and update record intact but never started on a cold boot. Recovered only with the dongle [7] |
| Custom firmware installed straight over another custom firmware | Units left dark | Developers' wiki, citing TransitFM1; one tool version was withdrawn after it left a unit dark [3] |
| Firmware that does not know the newer flash chip | Not a brick: saves stay in RAM and further updates are refused, so the unit is "locked" into that firmware until rescued through download mode | [2][6][10] |
| Replacement bootloader on the newer flash chip | One unit went black after m-bootloader 1.1 or 1.2 (download mode by the REC key still worked on it). Since then, owners report 2.2 and 2.3 working on that chip, one on a brand-new unit with all three checks passing. The current 2.4 has no report on that chip yet | [11][12][13] |
| Leftover data from another firmware | Garbled presets, or a firmware that will not start although correctly written | [1][6][5] |
| Updating to V16 and going back | Garbage preset names and wrong pitch on V15 unless a backup from before is restored | [5][11] |

Why an interrupted update is so damaging: the stock bootloader starts the
firmware without checking it [11], the update loader's resume-after-power-loss
does not work from a cold boot [7], and a half-written firmware hangs before
its own rescue code runs [3][7]. Dinghy's boot guard says the same of itself:
"This cannot rescue an image that fails before reaching the application"
(`firmware/src/bootguard.h`).

## 4. The recovery ladder

From easiest to hardest [1][2][6]:

1. **Firmware starts but is broken:** hold OCT− at power-on for Dinghy's USB
   rescue, then install again. Two crashes at boot go there automatically.
2. **Unit is in an update loader:** run the installer again; it resumes.
3. **Black screen, but the Mac sees `WL82 UBOOT1.00` (USB `4C4A:8057`):** the
   ROM download mode is answering. M-Flasher restores the backup or installs a
   firmware. If macOS says a disk is unreadable, click **Ignore**, never
   Initialise.
4. **Black screen, nothing on USB:** leave it on for two minutes, try the
   OCT−/OCT+ hold, try three quick restarts [6]. These only help if the
   firmware runs far enough.
5. **Still nothing:** the FM-1 Transporter. A Seeed XIAO RP2040 (not the
   RP2350) and a USB-C pigtail cable, three wires, no soldering, VBUS not
   connected. It forces download mode whatever is in flash [7][14]. Documented
   to have recovered units in exactly this state [7].

The hardware route works on macOS [7].

## 5. The two ways to install, compared

| | USB-MIDI update (`fm1_install.py`, web installers) | Download mode (M-Flasher) |
|---|---|---|
| How | The running firmware stages a loader; the loader pulls the package over MIDI and writes it | The ROM runs a flash loader in RAM; the app writes directly |
| Duration of the write | One to two minutes [3] | About ten seconds [4] |
| What is written | App area, sector by sector, in address order | Only sectors that differ, each verified, rewritten once on mismatch [5] |
| Backup | None taken | Full flash saved to disk before the first write of a session [5] |
| If interrupted | Documented dark units [7][8] | Same theoretical end state, but a far shorter window. No report found of a dark unit from an M-Flasher firmware install |
| Known problems | Installs that stall half-way, cause unknown [3] | On one newer-chip unit the app could not find the device after switching to download mode (open, no brick) [15]. Needs the admin password each time on a Mac [15] |

**m-bootloader** changes the picture: it checks the firmware before starting
it, opens a recovery menu if the firmware is damaged, and adds a key (REC at
power-on) for download mode [11]. With it, an interrupted write becomes
recoverable from the keys. But:

- It writes the boot area, the one part that normally never changes.
- The author's hardware tests are on the older chip. For the newer chip the
  README still says "install it at your own risk" [11] and the 2.4 release
  notes list the UC25HQ80 as not tested [12].
- The README is behind the bug tracker on this. In the thread about the one
  failure, two owners of newer-chip units later reported 2.2 and 2.3 working
  (firmware starts, PLAY opens the menu, REC enters download mode), and the
  author closed it with "everything works on the chip according to reports"
  [13]. 2.4 differs from 2.3 by a removed menu item and a newer compiler [12].
- If it fails to start, the ROM download mode is still reachable, by the REC
  key if the bootloader runs at all, otherwise only by the dongle [11]. In
  the one failure on record, REC did still work [13].
- One user found a firmware that uses the chip's second core unstable under
  m-bootloader until the clock was set to 320 MHz [15]. Dinghy does not use
  the second core.

## 6. Recommended procedure

Each numbered step is a stop point. Nothing proceeds without a clear result
from the one before.

**Before anything is written**

1. Charge the FM-1 fully. The unit runs the update from its battery even on
   USB, and a brown-out is the suspected cause of the worst documented case
   [7].
2. Quit Logic and any browser tab using MIDI. Plug the FM-1 directly into the
   Mac with a known data cable, no hub [2][6].
3. Take a full flash backup with M-Flasher and read the flash chip ID. Copy
   the backup somewhere else as well. This unit's own dump, not the official
   file, is the exact way back: one unit reporting V15 did not match the
   official V15 file [1].
4. Stay on V15. Do not update to V16 [5][11].
5. **Decide on the dongle.** Buying the two parts now means every failure
   above is recoverable at home. Recommended, given that development means
   dozens of installs.

**Then, depending on the chip**

6. This unit has the newer chip and now has m-bootloader 2.4. Confirm three
   things before anything else: stock V15 starts normally; PLAY held at
   power-on opens the recovery menu; REC held at power-on reaches download
   mode (M-Flasher sees it).
7. If any of those fails: hold REC at power-on, connect M-Flasher, and restore
   the pristine backup (`fm1-20261120-131629.bin`), which puts the stock
   bootloader back.

**The first install**

8. Install the unmodified Dinghy 0.5 first, not our own code, so a failure
   points at the path and not at our changes. Use M-Flasher's download-mode
   install.
9. Keep the Mac awake and on mains power (`caffeinate -dimsu`) [7].
10. Afterwards, check: it starts, it plays, a setting survives power-off, and
    OCT− at power-on reaches the USB rescue.

**For development**

11. Do most work in host mode on the Mac; install on the device only to
    measure and to play.
12. Add interrupted-write protection to our firmware early: a boot stub that
    checks the firmware before starting it, and a loader that writes the first
    sector last. ChoralRoot and TransitFM1 measured this to cut the
    unprotected window from the whole write to about 0.1 s or none [3][9].

## 7. Where sources disagree or are thin

- **Does the update loader resume after power loss?** The loader's own header
  comment says it does. A flash dump of a dead unit shows it did not on a cold
  boot [7]. The dump is hardware evidence, so this document trusts it.
- **How often is an interrupted write dark rather than in download mode?** Not
  known. Both outcomes are documented [7][8]; the hub's rescue page says "most
  units come back" [2].
- **M-Flasher has no build attestation.** The download's SHA-256 matches the
  digest GitHub records for the release, the release was published by the
  repository's CI, and the source is public (MIT, Rust). The app is ad-hoc
  signed, not notarised by Apple [4].
- **Dinghy 0.5 itself has little hardware testing.** Its README says 0.3 was
  tested on a device; 0.4's platform code ran on devices in OberVave, and 0.5's
  in VIBE FM Link on 7 November [16].
- **The developers' wiki is community-written** and marks each fact by how it
  was verified [1][3]. Facts tagged as hardware-verified are weighted above
  those taken from code.

## 8. What has been prepared

- M-Flasher 0.12.1 is installed in `/Applications` (checksum verified against
  the release).
- The official V15 package is in `recovery/FM-1_015.fwsc`, taken from M-VAVE's
  own repository, SHA-256 `db1642b2…7edb8a`, matching the value the wiki and
  rescue tools expect [1].
- The build toolchain works and produces `build/dinghy.fwsc`; the test suite
  passes.

## Sources

1. FM-1 Firmware Developers' Wiki, "Recovery" — https://sbegueria.github.io/fm1-dev-wiki/Recovery.html
2. The FM-1 Firmware Hub, rescue page — https://thefirmwarehub.com/tools/rescue/
3. FM-1 Firmware Developers' Wiki, "Update Protocol and Installers" — https://sbegueria.github.io/fm1-dev-wiki/Update-Protocol-and-Installers.html
4. M-Flasher: repository, usage guide and release 0.12.1 — https://github.com/jvitkauskas/m-flasher · https://github.com/jvitkauskas/m-flasher/blob/main/docs/usage.md · https://jvitkauskas.github.io/m-apps/flasher/guide/
5. M-Flasher, "Safety" — https://github.com/jvitkauskas/m-flasher/blob/main/docs/safety.md
6. MvaveFM1Unbricker, README and illustrated guide — https://github.com/Quixotic7/MvaveFM1Unbricker · https://quixotic7.github.io/MvaveFM1Unbricker/
7. MvaveFM1Unbricker, "Recovering a dark FM-1 with the FM-1 Transporter" (includes the flash dump analysis) — https://github.com/Quixotic7/MvaveFM1Unbricker/blob/main/TRANSPORTER-GUIDE.md
8. fm1-x0x issue #38, interrupted installs and recoveries — https://github.com/charlesvestal/fm1-x0x/issues/38
9. TransitFM1, "How TransitFM1 keeps a firmware change safe" — https://github.com/Quixotic7/TransitFM1/blob/main/docs/SAFETY.md
10. Felucca issue #320, the newer flash chip — https://github.com/hugelton/Felucca/issues/320
11. m-bootloader README — https://github.com/jvitkauskas/m-bootloader
12. m-bootloader release 2.4 notes — https://github.com/jvitkauskas/m-bootloader/releases/tag/v2.4
13. m-bootloader issue #37 (opened 20 Nov 2026, unanswered) and M-Flasher issue #13 (black screen after m-bootloader 1.x on the newer chip, then later reports of 2.2 and 2.3 working) — https://github.com/jvitkauskas/m-bootloader/issues/37 · https://github.com/jvitkauskas/m-flasher/issues/13
14. FM-1 Transporter — https://github.com/kurogedelic/FM-1-transporter
15. M-Flasher issues #64 (device not found on a newer-chip unit), #68 (password on every connection on a Mac) and #55 (second-core instability under the custom bootloader) — https://github.com/jvitkauskas/m-flasher/issues/64 · https://github.com/jvitkauskas/m-flasher/issues/68 · https://github.com/jvitkauskas/m-flasher/issues/55
16. Dinghy README — https://github.com/zvenson/dinghy
17. FM-1 Firmware Developers' Wiki, "Boot Chain and Bootloaders", "Flash Storage", "Getting Started", "Open Questions" — https://sbegueria.github.io/fm1-dev-wiki/
18. The Synth Source, "Custom firmware on the M-VAVE FM-1: a safety-first guide" — https://thesynthsource.com/fm1-custom-firmware-install-guide/
19. FM1-Unbricking-For-Dummies (the dongle route, step by step) — https://github.com/GoodPerson42/FM1-Unbricking-For-Dummies
20. awesome-fm-1, "Firmware update and recovery" — https://github.com/cicloid/awesome-fm-1
