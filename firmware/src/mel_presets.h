/* SPDX-License-Identifier: GPL-3.0-only */
/* Melisma's shipped pieces: starting points, each only the dials in a position (a preset is 22 bytes; there is
 * room for thousands). The first seven are the Melisma project's own presets, with its descriptions' intent;
 * the rest are in their spirit. Roles: E the engine plays, Y the seat is yours, - off. */
#ifndef MEL_PRESETS_H
#define MEL_PRESETS_H
#include "mel_engine.h"

typedef struct { const char *name; mel_piece_t piece; } mel_preset_t;
#define E MEL_ENGINE
#define Y MEL_YOU
#define X MEL_OFF
#define P(name, mood_, energy_, feel_, shape_, colour_, key_, len_, var_, boost_, r0, r1, r2, r3, s0, s1, s2) \
    {name, {.mood = mood_, .energy = energy_, .feel = feel_, .shape = shape_, .colour = colour_, .key = key_, .length = len_, \
            .variation = var_, .boost = boost_, .role = {r0, r1, r2, r3}, .sound = {s0, s1, s2, 0}}}
/* moods: 0 bright 1 warm 2 wistful 3 sad 4 dark 5 dreamy 6 tense; energies: 0 still .. 4 frantic; feels: 0 straight
 * 1 swung 2 waltz 3 lilting 4 odd; shapes: 0 loop 1 rise 2 fall 3 arch 4 q+a; colour 8 = aching; keys: 1 = C .. 12 = B;
 * lengths: 0..5 = 4, 8, 12, 16, 24, 32 bars */
static const mel_preset_t MEL_PRESET[] = {
    P("TEST BED",         2, 2, 0, 0, 0, 0,  1, 0,   0, E, E, Y, E, 0, 0, 0),   /* the default: wistful, moving, and your tune */
    P("HEBRIDEAN",        2, 3, 3, 3, 0, 3,  3, 3,   0, E, E, E, X, 0, 2, 4),   /* a lilting six-eight in the folk minor */
    P("LONG BUILD",       2, 3, 0, 1, 0, 10, 3, 1,   0, E, E, E, E, 1, 0, 0),   /* straight four, climbing for sixteen bars */
    P("SIDESTEP",         3, 3, 0, 0, 0, 6,  1, 4,   9, E, E, E, E, 1, 2, 1),   /* hard and syncopated, the dial well up */
    P("WIDE SHOT",        5, 0, 0, 3, 8, 4,  5, 2,   0, E, E, E, X, 2, 3, 3),   /* slow, floating, filmic, on the aching notes */
    P("HOLDING PATTERN",  2, 1, 0, 0, 0, 1,  3, 6,   0, E, E, Y, E, 0, 1, 0),   /* low notes, harmony, a tick: the tune is yours */
    P("LIMPING SEVEN",    4, 2, 4, 0, 0, 5,  3, 8,   3, E, E, E, E, 1, 2, 1),   /* seven to a bar, two-two-three */
    P("ANGELS ONE FIVE",  1, 2, 0, 3, 0, 4,  5, 4,   0, E, E, E, X, 0, 0, 3),   /* broad and unhurried British light music */
    P("FIRST LIGHT",      0, 1, 0, 1, 0, 8,  3, 21,  0, E, E, E, X, 2, 3, 3),   /* a calm morning that opens up */
    P("TAILWIND",         0, 3, 1, 0, 0, 6,  1, 12,  2, E, E, E, E, 0, 0, 1),   /* bright, swung and going somewhere */
    P("NIGHT MAIL",       2, 2, 1, 0, 0, 3,  1, 77,  0, E, E, Y, E, 0, 0, 1),   /* a swung loop to play over */
    P("SLOW ROLL",        1, 1, 2, 3, 0, 8,  3, 15,  0, E, E, E, X, 2, 1, 3),   /* a warm waltz */
    P("GROUND FOG",       3, 0, 0, 0, 0, 10, 1, 30,  0, E, E, Y, X, 2, 3, 3),   /* sad and still: almost nothing moves */
    P("CONTRAIL",         5, 1, 3, 2, 0, 1,  3, 44,  0, E, E, E, X, 2, 3, 2),   /* dreamy, lilting, settling as it goes */
    P("CROSSWIND",        6, 2, 4, 0, 0, 12, 1, 101, 4, E, E, E, E, 3, 2, 4),   /* tense, in seven */
    P("DEAD RECKONING",   4, 3, 0, 0, 0, 5,  1, 58,  5, E, E, Y, E, 1, 2, 1),   /* dark and driving: find your own way over it */
    P("BRIEFING ROOM",    2, 1, 1, 4, 0, 11, 1, 9,   0, E, E, E, E, 0, 0, 1),   /* a question, then its answer */
    P("HANGAR DANCE",     1, 3, 1, 0, 0, 11, 1, 200, 3, E, E, E, E, 0, 0, 4),   /* warm, swung, Saturday night */
    P("OVER THE CHANNEL", 0, 2, 3, 3, 0, 3,  3, 63,  0, E, E, E, E, 0, 2, 3),   /* bright six-eight, out and back */
    P("BLACKOUT BLUES",   3, 1, 1, 0, 0, 5,  2, 310, 0, E, E, Y, E, 0, 0, 1),   /* twelve bars, sad and swung: your solo */
    P("FINAL APPROACH",   6, 3, 0, 2, 0, 2,  3, 140, 0, E, E, E, E, 1, 4, 4),   /* tense and driving, easing all the way down */
    P("SCRAMBLE",         6, 4, 0, 1, 0, 7,  1, 5,   8, E, E, E, E, 3, 2, 4),   /* everything, at once, getting worse */
    P("SPIT AND POLISH",  0, 4, 0, 0, 0, 1,  0, 88,  0, E, E, E, E, 3, 2, 2),   /* four bright frantic bars */
    P("LIGHTS OUT",       3, 0, 2, 2, 8, 9,  3, 250, 0, E, E, E, X, 2, 3, 3),   /* a slow sad waltz that comes to rest */
};
#undef E
#undef Y
#undef X
#undef P
#define MEL_PRESETS (sizeof MEL_PRESET / sizeof MEL_PRESET[0])
#endif
