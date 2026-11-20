/* SPDX-License-Identifier: GPL-3.0-only */
/* Melisma's engine on a computer: what it promises, checked over the whole dial space.
 *
 *   cc -O2 -Wall -Wextra -Ifirmware/src -o build/host/mel_test tests/mel_test.c
 *   build/host/mel_test                 the checks
 *   build/host/mel_test dump [CODE]     a piece's notes as text (tests/golden/ holds some: a change shows there)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mel_engine.h"
#include "mel_presets.h"

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { if (fails++ < 20) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } } while (0)

static void describe(const mel_piece_t *p, char *s)
{
    sprintf(s, "%s %s %s %s col %u key %u len %u var %u boost %u", MEL_MOOD_NAME[p->mood], MEL_ENERGY_NAME[p->energy],
            MEL_FEEL_NAME[p->feel], MEL_SHAPE_NAME[p->shape], p->colour, p->key, p->length, p->variation, p->boost);
}

static void all_engine(mel_piece_t *p)
{
    uint32_t r;
    for (r = 0; r < MEL_ROLES; r++) p->role[r] = MEL_ENGINE;
}

static uint32_t lcg = 12345;
static uint32_t rnd(uint32_t n) { lcg = lcg * 1664525u + 1013904223u; return (lcg >> 8) % n; }
static void random_piece(mel_piece_t *p)
{
    uint32_t r;
    memset(p, 0, sizeof *p);
    p->mood = (uint8_t)rnd(MEL_MOODS); p->energy = (uint8_t)rnd(MEL_ENERGIES); p->feel = (uint8_t)rnd(MEL_FEELS);
    p->shape = (uint8_t)rnd(MEL_SHAPES); p->colour = (uint8_t)rnd(MEL_COLOURS); p->key = (uint8_t)rnd(MEL_KEYS);
    p->length = (uint8_t)rnd(MEL_LENGTHS); p->layout = (uint8_t)rnd(MEL_LAYOUTS); p->follow = (uint8_t)rnd(2);
    p->trim = (uint8_t)rnd(MEL_TRIMS); p->boost = (uint8_t)rnd(MEL_MAX_BOOST + 1u); p->variation = (uint16_t)rnd(MEL_VARIATIONS);
    p->style = (uint8_t)rnd(MEL_STYLES);
    for (r = 0; r < MEL_ROLES; r++) { p->role[r] = (uint8_t)rnd(4); p->sound[r] = (uint8_t)rnd(MEL_SOUNDS); p->take[r] = (uint8_t)rnd(MEL_TAKES); }
}

/* ---- the code */
static void test_code(void)
{
    mel_piece_t p, q;
    char text[MEL_CODE_TEXT], bad[MEL_CODE_TEXT];
    uint32_t i, j, c, caught = 0, tried = 0;
    mel_default(&p);
    CHECK(mel_valid(&p), "the default piece is not valid");
    mel_code_make(&p, text);
    CHECK(strlen(text) == 20 && text[6] == '-' && text[13] == '-', "code shape: %s", text);
    for (i = 0; i < 20000u; i++) {
        random_piece(&p);
        mel_code_make(&p, text);
        memset(&q, 0xEE, sizeof q);
        CHECK(mel_code_read(text, &q) == MEL_CODE_OK, "a made code does not read: %s", text);
        CHECK(!memcmp(&p, &q, sizeof p), "a code read back differs: %s", text);
    }
    for (i = 0; i < 300u; i++) {                /* one wrong character: always caught */
        random_piece(&p);
        mel_code_make(&p, text);
        for (j = 0; j < 20u; j++) {
            if (text[j] == '-') continue;
            for (c = 0; c < 32u; c++) {
                if (MEL_ALPHABET[c] == text[j]) continue;
                memcpy(bad, text, sizeof bad);
                bad[j] = MEL_ALPHABET[c];
                tried++;
                if (mel_code_read(bad, &q) != MEL_CODE_OK) caught++;
            }
        }
    }
    CHECK(caught == tried, "one wrong character got through: %u of %u caught", caught, tried);
    for (i = 0, caught = tried = 0; i < 3000u; i++) {   /* two neighbours swapped: nearly always */
        random_piece(&p);
        mel_code_make(&p, text);
        for (j = 0; j + 1u < 20u; j++) {
            uint32_t b = text[j + 1u] == '-' ? j + 2u : j + 1u;
            if (text[j] == '-' || text[j] == text[b]) continue;
            memcpy(bad, text, sizeof bad);
            bad[j] = text[b]; bad[b] = text[j];
            tried++;
            if (mel_code_read(bad, &q) != MEL_CODE_OK) caught++;
        }
    }
    CHECK(caught * 1000u >= tried * 995u, "swapped neighbours: only %u of %u caught", caught, tried);
    mel_default(&p);
    mel_code_make(&p, text);
    for (j = 0; text[j]; j++) bad[j] = (char)(text[j] >= 'A' && text[j] <= 'Z' ? text[j] + 32 : text[j] == '-' ? ' ' : text[j]);
    bad[j] = 0;
    CHECK(mel_code_read(bad, &q) == MEL_CODE_OK && !memcmp(&p, &q, sizeof p), "small letters and spaces should read");
    CHECK(mel_code_read("K7QM2X-VD9F4T-RW8AB", &q) == MEL_CODE_BAD_CHAR, "a short code");
    CHECK(mel_code_read("K7QM2X-VD9F4T-RW8A01", &q) == MEL_CODE_BAD_CHAR, "0 and 1 are not in the alphabet");
}

/* ---- every piece: no wrong notes, in its register, in its bar, in order */
static uint32_t strikes, crowded, distinct;     /* over everything: how often 3+ roles strike together */
static void check_piece_fill(const mel_piece_t *p, uint32_t fill);
static void check_piece(const mel_piece_t *p) { check_piece_fill(p, 0); }
static void check_piece_fill(const mel_piece_t *p, uint32_t fill)
{
    static const uint8_t ROLE_LO[3] = {33, 52, 60}, ROLE_HI[3] = {56, 75, 87};
    const mel_groove_t *gr = mel_groove(p);
    mel_plan_t pl;
    mel_ev_t ev[MEL_ROLES][MEL_MAX_EV], again[MEL_MAX_EV];
    uint32_t n[MEL_ROLES], bar, r, j, total;
    char d[160];
    describe(p, d);
    mel_plan(p, &pl);
    total = mel_bar_ticks(&pl);
    if (gr) CHECK(pl.tempo >= gr->lo && pl.tempo <= gr->hi, "%s: tempo %u out of its style's band", d, pl.tempo);
    else CHECK(pl.tempo >= MEL_TEMPO[p->energy][0] && pl.tempo <= MEL_TEMPO[p->energy][1], "%s: tempo %u out of its band", d, pl.tempo);
    CHECK(pl.ncand >= 2u && pl.cand[0] == 1u, "%s: nowhere to go (%u chords)", d, pl.ncand);
    for (bar = 0; bar < pl.bars; bar++) {
        uint8_t at[7 * 96];
        uint32_t deg = mel_degree(p, &pl, bar), t = mel_tension(p, &pl, bar);
        memset(at, 0, sizeof at);
        CHECK(deg >= 1u && deg <= 7u, "%s: bar %u on degree %u", d, bar, deg);
        CHECK(t <= 100u, "%s: tension %u", d, t);
        if (bar == 0u) CHECK(deg == 1u, "%s: does not open at home", d);
        for (r = 0; r < MEL_ROLES; r++) {
            n[r] = mel_bar(p, &pl, r, bar, fill, ev[r]);
            CHECK((n[r] >= 1u || (r == MEL_PULSE && gr && !gr->k && !gr->s && !gr->h)) && n[r] <= MEL_MAX_EV, "%s: %s bar %u has %u notes", d, MEL_ROLE_NAME[r], bar, n[r]);
            CHECK(mel_bar(p, &pl, r, bar, fill, again) == n[r] && !memcmp(ev[r], again, n[r] * sizeof again[0]),
                  "%s: %s bar %u is not the same twice", d, MEL_ROLE_NAME[r], bar);
            for (j = 0; j < n[r]; j++) {
                const mel_ev_t *e = &ev[r][j];
                uint32_t i, m = p->take[MEL_CHORDS], rung = r == MEL_CHORDS && (m == MEL_BROKEN || m == MEL_ROLLED);   /* (their notes ring on together) */
                CHECK(e->at < total, "%s: %s bar %u note %u starts at %u of %u", d, MEL_ROLE_NAME[r], bar, j, e->at, total);
                CHECK(e->len >= 1u && e->at + e->len <= total, "%s: %s bar %u note %u runs past the bar", d, MEL_ROLE_NAME[r], bar, j);
                CHECK(!j || e->at > ev[r][j - 1u].at, "%s: %s bar %u notes out of order", d, MEL_ROLE_NAME[r], bar);
                CHECK(!j || rung || ev[r][j - 1u].at + ev[r][j - 1u].len <= e->at, "%s: %s bar %u note %u overlaps the next", d, MEL_ROLE_NAME[r], bar, j - 1u);
                CHECK(e->vel >= 20u && e->vel <= 127u, "%s: velocity %u", d, e->vel);
                if (r != MEL_CHORDS) { CHECK(e->n == 1u, "%s: %s plays %u notes at once", d, MEL_ROLE_NAME[r], e->n); }
                else if (gr && gr->c && (gr->flags & (MEL_G_ARP | MEL_G_BOOGIE)) && m < MEL_CLIMB) { CHECK(e->n <= 2u, "%s: a chord of %u in a style that plays fewer", d, e->n); }
                else if (m == MEL_BLOCK || m == MEL_OPEN || m == MEL_OFFBEAT) { CHECK(e->n == 3u, "%s: a %s chord of %u notes", d, MEL_MANNER_NAME[m], e->n); }
                else if (m <= MEL_WAVE) { CHECK(e->n == 1u, "%s: %s plays %u notes at once", d, MEL_MANNER_NAME[m], e->n); }
                else { CHECK(e->n >= 1u && e->n <= 3u, "%s: %s plays %u notes at once", d, MEL_MANNER_NAME[m], e->n); }
                if (r == MEL_PULSE) {
                    CHECK(e->note[0] == MEL_THUMP || e->note[0] == MEL_SNAP || e->note[0] == MEL_TICK || e->note[0] == MEL_RIM || e->note[0] == MEL_OPENHAT, "%s: pulse note %u", d, e->note[0]);
                } else
                    for (i = 0; i < e->n; i++) {
                        CHECK(mel_in_scale(&pl, e->note[i]), "%s: %s bar %u plays %u, not in the scale", d, MEL_ROLE_NAME[r], bar, e->note[i]);
                        CHECK(e->note[i] >= ROLE_LO[r] && e->note[i] <= ROLE_HI[r], "%s: %s bar %u plays %u, out of its register", d, MEL_ROLE_NAME[r], bar, e->note[i]);
                    }
                if (!(at[e->at] >> r & 1u)) at[e->at] |= (uint8_t)(1u << r);
            }
        }
        if (n[MEL_BASS] && !(gr && gr->b)) CHECK(ev[MEL_BASS][0].at == 0u, "%s: bar %u: the bass is not on the downbeat", d, bar);
        if ((bar == 0u || p->shape != MEL_LOOP) && p->take[MEL_BASS] != MEL_PEDAL && !(gr && gr->b)) {   /* a bar's first bass note is its chord's root */
            int32_t rootpc = mel_step_midi(&pl, (int32_t)deg - 1) % 12;
            CHECK(ev[MEL_BASS][0].note[0] % 12 == rootpc, "%s: bar %u: the bass opens on %u, the chord's root is %d", d, bar, ev[MEL_BASS][0].note[0], rootpc);
        }
        if (fill && !(gr && (gr->flags & MEL_G_NOFILL))) {   /* a fill: the drums roll to the bar line, the harmony leaves the second half alone */
            uint32_t from = fill >= 2u ? 0u : total - (pl.leaves / 2u ? pl.leaves / 2u : 1u) * MEL_LEAF_TICKS;
            CHECK(ev[MEL_PULSE][n[MEL_PULSE] - 1u].note[0] == MEL_SNAP && ev[MEL_PULSE][n[MEL_PULSE] - 1u].at + MEL_LEAF_TICKS > total,
                  "%s: bar %u: the fill does not end on a snap at the bar line", d, bar);
            for (j = 0; j < n[MEL_CHORDS]; j++)
                CHECK(ev[MEL_CHORDS][j].at + ev[MEL_CHORDS][j].len <= (from ? from : total), "%s: bar %u: the chords play through the fill", d, bar);
        }
        for (j = 0; j < total && !fill; j++) {
            uint32_t c = (at[j] & 1u) + (at[j] >> 1 & 1u) + (at[j] >> 2 & 1u) + (at[j] >> 3 & 1u);
            strikes += c;
            if (c) distinct++;
            if (c >= 3u) crowded += c;
        }
    }
}

static void test_space(void)
{
    mel_piece_t p;
    uint32_t mood, energy, feel, shape, v, i, n = 0;
    for (mood = 0; mood < MEL_MOODS; mood++)
        for (energy = 0; energy < MEL_ENERGIES; energy++)
            for (feel = 0; feel < MEL_FEELS; feel++)
                for (shape = 0; shape < MEL_SHAPES; shape++)
                    for (v = 0; v < 3u; v++, n++) {
                        mel_default(&p);
                        all_engine(&p);
                        p.mood = (uint8_t)mood; p.energy = (uint8_t)energy; p.feel = (uint8_t)feel; p.shape = (uint8_t)shape;
                        p.variation = (uint16_t)(v * 409u + mood);
                        check_piece(&p);
                    }
    printf("space    %u pieces at the default length: %u strikes, %u.%02u%% where 3+ roles land together, %u.%02u distinct attacks a strike\n",
           n, strikes, crowded * 100u / strikes, crowded * 10000u / strikes % 100u, distinct / strikes, distinct * 100u / strikes % 100u);
    CHECK(crowded * 100u < strikes * 35u, "too many strikes land together: the parts are a wall");
    for (i = 0; i < 4000u; i++) {               /* and anywhere else in the space: colours, keys, lengths, the dial, takes */
        random_piece(&p);
        all_engine(&p);
        check_piece(&p);
        if (i < 1500u) check_piece_fill(&p, 1u + i % 2u);   /* and with every bar a fill */
    }
    for (i = 0; i < 500u; i++) {                /* the last bar: everyone on the home chord, on the beat */
        mel_plan_t pl;
        mel_ev_t ev[MEL_MAX_EV];
        uint32_t r, k;
        random_piece(&p);
        mel_plan(&p, &pl);
        for (r = 0; r < MEL_ROLES; r++) {
            CHECK(mel_ending(&p, &pl, r, ev) == 1u && ev[0].at == 0u, "an ending is one note on the beat");
            for (k = 0; r != MEL_PULSE && k < ev[0].n; k++)
                CHECK(mel_in_scale(&pl, ev[0].note[k]), "the ending's %s plays %u, not in the scale", MEL_ROLE_NAME[r], ev[0].note[k]);
        }
        CHECK(mel_ending(&p, &pl, MEL_BASS, ev) && ev[0].note[0] % 12u == pl.tonic, "the ending's bass is not the home note");
    }
}

/* ---- the shape is in the notes */
static void test_shape(void)
{
    mel_piece_t p;
    mel_plan_t pl;
    mel_ev_t ev[MEL_MAX_EV];
    uint32_t v, bar, r, j;
    for (v = 0; v < 50u; v++) {
        uint32_t vel[2] = {0, 0}, cnt[2] = {0, 0}, prev = 0;
        mel_default(&p);
        all_engine(&p);
        p.shape = MEL_RISE; p.length = 3; p.variation = (uint16_t)(v * 19u);
        p.energy = (uint8_t)(v % MEL_ENERGIES);
        mel_plan(&p, &pl);
        for (bar = 0; bar < pl.bars; bar++) {
            uint32_t t = mel_tension(&p, &pl, bar);
            CHECK(t >= prev, "rise: tension falls at bar %u", bar);
            prev = t;
            for (r = 0; r < MEL_ROLES; r++) {
                uint32_t n = mel_bar(&p, &pl, r, bar, 0, ev), h = bar >= pl.bars / 2u;
                for (j = 0; j < n; j++) { vel[h] += ev[j].vel; cnt[h]++; }
            }
        }
        CHECK(cnt[1] >= cnt[0], "rise %u: the second half is thinner (%u then %u notes)", v, cnt[0], cnt[1]);
        CHECK(vel[1] * cnt[0] > vel[0] * cnt[1] * 12u / 10u, "rise %u: the second half is not louder", v);
        p.shape = MEL_ARCH;
        mel_plan(&p, &pl);
        CHECK(mel_tension(&p, &pl, (pl.bars - 1u) / 2u) > mel_tension(&p, &pl, 0) + 50u &&
              mel_tension(&p, &pl, (pl.bars - 1u) / 2u) > mel_tension(&p, &pl, pl.bars - 1u) + 50u, "arch %u has no arch", v);
        CHECK(mel_degree(&p, &pl, pl.bars - 1u) == 1u, "arch %u does not come home", v);
        p.shape = MEL_LOOP;
        mel_plan(&p, &pl);
        CHECK(mel_tension(&p, &pl, 0) == mel_tension(&p, &pl, pl.bars - 1u), "a loop is not level");
    }
}

/* ---- the dials do what their words say */
static void test_dials(void)
{
    mel_piece_t p;
    mel_plan_t pl;
    mel_ev_t ev[MEL_MAX_EV];
    uint32_t e, v, r, bar, prev_n = 0, prev_t = 0;
    for (e = 0; e < MEL_ENERGIES; e++) {        /* more energy: more notes, faster */
        uint32_t n = 0, tempo = 0;
        for (v = 0; v < 40u; v++) {
            mel_default(&p);
            all_engine(&p);
            p.energy = (uint8_t)e; p.variation = (uint16_t)(v * 7u);
            mel_plan(&p, &pl);
            tempo += pl.tempo;
            for (bar = 0; bar < pl.bars; bar++)
                for (r = 0; r < MEL_ROLES; r++) n += mel_bar(&p, &pl, r, bar, 0, ev);
        }
        CHECK(n > prev_n && tempo > prev_t, "%s is not busier and faster than %s", MEL_ENERGY_NAME[e], e ? MEL_ENERGY_NAME[e - 1u] : "nothing");
        prev_n = n; prev_t = tempo;
    }
    for (v = 0; v < 200u; v++) {                /* a key is a key; a colour is its scale; a waltz has three */
        random_piece(&p);
        mel_plan(&p, &pl);
        if (p.key) CHECK(pl.tonic == p.key - 1u, "key %u gives home note %u", p.key, pl.tonic);
        CHECK(pl.mode == (p.colour ? p.colour - 1u : p.mood), "the scale is not the colour's");
        CHECK(pl.leaves == MEL_LEAVES[p.feel], "the bar has the wrong number of pulses");
        CHECK(pl.bars == MEL_LENGTH_BARS[p.length], "the piece has the wrong length");
        if (mel_groove(&p)) CHECK(pl.swing == (mel_groove(&p)->steps == 12u ? 64u : 0u), "a style's swing is its groove's");
        else CHECK((p.feel == MEL_SWUNG) == (pl.swing != 0), "swing where none was asked, or none where it was");
        if (pl.swing) CHECK(pl.swing > 48u && pl.swing < 80u, "a swung beat splits at %u of 96", pl.swing);
    }
    mel_default(&p);                            /* turning the mood keeps the rhythm: only the notes change */
    all_engine(&p);
    {
        mel_ev_t a[MEL_MAX_EV], b[MEL_MAX_EV];
        mel_plan_t pa, pb;
        uint32_t na, nb, j, same = 1;
        mel_piece_t q = p;
        q.mood = 3;
        mel_plan(&p, &pa); mel_plan(&q, &pb);
        for (bar = 0; bar < pa.bars; bar++) {
            na = mel_bar(&p, &pa, MEL_MELODY, bar, 0, a); nb = mel_bar(&q, &pb, MEL_MELODY, bar, 0, b);
            if (na != nb) same = 0;
            for (j = 0; j < na && j < nb; j++) if (a[j].at != b[j].at) same = 0;
        }
        CHECK(same, "changing the mood moved the tune's rhythm");
    }
}

/* ---- styles: each groove says what it should */
static uint32_t drum_at(const mel_ev_t *ev, uint32_t n, uint32_t tick, uint32_t note)
{
    uint32_t j;
    for (j = 0; j < n; j++)
        if (ev[j].note[0] == note && ev[j].at >= tick && ev[j].at < tick + 3u) return 1;
    return 0;
}
static void test_styles(void)
{
    mel_piece_t p;
    mel_plan_t pl;
    mel_ev_t ev[MEL_MAX_EV], f[MEL_MAX_EV];
    uint32_t st, feel, n, g, s, e, j, prev;
    for (g = 0; g < MEL_GROOVES; g++) {         /* the table itself: every row as long as its bar */
        const mel_groove_t *gr = &MEL_GROOVE[g];
        const char *row[7] = {gr->k, gr->k2, gr->s, gr->h, gr->b, gr->b2, gr->c};
        CHECK(gr->steps == 16u || gr->steps == 12u, "groove %u has %u steps", g, gr->steps);
        CHECK(gr->lo < gr->hi && gr->swing >= 50u && gr->swing <= 67u, "groove %u: tempo or swing", g);
        for (s = 0; s < 7u; s++)
            if (row[s]) CHECK(strlen(row[s]) == gr->steps, "groove %u row %u is %u long", g, s, (unsigned)strlen(row[s]));
    }
    for (st = 1; st < MEL_STYLES; st++)
        for (feel = 0; feel < 2u; feel++) {
            const mel_groove_t *gr;
            mel_default(&p);
            all_engine(&p);
            p.style = (uint8_t)st; p.feel = (uint8_t)feel;
            gr = mel_groove(&p);
            CHECK(gr != 0, "style %s has no groove in feel %u", MEL_STYLE_NAME[st], feel);
            if (!gr) continue;
            mel_plan(&p, &pl);
            n = mel_bar(&p, &pl, MEL_PULSE, 0, 0, ev);
            for (s = 0; s < gr->steps; s++) {   /* at MOVING the drums are the groove, hit for hit */
                if (gr->k) CHECK(drum_at(ev, n, mel_gtick(gr, s), MEL_THUMP) == (gr->k[s] != '.'), "%s: the kick at step %u", MEL_STYLE_NAME[st], s);
                if (gr->s) CHECK(drum_at(ev, n, mel_gtick(gr, s), gr->snare) == (gr->s[s] != '.'), "%s: the snare at step %u", MEL_STYLE_NAME[st], s);
            }
            if (!gr->k && !gr->s && !gr->h) CHECK(n == 0u, "%s should have no drums", MEL_STYLE_NAME[st]);
            if (gr->flags & MEL_G_NOFILL) CHECK(mel_bar(&p, &pl, MEL_PULSE, 0, 1, f) == n && !memcmp(ev, f, n * sizeof ev[0]), "%s took a fill", MEL_STYLE_NAME[st]);
            for (e = 0, prev = 0; e < MEL_ENERGIES; e++) {   /* more energy never means fewer drums, and it is faster */
                uint32_t tempo;
                p.energy = (uint8_t)e;
                mel_plan(&p, &pl);
                tempo = pl.tempo;
                j = mel_bar(&p, &pl, MEL_PULSE, 0, 0, f);
                CHECK(j >= prev, "%s: fewer drums at %s", MEL_STYLE_NAME[st], MEL_ENERGY_NAME[e]);
                CHECK(tempo >= gr->lo && tempo <= gr->hi, "%s: tempo %u", MEL_STYLE_NAME[st], tempo);
                prev = j;
            }
        }
    mel_default(&p);                            /* in three, six and seven a style leaves the writing to the engine */
    p.style = MEL_CLUB; p.feel = MEL_WALTZ;
    CHECK(mel_groove(&p) == 0, "a groove in three");
    mel_default(&p);
    p.style = MEL_ISLAND;                       /* the one drop: nothing on the one */
    all_engine(&p);
    mel_plan(&p, &pl);
    n = mel_bar(&p, &pl, MEL_PULSE, 0, 0, ev);
    CHECK(!drum_at(ev, n, 0, MEL_THUMP) && drum_at(ev, n, 96, MEL_THUMP), "the one drop's kick");
}

/* ---- a take: another idea for one part, the rest as they were */
static void test_takes(void)
{
    mel_piece_t p, q;
    mel_plan_t pa, pb;
    mel_ev_t a[MEL_MAX_EV], b[MEL_MAX_EV];
    uint32_t v, r, o, bar, na, nb, tk;
    for (v = 0; v < 60u; v++) {
        mel_default(&p);
        all_engine(&p);
        p.variation = (uint16_t)(v * 17u); p.energy = (uint8_t)(1u + v % 4u); p.mood = (uint8_t)(v % MEL_MOODS);
        r = v % MEL_ROLES;
        uint32_t distinct = 0;
        for (tk = 1; tk < MEL_TAKES; tk++) {
            uint32_t differs = 0;
            q = p;
            q.take[r] = (uint8_t)tk;
            mel_plan(&p, &pa); mel_plan(&q, &pb);
            CHECK(!memcmp(&pa, &pb, sizeof pa), "a take changed the plan (key, tempo or chords)");
            for (bar = 0; bar < pa.bars; bar++) {
                CHECK(mel_degree(&p, &pa, bar) == mel_degree(&q, &pb, bar), "a take changed the harmony");
                for (o = 0; o < MEL_ROLES; o++) {
                    na = mel_bar(&p, &pa, o, bar, 0, a); nb = mel_bar(&q, &pb, o, bar, 0, b);
                    if (o == r) differs |= na != nb || memcmp(a, b, na * sizeof a[0]);
                    else CHECK(na == nb && !memcmp(a, b, na * sizeof a[0]), "a take of the %s changed the %s", MEL_ROLE_NAME[r], MEL_ROLE_NAME[o]);
                }
            }
            distinct += differs;
            if (r == MEL_CHORDS || r == MEL_MELODY || (r == MEL_BASS && tk != MEL_ROOTS && tk != MEL_PEDAL))
                CHECK(differs, "take %u of the %s is the same as take 0 (variation %u)", tk, MEL_ROLE_NAME[r], p.variation);
        }
        CHECK(distinct >= 4u, "only %u of the %s's seven other takes differ from the first (variation %u)", distinct, MEL_ROLE_NAME[r], p.variation);
    }
}

/* ---- your keys */
static void test_keys(void)
{
    mel_piece_t p;
    mel_plan_t pl;
    uint32_t v, k, role;
    for (v = 0; v < 3000u; v++) {
        uint32_t prev = 0, homes = 0;
        random_piece(&p);
        mel_plan(&p, &pl);
        role = v % MEL_ROLES;
        for (k = 0; k < 27u; k++) {
            uint32_t note = mel_key_note(&p, &pl, role, k, (int32_t)(v % 5u) - 2);
            if (note == 255u) continue;
            if (p.layout != MEL_CHROMATIC) CHECK(mel_in_scale(&pl, (int32_t)note), "layout %u: key %u plays %u, not in the scale", p.layout, k, note);
            if (p.layout == MEL_DEGREES) {
                CHECK(!k || note > prev, "degrees: key %u is not above key %u", k, k - 1u);
                if (!k) CHECK(note % 12u == pl.tonic, "degrees: the first key is %u, not the home note %u", note % 12u, pl.tonic);
            } else
                CHECK(!k || note >= prev, "piano layouts must not go down");
            if (p.layout == MEL_CHROMATIC) CHECK(!k || note == prev + 1u, "chromatic is not chromatic");
            if (note % 12u == pl.tonic) homes++;
            prev = note;
        }
        CHECK(homes >= 2u, "layout %u: fewer than two home notes under the keys", p.layout);
    }
}

static void dump(const mel_piece_t *p)
{
    mel_plan_t pl;
    mel_ev_t ev[MEL_MAX_EV];
    char code[MEL_CODE_TEXT], d[160];
    uint32_t bar, r, j, n, i;
    mel_plan(p, &pl);
    mel_code_make(p, code);
    describe(p, d);
    printf("%s\n%s\ntempo %u, home %s, %u bars of %u x %u ticks, swing %u\n", code, d, pl.tempo, MEL_KEY_NAME[pl.tonic], pl.bars, pl.leaves, MEL_LEAF_TICKS, pl.swing);
    for (bar = 0; bar < pl.bars; bar++) {
        printf("bar %2u  tension %3u  degree %u  bright %u\n", bar + 1u, mel_tension(p, &pl, bar), mel_degree(p, &pl, bar), mel_brightness(p, &pl, bar));
        for (r = 0; r < MEL_ROLES; r++) {
            n = mel_bar(p, &pl, r, bar, 0, ev);
            printf("  %-6s", MEL_ROLE_NAME[r]);
            for (j = 0; j < n; j++) {
                printf(" %u+%u:", ev[j].at, ev[j].len);
                for (i = 0; i < ev[j].n; i++) printf("%s%u", i ? "," : "", ev[j].note[i]);
                printf("@%u", ev[j].vel);
            }
            printf("\n");
        }
    }
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "dump")) {
        mel_piece_t p;
        mel_default(&p);
        all_engine(&p);
        if (argc > 2) {
            int rc = mel_code_read(argv[2], &p);
            if (rc) { printf("code not read (%d)\n", rc); return 2; }
        }
        dump(&p);
        return 0;
    }
    test_code();
    test_space();
    test_shape();
    test_dials();
    test_styles();
    test_takes();
    test_keys();
    {                                           /* the shipped pieces: each valid, each its own, each sound */
        uint32_t i, j;
        for (i = 0; i < MEL_PRESETS; i++) {
            mel_piece_t q = MEL_PRESET[i].piece;
            CHECK(mel_valid(&q), "preset %s is not a valid piece", MEL_PRESET[i].name);
            CHECK(strlen(MEL_PRESET[i].name) <= 20u, "preset name too long for the screen: %s", MEL_PRESET[i].name);
            for (j = 0; j < i; j++) {
                CHECK(strcmp(MEL_PRESET[i].name, MEL_PRESET[j].name), "two presets called %s", MEL_PRESET[i].name);
                CHECK(memcmp(&MEL_PRESET[i].piece, &MEL_PRESET[j].piece, sizeof q), "presets %s and %s are the same piece", MEL_PRESET[i].name, MEL_PRESET[j].name);
            }
            all_engine(&q);
            check_piece(&q);
        }
        printf("presets  %u shipped pieces\n", (unsigned)MEL_PRESETS);
    }
    printf("%u checks, %u failed\n", checks, fails);
    return fails ? 1 : 0;
}
