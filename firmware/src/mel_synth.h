/* SPDX-License-Identifier: GPL-3.0-only */
/* Melisma's sound: a voice for each role. Bass, chords and melody are two-operator FM (a sine bent by a sine);
 * the pulse is a thump, a snap and a tick. Integers only. Everything here runs in the audio interrupt, and the
 * interrupt is the only one who touches a voice: the main loop asks through melisma.c's queue.
 *
 *   syn_note_on(role, note, vel, hold, bright, attack, sound)   hold: frames, or SYN_HELD until syn_note_off
 *   syn_drum(note, vel)
 *   syn_render(out, n)                                         adds n stereo frames
 */
#ifndef MEL_SYNTH_H
#define MEL_SYNTH_H
#include <stdint.h>

#define SYN_VOICES 12u
#define SYN_HELD 0xFFFFFFFFu
#define SYN_CTL 32u                             /* envelopes move once in this many frames */

typedef struct {                                /* a sound: how the modulator sits, how the note is shaped */
    uint8_t rn, rd;                             /* modulator : carrier frequency */
    uint16_t idx;                               /* how far the modulator bends the sine at its brightest */
    uint8_t idx_keep;                           /* how much of that stays after the attack, /16 */
    uint8_t idx_shift, dec_shift, rel_shift;    /* speeds: a bigger shift is slower */
    uint8_t sustain;                            /* the level a held note settles at, /16 of its peak */
    uint16_t level;                             /* peak, Q15, at full velocity */
} syn_sound_t;

#define SYN_SOUNDS 5u
static const syn_sound_t SYN_SOUND[3][SYN_SOUNDS] = {
    {{1, 1, 150, 4, 5, 7, 3, 11, 13500},        /* bass: round */
     {2, 1, 220, 3, 4, 6, 3, 9, 12300},         /*       with an edge */
     {1, 2, 120, 8, 6, 7, 3, 12, 13500},        /*       with a sub under it */
     {3, 1, 260, 2, 3, 6, 3, 8, 11500},         /*       plucked */
     {1, 1, 60, 10, 7, 8, 3, 13, 13500}},       /*       soft: nearly a sine */
    {{1, 1, 130, 2, 5, 7, 5, 8, 5600},          /* chords: an electric piano */
     {2, 1, 90, 6, 7, 9, 6, 12, 4700},          /*         an organ-like pad */
     {3, 1, 160, 1, 4, 6, 4, 5, 5600},          /*         a pluck */
     {1, 1, 40, 12, 8, 9, 7, 13, 5600},         /*         a soft pad */
     {4, 1, 110, 2, 4, 6, 5, 6, 5000}},         /*         glass */
    {{2, 1, 140, 6, 6, 8, 4, 11, 8000},         /* melody: a lead */
     {1, 1, 200, 5, 6, 8, 4, 10, 8000},         /*         a reed */
     {3, 1, 110, 3, 5, 7, 5, 8, 8400},          /*         a bell */
     {1, 1, 70, 10, 7, 9, 5, 12, 8400},         /*         a flute */
     {2, 1, 260, 4, 5, 7, 4, 9, 7400}}};        /*         brass */
static const uint8_t SYN_POOL[4] = {0, 2, 8, 12};   /* voices 0-1 bass, 2-7 chords, 8-11 melody */
static const uint8_t SYN_PAN[3][2] = {{128, 128}, {150, 104}, {108, 148}};   /* left, right: /128 */
static const uint16_t SYN_ATTACK_FRAMES[4] = {110, 330, 880, 2000};          /* 2.5, 7.5, 20, 45 ms */

typedef struct {
    uint32_t ph, phm, inc, incm, hold, age;
    int32_t amp, peak, sus, idx, idx_sus, att;
    uint8_t stage, role, note, ctl, idx_shift, dec_shift, rel_shift;
} syn_voice_t;
enum { SYN_OFF, SYN_ATTACK, SYN_ON, SYN_RELEASE };
static syn_voice_t syn_v[SYN_VOICES];
static uint32_t syn_ages;

static struct { uint32_t ph, inc; int32_t amp; } syn_thump;
static struct { uint32_t ph; int32_t amp, namp, last; } syn_snap;
static struct { int32_t amp, last; } syn_tick;
static uint32_t syn_rng = 0x1234ABCDu, syn_ctl;

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

static void syn_note_on(uint32_t role, uint32_t note, uint32_t vel, uint32_t hold, uint32_t bright, uint32_t attack, uint32_t sound)
{
    const syn_sound_t *s;
    syn_voice_t *v;
    uint32_t i, pick = SYN_POOL[role], best = 0xFFFFFFFFu, frames;
    if (role > 2u || note > 127u || !vel) return;
    s = &SYN_SOUND[role][sound % SYN_SOUNDS];
    for (i = SYN_POOL[role]; i < SYN_POOL[role + 1u]; i++) {   /* the same note, else a free voice, else the oldest */
        uint32_t score = syn_v[i].note == note && syn_v[i].stage ? 0u
                       : syn_v[i].stage == SYN_OFF ? 1u : syn_v[i].stage == SYN_RELEASE ? 2u + (syn_v[i].age & 0xFFFFFFu)
                       : 0x80000000u + (syn_v[i].age & 0xFFFFFFu);
        if (score < best) { best = score; pick = i; }
    }
    v = &syn_v[pick];
    if (v->stage == SYN_OFF) v->ph = v->phm = 0, v->amp = 0;   /* (a voice taken over keeps its phase: no click) */
    v->role = (uint8_t)role;
    v->note = (uint8_t)note;
    v->age = ++syn_ages;
    v->inc = syn_inc(note);
    v->incm = v->inc / s->rd * s->rn;
    v->peak = (int32_t)((uint32_t)s->level * (vel * vel + vel * 127u) >> 15);   /* between a straight line and a square: soft is still heard */
    if (v->peak < 64) v->peak = 64;
    v->sus = v->peak * s->sustain >> 4;
    v->idx = (int32_t)((uint32_t)s->idx * (40u + bright + vel / 2u) / 200u);   /* louder and more tense is brighter */
    v->idx_sus = v->idx * s->idx_keep >> 4;
    frames = SYN_ATTACK_FRAMES[role == 0u && attack > 1u ? 1u : attack & 3u];   /* (a bass always speaks promptly) */
    v->att = v->peak / (int32_t)frames + 1;
    v->idx_shift = s->idx_shift; v->dec_shift = s->dec_shift; v->rel_shift = s->rel_shift;
    v->hold = hold;
    v->ctl = SYN_CTL;
    v->stage = SYN_ATTACK;
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
    if (note == 36u) { syn_thump.ph = 0; syn_thump.inc = 15582000u; syn_thump.amp = a * 3 / 4; }   /* from 160 Hz down */
    else if (note == 38u) { syn_snap.ph = 0; syn_snap.amp = a / 3; syn_snap.namp = a / 2; }
    else syn_tick.amp = a * 2 / 5;
}

static int syn_quiet(void)
{
    uint32_t i;
    for (i = 0; i < SYN_VOICES; i++)
        if (syn_v[i].stage) return 0;
    return syn_thump.amp < 16 && syn_snap.amp < 16 && syn_snap.namp < 16 && syn_tick.amp < 16;
}

static void syn_render(int32_t *out, uint32_t n)
{
    uint32_t i, k;
    for (k = 0; k < SYN_VOICES; k++) {
        syn_voice_t *v = &syn_v[k];
        uint32_t ph = v->ph, phm = v->phm, ctl = v->ctl;
        int32_t amp = v->amp, idx = v->idx, pl = SYN_PAN[v->role][0], pr = SYN_PAN[v->role][1];
        if (v->stage == SYN_OFF) continue;      /* silent: costs nothing */
        if (v->role == 1u && (v->note & 1u)) { int32_t t = pl; pl = pr; pr = t; }   /* a chord spreads across the two sides */
        for (i = 0; i < n; i++) {
            int32_t m, s;
            if (v->stage == SYN_ATTACK) {
                amp += v->att;
                if (amp >= v->peak) { amp = v->peak; v->stage = SYN_ON; }
            }
            if (!--ctl) {                       /* the slow things */
                ctl = SYN_CTL;
                idx -= (idx - v->idx_sus) >> v->idx_shift;
                if (v->stage == SYN_ON) {
                    amp -= (amp - v->sus) >> v->dec_shift;
                    if (v->hold != SYN_HELD) {
                        if (v->hold <= SYN_CTL) v->stage = SYN_RELEASE;
                        else v->hold -= SYN_CTL;
                    }
                } else if (v->stage == SYN_RELEASE) {
                    amp -= (amp >> v->rel_shift) + 8;
                    if (amp <= 0) { amp = 0; v->stage = SYN_OFF; break; }
                } else if (v->hold != SYN_HELD)
                    v->hold = v->hold > SYN_CTL ? v->hold - SYN_CTL : 1u;
            }
            m = syn_sine(phm);
            phm += v->incm;
            s = (syn_sine(ph + ((uint32_t)(m * idx) << 8)) * amp) >> 15;
            ph += v->inc;
            out[2u * i] += (s * pl) >> 7;
            out[2u * i + 1u] += (s * pr) >> 7;
        }
        v->ph = ph; v->phm = phm; v->amp = amp; v->idx = idx; v->ctl = (uint8_t)ctl;
    }
    if (syn_thump.amp >= 16 || syn_snap.amp >= 16 || syn_snap.namp >= 16 || syn_tick.amp >= 16) {
        uint32_t rng = syn_rng, c = syn_ctl;
        for (i = 0; i < n; i++) {
            int32_t s = 0, noise;
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            noise = (int32_t)(rng & 0xFFFFu) - 32768;
            if (!(++c & 15u)) {                 /* every 16 frames: the drums die away, the thump falls */
                syn_thump.amp -= (syn_thump.amp >> 7) + 1;
                if (syn_thump.inc > 5000000u) syn_thump.inc -= syn_thump.inc >> 6;
                syn_snap.amp -= (syn_snap.amp >> 5) + 1;
                syn_snap.namp -= (syn_snap.namp >> 6) + 1;
                syn_tick.amp -= (syn_tick.amp >> 4) + 1;
                if (syn_thump.amp < 0) syn_thump.amp = 0;
                if (syn_snap.amp < 0) syn_snap.amp = 0;
                if (syn_snap.namp < 0) syn_snap.namp = 0;
                if (syn_tick.amp < 0) syn_tick.amp = 0;
            }
            if (syn_thump.amp) { syn_thump.ph += syn_thump.inc; s += (syn_sine(syn_thump.ph) * syn_thump.amp) >> 15; }
            if (syn_snap.amp | syn_snap.namp) {
                syn_snap.ph += 18500000u;       /* 190 Hz */
                s += (syn_sine(syn_snap.ph) * syn_snap.amp) >> 15;
                s += ((noise - syn_snap.last) * (syn_snap.namp >> 2)) >> 15;   /* the noise, its lows taken out */
                syn_snap.last = noise;
            }
            if (syn_tick.amp) {
                s += ((noise - syn_tick.last) * (syn_tick.amp >> 2)) >> 15;
                syn_tick.last = noise;
            }
            out[2u * i] += s;
            out[2u * i + 1u] += s;
        }
        syn_rng = rng; syn_ctl = c;
    }
}
#endif
