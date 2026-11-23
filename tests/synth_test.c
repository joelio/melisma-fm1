/* SPDX-License-Identifier: GPL-3.0-only */
/* Melisma's synth, measured (nobody here can hear it): every instrument is played and checked for its tuning, its
 * loudness, its peaks, that it ends without a click and goes silent, and that the room is stable.
 *   cc -O2 -Ifirmware/src -o build/host/synth_test tests/synth_test.c -lm && build/host/synth_test
 *   build/host/synth_test wav DIR      also writes DIR/NN-NAME.wav, a phrase on each instrument, to listen to */
#define SYN_HOST
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mel_synth.h"

#define FS 44100
static unsigned checks, fails;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int32_t buf[2 * 44100 * 12];
static uint32_t at;
static void run(uint32_t frames)
{
    while (frames) {
        uint32_t n = frames > 256u ? 256u : frames;
        if (at + n > sizeof buf / sizeof buf[0] / 2u) return;
        syn_render(buf + 2u * at, n);
        at += n; frames -= n;
    }
}
static void reset(void)
{
    memset(syn_v, 0, sizeof syn_v);
    memset(buf, 0, sizeof buf);
    memset(syn_cho, 0, sizeof syn_cho);
    memset(syn_rev_mem, 0, sizeof syn_rev_mem);
    rev_room = 99;
    syn_rng = 0x1234ABCDu; syn_ages = 0; syn_cho_ps = syn_cho_pf = syn_cho_c = syn_cho_w = 0; rev_lfo = 0;
    syn_cho_d[0] = syn_cho_d[1] = syn_cho_d[2] = 176 << 16;
    at = 0;
}
static double rms(uint32_t a, uint32_t b)
{
    double s = 0;
    uint32_t i;
    for (i = 2u * a; i < 2u * b; i++) s += (double)buf[i] * buf[i];
    return sqrt(s / (2.0 * (b - a) + 1e-9));
}
static int32_t peak(uint32_t a, uint32_t b)
{
    int32_t p = 0;
    uint32_t i;
    for (i = 2u * a; i < 2u * b; i++) { int32_t x = buf[i] < 0 ? -buf[i] : buf[i]; if (x > p) p = x; }
    return p;
}
static double pitch_hz(uint32_t a, uint32_t b, double guess)   /* by autocorrelation near the pitch expected */
{
    double best = -1e30, lag0 = FS / guess, bl = lag0;
    int l, lo = (int)(lag0 * 0.94), hi = (int)(lag0 * 1.06) + 1;
    double c[3] = {0, 0, 0};
    int li = 0;
    for (l = lo; l <= hi; l++) {
        double s = 0;
        uint32_t i;
        for (i = a; i + (uint32_t)l < b; i++) s += (double)(buf[2u * i] + buf[2u * i + 1u]) * (buf[2u * (i + (uint32_t)l)] + buf[2u * (i + (uint32_t)l) + 1u]);
        s /= (double)(b - a - (uint32_t)l);
        if (s > best) { best = s; bl = l; li = l; }
    }
    for (l = li - 1; l <= li + 1; l++) {        /* a parabola through the peak: between the frames */
        double s = 0;
        uint32_t i;
        for (i = a; i + (uint32_t)l < b; i++) s += (double)(buf[2u * i] + buf[2u * i + 1u]) * (buf[2u * (i + (uint32_t)l)] + buf[2u * (i + (uint32_t)l) + 1u]);
        c[l - li + 1] = s / (double)(b - a - (uint32_t)l);
    }
    if (c[0] - 2 * c[1] + c[2] != 0) bl = li + 0.5 * (c[0] - c[2]) / (c[0] - 2 * c[1] + c[2]);
    return FS / bl;
}
static void wav(const char *path, uint32_t frames)
{
    FILE *f = fopen(path, "wb");
    uint32_t i, n = frames * 4u, v;
    uint16_t h;
    if (!f) return;
    fwrite("RIFF", 1, 4, f); v = 36u + n; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); h = 1; fwrite(&h, 2, 1, f); h = 2; fwrite(&h, 2, 1, f);
    v = FS; fwrite(&v, 4, 1, f); v = FS * 4; fwrite(&v, 4, 1, f); h = 4; fwrite(&h, 2, 1, f); h = 16; fwrite(&h, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f);
    for (i = 0; i < 2u * frames; i++) {         /* as the app's output stage would: its default gain, a ceiling */
        int32_t x = buf[i] * 643 >> 8;
        int16_t s = (int16_t)(x > 32767 ? 32767 : x < -32767 ? -32767 : x);
        fwrite(&s, 2, 1, f);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    static const uint8_t ROLE_NOTE[3] = {40, 60, 72};
    uint32_t id, i;
    const char *dir = argc > 2 && !strcmp(argv[1], "wav") ? argv[2] : 0;
    printf("%-16s %-6s %7s %7s %7s %8s\n", "instrument", "kind", "cents", "rms", "peak", "tail");
    for (id = 0; id < SYN_INSTS; id++) {
        const syn_inst_t *in = &SYN_INST[id];
        uint32_t role = in->home, note = ROLE_NOTE[role];
        double f0 = 440.0 * pow(2.0, ((double)note - 69.0) / 12.0), hz, cents, r, tail;
        int32_t pk, jump = 0;
        /* one held note, dry: is it in tune, how loud is it, does it stop cleanly */
        reset();
        syn_room = 0;
        syn_note_on(role, note, 100, SYN_HELD, 64, 1, id);
        run(FS);                                /* one second held */
        if (in->eng == SYN_FM && in->idx_keep) f0 /= in->rd;   /* (a modulator below the note: that is the period; one that dies away leaves the note) */
        hz = in->vib ? pitch_hz(FS / 6, FS / 6 + 5600, f0)     /* before any vibrato; else once it has settled */
           : in->sustain ? pitch_hz(FS / 2, FS / 2 + 17640, f0) : pitch_hz(FS / 3, FS / 3 + 8820, f0);
        cents = 1200.0 * log2(hz / f0);
        r = rms(FS / 10, FS);
        pk = peak(0, FS);
        syn_note_off(role, note);
        run(FS * 3);
        tail = rms(at - 2205, at);
        for (i = 2; i < 2u * at; i++) { int32_t d = buf[i] - buf[i - 2]; if (d < 0) d = -d; if (i > 2u * FS && d > jump) jump = d; }
        printf("%-16s %-6s %+7.1f %7.0f %7d %8.1f\n", in->name, in->eng == SYN_FM ? "fm" : "analog", cents, r, pk, tail);
        CHECK(fabs(cents) < 12.0, "%s is %.1f cents out", in->name, cents);
        CHECK(r > (in->sustain ? 150.0 : 60.0), "%s is nearly silent (rms %.0f)", in->name, r);
        CHECK(pk < 30000, "%s peaks at %d on one note", in->name, pk);
        CHECK(tail < 2.0, "%s does not go silent (%.1f after three seconds)", in->name, tail);
        CHECK(jump < 3000, "%s clicks when it is let go (a step of %d)", in->name, jump);
        CHECK(syn_quiet(), "%s leaves a voice running", in->name);
        /* every note of its part's range, fast: nothing blows up */
        reset();
        syn_room = 3;
        for (i = 0; i < 48u; i++) {
            syn_note_on(role, 28u + role * 12u + i, 127, 3000, 127, 0, id);
            run(2000);
        }
        run(FS);
        CHECK(peak(0, at) < 60000, "%s peaks at %d across its range", in->name, peak(0, at));
        if (dir) {                              /* a phrase to listen to: a scale, then a chord */
            static const uint8_t TUNE[8] = {0, 2, 4, 7, 9, 7, 4, 2};
            char path[256], name[32];
            reset();
            syn_room = 2;
            for (i = 0; i < 8u; i++) { syn_note_on(role, note + TUNE[i], 96, 9000, 70, 1, id); run(11025); }
            for (i = 0; i < 3u && role; i++) syn_note_on(role, note + i * 4u - (i == 2u ? 1u : 0u), 90, 40000, 70, 1, id);
            if (!role) syn_note_on(role, note, 110, 40000, 70, 1, id);
            run(FS * 2);
            for (i = 0; in->name[i] && i < 31u; i++) name[i] = in->name[i] == ' ' ? '-' : in->name[i];
            name[i] = 0;
            snprintf(path, sizeof path, "%s/%02u-%s.wav", dir, id, name);
            wav(path, at);
        }
    }
    {   /* the room: how much comes back while the band plays, and that it dies away at every size */
        uint32_t room;
        static int32_t dry[2 * FS * 2];
        for (room = 0; room <= 4u; room++) {
            double wet = 0, late, d;
            reset();
            syn_room = room;
            for (i = 0; i < 6u; i++) syn_note_on(1, 55u + i * 3u, 110, 80000, 90, 0, 6);
            run(FS * 2);
            if (!room) { memcpy(dry, buf, sizeof dry); continue; }
            for (i = 2u * FS; i < 4u * FS; i++) wet += (double)(buf[i] - dry[i]) * (buf[i] - dry[i]);
            wet = sqrt(wet / (2.0 * FS));
            for (i = 2u * FS, d = 0; i < 4u * FS; i++) d += (double)dry[i] * dry[i];
            d = sqrt(d / (2.0 * FS));
            run(FS * 7);
            late = rms(at - FS / 2, at);
            printf("room %u: the room is %.1f dB under the band; %.2f left seven seconds after\n", room, 20.0 * log10(d / wet), late);
            CHECK(wet > d / 20.0, "room %u gives nothing back", room);
            CHECK(wet < d, "room %u is louder than the band", room);
            CHECK(late < 4.0, "room %u is still ringing (%.1f)", room, late);
            CHECK(peak(0, at) < 60000, "room %u overloads", room);
        }
        syn_room = 2;
    }
    printf("%u checks, %u failed\n", checks, fails);
    return fails ? 1 : 0;
}
