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
            .variation = var_, .boost = boost_, .role = {r0, r1, r2, r3}, .sound = {s0, 5 + s1, 10 + s2, 0}, .room = 2, .rev = MEL_REV}}
#define PS(name, style_, mood_, energy_, feel_, shape_, colour_, key_, len_, var_, boost_, r0, r1, r2, r3, s0, s1, s2, t0, t1, room_) \
    {name, {.style = style_, .mood = mood_, .energy = energy_, .feel = feel_, .shape = shape_, .colour = colour_, .key = key_, \
            .length = len_, .variation = var_, .boost = boost_, .role = {r0, r1, r2, r3}, .sound = {s0, s1, s2, 0}, .take = {t0, t1, 0, 0}, \
            .room = room_, .rev = MEL_REV}}
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
    /* the styles (PS: the style first; then the instruments by their numbers in mel_synth.h, how the bass and the
     * chords are played, and the room) */
    PS("NIGHT FIGHTER",    MEL_CLUB,      4, 3, 0, 1, 0, 10, 3, 21,  0, E, E, E, E, 19, 22, 15, 0, 0, 3),   /* house, dark, building: Moog bass, a pad, a mono lead */
    PS("LAST TRAIN HOME",  MEL_CLUB,      3, 2, 1, 0, 0, 8,  1, 44,  0, E, E, Y, E, 1, 5, 17, 0, 7, 2),     /* two-step, the chords off the beat, a square lead for you */
    PS("NAAFI BREAK",      MEL_STREET,    2, 1, 0, 0, 0, 6,  1, 12,  0, E, E, Y, E, 1, 5, 12, 0, 0, 2),     /* boom-bap, a held chord, your tune */
    PS("WIZARD PRANG",     MEL_FUNK,      1, 3, 0, 0, 0, 3,  1, 77,  2, E, E, E, E, 19, 7, 18, 3, 7, 1),    /* funk: octaves in the bass, stabs off the beat, brass */
    PS("VICTORY ROLL",     MEL_FUNK,      0, 3, 1, 3, 0, 6,  2, 30,  0, E, E, E, E, 0, 6, 18, 0, 0, 2),     /* second line */
    PS("ISLAND HOP",       MEL_ISLAND,    1, 2, 0, 0, 0, 1,  1, 5,   0, E, E, E, E, 0, 6, 13, 2, 0, 2),     /* one drop */
    PS("FAIR WEATHER",     MEL_BREEZE,    5, 1, 0, 0, 0, 6,  1, 61,  0, E, E, Y, E, 0, 5, 13, 0, 2, 2),     /* bossa, the chords climbing, your tune */
    PS("FIESTA FLIGHT",    MEL_FIESTA,    0, 3, 0, 0, 0, 1,  1, 90,  0, E, E, E, E, 3, 5, 18, 0, 0, 2),     /* salsa */
    PS("MESS PIANO",       MEL_LOUNGE,    2, 2, 1, 4, 0, 11, 2, 140, 0, E, E, E, E, 0, 5, 11, 0, 0, 2),     /* jazz: a walking bass, shell chords */
    PS("MILK RUN",         MEL_ROADHOUSE, 0, 2, 0, 0, 0, 8,  1, 8,   0, E, E, E, E, 0, 7, 12, 0, 0, 2),     /* the train beat */
    PS("BEAT UP THE FIELD", MEL_ROADHOUSE, 1, 3, 1, 0, 0, 5,  2, 19,  0, E, E, E, E, 3, 6, 14, 0, 0, 2),    /* a shuffle */
    PS("AUTOBAHN",         MEL_MOTOR,     0, 3, 0, 1, 0, 3,  5, 2,   0, E, E, E, E, 19, 23, 16, 0, 0, 3),   /* motorik: thirty-two bars, never varying */
    PS("BANDITS AT SIX",   MEL_RUSH,      3, 4, 0, 3, 0, 4,  3, 303, 0, E, E, E, E, 21, 22, 15, 0, 0, 3),   /* drum and bass */
    PS("ABOVE THE WEATHER", MEL_DRIFT,    5, 0, 0, 3, 0, 8,  3, 200, 0, E, E, E, E, 4, 22, 13, 0, 1, 4),    /* ambient: no pulse at all */
    /* and pieces for the instruments */
    PS("CHAPEL AT DAWN",   MEL_FREE,      5, 0, 2, 3, 0, 6,  3, 71,  0, E, E, E, X, 4, 45, 13, 0, 0, 4),    /* a pipe organ, slowly, in three */
    PS("CARIBBEAN LEAVE",  MEL_ISLAND,    0, 2, 0, 0, 0, 1,  1, 16,  0, E, E, E, E, 27, 46, 57, 2, 0, 2),   /* a steel drum over a one drop */
    PS("MUSIC HALL",       MEL_ROADHOUSE, 1, 3, 1, 0, 0, 8,  2, 55,  0, E, E, E, E, 27, 5, 55, 0, 0, 1),    /* a shuffle with a harmonica */
    PS("SMOKE ROOM",       MEL_LOUNGE,    2, 1, 1, 4, 0, 4,  2, 9,   0, E, E, E, E, 27, 34, 56, 0, 0, 2),   /* wood bass, vibes, a clarinet */
    PS("ARCADE",           MEL_CLUB,      0, 3, 0, 0, 0, 3,  1, 128, 0, E, E, E, E, 30, 41, 51, 0, 2, 1),   /* pulse waves and plucks */
    PS("TOY SHOP",         MEL_FREE,      0, 2, 2, 0, 0, 1,  1, 33,  0, E, E, E, X, 26, 39, 36, 0, 2, 2),   /* a music box waltz */
    PS("TRADE WINDS",      MEL_FREE,      1, 2, 3, 0, 0, 8,  1, 404, 0, E, E, E, E, 27, 35, 33, 0, 4, 2),   /* kalimba and marimba in six */
    PS("GHOST SIGNAL",     MEL_DRIFT,     3, 0, 0, 2, 0, 10, 3, 617, 0, E, E, E, E, 24, 44, 54, 0, 0, 4),   /* strings, a theremin, a long way off */
    PS("TOP COVER",        MEL_FUNK,      1, 3, 0, 0, 0, 5,  1, 88,  3, E, E, E, E, 28, 32, 52, 3, 7, 1),   /* slap bass, clav, a brass section */
    PS("NIGHT WATCH",      MEL_STREET,    4, 1, 0, 0, 0, 11, 1, 23,  0, E, E, Y, E, 24, 31, 48, 0, 0, 3),   /* a deep sub, a tine piano, a soft lead for you */
    PS("SUNDAY PARADE",    MEL_FREE,      0, 3, 0, 1, 0, 8,  3, 150, 0, E, E, E, E, 29, 40, 49, 0, 5, 3),   /* saws, climbing for sixteen bars */
    PS("TOWER BELLS",      MEL_FREE,      5, 1, 0, 3, 0, 3,  3, 12,  0, E, E, E, X, 4, 37, 38, 0, 6, 4),    /* tubular bells and a harp */
    PS("BRASS BAND",       MEL_FUNK,      0, 3, 1, 0, 0, 6,  1, 64,  0, E, E, E, E, 63, 89, 87, 0, 0, 2),   /* a tuba, horns and a trumpet on a second line */
    PS("SALOON",           MEL_ROADHOUSE, 1, 3, 0, 0, 0, 8,  1, 91,  0, E, E, E, E, 27, 71, 66, 0, 5, 1),   /* a banjo and a honky-tonk piano on the train beat */
    PS("COURT DANCE",      MEL_FREE,      0, 2, 2, 0, 0, 3,  1, 210, 0, E, E, E, X, 64, 67, 85, 0, 5, 3),   /* a harpsichord in three */
    PS("KYOTO RAIN",       MEL_FREE,      4, 1, 0, 3, 0, 5,  3, 77,  0, E, E, E, X, 4, 72, 86, 0, 6, 3),    /* a koto and a pan flute */
    PS("EVENSONG",         MEL_DRIFT,     3, 0, 0, 2, 0, 3,  3, 320, 0, E, E, E, E, 60, 77, 91, 0, 0, 4),   /* a choir and a violin, far off */
    PS("RAGA",             MEL_FREE,      1, 1, 3, 1, 0, 3,  3, 501, 0, E, E, E, E, 24, 75, 73, 6, 0, 2),   /* a drone, a harmonium, a sitar */
    PS("PARIS LEAVE",      MEL_FREE,      2, 2, 2, 0, 0, 8,  1, 17,  0, E, E, E, E, 27, 74, 91, 0, 7, 2),   /* an accordion waltz */
    PS("LATE SET",         MEL_LOUNGE,    3, 1, 1, 4, 0, 11, 2, 260, 0, E, E, E, E, 58, 65, 90, 0, 0, 2),   /* fretless bass, a piano, a saxophone */
    PS("WAREHOUSE",        MEL_CLUB,      4, 4, 0, 0, 0, 10, 1, 700, 0, E, E, E, E, 62, 80, 83, 0, 0, 3),   /* supersaws and acid */
    PS("GARAGE BAND",      MEL_FREE,      1, 3, 0, 0, 0, 5,  1, 39,  2, E, E, E, E, 61, 40, 84, 0, 0, 1),   /* fuzz, and plenty of it */
    PS("SNOW SCENE",       MEL_FREE,      5, 1, 2, 3, 0, 8,  3, 444, 0, E, E, E, X, 4, 69, 93, 0, 2, 3),    /* a celeste and bells */
    PS("ANGEL VOICES",     MEL_FREE,      5, 0, 0, 3, 0, 1,  3, 15,  0, E, E, E, X, 24, 76, 92, 0, 0, 4),   /* voices, more or less */
    PS("HILL COUNTRY",     MEL_ROADHOUSE, 0, 2, 0, 0, 0, 8,  1, 123, 0, E, E, E, E, 27, 68, 91, 0, 5, 2),   /* a dulcimer and a fiddle */
    PS("BIG FINISH",       MEL_FREE,      0, 3, 0, 1, 0, 1,  3, 999, 0, E, E, E, E, 63, 82, 87, 0, 0, 4),   /* strings and a trumpet, climbing */
};
#undef E
#undef Y
#undef X
#undef P
#undef PS
#define MEL_PRESETS (sizeof MEL_PRESET / sizeof MEL_PRESET[0])
#endif
