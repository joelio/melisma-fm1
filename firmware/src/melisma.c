/* SPDX-License-Identifier: GPL-3.0-only */
/* Melisma for the M-VAVE FM-1: a four-piece band you sit in with. A Merlin Engines instrument.
 * Built on Dinghy (platform.c and all below it: Felucca / SLOOP's platform). The design: docs/DESIGN.md.
 *
 *   KNOB1..4    mood, energy, feel, shape         with EDIT held: colour, boost, tempo, length
 *   SELECT      the key                           with EDIT held: the key layout
 *   ALGORITHM   the variation                     with EDIT held: who follows whom
 *   FX SEL ENV LFO   bass, chords, melody, pulse: the engine plays it -> you play it -> off
 *   PRESETS     the shipped pieces                with EDIT held: the output level
 *   GLO + a role button   that role's next sound  HOME + a role button   hold that part as it is
 *   PLAY        start and stop                    OCT- OCT+   your octave
 *   REC         keep what you play: it loops with the piece, and you can play over it again. HOME + REC: clear it
 *   keys        you, in the seat you took (or over the band's melody), in the layout chosen
 *
 * A dial turned while the band plays takes effect at the next bar line. The engine (mel_engine.h) is a pure
 * function of the settings; the sound (mel_synth.h) lives in the audio interrupt; this file is the glue.
 * Integers only; see docs/RULES.md. */
#define APP_NAME "MELISMA"
static void app_idle(void);
#define LCD_IDLE app_idle
#include "platform.c"
#include "mel_engine.h"
#include "mel_synth.h"
#include "mel_presets.h"
#include "mel_splash.h"

/* ---- the sequencer: the bar that plays, and the next one, ready before it is needed -------------------- */
#define MAX_TRIG 160u
typedef struct { uint32_t at, len; uint8_t role, n, vel, note[3]; } trig_t;   /* in frames from the bar line */
typedef struct {
    trig_t t[MAX_TRIG];
    uint32_t n, frames, bar;
    uint8_t bright, attack, sound[MEL_ROLES];
} barbuf_t;
static barbuf_t bars[2];
static volatile uint32_t seq_on, seq_cur, seq_ready, seq_need, seq_bar, seq_missed, seq_beat_ms;
static uint32_t seq_pos, seq_ti;               /* the interrupt's: where in the bar, the next note to strike */

/* the main loop's requests to the interrupt: your notes */
typedef struct { uint8_t on, role, note, vel, bright, attack, sound; } req_t;
static req_t req_q[32];
static volatile uint32_t req_w, req_r, seq_stop;

/* the output level: 0 is the default, a step is 1.5 dB (EDIT + PRESETS). Q8 gains for -6 .. +6 */
#define LIM_CEILING 28000
static const uint16_t OUT_GAIN[13] = {228, 271, 322, 383, 455, 541, 643, 764, 908, 1080, 1283, 1525, 1812};
static volatile int32_t out_gain = 643;
static int32_t lim_gain = 4096;

static void app_audio(int32_t *out, uint32_t n)
{
    uint32_t done = 0;
    while (req_r != req_w) {
        const req_t *q = &req_q[req_r % 32u];
        if (q->on) syn_note_on(q->role, q->note, q->vel, SYN_HELD, q->bright, q->attack, q->sound);
        else syn_note_off(q->role, q->note);
        req_r++;
    }
    if (seq_stop) { syn_release_all(); seq_stop = 0; }
    while (done < n) {
        uint32_t run = n - done;
        if (seq_on) {
            const barbuf_t *b = &bars[seq_cur];
            uint32_t lim;
            if (seq_pos >= b->frames) {         /* the bar line */
                if (seq_ready) { seq_cur ^= 1u; seq_ready = 0; }
                else seq_missed++;              /* the main loop was late: this bar once more */
                seq_pos = 0; seq_ti = 0;
                seq_bar = bars[seq_cur].bar;
                seq_need = 1;
                seq_beat_ms = fm1_ms;
                continue;
            }
            while (seq_ti < b->n && b->t[seq_ti].at <= seq_pos) {
                const trig_t *t = &b->t[seq_ti++];
                uint32_t i;
                if (t->role == MEL_PULSE) syn_drum(t->note[0], t->vel);
                else
                    for (i = 0; i < t->n; i++)
                        syn_note_on(t->role, t->note[i], t->vel, t->len, b->bright, b->attack, b->sound[t->role]);
            }
            lim = (seq_ti < b->n ? b->t[seq_ti].at : b->frames) - seq_pos;
            if (lim < run) run = lim;
        }
        syn_render(out + 2u * done, run);
        done += run;
        if (seq_on) seq_pos += run;
    }
    for (done = 0; done < n; done += 32u) {     /* the output stage: make it loud, and hold the loud moments under full scale */
        uint32_t m = n - done < 32u ? n - done : 32u, i;
        int32_t *o = out + 2u * done, gain = out_gain, pk = 0, g0 = lim_gain, g1, want;
        for (i = 0; i < 2u * m; i++) {
            int32_t x = (o[i] * gain) >> 8;
            o[i] = x;
            if (x < 0) x = -x;
            if (x > pk) pk = x;
        }
        want = pk > LIM_CEILING ? (int32_t)((uint32_t)LIM_CEILING * 4096u / (uint32_t)pk) : 4096;   /* Q12 */
        g1 = g0 + ((4096 - g0) >> 8) + 1;       /* it lets go slowly (a fifth of a second), so the bass is not bent */
        if (g1 > want) g1 = want;               /* and takes hold within this 0.7 ms */
        for (i = 0; i < m; i++) {
            int32_t g = g0 + (g1 - g0) * (int32_t)i / (int32_t)m, l = (o[2u * i] * g) >> 12, r = (o[2u * i + 1u] * g) >> 12;
            if (l > 30000) l = 30000 + ((l - 30000) >> 3); else if (l < -30000) l = -30000 + ((l + 30000) >> 3);
            if (r > 30000) r = 30000 + ((r - 30000) >> 3); else if (r < -30000) r = -30000 + ((r + 30000) >> 3);
            o[2u * i] = l;
            o[2u * i + 1u] = r;
        }
        lim_gain = g1;
    }
}

/* ---- the piece (main loop) ----------------------------------------------------------------------------- */
static struct {                                 /* what survives a power-off */
    mel_piece_t piece, held[MEL_ROLES];
    int8_t oct, level;                          /* level: the output trim, -6 .. 6 */
    uint8_t preset, spare;                      /* preset: where the PRESETS dial stands */
} set;
static mel_plan_t plan;

/* ---- your phrase: what you played while REC was lit, looping with the piece -----------------------------
 * A note is kept as a step of the scale, not as a pitch: turn the mood or the key and the phrase moves with the
 * band, still with no wrong notes. Time is kept as a part of the bar, so a change of tempo or feel carries it too. */
#define PHRASE_MAX 400u
typedef struct { uint16_t at, len; uint8_t bar, role, step, vel; } pnote_t;   /* at: bar / 65536; len: bars / 256 (0: still held); role | semitones above the step << 4 */
static struct { uint16_t n, spare; pnote_t note[PHRASE_MAX]; } phrase;
static uint32_t rec_on, phrase_dirty, phrase_new;
static uint16_t held_rec[27][3];                /* the phrase note each key is writing (+1) */

static void push_trig(barbuf_t *b, const trig_t *t)   /* in order of time */
{
    uint32_t i;
    if (b->n >= MAX_TRIG) return;
    for (i = b->n; i && b->t[i - 1u].at > t->at; i--) b->t[i] = b->t[i - 1u];
    b->t[i] = *t;
    b->n++;
}

static void seq_where(uint32_t *bar, uint32_t *at)   /* where the music is now: the bar, and how far through it (/65536) */
{
    uint32_t pos, frames;
    fm1_irq_off();
    *bar = seq_bar; pos = seq_pos; frames = bars[seq_cur].frames;
    fm1_irq_on();
    *at = frames >= 256u ? (pos << 8) / (frames >> 8) : 0u;
    if (*at > 65535u) *at = 65535u;
}

static uint32_t rec_begin(uint32_t role, uint32_t note, uint32_t vel)   /* -> the note's number (+1), 0: not kept */
{
    pnote_t *q;
    uint32_t bar, at;
    if (!rec_on || !seq_on || phrase.n >= PHRASE_MAX) return 0;
    seq_where(&bar, &at);
    q = &phrase.note[phrase.n];
    q->bar = (uint8_t)bar; q->at = (uint16_t)at; q->len = 0; q->vel = (uint8_t)vel;
    if (role == MEL_PULSE) { q->step = (uint8_t)note; q->role = MEL_PULSE; q->len = 4; phrase_new = 1; }
    else {
        int32_t st = mel_step_near(&plan, (int32_t)note);
        if (mel_step_midi(&plan, st) > (int32_t)note) st--;
        q->step = (uint8_t)(st < 0 ? 0 : st);
        q->role = (uint8_t)(role | (uint32_t)((int32_t)note - mel_step_midi(&plan, st)) << 4);
    }
    phrase_dirty = 1;
    return ++phrase.n;
}
static void rec_end(uint32_t idx)
{
    pnote_t *q;
    uint32_t bar, at;
    int32_t len;
    if (!idx || idx > phrase.n) return;
    q = &phrase.note[idx - 1u];
    seq_where(&bar, &at);
    len = (((int32_t)bar - (int32_t)q->bar) * 256) + (((int32_t)at - (int32_t)q->at) / 256);
    if (len <= 0) len += (int32_t)plan.bars * 256;
    q->len = (uint16_t)(len < 4 ? 4 : len > 65535 ? 65535 : len);
    phrase_new = 1;
}
static void phrase_clear(void)
{
    uint32_t k;
    phrase.n = 0;
    for (k = 0; k < 27u; k++) held_rec[k][0] = held_rec[k][1] = held_rec[k][2] = 0;
    phrase_dirty = phrase_new = 1;
}

static void fill_bar(barbuf_t *b, uint32_t bar)   /* one bar of the whole band, as the interrupt wants it */
{
    mel_ev_t ev[MEL_MAX_EV];
    uint32_t r, j, ticks = mel_bar_ticks(&plan);
    uint32_t leaf = (FS * 60u / plan.tempo) >> plan.eighths;
    b->n = 0;
    b->bar = bar % plan.bars;
    b->frames = leaf * plan.leaves;
    b->bright = (uint8_t)mel_brightness(&set.piece, &plan, bar);
    b->attack = plan.attack;
    for (r = 0; r < MEL_ROLES; r++) {
        const mel_piece_t *p = set.piece.role[r] == MEL_HELD ? &set.held[r] : &set.piece;
        mel_plan_t hp, *pl = &plan;
        uint32_t n, rt = ticks;
        b->sound[r] = set.piece.sound[r];
        if (!mel_engine_plays(&set.piece, r)) continue;
        if (p != &set.piece) {                  /* a held part keeps the settings it was made from */
            mel_plan(p, &hp);
            pl = &hp;
            rt = mel_bar_ticks(&hp);
        }
        n = mel_bar(p, pl, r, bar % pl->bars, ev);
        for (j = 0; j < n && b->n < MAX_TRIG; j++) {
            trig_t t;
            t.at = (uint32_t)ev[j].at * b->frames / rt;
            t.len = (uint32_t)ev[j].len * b->frames / rt;
            t.role = (uint8_t)r; t.n = ev[j].n; t.vel = ev[j].vel;
            t.note[0] = ev[j].note[0]; t.note[1] = ev[j].note[1]; t.note[2] = ev[j].note[2];
            push_trig(b, &t);
        }
    }
    for (j = 0; j < phrase.n; j++) {            /* and your phrase, where it falls in this bar */
        const pnote_t *q = &phrase.note[j];
        trig_t t;
        if (!q->len || q->bar % plan.bars != b->bar) continue;
        t.at = ((uint32_t)q->at * (b->frames >> 4)) >> 12;
        t.len = (uint32_t)q->len * (b->frames >> 8);
        t.role = q->role & 15u; t.n = 1; t.vel = q->vel;
        t.note[0] = t.role == MEL_PULSE ? q->step : (uint8_t)(mel_step_midi(&plan, q->step) + (q->role >> 4));
        t.note[1] = t.note[2] = 0;
        if (t.role != MEL_PULSE && t.note[0] > 127u) continue;
        push_trig(b, &t);
    }
}

static void seq_start(void)
{
    seq_on = 0;
    mel_plan(&set.piece, &plan);
    fill_bar(&bars[0], 0);
    fill_bar(&bars[1], 1);
    seq_cur = 0; seq_ready = 1; seq_need = 0; seq_bar = 0;
    seq_pos = 0; seq_ti = 0;
    seq_beat_ms = fm1_ms;
    seq_on = 1;
}
static void seq_halt(void)
{
    seq_on = 0;
    seq_stop = 1;
}
static void seq_service(uint32_t changed)       /* keep the next bar ready; a dial moved: make it again */
{
    if (!seq_on) return;
    if (changed) {
        fm1_irq_off();
        if (seq_ready) { seq_ready = 0; seq_need = 1; }
        fm1_irq_on();
    }
    if (seq_need && !seq_ready) {
        seq_need = 0;
        fill_bar(&bars[seq_cur ^ 1u], seq_bar + 1u);
        seq_ready = 1;
    }
}

/* ---- your playing -------------------------------------------------------------------------------------- */
static uint32_t seat(void)                      /* the role you play: the one set to YOU, else over the melody */
{
    uint32_t r;
    for (r = 0; r < MEL_ROLES; r++)
        if (set.piece.role[r] == MEL_YOU) return r;
    return MEL_MELODY;
}
static void ask(uint32_t on, uint32_t role, uint32_t note, uint32_t vel)
{
    req_t *q;
    if (note > 127u || req_w - req_r >= 32u) return;
    q = &req_q[req_w % 32u];
    q->on = (uint8_t)on; q->role = (uint8_t)role; q->note = (uint8_t)note; q->vel = (uint8_t)vel;
    q->bright = (uint8_t)mel_brightness(&set.piece, &plan, seq_bar);
    q->attack = plan.attack;
    q->sound = set.piece.sound[role];
    req_w++;
}
static uint8_t held_note[27][3], held_n[27], held_role[27];   /* what each key started, so it stops the same */
static uint8_t midi_note_role[128];
static volatile uint32_t pulse_ask;             /* a drum from the keys: note << 8 | velocity */

static void key_down(uint32_t k, uint32_t vel)
{
    uint32_t role = seat(), note = mel_key_note(&set.piece, &plan, role, k, set.oct), i;
    held_role[k] = (uint8_t)role;
    held_n[k] = 0;
    if (note > 127u) return;
    if (role == MEL_PULSE) {                    /* the pulse seat: the keys are the three drums */
        static const uint8_t DRUM[3] = {MEL_THUMP, MEL_SNAP, MEL_TICK};
        pulse_ask = (uint32_t)DRUM[k % 3u] << 8 | vel;
        (void)rec_begin(MEL_PULSE, DRUM[k % 3u], vel);
        return;
    }
    held_note[k][held_n[k]++] = (uint8_t)note;
    if (role == MEL_CHORDS && mel_in_scale(&plan, (int32_t)note)) {   /* the chords seat: one key, the chord on that note */
        int32_t s = mel_step_near(&plan, (int32_t)note);
        held_note[k][held_n[k]++] = (uint8_t)mel_step_midi(&plan, s + 2);
        held_note[k][held_n[k]++] = (uint8_t)mel_step_midi(&plan, s + 4);
    }
    for (i = 0; i < held_n[k]; i++) {
        ask(1, role, held_note[k][i], vel);
        midi_send(0x90u | role, held_note[k][i], vel);
        held_rec[k][i] = (uint16_t)rec_begin(role, held_note[k][i], vel);
    }
}
static void key_up(uint32_t k)
{
    uint32_t i;
    for (i = 0; i < held_n[k]; i++) {
        ask(0, held_role[k], held_note[k][i], 0);
        midi_send(0x80u | held_role[k], held_note[k][i], 0);
        rec_end(held_rec[k][i]);
        held_rec[k][i] = 0;
    }
    held_n[k] = 0;
}

static uint32_t idle_ok;
static void midi_in(void)                       /* notes from USB or the TRS jack play the seat too */
{
    uint8_t st, d1, d2;
    while (midi_get(&st, &d1, &d2) != 0) {
        uint32_t ty = st & 0xF0u, role = seat();
        if (d1 > 127u) continue;
        if (ty == 0x90u && d2) {
            if (role == MEL_PULSE) { pulse_ask = (uint32_t)(d1 % 3u == 0u ? MEL_THUMP : d1 % 3u == 1u ? MEL_SNAP : MEL_TICK) << 8 | d2; continue; }
            midi_note_role[d1] = (uint8_t)(role + 1u);
            ask(1, role, d1, d2);
        } else if ((ty == 0x80u || ty == 0x90u) && midi_note_role[d1]) {
            ask(0, midi_note_role[d1] - 1u, d1, 0);
            midi_note_role[d1] = 0;
        }
    }
}
static void app_idle(void)
{
    if (idle_ok) midi_in();
}

/* ---- the screen: the piece as four words, its shape and the bar that plays, the facts, the code --------- */
#define M_INK RGB(236, 232, 220)                /* cream, as a dial face */
#define M_DIM RGB(120, 118, 110)
#define M_RULE RGB(70, 68, 62)
#define M_ACC RGB(240, 170, 50)                 /* amber: the one accent */
static const uint16_t M_ROLE[MEL_ROLES] = {RGB(170, 120, 220), RGB(90, 160, 220), RGB(235, 165, 60), RGB(110, 190, 120)};

static char *put(char *p, const char *s) { while (*s) *p++ = *s++; *p = 0; return p; }
static char *putn(char *p, uint32_t v)
{
    char t[8];
    uint32_t n = 0;
    do t[n++] = (char)('0' + v % 10u); while ((v /= 10u) != 0u);
    while (n) *p++ = t[--n];
    *p = 0;
    return p;
}

static void draw_word(uint32_t row, const char *word, const char *label, int hot)
{
    cv_begin(240, 32, C_BLACK);
    cv_text(8, 0, &FONT_L, word, hot ? M_ACC : M_INK);
    cv_text(232 - text_w(&FONT_S, label), 14, &FONT_S, label, M_DIM);
    cv_blit(0, 6 + row * 32u);
}

static void draw_words(uint32_t alt, uint32_t hot)   /* alt: EDIT is held; hot: the dial last turned (+1) */
{
    const mel_piece_t *p = &set.piece;
    char s[24], *e;
    if (!alt) {
        draw_word(0, MEL_MOOD_NAME[p->mood], "mood", hot == 1u);
        draw_word(1, MEL_ENERGY_NAME[p->energy], "energy", hot == 2u);
        draw_word(2, MEL_FEEL_NAME[p->feel], "feel", hot == 3u);
        draw_word(3, MEL_SHAPE_NAME[p->shape], "shape", hot == 4u);
        return;
    }
    draw_word(0, MEL_COLOUR_NAME[p->colour], "colour", hot == 1u);
    e = putn(s, p->boost);
    draw_word(1, s, "boost", hot == 2u);
    if (p->trim) putn(s, plan.tempo); else put(s, "AUTO");
    draw_word(2, s, "tempo", hot == 3u);
    e = putn(s, MEL_LENGTH_BARS[p->length]);
    put(e, " BARS");
    draw_word(3, s, "length", hot == 4u);
}

static void draw_bars(uint32_t bar, uint32_t playing)   /* the shape across the piece, and this bar's four lanes */
{
    mel_ev_t ev[MEL_MAX_EV];
    uint32_t n = plan.bars, i, r, j, k, ticks = mel_bar_ticks(&plan);
    int32_t px = 0, py = 0;
    cv_begin(240, 64, C_BLACK);
    cv_rect(8, 0, 224, 1, M_RULE);
    for (i = 0; i < n; i++) {                   /* the tension, bar by bar: a line that is the shape */
        int32_t x = 8 + (int32_t)(i * 224u / n) + (int32_t)(112u / n), y = 22 - (int32_t)mel_tension(&set.piece, &plan, i) * 16 / 100;
        if (i) cv_line(px, py, x, y, M_DIM);
        px = x; py = y;
        cv_rect(8 + (int32_t)(i * 224u / n), 24, 1, 3, M_RULE);
    }
    if (playing || bar) cv_rect(8 + (int32_t)(bar * 224u / n), 4, (int32_t)(224u / n), 21, 0);
    for (i = 0; i < n; i++) {                   /* (again over the bar's block: the line stays on top) */
        int32_t x = 8 + (int32_t)(i * 224u / n) + (int32_t)(112u / n), y = 22 - (int32_t)mel_tension(&set.piece, &plan, i) * 16 / 100;
        if (i) cv_line(px, py, x, y, i == bar || i == bar + 1u ? M_ACC : M_DIM);
        px = x; py = y;
    }
    cv_rect(8 + (int32_t)(bar * 224u / n), 25, (int32_t)(224u / n) > 2 ? (int32_t)(224u / n) - 1 : 2, 2, M_ACC);
    for (r = 0; r < MEL_ROLES; r++) {           /* the lanes: where each part strikes in this bar */
        const mel_piece_t *p = set.piece.role[r] == MEL_HELD ? &set.held[r] : &set.piece;
        int32_t y = 31 + (int32_t)r * 8;
        uint16_t c = M_ROLE[r];
        cv_rect(8, y + 3, 224, 1, M_RULE);
        if (set.piece.role[r] == MEL_OFF) continue;
        if (set.piece.role[r] == MEL_YOU) {     /* your seat: an open lane */
            for (j = 0; j < 224u; j += 6u) cv_rect(8 + (int32_t)j, y + 3, 3, 1, c);
            continue;
        }
        if (p != &set.piece) { mel_plan_t hp; mel_plan(p, &hp); k = mel_bar(p, &hp, r, bar % hp.bars, ev); ticks = mel_bar_ticks(&hp); }
        else { k = mel_bar(p, &plan, r, bar, ev); ticks = mel_bar_ticks(&plan); }
        for (j = 0; j < k; j++) {
            int32_t x = 8 + (int32_t)(ev[j].at * 224u / ticks), w = (int32_t)(ev[j].len * 224u / ticks);
            cv_rect(x, y + 1, w < 2 ? 2 : r == MEL_PULSE ? 2 : w - 1, 5, c);
        }
    }
    for (j = 0; j < phrase.n; j++) {            /* your phrase: cream, over the lane of the part it is in */
        const pnote_t *q = &phrase.note[j];
        int32_t x = 8 + (int32_t)((uint32_t)q->at * 224u >> 16), w = (int32_t)((uint32_t)q->len * 224u >> 8);
        if (!q->len || q->bar % plan.bars != bar) continue;
        if (x + w > 232) w = 232 - x;
        cv_rect(x, 31 + (int32_t)(q->role & 3u) * 8 + 2, w < 2 ? 2 : w, 3, M_INK);
    }
    cv_blit(0, 138);
}

static void draw_facts(uint32_t bar, uint32_t alt)
{
    char s[40], *e;
    cv_begin(240, 16, C_BLACK);
    e = put(s, MEL_KEY_NAME[plan.tonic]);
    cv_text(8, 0, &FONT_S, s, M_INK);
    e = putn(s, plan.tempo);
    cv_text(44, 0, &FONT_S, s, M_INK);
    cv_text(44 + text_w(&FONT_S, s) + 3, 0, &FONT_S, "bpm", M_DIM);
    if (alt) {                                  /* with EDIT held: the output level, where the variation is */
        cv_text(110, 0, &FONT_S, "level ", M_DIM);
        e = put(s, set.level < 0 ? "-" : "+");
        e = putn(e, (uint32_t)(set.level < 0 ? -set.level : set.level));
        cv_text(110 + text_w(&FONT_S, "level "), 0, &FONT_S, s, M_ACC);
    } else {
        e = put(s, "var ");
        cv_text(110, 0, &FONT_S, s, M_DIM);
        e = putn(s, set.piece.variation);
        cv_text(110 + text_w(&FONT_S, "var "), 0, &FONT_S, s, M_INK);
    }
    e = putn(s, bar + 1u);
    e = put(e, "/");
    e = putn(e, plan.bars);
    cv_text(232 - text_w(&FONT_S, s), 0, &FONT_S, s, seq_on ? M_ACC : M_DIM);
    cv_blit(0, 204);
}

static char note_text[32];                      /* a word in the code's place for a moment: a preset's name, a sound */
static uint32_t note_ms, note_on;
static void say(const char *a, uint32_t n, const char *b)
{
    char *e = note_text;
    if (a) e = put(e, a);
    if (n) { e = putn(e, n); e = put(e, " "); }
    put(e, b);
    note_ms = fm1_ms;
    note_on = 1;
}

static void draw_code(uint32_t alt)
{
    char s[40];
    cv_begin(240, 16, C_BLACK);
    if (note_on)
        cv_text((240 - text_w(&FONT_S, note_text)) / 2, 0, &FONT_S, note_text, M_ACC);
    else if (alt) {
        char *e = put(s, "keys ");
        e = put(e, MEL_LAYOUT_NAME[set.piece.layout]);
        cv_text(8, 0, &FONT_S, s, M_DIM);
        put(s, set.piece.follow ? "band follows" : "you follow");
        cv_text(232 - text_w(&FONT_S, s), 0, &FONT_S, s, M_DIM);
    } else {
        mel_code_make(&set.piece, s);
        cv_text((240 - text_w(&FONT_S, s)) / 2, 0, &FONT_S, s, M_DIM);
    }
    cv_blit(0, 222);
}

static void splash(void)                        /* the maker's plate, while the instrument wakes */
{
    uint32_t y, t0;
    for (y = 0; y < 240u; y += 60u) {
        cv_begin(240, 60, C_BLACK);
        mel_splash_rows(y, 60);
        cv_blit(0, y);
    }
    for (t0 = fm1_ms; fm1_ms - t0 < 1600u; ) platform_poll();
    lcd_fill(0, 0, 240, 240, C_BLACK);
}

/* ---- the main loop ------------------------------------------------------------------------------------- */
static const uint8_t ROLE_BTN[MEL_ROLES] = {BTN_FX, BTN_SEL, BTN_ENV, BTN_LFO};

static int turn(uint8_t *v, int32_t by, uint32_t count, int wrap)   /* a dial: by detents, to its ends (or round) */
{
    int32_t x = (int32_t)*v + by, n = (int32_t)count;
    if (!by) return 0;
    x = wrap ? ((x % n) + n) % n : x < 0 ? 0 : x >= n ? n - 1 : x;
    if (x == (int32_t)*v) return 0;
    *v = (uint8_t)x;
    return 1;
}

static void app_main(void)
{
    uint32_t prev = 0, k, r, t_change = 0, dirty = 0, hot = 0, hot_ms = 0;
    uint32_t words_drawn = ~0u, bar_drawn = ~0u, code_drawn = ~0u;

    if (settings_load(&set, sizeof set) || !mel_valid(&set.piece) || set.oct < -2 || set.oct > 2) {
        mel_default(&set.piece);
        set.oct = 0;
    }
    if (set.level < -6 || set.level > 6) set.level = 0;
    if (set.preset >= MEL_PRESETS) set.preset = 0;
    if (blob_load(0, &phrase, sizeof phrase) || phrase.n > PHRASE_MAX) phrase.n = 0;
    out_gain = OUT_GAIN[set.level + 6];
    for (r = 0; r < MEL_ROLES; r++)
        if (!mel_valid(&set.held[r])) { set.held[r] = set.piece; if (set.piece.role[r] == MEL_HELD) set.piece.role[r] = MEL_ENGINE; }
    mel_plan(&set.piece, &plan);
    splash();

    for (;;) {
        uint32_t keys, down, up, rel, b, changed = 0, alt, st, lit = 0, bar;
        mel_piece_t *p = &set.piece;
        int32_t e;
        platform_poll();
        alt = fm1_in.buttons >> BTN_EDIT & 1u;

        /* the dials */
        if ((e = fm1_enc_take(ENC_KNOB1)) != 0 && (alt ? turn(&p->colour, e, MEL_COLOURS, 0) : turn(&p->mood, e, MEL_MOODS, 0))) changed = hot = 1;
        if ((e = fm1_enc_take(ENC_KNOB2)) != 0 && (alt ? turn(&p->boost, e, MEL_MAX_BOOST + 1u, 0) : turn(&p->energy, e, MEL_ENERGIES, 0))) changed = 1, hot = 2;
        if ((e = fm1_enc_take(ENC_KNOB3)) != 0 && (alt ? turn(&p->trim, e, MEL_TRIMS, 0) : turn(&p->feel, e, MEL_FEELS, 0))) changed = 1, hot = 3;
        if ((e = fm1_enc_take(ENC_KNOB4)) != 0 && (alt ? turn(&p->length, e, MEL_LENGTHS, 0) : turn(&p->shape, e, MEL_SHAPES, 0))) changed = 1, hot = 4;
        if ((e = fm1_enc_take(ENC_SELECT)) != 0 && (alt ? turn(&p->layout, e, MEL_LAYOUTS, 0) : turn(&p->key, e, MEL_KEYS, 1))) changed = 1, hot = 5;
        if ((e = fm1_enc_take(ENC_ALGORITHM)) != 0) {
            if (alt) { if (turn(&p->follow, e, 2, 0)) changed = 1, hot = 6; }
            else {
                int32_t v = ((int32_t)p->variation + e) % (int32_t)MEL_VARIATIONS;
                p->variation = (uint16_t)(v < 0 ? v + (int32_t)MEL_VARIATIONS : v);
                changed = 1, hot = 6;
            }
        }
        e = fm1_enc_take(ENC_PRESETS);          /* the shipped pieces; with EDIT held: the output level */
        if (e && !alt) {
            int32_t i = ((int32_t)set.preset + e) % (int32_t)MEL_PRESETS;
            uint8_t layout = p->layout, follow = p->follow;
            set.preset = (uint8_t)(i < 0 ? i + (int32_t)MEL_PRESETS : i);
            *p = MEL_PRESET[set.preset].piece;
            p->layout = layout; p->follow = follow;   /* (how you play is yours, not the piece's) */
            say(0, set.preset + 1u, MEL_PRESET[set.preset].name);
            changed = 1; hot = 0;
        }
        if (e && alt) {
            int32_t l = set.level + e;
            set.level = (int8_t)(l < -6 ? -6 : l > 6 ? 6 : l);
            out_gain = OUT_GAIN[set.level + 6];
            dirty = 1; t_change = fm1_ms;
            bar_drawn = ~0u;
        }

        /* the buttons */
        b = fm1_input_edges(&rel);
        for (r = 0; r < MEL_ROLES; r++)
            if (b >> ROLE_BTN[r] & 1u) {
                if (fm1_in.buttons >> BTN_GLO & 1u) {              /* GLO + a role: its next sound */
                    if (r != MEL_PULSE) {
                        p->sound[r] = (uint8_t)((p->sound[r] + 1u) % SYN_SOUNDS);
                        putn(put(put(note_text, MEL_ROLE_NAME[r]), " sound "), p->sound[r] + 1u);
                        note_ms = fm1_ms;
                        note_on = 1;
                    }
                } else if (fm1_in.buttons >> BTN_HOME & 1u) {      /* HOME + a role: hold it as it is, or let it go */
                    if (p->role[r] == MEL_HELD) p->role[r] = MEL_ENGINE;
                    else if (p->role[r] == MEL_ENGINE) { set.held[r] = *p; p->role[r] = MEL_HELD; }
                } else if (p->role[r] == MEL_ENGINE || p->role[r] == MEL_HELD) {
                    for (k = 0; k < MEL_ROLES; k++)                /* one seat: the part you left goes back to the engine */
                        if (p->role[k] == MEL_YOU) p->role[k] = MEL_ENGINE;
                    p->role[r] = MEL_YOU;
                } else p->role[r] = p->role[r] == MEL_YOU ? MEL_OFF : MEL_ENGINE;
                changed = 1;
            }
        if (b & (1u << BTN_PLAY)) { if (seq_on) { seq_halt(); rec_on = 0; } else seq_start(); bar_drawn = ~0u; }
        if (b & (1u << BTN_REC)) {
            if (fm1_in.buttons >> BTN_HOME & 1u) { phrase_clear(); say("PHRASE CLEARED", 0, ""); }
            else {
                rec_on = !rec_on;
                if (rec_on && !seq_on) seq_start();   /* REC from rest starts the band */
                say(rec_on ? "RECORDING: PLAY" : "KEPT ", rec_on ? 0u : phrase.n, rec_on ? "" : "NOTES");
            }
            bar_drawn = ~0u;
        }
        if (b & (1u << BTN_OCT_DOWN) && set.oct > -2) { set.oct--; dirty = 1; t_change = fm1_ms; }
        if (b & (1u << BTN_OCT_UP) && set.oct < 2) { set.oct++; dirty = 1; t_change = fm1_ms; }

        if (changed) {
            mel_plan(p, &plan);
            dirty = 1; t_change = hot_ms = fm1_ms;
            words_drawn = bar_drawn = code_drawn = ~0u;
        }
        seq_service(changed || phrase_new);
        if (phrase_new) { phrase_new = 0; bar_drawn = ~0u; }

        /* your keys */
        keys = fm1_in.notes;
        down = keys & ~prev;
        up = prev & ~keys;
        prev = keys;
        for (k = 0; k < 27u; k++) {
            if (up >> k & 1u) key_up(k);
            if (down >> k & 1u) key_down(k, 100);
        }
        if (pulse_ask) {                        /* (a drum is a strike, not a held voice: it goes in beside the notes) */
            uint32_t a = pulse_ask;
            pulse_ask = 0;
            fm1_irq_off();
            syn_drum(a >> 8, a & 0x7Fu);
            fm1_irq_on();
        }
        midi_in();

        /* the lights: what fits glows, the home note is lit, what you hold is lit */
        bar = seq_on ? seq_bar : 0u;
        for (k = 0; k < 27u; k++) {
            uint32_t note = mel_key_note(p, &plan, seat(), k, set.oct), in = note < 128u && mel_in_scale(&plan, (int32_t)note);
            uint32_t home = in && note % 12u == plan.tonic;
            if (keys >> k & 1u) lit |= 1u << k;
            fm1_led_key(KEY_LED(k), (keys >> k & 1u) || home);
            fm1_led_key_glow(KEY_LED(k), in && !home);
        }
        for (r = 0; r < MEL_ROLES; r++) {       /* a role's light: lit = the engine plays, blinking = you, glowing = held */
            uint32_t s = p->role[r];
            fm1_led_key(ROLE_BTN[r], s == MEL_ENGINE || (s == MEL_YOU && (fm1_ms >> 8 & 1u)));
            fm1_led_key_glow(ROLE_BTN[r], s == MEL_HELD);
        }
        fm1_led_key(BTN_PLAY, seq_on && fm1_ms - seq_beat_ms > 90u);   /* lit while it plays, a wink on the bar line */
        fm1_led_key(BTN_OCT_DOWN, set.oct < 0);
        fm1_led_key(BTN_OCT_UP, set.oct > 0);
        fm1_led_key(BTN_EDIT, alt);
        fm1_led_key(BTN_REC, rec_on);           /* lit: it is listening; glowing: there is a phrase */
        fm1_led_key_glow(BTN_REC, !rec_on && phrase.n);

        /* the screen: only what changed */
        if (hot && fm1_ms - hot_ms > 1200u) { hot = 0; words_drawn = ~0u; }
        idle_ok = 1;
        if ((st = alt << 8 | hot) != words_drawn) { words_drawn = st; draw_words(alt, hot); }
        if ((st = bar | seq_on << 8 | alt << 9) != bar_drawn) { bar_drawn = st; draw_bars(bar, seq_on); draw_facts(bar, alt); }
        if (note_on && fm1_ms - note_ms > 2500u) note_on = 0;
        if ((st = alt | note_on << 1 | note_ms << 2) != code_drawn) { code_drawn = st; draw_code(alt); }
        idle_ok = 0;
        (void)lit;

        /* keep it: two seconds after the last change, when nothing sounds (an erase stalls the sound) */
        if ((dirty || phrase_dirty) && !seq_on && syn_quiet() && fm1_ms - t_change > 2000u) {
            if (dirty) settings_save(&set, sizeof set);
            if (phrase_dirty) blob_save(0, &phrase, sizeof phrase);
            dirty = phrase_dirty = 0;
        }
    }
}
