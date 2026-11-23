/* SPDX-License-Identifier: GPL-3.0-only */
/* Melisma's sound. A voice is one of several kinds of instrument, with a life of its own (a vibrato that comes in on
 * a held note, a slow drift, a place across the two sides), and the whole band plays into a chorus and a room.
 *
 *   FM        a sine bent by a sine, the bender fed back into itself: pianos, bells, the first sounds
 *   ANALOGUE  two sawtooth oscillators (or a pulse), one under them an octave down, a little drive, and a resonant
 *             filter with an envelope of its own: leads, basses, brass, pads, strings
 *
 * The pulse is five drums. Integers only, and nothing wider than 32 bits. Everything here runs in the audio
 * interrupt, and the interrupt is the only one who touches a voice: the main loop asks through melisma.c's queue.
 *
 *   syn_note_on(role, note, vel, hold, bright, attack, inst)   hold: frames, or SYN_HELD until syn_note_off
 *   syn_drum(note, vel)                                        syn_room: 0 dry .. 4 far
 *   syn_render(out, n)                                         adds n stereo frames (n <= 256)
 *
 * The oscillators, the filter and the reverb follow Emilie Gillet's Braids and Rings (MIT; pichenettes/eurorack:
 * braids/analog_oscillator.cc, braids/svf.h, rings/dsp/fx/reverb.h), the chorus her Plaits ensemble; the choice of
 * what a lead needs follows Felucca-Engines' ANALOG (MIT; hugelton/Felucca-Engines). docs/SOUND-RESEARCH.md. */
#ifndef MEL_SYNTH_H
#define MEL_SYNTH_H
#include <stdint.h>

#define SYN_VOICES 12u
#define SYN_HELD 0xFFFFFFFFu
#define SYN_CTL 32u                             /* envelopes, pitch and the filter move once in this many frames */
#define SYN_BLOCK 256u

/* ---- the instruments ----------------------------------------------------------------------------------- */
enum { SYN_FM, SYN_VA };
#define SYN_MONO 1u                             /* one note at a time, gliding from the last */
#define SYN_UNI 2u                              /* the part's voices stacked on a note, spread in pitch and place */
#define SYN_FOLLOW 4u                           /* the filter opens with the loudness (brass) */
#define SYN_PULSE 8u                            /* the second oscillator is taken from the first: a pulse, its width moving */
#define SYN_MOOD 255u                           /* (attack) as the mood asks */
typedef struct {
    const char *name;
    uint8_t home;                               /* the part it is first offered to: 0 bass, 1 chords, 2 melody */
    uint8_t eng, flags;
    uint8_t attack;                             /* which of SYN_ATTACK_FRAMES, or SYN_MOOD */
    uint8_t dec_shift, rel_shift;               /* speeds: a bigger shift is slower */
    uint8_t sustain;                            /* the level a held note settles at, /16 of its peak */
    uint16_t level;                             /* peak, Q15, at full velocity */
    uint8_t vib, cho, rev;                      /* vibrato, cents; how much goes to the chorus and to the room, /128 */
    /* FM */
    uint8_t rn, rd;                             /* modulator : carrier frequency */
    uint16_t idx;                               /* how far the modulator bends the sine at its brightest (82 = one radian) */
    uint8_t idx_keep, idx_shift;                /* how much of that stays, /16, and how fast it goes */
    uint8_t fb;                                 /* the modulator bent by itself (42 = one radian) */
    /* analogue */
    int8_t det, semi2;                          /* the second oscillator: cents and semitones from the first */
    uint8_t mix2, sub, drive;                   /* its share /128 (64: equal); the octave below /128; how hard the filter is driven */
    uint8_t cut, res, env, fdec, fsus, track;   /* the filter: where it rests (a note number), resonance, how many semitones
                                                   its envelope opens it, how fast that falls, to what (/16), and how far it
                                                   follows the keyboard (/16) */
} syn_inst_t;
#define FM(name, home, attack, dec, rel, sus, level, vib, cho, rev, rn, rd, idx, keep, ishift, fb) \
    {name, home, SYN_FM, 0, attack, dec, rel, sus, level, vib, cho, rev, rn, rd, idx, keep, ishift, fb, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
#define FMF(name, home, flags, attack, dec, rel, sus, level, vib, cho, rev, rn, rd, idx, keep, ishift, fb) \
    {name, home, SYN_FM, flags, attack, dec, rel, sus, level, vib, cho, rev, rn, rd, idx, keep, ishift, fb, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
#define VA(name, home, flags, attack, dec, rel, sus, level, vib, cho, rev, det, semi2, mix2, sub, drive, cut, res, env, fdec, fsus, track) \
    {name, home, SYN_VA, flags, attack, dec, rel, sus, level, vib, cho, rev, 1, 1, 0, 0, 0, 0, det, semi2, mix2, sub, drive, cut, res, env, fdec, fsus, track}
/* An instrument's number is what a piece and its code keep: add at the end, never reorder. */
static const syn_inst_t SYN_INST[] = {
    FM("ROUND BASS",   0, SYN_MOOD, 7, 3, 11, 13500, 0, 0, 0,     1, 1, 150, 4, 5, 30),
    FM("EDGE BASS",    0, SYN_MOOD, 6, 3, 9, 12300, 0, 0, 0,      2, 1, 220, 3, 4, 40),
    FM("SUB BASS",     0, SYN_MOOD, 7, 3, 12, 13500, 0, 0, 0,     1, 2, 120, 8, 6, 20),
    FM("PLUCK BASS",   0, SYN_MOOD, 6, 3, 8, 11500, 0, 0, 0,      3, 1, 260, 2, 3, 30),
    FM("SOFT BASS",    0, SYN_MOOD, 8, 3, 13, 13500, 0, 0, 0,     1, 1, 60, 10, 7, 0),
    FM("FM PIANO",     1, SYN_MOOD, 7, 5, 8, 5600, 0, 70, 50,     1, 1, 130, 2, 5, 20),
    FM("FM ORGAN",     1, SYN_MOOD, 9, 6, 12, 4700, 6, 90, 55,    2, 1, 90, 6, 7, 30),
    FM("FM PLUCK",     1, SYN_MOOD, 6, 4, 5, 5600, 0, 50, 55,     3, 1, 160, 1, 4, 20),
    FM("SOFT PAD",     1, SYN_MOOD, 9, 7, 13, 5600, 8, 110, 70,   1, 1, 40, 12, 8, 35),
    FM("GLASS",        1, SYN_MOOD, 6, 5, 6, 5000, 0, 60, 70,     4, 1, 110, 2, 4, 0),
    FM("FM LEAD",      2, SYN_MOOD, 8, 4, 11, 8000, 18, 35, 55,   2, 1, 140, 6, 6, 45),
    FM("FM REED",      2, SYN_MOOD, 8, 4, 10, 8000, 16, 30, 50,   1, 1, 200, 5, 6, 60),
    FM("FM BELL",      2, SYN_MOOD, 7, 5, 8, 8400, 0, 50, 70,     3, 1, 110, 3, 5, 0),
    FM("FM FLUTE",     2, SYN_MOOD, 9, 5, 12, 8400, 20, 25, 60,   1, 1, 70, 10, 7, 15),
    FM("FM BRASS",     2, SYN_MOOD, 7, 4, 9, 7400, 14, 35, 50,    2, 1, 260, 4, 5, 55),
    /*  name              home flags                attack dec rel sus level  vib cho rev   det semi mix sub drv  cut res env fdec fsus trk */
    VA("MONO LEAD",       2, SYN_MONO,               1, 8, 5, 13, 9800,   20, 30, 55,     6,  0, 64, 50, 40,   84, 40, 26, 9, 8, 8),
    VA("UNISON SAW",      2, SYN_UNI,                1, 8, 5, 13, 9400,   12, 50, 60,     9,  0, 64, 0, 20,    96, 15, 12, 9, 10, 8),
    VA("SQUARE LEAD",     2, SYN_MONO | SYN_PULSE,   1, 8, 5, 13, 12000,   22, 30, 55,     0,  0, 64, 0, 10,    92, 30, 12, 9, 9, 8),
    VA("SYNTH BRASS",     2, SYN_FOLLOW,             3, 8, 5, 13, 12500,   12, 40, 55,     5,  0, 64, 0, 20,    64, 12, 40, 9, 8, 6),
    VA("MOOG BASS",       0, SYN_MONO,               0, 7, 4, 11, 13000,  0, 0, 0,        0,  0, 0, 100, 50,   54, 35, 36, 8, 2, 8),
    VA("ACID",            0, SYN_MONO,               0, 7, 4, 10, 7000,  0, 0, 0,        0,  0, 0, 0, 80,     50, 105, 40, 8, 0, 8),
    VA("REESE",           0, 0,                      1, 9, 4, 13, 16500,  0, 20, 0,       22, 0, 64, 0, 30,    74, 12, 4, 9, 12, 6),
    VA("ANALOGUE PAD",    1, 0,                      5, 10, 8, 14, 6700,  8, 110, 80,     8,  0, 64, 0, 0,     80, 10, 10, 11, 9, 6),
    VA("STRING MACHINE",  1, 0,                      4, 10, 8, 14, 6500,  6, 127, 70,     5, 12, 48, 0, 0,     96, 5, 4, 10, 12, 6),
    /* more basses */
    VA("DEEP SUB",        0, 0,                      1, 9, 4, 14, 15000,  0, 0, 0,        0,  0, 0, 127, 0,    46, 0, 0, 9, 0, 14),
    VA("RUBBER BASS",     0, 0,                      0, 7, 4, 9, 12000,   0, 0, 0,        0,  0, 0, 60, 30,    48, 60, 30, 7, 0, 8),
    FM("PICK BASS",       0, 0, 7, 3, 7, 15000, 0, 0, 0,                   1, 1, 300, 2, 3, 50),
    FM("WOOD BASS",       0, 1, 8, 4, 4, 15500, 0, 0, 0,                   1, 1, 170, 1, 4, 20),
    FM("SLAP BASS",       0, 0, 7, 3, 6, 15000, 0, 0, 0,                   1, 1, 380, 3, 2, 70),
    VA("SAW BASS",        0, 0,                      0, 8, 4, 12, 16000,  0, 15, 0,       8,  0, 64, 40, 20,   58, 15, 22, 8, 4, 8),
    VA("PULSE BASS",      0, SYN_MONO | SYN_PULSE,   0, 8, 4, 12, 15000,  0, 0, 0,        0,  0, 64, 0, 20,    60, 25, 24, 8, 3, 8),
    /* more keys, mallets and bells */
    FM("TINE PIANO",      1, 0, 9, 5, 3, 6800, 0, 60, 55,                  14, 1, 60, 0, 3, 0),
    VA("CLAV",            1, SYN_PULSE,              0, 7, 4, 3, 9000,    0, 30, 40,      0,  0, 64, 0, 40,    70, 40, 40, 6, 2, 8),
    FM("MARIMBA",         1, 0, 7, 5, 0, 8200, 0, 20, 60,                  4, 1, 140, 0, 2, 0),
    FM("VIBES",           1, 0, 10, 7, 0, 7000, 0, 90, 80,                 4, 1, 90, 1, 4, 0),
    FM("KALIMBA",         1, 0, 8, 5, 0, 7800, 0, 30, 65,                  25, 4, 120, 0, 2, 0),
    FM("GLOCKENSPIEL",    1, 0, 9, 6, 0, 6000, 0, 40, 85,                  11, 4, 100, 2, 5, 0),
    FM("TUBULAR BELL",    1, 0, 11, 8, 0, 6400, 0, 40, 100,                7, 2, 200, 4, 7, 0),
    FM("HARP",            1, 0, 9, 6, 0, 7600, 0, 30, 75,                  2, 1, 110, 0, 3, 10),
    FM("MUSIC BOX",       1, 0, 9, 6, 0, 5600, 0, 30, 80,                  9, 2, 70, 0, 3, 0),
    VA("POLY SAW",        1, 0,                      1, 8, 6, 12, 9000,   6, 60, 60,      7,  0, 64, 0, 10,    88, 15, 16, 9, 6, 8),
    VA("SYNTH PLUCK",     1, 0,                      0, 7, 5, 3, 8600,    0, 50, 70,      6,  0, 64, 0, 10,    60, 30, 45, 6, 0, 8),
    VA("PWM PAD",         1, SYN_PULSE,              5, 10, 8, 14, 6500,  6, 100, 80,     0,  0, 64, 0, 0,     86, 10, 6, 11, 10, 6),
    VA("SWEEP PAD",       1, SYN_FOLLOW,             5, 10, 8, 14, 7400,  6, 110, 90,     10, 0, 64, 0, 0,     60, 20, 36, 11, 8, 6),
    VA("WARM STRINGS",    1, 0,                      5, 10, 9, 14, 7000,  10, 127, 85,    7,  0, 64, 0, 0,     86, 0, 0, 10, 16, 6),
    FM("PIPE ORGAN",      1, 2, 9, 6, 16, 4600, 0, 60, 110,                2, 1, 60, 16, 8, 40),
    FM("JAZZ ORGAN",      1, 0, 9, 4, 16, 4400, 5, 110, 50,                3, 1, 70, 16, 8, 0),
    /* more leads */
    VA("SAW LEAD",        2, 0,                      1, 8, 5, 13, 9000,   18, 35, 55,     10, 0, 64, 0, 30,    92, 25, 18, 9, 8, 8),
    VA("SOFT LEAD",       2, SYN_MONO | SYN_PULSE,   2, 8, 5, 14, 11000,  24, 30, 60,     0,  0, 64, 0, 0,     80, 10, 6, 9, 10, 8),
    VA("FIFTH LEAD",      2, SYN_MONO,               1, 8, 5, 13, 9400,   18, 30, 55,     4,  7, 50, 0, 30,    88, 30, 18, 9, 8, 8),
    VA("OCTAVE LEAD",     2, SYN_MONO,               1, 8, 5, 13, 9000,   18, 30, 55,     5, 12, 56, 40, 20,   88, 25, 18, 9, 8, 8),
    VA("CHIP LEAD",       2, SYN_PULSE,              0, 8, 3, 15, 8500,   25, 0, 25,      0,  0, 64, 0, 0,     120, 0, 0, 9, 16, 0),
    VA("BRASS SECTION",   2, SYN_UNI | SYN_FOLLOW,   3, 8, 5, 13, 9000,  10, 40, 60,     8,  0, 64, 0, 20,    62, 12, 42, 9, 8, 6),
    FMF("WHISTLE",        2, SYN_MONO, 2, 9, 5, 15, 6500, 28, 10, 80,      1, 1, 12, 16, 8, 0),
    FMF("THEREMIN",       2, SYN_MONO, 3, 9, 6, 15, 6400, 35, 20, 75,      1, 1, 40, 14, 8, 30),
    FM("HARMONICA",       2, 1, 8, 4, 13, 6800, 18, 25, 50,                1, 1, 170, 12, 6, 60),
    FM("CLARINET",        2, 2, 9, 5, 14, 7000, 14, 20, 60,                2, 1, 90, 12, 7, 10),
    FM("STEEL DRUM",      2, 0, 8, 5, 0, 8600, 0, 40, 70,                  5, 2, 130, 2, 4, 0),
    /* a third batch. Basses */
    FMF("FRETLESS",       0, SYN_MONO, 1, 8, 4, 12, 12000, 12, 15, 0,      1, 1, 120, 6, 6, 35),
    FM("BOOM BASS",       0, 0, 10, 5, 0, 15500, 0, 0, 0,                  1, 1, 25, 0, 3, 0),
    FM("ORGAN BASS",      0, 0, 9, 3, 15, 9500, 0, 0, 0,                  2, 1, 60, 16, 8, 0),
    VA("FUZZ BASS",       0, 0,                      0, 8, 4, 12, 9000,   0, 0, 0,        6,  0, 64, 60, 127,  62, 30, 20, 8, 6, 8),
    VA("UNISON BASS",     0, SYN_UNI,                0, 8, 4, 13, 13500,  0, 10, 0,       10, 0, 64, 0, 20,    60, 15, 18, 8, 6, 8),
    FM("TUBA",            0, 2, 8, 4, 13, 11500, 0, 0, 0,                  1, 1, 140, 10, 6, 45),
    VA("CELLO",           0, SYN_MONO,               3, 9, 6, 14, 15000,  16, 20, 0,      4,  0, 40, 0, 10,    62, 20, 10, 10, 10, 10),
    /* keys, plucked things, pads */
    FM("FM GRAND",        1, 0, 10, 6, 2, 7200, 0, 20, 55,                 1, 1, 230, 1, 4, 25),
    FMF("HONKY TONK",     1, SYN_UNI, 0, 10, 6, 2, 7400, 0, 10, 45,        1, 1, 230, 1, 4, 25),
    VA("HARPSICHORD",     1, SYN_PULSE,              0, 8, 4, 2, 10500,    0, 10, 50,      0,  0, 64, 0, 20,    96, 20, 20, 6, 6, 10),
    FMF("DULCIMER",       1, SYN_UNI, 0, 9, 6, 0, 7600, 0, 30, 70,         3, 1, 150, 1, 3, 15),
    FM("CELESTE",         1, 0, 9, 6, 0, 6000, 0, 40, 80,                  4, 1, 60, 1, 4, 0),
    FM("CRYSTAL",         1, 0, 10, 7, 0, 5200, 0, 50, 100,                7, 1, 90, 3, 6, 0),
    VA("BANJO",           1, 0,                      0, 6, 4, 1, 6500,    0, 0, 35,       0,  0, 0, 0, 50,     80, 50, 30, 5, 2, 12),
    FM("KOTO",            1, 0, 9, 6, 0, 7600, 0, 15, 70,                  3, 1, 240, 0, 3, 30),
    FM("SITAR",           1, 0, 10, 6, 1, 6800, 0, 20, 60,                 1, 1, 200, 6, 7, 60),
    VA("ACCORDION",       1, SYN_PULSE | SYN_UNI,    2, 9, 4, 15, 8500,   8, 40, 40,      0,  0, 64, 0, 0,     92, 10, 0, 9, 16, 8),
    FM("HARMONIUM",       1, 3, 9, 5, 15, 5200, 4, 50, 70,                 1, 1, 110, 14, 8, 70),
    VA("VOX PAD",         1, 0,                      5, 10, 8, 14, 5000,  10, 90, 90,     8,  0, 64, 0, 0,     77, 80, 0, 10, 16, 0),
    VA("CHOIR",           1, SYN_PULSE,              5, 10, 8, 14, 6500, 10, 90, 100,    0,  0, 64, 0, 0,     69, 72, 0, 10, 16, 0),
    FM("GLASS PAD",       1, 5, 10, 8, 13, 5200, 6, 90, 100,               4, 1, 60, 8, 9, 0),
    VA("DARK PAD",        1, 0,                      5, 10, 8, 14, 8600,  6, 80, 85,      10, 0, 64, 30, 0,    66, 15, 0, 10, 16, 6),
    VA("SUPERSAW PAD",    1, SYN_UNI,                4, 10, 8, 14, 7600,  6, 100, 80,     12, 0, 64, 0, 0,     92, 5, 6, 10, 12, 6),
    VA("FIFTH PAD",       1, 0,                      5, 10, 8, 14, 7000,  6, 100, 85,     6,  7, 50, 0, 0,     84, 10, 6, 11, 10, 6),
    VA("OCTAVE STRINGS",  1, 0,                      4, 10, 8, 14, 6800,  10, 127, 85,    6, 12, 64, 0, 0,     92, 5, 0, 10, 16, 6),
    /* leads and solo instruments */
    VA("ACID LEAD",       2, SYN_MONO,               0, 8, 4, 12, 4200,   0, 20, 50,      0,  0, 0, 0, 80,     62, 105, 40, 8, 2, 8),
    VA("FUZZ LEAD",       2, SYN_MONO,               1, 8, 5, 13, 6800,   20, 25, 55,     7,  0, 64, 0, 127,   96, 30, 12, 9, 8, 8),
    FM("OCARINA",         2, 1, 9, 5, 14, 7600, 14, 15, 70,                1, 1, 25, 12, 7, 10),
    FM("PAN FLUTE",       2, 1, 9, 5, 13, 7600, 16, 15, 80,                1, 1, 170, 2, 3, 20),
    FM("TRUMPET",         2, 1, 8, 4, 13, 7000, 16, 15, 55,                1, 1, 230, 11, 6, 50),
    FMF("TROMBONE",       2, SYN_MONO, 2, 8, 4, 13, 7600, 12, 15, 55,      1, 1, 190, 11, 6, 45),
    FM("FRENCH HORN",     2, 3, 9, 5, 14, 7800, 8, 20, 80,                 1, 1, 90, 12, 7, 30),
    FM("SAX",             2, 1, 8, 4, 13, 6800, 20, 15, 55,                1, 1, 150, 12, 6, 62),
    VA("VIOLIN",          2, SYN_MONO,               3, 9, 5, 14, 10500,  26, 15, 70,     3,  0, 30, 0, 10,    92, 25, 6, 10, 12, 10),
    VA("VOX LEAD",        2, SYN_MONO | SYN_PULSE,   2, 9, 5, 14, 7000,  24, 25, 70,     0,  0, 64, 0, 0,     79, 100, 0, 10, 16, 0),
    FM("BELL LEAD",       2, 0, 9, 6, 4, 7600, 0, 40, 75,                  7, 2, 110, 3, 5, 0),
};
#undef FM
#undef FMF
#undef VA
#define SYN_INSTS (sizeof SYN_INST / sizeof SYN_INST[0])
static const uint8_t SYN_FIRST[3] = {0, 5, 10}; /* what a part plays when asked for an instrument this build does not have */

static const uint8_t SYN_POOL[4] = {0, 2, 8, 12};   /* voices 0-1 bass, 2-7 chords, 8-11 melody */
static const int8_t SYN_PAN[3] = {0, 0, 10};        /* where a part sits, -64 left .. 64 right (a chord's notes spread themselves) */
static const uint16_t SYN_ATTACK_FRAMES[6] = {110, 330, 880, 2000, 5300, 13200};   /* 2.5, 7.5, 20, 45, 120, 300 ms */
static const uint16_t SYN_F[128] = {            /* the filter's coefficient for a cutoff at each note, run twice a frame: 2 sin(pi f / 88200), Q15 */
    19, 20, 21, 23, 24, 25, 27, 29, 30, 32, 34, 36,
    38, 40, 43, 45, 48, 51, 54, 57, 61, 64, 68, 72,
    76, 81, 86, 91, 96, 102, 108, 114, 121, 128, 136, 144,
    153, 162, 171, 182, 192, 204, 216, 229, 242, 257, 272, 288,
    305, 324, 343, 363, 385, 408, 432, 458, 485, 514, 544, 576,
    611, 647, 685, 726, 769, 815, 864, 915, 969, 1027, 1088, 1153,
    1221, 1294, 1371, 1452, 1539, 1630, 1727, 1830, 1939, 2054, 2176, 2305,
    2442, 2587, 2741, 2904, 3077, 3260, 3453, 3658, 3876, 4106, 4350, 4608,
    4881, 5171, 5478, 5803, 6147, 6511, 6897, 7305, 7738, 8195, 8680, 9193,
    9735, 10310, 10917, 11560, 12239, 12957, 13717, 14519, 15367, 16262, 17207, 18204,
    19255, 20362, 21529, 22757, 24047, 25403, 26826, 28317};

typedef struct {
    const syn_inst_t *in;
    uint32_t ph, ph2, phs, inc, inc2, hold, age, ticks, vph, pw;
    int32_t amp, peak, sus, att, idx, idx_sus, y1, y2, nx1, nx2, lp, bp, f, q, fenv, fsus;
    int32_t pitch, target, drift, dtarget, det;   /* 1/256 of a semitone */
    int16_t gl, gr;
    uint8_t stage, role, note, ctl, vel, bright;
} syn_voice_t;
enum { SYN_OFF, SYN_ATTACK, SYN_ON, SYN_RELEASE };
static syn_voice_t syn_v[SYN_VOICES];
static uint32_t syn_ages;
static struct { uint32_t ph, inc; int32_t amp; } syn_thump;
static struct { uint32_t ph; int32_t amp, namp, last; } syn_snap;
static struct { int32_t amp, last; } syn_tick;
static struct { uint32_t ph; int32_t amp; } syn_rim;
static struct { int32_t amp; } syn_open;
static uint32_t syn_rng = 0x1234ABCDu, syn_ctl;
static volatile uint32_t syn_room = 2;          /* 0 dry, 1 close, 2 room, 3 hall, 4 far */
static volatile uint16_t syn_mix[4] = {128, 128, 128, 128};   /* each part's level, /128: bass, chords, melody, pulse */

static int32_t syn_sine(uint32_t ph)            /* a full cycle over 2^32 -> -32767..32767 (Dinghy's refined parabola) */
{
    int32_t x = (int32_t)ph >> 16, ax = x < 0 ? -x : x, y, ay;
    y = (x * (32768 - ax)) >> 13;
    ay = y < 0 ? -y : y;
    y += ((((y * ay) >> 15) - y) * 7373) >> 15;
    return y > 32767 ? 32767 : y < -32767 ? -32767 : y;
}

static const uint32_t SYN_INC_C9[12] = {        /* MIDI 120..131; lower octaves shift right */
    815363807u, 863847862u, 915214929u, 969636441u, 1027294024u, 1088380105u,
    1153098554u, 1221665363u, 1294309365u, 1371273005u, 1452813141u, 1539201906u};
static uint32_t syn_inc(uint32_t note) { return SYN_INC_C9[note % 12u] >> (10u - note / 12u); }
static uint32_t syn_pitch_inc(int32_t p)        /* a pitch in 1/256 semitone -> the phase's step (within a cent) */
{
    uint32_t n, a, b;
    if (p < 0) p = 0;
    if (p > 119 * 256) p = 119 * 256;
    n = (uint32_t)p >> 8;
    a = syn_inc(n); b = syn_inc(n + 1u);
    return a + ((b - a) >> 8) * ((uint32_t)p & 255u);
}

/* ---- a voice, every SYN_CTL frames: its envelope, its pitch, its filter ---------------------------------- */
static void syn_control(syn_voice_t *v)
{
    const syn_inst_t *in = v->in;
    int32_t p, d;
    v->ticks++;
    v->idx -= (v->idx - v->idx_sus) >> in->idx_shift;
    if (v->stage == SYN_ATTACK) {
        if (v->amp >= v->peak) { v->amp = v->peak; v->stage = SYN_ON; }
        if (v->hold != SYN_HELD) v->hold = v->hold > SYN_CTL ? v->hold - SYN_CTL : 1u;
    } else if (v->stage == SYN_ON) {
        v->amp -= (v->amp - v->sus) >> in->dec_shift;
        if (v->hold != SYN_HELD) {
            if (v->hold <= SYN_CTL) v->stage = SYN_RELEASE;
            else v->hold -= SYN_CTL;
        }
    } else if (v->stage == SYN_RELEASE) {
        v->amp -= (v->amp >> in->rel_shift) + 8;
        if (v->amp <= 0) { v->amp = 0; v->stage = SYN_OFF; return; }
    }
    d = v->target - v->pitch;                   /* the glide: most of the way in a tenth of a second */
    if (d) v->pitch += d >> 6 ? d >> 6 : d > 0 ? 1 : -1;
    if (!(v->ticks & 15u)) {                    /* a slow wander, a few cents either way */
        if (!(v->ticks & 1023u)) { syn_rng ^= syn_rng << 13; syn_rng ^= syn_rng >> 17; syn_rng ^= syn_rng << 5; v->dtarget = (int32_t)(syn_rng % 17u) - 8; }
        if (v->drift != v->dtarget) v->drift += v->drift < v->dtarget ? 1 : -1;
    }
    p = v->pitch + v->drift + v->det;
    if (in->vib && v->ticks > 413u) {           /* vibrato: after 0.3 s, growing over 0.4 s */
        uint32_t t = v->ticks - 413u;
        int32_t depth = (int32_t)in->vib * 256 / 100;
        if (t < 551u) depth = depth * (int32_t)t / 551;
        v->vph += 17141000u + (v->age & 7u) * 400000u;   /* about 5.5 Hz, each voice a little its own */
        p += (syn_sine(v->vph) * depth) >> 15;
    }
    v->inc = syn_pitch_inc(in->eng == SYN_VA ? p - (int32_t)in->det * 128 / 100 : p);   /* (two oscillators sit either side of the note) */
    if (in->eng == SYN_FM) {
        v->inc2 = v->inc / in->rd * in->rn;
        v->inc2 += v->inc2 >> 9;                /* the modulator a hair sharp: the tone moves slowly */
    } else {
        int32_t e, c;
        v->inc2 = syn_pitch_inc(p + (int32_t)in->det * 128 / 100 + (int32_t)in->semi2 * 256);
        if (in->flags & SYN_PULSE) {            /* the pulse's width: a third, breathing slowly */
            v->pw = 1503238553u + (uint32_t)(syn_sine((v->ticks + v->age * 97u) * 2182000u) * 9000);
        }
        if (in->flags & SYN_FOLLOW) e = v->peak > 255 ? (v->amp << 7) / (v->peak >> 8) : 0;
        else { v->fenv -= (v->fenv - v->fsus) >> in->fdec; e = v->fenv; }
        if (e > 32767) e = 32767;
        c = (int32_t)in->cut * 16 + ((e * (int32_t)in->env) >> 11) + ((int32_t)v->note - 60) * in->track
          + ((int32_t)v->bright - 64) * 6 + ((int32_t)v->vel - 90) * 3;
        if (c < 20 * 16) c = 20 * 16;
        if (c > 126 * 16) c = 126 * 16;
        v->f = SYN_F[c >> 4] + (((int32_t)SYN_F[(c >> 4) + 1] - SYN_F[c >> 4]) * (c & 15) >> 4);
    }
}

/* ---- starting and stopping notes ------------------------------------------------------------------------- */
static syn_voice_t *syn_pick(uint32_t role, uint32_t note, uint32_t taken)   /* the same note, else a free voice, else the oldest */
{
    uint32_t i, pick = SYN_POOL[role], best = 0xFFFFFFFFu;
    for (i = SYN_POOL[role]; i < SYN_POOL[role + 1u]; i++) {
        uint32_t score = syn_v[i].note == note && syn_v[i].stage ? 0u
                       : syn_v[i].stage == SYN_OFF ? 1u : syn_v[i].stage == SYN_RELEASE ? 2u + (syn_v[i].age & 0xFFFFFFu)
                       : 0x80000000u + (syn_v[i].age & 0xFFFFFFu);
        if (taken >> i & 1u) continue;
        if (score < best) { best = score; pick = i; }
    }
    return &syn_v[pick];
}

static void syn_note_on(uint32_t role, uint32_t note, uint32_t vel, uint32_t hold, uint32_t bright, uint32_t attack, uint32_t inst)
{
    static const int8_t SPREAD[4] = {-12, 4, -4, 12}, PAN[4] = {-44, 16, -16, 44};
    const syn_inst_t *in;
    uint32_t k, n = 1, taken = 0, frames, pool, i, mono, whole;
    if (role > 2u || note > 119u || !vel) return;
    in = &SYN_INST[inst < SYN_INSTS ? inst : SYN_FIRST[role]];
    pool = SYN_POOL[role + 1u] - SYN_POOL[role];
    mono = (in->flags & SYN_MONO) && role != 1u;    /* (the chords are never one note at a time) */
    if (in->flags & SYN_UNI) n = role == 1u ? 2u : pool;
    whole = n == pool;                              /* every voice the part has, on this one note */
    for (i = SYN_POOL[role] + 1u; mono && i < SYN_POOL[role + 1u]; i++)   /* whatever else the part was sounding lets go */
        if (syn_v[i].stage && syn_v[i].stage != SYN_RELEASE) syn_v[i].stage = SYN_RELEASE;
    for (k = 0; k < n; k++) {
        syn_voice_t *v = mono ? &syn_v[SYN_POOL[role]] : whole ? &syn_v[SYN_POOL[role] + k] : syn_pick(role, note, taken);
        int32_t pan = SYN_PAN[role], peak, was = v->pitch;
        uint32_t legato = mono && v->in == in && (v->stage == SYN_ATTACK || v->stage == SYN_ON);
        uint32_t sounding = v->stage != SYN_OFF && v->in == in;
        taken |= 1u << (uint32_t)(v - syn_v);
        if (v->stage == SYN_OFF || v->in != in) {   /* (a voice taken over by its own instrument keeps its phase: no click) */
            v->ph = v->ph2 = v->phs = 0; v->amp = 0;
            v->y1 = v->y2 = v->nx1 = v->nx2 = v->lp = v->bp = 0;
            v->drift = v->dtarget = 0;
        }
        v->in = in;
        v->role = (uint8_t)role;
        v->note = (uint8_t)note;
        v->vel = (uint8_t)vel;
        v->bright = (uint8_t)bright;
        v->age = ++syn_ages;
        v->target = (int32_t)note * 256;
        v->pitch = (mono || (whole && n > 1u)) && sounding ? was : v->target;   /* it glides from where it was */
        v->det = n == 2u ? (k ? 18 : -18) : n > 2u ? (int32_t)SPREAD[k & 3u] * 256 / 100 : 0;
        if (role == 1u && n == 1u) pan += note & 1u ? 24 : -24;   /* a chord spreads across the two sides */
        if (n == 2u) pan += k ? 34 : -34;
        else if (n > 2u) pan += PAN[k & 3u];
        v->gl = (int16_t)(128 - pan); v->gr = (int16_t)(128 + pan);
        peak = (int32_t)((uint32_t)in->level * (vel * vel + vel * 127u) >> 15);   /* between a straight line and a square: soft is still heard */
        peak = (peak * (int32_t)syn_mix[role]) >> 7;
        if (n == 2u) peak = peak * 23 >> 5;
        else if (n > 2u) peak = peak * 9 >> 4;
        if (peak < 64) peak = 64;
        v->peak = peak;
        v->sus = peak * in->sustain >> 4;
        v->hold = hold;
        if (legato) continue;                   /* a joined note: the pitch moves, nothing starts again */
        v->idx = (int32_t)((uint32_t)in->idx * (40u + bright + vel / 2u) / 200u);   /* louder and more tense is brighter */
        if (in->eng == SYN_FM) {                /* and never so bright that it folds back as noise */
            uint32_t fm = syn_inc(note) / 97391u * in->rn / in->rd, top = fm ? 1458000u / fm : 0u;
            top = top > 101u ? top - 81u : 20u;
            if ((uint32_t)v->idx > top) v->idx = (int32_t)top;
        }
        v->idx_sus = v->idx * in->idx_keep >> 4;
        frames = SYN_ATTACK_FRAMES[in->attack == SYN_MOOD ? (role == 0u && attack > 1u ? 1u : attack & 3u) : in->attack];
        v->att = peak / (int32_t)frames + 1;
        v->fenv = 32767;
        v->fsus = (int32_t)in->fsus * 2047;
        v->q = 32767 - (int32_t)in->res * 245;
        v->ticks = 0;
        v->vph = 0;
        v->ctl = 1;                             /* (the first frame sets its pitch and filter) */
        v->stage = SYN_ATTACK;
    }
}

static void syn_note_off(uint32_t role, uint32_t note)
{
    uint32_t i;
    if (role > 2u) return;
    for (i = SYN_POOL[role]; i < SYN_POOL[role + 1u]; i++)
        if (syn_v[i].note == note && syn_v[i].hold == SYN_HELD && syn_v[i].stage && syn_v[i].stage != SYN_RELEASE)
            syn_v[i].stage = SYN_RELEASE;
}

static void syn_release_all(void)
{
    uint32_t i;
    for (i = 0; i < SYN_VOICES; i++)
        if (syn_v[i].stage) syn_v[i].stage = SYN_RELEASE;
}

static void syn_drum(uint32_t note, uint32_t vel)
{
    int32_t a = (int32_t)((vel * vel + vel * 127u) * 5u / 8u);   /* 0..20161 */
    a = (a * (int32_t)syn_mix[3]) >> 7;
    if (note == 36u) { syn_thump.ph = 0; syn_thump.inc = 15582000u; syn_thump.amp = a * 3 / 4; }   /* from 160 Hz down */
    else if (note == 38u) { syn_snap.ph = 0; syn_snap.amp = a / 3; syn_snap.namp = a / 2; }
    else if (note == 37u) { syn_rim.ph = 0; syn_rim.amp = a / 2; }             /* a rim click: a short high knock */
    else if (note == 46u) syn_open.amp = a / 3;                                /* the hat, open: it rings */
    else { syn_tick.amp = a * 2 / 5; syn_open.amp >>= 3; }                      /* closed: it stops the open one */
}

/* ---- the chorus and the room ----------------------------------------------------------------------------- */
#if defined(DINGHY_HOST) || defined(SYN_HOST)
#define SYN_BIG
#else
#define SYN_BIG __attribute__((section(".pool")))
#endif
#define SYN_SAT(x) ((x) > 32767 ? 32767 : (x) < -32767 ? -32767 : (x))
#define SYN_CLIP(x, m) ((x) > (m) ? (m) : (x) < -(m) ? -(m) : (x))
static int16_t syn_cho[512];
static uint32_t syn_cho_w, syn_cho_ps, syn_cho_pf, syn_cho_c;
static int32_t syn_cho_d[3] = {176 << 16, 176 << 16, 176 << 16}, syn_cho_step[3];

/* three taps on one short line, each wandering slowly and shimmering a little, a third of a turn apart */
static void syn_chorus(const int32_t *in, int32_t *out, int32_t *rev, uint32_t n)
{
    uint32_t i, k, w = syn_cho_w;
    for (i = 0; i < n; i++) {
        int32_t t[3], x = in[i];
        if (!(syn_cho_c++ & 31u)) {
            syn_cho_ps += 73043u * 32u; syn_cho_pf += 639862u * 32u;
            for (k = 0; k < 3u; k++) {
                int32_t to = (176 << 16) + syn_sine(syn_cho_ps + k * 1431655765u) * 294 + syn_sine(syn_cho_pf + k * 1431655765u) * 30;
                syn_cho_step[k] = (to - syn_cho_d[k]) >> 5;
            }
        }
        syn_cho[w] = (int16_t)SYN_SAT(x);
        for (k = 0; k < 3u; k++) {
            uint32_t d = (uint32_t)syn_cho_d[k], j = d >> 16;
            int32_t fr = (int32_t)(d >> 1 & 0x7FFFu), a = syn_cho[(w - j) & 511u], b = syn_cho[(w - j - 1u) & 511u];
            t[k] = a + (((b - a) * fr) >> 15);
            syn_cho_d[k] += syn_cho_step[k];
        }
        out[2u * i] += (t[0] + t[1]) >> 1;
        out[2u * i + 1u] += (t[2] + t[1]) >> 1;
        rev[i] += (t[0] + t[2]) >> 2;           /* (what is chorused is in the room too) */
        w = (w + 1u) & 511u;
    }
    syn_cho_w = w;
}

/* The room: four all-passes to smear what comes in, then two loops feeding each other, each an all-pass pair, a
 * darkening, a long delay (Griesinger's layout, with Rings' lengths). One tap moves, so it does not ring. */
#define REV_SIZE 21680u
static int16_t syn_rev_mem[REV_SIZE] SYN_BIG;
static const uint16_t REV_LEN[10] = {150, 214, 319, 527, 2182, 2690, 4501, 2525, 2197, 6312};
static uint16_t rev_at[10], rev_w[10], rev_len[10];
static int32_t rev_lp1, rev_lp2;
static uint32_t rev_lfo, rev_room = 99;
/* (products are rounded towards nothing, so a loop can only lose energy: the tail dies to silence, not to a hum) */
#define REV_Q(p) (((p) + (((p) >> 31) & 32767)) >> 15)
#define REV_AP(k, g) do { int16_t *b = syn_rev_mem + rev_at[k]; int32_t d = b[rev_w[k]], y = d - REV_Q((x * (g))), s = x + REV_Q((y * (g))); \
                          b[rev_w[k]] = (int16_t)SYN_SAT(s); if (++rev_w[k] >= rev_len[k]) rev_w[k] = 0; x = y; } while (0)
static void syn_reverb(const int32_t *in, int32_t *out, uint32_t n)
{
    /* a room is a size (how much of each line is used, /64), how much comes round again, and how much comes back */
    static const uint8_t SIZE[5] = {0, 19, 29, 48, 64};
    static const uint16_t KRT[5] = {0, 12600, 16000, 18000, 20800}, RET[5] = {0, 5200, 6800, 7600, 8600};
    uint32_t i, room = syn_room > 4u ? 4u : syn_room;
    int32_t krt = KRT[room], ret = RET[room], off;
    if (room != rev_room) {                     /* another room: its lines, and silence in them */
        uint32_t k, a = 0;
        for (k = 0; k < 10u; k++) {
            rev_at[k] = (uint16_t)a; a += REV_LEN[k]; rev_w[k] = 0;
            rev_len[k] = (uint16_t)((uint32_t)REV_LEN[k] * (room ? SIZE[room] : 64u) / 64u);
        }
        for (k = 0; k < REV_SIZE; k++) syn_rev_mem[k] = 0;
        rev_lp1 = rev_lp2 = 0;
        rev_room = room;
    }
    if (!room) return;
    rev_lfo += 29200u * n;                      /* 0.3 Hz */
    off = 110 + ((syn_sine(rev_lfo) * 100) >> 15);
    for (i = 0; i < n; i++) {
        int32_t x = in[i] >> 2, ap, t1, t2, l, r;
        const int16_t *d1 = syn_rev_mem + rev_at[6], *d2 = syn_rev_mem + rev_at[9];
        uint32_t j = rev_w[9] + (uint32_t)off;
        if (j >= rev_len[9]) j -= rev_len[9];
        t2 = d2[j];                             /* the second loop's tail, from a place that moves */
        t1 = d1[rev_w[6]];
        x = SYN_SAT(x);
        REV_AP(0, 20480); REV_AP(1, 20480); REV_AP(2, 20480); REV_AP(3, 20480);
        ap = SYN_CLIP(x, 45000);
        x = ap + REV_Q(t2 * krt);
        x = SYN_CLIP(x, 45000);
        rev_lp1 += REV_Q((x - rev_lp1) * 22938); x = rev_lp1;
        REV_AP(4, -20480); REV_AP(5, 20480);
        l = x;
        syn_rev_mem[rev_at[6] + rev_w[6]] = (int16_t)SYN_SAT(x);
        if (++rev_w[6] >= rev_len[6]) rev_w[6] = 0;
        x = ap + REV_Q(t1 * krt);
        x = SYN_CLIP(x, 45000);
        rev_lp2 += REV_Q((x - rev_lp2) * 22938); x = rev_lp2;
        REV_AP(7, -20480); REV_AP(8, 20480);
        r = x;
        syn_rev_mem[rev_at[9] + rev_w[9]] = (int16_t)SYN_SAT(x);
        if (++rev_w[9] >= rev_len[9]) rev_w[9] = 0;
        out[2u * i] += (l * ret) >> 12;
        out[2u * i + 1u] += (r * ret) >> 12;
    }
}

static int syn_quiet(void)
{
    uint32_t i;
    for (i = 0; i < SYN_VOICES; i++)
        if (syn_v[i].stage) return 0;
    return syn_thump.amp < 16 && syn_snap.amp < 16 && syn_snap.namp < 16 && syn_tick.amp < 16 && syn_rim.amp < 16 && syn_open.amp < 16;
}

/* ---- the sound ------------------------------------------------------------------------------------------- */
/* a sawtooth without the fold-back (Braids' RenderSaw): the step is smoothed over the two frames it falls between */
#define SYN_SAW(out, ph, inc, nx) do { \
        int32_t th_ = (nx); (nx) = 0; \
        if ((ph) < (inc)) { uint32_t t_ = (ph) / ((inc) >> 16), u_; if (t_ > 65535u) t_ = 65535u; u_ = 65535u - t_; \
                            th_ -= (int32_t)((t_ * t_) >> 18); (nx) += (int32_t)((u_ * u_) >> 18); } \
        (nx) += (int32_t)((ph) >> 17); \
        (out) = (th_ - 16384) << 1; } while (0)
static void syn_render(int32_t *out, uint32_t n)
{
    static int32_t vb[SYN_BLOCK], cho[SYN_BLOCK], rev[SYN_BLOCK];
    uint32_t i, k, any = 0;
    if (n > SYN_BLOCK) n = SYN_BLOCK;
    for (i = 0; i < n; i++) cho[i] = rev[i] = 0;
    for (k = 0; k < SYN_VOICES; k++) {
        syn_voice_t *v = &syn_v[k];
        const syn_inst_t *in = v->in;
        uint32_t done = 0;
        int32_t gc, gv;
        if (v->stage == SYN_OFF) continue;      /* silent: costs nothing */
        while (done < n && v->stage != SYN_OFF) {
            uint32_t seg, ph = v->ph, ph2 = v->ph2;
            int32_t amp, att;
            if (!--v->ctl) { v->ctl = SYN_CTL; syn_control(v); if (v->stage == SYN_OFF) break; }
            seg = n - done < v->ctl ? n - done : v->ctl;
            amp = v->amp; att = v->stage == SYN_ATTACK ? v->att : 0;
            if (in->eng == SYN_FM) {
                int32_t idx = v->idx, y1 = v->y1, y2 = v->y2, kfb = (int32_t)in->fb * 31;
                uint32_t inc = v->inc, incm = v->inc2;
                for (i = 0; i < seg; i++) {
                    int32_t m, s;
                    amp += att; if (amp > v->peak) amp = v->peak;
                    m = syn_sine(ph2 + ((uint32_t)(((y1 + y2) >> 1) * kfb) << 4));
                    ph2 += incm;
                    s = syn_sine(ph + ((uint32_t)(m * idx) << 8));
                    ph += inc;
                    y2 = y1; y1 = m;
                    vb[done + i] = (s * amp) >> 15;
                }
                v->y1 = y1; v->y2 = y2;
            } else {
                int32_t nx1 = v->nx1, nx2 = v->nx2, lp = v->lp, bp = v->bp, f = v->f, q = v->q;
                int32_t m2 = in->mix2, sub = (int32_t)in->sub << 7, drv = in->drive ? 64 + in->drive : 0;
                uint32_t inc = v->inc, inc2 = v->inc2, phs = v->phs, pw = v->pw, pulse = in->flags & SYN_PULSE;
                for (i = 0; i < seg; i++) {
                    int32_t a, b, s, hp;
                    amp += att; if (amp > v->peak) amp = v->peak;
                    ph += inc;
                    SYN_SAW(a, ph, inc, nx1);
                    if (pulse) { uint32_t p2 = ph + pw; SYN_SAW(b, p2, inc, nx2); s = (a - b) >> 1; }
                    else { ph2 += inc2; SYN_SAW(b, ph2, inc2, nx2); s = (a * (128 - m2) + b * m2) >> 7; }
                    phs += inc >> 1;
                    s += (int32_t)phs < 0 ? -sub : sub;
                    if (drv) {                  /* pushed into a soft ceiling before the filter: it thickens */
                        s = (s * drv) >> 7;
                        s = SYN_CLIP(s, 32767);
                        s = (3 * s - ((((s * s) >> 15) * s) >> 15)) >> 1;
                    }
                    s >>= 1;
                    lp += (f * bp) >> 15; hp = s - lp - ((q * bp) >> 15); hp = SYN_CLIP(hp, 65000); bp += (f * hp) >> 15;
                    lp += (f * bp) >> 15; hp = s - lp - ((q * bp) >> 15); hp = SYN_CLIP(hp, 65000); bp += (f * hp) >> 15;
                    lp = SYN_CLIP(lp, 50000); bp = SYN_CLIP(bp, 50000);
                    vb[done + i] = (lp * amp) >> 14;
                }
                v->nx1 = nx1; v->nx2 = nx2; v->lp = lp; v->bp = bp; v->phs = phs;
            }
            v->ph = ph; v->ph2 = ph2; v->amp = amp;
            v->ctl = (uint8_t)(v->ctl - seg + 1u);   /* (it is counted down again at the top) */
            done += seg;
        }
        gc = in->cho; gv = v->role ? in->rev : 0;    /* (the bass stays out of the room) */
        for (i = 0; i < done; i++) {
            int32_t s = vb[i];
            out[2u * i] += (s * v->gl) >> 7;
            out[2u * i + 1u] += (s * v->gr) >> 7;
            cho[i] += (s * gc) >> 7;
            rev[i] += (s * gv) >> 7;
        }
        any = 1;
    }
    if (syn_thump.amp >= 16 || syn_snap.amp >= 16 || syn_snap.namp >= 16 || syn_tick.amp >= 16 || syn_rim.amp >= 16 || syn_open.amp >= 16) {
        uint32_t rng = syn_rng, c = syn_ctl;
        for (i = 0; i < n; i++) {
            int32_t s = 0, hi = 0, noise;
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            noise = (int32_t)(rng & 0xFFFFu) - 32768;
            if (!(++c & 15u)) {                 /* every 16 frames: the drums die away, the thump falls */
                syn_thump.amp -= (syn_thump.amp >> 7) + 1;
                if (syn_thump.inc > 5000000u) syn_thump.inc -= syn_thump.inc >> 6;
                syn_snap.amp -= (syn_snap.amp >> 5) + 1;
                syn_snap.namp -= (syn_snap.namp >> 6) + 1;
                syn_tick.amp -= (syn_tick.amp >> 4) + 1;
                syn_rim.amp -= (syn_rim.amp >> 5) + 1;
                syn_open.amp -= (syn_open.amp >> 8) + 1;
                if (syn_rim.amp < 0) syn_rim.amp = 0;
                if (syn_open.amp < 0) syn_open.amp = 0;
                if (syn_thump.amp < 0) syn_thump.amp = 0;
                if (syn_snap.amp < 0) syn_snap.amp = 0;
                if (syn_snap.namp < 0) syn_snap.namp = 0;
                if (syn_tick.amp < 0) syn_tick.amp = 0;
            }
            if (syn_thump.amp) { syn_thump.ph += syn_thump.inc; s += (syn_sine(syn_thump.ph) * syn_thump.amp) >> 15; }
            if (syn_snap.amp | syn_snap.namp) {
                syn_snap.ph += 18500000u;       /* 190 Hz */
                hi += (syn_sine(syn_snap.ph) * syn_snap.amp) >> 15;
                hi += ((noise - syn_snap.last) * (syn_snap.namp >> 2)) >> 15;   /* the noise, its lows taken out */
                syn_snap.last = noise;
            }
            if (syn_tick.amp | syn_open.amp) {
                hi += ((noise - syn_tick.last) * ((syn_tick.amp + syn_open.amp) >> 2)) >> 15;
                syn_tick.last = noise;
            }
            if (syn_rim.amp) {
                syn_rim.ph += 165564000u;       /* 1700 Hz */
                hi += (syn_sine(syn_rim.ph) * syn_rim.amp) >> 15;
            }
            out[2u * i] += s + hi;
            out[2u * i + 1u] += s + hi;
            rev[i] += hi >> 2;                  /* (the snap and the ticks have a little room; the thump has none) */
        }
        syn_rng = rng; syn_ctl = c;
    }
    (void)any;
#ifdef MEL_BURN                                 /* a test build: this much more work a frame, to find where the time runs out */
    { volatile uint32_t burn = 0; for (i = 0; i < n * (uint32_t)(MEL_BURN); i++) burn += i; }
#endif
    syn_chorus(cho, out, rev, n);
    syn_reverb(rev, out, n);
}
#endif
