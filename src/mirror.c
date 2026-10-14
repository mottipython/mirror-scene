/*
 * mirror-scene: a cartoon screensaver of the mirror gag.
 *
 * A man in a nightshirt creeps up to a mirror whose glass is gone, and the man on
 * the other side copies everything he does, almost. Then a third man in the same
 * disguise walks through the frame.
 *
 *   mirror-scene            full screen on every display; any key or mouse movement quits
 *   mirror-scene --window   in a window; Esc or Q quits
 *   MirrorScene.scr /s      Windows screensaver conventions
 */
#include <SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* The pictures (drawn with Gemini, prepared by tools/make_sprites.py) are built
 * into the program as PNG and JPEG files and decoded at start-up. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_THREAD_LOCALS /* else Windows builds need libwinpthread-1.dll */
#include "stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if defined(__APPLE__)
/* Mach-O: its own section names, and C names carry a leading underscore */
#define ART_SECTION ".section __DATA,__const\n"
#define ART_SYM "_art_"
#elif defined(_WIN32)
#define ART_SECTION ".section .rdata,\"dr\"\n"
#define ART_SYM "art_"
#else
#define ART_SECTION ".section .rodata\n"
#define ART_SYM "art_"
#endif
#define ART(name, file) \
    __asm__(ART_SECTION ".globl " ART_SYM #name "\n" ART_SYM #name ":\n.incbin \"assets/art/" file "\"\n" \
            ".globl " ART_SYM #name "_end\n" ART_SYM #name "_end:\n.text\n"); \
    extern const unsigned char art_##name[], art_##name##_end[];
/* Every picture, by name and file. The figures are all drawn to one scale: each
 * frame stands 480 high with his feet at the middle of its bottom edge. */
#define SPRITES \
    X(room, "room.jpg") X(room_front, "room_front.png") X(hat, "hat.png") \
    X(g_w1, "g_w1.png") X(g_w2, "g_w2.png") X(g_w3, "g_w3.png") X(g_w4, "g_w4.png") \
    X(g_stand, "g_stand.png") X(g_peer, "g_peer.png") X(g_brows, "g_brows.png") X(g_cigar, "g_cigar.png") \
    X(g_arms, "g_arms.png") X(g_kick, "g_kick.png") X(g_spin, "g_spin.png") X(g_crouch, "g_crouch.png") \
    X(g_wave, "g_wave.png") X(g_hold, "g_hold.png") X(g_hat_on, "g_hat_on.png") X(g_grin, "g_grin.png") \
    X(hp_w1, "hp_w1.png") X(hp_w2, "hp_w2.png") X(hp_w3, "hp_w3.png") X(hp_w4, "hp_w4.png") \
    X(hp_stand, "hp_stand.png") X(hp_peer, "hp_peer.png") X(hp_brows, "hp_brows.png") X(hp_cigar, "hp_cigar.png") \
    X(hp_arms, "hp_arms.png") X(hp_kick, "hp_kick.png") X(hp_spin, "hp_spin.png") X(hp_crouch, "hp_crouch.png") \
    X(hp_wave, "hp_wave.png") X(hp_hold, "hp_hold.png") X(hp_hat_on, "hp_hat_on.png") X(hp_grin, "hp_grin.png") \
    X(h_stand, "h_stand.png") X(h_horn, "h_horn.png") X(h_drop, "h_drop.png") X(h_grin, "h_grin.png") \
    X(c_w1, "c_w1.png") X(c_w2, "c_w2.png") X(c_stand, "c_stand.png") X(c_shrug, "c_shrug.png")
#define X(name, file) ART(name, file)
SPRITES
#undef X
#define X(name, file) SP_##name,
enum { SPRITES NSPRITE };
#undef X
#define X(name, file) {art_##name, art_##name##_end},
static struct { const unsigned char *file, *end; int w, h; unsigned char *px; } sprite[NSPRITE] = { SPRITES };
#undef X
static SDL_Texture **cur_tex; /* the pictures, for the renderer being drawn to */

static void load_sprites(void)
{
    for (int i = 0; i < NSPRITE; i++) {
        int n;
        sprite[i].px = stbi_load_from_memory(sprite[i].file, (int)(sprite[i].end - sprite[i].file), &sprite[i].w, &sprite[i].h, &n, 4);
        if (!sprite[i].px) { sprite[i].w = sprite[i].h = 1; }
    }
}
static void make_textures(SDL_Renderer *r, SDL_Texture **tex)
{
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    for (int i = 0; i < NSPRITE; i++) {
        tex[i] = NULL;
        if (!sprite[i].px) continue;
        tex[i] = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, sprite[i].w, sprite[i].h);
        if (!tex[i]) continue;
        SDL_UpdateTexture(tex[i], NULL, sprite[i].px, sprite[i].w * 4);
        SDL_SetTextureBlendMode(tex[i], SDL_BLENDMODE_BLEND);
    }
}

/* ---- options ---- */
static int opt_window = 0;
static double opt_speed = 1.0;
static uint64_t opt_seed = 0;
static const char *opt_snapshot = NULL;
static double opt_time = 30;
static int opt_w = 1920, opt_h = 1200;

/* ---- colours ---- */
typedef struct { Uint8 r, g, b; } Rgb;
static const Rgb BG = {8, 8, 20}, INK = {30, 24, 20}, RED = {170, 40, 32}, PAPER = {250, 244, 226};

/* ---- geometry buffer ---- */
static SDL_Vertex *vb;
static int *ib;
static int nv, ni, vcap, icap;

static void vb_need(int v, int i)
{
    if (nv + v > vcap) { vcap = (nv + v) * 2; vb = realloc(vb, sizeof *vb * vcap); }
    if (ni + i > icap) { icap = (ni + i) * 2; ib = realloc(ib, sizeof *ib * icap); }
}
static inline SDL_Color col(Rgb c, double a)
{
    SDL_Color o = {c.r, c.g, c.b, (Uint8)(a < 0 ? 0 : a > 1 ? 255 : a * 255)};
    return o;
}
static inline void vert(double x, double y, SDL_Color c)
{
    vb[nv].position.x = (float)x; vb[nv].position.y = (float)y;
    vb[nv].color = c; vb[nv].tex_coord.x = vb[nv].tex_coord.y = 0;
    nv++;
}
static void quad4(double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3,
                  SDL_Color c0, SDL_Color c1, SDL_Color c2, SDL_Color c3)
{
    vb_need(4, 6);
    int b = nv;
    vert(x0, y0, c0); vert(x1, y1, c1); vert(x2, y2, c2); vert(x3, y3, c3);
    ib[ni++] = b; ib[ni++] = b + 1; ib[ni++] = b + 2; ib[ni++] = b; ib[ni++] = b + 2; ib[ni++] = b + 3;
}
static void quad(double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3, SDL_Color c)
{
    quad4(x0, y0, x1, y1, x2, y2, x3, y3, c, c, c, c);
}
static void rect(double x, double y, double w, double h, SDL_Color c) { quad(x, y, x + w, y, x + w, y + h, x, y + h, c); }
static void disc(double cx, double cy, double r, SDL_Color centre, SDL_Color rim)
{
    int n = r < 4 ? 10 : r < 30 ? 28 : 56;
    vb_need(n + 1, 3 * n);
    int b = nv;
    vert(cx, cy, centre);
    for (int i = 0; i < n; i++) vert(cx + r * cos(i * 2 * M_PI / n), cy + r * sin(i * 2 * M_PI / n), rim);
    for (int i = 0; i < n; i++) { ib[ni++] = b; ib[ni++] = b + 1 + i; ib[ni++] = b + 1 + (i + 1) % n; }
}
/* a stroke with round ends */
static void line(double x0, double y0, double x1, double y1, double w, SDL_Color c)
{
    double dx = x1 - x0, dy = y1 - y0, l = hypot(dx, dy);
    if (l > 1e-9) {
        double nx = -dy / l * w / 2, ny = dx / l * w / 2;
        quad(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, c);
    }
    if (w > 2.5) { disc(x0, y0, w / 2, c, c); disc(x1, y1, w / 2, c, c); }
}
__attribute__((unused)) static void ring(double cx, double cy, double r, double w, SDL_Color c)
{
    for (int i = 0; i < 64; i++) {
        double a = i * M_PI / 32, b = (i + 1) * M_PI / 32;
        line(cx + cos(a) * r, cy + sin(a) * r, cx + cos(b) * r, cy + sin(b) * r, w, c);
    }
}
static void flush(SDL_Renderer *r)
{
    if (ni) SDL_RenderGeometry(r, NULL, vb, nv, ib, ni);
    nv = ni = 0;
}

static SDL_Renderer *ren; /* where the current frame is being drawn */

/* a picture stretched over a rectangle; sx0..sx1 picks a slice of it across (0..1) */
static void pic_part(int which, double x, double y, double w, double h, double sx0, double sx1, double tilt, int flip)
{
    flush(ren);
    if (!cur_tex || !cur_tex[which]) return;
    SDL_Rect src = {(int)(sx0 * sprite[which].w), 0, (int)((sx1 - sx0) * sprite[which].w), sprite[which].h};
    SDL_FRect dst = {(float)x, (float)y, (float)w, (float)h};
    SDL_FPoint pivot = {(float)(w / 2), (float)h};
    SDL_RenderCopyExF(ren, cur_tex[which], &src, &dst, tilt, &pivot, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}
/* a picture `tall` high, standing with the point (ax, ay) of it (0..1 across and down) at (x, y) */
__attribute__((unused)) static void pic(int which, double x, double y, double tall, double ax, double ay)
{
    double w = tall * sprite[which].w / sprite[which].h;
    pic_part(which, x - w * ax, y - tall * ay, w, tall, 0, 1, 0, 0);
}

/* ---- lettering: pen strokes on a 4 x 6 grid, drawn with a round nib ---- */
static const char *GLYPH[] = {
    "A06 02 20 42 46|04 44", "B00 06 36 45 44 33 03|33 42 41 30 00", "C41 30 10 01 05 16 36 45",
    "D00 06 26 44 42 20 00", "E40 00 06 46|03 33", "F40 00 06|03 33", "G41 30 10 01 05 16 36 45 43 23",
    "H00 06|40 46|03 43", "I20 26|10 30|16 36", "J40 45 36 16 05", "K00 06|40 03 46", "L00 06 46",
    "M06 00 23 40 46", "N06 00 46 40", "O10 30 41 45 36 16 05 01 10", "P06 00 30 41 42 33 03",
    "Q10 30 41 45 36 16 05 01 10|24 46", "R06 00 30 41 42 33 03|23 46", "S41 30 10 01 02 13 33 44 45 36 16 05",
    "T00 40|20 26", "U00 05 16 36 45 40", "V00 26 40", "W00 16 23 36 40", "X00 46|40 06", "Y00 23 40|23 26",
    "Z00 40 06 46", "010 30 41 45 36 16 05 01 10", "111 20 26|16 36", "201 10 30 41 42 06 46",
    "301 10 30 41 42 33 23|33 44 45 36 16 05", "436 30 04 44", "540 00 03 33 44 45 36 16 05",
    "630 10 01 05 16 36 45 44 33 03", "700 40 16", "813 02 01 10 30 41 42 33 13 04 05 16 36 45 44 33",
    "943 13 02 01 10 30 41 45 36 16", ",25 26 17", ".26 26", ":22 22|25 25", "'20 21", "?01 10 30 41 42 24|26 26",
    "-13 33", "!20 24|26 26", "+03 43|21 25", "=02 42|04 44",
};
static const char *glyph(char ch)
{
    for (unsigned i = 0; i < sizeof GLYPH / sizeof *GLYPH; i++)
        if (GLYPH[i][0] == ch) return GLYPH[i] + 1;
    return NULL;
}
static double text_width(const char *str, double s) { return strlen(str) * 6.2 * s - 2.2 * s; }
/* text centred on cx, top at y; s is one grid step, so letters are 6s tall */
static void text(const char *str, double cx, double y, double s, double weight, SDL_Color c)
{
    double x = cx - text_width(str, s) / 2;
    for (; *str; str++, x += 6.2 * s) {
        const char *g = glyph(*str);
        if (!g) continue;
        int have = 0; double lx = 0, ly = 0;
        for (; *g; g++) {
            if (*g == '|') { have = 0; continue; }
            if (*g == ' ') continue;
            double gx = x + (g[0] - '0') * s, gy = y + (g[1] - '0') * s;
            g++;
            if (have) line(lx, ly, gx, gy, s * weight, c);
            disc(gx, gy, s * weight * (have && lx == gx && ly == gy ? 0.62 : 0.5), c, c); /* the nib is round */
            lx = gx; ly = gy; have = 1;
        }
    }
}
/* ---- the scene ----
 *
 * One man (G) stands on the near side of an empty mirror frame, and his
 * "reflection" (H) on the other. The show is written as G's track: where he is and
 * what pose he is in, key by key. H, unless told otherwise, is G mirrored about the
 * middle of the screen a moment later (his lag), which is what makes him almost a
 * reflection. Where H gives himself away he has keys of his own. The third man (C)
 * and the hats have their own timings. One round takes ROUND seconds and then the
 * next is made, with the dance moves in a new order. */
#define WALK (-1)       /* a pose: walking, from this key's place to the next one's */
#define MIRROR (-2)     /* an H pose: be G's reflection */
#define GONE (-3)       /* not on the screen */
#define LAG 0.07        /* how far behind G his reflection usually is */
#define NEAR 0.40       /* where G stands by the frame, along his path; H at 1 - NEAR */
#define OFF (-0.4)      /* off the left edge */

typedef struct { double t, x; int pose, flip; double lag; } Key;
typedef struct { Key k[160]; int n; } Track;
static Track G, Hk, C;
/* A hat between two places. y is up from the floor in his heights; x is across the
 * screen, or for an end that is ON_G or ON_H, his heights forward of that man's middle. */
enum { ON_SCREEN, ON_G, ON_H };
typedef struct { double t0, t1, x0, y0, x1, y1, lift, turns; int on0, on1; } Flight;
typedef struct { double t0, t1, x, y; const char *say; } Bubble;
static Flight hats[8];
static Bubble bubbles[4]; /* x across the screen, y up from the floor in his heights */
/* The room picture, and where the men stand in it: the floor line, and their height,
 * as parts of the picture's height */
/* The path he walks, in the room picture's width and height: where it is at 0 (the
 * near room) and 1 (the room beyond), with the middle of the frame at 0.5. TALL is
 * his height at 0, as part of the picture's height, and FAR_SCALE his size at 1. */
#define PATH_AX 0.05
#define PATH_AY 0.80
#define PATH_BX 1.05
#define PATH_BY 0.69
#define TALL 0.48
#define FAR_SCALE 0.55
static int nhat, nbubble;
static double ROUND, T, anim_t;
static uint64_t seed_state;

static uint64_t next_rand(void)
{
    seed_state ^= seed_state << 13; seed_state ^= seed_state >> 7; seed_state ^= seed_state << 17;
    return seed_state;
}
static double frand(void) { return (next_rand() >> 11) * (1.0 / 9007199254740992.0); }
static double ramp(double t, double a, double b) { return t <= a ? 0 : t >= b ? 1 : (t - a) / (b - a); }
static double ease(double x) { return x * x * (3 - 2 * x); }

static void key(Track *tr, double t, double x, int pose, int flip, double lag)
{
    if (tr->n < (int)(sizeof tr->k / sizeof *tr->k)) tr->k[tr->n++] = (Key){t, x, pose, flip, lag};
}

/* G's track is written with a cursor: each call adds a key at `now` and moves on */
static double now, gx;
static int gflip;
static void g(int pose, double dur) { key(&G, now, gx, pose, gflip, 0); now += dur; }
static void g_walk(double to, double dur)
{
    gflip = to < gx;
    key(&G, now, gx, WALK, gflip, 0);
    gx = to; now += dur;
}
static void h_mirror(double t, double lag) { key(&Hk, t, 0, MIRROR, 0, lag); }
static void h(double t, int pose) { key(&Hk, t, 1 - NEAR, pose, 0, 0); }
static void fly(double t0, double t1, int on0, double x0, double y0, int on1, double x1, double y1, double lift, double turns)
{
    if (nhat < 8) hats[nhat++] = (Flight){t0, t1, x0, y0, x1, y1, lift, turns, on0, on1};
}

static void build_round(void)
{
    G.n = Hk.n = C.n = nhat = nbubble = 0;
    now = 0; gx = OFF; gflip = 0;
    key(&C, 0, 0, GONE, 0, 0);
    h_mirror(0, 0);
    g(SP_g_stand, 1.0);
    /* in he comes, and so does the other one */
    g_walk(NEAR, 2.8);
    gflip = 0;
    g(SP_g_stand, 0.9); g(SP_g_peer, 1.5); g(SP_g_brows, 1.2);
    h_mirror(now, LAG);
    g(SP_g_crouch, 0.5); g(SP_g_stand, 0.35); g(SP_g_crouch, 0.3); g(SP_g_peer, 0.9);
    /* a sudden turn round, to catch him out; he is a little slow */
    h_mirror(now, 0.13);
    g(SP_g_spin, 0.22); gflip = 1; g(SP_g_stand, 0.6); g(SP_g_spin, 0.22); gflip = 0; g(SP_g_brows, 1.1);
    h_mirror(now, LAG);
    /* off, and a peek back round the edge */
    g_walk(OFF, 1.7);
    gflip = 0; g(SP_g_stand, 1.6);
    gx = 0.02; g(SP_g_peer, 1.3);
    g_walk(NEAR, 1.2); gflip = 0; g(SP_g_stand, 0.6);

    /* the dance: six moves in a new order every time, and once he is late */
    int moves[] = {SP_g_arms, SP_g_kick, SP_g_wave, SP_g_crouch, SP_g_cigar, SP_g_peer, SP_g_brows, SP_g_spin};
    int nm = sizeof moves / sizeof *moves;
    for (int i = nm - 1; i > 0; i--) { int j = (int)(frand() * (i + 1)) % (i + 1), k = moves[i]; moves[i] = moves[j]; moves[j] = k; }
    int late = 2 + (int)(frand() * 4) % 4;
    for (int i = 0; i < 6; i++) {
        if (i == late) h_mirror(now, 0.4);
        g(moves[i], 0.55 + 0.35 * frand());
        if (moves[i] == SP_g_spin) { gflip = 0; }
        if (i == late) { h_mirror(now, LAG); g(SP_g_stand, 0.25); g(SP_g_brows, 0.9); g(SP_g_peer, 0.9); }
    }
    g(SP_g_stand, 0.6);

    /* he drops his hat: G has none to drop. G picks one up anyway, and so on. Where
     * the hat is held in the drawings, in his heights from his middle and the floor: */
    const double DROP_X = 0.2, DROP_Y = 0.44, HOLD_X = 0.34, HOLD_Y = 0.42;
    h(now, SP_h_drop);
    g(SP_g_brows, 0.45);
    h(now, SP_h_stand);
    double down = now + 0.4;
    fly(now, down, ON_H, DROP_X, DROP_Y, ON_H, DROP_X, 0.0, 0.05, 0.4);
    g(SP_g_brows, 0.5);
    g(SP_g_peer, 1.1);
    h_mirror(now, LAG);
    fly(down, now + 0.45 + LAG, ON_H, DROP_X, 0.0, ON_H, DROP_X, 0.0, 0, 0);   /* it lies there till he is down for it */
    g(SP_g_crouch, 0.6); g(SP_g_hold, 1.0); g(SP_g_hat_on, 1.1); g(SP_g_grin, 1.2); g(SP_g_hold, 0.5);
    /* and both throw them away */
    fly(now, now + 0.9, ON_G, HOLD_X, HOLD_Y, ON_SCREEN, -0.5, 1.4, 0.6, -1.5);
    fly(now + LAG, now + 0.9 + LAG, ON_H, HOLD_X, HOLD_Y, ON_SCREEN, 1.5, 1.4, 0.6, 1.5);
    g(SP_g_stand, 0.8);

    /* the horn */
    g(SP_g_stand, 1.0);
    h(now, SP_h_horn);
    if (nbubble < 4) bubbles[nbubble++] = (Bubble){now + 0.1, now + 1.1, 1 - NEAR, 1.0, "HONK!"};
    g(SP_g_stand, 0.9);
    h(now, SP_h_grin);
    g(SP_g_peer, 1.3);
    h_mirror(now, LAG);
    g(SP_g_brows, 0.8);

    /* through the frame and back, each passing the other */
    g_walk(1 - NEAR, 1.6); gflip = 1; g(SP_g_stand, 0.6); g(SP_g_brows, 0.9);
    g_walk(NEAR, 1.6); gflip = 0; g(SP_g_stand, 0.6);

    /* and a third man walks in, between them */
    key(&C, now, 1.5, WALK, 0, 0);
    g(SP_g_stand, 1.8); g(SP_g_peer, 1.4);
    key(&C, now, 0.52, SP_c_stand, 0, 0);
    g(SP_g_brows, 0.8);
    key(&C, now, 0.52, SP_c_shrug, 0, 0);
    g(SP_g_brows, 1.6);
    key(&C, now, 0.52, SP_c_stand, 0, 0);
    g_walk(OFF - 0.02, 2.2);
    key(&C, now, 0.52, SP_c_shrug, 0, 0);
    g(SP_g_stand, 2.4);
    ROUND = now;
    key(&G, ROUND + 10, gx, SP_g_stand, gflip, 0);
    key(&C, ROUND + 10, 0.5, SP_c_shrug, 0, 0);
}

/* lift: how far off the floor, and shrink: how much shorter (ducking), both in his heights */
typedef struct { int pose; double x; int flip; double lift, shrink; } Pose;

static int find(const Track *tr, double t)
{
    int i = 0;
    while (i + 1 < tr->n && tr->k[i + 1].t <= t) i++;
    return i;
}
/* where someone is at time t on a track; walking picks its frame by the ground covered */
static Pose at(const Track *tr, double t, const int *cycle, int ncycle, double stride)
{
    int i = find(tr, t);
    const Key *k = &tr->k[i];
    Pose p = {k->pose, k->x, k->flip};
    if (k->pose == WALK) {
        const Key *nx = i + 1 < tr->n ? &tr->k[i + 1] : k;
        double f = nx->t > k->t ? ramp(t, k->t, nx->t) : 1;
        p.x = k->x + (nx->x - k->x) * f;
        p.pose = cycle[(int)(fabs(p.x - k->x) / stride) % ncycle];
        p.flip = nx->x < k->x;
    }
    return p;
}
static const int G_WALK[4] = {SP_g_w1, SP_g_w2, SP_g_w3, SP_g_w4};
static const int C_WALK[2] = {SP_c_w1, SP_c_w2};
static Pose g_at(double t) { return at(&G, t, G_WALK, 4, 0.045); }
static Pose h_at(double t)
{
    const Key *k = &Hk.k[find(&Hk, t)];
    if (k->pose != MIRROR) return (Pose){k->pose, k->x, 0, 0, 0};
    Pose p = g_at(t - k->lag);
    p.x = 1 - p.x; p.flip = !p.flip;
    if (p.pose >= SP_g_w1 && p.pose <= SP_g_grin) p.pose += SP_hp_w1 - SP_g_w1;   /* it is Harpo, in the same disguise */
    return p;
}

static void update(double dt)
{
    anim_t += dt;
    T += dt * opt_speed;
    if (T >= ROUND) { T = 0; build_round(); }
}

/* ---- drawing ---- */
static double unit;
static int sw, sh;
static double rx0, ry0, rw, rh;  /* where the room picture is on the screen */

/* one frame of a figure, feet at (x, y); the frames are all to one scale, 480 high */
static void figure(int which, double x, double y, double tall, int flip, double alpha, double shrink)
{
    flush(ren);
    if (which < 0 || alpha <= 0.01 || !cur_tex || !cur_tex[which]) return;
    double k = tall / 480.0, w = sprite[which].w * k, hgt = sprite[which].h * k * (1 - shrink);
    SDL_FRect dst = {(float)(x - w / 2), (float)(y - hgt + 10 * k), (float)w, (float)hgt};
    SDL_SetTextureAlphaMod(cur_tex[which], (Uint8)(fmin(alpha, 1) * 255));
    SDL_RenderCopyExF(ren, cur_tex[which], NULL, &dst, 0, NULL, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureAlphaMod(cur_tex[which], 255);
}
static void shadow(double x, double y, double r)
{
    int n = 28;
    vb_need(n + 1, 3 * n);
    int b = nv;
    vert(x, y, col(INK, 0.35));
    for (int i = 0; i < n; i++) vert(x + r * cos(i * 2 * M_PI / n), y + r * 0.22 * sin(i * 2 * M_PI / n), col(INK, 0));
    for (int i = 0; i < n; i++) { ib[ni++] = b; ib[ni++] = b + 1 + i; ib[ni++] = b + 1 + (i + 1) % n; }
}
/* Where on the screen a place on his path is. The path runs from the near room (0)
 * through the middle of the frame (0.5) into the room beyond (1), getting further
 * away, so his feet rise and he gets smaller; outside 0..1 it carries on the same. */
static void spot(double x, double *sx, double *feet, double *tall)
{
    *sx = rx0 + rw * (PATH_AX + (PATH_BX - PATH_AX) * x);
    *feet = ry0 + rh * (PATH_AY + (PATH_BY - PATH_AY) * x);
    *tall = rh * TALL * (1 + (FAR_SCALE - 1) * x);
}
static void person(Pose p)
{
    if (p.pose < 0) return;
    double x, feet, tall;
    spot(p.x, &x, &feet, &tall);
    if (x < -tall || x > sw + tall) return;
    shadow(x, feet, tall * 0.2 * (1 - p.lift * 4));
    figure(p.pose, x, feet - p.lift * tall, tall, p.flip, 1, p.shrink);
}
static void hat_at(double x, double y, double tall, double deg)
{
    flush(ren);
    if (!cur_tex || !cur_tex[SP_hat]) return;
    double w = tall * 0.3, hh = w * sprite[SP_hat].h / sprite[SP_hat].w;
    SDL_FRect d = {(float)(x - w / 2), (float)(y - hh), (float)w, (float)hh};
    SDL_RenderCopyExF(ren, cur_tex[SP_hat], NULL, &d, deg, NULL, SDL_FLIP_NONE);
}

/* Where a hat's end is: on the screen, its height (y up from the floor in his
 * heights) at that place, and how tall a man is there. Forward of a man is the way
 * he faces. */
static void place(int on, double x, double y, Pose gp, Pose hp, double *sx, double *sy, double *tall, double *path)
{
    double feet;
    if (on == ON_SCREEN) { *path = x; spot(x, sx, &feet, tall); }
    else {
        Pose p = on == ON_G ? gp : hp;
        int left = (p.pose >= SP_h_stand) != p.flip;   /* his own drawings, and the third man's, face left */
        *path = p.x;
        spot(p.x, sx, &feet, tall);
        *sx += (left ? -x : x) * *tall;
    }
    *sy = feet - y * *tall;
}

static void draw_scene(SDL_Renderer *r, int w, int h)
{
    ren = r; sw = w; sh = h;
    unit = (w < h ? w : h) * 0.0125;
    SDL_SetRenderDrawColor(r, BG.r, BG.g, BG.b, 255);
    SDL_RenderClear(r);
    /* the room, filling the screen and cut to fit */
    {
        double s = fmax((double)w / sprite[SP_room].w, (double)h / sprite[SP_room].h);
        rw = sprite[SP_room].w * s; rh = sprite[SP_room].h * s; rx0 = (w - rw) / 2; ry0 = (h - rh) / 2;
        pic_part(SP_room, rx0, ry0, rw, rh, 0, 1, 0, 0);
    }
    /* candlelight that wavers a little */
    double flicker = 0.5 + 0.3 * sin(anim_t * 7.1) * sin(anim_t * 2.3 + 1);
    rect(0, 0, w, h, (SDL_Color){255, 190, 110, (Uint8)(10 + 8 * flicker)});
    flush(r);

    /* Everyone beyond the frame is drawn first, then the near room over them with
     * the opening cut out of it, so that they are seen only through the frame; then
     * everyone on the near side. */
    Pose gp = g_at(T), hp = h_at(T), cp = at(&C, T, C_WALK, 2, 0.06);
    for (int side = 1; side >= 0; side--) {
        if (cp.pose >= 0 && (cp.x >= 0.5) == side) person(cp);
        flush(r);
        for (int i = 0; i < nhat; i++) {             /* hats lying or flying */
            const Flight *f = &hats[i];
            if (T < f->t0 || T > f->t1) continue;
            double x0, y0, t0, p0, x1, y1, t1, p1;
            double q = ramp(T, f->t0, f->t1);
            place(f->on0, f->x0, f->y0, gp, hp, &x0, &y0, &t0, &p0);
            place(f->on1, f->x1, f->y1, gp, hp, &x1, &y1, &t1, &p1);
            if ((p0 + (p1 - p0) * q >= 0.5) != side) continue;
            double y = y0 + (y1 - y0) * q * q - f->lift * t0 * 4 * q * (1 - q);
            hat_at(x0 + (x1 - x0) * q, y, t0 + (t1 - t0) * q, f->turns * 360 * q);
        }
        if (hp.pose >= 0 && (hp.x >= 0.5) == side) person(hp);
        flush(r);
        if ((gp.x >= 0.5) == side) person(gp);
        flush(r);
        if (side) pic_part(SP_room_front, rx0, ry0, rw, rh, 0, 1, 0, 0);
    }
    double hx, hfeet, htall;
    spot(1 - NEAR, &hx, &hfeet, &htall);
    for (int i = 0; i < nbubble; i++) {
        const Bubble *b = &bubbles[i];
        if (T < b->t0 || T > b->t1) continue;
        double q = ramp(T, b->t0, b->t1), pop = 1 + 0.25 * sin(fmin(q * 8, 1) * M_PI);
        text(b->say, hx - htall * 0.35, hfeet - b->y * htall - q * unit * 3, unit * 0.9 * pop, 1.1, col(RED, 1 - ramp(q, 0.7, 1)));
    }
    flush(r);
    /* a title card at the start, as in the pictures of the time, and a fade at each end */
    double card = 1 - ease(ramp(T, 2.2, 3.0));
    if (card > 0) {
        rect(0, 0, w, h, col(BG, card * 0.85));
        text("THE MIRROR SCENE", w / 2.0, h * 0.42, unit * 1.1, 1.0, col(PAPER, card));
        flush(r);
    }
    double dark = fmax(1 - ramp(T, 0, 0.8), ramp(T, ROUND - 1.2, ROUND));
    if (dark > 0) rect(0, 0, w, h, col(BG, dark));
    flush(r);
}

typedef struct { SDL_Window *win; SDL_Renderer *ren; SDL_Texture *scene, *tex[NSPRITE]; int w, h; } View;

static void size_view(View *v)
{
    int ow, oh;
    SDL_RendererInfo info;
    SDL_GetRendererOutputSize(v->ren, &ow, &oh);
    if (ow < 1 || oh < 1) return;
    int ss = 2; /* drawn at twice the size and scaled down, for smooth edges */
    if (SDL_GetRendererInfo(v->ren, &info) == 0) {
        if (!(info.flags & SDL_RENDERER_TARGETTEXTURE) || (info.flags & SDL_RENDERER_SOFTWARE)) ss = 0;
        else if (info.max_texture_width && (ow * 2 > info.max_texture_width || oh * 2 > info.max_texture_height)) ss = 0;
    }
    if (v->w == ow * (ss ? ss : 1) && v->h == oh * (ss ? ss : 1) && (v->scene || !ss)) return;
    if (v->scene) SDL_DestroyTexture(v->scene);
    v->scene = NULL; v->w = ow; v->h = oh;
    if (!ss) return;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    v->scene = SDL_CreateTexture(v->ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, ow * ss, oh * ss);
    if (v->scene) { v->w = ow * ss; v->h = oh * ss; }
}
static void render(View *v)
{
    if (v->scene) SDL_SetRenderTarget(v->ren, v->scene);
    cur_tex = v->tex;
    draw_scene(v->ren, v->w, v->h);
    if (v->scene) { SDL_SetRenderTarget(v->ren, NULL); SDL_RenderCopy(v->ren, v->scene, NULL, NULL); }
    SDL_RenderPresent(v->ren);
}
static int make_view(View *v, int display)
{
    Uint32 flags = SDL_WINDOW_ALLOW_HIGHDPI;
    int x = SDL_WINDOWPOS_CENTERED_DISPLAY(display);
    if (opt_window) flags |= SDL_WINDOW_RESIZABLE;
    else flags |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
    memset(v, 0, sizeof *v);
    v->win = SDL_CreateWindow("The Mirror Scene", x, x, 1200, 750, flags);
    if (!v->win) return 0;
    v->ren = SDL_CreateRenderer(v->win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    if (!v->ren) v->ren = SDL_CreateRenderer(v->win, -1, SDL_RENDERER_SOFTWARE);
    if (!v->ren) return 0;
    SDL_SetRenderDrawBlendMode(v->ren, SDL_BLENDMODE_BLEND);
    make_textures(v->ren, v->tex);
    return 1;
}

static void start_show(void)
{
    seed_state = opt_seed ? opt_seed * 0x9E3779B97F4A7C15ULL : ((uint64_t)time(NULL) << 20) ^ SDL_GetPerformanceCounter();
    if (!seed_state) seed_state = 1;
    for (int i = 0; i < 6; i++) next_rand();
    anim_t = 0; T = 0;
    build_round();
}

static int snapshot(void)
{
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, opt_w, opt_h, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_Renderer *r = surf ? SDL_CreateSoftwareRenderer(surf) : NULL;
    if (!r) { fprintf(stderr, "mirror-scene: %s\n", SDL_GetError()); return 1; }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_Texture *tex[NSPRITE];
    make_textures(r, tex);
    cur_tex = tex;
    start_show();
    for (double t = 0; t < opt_time; t += 1.0 / 60) update(1.0 / 60);
    draw_scene(r, opt_w, opt_h);
    SDL_RenderPresent(r);
    if (SDL_SaveBMP(surf, opt_snapshot) != 0) { fprintf(stderr, "mirror-scene: %s\n", SDL_GetError()); return 1; }
    return 0;
}

static void usage(void)
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "The Mirror Scene",
        "The Mirror Scene screensaver\n\n"
        "  --window        run in a window (Esc or Q quits)\n"
        "  --speed X       animation speed, 1 = normal\n"
        "  --seed N        repeatable sequence\n"
        "  --snapshot FILE.bmp [--time SECONDS] [--size WxH]\n"
        "                  write a picture of the screensaver after SECONDS and exit\n", NULL);
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if ((a[0] == '/' || a[0] == '-') && (a[1] == 's' || a[1] == 'S') && !a[2]) continue;
        else if ((a[0] == '/' || a[0] == '-') && (a[1] == 'p' || a[1] == 'P') && (!a[2] || a[2] == ':')) return 0;
        else if ((a[0] == '/' || a[0] == '-') && (a[1] == 'c' || a[1] == 'C') && (!a[2] || a[2] == ':')) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "The Mirror Scene", "The Mirror Scene screensaver has no settings.", NULL);
            return 0;
        }
        else if (!strcmp(a, "--window")) opt_window = 1;
        else if (!strcmp(a, "--speed") && i + 1 < argc) opt_speed = SDL_atof(argv[++i]);
        else if (!strcmp(a, "--seed") && i + 1 < argc) opt_seed = SDL_strtoull(argv[++i], NULL, 10);
        else if (!strcmp(a, "--snapshot") && i + 1 < argc) opt_snapshot = argv[++i];
        else if (!strcmp(a, "--time") && i + 1 < argc) opt_time = SDL_atof(argv[++i]);
        else if (!strcmp(a, "--size") && i + 1 < argc) {
            const char *x = SDL_strchr(argv[++i], 'x');
            opt_w = SDL_atoi(argv[i]); opt_h = x ? SDL_atoi(x + 1) : 0;
            if (opt_w < 64 || opt_h < 64) { usage(); return 1; }
        }
        else { usage(); return 1; }
    }
    if (opt_speed < 0.05) opt_speed = 0.05;
    load_sprites();

    if (opt_snapshot) return snapshot();
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "mirror-scene: %s\n", SDL_GetError()); return 1; }

    View views[8];
    int nviews = 0, ndisp = opt_window ? 1 : SDL_GetNumVideoDisplays();
    if (ndisp < 1) ndisp = 1;
    for (int d = 0; d < ndisp && nviews < 8; d++)
        if (make_view(&views[nviews], d)) nviews++;
    if (!nviews) { fprintf(stderr, "mirror-scene: %s\n", SDL_GetError()); return 1; }
    if (!opt_window) SDL_ShowCursor(SDL_DISABLE);
    start_show();

    Uint64 freqc = SDL_GetPerformanceFrequency(), last = SDL_GetPerformanceCounter(), start = last;
    double moved = 0;
    int running = 1;
    while (running) {
        SDL_Event e;
        double since = (double)(SDL_GetPerformanceCounter() - start) / freqc;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) running = 0;
            else if (e.type == SDL_KEYDOWN) {
                if (!opt_window || e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_q) running = 0;
            } else if (!opt_window && since > 1.0) {
                if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEWHEEL || e.type == SDL_FINGERDOWN) running = 0;
                else if (e.type == SDL_MOUSEMOTION) {
                    moved += abs(e.motion.xrel) + abs(e.motion.yrel);
                    if (moved > 24) running = 0;
                }
            }
        }
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - last) / freqc;
        last = now;
        if (dt > 0.1) dt = 0.1;
        update(dt);
        for (int i = 0; i < nviews; i++) { size_view(&views[i]); render(&views[i]); }
        SDL_Delay(views[0].scene ? 1 : 12);
    }
    SDL_Quit();
    return 0;
}
