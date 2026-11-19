#ifndef HUD_INTERNAL_H
#define HUD_INTERNAL_H

#include "hud_renderer.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t r, g, b, a;
} rgba_t;

static inline rgba_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    rgba_t c = {r, g, b, a};
    return c;
}

/* Shared IP / Telemetry Setup Modal geometry (render_ip_modal & touch handler use identical coordinates) */
#define IP_MODAL_W         600
#define IP_MODAL_H         570
#define IP_MODAL_X         ((SCREEN_W - IP_MODAL_W) / 2)
#define IP_MODAL_Y         ((SCREEN_H - IP_MODAL_H) / 2)

#define IP_KEY_W           156
#define IP_KEY_H           50
#define IP_KEY_GAP         10
#define IP_KEY_GX          (IP_MODAL_X + (IP_MODAL_W - (IP_KEY_W * 3 + IP_KEY_GAP * 2)) / 2)
#define IP_KEY_GY          (IP_MODAL_Y + 134)

#define IP_PROTO_Y         (IP_KEY_GY + 4 * (IP_KEY_H + IP_KEY_GAP) + 8)
#define IP_PROTO_BTN_Y     (IP_PROTO_Y + 16)
#define IP_PROTO_W         84
#define IP_PROTO_H         36
#define IP_PROTO_GAP       6
#define IP_PROTO_COUNT     ((int)GT7_HEARTBEAT_COUNT)
#define IP_PROTO_X_START   (IP_MODAL_X + (IP_MODAL_W - (IP_PROTO_W * IP_PROTO_COUNT + IP_PROTO_GAP * (IP_PROTO_COUNT - 1))) / 2)

#define IP_ACTION_BTN_Y    (IP_PROTO_BTN_Y + IP_PROTO_H + 16)
#define IP_ACTION_BTN_H    52

/* Shared drawing & formatting primitives implemented in source/hud_renderer.c */
void hud_fill_rect(SDL_Renderer *r, int x, int y, int w, int h, rgba_t c);
void hud_draw_rect(SDL_Renderer *r, int x, int y, int w, int h, int border, rgba_t c);
void hud_draw_panel(SDL_Renderer *r, int x, int y, int w, int h, rgba_t bg, rgba_t border_col, int cut);
void hud_draw_thick_line(SDL_Renderer *r, int x1, int y1, int x2, int y2, int thickness, rgba_t c);
void hud_fill_circle(SDL_Renderer *r, int cx, int cy, int radius, rgba_t c);
void hud_draw_circle(SDL_Renderer *r, int cx, int cy, int radius, int thickness, rgba_t c);
void hud_draw_text(SDL_Renderer *r, const char *str, int x, int y, int scale, rgba_t c);
void hud_draw_text_centered(SDL_Renderer *r, const char *str, int cx, int y, int scale, rgba_t c);
void hud_draw_text_right(SDL_Renderer *r, const char *str, int rx, int y, int scale, rgba_t c);
void hud_draw_modern_char(SDL_Renderer *r, char ch, int x, int y, int w, int h, int t, rgba_t col);
void hud_draw_modern_number_centered(SDL_Renderer *r, int val, int digits, bool zero_pad, int center_x, int y, int digit_w, int digit_h, int thick, int spacing, rgba_t col);
void hud_draw_segmented_arc(SDL_Renderer *r, int cx, int cy, int radius, int thickness, float start_deg, float end_deg, int num_segs, float fill_pct, rgba_t col_on, rgba_t col_off);
void hud_format_lap_time(int32_t ms, char *out, size_t out_len);
void hud_format_sector_time(int32_t ms, char *out, size_t out_len);
rgba_t hud_tire_temp_color(float temp_c);
char hud_gear_to_char(int gear);
void hud_get_dashboard_clock(char *out_buf, size_t out_sz, bool use_12h);
void hud_get_dashboard_date_strings(char *wday_buf, size_t wday_sz, char *date_buf, size_t date_sz);

/* Inline short-name aliases for clean modular rendering code */
static inline void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, rgba_t c) {
    hud_fill_rect(r, x, y, w, h, c);
}
static inline void draw_rect(SDL_Renderer *r, int x, int y, int w, int h, int border, rgba_t c) {
    hud_draw_rect(r, x, y, w, h, border, c);
}
static inline void draw_panel(SDL_Renderer *r, int x, int y, int w, int h, rgba_t bg, rgba_t border_col, int cut) {
    hud_draw_panel(r, x, y, w, h, bg, border_col, cut);
}
static inline void draw_thick_line(SDL_Renderer *r, int x1, int y1, int x2, int y2, int thickness, rgba_t c) {
    hud_draw_thick_line(r, x1, y1, x2, y2, thickness, c);
}
static inline void fill_circle(SDL_Renderer *r, int cx, int cy, int radius, rgba_t c) {
    hud_fill_circle(r, cx, cy, radius, c);
}
static inline void draw_circle(SDL_Renderer *r, int cx, int cy, int radius, int thickness, rgba_t c) {
    hud_draw_circle(r, cx, cy, radius, thickness, c);
}
static inline void draw_text(SDL_Renderer *r, const char *str, int x, int y, int scale, rgba_t c) {
    hud_draw_text(r, str, x, y, scale, c);
}
static inline void draw_text_centered(SDL_Renderer *r, const char *str, int cx, int y, int scale, rgba_t c) {
    hud_draw_text_centered(r, str, cx, y, scale, c);
}
static inline void draw_text_right(SDL_Renderer *r, const char *str, int rx, int y, int scale, rgba_t c) {
    hud_draw_text_right(r, str, rx, y, scale, c);
}
static inline void draw_modern_char(SDL_Renderer *r, char ch, int x, int y, int w, int h, int t, rgba_t col) {
    hud_draw_modern_char(r, ch, x, y, w, h, t, col);
}
static inline void draw_modern_number_centered(SDL_Renderer *r, int val, int digits, bool zero_pad, int center_x, int y, int digit_w, int digit_h, int thick, int spacing, rgba_t col) {
    hud_draw_modern_number_centered(r, val, digits, zero_pad, center_x, y, digit_w, digit_h, thick, spacing, col);
}
static inline void draw_segmented_arc(SDL_Renderer *r, int cx, int cy, int radius, int thickness, float start_deg, float end_deg, int num_segs, float fill_pct, rgba_t col_on, rgba_t col_off) {
    hud_draw_segmented_arc(r, cx, cy, radius, thickness, start_deg, end_deg, num_segs, fill_pct, col_on, col_off);
}
static inline void format_lap_time(int32_t ms, char *out, size_t out_len) {
    hud_format_lap_time(ms, out, out_len);
}
static inline void format_sector_time(int32_t ms, char *out, size_t out_len) {
    hud_format_sector_time(ms, out, out_len);
}
static inline rgba_t tire_temp_color(float temp_c) {
    return hud_tire_temp_color(temp_c);
}
static inline char gear_to_char(int gear) {
    return hud_gear_to_char(gear);
}
static inline void get_dashboard_clock(char *out_buf, size_t out_sz, bool use_12h) {
    hud_get_dashboard_clock(out_buf, out_sz, use_12h);
}
static inline void get_dashboard_date_strings(char *wday_buf, size_t wday_sz, char *date_buf, size_t date_sz) {
    hud_get_dashboard_date_strings(wday_buf, wday_sz, date_buf, date_sz);
}

/* Luxury EV cockpits module implemented in source/hud_ev_cockpits.c */
void hud_render_theme_ev_cockpit(SDL_Renderer *r, const gt7_state_t *state, const hud_ui_state_t *ui, uint32_t now_ms, int top_y, int bot_y);

#endif /* HUD_INTERNAL_H */
