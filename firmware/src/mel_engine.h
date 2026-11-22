/* SPDX-License-Identifier: GPL-3.0-only */
/* Melisma's engine: word dials in, notes out. A new engine written for the FM-1 from the ideas of Melisma
 * (a Merlin Engines project): the vocabulary and its mappings are that project's docs/accessible.md.
 *
 * Pure: no platform, no globals but const tables, no float, nothing 64-bit. The same file runs on the FM-1, in
 * tests/host.c and in tests/mel_test.c. A piece is a function of its settings and nothing else: there is no random
 * number generator, only a hash of (variation, role, bar, index), so the same settings always give the same music,
 * and one bar can be made without making the ones before it.
 *
 *   mel_piece_t   the settings: what the dials say (15 characters as a code, mel_code_*)
 *   mel_plan()    what follows from them for the whole piece (key, metre, tempo, the chords to choose from)
 *   mel_bar()     one role's notes in one bar
 */
#ifndef MEL_ENGINE_H
#define MEL_ENGINE_H
#include <stdint.h>

#define MEL_MARK 1u                             /* the engine version a code is made on: its tables are frozen */

enum { MEL_BASS, MEL_CHORDS, MEL_MELODY, MEL_PULSE, MEL_ROLES };
enum { MEL_ENGINE, MEL_YOU, MEL_HELD, MEL_OFF };            /* a role's state */
enum { MEL_LOOP, MEL_RISE, MEL_FALL, MEL_ARCH, MEL_QA };    /* shape */
enum { MEL_STRAIGHT, MEL_SWUNG, MEL_WALTZ, MEL_LILTING, MEL_ODD };   /* feel */
enum { MEL_DEGREES, MEL_SNAPPED, MEL_CHROMATIC };           /* key layout */

#define MEL_MOODS 7u
#define MEL_ENERGIES 5u
#define MEL_FEELS 5u
#define MEL_SHAPES 5u
#define MEL_COLOURS 9u                          /* 0 = as the mood */
#define MEL_KEYS 13u                            /* 0 = the engine chooses */
#define MEL_LENGTHS 6u
#define MEL_VARIATIONS 1024u
#define MEL_SOUNDS 64u                          /* an instrument's number (mel_synth.h has the list) */
#define MEL_KITS 8u
#define MEL_ROOMS 5u                            /* dry, close, room, hall, far */
#define MEL_REV 1u                              /* how a stored piece numbers its instruments (0: by part, before there was a list) */
#define MEL_LAYOUTS 3u
#define MEL_TRIMS 16u                           /* 0 = the engine chooses */
#define MEL_STYLES 12u                          /* the first is FREE: the evenly spread engine (the code has room for 16) */
#define MEL_TAKES 8u                            /* a part's own idea: another take of the same piece */
#define MEL_MAX_BOOST 11u

typedef struct {                                /* (no padding: it is compared and stored as bytes) */
    uint16_t variation;
    uint8_t mood, energy, feel, shape, colour, key, length, layout, follow, trim, boost, style;
    uint8_t role[MEL_ROLES], sound[MEL_ROLES], take[MEL_ROLES], room, rev;
} mel_piece_t;
_Static_assert(sizeof(mel_piece_t) == 28, "mel_piece_t must have no padding");

static const char *const MEL_MOOD_NAME[MEL_MOODS] = {"BRIGHT", "WARM", "WISTFUL", "SAD", "DARK", "DREAMY", "TENSE"};
static const char *const MEL_ENERGY_NAME[MEL_ENERGIES] = {"STILL", "CALM", "MOVING", "DRIVING", "FRANTIC"};
static const char *const MEL_FEEL_NAME[MEL_FEELS] = {"STRAIGHT", "SWUNG", "WALTZ", "LILTING", "ODD"};
static const char *const MEL_SHAPE_NAME[MEL_SHAPES] = {"LOOP", "RISE", "FALL", "ARCH", "Q + A"};
static const char *const MEL_COLOUR_NAME[MEL_COLOURS] = {"AUTO", "BRIGHT", "WARM", "WISTFUL", "SAD", "DARK", "DREAMY",
                                                         "TENSE", "ACHING"};
static const char *const MEL_KEY_NAME[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
static const char *const MEL_ROLE_NAME[MEL_ROLES] = {"BASS", "CHORDS", "MELODY", "PULSE"};
static const char *const MEL_LAYOUT_NAME[MEL_LAYOUTS] = {"DEGREES", "SNAPPED", "CHROMATIC"};
enum { MEL_FREE, MEL_CLUB, MEL_STREET, MEL_FUNK, MEL_ISLAND, MEL_BREEZE, MEL_FIESTA, MEL_LOUNGE, MEL_ROADHOUSE, MEL_MOTOR,
       MEL_RUSH, MEL_DRIFT };
static const char *const MEL_STYLE_NAME[MEL_STYLES] = {"FREE", "CLUB", "STREET", "FUNK", "ISLAND", "BREEZE", "FIESTA", "LOUNGE",
                                                       "ROADHOUSE", "MOTOR", "RUSH", "DRIFT"};
/* a take of the chords or the bass is a manner of playing it; of the melody or the pulse, another idea */
enum { MEL_WRITTEN, MEL_ROOTS, MEL_FIFTHS, MEL_OCTAVES, MEL_WALK, MEL_TRIAD, MEL_PEDAL, MEL_PUSH };
static const char *const MEL_BASS_NAME[MEL_TAKES] = {"WRITTEN", "ROOTS", "FIFTHS", "OCTAVES", "WALK", "TRIAD", "PEDAL", "PUSH"};
enum { MEL_BLOCK, MEL_OPEN, MEL_CLIMB, MEL_DESCEND, MEL_WAVE, MEL_BROKEN, MEL_ROLLED, MEL_OFFBEAT };
static const char *const MEL_MANNER_NAME[MEL_TAKES] = {"BLOCK", "OPEN", "CLIMB", "FALL", "WAVE", "BROKEN", "ROLLED", "OFFBEAT"};
static const uint8_t MEL_LENGTH_BARS[MEL_LENGTHS] = {4, 8, 12, 16, 24, 32};

static void mel_default(mel_piece_t *p)         /* doing nothing already sounds good: you on the melody, the band behind */
{
    static const mel_piece_t D = {.mood = 2, .energy = 2, .length = 1, .role = {MEL_ENGINE, MEL_ENGINE, MEL_YOU, MEL_ENGINE},
                                  .sound = {0, 5, 10, 0}, .room = 2, .rev = MEL_REV};
    *p = D;
}

static int mel_valid(const mel_piece_t *p)
{
    uint32_t r;
    if (p->mood >= MEL_MOODS || p->energy >= MEL_ENERGIES || p->feel >= MEL_FEELS || p->shape >= MEL_SHAPES ||
        p->colour >= MEL_COLOURS || p->key >= MEL_KEYS || p->length >= MEL_LENGTHS || p->layout >= MEL_LAYOUTS ||
        p->follow > 1u || p->trim >= MEL_TRIMS || p->boost > MEL_MAX_BOOST || p->variation >= MEL_VARIATIONS ||
        p->style >= MEL_STYLES || p->room >= MEL_ROOMS || p->rev != MEL_REV)
        return 0;
    for (r = 0; r < MEL_ROLES; r++)
        if (p->role[r] > MEL_OFF || p->sound[r] >= (r == MEL_PULSE ? MEL_KITS : MEL_SOUNDS) || p->take[r] >= MEL_TAKES) return 0;
    return 1;
}

/* ---- the code: a piece as 21 characters ------------------------------------------------------------------
 * 96 bits of settings in 20 characters of 5 bits, and a check character. The alphabet has no 0, 1, I or O, so
 * nothing in it can be read as something else. The check is Luhn's, mod 32: it catches any one wrong character,
 * and two neighbours swapped (all but one pair of them). */
#define MEL_CODE_CHARS 21u
#define MEL_CODE_DATA 20u
#define MEL_CODE_GROUP 7u
#define MEL_CODE_TEXT 24u                       /* "XXXXXXX-XXXXXXX-XXXXXXX" and the NUL */
#define MEL_FIELDS 27u
enum { MEL_CODE_OK, MEL_CODE_BAD_CHAR, MEL_CODE_BAD_CHECK, MEL_CODE_OTHER_MARK, MEL_CODE_BAD_VALUE };
static const char MEL_ALPHABET[33] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";

static const uint8_t MEL_FIELD_BITS[MEL_FIELDS] = {4, 3, 3, 3, 3, 4, 4, 3, 10, 2, 2, 2, 2, 6, 6, 6, 3, 2, 1, 4, 4,
                                                   4, 3, 3, 3, 3, 3};   /* the mark first; 96 bits */
static void mel_fields(mel_piece_t *p, uint16_t *f, int unpack)   /* the settings as a row of numbers, in the code's order */
{
    uint8_t *b[MEL_FIELDS - 1u] = {&p->mood, &p->energy, &p->feel, &p->shape, &p->colour, &p->key, &p->length, 0,
                      &p->role[0], &p->role[1], &p->role[2], &p->role[3], &p->sound[0], &p->sound[1], &p->sound[2],
                      &p->sound[3], &p->layout, &p->follow, &p->trim, &p->boost, &p->style, &p->take[0], &p->take[1],
                      &p->take[2], &p->take[3], &p->room};
    uint32_t i;
    for (i = 0; i < MEL_FIELDS - 1u; i++) {
        if (i == 7u) {
            if (unpack) p->variation = f[8]; else f[8] = p->variation;
        } else if (unpack) *b[i] = (uint8_t)f[i + 1u];
        else f[i + 1u] = *b[i];
    }
}

static uint32_t mel_luhn(const uint8_t *v, uint32_t n, uint32_t factor)   /* from the right, every other one doubled */
{
    uint32_t sum = 0;
    while (n--) {
        uint32_t a = v[n] * factor;
        sum += a / 32u + a % 32u;
        factor = 3u - factor;
    }
    return sum % 32u;
}

static void mel_code_make(const mel_piece_t *p, char *text)   /* text: MEL_CODE_TEXT bytes, "XXXXXX-XXXXXX-XXXXXX" */
{
    mel_piece_t q = *p;
    uint16_t f[MEL_FIELDS];
    uint8_t v[MEL_CODE_CHARS] = {0};
    uint32_t i, bit = 0, k;
    f[0] = MEL_MARK;
    mel_fields(&q, f, 0);
    for (i = 0; i < MEL_FIELDS; i++)
        for (k = 0; k < MEL_FIELD_BITS[i]; k++, bit++)
            if (f[i] >> k & 1u) v[bit / 5u] |= (uint8_t)(1u << (bit % 5u));
    v[MEL_CODE_DATA] = (uint8_t)((32u - mel_luhn(v, MEL_CODE_DATA, 2)) % 32u);
    for (i = 0, k = 0; i < MEL_CODE_CHARS; i++) {
        if (i == MEL_CODE_GROUP || i == 2u * MEL_CODE_GROUP) text[k++] = '-';
        text[k++] = MEL_ALPHABET[v[i]];
    }
    text[k] = 0;
}

static int mel_code_read(const char *text, mel_piece_t *out)   /* dashes, spaces and small letters are fine */
{
    mel_piece_t q = {0};
    uint16_t f[MEL_FIELDS] = {0};
    uint8_t v[MEL_CODE_CHARS];
    uint32_t i, n = 0, bit = 0, k;
    for (; *text; text++) {
        char c = *text;
        if (c == '-' || c == ' ') continue;
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        for (i = 0; i < 32u && MEL_ALPHABET[i] != c; i++) {}
        if (i == 32u || n == MEL_CODE_CHARS) return MEL_CODE_BAD_CHAR;
        v[n++] = (uint8_t)i;
    }
    if (n != MEL_CODE_CHARS) return MEL_CODE_BAD_CHAR;
    if (mel_luhn(v, MEL_CODE_CHARS, 1)) return MEL_CODE_BAD_CHECK;
    for (i = 0; i < MEL_FIELDS; i++)
        for (k = 0; k < MEL_FIELD_BITS[i]; k++, bit++)
            if (v[bit / 5u] >> (bit % 5u) & 1u) f[i] |= (uint16_t)(1u << k);
    if (f[0] != MEL_MARK) return MEL_CODE_OTHER_MARK;
    if (v[MEL_CODE_DATA - 1u] >> 1) return MEL_CODE_BAD_VALUE;   /* the spare bits */
    mel_fields(&q, f, 1);
    q.rev = MEL_REV;
    if (!mel_valid(&q)) return MEL_CODE_BAD_VALUE;
    *out = q;
    return MEL_CODE_OK;
}

/* ---- noise: a hash, not a generator ------------------------------------------------------------------- */
enum { MEL_SALT_TONIC = 1, MEL_SALT_TEMPO, MEL_SALT_HARMONY, MEL_SALT_STEP, MEL_SALT_BASS };
static uint32_t mel_mix(uint32_t x)
{
    x ^= x >> 16; x *= 0x7FEB352Du;
    x ^= x >> 15; x *= 0x846CA68Bu;
    return x ^ x >> 16;
}
static uint32_t mel_noise(uint32_t seed, uint32_t role, uint32_t bar, uint32_t index, uint32_t salt)
{
    uint32_t h = mel_mix(seed ^ 0x5DEECE66u);
    h = mel_mix(h ^ (role + 0x51u));
    h = mel_mix(h ^ (bar + 0x9E37u));
    h = mel_mix(h ^ (index + 0x1F83u));
    return mel_mix(h ^ salt * 0x85EBCA6Bu);
}

/* ---- styles: the band's rhythmic language ----------------------------------------------------------------
 * A style in four (the STRAIGHT and SWUNG feels) has a groove: where the drums fall, and, where the idiom has one,
 * the bass's line and the chords' rhythm. A part the groove does not give is written by the engine as ever, and so
 * is everything in three, six and seven. Rows are steps of the bar (16, or 12 for a triplet feel):
 *   drums    X accented, x struck, o a ghost, . rest
 *   bass     R root, 3 5 6 7 the chord's and scale's other notes, 8 the octave, N the next chord's root (arriving
 *            early), a a step below the next root, t the chord's next note up (a walk)
 *   chords   x struck
 * Only patterns that could be checked against a source, or are standard beyond doubt, are here: docs/RESEARCH-NOTES.md
 * section 5 has them with their sources, and the ones left out. Energy thins a groove (the hats go first, then the
 * ghosts) and never adds what the idiom does not have. */
#define MEL_THUMP 36u                           /* the pulse's notes: which sound, not which pitch */
#define MEL_RIM 37u
#define MEL_SNAP 38u
#define MEL_TICK 42u
#define MEL_OPENHAT 46u
#define MEL_G_OPENX 1u                          /* a hat's X is an open hat (else an accent) */
#define MEL_G_SEVENTHS 2u                       /* the chords are the third, fifth and seventh: the bass has the root */
#define MEL_G_ARP 4u                            /* the chords' row is played a note at a time */
#define MEL_G_BOOGIE 8u                         /* the chords' row is root and fifth, root and sixth, beat by beat */
#define MEL_G_NOFILL 16u                        /* it never varies: no fills */
typedef struct {
    const char *k, *k2, *s, *h, *b, *b2, *c;    /* kick (k2: every other bar), snare, hat, bass (b2 likewise), chords; 0: none / the engine's */
    uint8_t steps, swing, lo, hi, snare, bgate, cgate, flags;   /* swing: the first of a pair of steps takes this much of it (%) */
} mel_groove_t;
enum { MEL_GR_HOUSE, MEL_GR_TWOSTEP, MEL_GR_BOOMBAP, MEL_GR_FUNK, MEL_GR_SECONDLINE, MEL_GR_ONEDROP, MEL_GR_BOSSA, MEL_GR_SALSA,
       MEL_GR_JAZZ, MEL_GR_TRAIN, MEL_GR_SHUFFLE, MEL_GR_MOTORIK, MEL_GR_DNB, MEL_GR_AMBIENT, MEL_GROOVES };
static const mel_groove_t MEL_GROOVE[MEL_GROOVES] = {
    /* house: the kick on every beat, a tick on every offbeat */
    {"x...x...x...x...", 0, "....x.......x...", "..X...X...X...X.", "..R...8...R...8.", 0, 0, 16, 50, 118, 128, MEL_SNAP, 55, 0, 0},
    /* two-step garage: no kick on two, three or four, and a heavy skip */
    {"x.o.......x.....", 0, "....X..o.o..X...", "..Xo..Xo..Xo..Xo", "..R..RR.R..R.R.R", 0, 0, 16, 58, 128, 138, MEL_SNAP, 60, 0, 0},
    /* boom-bap: a loud snare on two and four, lazy kicks between, one chord held */
    {"x......x..x.....", "x......x.xx.....", "....X.......X...", "x.x.x.x.x.x.x.x.", "R......R..R.....", 0, "x...............", 16, 60, 84, 96, MEL_SNAP, 90, 95, 0},
    /* funk: the heavy one, ghosted snares, the hat opening before two and four come round */
    {"x.x.......x..x..", 0, "....X..o.o.oX..o", "xxxxxxxXxxxxxXxx", 0, 0, 0, 16, 50, 96, 116, MEL_SNAP, 0, 0, MEL_G_OPENX},
    /* second line: a rolling snare in three-three-two, between straight and swung */
    {"x.......x.....x.", 0, "XooXooXoXooXooXo", "....x.......x...", "R.......5.....a.", 0, 0, 16, 57, 90, 120, MEL_SNAP, 80, 0, 0},
    /* one drop: nothing on the one, kick and rim together on the drop, the chord on every offbeat */
    {"....x.......x...", 0, "....x.......x...", "x.X.x.X.x.X.x.X.", 0, 0, "..x...x...x...x.", 16, 50, 65, 85, MEL_RIM, 0, 45, 0},
    /* bossa nova: quiet and even, the rim's figure over it, a bass that arrives early */
    {"x..xx..xx..xx..x", 0, "x..x..x...x..x..", "xxxxxxxxxxxxxxxx", "R..55..NR..55..N", 0, 0, 16, 50, 60, 75, MEL_RIM, 85, 0, MEL_G_SEVENTHS},
    /* salsa: the clave (two-three), the shell pattern, a bass that avoids the one, the piano's montuno */
    {0, 0, "..x.x...x..x..x.", "x.x.xx.xx.xx.x.x", "...5..N....5..N.", 0, "x.xx.x.x.x.x.x.x", 16, 50, 80, 110, MEL_RIM, 95, 80, MEL_G_ARP},
    /* jazz: the ride, the hat's chick on two and four, four to the bar in the bass */
    {"o..o..o..o..", 0, "...x.....x..", "x..x.xx..x.x", "R..t..t..a..", 0, "x....x......", 12, 50, 110, 200, MEL_RIM, 92, 40, MEL_G_SEVENTHS},
    /* the train beat: brushes all the way through, root and fifth */
    {"x...x...x...x...", 0, "ooXoooXoooXoooXo", 0, "R...5...R...5...", 0, "..x...x...x...x.", 16, 50, 90, 130, MEL_SNAP, 80, 45, 0},
    /* shuffle: long and short on every part */
    {"x.....x.....", 0, "...X.....X..", "x.xx.xx.xx.x", "R.R3.35.56.6", "7.76.65.53.3", "x.xx.xx.xx.x", 12, 50, 70, 130, MEL_SNAP, 80, 70, MEL_G_BOOGIE},
    /* motorik: it never varies */
    {"x.x...x.x.x...x.", 0, "....x.......x...", "x.x.x.x.x.x.x.x.", "R.R.R.R.R.R.R.R.", 0, "x.x.x.x.x.x.x.x.", 16, 50, 120, 150, MEL_SNAP, 70, 60, MEL_G_NOFILL},
    /* drum and bass: fast drums over a slow bass and a held chord */
    {"x.........x.....", 0, "....X.......X...", "x.x.x.x.x.x.x.x.", "R.........R.....", 0, "x...............", 16, 50, 160, 175, MEL_SNAP, 95, 97, 0},
    /* ambient: no pulse at all */
    {0, 0, 0, 0, "R...............", 0, "x...............", 16, 50, 60, 90, MEL_SNAP, 97, 99, MEL_G_NOFILL},
};
static const mel_groove_t *mel_groove(const mel_piece_t *p)   /* the groove a piece is played in; 0: the engine writes it all */
{
    static const uint8_t PICK[MEL_STYLES][2] = {   /* by style, in the straight feel and the swung */
        {0, 0}, {MEL_GR_HOUSE, MEL_GR_TWOSTEP}, {MEL_GR_BOOMBAP, MEL_GR_BOOMBAP}, {MEL_GR_FUNK, MEL_GR_SECONDLINE},
        {MEL_GR_ONEDROP, MEL_GR_ONEDROP}, {MEL_GR_BOSSA, MEL_GR_BOSSA}, {MEL_GR_SALSA, MEL_GR_SALSA}, {MEL_GR_JAZZ, MEL_GR_JAZZ},
        {MEL_GR_TRAIN, MEL_GR_SHUFFLE}, {MEL_GR_MOTORIK, MEL_GR_MOTORIK}, {MEL_GR_DNB, MEL_GR_DNB}, {MEL_GR_AMBIENT, MEL_GR_AMBIENT}};
    if (p->style == MEL_FREE || p->style >= MEL_STYLES || p->feel > 1u) return 0;
    return &MEL_GROOVE[PICK[p->style][p->feel]];
}

/* ---- the plan: what the settings decide for the whole piece -------------------------------------------- */
static const uint8_t MEL_MODE[8][7] = {         /* the colours, as semitones above the home note */
    {0, 2, 4, 5, 7, 9, 11},                     /* bright: major */
    {0, 2, 4, 5, 7, 9, 10},                     /* warm: mixolydian */
    {0, 2, 3, 5, 7, 9, 10},                     /* wistful: dorian */
    {0, 2, 3, 5, 7, 8, 10},                     /* sad: minor */
    {0, 1, 3, 5, 7, 8, 10},                     /* dark: phrygian */
    {0, 2, 4, 6, 7, 9, 11},                     /* dreamy: lydian */
    {0, 2, 3, 5, 7, 8, 11},                     /* tense: harmonic minor */
    {0, 2, 3, 5, 7, 9, 11}};                    /* aching: melodic minor */
static const uint8_t MEL_TEMPO[MEL_ENERGIES][2] = {{60, 72}, {72, 92}, {92, 116}, {116, 140}, {140, 170}};
static const uint8_t MEL_ONSETS[MEL_ENERGIES][2] = {{1, 2}, {2, 4}, {4, 6}, {6, 10}, {10, 16}};
static const uint8_t MEL_DENSITY[MEL_ROLES][2] = {{1, 2}, {1, 4}, {1, 1}, {3, 4}};   /* of the energy's onsets */
static const uint8_t MEL_CROWDED[MEL_ROLES][2] = {{2, 3}, {1, 2}, {3, 4}, {1, 2}};   /* fuller than this: a finer grid */
static const uint8_t MEL_BACKBEAT[MEL_FEELS] = {2, 2, 1, 3, 4};                      /* the pulse a sparse tick answers on */
static const uint8_t MEL_AUTO_TONIC[7] = {0, 7, 5, 2, 10, 9, 3};                     /* C G F D Bb A Eb */
static const uint8_t MEL_HOME_ORDER[7] = {1, 6, 4, 3, 2, 7, 5};                      /* chords, from at rest to far from home */
static const uint8_t MEL_LEAVES[MEL_FEELS] = {4, 4, 3, 6, 7};                        /* metrical pulses in a bar */
static const uint8_t MEL_EIGHTHS[MEL_FEELS] = {0, 0, 0, 1, 1};                       /* the pulse is a quaver, not a crotchet */
static const uint8_t MEL_STRONG[MEL_FEELS] = {0x05, 0x05, 0x01, 0x09, 0x15};         /* bit l: pulse l starts a group */
/* what a mood asks of the playing (after Juslin & Laukka's cues): how long notes are held (%), how soft the
 * attack is (0 sharp .. 3 slow), how bright the sound is (-2 .. 2) */
static const int8_t MEL_CUE[MEL_MOODS][3] = {{75, 0, 1}, {85, 1, 0}, {90, 1, 0}, {97, 3, -2}, {88, 2, -1}, {97, 3, 0}, {60, 0, 2}};

#define MEL_LEAF_TICKS 96u                      /* a metrical pulse, in ticks: a bar is at most 7 x 96 */

typedef struct {
    uint8_t mode, tonic, leaves, eighths, bars, boost, ncand, anchor, swing, gate, attack;
    int8_t bright, push;                        /* push: a section leans the whole shape up or down (0: as dialled) */
    uint8_t cand[7];
    uint16_t tempo;                             /* crotchets a minute */
} mel_plan_t;

static int mel_engine_plays(const mel_piece_t *p, uint32_t role)
{
    return p->role[role] == MEL_ENGINE || p->role[role] == MEL_HELD;
}

static void mel_plan(const mel_piece_t *p, mel_plan_t *pl)
{
    uint32_t seed = p->variation, lo = MEL_TEMPO[p->energy][0], hi = MEL_TEMPO[p->energy][1], t, i, r;
    const mel_groove_t *g = mel_groove(p);
    const uint8_t *m;
    pl->mode = p->colour ? (uint8_t)(p->colour - 1u) : p->mood;
    pl->tonic = p->key ? (uint8_t)(p->key - 1u) : MEL_AUTO_TONIC[mel_noise(seed, 0, ~0u, ~0u, MEL_SALT_TONIC) % 7u];
    pl->leaves = MEL_LEAVES[p->feel];
    pl->eighths = MEL_EIGHTHS[p->feel];
    pl->bars = MEL_LENGTH_BARS[p->length];
    pl->boost = p->boost;
    pl->push = 0;
    t = lo + mel_noise(seed, 0, ~0u, ~0u, MEL_SALT_TEMPO) % (hi - lo + 1u);
    if (g) {                                    /* a style has its own tempos: the energy moves through them */
        lo = g->lo; hi = g->hi;
        t = lo + (hi - lo) * p->energy / (MEL_ENERGIES - 1u);
    }
    t += (hi - t) * p->boost / MEL_MAX_BOOST;   /* the dial pushes it to the top of its band, never out of it */
    if (p->trim) t = lo + (hi - lo) * (p->trim - 1u) / (MEL_TRIMS - 2u);
    pl->tempo = (uint16_t)t;
    pl->swing = 0;
    if (p->feel == MEL_SWUNG) {                 /* the long part of a swung beat, in ticks: it straightens as the tempo rises */
        uint32_t ratio = 60000u * 256u / t / 100u;   /* Friberg: the short note stays near 100 ms; Q8 */
        ratio = ratio < 512u ? 256u : ratio - 256u;
        if (ratio > 896u) ratio = 896u;
        ratio = 256u + (ratio - 256u) * 154u / 256u;   /* a band, not a ride cymbal: 0.6 of the way */
        pl->swing = (uint8_t)(MEL_LEAF_TICKS * ratio / (ratio + 256u));
    }
    if (g) pl->swing = g->steps == 12u ? 64u : 0u;   /* what the engine writes itself sits on the groove's own grid */
    pl->gate = (uint8_t)MEL_CUE[p->mood][0];
    pl->attack = (uint8_t)MEL_CUE[p->mood][1];
    pl->bright = MEL_CUE[p->mood][2];
    m = MEL_MODE[pl->mode];
    for (i = 0; i < 7u; i++) pl->cand[i] = 0;
    for (i = 0, pl->ncand = 0; i < 7u; i++) {   /* the chords a piece may sit on: the major and minor ones */
        uint32_t d = MEL_HOME_ORDER[i] - 1u;
        uint32_t third = (m[(d + 2u) % 7u] + 12u - m[d]) % 12u, fifth = (m[(d + 4u) % 7u] + 12u - m[d]) % 12u;
        if (d == 0u || ((third == 3u || third == 4u) && fifth == 7u)) pl->cand[pl->ncand++] = (uint8_t)(d + 1u);
    }
    for (r = 0; r < MEL_ROLES && !mel_engine_plays(p, r); r++) {}
    pl->anchor = (uint8_t)r;                    /* the lowest part playing keeps the downbeat (MEL_ROLES: nobody plays) */
}

static uint32_t mel_lerp(uint32_t a, uint32_t b, uint32_t i, uint32_t span)
{
    if (!span) return a;
    return a <= b ? a + (b - a) * i / span : a - (a - b) * i / span;
}

static uint32_t mel_tension(const mel_piece_t *p, const mel_plan_t *pl, uint32_t bar)   /* 0..100: the shape, bar by bar */
{
    uint32_t n = pl->bars, t, peak, q;
    bar %= n;
    switch (p->shape) {
    default: t = 35; break;                     /* a loop: level, and not at rest */
    case MEL_RISE: t = mel_lerp(0, 100, bar, n - 1u); break;
    case MEL_FALL: t = mel_lerp(100, 0, bar, n - 1u); break;
    case MEL_ARCH:
        peak = (n - 1u) / 2u;
        t = bar <= peak ? mel_lerp(0, 100, bar, peak) : mel_lerp(100, 0, bar - peak, n - 1u - peak);
        break;
    case MEL_QA:
        q = n / 2u;
        t = bar < q ? mel_lerp(20, 100, bar, q - 1u) : mel_lerp(60, 0, bar - q, n - q - 1u);
        break;
    }
    if (pl->push) {
        int32_t x = (int32_t)t + pl->push;
        t = (uint32_t)(x < 0 ? 0 : x > 100 ? 100 : x);
    }
    return t + (100u - t) * pl->boost / (2u * MEL_MAX_BOOST);
}

static uint32_t mel_degree(const mel_piece_t *p, const mel_plan_t *pl, uint32_t bar)   /* the chord a bar sits on: 1..7 */
{
    uint32_t n = pl->bars, last = pl->ncand - 1u;
    int32_t idx;
    bar %= n;
    if (bar == 0u) return 1;
    if (bar == n - 1u) {
        if (p->shape == MEL_FALL || p->shape == MEL_ARCH || p->shape == MEL_QA) return 1;   /* these come home */
        if (p->shape == MEL_RISE) return pl->cand[last];
    }
    idx = (int32_t)(mel_tension(p, pl, bar) * last / 100u) + (int32_t)(mel_noise(p->variation, MEL_CHORDS, bar, 0, MEL_SALT_HARMONY) % 3u) - 1;
    return pl->cand[idx < 0 ? 0 : idx > (int32_t)last ? (int32_t)last : idx];
}

/* ---- pitch: scale steps. Step 0 is the home note in MIDI octave -1; seven steps an octave ---------------- */
static int32_t mel_step_midi(const mel_plan_t *pl, int32_t step)
{
    int32_t oct = step >= 0 ? step / 7 : -((6 - step) / 7);
    return pl->tonic + 12 * oct + MEL_MODE[pl->mode][step - 7 * oct];
}
static int32_t mel_step_near(const mel_plan_t *pl, int32_t midi)   /* the scale step nearest a pitch (a tie: the lower) */
{
    int32_t s0 = (midi - pl->tonic) * 7 / 12, s, best = s0 - 2, bd = 1000;
    for (s = s0 - 2; s <= s0 + 2; s++) {
        int32_t d = mel_step_midi(pl, s) - midi;
        if (d < 0) d = -d;
        if (d < bd) { bd = d; best = s; }
    }
    return best;
}
static int mel_in_scale(const mel_plan_t *pl, int32_t midi)
{
    return mel_step_midi(pl, mel_step_near(pl, midi)) == midi;
}
static int32_t mel_into(int32_t pc, int32_t lo)   /* a pitch class, in the octave from lo up */
{
    return lo + ((pc - lo) % 12 + 12) % 12;
}

/* ---- one role's bar ------------------------------------------------------------------------------------ */
#define MEL_MAX_EV 40u
typedef struct {                                /* (no padding) */
    uint16_t at, len;                           /* ticks from the bar line; how long it is held */
    uint8_t note[3], n, vel, spare;
} mel_ev_t;

static const uint8_t MEL_BASS_LO = 33, MEL_CHORD_LO = 52, MEL_MELODY_LO = 64, MEL_MELODY_SPAN = 19;
static const int8_t MEL_ENERGY_LIFT[MEL_ENERGIES] = {-4, -2, 0, 2, 4};   /* more energy sits higher */

static uint32_t mel_bar_ticks(const mel_plan_t *pl) { return pl->leaves * MEL_LEAF_TICKS; }

static uint32_t mel_slot_tick(const mel_plan_t *pl, uint32_t slot, uint32_t sub)
{
    uint32_t leaf = slot / sub, f = slot % sub, s = pl->swing;
    if (!s || sub == 1u) return leaf * MEL_LEAF_TICKS + f * MEL_LEAF_TICKS / sub;
    if (sub == 2u) return leaf * MEL_LEAF_TICKS + (f ? s : 0u);
    return leaf * MEL_LEAF_TICKS + (f == 0u ? 0u : f == 1u ? s / 2u : f == 2u ? s : s + (MEL_LEAF_TICKS - s) / 2u);
}

/* where a role strikes in a bar: slot numbers, in order. Returns how many; *psub: slots to a pulse */
static uint32_t mel_place(const mel_piece_t *p, const mel_plan_t *pl, uint32_t role, uint32_t bar, uint8_t *slot, uint32_t *psub)
{
    uint32_t e = p->energy, t = mel_tension(p, pl, bar), leaves = pl->leaves, sub = 1, k, n, j, base = 0, span, rot = 0;
    uint32_t quiet = MEL_ONSETS[e][0] * MEL_DENSITY[role][0] / MEL_DENSITY[role][1];
    uint32_t busy = MEL_ONSETS[e][1] * MEL_DENSITY[role][0] / MEL_DENSITY[role][1];
    uint32_t floor_sub = pl->boost >= 8u ? 4u : pl->boost >= 4u ? 2u : 1u;
    uint32_t take = role >= MEL_MELODY ? p->take[role] : 0u, floor_form = (p->feel == MEL_ODD) ^ (take & 1u);   /* a take spreads its notes the other way */
    if (!quiet) quiet = 1;
    if (busy < quiet) busy = quiet;
    quiet += (busy - quiet) * pl->boost / (2u * MEL_MAX_BOOST);
    k = quiet + (busy - quiet) * t / 100u;
    while (sub < 4u && k * MEL_CROWDED[role][1] > leaves * sub * MEL_CROWDED[role][0]) sub *= 2u;   /* room between the notes */
    if (role != pl->anchor && sub < floor_sub) sub = floor_sub;
    n = leaves * sub;
    if (k > n) k = n;
    span = n;
    if (role == MEL_CHORDS && pl->anchor == MEL_BASS && leaves > 1u && k <= (leaves - 1u) * sub / 2u + 1u && k < leaves) {
        base = sub;                             /* the harmony answers the bass: after the downbeat, never on it */
        span = n - sub;
    } else if (role == MEL_MELODY && pl->anchor != MEL_MELODY && sub >= 2u)
        rot = sub / 2u;                         /* the tune leans half a pulse behind */
    else if (role == MEL_PULSE && pl->anchor != MEL_PULSE) {
        if (sub == 4u && pl->boost >= 8u) rot = 1;                     /* at the top of the dial the tick sits behind the beat */
        else if (sub == 1u && k == 1u) rot = MEL_BACKBEAT[p->feel];    /* one tick a bar: it answers the downbeat */
        else if (sub == 1u && k == 2u && leaves == 4u) rot = 1;        /* two: the backbeat, two and four */
    }
    if (role != pl->anchor && take >= 2u)       /* and, where it does not keep the downbeat, starts somewhere else */
        rot += (take >> 1) * (sub >= 2u ? sub / 2u : 1u);
    for (j = 0; j < k; j++) {
        uint32_t at = floor_form ? j * span / k : (j * span + k - 1u) / k;   /* evenly: 2+2+3, or 3+3+2 */
        slot[j] = (uint8_t)((base + at + rot) % n);
    }
    for (j = 1; j < k; j++) {                   /* in order (a lean can wrap the last one round) */
        uint8_t v = slot[j];
        uint32_t i = j;
        for (; i && slot[i - 1u] > v; i--) slot[i] = slot[i - 1u];
        slot[i] = v;
    }
    *psub = sub;
    return k;
}

/* a role's bar from the groove's rows */
static uint32_t mel_gtick(const mel_groove_t *g, uint32_t s)
{
    if (g->steps == 12u) return s * 32u;
    return (s >> 1) * 48u + (s & 1u ? 48u * g->swing / 100u : 0u);
}
static uint32_t mel_groove_bar(const mel_piece_t *p, const mel_plan_t *pl, const mel_groove_t *g, uint32_t role, uint32_t bar, mel_ev_t *ev)
{
    uint32_t n = 0, s, t = mel_tension(p, pl, bar), total = mel_bar_ticks(pl), root = mel_degree(p, pl, bar) - 1u, en = p->energy, seen = 0, i;
    int32_t base = 33 + (int32_t)t * 79 / 100 + (en >= 3u ? 6 : 0);
    if (role == MEL_PULSE) {
        const char *row[3];
        uint32_t r;
        row[0] = (bar & 1u) && g->k2 ? g->k2 : g->k; row[1] = g->s; row[2] = g->h;
        for (s = 0; s < g->steps; s++)
            for (r = 0; r < 3u; r++) {
                char c = row[r] ? row[r][s] : '.';
                int32_t vel = base + (c == 'X' ? 14 : c == 'o' ? -28 : 0) + (r == 0u ? 8 : r == 2u ? -10 : 0);
                mel_ev_t *e = &ev[n];
                if (c == '.' || n >= MEL_MAX_EV) continue;
                if (c == 'o' && en < 2u) continue;                                   /* the ghost notes come with energy */
                if (r == 2u && (en == 0u || (en == 1u && (seen++ & 1u)))) continue;  /* the hats: none when still, half when calm */
                e->at = (uint16_t)(mel_gtick(g, s) + r);   /* (a tick apart, so each is its own strike) */
                e->len = 1; e->n = 1; e->note[1] = e->note[2] = e->spare = 0;
                e->note[0] = (uint8_t)(r == 0u ? MEL_THUMP : r == 1u ? g->snare : c == 'X' && (g->flags & MEL_G_OPENX) ? MEL_OPENHAT : MEL_TICK);
                e->vel = (uint8_t)(vel < 20 ? 20 : vel > 127 ? 127 : vel);
                n++;
            }
        return n;
    }
    if (role == MEL_BASS) {
        const char *row = (bar & 1u) && g->b2 ? g->b2 : g->b;
        int32_t rootn = mel_into(mel_step_midi(pl, (int32_t)root), MEL_BASS_LO), st = mel_step_near(pl, rootn), walk = 0;
        int32_t nextn = mel_into(mel_step_midi(pl, (int32_t)mel_degree(p, pl, bar + 1u) - 1), MEL_BASS_LO);
        for (s = 0; s < g->steps && n < MEL_MAX_EV; s++) {
            char c = row[s];
            int32_t note = rootn, vel = base + (s == 0u ? 12 : 0);
            mel_ev_t *e = &ev[n];
            if (c == '.') continue;
            if ((en == 0u && n) || (en == 1u && (seen++ & 1u))) continue;   /* with little energy: fewer of them */
            if (c == '3' || c == '5' || c == '6' || c == '7') note = mel_into(mel_step_midi(pl, st + (c - '1')), MEL_BASS_LO);
            else if (c == '8') note = rootn + 12;
            else if (c == 'N') note = s >= g->steps / 2u ? nextn : rootn;
            else if (c == 't') note = mel_into(mel_step_midi(pl, st + 2 * ++walk), MEL_BASS_LO);
            else if (c == 'a') {
                int32_t a = mel_step_near(pl, nextn) - 1;
                note = mel_step_midi(pl, a) < MEL_BASS_LO ? mel_step_midi(pl, a + 2) : mel_step_midi(pl, a);
            }
            e->at = (uint16_t)mel_gtick(g, s); e->len = 1; e->n = 1; e->note[1] = e->note[2] = e->spare = 0;
            e->note[0] = (uint8_t)note;
            e->vel = (uint8_t)(vel < 20 ? 20 : vel > 127 ? 127 : vel);
            n++;
        }
        for (i = 0; i < n; i++) {
            uint32_t len = ((i + 1u < n ? ev[i + 1u].at : total) - ev[i].at) * g->bgate / 100u;
            ev[i].len = (uint16_t)(len ? len : 1u);
        }
        return n;
    }
    {                                           /* the chords */
        static const uint8_t TURN[4] = {0, 1, 2, 1};
        uint8_t tone[3];
        uint32_t a, hits = 0, sev = g->flags & MEL_G_SEVENTHS ? 2u : 0u;
        int32_t rootc = mel_into(mel_step_midi(pl, (int32_t)root), MEL_CHORD_LO), stc = mel_step_near(pl, rootc);
        for (i = 0; i < 3u; i++) tone[i] = (uint8_t)mel_into(mel_step_midi(pl, (int32_t)(root + sev + 2u * i)), MEL_CHORD_LO);
        for (i = 1; i < 3u; i++)
            for (a = i; a && tone[a - 1u] > tone[a]; a--) { uint8_t x = tone[a]; tone[a] = tone[a - 1u]; tone[a - 1u] = x; }
        if (p->take[MEL_CHORDS] == MEL_OPEN) { uint8_t x = (uint8_t)(tone[1] + 12u); tone[1] = tone[2]; tone[2] = x; }
        for (s = 0; s < g->steps && n < MEL_MAX_EV; s++) {
            mel_ev_t *e = &ev[n];
            int32_t vel = base - 8 + (s == 0u ? 6 : 0);
            if (g->c[s] == '.') continue;
            if ((en == 0u && n) || (en == 1u && (seen++ & 1u))) continue;
            e->at = (uint16_t)mel_gtick(g, s); e->len = 1; e->spare = 0;
            e->note[0] = tone[0]; e->note[1] = tone[1]; e->note[2] = tone[2]; e->n = 3;
            if (p->take[MEL_CHORDS] >= MEL_CLIMB) {}   /* (a manner of its own: whole chords, for it to work on) */
            else if (g->flags & MEL_G_ARP) { e->note[0] = tone[TURN[hits & 3u]]; e->note[1] = e->note[2] = 0; e->n = 1; }
            else if (g->flags & MEL_G_BOOGIE) {
                e->note[0] = (uint8_t)rootc;
                e->note[1] = (uint8_t)mel_step_midi(pl, stc + (s / 3u & 1u ? 5 : 4));
                e->note[2] = 0; e->n = 2;
            }
            e->vel = (uint8_t)(vel < 20 ? 20 : vel > 127 ? 127 : vel);
            hits++; n++;
        }
        for (i = 0; i < n; i++) {
            uint32_t len = ((i + 1u < n ? ev[i + 1u].at : total) - ev[i].at) * g->cgate / 100u;
            ev[i].len = (uint16_t)(len ? len : 1u);
        }
        return n;
    }
}

/* put a note into a bar, in its place in time; one already there at that moment becomes it */
static uint32_t mel_add(mel_ev_t *ev, uint32_t n, uint32_t at, uint32_t note, uint32_t len, uint32_t vel)
{
    uint32_t i, j;
    mel_ev_t *e;
    for (i = 0; i < n && ev[i].at < at; i++) ;
    if (i == n || ev[i].at != at) {
        if (n >= MEL_MAX_EV) return n;
        for (j = n; j > i; j--) ev[j] = ev[j - 1u];
        n++;
    }
    if (i && (uint32_t)ev[i - 1u].at + ev[i - 1u].len > at) ev[i - 1u].len = (uint16_t)(at - ev[i - 1u].at > 3u ? at - ev[i - 1u].at - 2u : 1u);
    if (i + 1u < n && at + len > ev[i + 1u].at) len = ev[i + 1u].at - at > 3u ? ev[i + 1u].at - at - 2u : 1u;
    e = &ev[i];
    e->at = (uint16_t)at; e->len = (uint16_t)len; e->n = 1; e->vel = (uint8_t)vel;
    e->note[0] = (uint8_t)note; e->note[1] = e->note[2] = e->spare = 0;
    return n;
}

/* fill: 0 an ordinary bar; 1 its second half turns the corner (the drums roll, the bass walks into the next chord,
 * the harmony leaves room); 2 the whole bar builds */
static uint32_t mel_bar(const mel_piece_t *p, const mel_plan_t *pl, uint32_t role, uint32_t bar, uint32_t fill, mel_ev_t *ev)
{
    uint8_t slot[MEL_MAX_EV];
    const mel_groove_t *g = mel_groove(p);
    uint32_t grooved = g && (role == MEL_PULSE || (role == MEL_BASS && g->b) || (role == MEL_CHORDS && g->c));
    uint32_t sub = 1, k = grooved ? mel_groove_bar(p, pl, g, role, bar, ev) : mel_place(p, pl, role, bar, slot, &sub), j, t = mel_tension(p, pl, bar);
    uint32_t root = mel_degree(p, pl, bar) - 1u, total = mel_bar_ticks(pl), seed = p->variation | (role >= MEL_MELODY ? (uint32_t)p->take[role] << 10 : 0u);
    int32_t lo = MEL_MELODY_LO + MEL_ENERGY_LIFT[p->energy], hi = lo + MEL_MELODY_SPAN, step = 0;
    int32_t target = lo + 3 + (hi - lo - 6) * (int32_t)t / 100;
    if (g && (g->flags & MEL_G_NOFILL)) fill = 0;
    for (j = 0; j < k && !grooved; j++) {
        mel_ev_t *e = &ev[j];
        uint32_t at = mel_slot_tick(pl, slot[j], sub), next = j + 1u < k ? mel_slot_tick(pl, slot[j + 1u], sub) : total;
        uint32_t leaf = slot[j] / sub, gap = next - at;
        int32_t vel = 33 + (int32_t)t * 79 / 100;                        /* pp .. ff with the tension */
        if (slot[j] % sub) vel -= 8;                                     /* between the pulses: lighter */
        else if (leaf == 0u) vel += 12;                                  /* the downbeat */
        else if (MEL_STRONG[p->feel] >> leaf & 1u) vel += 6;             /* the start of a group */
        e->at = (uint16_t)at;
        e->note[1] = e->note[2] = e->spare = 0;
        e->n = 1;
        switch (role) {
        case MEL_BASS: {
            int fifth = j && mel_noise(seed, role, bar, j, MEL_SALT_BASS) % 3u == 0u;   /* later notes may take the fifth */
            e->note[0] = (uint8_t)mel_into(mel_step_midi(pl, (int32_t)root + (fifth ? 4 : 0)), MEL_BASS_LO);
            e->len = (uint16_t)(gap * 85u / 100u);
            break;
        }
        case MEL_CHORDS: {
            uint32_t i, a;
            for (i = 0; i < 3u; i++)             /* a triad, folded into one octave: the voices move as little as they can */
                e->note[i] = (uint8_t)mel_into(mel_step_midi(pl, (int32_t)(root + 2u * i + (g && (g->flags & MEL_G_SEVENTHS) ? 2u : 0u))), MEL_CHORD_LO);
            for (i = 1; i < 3u; i++)
                for (a = i; a && e->note[a - 1u] > e->note[a]; a--) {
                    uint8_t x = e->note[a]; e->note[a] = e->note[a - 1u]; e->note[a - 1u] = x;
                }
            if (p->take[role] == MEL_OPEN) {     /* open: the middle voice goes up an octave */
                uint8_t x = (uint8_t)(e->note[1] + 12u); e->note[1] = e->note[2]; e->note[2] = x;
            }
            e->n = 3;
            e->len = (uint16_t)(gap * (pl->gate > 90u ? 97u : 90u) / 100u);
            vel -= 8;
            break;
        }
        case MEL_MELODY:
            if (j == 0u) {                       /* a bar opens on a note of its chord, near where the shape points */
                int32_t s0 = mel_step_near(pl, target), s, best = s0, second = s0, bd = 1000, sd = 1000;
                for (s = s0 - 4; s <= s0 + 4; s++) {
                    uint32_t in = (uint32_t)((s % 7 + 7) % 7 + 7 - (int32_t)root) % 7u;
                    int32_t d = mel_step_midi(pl, s) - target;
                    if (in != 0u && in != 2u && in != 4u) continue;
                    if (d < 0) d = -d;
                    if (d < bd) { second = best; sd = bd; best = s; bd = d; }
                    else if (d < sd) { second = s; sd = d; }
                }
                step = mel_noise(seed, role, bar, 0, MEL_SALT_STEP) & 1u ? second : best;
            } else {                             /* then it walks: a step or two, pulled back towards the shape */
                static const int8_t STEPS[4] = {-2, -1, 1, 2};
                int32_t here = mel_step_midi(pl, step), mv = STEPS[mel_noise(seed, role, bar, j, MEL_SALT_STEP) & 3u];
                if (here < target - 3) mv = mv < 0 ? -mv : mv;
                else if (here > target + 3) mv = mv < 0 ? mv : -mv;
                step += mv;
            }
            while (mel_step_midi(pl, step) > hi) step--;
            while (mel_step_midi(pl, step) < lo) step++;
            e->note[0] = (uint8_t)mel_step_midi(pl, step);
            e->len = (uint16_t)(gap * pl->gate / 100u);
            vel += 4;
            break;
        default:                                 /* the pulse: a thump on one, a snap where a busy bar turns, ticks between */
            e->note[0] = slot[j] == 0u ? MEL_THUMP
                       : !(slot[j] % sub) && (MEL_STRONG[p->feel] >> leaf & 1u) && p->energy >= 3u ? MEL_SNAP : MEL_TICK;
            e->len = 8;
            vel -= 4;
            break;
        }
        if (!e->len) e->len = 1;
        e->vel = (uint8_t)(vel < 20 ? 20 : vel > 127 ? 127 : vel);
    }
    if (role == MEL_BASS && p->take[role] && k) {   /* the bass, played another way */
        static const uint8_t TRIAD[4] = {0, 2, 4, 2};
        uint32_t m = p->take[role], half = (pl->leaves + 1u) / 2u * MEL_LEAF_TICKS;
        int32_t rootn = mel_into(mel_step_midi(pl, (int32_t)root), MEL_BASS_LO), st = mel_step_near(pl, rootn);
        if (m >= MEL_FIFTHS && m <= MEL_TRIAD && k == 1u && ev[0].at + 8u < half) {   /* (it takes two notes to tell: the bar's one note becomes two) */
            ev[1] = ev[0];
            if (ev[0].at + ev[0].len + 4u > half) ev[0].len = (uint16_t)(half - ev[0].at - 4u);
            ev[1].at = (uint16_t)half; ev[1].len = (uint16_t)((total - half) * 85u / 100u);
            ev[1].vel = (uint8_t)(ev[1].vel > 26u ? ev[1].vel - 6u : 20u);
            k = 2;
        }
        for (j = 0; j < k && m != MEL_PUSH; j++)
            ev[j].note[0] = (uint8_t)(m == MEL_ROOTS ? rootn
                          : m == MEL_FIFTHS ? (j & 1u ? mel_into(mel_step_midi(pl, st + 4), MEL_BASS_LO) : rootn)
                          : m == MEL_OCTAVES ? rootn + (j & 1u ? 12 : 0)
                          : m == MEL_WALK ? mel_step_midi(pl, st + (int32_t)(j % 5u))
                          : m == MEL_TRIAD ? mel_step_midi(pl, st + TRIAD[j & 3u])
                          : mel_into(pl->tonic, MEL_BASS_LO));
        if (m == MEL_PUSH)                      /* a pickup: the next bar's root, half a pulse early */
            k = mel_add(ev, k, total - MEL_LEAF_TICKS / 2u, (uint32_t)mel_into(mel_step_midi(pl, (int32_t)mel_degree(p, pl, bar + 1u) - 1), MEL_BASS_LO), 40, 92);
    }
    if (role == MEL_PULSE && p->take[role] && !(grooved && !k)) {   /* the pulse's ideas: a backbeat, a pickup, a late snap, a push */
        uint32_t m = p->take[role], mid = pl->leaves / 2u, sw = pl->swing && !pl->eighths ? pl->swing : MEL_LEAF_TICKS / 2u;
        uint32_t sn = grooved ? 1u : 0u, len = grooved ? 1u : 8u;   /* (in a groove each drum has its own tick) */
        if (m == 1u || m == 5u || m == 7u) k = mel_add(ev, k, mid * MEL_LEAF_TICKS + sn, MEL_SNAP, len, 100);
        if (m == 2u || m == 5u || m == 7u) k = mel_add(ev, k, (pl->leaves - 1u) * MEL_LEAF_TICKS + sw, MEL_THUMP, len, 88);
        if (m == 3u || m == 6u || m == 7u) k = mel_add(ev, k, (pl->leaves - 1u) * MEL_LEAF_TICKS + sn, MEL_SNAP, len, 96);
        if (m == 4u || m == 6u) k = mel_add(ev, k, mid * MEL_LEAF_TICKS + sw, MEL_THUMP, len, 90);
    }
    if (role == MEL_CHORDS && p->take[role] >= MEL_CLIMB && k) {   /* the chords, played another way */
        mel_ev_t c = ev[0], blk[8];
        uint32_t m = p->take[role], nb = k < 8u ? k : 8u, n = 0, i;
        for (i = 0; i < nb; i++) blk[i] = ev[i];
        if (m <= MEL_WAVE) {                    /* one note at a time, through the bar */
            static const uint8_t ORDER[3][4] = {{0, 1, 2, 0}, {2, 1, 0, 2}, {0, 1, 2, 1}};
            uint32_t s2 = p->energy >= 3u ? 2u : 1u, cnt = pl->leaves * s2, per = m == MEL_WAVE ? 4u : 3u;
            for (i = 0; i < cnt && n < MEL_MAX_EV; i++, n++) {
                mel_ev_t *e = &ev[n];
                uint32_t at = mel_slot_tick(pl, i, s2), next = i + 1u < cnt ? mel_slot_tick(pl, i + 1u, s2) : total;
                e->at = (uint16_t)at; e->len = (uint16_t)((next - at) * 80u / 100u);
                e->n = 1; e->note[0] = c.note[ORDER[m - MEL_CLIMB][i % per]]; e->note[1] = e->note[2] = e->spare = 0;
                e->vel = (uint8_t)(c.vel + (i == 0u ? 8u : i % s2 ? 0u : 4u));
            }
        } else if (m == MEL_OFFBEAT) {          /* short stabs between the pulses */
            uint32_t cnt = pl->eighths ? pl->leaves / 2u : pl->leaves, stride = p->energy < 2u && cnt > 2u ? 2u : 1u;
            for (i = 0; i < cnt && n < MEL_MAX_EV; i += stride, n++) {
                ev[n] = c;
                ev[n].at = (uint16_t)(pl->eighths ? (2u * i + 1u) * MEL_LEAF_TICKS : mel_slot_tick(pl, 2u * i + 1u, 2));
                ev[n].len = (uint16_t)(ev[n].at % MEL_LEAF_TICKS > MEL_LEAF_TICKS - 32u ? MEL_LEAF_TICKS - ev[n].at % MEL_LEAF_TICKS - 2u : 30u);
            }
        } else                                  /* broken: the root, then the two above it. Rolled: strummed upwards */
            for (i = 0; i < nb && n + 3u <= MEL_MAX_EV; i++) {
                const mel_ev_t *b = &blk[i];
                uint32_t parts = m == MEL_BROKEN ? 2u : 3u, q;
                uint32_t gap = m == MEL_BROKEN ? (b->len >= MEL_LEAF_TICKS ? (pl->swing ? pl->swing : MEL_LEAF_TICKS / 2u) : b->len / 2u) : 6u;
                if (b->len <= gap * (parts - 1u) + 2u) { ev[n++] = *b; continue; }   /* (no room: as a block) */
                for (q = 0; q < parts; q++, n++) {
                    mel_ev_t *e = &ev[n];
                    *e = *b;
                    e->at = (uint16_t)(b->at + q * gap); e->len = (uint16_t)(b->len - q * gap);
                    e->note[1] = e->note[2] = 0; e->n = 1;
                    e->note[0] = b->note[q];
                    if (m == MEL_BROKEN && q) { e->note[1] = b->note[2]; e->n = 2; }
                    if (m == MEL_BROKEN && !q) e->len = (uint16_t)gap;
                }
            }
        k = n;
    }
    if (fill) {
        uint32_t fl = pl->leaves / 2u ? pl->leaves / 2u : 1u, from = fill >= 2u ? 0u : total - fl * MEL_LEAF_TICKS, n = 0;
        if (role == MEL_PULSE) {                /* a roll: snaps and ticks, growing, a snap to finish */
            uint32_t hits = (pl->eighths || (p->energy < 3u && fill < 2u)) ? 2u : 4u, leaf, h, count, i = 0;
            while (n < k && ev[n].at < from) n++;
            count = (total - from) / MEL_LEAF_TICKS * hits;
            for (leaf = from / MEL_LEAF_TICKS; leaf < pl->leaves; leaf++)
                for (h = 0; h < hits && n < MEL_MAX_EV; h++, i++) {
                    mel_ev_t *e = &ev[n];
                    int last = i + 1u == count;
                    if (h && !last && mel_noise(seed, role, bar, i, MEL_SALT_STEP) % 5u == 0u) continue;   /* a gap, for shape */
                    e->at = (uint16_t)(leaf * MEL_LEAF_TICKS + h * MEL_LEAF_TICKS / hits);
                    e->len = 8; e->n = 1; e->note[1] = e->note[2] = e->spare = 0;
                    e->note[0] = last || !(h & 1u) ? MEL_SNAP : MEL_TICK;
                    e->vel = (uint8_t)(last ? 122u : 62u + 50u * i / count);
                    n++;
                }
            return n;
        }
        if (role == MEL_CHORDS) {               /* the harmony gets out of the way */
            for (j = 0; j < k && ev[j].at < from; j++)
                if ((uint32_t)ev[j].at + ev[j].len + 4u > from) ev[j].len = (uint16_t)(from - ev[j].at > 5u ? from - ev[j].at - 4u : 1u);
            if (!j) { ev[0].at = 0; ev[0].len = 40; j = 1; }   /* (a whole bar of build: one stab on the downbeat) */
            return j;
        }
        if (role == MEL_BASS) {                 /* two steps up into the next bar's chord */
            uint32_t last = total - MEL_LEAF_TICKS, half = MEL_LEAF_TICKS / 2u;
            int32_t to = mel_step_near(pl, mel_into(mel_step_midi(pl, (int32_t)mel_degree(p, pl, bar + 1u) - 1), MEL_BASS_LO));
            int32_t dir = mel_step_midi(pl, to - 2) < MEL_BASS_LO ? -1 : 1;   /* from below; from above where there is no room */
            while (n < k && ev[n].at < last) n++;
            if (n && (uint32_t)ev[n - 1u].at + ev[n - 1u].len > last) ev[n - 1u].len = (uint16_t)(last - ev[n - 1u].at > 3u ? last - ev[n - 1u].at - 2u : 1u);
            for (j = 0; j < 2u && n < MEL_MAX_EV; j++, n++) {
                mel_ev_t *e = &ev[n];
                e->at = (uint16_t)(last + j * half);
                e->len = (uint16_t)(half * 85u / 100u); e->n = 1; e->note[1] = e->note[2] = e->spare = 0;
                e->note[0] = (uint8_t)mel_step_midi(pl, to - dir * (2 - (int32_t)j));
                e->vel = (uint8_t)(88u + 12u * j);
            }
            return n;
        }
    }
    return k;
}

/* the last bar: everyone lands on the home chord's first beat and holds it */
static uint32_t mel_ending(const mel_piece_t *p, const mel_plan_t *pl, uint32_t role, mel_ev_t *ev)
{
    mel_ev_t *e = &ev[0];
    uint32_t i, a, total = mel_bar_ticks(pl);
    (void)p;
    e->at = 0; e->len = (uint16_t)(total - 2u); e->n = 1; e->vel = 104; e->note[1] = e->note[2] = e->spare = 0;
    switch (role) {
    case MEL_BASS: e->note[0] = (uint8_t)mel_into(pl->tonic, MEL_BASS_LO); break;
    case MEL_CHORDS:
        for (i = 0; i < 3u; i++) e->note[i] = (uint8_t)mel_into(mel_step_midi(pl, (int32_t)(2u * i)), MEL_CHORD_LO);
        for (i = 1; i < 3u; i++)
            for (a = i; a && e->note[a - 1u] > e->note[a]; a--) { uint8_t x = e->note[a]; e->note[a] = e->note[a - 1u]; e->note[a - 1u] = x; }
        e->n = 3; e->vel = 92;
        break;
    case MEL_MELODY: e->note[0] = (uint8_t)mel_into(pl->tonic, MEL_MELODY_LO + 3); break;
    default: e->note[0] = MEL_THUMP; e->len = 8; e->vel = 118; break;
    }
    return 1;
}

static uint32_t mel_brightness(const mel_piece_t *p, const mel_plan_t *pl, uint32_t bar)   /* 0..127: how open the sound is */
{
    int32_t b = 40 + (int32_t)mel_tension(p, pl, bar) * 55 / 100 + 12 * pl->bright;
    return (uint32_t)(b < 0 ? 0 : b > 127 ? 127 : b);
}

/* ---- your keys: what the 27 keys play (0 = F3 .. 26 = G5), for the seat you are in ------------------------
 * degrees: every key is the next note of the scale, the home note under key 0. snapped: a piano, and a key that
 * is not in the scale plays the nearest one that is. chromatic: a piano. */
static const int8_t MEL_SEAT_SHIFT[MEL_ROLES] = {-24, -12, 0, 0};
static uint32_t mel_key_note(const mel_piece_t *p, const mel_plan_t *pl, uint32_t role, uint32_t key, int32_t octave)
{
    int32_t base = 53 + MEL_SEAT_SHIFT[role] + 12 * octave, note;
    if (p->layout == MEL_DEGREES) {
        int32_t home = mel_step_near(pl, mel_into(pl->tonic, base - 5));   /* the home note nearest F, from just below */
        note = mel_step_midi(pl, home + (int32_t)key);
    } else {
        note = base + (int32_t)key;
        if (p->layout == MEL_SNAPPED) note = mel_step_midi(pl, mel_step_near(pl, note));
    }
    return note < 0 || note > 127 ? 255u : (uint32_t)note;
}
#endif
