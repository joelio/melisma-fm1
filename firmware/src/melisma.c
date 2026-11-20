/* SPDX-License-Identifier: GPL-3.0-only */
/* Melisma for the M-VAVE FM-1: a four-piece band you sit in with. A Merlin Engines instrument.
 * Built on Dinghy (platform.c and all below it: Felucca / SLOOP's platform). The design: docs/DESIGN.md.
 *
 *   KNOB1..4    mood, energy, style, shape        with EDIT held: colour, boost, feel, length
 *   SELECT      the key                           with EDIT held: the key layout
 *   ALGORITHM   the variation                     with EDIT held: the tempo
 *   PRESETS     the shipped pieces                with EDIT held: the output level
 *   FX SEL ENV LFO   bass, chords, melody, pulse. A tap: out, or back in, at the next bar. Held: the seat is yours.
 *               with GLO: its next sound. HOME: hold it as it is. SEQ: its next manner or idea. EDIT: it alone, while held
 *   ARP         a fill to the end of this bar; held: a build for as long as it is held
 *   SEQ         held (or tapped, to stay): the keys are the panel. Black keys: the sections (INTRO A B BREAK ENDING),
 *               a new idea for each part, BACK, ROLL. White keys: eight scenes (HOME + key keeps one), then the
 *               eight ways the chords are played
 *   PLAY        start and stop                    OCT- OCT+   your octave
 *   REC         keep what you play: it loops with the piece, and you can play over it again. HOME + REC: undo the
 *               last pass (held: clear it all). EDIT + REC: snap what you play to the beat, or not
 *   SAVE        keep everything now; held: the piece's code, large
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
#define MAX_TRIG 200u
typedef struct { uint32_t at, len; uint8_t role, n, vel, note[3]; } trig_t;   /* in frames from the bar line */
typedef struct {
    trig_t t[MAX_TRIG];
    uint32_t n, frames, bar, abs;               /* abs: which bar of the performance this was made for (a fill) */
    uint8_t bright, attack, sec, last, sound[MEL_ROLES];
} barbuf_t;
static barbuf_t bars[3];                        /* the bar that plays, the next, and one to write in */
static volatile uint32_t seq_on, seq_cur, seq_nxt, seq_ready, seq_need, seq_bar, seq_abs, seq_missed, seq_beat_ms;
static volatile uint32_t seq_swap, seq_solo, seq_ended;   /* a bar to take up now (+1); the roles let through; the ending is over */
#ifdef DINGHY_HOST
#define load_now() 0u
#else
#define load_now() fm1_ticks()
#endif
#define LOAD_BLOCK (256u * 1000000u / 44100u * 24u)   /* a block of sound, in the timer's ticks (24 a microsecond) */
static volatile uint32_t load_peak;             /* the longest the interrupt has taken (main clears it) */
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
    uint32_t done = 0, t0 = load_now();
    if (seq_swap) {                             /* this bar, made again (a fill): take it up where the music is */
        uint32_t x = seq_swap - 1u;
        if (seq_on && bars[x].abs == seq_abs && bars[x].frames == bars[seq_cur].frames) {
            seq_cur = x;
            for (seq_ti = 0; seq_ti < bars[x].n && bars[x].t[seq_ti].at < seq_pos; seq_ti++) {}
        }
        seq_swap = 0;
    }
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
                if (b->last) { seq_on = 0; seq_ended = 1; continue; }   /* the ending has been played: let it ring */
                if (seq_ready) { seq_cur = seq_nxt; seq_ready = 0; }
                else seq_missed++;              /* the main loop was late: this bar once more */
                seq_pos = 0; seq_ti = 0; seq_abs++;
                seq_bar = bars[seq_cur].bar;
                seq_need = 1;
                seq_beat_ms = fm1_ms;
                continue;
            }
            while (seq_ti < b->n && b->t[seq_ti].at <= seq_pos) {
                const trig_t *t = &b->t[seq_ti++];
                uint32_t i;
                if (seq_solo && !(seq_solo >> t->role & 1u)) continue;
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
    t0 = load_now() - t0;
    if (t0 > load_peak) load_peak = t0;
}

/* ---- the piece (main loop) ----------------------------------------------------------------------------- */
static struct {                                 /* what survives a power-off */
    mel_piece_t piece, held[MEL_ROLES];
    mel_piece_t scene[8];                       /* pieces kept under the panel's white keys */
    int8_t oct, level;                          /* level: the output trim, -6 .. 6 */
    uint8_t preset, snap, scenes, scene_at;     /* where the PRESETS dial stands; REC snaps to the beat; which scenes are kept; the one last used (+1) */
} set;
static mel_plan_t plan;
static uint32_t dirty, t_change;                /* something to keep, and when it last changed */

/* the sections: the same piece, leaning another way. INTRO: chords and pulse. B: more of everything, further from
 * home. BREAK: chords and melody. ENDING: one bar, everyone on the home chord, and the band stops */
enum { SEC_INTRO, SEC_A, SEC_B, SEC_BREAK, SEC_END, SECS };
static const char *const SEC_NAME[SECS] = {"INTRO", "A", "B", "BREAK", "ENDING"};
static const int8_t SEC_PUSH[SECS] = {-25, 0, 30, 0, 0};
static uint32_t sec_cur = SEC_A, fill_hold, pass_mark;
static int sec_plays(uint32_t sec, uint32_t r)
{
    return !(sec == SEC_INTRO && (r == MEL_BASS || r == MEL_MELODY)) && !(sec == SEC_BREAK && (r == MEL_BASS || r == MEL_PULSE));
}

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
    if (set.snap) {                             /* to the nearest place a note of this feel would fall */
        uint32_t total = mel_bar_ticks(&plan), tick = at * total >> 16, sub = plan.swing || plan.eighths ? 2u : 4u;
        uint32_t n = plan.leaves * sub, s, best = 0, bd = ~0u;
        for (s = 0; s <= n; s++) {
            uint32_t g = s < n ? mel_slot_tick(&plan, s, sub) : total, d = g > tick ? g - tick : tick - g;
            if (d < bd) { bd = d; best = g; }
        }
        if (best >= total) { best = 0; bar = (bar + 1u) % plan.bars; }
        at = best * 65536u / total;
    }
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
    if (len < -(int32_t)plan.bars * 128) len += (int32_t)plan.bars * 256;   /* (round the end of the piece) */
    q->len = (uint16_t)(len < (set.snap ? 12 : 4) ? (set.snap ? 12 : 4) : len > 65535 ? 65535 : len);
    phrase_new = 1;
}
static void phrase_clear(void)
{
    uint32_t k;
    phrase.n = 0; pass_mark = 0;
    for (k = 0; k < 27u; k++) held_rec[k][0] = held_rec[k][1] = held_rec[k][2] = 0;
    phrase_dirty = phrase_new = 1;
}

/* one bar of the whole band, as the interrupt wants it. fill: mel_bar's; sec: the section it is played in */
static void fill_bar(barbuf_t *b, uint32_t bar, uint32_t fill, uint32_t sec)
{
    mel_ev_t ev[MEL_MAX_EV];
    mel_plan_t mp = plan;
    uint32_t r, j, ticks = mel_bar_ticks(&plan);
    uint32_t leaf = (FS * 60u / plan.tempo) >> plan.eighths;
    mp.push = SEC_PUSH[sec];
    b->n = 0;
    b->bar = bar % plan.bars;
    b->frames = leaf * plan.leaves;
    b->sec = (uint8_t)sec;
    b->last = sec == SEC_END;
    b->bright = (uint8_t)mel_brightness(&set.piece, &mp, bar);
    b->attack = plan.attack;
    for (r = 0; r < MEL_ROLES; r++) {
        const mel_piece_t *p = set.piece.role[r] == MEL_HELD ? &set.held[r] : &set.piece;
        mel_plan_t hp, *pl = &mp;
        uint32_t n, rt = ticks;
        b->sound[r] = set.piece.sound[r];
        if (!mel_engine_plays(&set.piece, r) || !sec_plays(sec, r)) continue;
        if (p != &set.piece) {                  /* a held part keeps the settings it was made from */
            mel_plan(p, &hp);
            hp.push = mp.push;
            pl = &hp;
            rt = mel_bar_ticks(&hp);
        }
        n = sec == SEC_END ? mel_ending(p, pl, r, ev) : mel_bar(p, pl, r, bar % pl->bars, fill, ev);
        for (j = 0; j < n && b->n < MAX_TRIG; j++) {
            trig_t t;
            t.at = (uint32_t)ev[j].at * b->frames / rt;
            t.len = (uint32_t)ev[j].len * b->frames / rt;
            t.role = (uint8_t)r; t.n = ev[j].n; t.vel = ev[j].vel;
            t.note[0] = ev[j].note[0]; t.note[1] = ev[j].note[1]; t.note[2] = ev[j].note[2];
            push_trig(b, &t);
        }
    }
    for (j = 0; j < phrase.n && sec != SEC_END; j++) {   /* and your phrase, where it falls in this bar */
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

static uint32_t seq_free(void)                  /* a bar nobody is playing or waiting to play */
{
    uint32_t x, cur, nx, rdy;
    fm1_irq_off();
    cur = seq_cur; nx = seq_nxt; rdy = seq_ready;
    fm1_irq_on();
    for (x = 0; x < 2u; x++)
        if (x != cur && !(rdy && x == nx)) break;
    return x;
}
static void seq_start(void)
{
    seq_on = 0; seq_swap = 0; seq_ended = 0;
    if (sec_cur == SEC_END) sec_cur = SEC_A;
    mel_plan(&set.piece, &plan);
    fill_bar(&bars[0], 0, 0, sec_cur);
    fill_bar(&bars[1], 1, 0, sec_cur);
    seq_cur = 0; seq_nxt = 1; seq_ready = 1; seq_need = 0; seq_bar = 0; seq_abs = 0;
    seq_pos = 0; seq_ti = 0;
    seq_beat_ms = fm1_ms;
    seq_on = 1;
}
static void seq_halt(void)
{
    seq_on = 0;
    seq_stop = 1;
}
static void seq_service(uint32_t changed)       /* keep the next bar ready; something changed: make it again */
{
    static uint32_t redo;
    if (changed) redo = 1;
    if (!seq_on) { redo = 0; return; }
    if (seq_swap) return;                       /* (the interrupt is about to take a bar up: wait for it) */
    if (redo) {
        redo = 0;
        fm1_irq_off();
        if (seq_ready) { seq_ready = 0; seq_need = 1; }
        fm1_irq_on();
    }
    if (seq_need && !seq_ready) {
        uint32_t x = seq_free();
        seq_need = 0;
        fill_bar(&bars[x], seq_bar + 1u, fill_hold ? 2u : 0u, sec_cur);
        seq_nxt = x;
        seq_ready = 1;
    }
}
static void fill_swap(uint32_t fill)            /* the bar that is playing, again with a fill: from where the music is */
{
    uint32_t x, bar, abs, sec;
    if (!seq_on || seq_swap) return;
    fm1_irq_off();
    bar = seq_bar; abs = seq_abs; sec = bars[seq_cur].sec;
    fm1_irq_on();
    if (sec == SEC_END) return;
    x = seq_free();
    fill_bar(&bars[x], bar, fill, sec);
    bars[x].abs = abs;
    seq_swap = x + 1u;
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
        static const uint8_t DRUM[5] = {MEL_THUMP, MEL_SNAP, MEL_TICK, MEL_RIM, MEL_OPENHAT};
        pulse_ask = (uint32_t)DRUM[k % 5u] << 8 | vel;
        (void)rec_begin(MEL_PULSE, DRUM[k % 5u], vel);
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
#define M_ACC RGB(169, 205, 180)                /* Sky, the colour under a Spitfire: the one accent */
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
    char s[24], t[16], *e;
    if (alt == 2u) {                            /* the keys are the panel: what it has set */
        draw_word(0, SEC_NAME[sec_cur], "section", 1);
        draw_word(1, MEL_MANNER_NAME[p->take[MEL_CHORDS]], "chords", 0);
        draw_word(2, MEL_BASS_NAME[p->take[MEL_BASS]], "bass", 0);
        putn(put(s, "TUNE "), p->take[MEL_MELODY] + 1u);
        putn(put(t, "beat "), p->take[MEL_PULSE] + 1u);
        draw_word(3, s, t, 0);
        return;
    }
    if (!alt) {
        draw_word(0, MEL_MOOD_NAME[p->mood], "mood", hot == 1u);
        draw_word(1, MEL_ENERGY_NAME[p->energy], "energy", hot == 2u);
        draw_word(2, MEL_STYLE_NAME[p->style], mel_groove(p) || p->style == MEL_FREE ? "style" : "in 4 only", hot == 3u);
        draw_word(3, MEL_SHAPE_NAME[p->shape], "shape", hot == 4u);
        return;
    }
    draw_word(0, MEL_COLOUR_NAME[p->colour], "colour", hot == 1u);
    e = putn(s, p->boost);
    draw_word(1, s, "boost", hot == 2u);
    draw_word(2, MEL_FEEL_NAME[p->feel], "feel", hot == 3u);
    e = putn(s, MEL_LENGTH_BARS[p->length]);
    put(e, " BARS");
    draw_word(3, s, "length", hot == 4u);
}

static void draw_bars(uint32_t bar, uint32_t playing)   /* the shape across the piece, and this bar's four lanes */
{
    mel_ev_t ev[MEL_MAX_EV];
    mel_plan_t mp = plan;
    uint32_t n = plan.bars, i, r, j, k, ticks = mel_bar_ticks(&plan), sec = playing ? bars[seq_cur].sec : sec_cur;
    int32_t px = 0, py = 0;
    if (sec >= SEC_END) sec = SEC_A;
    mp.push = SEC_PUSH[sec];
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
        if (set.piece.role[r] == MEL_OFF || !sec_plays(sec, r)) continue;
        if (set.piece.role[r] == MEL_YOU) {     /* your seat: an open lane */
            for (j = 0; j < 224u; j += 6u) cv_rect(8 + (int32_t)j, y + 3, 3, 1, c);
            continue;
        }
        if (p != &set.piece) { mel_plan_t hp; mel_plan(p, &hp); hp.push = mp.push; k = mel_bar(p, &hp, r, bar % hp.bars, 0, ev); ticks = mel_bar_ticks(&hp); }
        else { k = mel_bar(p, &mp, r, bar, 0, ev); ticks = mel_bar_ticks(&plan); }
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

static uint32_t load_show;                      /* the interrupt's longest block lately, as a part of a hundred */
static void draw_code(uint32_t alt)             /* alt: 1 EDIT is held, 2 the keys are the panel */
{
    char s[40], *e;
    cv_begin(240, 16, C_BLACK);
    if (note_on)
        cv_text((240 - text_w(&FONT_S, note_text)) / 2, 0, &FONT_S, note_text, M_ACC);
    else if (alt == 1u) {
        e = put(s, "keys ");
        e = put(e, MEL_LAYOUT_NAME[set.piece.layout]);
        cv_text(8, 0, &FONT_S, s, M_DIM);
        e = put(s, set.snap ? "snap  " : "");
        e = put(e, "load ");
        put(putn(e, load_show), "%");
        cv_text(232 - text_w(&FONT_S, s), 0, &FONT_S, s, M_DIM);
    } else if (alt == 2u) {
        cv_text(8, 0, &FONT_S, "PANEL", M_ACC);
        if (set.scene_at) putn(put(s, "scene "), set.scene_at); else put(s, "");
        cv_text(232 - text_w(&FONT_S, s), 0, &FONT_S, s, M_DIM);
    } else {
        mel_code_make(&set.piece, s);
        cv_text((240 - text_w(&FONT_S, s)) / 2, 0, &FONT_S, s, M_DIM);
    }
    cv_blit(0, 222);
}

static void text_2x(int32_t x, int32_t y, const char *s, uint16_t c)   /* the large face, twice the size */
{
    const felucca_font_t *f = &FONT_L;
    uint16_t ramp[16];
    uint32_t r = c >> 11, g = (c >> 5) & 63u, b = c & 31u, a;
    for (a = 0; a < 16u; a++)
        ramp[a] = (uint16_t)(((r * a / 15u) << 11) | ((g * a / 15u) << 5) | (b * a / 15u));
    for (; *s; s++) {
        uint32_t gi = glyph(f, (uint8_t)*s), gx, gy, w = f->bw[gi], bpr = (w + 1u) / 2u;
        const uint8_t *gd = f->data + f->off[gi];
        for (gy = 0; gy < f->h; gy++)
            for (gx = 0; gx < w; gx++) {
                uint32_t v = gd[gy * bpr + gx / 2u];
                v = (gx & 1u) ? (v & 15u) : (v >> 4);
                if (v) cv_rect(x + 2 * ((int32_t)gx - (int32_t)f->pad), y + 2 * (int32_t)gy, 2, 2, ramp[v]);
            }
        x += 2 * f->adv[gi];
    }
}

static void draw_card(void)                     /* the piece's code, to read across a room: SAVE held */
{
    const mel_piece_t *p = &set.piece;
    char s[MEL_CODE_TEXT], part[8], w[48], *e;
    uint32_t i, j;
    mel_code_make(p, s);
    cv_begin(240, 24, C_BLACK);
    cv_text((240 - text_w(&FONT_S, "THIS PIECE, TO SHARE")) / 2, 6, &FONT_S, "THIS PIECE, TO SHARE", M_DIM);
    cv_blit(0, 0);
    for (i = 0; i < 3u; i++) {
        for (j = 0; j < 6u; j++) part[j] = s[i * 7u + j];
        part[6] = 0;
        cv_begin(240, 64, C_BLACK);
        text_2x((240 - 2 * text_w(&FONT_L, part)) / 2, 0, part, i == 1u ? M_ACC : M_INK);
        cv_blit(0, 24 + i * 64u);
    }
    e = put(w, MEL_MOOD_NAME[p->mood]); e = put(e, " ");
    e = put(e, MEL_ENERGY_NAME[p->energy]); e = put(e, " ");
    put(e, MEL_STYLE_NAME[p->style]);
    cv_begin(240, 24, C_BLACK);
    cv_text((240 - text_w(&FONT_S, w)) / 2, 2, &FONT_S, w, M_DIM);
    cv_blit(0, 216);
}

static void splash(void)                        /* the maker's mark, then the instrument's plate, while it wakes */
{
    uint32_t i, y, t0;
    for (i = 0; i < MEL_SPLASHES; i++) {
        for (y = 0; y < 240u; y += 60u) {
            cv_begin(240, 60, C_BLACK);
            mel_splash_rows(i, y, 60);
            cv_blit(0, y);
        }
        for (t0 = fm1_ms; fm1_ms - t0 < 1500u; ) platform_poll();
    }
    lcd_fill(0, 0, 240, 240, C_BLACK);
}

/* ---- the panel: with SEQ held (or tapped, to stay) the keys choose, and do not play --------------------------
 * black keys: INTRO A B BREAK ENDING, then a new idea for the bass, chords, melody, pulse, then BACK and ROLL
 * white keys: eight scenes (HOME + key keeps the piece there), then the eight ways the chords are played */
static const uint8_t PANEL[27] = {30, 0, 31, 1, 32, 2, 33, 34, 3, 35, 4, 36, 37, 10, 40, 11, 41, 12, 42, 43, 13, 44, 20, 45, 46, 21, 47};
static mel_piece_t undo_piece;
static uint32_t undo_ok;
static void undo_keep(void) { undo_piece = set.piece; undo_ok = 1; }
static void sayn(const char *a, uint32_t n)
{
    putn(put(note_text, a), n);
    note_ms = fm1_ms;
    note_on = 1;
}
static void next_take(uint32_t r)               /* a part's next manner, or idea */
{
    mel_piece_t *p = &set.piece;
    p->take[r] = (uint8_t)((p->take[r] + 1u) % MEL_TAKES);
    if (r == MEL_BASS) say("BASS ", 0, MEL_BASS_NAME[p->take[r]]);
    else if (r == MEL_CHORDS) say("CHORDS ", 0, MEL_MANNER_NAME[p->take[r]]);
    else sayn(r == MEL_MELODY ? "MELODY IDEA " : "PULSE IDEA ", p->take[r] + 1u);
}
static uint32_t panel_key(uint32_t k)           /* -> the piece changed */
{
    mel_piece_t *p = &set.piece;
    uint32_t a = PANEL[k], i;
    if (a < 10u) {                              /* a section, from the next bar: a fill takes us there */
        if (a == SEC_END && !seq_on) return 0;
        if (a != sec_cur) fill_swap(1);
        sec_cur = a;
        say("SECTION ", 0, SEC_NAME[a]);
        return 1;
    }
    if (a < 20u) { undo_keep(); next_take(a - 10u); return 1; }
    if (a == 20u) {                             /* back: the piece as it was before the last change here (again: forward) */
        mel_piece_t x = *p;
        if (!undo_ok) { say("NOTHING TO GO BACK TO", 0, ""); return 0; }
        *p = undo_piece; undo_piece = x;
        say("BACK", 0, "");
        return 1;
    }
    if (a == 21u) {                             /* roll: another piece from the same words. Its number is shown: nothing is lost */
        undo_keep();
        p->variation = (uint16_t)(mel_noise(fm1_ms, p->variation, seq_abs, k, 7) % MEL_VARIATIONS);
        sayn("ROLLED: VARIATION ", p->variation);
        return 1;
    }
    if (a < 40u) {                              /* a scene */
        uint8_t layout = p->layout, follow = p->follow;
        i = a - 30u;
        if (fm1_in.buttons >> BTN_HOME & 1u) {
            set.scene[i] = *p;
            set.scenes |= (uint8_t)(1u << i);
            set.scene_at = (uint8_t)(i + 1u);
            dirty = 1; t_change = fm1_ms;
            sayn("KEPT AS SCENE ", i + 1u);
            return 0;
        }
        if (!(set.scenes >> i & 1u)) { say("EMPTY: HOME + KEY KEEPS ONE", 0, ""); return 0; }
        undo_keep();
        fill_swap(1);
        *p = set.scene[i];
        p->layout = layout; p->follow = follow; /* (how you play is yours, not the scene's) */
        set.scene_at = (uint8_t)(i + 1u);
        sayn("SCENE ", i + 1u);
        return 1;
    }
    undo_keep();
    p->take[MEL_CHORDS] = (uint8_t)(a - 40u);
    say("CHORDS ", 0, MEL_MANNER_NAME[a - 40u]);
    return 1;
}

static void keep(void)                          /* write it all down (an erase stalls the sound for a moment) */
{
    settings_save(&set, sizeof set);
    if (phrase_dirty) blob_save(0, &phrase, sizeof phrase);
    dirty = phrase_dirty = 0;
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
    uint32_t prev = 0, k, r, hot = 0, hot_ms = 0, held_back = 0, panel_latch = 0;
    uint32_t words_drawn = ~0u, bar_drawn = ~0u, code_drawn = ~0u;
    uint32_t seat_ms[MEL_ROLES] = {0}, seat_used = 0, seq_ms = 0, seq_used = 0, arp_ms = 0, rec_ms = 0, rec_wait = 0;
    uint32_t save_ms = 0, save_down = 0, card = 0, load_ms = 0;

    if (settings_load(&set, sizeof set) || !mel_valid(&set.piece) || set.oct < -2 || set.oct > 2) {
        memset(&set, 0, sizeof set);
        mel_default(&set.piece);
        set.snap = 1;
    }
    if (set.level < -6 || set.level > 6) set.level = 0;
    if (set.preset >= MEL_PRESETS) set.preset = 0;
    if (set.scene_at > 8u) set.scene_at = 0;
    for (k = 0; k < 8u; k++)
        if (!mel_valid(&set.scene[k])) set.scenes &= (uint8_t)~(1u << k);
    if (blob_load(0, &phrase, sizeof phrase) || phrase.n > PHRASE_MAX) phrase.n = 0;
    pass_mark = phrase.n;
    out_gain = OUT_GAIN[set.level + 6];
    for (r = 0; r < MEL_ROLES; r++)
        if (!mel_valid(&set.held[r])) { set.held[r] = set.piece; if (set.piece.role[r] == MEL_HELD) set.piece.role[r] = MEL_ENGINE; }
    mel_plan(&set.piece, &plan);
    splash();

    for (;;) {
        uint32_t keys, down, up, rel, b, changed = 0, redo = 0, alt, st, bar, btn, home, seqd, panel, mode;
        mel_piece_t *p = &set.piece;
        int32_t e;
        platform_poll();
        btn = fm1_in.buttons;
        alt = btn >> BTN_EDIT & 1u;
        home = btn >> BTN_HOME & 1u;
        seqd = btn >> BTN_SEQ & 1u;

        /* the dials */
        if ((e = fm1_enc_take(ENC_KNOB1)) != 0 && (alt ? turn(&p->colour, e, MEL_COLOURS, 0) : turn(&p->mood, e, MEL_MOODS, 0))) changed = hot = 1;
        if ((e = fm1_enc_take(ENC_KNOB2)) != 0 && (alt ? turn(&p->boost, e, MEL_MAX_BOOST + 1u, 0) : turn(&p->energy, e, MEL_ENERGIES, 0))) {
            changed = 1, hot = 2;
            if (alt && p->boost == MEL_MAX_BOOST) say("THROUGH THE GATE", 0, "");
        }
        if ((e = fm1_enc_take(ENC_KNOB3)) != 0 && (alt ? turn(&p->feel, e, MEL_FEELS, 0) : turn(&p->style, e, MEL_STYLES, 0))) changed = 1, hot = 3;
        if ((e = fm1_enc_take(ENC_KNOB4)) != 0 && (alt ? turn(&p->length, e, MEL_LENGTHS, 0) : turn(&p->shape, e, MEL_SHAPES, 0))) changed = 1, hot = 4;
        if ((e = fm1_enc_take(ENC_SELECT)) != 0 && (alt ? turn(&p->layout, e, MEL_LAYOUTS, 0) : turn(&p->key, e, MEL_KEYS, 1))) changed = 1, hot = 5;
        if ((e = fm1_enc_take(ENC_ALGORITHM)) != 0) {
            if (alt) {                          /* the tempo, within what the energy (or the style) allows; all the way down: the engine's own */
                if (turn(&p->trim, e, MEL_TRIMS, 0)) {
                    mel_plan(p, &plan);
                    if (p->trim) sayn("TEMPO ", plan.tempo); else say("TEMPO: THE ENGINE'S OWN", 0, "");
                    changed = 1;
                }
            } else {
                int32_t v = ((int32_t)p->variation + e) % (int32_t)MEL_VARIATIONS;
                p->variation = (uint16_t)(v < 0 ? v + (int32_t)MEL_VARIATIONS : v);
                changed = 1, hot = 6;
            }
        }
        e = fm1_enc_take(ENC_PRESETS);          /* the shipped pieces; with EDIT held: the output level */
        if (e && !alt) {
            int32_t i = ((int32_t)set.preset + e) % (int32_t)MEL_PRESETS;
            uint8_t layout = p->layout, follow = p->follow;
            undo_keep();
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

        /* the buttons. A role's: a tap takes the part out, or brings it back, at the next bar; held, the seat is yours */
        b = fm1_input_edges(&rel);
        for (r = 0; r < MEL_ROLES; r++) {
            uint32_t bit = 1u << ROLE_BTN[r];
            if (b & bit) {
                seat_ms[r] = fm1_ms;
                seat_used |= 1u << r;
                if (btn >> BTN_GLO & 1u) {      /* GLO + a role: its next sound */
                    if (r != MEL_PULSE) {
                        p->sound[r] = (uint8_t)((p->sound[r] + 1u) % SYN_SOUNDS);
                        putn(put(put(note_text, MEL_ROLE_NAME[r]), " sound "), p->sound[r] + 1u);
                        note_ms = fm1_ms;
                        note_on = 1;
                        changed = 1;
                    }
                } else if (home) {              /* HOME + a role: hold it as it is, or let it go */
                    if (p->role[r] == MEL_HELD) p->role[r] = MEL_ENGINE;
                    else if (p->role[r] == MEL_ENGINE) { set.held[r] = *p; p->role[r] = MEL_HELD; }
                    changed = 1;
                } else if (seqd) {              /* SEQ + a role: its next manner or idea */
                    undo_keep();
                    next_take(r);
                    seq_used = 1;
                    changed = 1;
                } else if (!alt) seat_used &= ~(1u << r);   /* (EDIT + a role: it alone, while held: below) */
            }
            if ((btn & bit) && !(seat_used >> r & 1u) && fm1_ms - seat_ms[r] >= 450u) {
                for (k = 0; k < MEL_ROLES; k++) /* one seat: the part you left goes back to the engine */
                    if (p->role[k] == MEL_YOU) p->role[k] = MEL_ENGINE;
                p->role[r] = MEL_YOU;
                seat_used |= 1u << r;
                say("YOUR SEAT: ", 0, MEL_ROLE_NAME[r]);
                changed = 1;
            }
            if ((rel & bit) && !(seat_used >> r & 1u)) {
                uint32_t s = p->role[r];
                seat_used |= 1u << r;
                if (s == MEL_OFF) p->role[r] = held_back >> r & 1u ? MEL_HELD : MEL_ENGINE;
                else if (s == MEL_YOU) p->role[r] = MEL_ENGINE;
                else { held_back = (held_back & ~(1u << r)) | (uint32_t)(s == MEL_HELD) << r; p->role[r] = MEL_OFF; }
                changed = 1;
            }
        }
        seq_solo = alt ? (btn >> BTN_FX & 1u) | (btn >> BTN_SEL & 1u) << 1 | (btn >> BTN_ENV & 1u) << 2 | (btn >> BTN_LFO & 1u) << 3 : 0u;

        if (b & (1u << BTN_SEQ)) { seq_ms = fm1_ms; seq_used = 0; }
        if ((rel & (1u << BTN_SEQ)) && !seq_used && fm1_ms - seq_ms < 400u) panel_latch = !panel_latch;
        panel = panel_latch || seqd;

        if (b & (1u << BTN_ARP)) { fill_swap(1); arp_ms = fm1_ms; }   /* a fill now; held: a build, bar after bar */
        k = (btn >> BTN_ARP & 1u) && fm1_ms - arp_ms > 250u;
        if (k != fill_hold) { fill_hold = k; redo = 1; }

        if (b & (1u << BTN_PLAY)) { if (seq_on) { seq_halt(); rec_on = 0; } else seq_start(); bar_drawn = ~0u; }
        if (seq_ended) { seq_ended = 0; rec_on = 0; sec_cur = SEC_A; words_drawn = bar_drawn = ~0u; }
        if (b & (1u << BTN_REC)) {
            if (home) { rec_ms = fm1_ms; rec_wait = 1; }
            else if (alt) {
                set.snap = !set.snap;
                say(set.snap ? "REC SNAPS TO THE BEAT" : "REC KEEPS YOUR TIMING", 0, "");
                dirty = 1; t_change = fm1_ms;
            } else {
                rec_on = !rec_on;
                if (rec_on) pass_mark = phrase.n;
                if (rec_on && !seq_on) seq_start();   /* REC from rest starts the band */
                say(rec_on ? "RECORDING: PLAY" : "KEPT ", rec_on ? 0u : phrase.n, rec_on ? "" : "NOTES");
            }
            bar_drawn = ~0u;
        }
        if (rec_wait && (rel & (1u << BTN_REC))) {     /* HOME + REC, let go: the last pass is undone */
            rec_wait = 0;
            if (phrase.n > pass_mark) {
                phrase.n = (uint16_t)pass_mark;
                for (k = 0; k < 27u; k++) held_rec[k][0] = held_rec[k][1] = held_rec[k][2] = 0;
                phrase_dirty = phrase_new = 1;
                say("LAST PASS UNDONE", 0, "");
            } else say("HOLD TO CLEAR THE PHRASE", 0, "");
        } else if (rec_wait && fm1_ms - rec_ms > 1200u) {   /* held: all of it */
            rec_wait = 0;
            phrase_clear();
            say("PHRASE CLEARED", 0, "");
        }
        if (b & (1u << BTN_SAVE)) { save_ms = fm1_ms; save_down = 1; }
        if (save_down && !card && (btn >> BTN_SAVE & 1u) && fm1_ms - save_ms > 400u) { card = 1; draw_card(); }
        if (save_down && (rel & (1u << BTN_SAVE))) {
            save_down = 0;
            if (card) {
                card = 0;
                lcd_fill(0, 0, 240, 240, C_BLACK);
                words_drawn = bar_drawn = code_drawn = ~0u;
            } else { keep(); say("SAVED", 0, ""); }
        }
        if (b & (1u << BTN_OCT_DOWN) && set.oct > -2) { set.oct--; dirty = 1; t_change = fm1_ms; }
        if (b & (1u << BTN_OCT_UP) && set.oct < 2) { set.oct++; dirty = 1; t_change = fm1_ms; }

        /* your keys: notes, or with the panel up, choices */
        keys = fm1_in.notes;
        down = keys & ~prev;
        up = prev & ~keys;
        prev = keys;
        for (k = 0; k < 27u; k++) {
            if (up >> k & 1u) key_up(k);
            if (!(down >> k & 1u)) continue;
            if (!panel) key_down(k, 100);
            else { seq_used = 1; if (panel_key(k)) changed = 1; words_drawn = code_drawn = ~0u; }
        }
        if (pulse_ask) {                        /* (a drum is a strike, not a held voice: it goes in beside the notes) */
            uint32_t a = pulse_ask;
            pulse_ask = 0;
            fm1_irq_off();
            syn_drum(a >> 8, a & 0x7Fu);
            fm1_irq_on();
        }
        midi_in();

        if (changed) {
            mel_plan(p, &plan);
            dirty = 1; t_change = hot_ms = fm1_ms;
            words_drawn = bar_drawn = code_drawn = ~0u;
        }
        seq_service(changed || phrase_new || redo);
        if (phrase_new) { phrase_new = 0; bar_drawn = ~0u; }

        /* the lights. Keys: what fits glows, the home note is lit, what you hold is lit; with the panel up, what is chosen */
        bar = seq_on ? seq_bar : 0u;
        for (k = 0; k < 27u; k++) {
            uint32_t note = mel_key_note(p, &plan, seat(), k, set.oct), in = note < 128u && mel_in_scale(&plan, (int32_t)note);
            uint32_t on = in && note % 12u == plan.tonic, glow = in && !on;
            if (panel) {
                uint32_t a = PANEL[k];
                on = a < 10u ? a == sec_cur : a < 30u ? 0u : a < 40u ? set.scene_at == a - 29u : p->take[MEL_CHORDS] == a - 40u;
                glow = !on && (a < 30u || a >= 40u || (set.scenes >> (a - 30u) & 1u));
            }
            fm1_led_key(KEY_LED(k), (keys >> k & 1u) || on);
            fm1_led_key_glow(KEY_LED(k), glow);
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
        fm1_led_key(BTN_SEQ, panel);
        fm1_led_key(BTN_ARP, fill_hold);
        fm1_led_key(BTN_REC, rec_on);           /* lit: it is listening; glowing: there is a phrase */
        fm1_led_key_glow(BTN_REC, !rec_on && phrase.n);
        fm1_led_key_glow(BTN_SAVE, dirty || phrase_dirty);   /* glowing: something is not yet kept */

        /* the screen: only what changed */
        if (fm1_ms - load_ms > 2000u) { load_show = load_peak * 100u / LOAD_BLOCK; load_peak = 0; load_ms = fm1_ms; }
        if (hot && fm1_ms - hot_ms > 1200u) { hot = 0; words_drawn = ~0u; }
        mode = alt ? 1u : panel ? 2u : 0u;
        idle_ok = 1;
        if (!card) {
            if ((st = mode << 8 | hot | sec_cur << 10) != words_drawn) { words_drawn = st; draw_words(mode, hot); }
            if ((st = bar | seq_on << 8 | alt << 9 | sec_cur << 10) != bar_drawn) { bar_drawn = st; draw_bars(bar, seq_on); draw_facts(bar, alt); }
            if (note_on && fm1_ms - note_ms > 2500u) note_on = 0;
            if ((st = (note_ms << 14) ^ (mode | note_on << 2 | (uint32_t)set.scene_at << 3 | (alt ? load_show : 0u) << 7)) != code_drawn) { code_drawn = st; draw_code(mode); }
        }
        idle_ok = 0;

        /* keep it: two seconds after the last change, when nothing sounds (an erase stalls the sound); or SAVE, at once */
        if ((dirty || phrase_dirty) && !seq_on && syn_quiet() && fm1_ms - t_change > 2000u) keep();
    }
}
