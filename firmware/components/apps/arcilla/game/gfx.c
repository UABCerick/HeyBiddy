#include "gfx.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t *FB, *BG, *SCREEN;
static int TW = SCR, TH = SCR;   /* destino actual: la pantalla o un lienzo (caché del tablero) */
static uint8_t GA = 255;

static void build_background(void) {
    /* Degradado radial oscuro con un tramado sutil, igual que el prototipo */
    for (int y = 0; y < SCR; y++) for (int x = 0; x < SCR; x++) {
        float dx = x - 180.0f, dy = y - 150.0f, d = sqrtf(dx * dx + dy * dy);
        float t = (d - 20.0f) / 180.0f; if (t < 0) t = 0; if (t > 1) t = 1;
        int r = (int)(0x23 + (0x0d - 0x23) * t), g = (int)(0x1c + (0x0a - 0x1c) * t), b = (int)(0x2b + (0x10 - 0x2b) * t);
        if ((y % 3) == 0 && ((x + (y / 3 % 2) * 3) % 6) == 0) { r += 4; g += 4; b += 4; }
        BG[y * SCR + x] = RGB(r, g, b);
    }
}
void gfx_init(uint16_t *framebuffer, uint16_t *background) { FB = SCREEN = framebuffer; BG = background; build_background(); }
void gfx_target(uint16_t *buf, int w, int h) { FB = buf; TW = w; TH = h; }
void gfx_target_screen(void) { FB = SCREEN; TW = SCR; TH = SCR; }
int gfx_tw(void) { return TW; }
int gfx_th(void) { return TH; }
const uint16_t *gfx_bg(void) { return BG; }
uint16_t *gfx_fb(void) { return FB; }
void gfx_background(void) { memcpy(SCREEN, BG, SCR * SCR * 2); }
void gfx_global_alpha(uint8_t a) { GA = a; }

/* Mezcla RGB565 con alfa de 5 bits: una multiplicación por píxel en vez de tres divisiones.
 * Los canales se separan en una palabra de 32 bits (verde arriba) para operar los tres a la vez. */
static inline uint32_t spread(uint16_t c) { return (c | ((uint32_t)c << 16)) & 0x07E0F81Fu; }
static inline uint16_t blend32(uint16_t d, uint32_t fg, uint32_t a5) {
    uint32_t bg = spread(d);
    uint32_t r = ((((fg - bg) * a5) >> 5) + bg) & 0x07E0F81Fu;
    return (uint16_t)(r | (r >> 16));
}
static inline uint16_t blend(uint16_t d, uint16_t s, uint32_t a) { return blend32(d, spread(s), (a + 4) >> 3); }
void gfx_fill_a(int x, int y, int w, int h, uint16_t c, uint8_t alpha) {
    uint32_t a = (uint32_t)alpha * GA / 255;
    if (a == 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > TW) w = TW - x;
    if (y + h > TH) h = TH - y;
    if (w <= 0 || h <= 0) return;
    uint32_t fg = spread(c), a5 = (a + 4) >> 3;
    for (int j = 0; j < h; j++) {
        uint16_t *p = FB + (y + j) * TW + x;
        if (a >= 255 || a5 >= 32) for (int i = 0; i < w; i++) p[i] = c;
        else if (a5) for (int i = 0; i < w; i++) p[i] = blend32(p[i], fg, a5);
    }
}
void gfx_fill(int x, int y, int w, int h, uint16_t c) { gfx_fill_a(x, y, w, h, c, 255); }
void gfx_pixel_a(int x, int y, uint16_t c, uint8_t alpha) { gfx_fill_a(x, y, 1, 1, c, alpha); }

void gfx_arc(int cx, int cy, int r, float a0, float a1, int thick, uint16_t c, uint8_t alpha) {
    if (a1 <= a0) return;
    float step = 1.0f / (float)r;
    for (float a = a0; a <= a1; a += step) {
        int x = cx + (int)lroundf(cosf(a) * r), y = cy + (int)lroundf(sinf(a) * r);
        gfx_fill_a(x - thick / 2, y - thick / 2, thick, thick, c, alpha);
    }
}

/* ---------- Texto ---------- */
static uint32_t next_cp(const char **s) {
    const unsigned char *p = (const unsigned char *)*s;
    uint32_t c = p[0];
    if (c < 0x80) { *s += 1; return c; }
    if ((c & 0xE0) == 0xC0) { *s += 2; return ((c & 0x1F) << 6) | (p[1] & 0x3F); }
    if ((c & 0xF0) == 0xE0) { *s += 3; return ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); }
    *s += 4; return '?';
}
static const glyph_t *find(const font_t *f, uint32_t cp) {
    int lo = 0, hi = f->n - 1;
    while (lo <= hi) { int m = (lo + hi) / 2; if (f->g[m].cp == cp) return &f->g[m]; if (f->g[m].cp < cp) lo = m + 1; else hi = m - 1; }
    return NULL;
}
static const glyph_t *glyph(const font_t *f, uint32_t cp) {
    const glyph_t *g = find(f, cp);
    if (g) return g;
    if (cp == 0x2192) return find(f, '>');
    if (cp == 0x2212) return find(f, '-');
    if (cp == 0x00B2) return find(f, '2');
    if (cp == 0x03C7) return find(f, 'x');
    if (cp == 0x00D7) return find(f, 'x');
    if (cp == 0x00B1) return find(f, '+');
    if (cp == 0x2248) return find(f, '~');
    if (cp == 0x2264) return find(f, '<');
    return find(f, '?');
}
int gfx_text_w(const char *s, const font_t *f) {
    int w = 0;
    while (*s) { const glyph_t *g = glyph(f, next_cp(&s)); if (g) w += g->adv; }
    return w;
}
static void draw_glyph(const font_t *f, const glyph_t *g, int x, int base, uint16_t c, uint8_t alpha) {
    /* escribe directo en el búfer: antes era una llamada a gfx_fill_a por píxel */
    uint32_t a = (uint32_t)alpha * GA / 255; if (!a) return;
    int bpr = (g->w + 7) / 8, x0 = x + g->xo, y0 = base + g->yo;
    const uint8_t *b = f->bits + g->off;
    for (int j = 0; j < g->h; j++) {
        int yy = y0 + j; if ((unsigned)yy >= (unsigned)TH) continue;
        const uint8_t *row = b + j * bpr; uint16_t *dst = FB + yy * TW;
        for (int i = 0; i < g->w; i++) {
            if (!(row[i >> 3] & (0x80 >> (i & 7)))) continue;
            int xx = x0 + i; if ((unsigned)xx >= (unsigned)TW) continue;
            dst[xx] = a >= 255 ? c : blend(dst[xx], c, a);
        }
    }
}
static void text_raw(const char *s, int x, int base, const font_t *f, uint16_t c, uint8_t a) {
    while (*s) { const glyph_t *g = glyph(f, next_cp(&s)); if (!g) continue; draw_glyph(f, g, x, base, c, a); x += g->adv; }
}
void gfx_text(const char *s, int x, int y, const font_t *f, uint16_t c, align_t al, bool shadow) {
    int w = gfx_text_w(s, f);
    if (al == AL_CENTER) x -= w / 2; else if (al == AL_RIGHT) x -= w;
    int base = y + f->cap / 2;
    if (shadow) text_raw(s, x + 1, base + 1, f, 0, 150);
    text_raw(s, x, base, f, c, 255);
}
/* Parte el texto en líneas que caben en max_w. cb puede ser NULL (solo cuenta). */
static int wrap_impl(const char *s, int max_w, const font_t *f, int cx, int y, int lh, uint16_t c, bool draw) {
    char line[160], cand[160]; int n = 0; line[0] = 0;
    const char *p = s;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        const char *e = p; while (*e && *e != ' ') e++;
        int wl = (int)(e - p);
        if (line[0]) snprintf(cand, sizeof cand, "%s %.*s", line, wl, p); else snprintf(cand, sizeof cand, "%.*s", wl, p);
        if (line[0] && gfx_text_w(cand, f) > max_w) {
            if (draw) gfx_text(line, cx, y + n * lh, f, c, AL_CENTER, true);
            n++; snprintf(line, sizeof line, "%.*s", wl, p);
        } else strcpy(line, cand);
        p = e;
    }
    if (line[0]) { if (draw) gfx_text(line, cx, y + n * lh, f, c, AL_CENTER, true); n++; }
    return n;
}
int gfx_wrap(const char *s, int cx, int y, int max_w, int lh, const font_t *f, uint16_t c) { return wrap_impl(s, max_w, f, cx, y, lh, c, true); }
int gfx_wrap_count(const char *s, int max_w, const font_t *f) { return wrap_impl(s, max_w, f, 0, 0, 0, 0, false); }

/* ---------- Formas para efectos (dados, ondas, brillos) ---------- */
void gfx_px(int x, int y, uint16_t c, uint8_t alpha) {
    if ((unsigned)x >= (unsigned)TW || (unsigned)y >= (unsigned)TH) return;
    uint32_t a = (uint32_t)alpha * GA / 255; if (!a) return;
    uint16_t *p = FB + y * TW + x; *p = a >= 255 ? c : blend(*p, c, a);
}
void gfx_disc(int cx, int cy, int r, uint16_t c, uint8_t alpha) {
    for (int dy = -r; dy <= r; dy++) { int w = (int)sqrtf((float)(r * r - dy * dy)); gfx_fill_a(cx - w, cy + dy, 2 * w + 1, 1, c, alpha); }
}
void gfx_ring(int cx, int cy, int r, int thick, uint16_t c, uint8_t alpha) {
    int ri = r - thick; if (ri < 0) ri = 0;
    for (int dy = -r; dy <= r; dy++) {
        int wo = (int)sqrtf((float)(r * r - dy * dy));
        if (abs(dy) >= ri) { gfx_fill_a(cx - wo, cy + dy, 2 * wo + 1, 1, c, alpha); continue; }
        int wi = (int)sqrtf((float)(ri * ri - dy * dy));
        gfx_fill_a(cx - wo, cy + dy, wo - wi, 1, c, alpha); gfx_fill_a(cx + wi + 1, cy + dy, wo - wi, 1, c, alpha);
    }
}

/* Arte indexado (art_data): escala por vecino más cercano. pal ya trae los colores del equipo.
 * mode 0 normal · 1 silueta (color sil) · 2 destello blanco (contorno se mantiene) */
void gfx_art_sub(const uint8_t *px, int w, int h, int sx0, int sy0, int sw, int sh, float x, float y, float sc, bool flip, const uint16_t *pal, uint8_t alpha, int mode, uint16_t sil) {
    int dw = (int)(sw * sc + 0.5f), dh = (int)(sh * sc + 0.5f);
    if (dw <= 0 || dh <= 0 || dw > 720) return;
    int ox = (int)lroundf(x), oy = (int)lroundf(y);
    int16_t sxs[720];
    for (int i = 0; i < dw; i++) { int s = (int)(i / sc); if (s >= sw) s = sw - 1; sxs[i] = (int16_t)(sx0 + (flip ? sw - 1 - s : s)); }
    uint32_t a = (uint32_t)alpha * GA / 255; if (!a) return;
    for (int j = 0; j < dh; j++) {
        int yy = oy + j; if ((unsigned)yy >= (unsigned)TH) continue;
        int sy = (int)(j / sc); if (sy >= sh) sy = sh - 1;
        const uint8_t *row = px + (sy0 + sy) * w; uint16_t *dst = FB + yy * TW;
        for (int i = 0; i < dw; i++) {
            int xx = ox + i; if ((unsigned)xx >= (unsigned)TW) continue;
            uint8_t k = row[sxs[i]]; if (!k) continue;
            uint16_t c = mode == 1 ? sil : (mode == 2 && k != 1) ? 0xFFFF : pal[k];
            dst[xx] = a >= 255 ? c : blend(dst[xx], c, a);
        }
    }
    (void)h;
}
void gfx_art(const uint8_t *px, int w, int h, float x, float y, float sc, bool flip, const uint16_t *pal, uint8_t alpha, int mode, uint16_t sil) {
    gfx_art_sub(px, w, h, 0, 0, w, h, x, y, sc, flip, pal, alpha, mode, sil);
}
