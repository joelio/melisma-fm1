<p align="center"><img src="../assets/logo/melisma.svg" width="130" alt="Melisma"></p>

# Installing Melisma, and going back

Melisma replaces the FM-1's own firmware. Nothing here is permanent: you can return to M-VAVE's firmware at any
time. Installing any firmware is at your own risk; the steps below are the careful way to do it.

## Before you start

- **Which FM-1s.** Melisma was developed on a unit with the newer flash chip (UC25HQ80, id `0xB36014`). The
  platform it is built on supports the older Puya chip too, but Melisma itself has not been tried on one.
  M-Flasher tells you which chip you have.
- **What you need.** A USB data cable, and [M-Flasher](https://jvitkauskas.github.io/m-apps/flasher/) (free, for
  Windows, macOS and Linux).
- **Stay on M-VAVE's V15 or earlier.** Do not update the stock firmware to V16 first: it changes the boot area in
  a way that makes custom firmware harder to install.

## 1. Take a backup

In M-Flasher, connect the FM-1 and choose **Backup**. Keep the file somewhere safe. It is a complete copy of
your instrument, including M-VAVE's firmware and your own patches, and it is how you go back.

## 2. Install

1. Download `melisma-X.Y.fwsc` from the [latest release](https://github.com/joelio/melisma-fm1/releases/latest).
2. Put the FM-1 in **download mode**: switch it off, then hold **REC** while switching it on (with m-bootloader),
   or follow M-Flasher's own prompt for your unit.
3. In M-Flasher, choose **Install firmware**, pick the `.fwsc` file, and wait for it to finish and verify. It
   takes about ten seconds.
4. Switch the FM-1 off and on.

You should see the Merlin Engines mark, then the Melisma plate, then the **training flight**: a short lesson that
teaches the instrument. HOME + PLAY leaves it.

The Firmware Hub's web installer and other FM-1 installers that take `.fwsc` packages should work too; M-Flasher
in download mode is the route that was tested.

## 3. Updating to a newer Melisma

Install the newer `.fwsc` the same way. Your piece, scenes, phrase and levels are kept across updates where the
stored layout allows; the release notes say when it does not.

## Going back

To return to M-VAVE's firmware, in M-Flasher either:

- **Restore** the backup you took in step 1 (this brings back everything, exactly as it was), or
- **Install** M-VAVE's own package (V15, `FM-1_015.fwsc`, from M-VAVE or the Firmware Hub).

To move to another community firmware, install its `.fwsc` the same way.

## If something goes wrong

| What you see | What to do |
|---|---|
| Melisma starts but misbehaves | Switch off and on. If it persists, install it again |
| A blank or frozen screen after an install | Hold **OCT−** while switching on. This starts the updater, which is separate from Melisma. Install again from there |
| Nothing at all | Put the FM-1 in download mode (hold **REC** while switching on, with m-bootloader) and install or restore with M-Flasher. Download mode is in the chip itself and cannot be erased |
| Download mode will not start either | See [FLASHING-SAFETY.md](FLASHING-SAFETY.md) for the recovery ladder, including the hardware route |

[FLASHING-SAFETY.md](FLASHING-SAFETY.md) explains how FM-1s get into trouble and how they are recovered, with
its sources.

## How installers see it

Melisma's package identity is `FM-1_1650XY` for version X.Y (1.0 is `FM-1_165010`). It carries the Dinghy update
loader, so another firmware can be installed over it in the usual way.

## Checking a download

Each release is built by GitHub from the tagged source and carries a build attestation. To check a package:

```sh
gh attestation verify melisma-X.Y.fwsc --repo joelio/melisma-fm1 \
  --signer-workflow zvenson/fm1-build/.github/workflows/build.yml
```
