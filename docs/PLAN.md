# Plan: from Mark I to an instrument you perform on

Draft for Joel to review, 20 November 2026. Based on [RESEARCH-NOTES.md](RESEARCH-NOTES.md). Nothing here is
built yet. Each stage ends with something to install and play.

## What is missing today

Mark I can be steered but not *performed*. Changes are slow to make (one dial at a time), there is one rhythmic
language for every piece, the sound is dry, and the bass disappears on the speaker.

## Stage 1: Performance

The aim: both hands busy, the music changing on the bar, nothing lost.

| Feature | Gesture | Notes |
|---|---|---|
| Mutes on the bar line | Tap a seat button: the part drops out or comes back at the next bar | Today a tap cycles three states; this makes the common move one tap |
| Take a seat | Hold a seat button for half a second | Keeps seats, frees the tap for muting |
| Solo | EDIT + seat button, while held | |
| Fill | Tap **ARP**: a fill to the end of this bar. Hold it: a build that lasts as long as you hold | Written by the engine from the bar number, so it replays the same |
| Sections | Hold **SEQ** + a black key: intro, A, B, break, ending. Lands on the next bar, with a fill in the bar before | A section is a small offset to the dials, not a separate pattern |
| Scenes | Hold **SEQ** + a white key to recall a scene on the next bar. **SEQ + HOME** + a white key captures the current dials and mutes | 16 scenes, a few bytes each |
| Reroll a part | **SEQ** + a seat button: a new idea for that part at the next bar. Again within two bars: go back | Each part gets its own variation number |
| Undo a recording pass | **HOME + REC** undoes the last pass; hold for a second and a half to clear | The convention three other FM-1 firmwares share |
| Count-in and snap | REC waits for the bar line; a setting snaps your timing to the grid | |

Also in this stage, because they are cheap and make everything else trustworthy:
- A load meter on the device (how much of each audio block we use, and the worst case).
- The processor clock read at start-up and shown on a hidden engineering page.

## Stage 2: Styles

A new word dial, **style**, giving the band a rhythmic language: *free, club, street, funk, island, breeze,
fiesta, lounge, roadhouse, motor, rush, drift*. Feel still chooses the metre; each style has a straight and a
swung form (club is house straight and two-step swung).

- **How:** every step of every part gets a rank in a small table per style, and a step plays when its rank
  clears the energy. *Free* is today's evenly spread engine. This is the method Pocket Band uses, and it keeps
  energy meaning what it means now.
- **Bass that leads somewhere:** approach and passing notes into the next chord.
- **Richer harmony where a style needs it:** sevenths for lounge, breeze and roadhouse; a major chord on the fifth
  for fiesta.
- **A bigger drum kit:** kick, snare, clap, closed and open hat, tom, rim, cowbell, shaker, in an 808 and a 909
  flavour, chosen by the style.
- **Where the dial goes:** style takes KNOB3 on the main layer and its word on the screen; feel moves to
  EDIT + KNOB3; tempo moves to EDIT + ALGORITHM. "Who follows whom" stops being a setting (see stage 4).

Caution: about a third of the researched patterns are tagged as unsure. Each style needs your ears before it is
called done. I will mark the unsure ones in the code and in the notes.

The share code grows by four bits for the style. No code has been shared yet, so the format changes now, once.

## Stage 3: Sound

- **Speaker-aware voicing:** detect headphones; on the speaker, cut what it cannot play and add harmonics so the
  bass is heard.
- **Space:** a small reverb and a tempo-synced delay, with a send level per part, and the bass and chords
  ducking under the kick. Other FM-1 firmwares run these within budget in integer code we may reuse.
- **Better instruments:** modulator feedback (for saw-like basses, brass, strings), and a larger set of named
  sounds: electric piano, organ, bell, brass, marimba, kalimba, clav, pad, plucked string.
- **Touch:** velocity and pressure from the keys. The biggest gain in playability, and the riskiest port in this
  plan because it replaces the key scan. Done last in this stage, behind a setting.

Not planned: the second processor core. Some units misread memory on it and ours matches the reported
conditions. We use it only if the load meter says we must.

## Stage 4: Keeping, sharing, following

- **Logbook:** the last 64 pieces you passed through, and named saves (**SAVE**).
- **Codes:** a share screen, and entering a code on the device.
- **A take:** record a whole performance (sections, mutes, fills, dial moves) and replay it exactly.
- **The band follows you:** in the bass or chords seat, the engine re-harmonises around what you play; in the
  melody seat, it follows the chords your tune implies, changing only when it is sure.
- **MIDI clock** in and out, timed from the hardware timer.
- **Interrupted-write protection**, from the flashing safety notes.
- **Companion page:** open a link, send a piece to the unit.

## The Merlin Engines character

Kept throughout: the mark at power-on, the data plate, Sky as the one accent, DIN capitals, plain words.
Additions, all small:

| Touch | What it is |
|---|---|
| Hours run | The plate shows total playing hours, like an engine log. Kept across power-off |
| Serial number | The plate carries a serial derived from the unit itself |
| Through the gate | Turning boost to eleven shows a brief "THROUGH THE GATE", the wartime term for emergency boost |
| Run-up | Holding PLAY at power-on plays a short engine start: a low growl catching and settling |
| Engineering page | A hidden page (a button combination at power-on) with the load meter, clock, flash chip and build number, laid out as an instrument check card |
| Holding pattern | Left alone for five minutes, the screen becomes a slow radar sweep |
| Squadron numbers | A few variation numbers do something of their own: 617 skips like a bouncing bomb, 303 goes acid |
| ACE | Play A, C, E in order and a small tally mark is chalked on the plate |
| Pilot's Notes | The user guide, [PILOTS-NOTES.md](PILOTS-NOTES.md), written as one |

None of these change the music unless asked for, and none appear on the main screen uninvited.

## Order, and what I need from you

I would build in the order above: performance first (it changes how the instrument feels at once, and needs no
new sound work), then styles, then sound, then keeping and sharing.

Decisions:
1. Is that order right, or do styles or sound matter more to you first?
2. Are the performance gestures above acceptable, in particular tap-to-mute with hold-to-take-a-seat?
3. Is moving feel under EDIT, to give style a main dial, acceptable?
4. Which of the Easter eggs do you want, and are any too much?
