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
#define MEL_SOUNDS 8u
#define MEL_LAYOUTS 3u
#define MEL_TRIMS 16u                           /* 0 = the engine chooses */
#define MEL_MAX_BOOST 11u

typedef struct {                                /* (no padding: it is compared and stored as bytes) */
    uint16_t variation;
    uint8_t mood, energy, feel, shape, colour, key, length, layout, follow, trim, boost;
    uint8_t role[MEL_ROLES], sound[MEL_ROLES], spare;
} mel_piece_t;
_Static_assert(sizeof(mel_piece_t) == 22, "mel_piece_t must have no padding");

static const char *const MEL_MOOD_NAME[MEL_MOODS] = {"BRIGHT", "WARM", "WISTFUL", "SAD", "DARK", "DREAMY", "TENSE"};
static const char *const MEL_ENERGY_NAME[MEL_ENERGIES] = {"STILL", "CALM", "MOVING", "DRIVING", "FRANTIC"};
static const char *const MEL_FEEL_NAME[MEL_FEELS] = {"STRAIGHT", "SWUNG", "WALTZ", "LILTING", "ODD"};
static const char *const MEL_SHAPE_NAME[MEL_SHAPES] = {"LOOP", "RISE", "FALL", "ARCH", "Q + A"};
static const char *const MEL_COLOUR_NAME[MEL_COLOURS] = {"AUTO", "BRIGHT", "WARM", "WISTFUL", "SAD", "DARK", "DREAMY",
                                                         "TENSE", "ACHING"};
static const char *const MEL_KEY_NAME[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
static const char *const MEL_ROLE_NAME[MEL_ROLES] = {"BASS", "CHORDS", "MELODY", "PULSE"};
static const char *const MEL_LAYOUT_NAME[MEL_LAYOUTS] = {"DEGREES", "SNAPPED", "CHROMATIC"};
static const uint8_t MEL_LENGTH_BARS[MEL_LENGTHS] = {4, 8, 12, 16, 24, 32};

static void mel_default(mel_piece_t *p)         /* doing nothing already sounds good: you on the melody, the band behind */
{
    static const mel_piece_t D = {.mood = 2, .energy = 2, .length = 1, .role = {MEL_ENGINE, MEL_ENGINE, MEL_YOU, MEL_ENGINE}};
    *p = D;
}

static int mel_valid(const mel_piece_t *p)
{
    uint32_t r;
    if (p->mood >= MEL_MOODS || p->energy >= MEL_ENERGIES || p->feel >= MEL_FEELS || p->shape >= MEL_SHAPES ||
        p->colour >= MEL_COLOURS || p->key >= MEL_KEYS || p->length >= MEL_LENGTHS || p->layout >= MEL_LAYOUTS ||
        p->follow > 1u || p->trim >= MEL_TRIMS || p->boost > MEL_MAX_BOOST || p->variation >= MEL_VARIATIONS)
        return 0;
    for (r = 0; r < MEL_ROLES; r++)
        if (p->role[r] > MEL_OFF || p->sound[r] >= MEL_SOUNDS) return 0;
    return 1;
}

/* ---- the code: a piece as 15 characters ------------------------------------------------------------------
 * 68 bits of settings in 14 characters of 5 bits, and a check character. The alphabet has no 0, 1, I or O, so
 * nothing in it can be read as something else. The check is Luhn's, mod 32: it catches any one wrong character,
 * and two neighbours swapped (all but one pair of them). */
#define MEL_CODE_CHARS 15u
#define MEL_CODE_TEXT 18u                       /* with the two dashes and the NUL */
enum { MEL_CODE_OK, MEL_CODE_BAD_CHAR, MEL_CODE_BAD_CHECK, MEL_CODE_OTHER_MARK, MEL_CODE_BAD_VALUE };
static const char MEL_ALPHABET[33] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";

static const uint8_t MEL_FIELD_BITS[23] = {4, 3, 3, 3, 3, 4, 4, 3, 10, 2, 2, 2, 2, 3, 3, 3, 3, 2, 1, 4, 4, 0, 0};
static void mel_fields(mel_piece_t *p, uint16_t *f, int unpack)   /* the settings as a row of numbers, in the code's order */
{
    uint8_t *b[20] = {&p->mood, &p->energy, &p->feel, &p->shape, &p->colour, &p->key, &p->length, 0,
                      &p->role[0], &p->role[1], &p->role[2], &p->role[3], &p->sound[0], &p->sound[1], &p->sound[2],
                      &p->sound[3], &p->layout, &p->follow, &p->trim, &p->boost};
    uint32_t i;
    for (i = 0; i < 20u; i++) {
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

static void mel_code_make(const mel_piece_t *p, char *text)   /* text: MEL_CODE_TEXT bytes, "XXXXX-XXXXX-XXXXX" */
{
    mel_piece_t q = *p;
    uint16_t f[21];
    uint8_t v[MEL_CODE_CHARS] = {0};
    uint32_t i, bit = 0, k;
    f[0] = MEL_MARK;
    mel_fields(&q, f, 0);
    for (i = 0; i < 21u; i++)
        for (k = 0; k < MEL_FIELD_BITS[i]; k++, bit++)
            if (f[i] >> k & 1u) v[bit / 5u] |= (uint8_t)(1u << (bit % 5u));
    v[14] = (uint8_t)((32u - mel_luhn(v, 14, 2)) % 32u);
    for (i = 0, k = 0; i < MEL_CODE_CHARS; i++) {
        if (i == 5u || i == 10u) text[k++] = '-';
        text[k++] = MEL_ALPHABET[v[i]];
    }
    text[k] = 0;
}

static int mel_code_read(const char *text, mel_piece_t *out)   /* dashes, spaces and small letters are fine */
{
    mel_piece_t q = {0};
    uint16_t f[21] = {0};
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
    for (i = 0; i < 21u; i++)
        for (k = 0; k < MEL_FIELD_BITS[i]; k++, bit++)
            if (v[bit / 5u] >> (bit % 5u) & 1u) f[i] |= (uint16_t)(1u << k);
    if (f[0] != MEL_MARK) return MEL_CODE_OTHER_MARK;
    if (v[13] >> 3) return MEL_CODE_BAD_VALUE;  /* the two spare bits */
    mel_fields(&q, f, 1);
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
    int8_t bright;
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
    const uint8_t *m;
    pl->mode = p->colour ? (uint8_t)(p->colour - 1u) : p->mood;
    pl->tonic = p->key ? (uint8_t)(p->key - 1u) : MEL_AUTO_TONIC[mel_noise(seed, 0, ~0u, ~0u, MEL_SALT_TONIC) % 7u];
    pl->leaves = MEL_LEAVES[p->feel];
    pl->eighths = MEL_EIGHTHS[p->feel];
    pl->bars = MEL_LENGTH_BARS[p->length];
    pl->boost = p->boost;
    t = lo + mel_noise(seed, 0, ~0u, ~0u, MEL_SALT_TEMPO) % (hi - lo + 1u);
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
    pl->gate = (uint8_t)MEL_CUE[p->mood][0];
    pl->attack = (uint8_t)MEL_CUE[p->mood][1];
    pl->bright = MEL_CUE[p->mood][2];
    m = MEL_MODE[pl->mode];
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
#define MEL_MAX_EV 28u
#define MEL_THUMP 36u                           /* the pulse's notes: which sound, not which pitch */
#define MEL_SNAP 38u
#define MEL_TICK 42u
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
    for (j = 0; j < k; j++) {
        uint32_t at = p->feel == MEL_ODD ? j * span / k : (j * span + k - 1u) / k;   /* evenly: 2+2+3, or 3+3+2 */
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

static uint32_t mel_bar(const mel_piece_t *p, const mel_plan_t *pl, uint32_t role, uint32_t bar, mel_ev_t *ev)
{
    uint8_t slot[MEL_MAX_EV];
    uint32_t sub, k = mel_place(p, pl, role, bar, slot, &sub), j, t = mel_tension(p, pl, bar);
    uint32_t root = mel_degree(p, pl, bar) - 1u, total = mel_bar_ticks(pl), seed = p->variation;
    int32_t lo = MEL_MELODY_LO + MEL_ENERGY_LIFT[p->energy], hi = lo + MEL_MELODY_SPAN, step = 0;
    int32_t target = lo + 3 + (hi - lo - 6) * (int32_t)t / 100;
    for (j = 0; j < k; j++) {
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
                e->note[i] = (uint8_t)mel_into(mel_step_midi(pl, (int32_t)(root + 2u * i)), MEL_CHORD_LO);
            for (i = 1; i < 3u; i++)
                for (a = i; a && e->note[a - 1u] > e->note[a]; a--) {
                    uint8_t x = e->note[a]; e->note[a] = e->note[a - 1u]; e->note[a - 1u] = x;
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
    return k;
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
