#include "hud_internal.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static void format_ev_fluid_temp(const gt7_state_t *state, const hud_ui_state_t *ui, const char *prefix, char *out, size_t out_sz) {
    float temp_c = (state->water_temp > 0.0f) ? state->water_temp :
                   ((state->oil_temp > 0.0f) ? state->oil_temp : 86.0f);
    if (ui->use_mph) {
        snprintf(out, out_sz, "%s%.0f°F", prefix, temp_c * 1.8f + 32.0f);
    } else {
        snprintf(out, out_sz, "%s%.0f°C", prefix, temp_c);
    }
}

static int active_ev_speed_limit(const gt7_state_t *state, const hud_ui_state_t *ui) {
    if (state->flags & GT7_FLAG_IN_PIT) {
        return ui->use_mph ? 40 : 60;
    }
    if (state->speed_kmh < 5.0f && state->throttle_pct < 1.0f) {
        return 20;
    }
    return ui->use_mph ? 75 : 120;
}

static void draw_ev_brand_selector(SDL_Renderer *r, const hud_ui_state_t *ui, int top_y) {
    static const char *brands[EV_STYLE_COUNT] = {
        "[+] BENZ MBUX EQ",
        "[#] BMW i-CURVED M",
        "[O] AUDI e-tron GT",
        "[*] VOLVO EX90 / POLESTAR"
    };

    static const rgba_t brand_colors[EV_STYLE_COUNT] = {
        {0, 225, 255, 255},   /* Benz EQ Cyan */
        {0, 160, 255, 255},   /* BMW Electric Blue */
        {235, 45, 55, 255},   /* Audi Sport Red */
        {180, 225, 255, 255}  /* Volvo Scandinavian Ice Blue */
    };

    int bar_y = top_y + 4;
    int bar_h = 32;
    int tab_w = 302;
    int gap = 8;
    int start_x = 24;

    for (int i = 0; i < EV_STYLE_COUNT; ++i) {
        int tx = start_x + i * (tab_w + gap);
        bool active = ((int)ui->ev_style == i);
        rgba_t col = brand_colors[i];

        if (active) {
            fill_rect(r, tx, bar_y, tab_w, bar_h, rgba(col.r / 6, col.g / 6, col.b / 6, 255));
            draw_rect(r, tx, bar_y, tab_w, bar_h, 2, col);
            draw_text_centered(r, brands[i], tx + tab_w / 2, bar_y + 9, 2, col);
            fill_rect(r, tx + 8, bar_y + bar_h - 3, tab_w - 16, 3, col);
        } else {
            fill_rect(r, tx, bar_y, tab_w, bar_h, rgba(0, 0, 0, 255));
            draw_rect(r, tx, bar_y, tab_w, bar_h, 1, rgba(40, 52, 70, 255));
            draw_text_centered(r, brands[i], tx + tab_w / 2, bar_y + 9, 2, rgba(140, 155, 175, 255));
        }
    }
}

/* Helper: Draw simple 1px line */
static void draw_line(SDL_Renderer *r, int x1, int y1, int x2, int y2, rgba_t c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderDrawLine(r, x1, y1, x2, y2);
}

/* Helper: Draw high-contrast Speed Limit sign (European red circle or US rectangle) */
static void draw_speed_limit_sign(SDL_Renderer *r, int cx, int cy, int limit, bool us_style) {
    if (us_style) {
        fill_rect(r, cx - 18, cy - 22, 36, 44, rgba(240, 245, 250, 255));
        draw_rect(r, cx - 18, cy - 22, 36, 44, 2, rgba(20, 20, 20, 255));
        draw_text_centered(r, "LIMIT", cx, cy - 17, 1, rgba(20, 20, 20, 255));
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", limit);
        draw_text_centered(r, buf, cx, cy - 2, 2, rgba(20, 20, 20, 255));
    } else {
        fill_circle(r, cx, cy, 18, rgba(240, 245, 250, 255));
        draw_circle(r, cx, cy, 18, 4, rgba(225, 30, 35, 255));
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", limit);
        draw_text_centered(r, buf, cx, cy - 8, 2, rgba(20, 20, 20, 255));
    }
}

/* Helper: Draw Mercedes MBUX / Classic Dial Radial Sunburst Rays */
static void draw_radial_sunburst_rays(SDL_Renderer *r, int cx, int cy, int r_inner, int r_outer, int count, rgba_t color) {
    for (int i = 0; i < count; ++i) {
        float deg = (float)i * (360.0f / (float)count);
        float rad = deg * 3.14159265f / 180.0f;
        int x0 = cx + (int)(r_inner * cosf(rad));
        int y0 = cy + (int)(r_inner * sinf(rad));
        int x1 = cx + (int)(r_outer * cosf(rad));
        int y1 = cy + (int)(r_outer * sinf(rad));
        draw_line(r, x0, y0, x1, y1, color);
    }
}

/* Helper: Draw glowing needle with dynamic vibrant trailing arc */
static void draw_needle_with_trail(SDL_Renderer *r, int cx, int cy, float deg_start, float deg_curr, int length, int trail_rad, rgba_t needle_col, rgba_t trail_col) {
    if (deg_curr > deg_start + 1.0f) {
        draw_segmented_arc(r, cx, cy, trail_rad, 4, deg_start, deg_curr, 32, 1.0f, trail_col, rgba(0, 0, 0, 0));
    }
    float rad = deg_curr * 3.14159265f / 180.0f;
    float c = cosf(rad);
    float s = sinf(rad);
    int r_hub = 18;
    int x0 = cx + (int)(r_hub * c);
    int y0 = cy + (int)(r_hub * s);
    int tip_x = cx + (int)(length * c);
    int tip_y = cy + (int)(length * s);

    /* Slender tapered glowing laser needle with soft bloom */
    draw_thick_line(r, x0, y0, tip_x, tip_y, 4, rgba(needle_col.r, needle_col.g, needle_col.b, 90));
    draw_thick_line(r, x0, y0, tip_x, tip_y, 2, needle_col);

    /* Center Hub Cap */
    fill_circle(r, cx, cy, 18, rgba(8, 12, 18, 255));
    draw_circle(r, cx, cy, 18, 2, rgba(220, 235, 255, 240));
    draw_circle(r, cx, cy, 8, 1, needle_col);
    fill_circle(r, cx, cy, 4, needle_col);
}

/* Helper: Draw radial tick marks with numerical graduation labels */
static void draw_radial_dial_ticks(SDL_Renderer *r, int cx, int cy, int r_in, int r_out, float start_deg, float end_deg, int num_ticks, int major_step, rgba_t col_major, rgba_t col_minor) {
    if (num_ticks <= 1) return;
    float step = (end_deg - start_deg) / (float)(num_ticks - 1);
    for (int i = 0; i < num_ticks; ++i) {
        float deg = start_deg + i * step;
        float rad = deg * 3.14159265f / 180.0f;
        float c = cosf(rad);
        float s = sinf(rad);
        bool is_major = (major_step > 0) && (i % major_step == 0);
        int cur_in = is_major ? (r_in - 6) : r_in;
        int x0 = cx + (int)(cur_in * c);
        int y0 = cy + (int)(cur_in * s);
        int x1 = cx + (int)(r_out * c);
        int y1 = cy + (int)(r_out * s);
        draw_thick_line(r, x0, y0, x1, y1, is_major ? 3 : 1, is_major ? col_major : col_minor);
    }
}

/* =========================================================================
 * VECTOR ICON DRAWING PRIMITIVES (Zero UTF-8 font corruption, 100% crisp)
 * ========================================================================= */

/* Top-down Car Silhouette with seats / seatbelt indicators */
static void draw_icon_car_topdown(SDL_Renderer *r, int cx, int cy, int w, int h, rgba_t body_col, rgba_t seat_alert_col, bool alert_front_right, bool alert_rear_mid) {
    int x0 = cx - w / 2;
    int y0 = cy - h / 2;
    /* Outer chassis body */
    draw_panel(r, x0, y0, w, h, rgba(10, 18, 30, 255), body_col, 5);
    /* Windshield */
    int ws_y = y0 + (int)(h * 0.22f);
    draw_line(r, x0 + 4, ws_y, x0 + w - 5, ws_y, body_col);
    /* Rear window */
    int rw_y = y0 + (int)(h * 0.72f);
    draw_line(r, x0 + 4, rw_y, x0 + w - 5, rw_y, body_col);
    /* Wheels (4 dark nubs) */
    fill_rect(r, x0 - 2, y0 + 3, 2, 6, rgba(50, 70, 95, 255));
    fill_rect(r, x0 + w, y0 + 3, 2, 6, rgba(50, 70, 95, 255));
    fill_rect(r, x0 - 2, y0 + h - 9, 2, 6, rgba(50, 70, 95, 255));
    fill_rect(r, x0 + w, y0 + h - 9, 2, 6, rgba(50, 70, 95, 255));
    /* Passenger Seats */
    /* Front Driver (left) */
    fill_rect(r, cx - 6, cy - 3, 4, 5, rgba(60, 220, 120, 255));
    /* Front Passenger (right) */
    fill_rect(r, cx + 2, cy - 3, 4, 5, alert_front_right ? seat_alert_col : rgba(60, 220, 120, 255));
    /* Rear Seats (3 seats) */
    fill_rect(r, cx - 7, cy + 5, 3, 4, rgba(60, 220, 120, 255));
    fill_rect(r, cx - 1, cy + 5, 3, 4, alert_rear_mid ? seat_alert_col : rgba(60, 220, 120, 255));
    fill_rect(r, cx + 5, cy + 5, 3, 4, rgba(60, 220, 120, 255));
}

/* Seatbelt Warning Icon */
static void draw_icon_seatbelt(SDL_Renderer *r, int cx, int cy, rgba_t col) {
    /* Head */
    fill_circle(r, cx, cy - 6, 3, col);
    /* Body */
    draw_line(r, cx - 4, cy + 6, cx + 4, cy + 6, col);
    draw_line(r, cx - 4, cy + 6, cx - 2, cy - 1, col);
    draw_line(r, cx + 4, cy + 6, cx + 2, cy - 1, col);
    /* Diagonal Seatbelt sash */
    draw_thick_line(r, cx - 5, cy - 2, cx + 4, cy + 6, 2, col);
}

/* Steering Wheel Assist Icon (MBUX / BMW ADAS) */
static void draw_icon_steering_wheel(SDL_Renderer *r, int cx, int cy, int rad, rgba_t col) {
    draw_circle(r, cx, cy, rad, 2, col);
    fill_circle(r, cx, cy, 3, col);
    /* Left, Right, Bottom spokes */
    draw_line(r, cx - rad + 2, cy, cx - 3, cy, col);
    draw_line(r, cx + 3, cy, cx + rad - 2, cy, col);
    draw_line(r, cx, cy + 3, cx, cy + rad - 2, col);
}

/* Mercedes MBUX Dual-Leaf Eco Icon */
static void draw_icon_leaf(SDL_Renderer *r, int cx, int cy, rgba_t col) {
    /* Left leaf curve */
    draw_thick_line(r, cx - 8, cy - 4, cx, cy + 5, 2, col);
    draw_thick_line(r, cx - 8, cy - 4, cx - 2, cy - 1, 2, col);
    draw_thick_line(r, cx - 2, cy - 1, cx, cy + 5, 2, col);
    /* Right leaf curve */
    draw_thick_line(r, cx + 8, cy - 6, cx, cy + 5, 2, col);
    draw_thick_line(r, cx + 8, cy - 6, cx + 3, cy - 2, 2, col);
    draw_thick_line(r, cx + 3, cy - 2, cx, cy + 5, 2, col);
    /* Stem */
    draw_line(r, cx, cy + 5, cx, cy + 9, col);
}

/* Fuel Pump Icon */
static void draw_icon_fuel_pump(SDL_Renderer *r, int x, int y, rgba_t col) {
    /* Dispenser box */
    draw_rect(r, x, y, 10, 13, 2, col);
    /* Window in pump */
    draw_line(r, x + 2, y + 3, x + 8, y + 3, col);
    /* Hose */
    draw_line(r, x + 10, y + 3, x + 13, y + 5, col);
    draw_line(r, x + 13, y + 5, x + 13, y + 10, col);
    /* Nozzle */
    draw_line(r, x + 13, y + 10, x + 11, y + 12, col);
}

/* High-Voltage EV Battery Icon */
static void __attribute__((unused)) draw_icon_battery(SDL_Renderer *r, int x, int y, int w, int h, float pct, rgba_t col) {
    draw_rect(r, x, y, w, h, 2, col);
    /* Terminal nub */
    fill_rect(r, x + w, y + h / 4, 2, h / 2, col);
    /* Fill segments */
    int fill_w = (int)((w - 4) * pct);
    if (fill_w > 0) {
        fill_rect(r, x + 2, y + 2, fill_w, h - 4, col);
    }
}

/* Headlight Beam Icon */
static void draw_icon_headlight(SDL_Renderer *r, int cx, int cy, rgba_t col) {
    /* Lens dome (left side) */
    draw_circle(r, cx - 2, cy, 6, 2, col);
    fill_rect(r, cx - 2, cy - 7, 7, 14, rgba(0, 0, 0, 255));
    draw_line(r, cx - 2, cy - 6, cx - 2, cy + 6, col);
    /* 3 Rays */
    draw_line(r, cx + 2, cy - 4, cx + 10, cy - 4, col);
    draw_line(r, cx + 2, cy,     cx + 10, cy,     col);
    draw_line(r, cx + 2, cy + 4, cx + 10, cy + 4, col);
}

/* Snowflake Temperature Icon */
static void draw_icon_snowflake(SDL_Renderer *r, int cx, int cy, rgba_t col) {
    /* Cross */
    draw_line(r, cx, cy - 6, cx, cy + 6, col);
    draw_line(r, cx - 6, cy, cx + 6, cy, col);
    /* Diagonals */
    draw_line(r, cx - 4, cy - 4, cx + 4, cy + 4, col);
    draw_line(r, cx - 4, cy + 4, cx + 4, cy - 4, col);
    /* Center point */
    fill_circle(r, cx, cy, 1, col);
}

/* Lane Assist Active Icon (ADAS) */
static void draw_icon_lane_assist(SDL_Renderer *r, int cx, int cy, rgba_t col) {
    /* Left lane line */
    draw_thick_line(r, cx - 14, cy + 8, cx - 7, cy - 8, 2, col);
    /* Right lane line */
    draw_thick_line(r, cx + 14, cy + 8, cx + 7, cy - 8, 2, col);
    /* Small car centered in lane */
    draw_panel(r, cx - 4, cy - 4, 8, 8, rgba(0, 0, 0, 0), col, 2);
}

/* Radar Cruise Distance Icon */
static void draw_icon_radar_distance(SDL_Renderer *r, int cx, int cy, rgba_t col) {
    /* Lead car */
    draw_panel(r, cx - 14, cy - 5, 8, 10, rgba(0, 0, 0, 0), col, 2);
    /* Radar waves */
    draw_line(r, cx - 2, cy - 4, cx, cy, col);
    draw_line(r, cx, cy, cx - 2, cy + 4, col);
    draw_line(r, cx + 2, cy - 6, cx + 5, cy, col);
    draw_line(r, cx + 5, cy, cx + 2, cy + 6, col);
    /* Trailing car */
    draw_panel(r, cx + 8, cy - 5, 8, 10, rgba(0, 0, 0, 0), col, 2);
}

/* 1. MERCEDES-BENZ MBUX CLASSIC COCKPIT (Dual 492px Dials, Horizon Wave, "From Reset" Card) */
static void render_ev_benz_eq(SDL_Renderer *r, const gt7_state_t *state, const hud_ui_state_t *ui, uint32_t now_ms, int content_y, int content_h) {
    (void)now_ms;
    rgba_t text_wh   = rgba(245, 250, 255, 255);
    rgba_t text_dim  = rgba(140, 160, 185, 255);
    rgba_t mb_blue   = rgba(0, 140, 255, 255);
    rgba_t mb_glow   = rgba(0, 110, 240, 85);
    rgba_t mb_red    = rgba(245, 45, 65, 255);
    rgba_t mb_cyan   = rgba(0, 225, 255, 255);
    rgba_t mb_green  = rgba(60, 220, 120, 255);

    /* Ambient Electric Horizon Wave in center */
    for (int y = content_y + 80; y <= content_y + 360; y += 2) {
        float dist = fabsf((float)(y - (content_y + 220))) / 140.0f;
        if (dist < 1.0f) {
            float intensity = 1.0f - dist * dist;
            uint8_t a = (uint8_t)(intensity * 85.0f);
            draw_thick_line(r, 220, y, 1060, y, 2, rgba(0, 130, 255, a));
            if (dist < 0.35f) {
                uint8_t a2 = (uint8_t)((1.0f - dist / 0.35f) * 60.0f);
                draw_thick_line(r, 340, y, 940, y, 2, rgba(0, 225, 255, a2));
            }
        }
    }

    float speed = ui->use_mph ? state->speed_mph : state->speed_kmh;
    float max_spd = ui->use_mph ? 160.0f : 240.0f;
    float spd_frac = speed / max_spd;
    if (spd_frac > 1.0f) spd_frac = 1.0f;

    float rpm = state->engine_rpm;
    float max_rpm = (state->rpm_limiter > 1000) ? (float)state->rpm_limiter : 8000.0f;
    float rpm_frac = rpm / max_rpm;
    if (rpm_frac > 1.0f) rpm_frac = 1.0f;

    int cy = content_y + content_h / 2;
    int dial_rad = 246; /* 492px diameter, filling screen height */

    /* ================= LEFT DIAL: Mercedes Speedometer ================= */
    int left_cx = 246;
    fill_circle(r, left_cx, cy, dial_rad, rgba(0, 0, 0, 255));
    draw_circle(r, left_cx, cy, dial_rad, 2, rgba(235, 245, 255, 255));
    draw_circle(r, left_cx, cy, dial_rad - 2, 1, mb_glow);

    /* Radial Sunburst Rays */
    draw_radial_sunburst_rays(r, left_cx, cy, 32, dial_rad - 26, 72, rgba(30, 75, 125, 80));

    /* Graduation Ticks: outer ticks at perimeter */
    draw_radial_dial_ticks(r, left_cx, cy, dial_rad - 22, dial_rad - 6, 135.0f, 405.0f, 33, 4, rgba(240, 245, 255, 255), rgba(70, 95, 130, 255));

    /* Dynamic Glowing Red Needle & Arc Trail (rendered FIRST so numbers sit on top) */
    float spd_deg = 135.0f + spd_frac * 270.0f;
    draw_needle_with_trail(r, left_cx, cy, 135.0f, spd_deg, dial_rad - 22, dial_rad - 6, mb_red, rgba(255, 45, 75, 180));

    /* Numbers: 0..160 mph or 0..240 km/h (matched 1:1 with needle sweep) */
    static const int mb_spds_mph[9] = {0, 20, 40, 60, 80, 100, 120, 140, 160};
    static const int mb_spds_kmh[9] = {0, 30, 60, 90, 120, 150, 180, 210, 240};
    for (int i = 0; i < 9; ++i) {
        float deg = 135.0f + (float)i * 33.75f;
        float rad = deg * 3.14159265f / 180.0f;
        int tx = left_cx + (int)((dial_rad - 54) * cosf(rad));
        int ty = cy + (int)((dial_rad - 54) * sinf(rad));
        char lbuf[8];
        snprintf(lbuf, sizeof(lbuf), "%d", ui->use_mph ? mb_spds_mph[i] : mb_spds_kmh[i]);
        draw_text_centered(r, lbuf, tx, ty - 6, 2, text_wh);
    }
    draw_text_centered(r, ui->use_mph ? "mph" : "km/h", left_cx, cy - 80, 2, text_dim);
    /* Inner Left Dial Telemetry: Boost Bar & G-Load */
    char l_in[32];
    snprintf(l_in, sizeof(l_in), "BOOST %+.2fB", state->turbo_bar);
    draw_text_centered(r, l_in, left_cx, cy + 44, 1, mb_cyan);
    snprintf(l_in, sizeof(l_in), "G-LOAD %.2fG", sqrtf(state->g_lat * state->g_lat + state->g_lon * state->g_lon));
    draw_text_centered(r, l_in, left_cx, cy + 60, 1, text_dim);

    /* Dynamic Fuel Gauge at bottom of left dial */
    int fuel_y = cy + dial_rad - 56;
    int cur_range = (int)(state->fuel_pct * (ui->use_mph ? 3.8f : 6.1f));
    char f_lbl[24];
    snprintf(f_lbl, sizeof(f_lbl), "%d %s", cur_range, ui->use_mph ? "mi" : "km");
    draw_icon_fuel_pump(r, left_cx - 50, fuel_y - 12, text_wh);
    draw_text(r, f_lbl, left_cx - 26, fuel_y - 10, 1, text_wh);
    draw_text(r, "0", left_cx - 48, fuel_y + 8, 1, text_dim);
    draw_text(r, "1/2", left_cx - 8, fuel_y + 14, 1, text_dim);
    draw_text(r, "1", left_cx + 36, fuel_y + 8, 1, text_dim);
    draw_segmented_arc(r, left_cx, cy, dial_rad - 16, 4, 250.0f, 290.0f, 10, state->fuel_pct / 100.0f, text_wh, rgba(40, 60, 85, 255));

    /* ================= RIGHT DIAL: Mercedes Tachometer & Power ================= */
    int right_cx = SCREEN_W - 246;
    fill_circle(r, right_cx, cy, dial_rad, rgba(0, 0, 0, 255));
    draw_circle(r, right_cx, cy, dial_rad, 2, rgba(235, 245, 255, 255));
    draw_circle(r, right_cx, cy, dial_rad - 2, 1, mb_glow);

    /* Radial Sunburst Rays */
    draw_radial_sunburst_rays(r, right_cx, cy, 32, dial_rad - 26, 72, rgba(30, 75, 125, 80));

    /* Graduation Ticks */
    draw_radial_dial_ticks(r, right_cx, cy, dial_rad - 22, dial_rad - 6, 135.0f, 405.0f, 33, 4, rgba(240, 245, 255, 255), rgba(70, 95, 130, 255));

    /* Dynamic Glowing Red Needle & Arc Trail (rendered FIRST so numbers sit on top) */
    float rpm_deg = 135.0f + rpm_frac * 270.0f;
    draw_needle_with_trail(r, right_cx, cy, 135.0f, rpm_deg, dial_rad - 22, dial_rad - 6, mb_red, rgba(255, 45, 75, 180));

    /* Numbers: 0, 1, 2, 3, 4, 5, 6, 7, 8 (staggered inside ticks/trail) */
    for (int i = 0; i <= 8; ++i) {
        float deg = 135.0f + (float)i * 33.75f;
        float rad = deg * 3.14159265f / 180.0f;
        int tx = right_cx + (int)((dial_rad - 54) * cosf(rad));
        int ty = cy + (int)((dial_rad - 54) * sinf(rad));
        char lbuf[8];
        snprintf(lbuf, sizeof(lbuf), "%d", i);
        draw_text_centered(r, lbuf, tx, ty - 6, 2, (i >= 7) ? mb_red : text_wh);
    }
    draw_text_centered(r, "x1000/min", right_cx, cy - 80, 1, text_dim);
    /* Inner Right Dial Telemetry: Engine RPM & Coolant/Oil Temps */
    snprintf(l_in, sizeof(l_in), "%.0f RPM", state->engine_rpm);
    draw_text_centered(r, l_in, right_cx, cy + 44, 1, mb_cyan);
    snprintf(l_in, sizeof(l_in), "H2O %.0fC | OIL %.0fC", state->water_temp, state->oil_temp);
    draw_text_centered(r, l_in, right_cx, cy + 60, 1, text_dim);

    /* POWER & CHARGE readouts at bottom of right dial */
    int pwr_lbl_y = cy + dial_rad - 48;
    draw_text(r, "POWER", right_cx - 48, pwr_lbl_y, 1, text_wh);
    draw_text(r, "CHARGE", right_cx + 12, pwr_lbl_y, 1, mb_cyan);
    if (state->energy_recovery_kw < -1.0f) {
        char kw_buf[16];
        snprintf(kw_buf, sizeof(kw_buf), "%d kW", (int)fabsf(state->energy_recovery_kw));
        draw_text_centered(r, kw_buf, right_cx + 34, pwr_lbl_y + 14, 1, mb_cyan);
    } else if (state->energy_recovery_kw > 1.0f) {
        char kw_buf[16];
        snprintf(kw_buf, sizeof(kw_buf), "%d kW", (int)state->energy_recovery_kw);
        draw_text_centered(r, kw_buf, right_cx - 28, pwr_lbl_y + 14, 1, text_wh);
    }

    /* Dynamic Vertical Gear Selector Column & Driving Mode Pill (outside right dial) */
    int gr_x = 1208;
    int gr_y = content_y + content_h - 48;
    bool is_p = (state->speed_kmh < 0.5f && state->current_gear < 0 && state->throttle_pct < 1.0f && state->engine_rpm < 1400.0f);
    bool is_r = (state->current_gear == 0);
    bool is_n = (state->current_gear == -1 && !is_p);
    bool is_d = (state->current_gear >= 1);

    draw_text(r, "R", gr_x, gr_y - 24, 1, is_r ? rgba(255, 195, 45, 255) : text_dim);
    draw_text(r, "N", gr_x, gr_y - 12, 1, is_n ? text_wh : text_dim);
    draw_text(r, "D", gr_x, gr_y,      1, is_d ? text_wh : text_dim);

    if (is_p) {
        draw_modern_char(r, 'P', gr_x + 14, gr_y - 22, 16, 24, 4, text_wh);
    } else if (is_r) {
        draw_modern_char(r, 'R', gr_x + 14, gr_y - 22, 16, 24, 4, rgba(255, 195, 45, 255));
    } else if (is_n) {
        draw_modern_char(r, 'N', gr_x + 14, gr_y - 22, 16, 24, 4, text_wh);
    } else {
        int disp_g = (state->current_gear > 9) ? 9 : state->current_gear;
        draw_modern_char(r, 'D', gr_x + 12, gr_y - 22, 14, 24, 4, text_wh);
        draw_modern_char(r, (char)('0' + disp_g), gr_x + 28, gr_y - 20, 12, 20, 3, mb_cyan);
    }

    /* Dynamic Drive Mode Pill: [ S ] when throttle > 75%, otherwise [ C ] */
    bool is_sport = (state->throttle_pct > 75.0f);
    fill_rect(r, gr_x + 10, gr_y + 10, 32, 18, is_sport ? rgba(90, 15, 25, 255) : rgba(0, 45, 80, 255));
    draw_rect(r, gr_x + 10, gr_y + 10, 32, 18, 1, is_sport ? mb_red : mb_cyan);
    draw_text_centered(r, is_sport ? "S" : "C", gr_x + 26, gr_y + 12, 1, is_sport ? mb_red : mb_cyan);

    /* ================= CENTER TOP: Speed Limit, ADAS, Digital Speed ================= */
    int mid_cx = SCREEN_W / 2;
    int top_row_y = content_y + 4;

    /* Speed Limit Sign: LIMIT 50 */
    draw_speed_limit_sign(r, mid_cx - 150, top_row_y + 20, 50, true);

    /* Radar Cruise Distance Icon */
    draw_icon_radar_distance(r, mid_cx - 95, top_row_y + 20, text_dim);

    /* Center Modern Digital Speed */
    int ispd = (int)roundf(speed);
    draw_modern_number_centered(r, ispd, 1, false, mid_cx, top_row_y + 6, 26, 42, 6, 6, text_wh);
    draw_text_centered(r, ui->use_mph ? "mph" : "km/h", mid_cx, top_row_y + 52, 1, text_dim);

    /* Steering & Lane Assist Icons */
    draw_icon_steering_wheel(r, mid_cx + 76, top_row_y + 20, 10, mb_green);
    draw_icon_lane_assist(r, mid_cx + 120, top_row_y + 20, text_dim);

    /* Red Seatbelt Warning cleanly positioned to the right of Lane Assist (zero overlap with speed) */
    draw_icon_seatbelt(r, mid_cx + 162, top_row_y + 20, mb_red);

    /* ================= CENTER FLOATING CARD: "From Reset" / "ENTIRE" ================= */
    int card_w = 272;
    int card_h = 132;
    int card_x = mid_cx - card_w / 2;
    int card_y = content_y + 82;

    draw_text_centered(r, "From Reset", mid_cx, card_y - 16, 2, text_dim);
    draw_panel(r, card_x, card_y, card_w, card_h, rgba(0, 0, 0, 255), rgba(0, 195, 255, 220), 12);
    draw_text_centered(r, "ENTIRE", mid_cx, card_y + 8, 1, mb_cyan);

    char c_buf[32];
    /* Row 1: Real-time Fuel Level & Current/Top Speed */
    snprintf(c_buf, sizeof(c_buf), "%.1f L", (state->fuel_level > 0.0f) ? state->fuel_level : (state->fuel_pct * 0.65f));
    draw_text(r, c_buf, card_x + 18, card_y + 26, 2, text_wh);
    snprintf(c_buf, sizeof(c_buf), "%.0f %s", speed, ui->use_mph ? "mph" : "km/h");
    draw_text(r, c_buf, card_x + 154, card_y + 26, 2, text_wh);

    /* Row 2: Live Trip Distance & Lap Timer */
    float trip_dist = (state->session_distance_km > 0.05f) ?
                      (ui->use_mph ? state->session_distance_km * 0.621371f : state->session_distance_km) :
                      ((float)state->current_lap * (ui->use_mph ? 2.6f : 4.2f) + ((float)state->current_lap_ms / 60000.0f) * (speed * 0.012f));
    snprintf(c_buf, sizeof(c_buf), "%.1f %s", trip_dist, ui->use_mph ? "mi" : "km");
    draw_text(r, c_buf, card_x + 18, card_y + 60, 2, text_wh);
    int lap_sec = (state->current_lap_ms > 0) ? (int)(state->current_lap_ms / 1000) : (int)((now_ms / 1000) % 3600);
    snprintf(c_buf, sizeof(c_buf), "%02d:%02d", lap_sec / 60, lap_sec % 60);
    draw_text(r, c_buf, card_x + 154, card_y + 60, 2, text_wh);

    /* Row 3: Remaining Dynamic Range & Race Position */
    int rem_range = (int)(state->fuel_pct * (ui->use_mph ? 3.8f : 6.1f));
    snprintf(c_buf, sizeof(c_buf), "%d %s", rem_range, ui->use_mph ? "mi" : "km");
    draw_text(r, c_buf, card_x + 18, card_y + 94, 2, mb_cyan);
    draw_icon_leaf(r, mid_cx, card_y + 102, mb_green);
    snprintf(c_buf, sizeof(c_buf), "POS %d/%d", (state->race_position > 0) ? state->race_position : 1, (state->total_cars > 0) ? state->total_cars : 16);
    draw_text(r, c_buf, card_x + 154, card_y + 94, 2, text_wh);

    /* ================= CENTER BELOW CARD: Seat Occupancy Graphic, AWD Split & AMG TRACK PACE ================= */
    int occ_w = 248, occ_h = 22, occ_x = mid_cx - occ_w / 2, occ_y = card_y + card_h + 6;
    draw_panel(r, occ_x, occ_y, occ_w, occ_h, rgba(0, 0, 0, 255), rgba(35, 70, 115, 255), 8);
    draw_icon_car_topdown(r, occ_x + 42, occ_y + 11, 16, 18, rgba(50, 95, 145, 255), mb_red, false, false);
    draw_thick_line(r, occ_x + 70, occ_y + 11, occ_x + 95, occ_y + 11, 3, mb_red);
    fill_rect(r, occ_x + 95, occ_y + 6, 8, 10, mb_red);
    snprintf(c_buf, sizeof(c_buf), "STEER %+.1fD", state->steer_angle_fl_deg);
    draw_text_right(r, c_buf, occ_x + occ_w - 10, occ_y + 5, 1, mb_cyan);

    int bot_card_y = occ_y + occ_h + 6;
    draw_panel(r, mid_cx - 136, bot_card_y, 272, 46, rgba(0, 0, 0, 255), rgba(35, 70, 115, 255), 8);
    char awd_buf[64];
    snprintf(awd_buf, sizeof(awd_buf), "4MATIC: F %d%% | R %d%%",
             (state->torque_vector_nm[0] > 0.1f || state->torque_vector_nm[2] > 0.1f) ?
             (int)(100.0f * (state->torque_vector_nm[0] + state->torque_vector_nm[1]) / (state->torque_vector_nm[0] + state->torque_vector_nm[1] + state->torque_vector_nm[2] + state->torque_vector_nm[3] + 0.1f)) : 46,
             (state->torque_vector_nm[0] > 0.1f || state->torque_vector_nm[2] > 0.1f) ?
             (int)(100.0f * (state->torque_vector_nm[2] + state->torque_vector_nm[3]) / (state->torque_vector_nm[0] + state->torque_vector_nm[1] + state->torque_vector_nm[2] + state->torque_vector_nm[3] + 0.1f)) : 54);
    draw_text_centered(r, awd_buf, mid_cx, bot_card_y + 6, 2, mb_cyan);

    char sc_fl = (state->surface_type[0] != 0) ? (char)state->surface_type[0] : 'T';
    char sc_fr = (state->surface_type[1] != 0) ? (char)state->surface_type[1] : 'T';
    char sc_rl = (state->surface_type[2] != 0) ? (char)state->surface_type[2] : 'T';
    char sc_rr = (state->surface_type[3] != 0) ? (char)state->surface_type[3] : 'T';
    snprintf(awd_buf, sizeof(awd_buf), "FL %.0f°[%c] FR %.0f°[%c] RL %.0f°[%c] RR %.0f°[%c]",
             state->tire_temp[0], sc_fl, state->tire_temp[1], sc_fr,
             state->tire_temp[2], sc_rl, state->tire_temp[3], sc_rr);
    draw_text_centered(r, awd_buf, mid_cx, bot_card_y + 28, 1, text_wh);

    /* AMG TRACK PACE Lap Delta & Pedal Telemetry Card (fills lower-center space on True OLED Black) */
    int pace_y = bot_card_y + 52;
    draw_panel(r, mid_cx - 136, pace_y, 272, 68, rgba(0, 0, 0, 255), rgba(0, 165, 235, 190), 8);
    draw_text(r, "AMG TRACK PACE", mid_cx - 124, pace_y + 6, 1, mb_cyan);
    snprintf(c_buf, sizeof(c_buf), "%s%.3fs", (state->delta_ms <= 0) ? "-" : "+", fabsf((float)state->delta_ms / 1000.0f));
    draw_text_right(r, c_buf, mid_cx + 124, pace_y + 6, 1, (state->delta_ms <= 0) ? mb_green : mb_red);

    char best_b[24], s_b[64];
    format_lap_time(state->best_lap_ms, best_b, sizeof(best_b));
    snprintf(s_b, sizeof(s_b), "BEST %s  |  S%d ACTIVE", best_b, state->current_sector + 1);
    draw_text_centered(r, s_b, mid_cx, pace_y + 22, 1, text_wh);

    /* Dual THR / BRK bars inside AMG Track Pace card */
    draw_text(r, "THR", mid_cx - 124, pace_y + 39, 1, mb_green);
    fill_rect(r, mid_cx - 94, pace_y + 40, 88, 7, rgba(16, 26, 38, 255));
    fill_rect(r, mid_cx - 94, pace_y + 40, (int)(88.0f * state->throttle_pct / 100.0f), 7, mb_green);

    draw_text(r, "BRK", mid_cx + 6, pace_y + 39, 1, mb_red);
    fill_rect(r, mid_cx + 36, pace_y + 40, 88, 7, rgba(28, 16, 22, 255));
    fill_rect(r, mid_cx + 36, pace_y + 40, (int)(88.0f * state->brake_pct / 100.0f), 7, mb_red);

    snprintf(s_b, sizeof(s_b), "SUSP %.0f/%.0f/%.0f/%.0fMM  RH %.0fMM",
             state->susp_height[0], state->susp_height[1],
             state->susp_height[2], state->susp_height[3], state->body_height);
    draw_text_centered(r, s_b, mid_cx, pace_y + 52, 1, text_dim);

    int status_y = content_y + content_h - 22;
    char mb_clock[16];
    get_dashboard_clock(mb_clock, sizeof(mb_clock), true);
    draw_text(r, mb_clock, 345, status_y, 2, text_dim);
    if (is_p) {
        draw_text(r, "PARK", 525, status_y, 2, mb_red);
        fill_rect(r, 625, status_y - 2, 22, 20, mb_blue);
        draw_text_centered(r, "P", 636, status_y, 2, text_wh);
    } else {
        draw_text(r, "READY", 520, status_y, 2, mb_green);
        char lap_buf[16];
        snprintf(lap_buf, sizeof(lap_buf), "LAP %d", (state->current_lap > 0) ? state->current_lap : 1);
        draw_text(r, lap_buf, 620, status_y, 2, text_wh);
    }
    char mb_temp_buf[24];
    format_ev_fluid_temp(state, ui, "H2O ", mb_temp_buf, sizeof(mb_temp_buf));
    draw_icon_snowflake(r, 706, status_y + 7, text_dim);
    draw_text(r, mb_temp_buf, 718, status_y, 2, text_dim);
}

/* Motorsport chamfered panel with glowing header accent strip */

/* --- LUXURY OEM VECTOR ICONS (BMW OS 8.5 & VOLVO EX90) --- */
static void draw_vector_fuel_pump(SDL_Renderer *r, int x, int y, int h, rgba_t c) {
    int w = (h * 7) / 10;
    draw_rect(r, x, y, w, h, 1, c);
    fill_rect(r, x + 2, y + 2, w - 4, h / 3, c);
    fill_rect(r, x - 1, y + h - 2, w + 2, 2, c);
    /* Hose */
    draw_thick_line(r, x + w, y + 3, x + w + 3, y + 3, 1, c);
    draw_thick_line(r, x + w + 3, y + 3, x + w + 3, y + h - 3, 1, c);
    draw_thick_line(r, x + w + 3, y + h - 3, x + w + 1, y + h - 4, 1, c);
}

static void draw_vector_battery(SDL_Renderer *r, int x, int y, int w, int h, int fill_pct, rgba_t border_col, rgba_t fill_col) {
    draw_rect(r, x, y, w, h, 1, border_col);
    fill_rect(r, x + w, y + h / 4, 2, h / 2, border_col);
    int inner_w = w - 4;
    int cur_w = (inner_w * fill_pct) / 100;
    if (cur_w > inner_w) cur_w = inner_w;
    if (cur_w > 0) {
        fill_rect(r, x + 2, y + 2, cur_w, h - 4, fill_col);
    }
}

static void draw_topdown_car(SDL_Renderer *r, int cx, int cy, rgba_t c) {
    draw_rect(r, cx - 7, cy - 12, 14, 24, 1, c);
    draw_rect(r, cx - 5, cy - 5, 10, 10, 1, c);
    draw_thick_line(r, cx - 5, cy - 5, cx + 5, cy - 5, 1, c);
    fill_rect(r, cx - 9, cy - 10, 2, 5, c);
    fill_rect(r, cx + 7, cy - 10, 2, 5, c);
    fill_rect(r, cx - 9, cy + 5, 2, 5, c);
    fill_rect(r, cx + 7, cy + 5, 2, 5, c);
}

static void draw_dashed_progress_bar(SDL_Renderer *r, int x, int y, int segments, int filled_segs, int seg_w, int seg_h, int gap, rgba_t fill_col, rgba_t empty_col) {
    for (int i = 0; i < segments; i++) {
        rgba_t col = (i < filled_segs) ? fill_col : empty_col;
        fill_rect(r, x + i * (seg_w + gap), y, seg_w, seg_h, col);
    }
}

/* Authentic Electric Vehicle Charging Plug Vector Icon (BMW i / M-EV) */
static void draw_icon_ev_plug(SDL_Renderer *r, int cx, int cy, rgba_t col) {
    /* Two prongs */
    draw_line(r, cx - 3, cy - 6, cx - 3, cy - 2, col);
    draw_line(r, cx + 1, cy - 6, cx + 1, cy - 2, col);
    /* Body */
    draw_rect(r, cx - 5, cy - 2, 8, 7, 1, col);
    fill_rect(r, cx - 4, cy - 1, 6, 5, col);
    /* Cable */
    draw_line(r, cx - 1, cy + 5, cx - 1, cy + 9, col);
}

/* Official BMW ///M Tri-Color Emblem */
static void draw_bmw_m_logo(SDL_Renderer *r, int cx, int cy) {
    rgba_t col_light_blue = rgba(0, 163, 224, 255);
    rgba_t col_dark_blue  = rgba(0, 43, 110, 255);
    rgba_t col_red        = rgba(230, 26, 35, 255);
    rgba_t col_white      = rgba(245, 250, 255, 255);

    int sx = cx - 30;
    int sy = cy - 8;
    int h = 18;
    for (int dy = 0; dy < h; dy++) {
        int x_off = (h - dy) / 3;
        fill_rect(r, sx + x_off, sy + dy, 6, 1, col_light_blue);
        fill_rect(r, sx + x_off + 8, sy + dy, 6, 1, col_dark_blue);
        fill_rect(r, sx + x_off + 16, sy + dy, 7, 1, col_red);
    }
    draw_modern_char(r, 'M', sx + 28, sy - 2, 18, 22, 3, col_white);
}

/* 3D Perspective BMW Vehicle Rendering in Lane (Assisted Driving Highway View) */
static void draw_bmw_3d_car(SDL_Renderer *r, int cx, int cy) {
    /* Shadow underneath tires */
    fill_rect(r, cx - 64, cy + 22, 128, 6, rgba(0, 0, 0, 160));
    fill_rect(r, cx - 54, cy + 24, 108, 4, rgba(0, 0, 0, 180));

    /* Lower chassis / door panel with metallic gradient */
    draw_panel(r, cx - 60, cy - 6, 120, 26, rgba(235, 240, 250, 255), rgba(175, 190, 210, 255), 4);
    fill_rect(r, cx - 58, cy + 8, 116, 8, rgba(215, 222, 235, 255));
    /* Aerodynamic rocker skirt */
    fill_rect(r, cx - 54, cy + 18, 108, 3, rgba(40, 50, 65, 255));

    /* Roof greenhouse & pillars */
    draw_panel(r, cx - 36, cy - 25, 76, 21, rgba(20, 26, 38, 255), rgba(160, 180, 205, 255), 6);
    /* Tinted Windows glass */
    fill_rect(r, cx - 31, cy - 21, 33, 14, rgba(45, 75, 110, 230));
    fill_rect(r, cx + 6,  cy - 21, 30, 14, rgba(45, 75, 110, 230));
    /* B-pillar separator */
    fill_rect(r, cx + 3,  cy - 21, 3, 14, rgba(25, 30, 42, 255));

    /* Hood and trunk slope bevels */
    draw_thick_line(r, cx - 60, cy - 6, cx - 36, cy - 21, 2, rgba(245, 250, 255, 255));
    draw_thick_line(r, cx + 60, cy - 4, cx + 41, cy - 21, 2, rgba(245, 250, 255, 255));

    /* Front LED Laser Headlight & Rear LED L-Shape Taillight */
    fill_rect(r, cx + 56, cy - 2, 5, 5, rgba(0, 230, 255, 255));
    fill_circle(r, cx + 58, cy, 3, rgba(0, 210, 255, 160));
    fill_rect(r, cx - 60, cy - 2, 4, 5, rgba(255, 35, 35, 255));
    fill_circle(r, cx - 59, cy, 3, rgba(255, 35, 35, 160));

    /* Front wheel & Rear wheel (BMW M Style multi-spoke alloy) */
    fill_circle(r, cx - 36, cy + 18, 11, rgba(16, 20, 28, 255));
    draw_circle(r, cx - 36, cy + 18, 11, 2, rgba(170, 185, 205, 255));
    fill_circle(r, cx - 36, cy + 18, 4, rgba(210, 225, 245, 255));

    fill_circle(r, cx + 36, cy + 18, 11, rgba(16, 20, 28, 255));
    draw_circle(r, cx + 36, cy + 18, 11, 2, rgba(170, 185, 205, 255));
    fill_circle(r, cx + 36, cy + 18, 4, rgba(210, 225, 245, 255));

    /* Distinctive BMW Hofmeister kink & shoulder character line */
    draw_line(r, cx - 54, cy + 4, cx + 54, cy + 4, rgba(160, 185, 215, 255));
    draw_line(r, cx + 37, cy - 14, cx + 42, cy - 8, rgba(235, 240, 250, 255));
}

/* Batched crystal glass chevron fill + faceted horizontal crystal highlights */
static void draw_bmw_chevron_glass(SDL_Renderer *r, int content_y, int top_y, int mid_y_top, int mid_y_bot, int bot_y,
                                   int elb_x, int roof_x1, int floor_x1, bool is_right,
                                   rgba_t blade_glass, rgba_t crystal_line) {
    SDL_Rect glass_rects[512];
    SDL_Rect line_rects[64];
    int n_glass = 0;
    int n_lines = 0;

    for (int y = top_y; y <= bot_y; ++y) {
        int ox;
        if (!is_right) {
            if (y < mid_y_top) {
                ox = elb_x + (mid_y_top - y) * (roof_x1 - elb_x) / (mid_y_top - top_y);
            } else if (y <= mid_y_bot) {
                ox = elb_x;
            } else {
                ox = elb_x + (y - mid_y_bot) * (floor_x1 - elb_x) / (bot_y - mid_y_bot);
            }
        } else {
            if (y < mid_y_top) {
                ox = elb_x - (mid_y_top - y) * (elb_x - roof_x1) / (mid_y_top - top_y);
            } else if (y <= mid_y_bot) {
                ox = elb_x;
            } else {
                ox = elb_x - (y - mid_y_bot) * (elb_x - floor_x1) / (bot_y - mid_y_bot);
            }
            ox -= 46;
        }
        glass_rects[n_glass++] = (SDL_Rect){ox, y, 46, 1};
        if (n_glass == 512) {
            SDL_SetRenderDrawColor(r, blade_glass.r, blade_glass.g, blade_glass.b, blade_glass.a);
            SDL_RenderFillRects(r, glass_rects, n_glass);
            n_glass = 0;
        }
        if ((y - content_y) % 18 == 0) {
            line_rects[n_lines++] = (SDL_Rect){ox, y, 47, 1};
            if (n_lines == 64) {
                SDL_SetRenderDrawColor(r, crystal_line.r, crystal_line.g, crystal_line.b, crystal_line.a);
                SDL_RenderFillRects(r, line_rects, n_lines);
                n_lines = 0;
            }
        }
    }
    if (n_glass > 0) {
        SDL_SetRenderDrawColor(r, blade_glass.r, blade_glass.g, blade_glass.b, blade_glass.a);
        SDL_RenderFillRects(r, glass_rects, n_glass);
    }
    if (n_lines > 0) {
        SDL_SetRenderDrawColor(r, crystal_line.r, crystal_line.g, crystal_line.b, crystal_line.a);
        SDL_RenderFillRects(r, line_rects, n_lines);
    }
}

/* =========================================================================
 * 2. BMW CURVED DISPLAY OS 8.5/9 (MULTI-STYLE SUITE: M DYNAMIC, 3D MAP, RADAR, REDUCED)
 * Directly matching production photos 1, 2, 3, 4, and 5 with authentic geometry,
 * isometric triangle mesh, dual stacked speed (mph/kmh), CraftedClarity crystal facets,
 * and comprehensive Gran Turismo 7 motorsport telemetry integration.
 * ========================================================================= */
static void render_ev_bmw_i(SDL_Renderer *r, const gt7_state_t *state, const hud_ui_state_t *ui, uint32_t now_ms, int content_y, int content_h) {
    (void)now_ms;
    rgba_t bmw_cyan      = rgba(0, 220, 255, 255);
    rgba_t text_dim      = rgba(135, 155, 180, 255);
    rgba_t text_wh       = rgba(245, 250, 255, 255);
    rgba_t chevron_outer = rgba(255, 255, 255, 255);
    rgba_t m_blue        = rgba(0, 163, 224, 255);
    rgba_t m_dark_blue   = rgba(0, 43, 110, 255);
    rgba_t m_red         = rgba(230, 26, 35, 255);
    rgba_t gold          = rgba(245, 190, 40, 255);

    /* Background: True OLED Pitch Black (#000000) */
    fill_rect(r, 0, content_y, 1280, content_h, rgba(0, 0, 0, 255));

    /* --- TOP SUB-MODE SELECTOR TABS (x: 410 to 870, y: content_y + 4) --- */
    static const struct {
        const char *name;
        int x;
        int w;
        bmw_view_t view;
    } sub_tabs[] = {
        { "///M SPORT", 410, 105, BMW_VIEW_M_DYNAMIC },
        { "3D MAP",    525,  95, BMW_VIEW_MAP_COCKPIT },
        { "RADAR",     630,  95, BMW_VIEW_ASSIST_RADAR },
        { "REDUCED",   735, 105, BMW_VIEW_MINIMAL_MAP }
    };
    for (size_t t = 0; t < sizeof(sub_tabs) / sizeof(sub_tabs[0]); t++) {
        bool active = (ui->bmw_view == sub_tabs[t].view);
        int tx = sub_tabs[t].x;
        int ty = content_y + 4;
        int tw = sub_tabs[t].w;
        int th = 22;
        if (active) {
            fill_rect(r, tx, ty, tw, th, rgba(0, 110, 210, 180));
            draw_rect(r, tx, ty, tw, th, 1, bmw_cyan);
            draw_text_centered(r, sub_tabs[t].name, tx + tw / 2, ty + 4, 1, text_wh);
        } else {
            fill_rect(r, tx, ty, tw, th, rgba(10, 18, 30, 120));
            draw_rect(r, tx, ty, tw, th, 1, rgba(40, 70, 105, 100));
            draw_text_centered(r, sub_tabs[t].name, tx + tw / 2, ty + 4, 1, text_dim);
        }
    }

    /* Common speed variables */
    int spd_pri = ui->use_mph ? (int)(state->speed_mph + 0.5f) : (int)(state->speed_kmh + 0.5f);
    int spd_sec = ui->use_mph ? (int)(state->speed_kmh + 0.5f) : (int)(state->speed_mph + 0.5f);
    if (spd_pri < 0) spd_pri = 0;
    if (spd_sec < 0) spd_sec = 0;

    /* Common gear letter */
    char gear_ch = 'D';
    if (state->current_gear == 0) {
        gear_ch = 'R';
    } else if (state->current_gear > 0) {
        int disp_g = (state->current_gear > 9) ? 9 : state->current_gear;
        gear_ch = (char)('0' + disp_g);
    } else if (state->speed_kmh < 0.2f && state->throttle_pct < 0.5f && state->engine_rpm < 1400.0f) {
        gear_ch = 'P';
    } else {
        gear_ch = 'N';
    }

    /* Common session trip distance (accumulated from real driving telemetry) */
    float trip_km = (state->session_distance_km > 0.01f) ?
                    (1503.7f + state->session_distance_km) :
                    ((state->current_lap > 0) ? (1503.7f + state->current_lap * 5.807f + state->speed_kmh * 0.02f) : 1503.7f);
    char trip_str[32];
    snprintf(trip_str, sizeof(trip_str), "%.1f km", trip_km);

    /* =========================================================================
     * VIEW 1: BMW ///M DYNAMIC / SPORT COCKPIT (Photos 1 & 2)
     * Dual Symmetrical Chevrons, Central 3D Isometric Triangular Wireframe Grid,
     * Top ///M Logo, Center Stacked Speed (mph over km/h), Dynamic Red/Cyan Neon Wedges,
     * Lower Center Gear, and Rich Motorsport Telemetry across open flanks.
     * ========================================================================= */
    if (ui->bmw_view == BMW_VIEW_M_DYNAMIC) {
        int top_y = content_y + 40;
        int bot_y = content_y + content_h - 55;
        int mid_y_top = content_y + 195;
        int mid_y_bot = content_y + 265;

        /* Authentic chevron geometry matching Photo 1 */
        int l_elb_x = 240;
        int l_roof_x1 = 375, l_roof_x2 = 430;
        int l_floor_x1 = 375, l_floor_x2 = 430;

        int r_elb_x = 1280 - 240; /* 1040 */
        int r_roof_x1 = 1280 - 375, r_roof_x2 = 1280 - 430; /* 905 to 850 */
        int r_floor_x1 = 1280 - 375, r_floor_x2 = 1280 - 430; /* 905 to 850 */

        /* 1. CENTRAL ISOMETRIC TRIANGULAR WIREFRAME LATTICE GRID (between chevrons) */
        int grid_x1 = 435, grid_x2 = 845;
        int grid_y1 = content_y + 45, grid_y2 = content_y + 475;
        int tri_h = 28, tri_w = 32;
        rgba_t grid_col = rgba(0, 110, 210, 75);
        rgba_t node_col = rgba(0, 190, 255, 120);

        for (int gy = grid_y1; gy <= grid_y2; gy += tri_h) {
            draw_thick_line(r, grid_x1, gy, grid_x2, gy, 1, grid_col);
        }
        for (int gx = grid_x1 - 160; gx <= grid_x2 + 160; gx += tri_w) {
            int dx_diag = (grid_y2 - grid_y1) * tri_w / (2 * tri_h);
            draw_thick_line(r, gx, grid_y1, gx + dx_diag, grid_y2, 1, grid_col);
            draw_thick_line(r, gx, grid_y1, gx - dx_diag, grid_y2, 1, grid_col);
        }
        for (int row = 0; row <= (grid_y2 - grid_y1) / tri_h; row++) {
            int gy = grid_y1 + row * tri_h;
            int x_off = (row % 2 == 0) ? 0 : tri_w / 2;
            for (int gx = grid_x1 + x_off; gx <= grid_x2; gx += tri_w) {
                fill_circle(r, gx, gy, 1, node_col);
            }
        }

        /* 2. CRAFTED-CLARITY CRYSTAL GLASS BLADE (Inside Left & Right Chevrons, batched SDL rects) */
        rgba_t blade_glass = rgba(10, 24, 52, 160);
        rgba_t crystal_line = rgba(0, 190, 255, 45);
        draw_bmw_chevron_glass(r, content_y, top_y, mid_y_top, mid_y_bot, bot_y,
                               l_elb_x, l_roof_x1, l_floor_x1, false, blade_glass, crystal_line);
        draw_bmw_chevron_glass(r, content_y, top_y, mid_y_top, mid_y_bot, bot_y,
                               r_elb_x, r_roof_x1, r_floor_x1, true, blade_glass, crystal_line);

        /* 3. DYNAMIC RADIANT NEON WEDGES (Fiery Red-Orange in Dynamic / Cyan in Eco) */
        bool is_dynamic_flare = (state->throttle_pct > 25.0f || state->turbo_bar > 0.0f || state->engine_rpm > 4000 || state->speed_kmh > 70.0f);
        for (int offset = 10; offset <= 48; offset += 2) {
            float t = (float)(offset - 10) / 38.0f;
            int a = (int)((1.0f - t * t) * (is_dynamic_flare ? 220.0f : 175.0f));
            if (a < 12) a = 12;
            rgba_t w_col;
            if (is_dynamic_flare) {
                if (offset <= 18) w_col = rgba(255, 225, 195, a);
                else if (offset <= 32) w_col = rgba(255, 55, 30, a);
                else w_col = rgba(195, 15, 25, (uint8_t)(a * 0.7f));
            } else {
                if (offset <= 18) w_col = rgba(225, 245, 255, a);
                else if (offset <= 32) w_col = rgba(0, 215, 255, a);
                else w_col = rgba(0, 85, 215, (uint8_t)(a * 0.7f));
            }
            /* Inner lower diagonal wedge */
            draw_thick_line(r, l_elb_x + offset, mid_y_bot, l_floor_x1 + offset, bot_y, 2, w_col);
            draw_thick_line(r, r_elb_x - offset, mid_y_bot, r_floor_x1 - offset, bot_y, 2, w_col);
            /* Inner upper diagonal wedge */
            draw_thick_line(r, l_elb_x + offset, mid_y_top, l_roof_x1 + offset, top_y, 2, w_col);
            draw_thick_line(r, r_elb_x - offset, mid_y_top, r_roof_x1 - offset, top_y, 2, w_col);
        }

        /* 4. BRILLIANT WHITE METALLIC OUTER BEZEL LINES */
        draw_thick_line(r, l_elb_x, mid_y_top, l_elb_x, mid_y_bot, 2, chevron_outer);
        draw_thick_line(r, l_elb_x, mid_y_top, l_roof_x1, top_y, 2, chevron_outer);
        draw_thick_line(r, l_roof_x1, top_y, l_roof_x2, top_y, 2, chevron_outer);
        draw_thick_line(r, l_elb_x, mid_y_bot, l_floor_x1, bot_y, 2, chevron_outer);
        draw_thick_line(r, l_floor_x1, bot_y, l_floor_x2, bot_y, 2, chevron_outer);

        draw_thick_line(r, r_elb_x, mid_y_top, r_elb_x, mid_y_bot, 2, chevron_outer);
        draw_thick_line(r, r_elb_x, mid_y_top, r_roof_x1, top_y, 2, chevron_outer);
        draw_thick_line(r, r_roof_x1, top_y, r_roof_x2, top_y, 2, chevron_outer);
        draw_thick_line(r, r_elb_x, mid_y_bot, r_floor_x1, bot_y, 2, chevron_outer);
        draw_thick_line(r, r_floor_x1, bot_y, r_floor_x2, bot_y, 2, chevron_outer);

        /* M Tri-Color Ribbon on Left Roof */
        fill_rect(r, l_roof_x1 + 4, top_y - 3, 12, 4, m_blue);
        fill_rect(r, l_roof_x1 + 16, top_y - 3, 12, 4, m_dark_blue);
        fill_rect(r, l_roof_x1 + 28, top_y - 3, 12, 4, m_red);

        /* Cyan/Red Ribbon on Right Roof */
        fill_rect(r, r_roof_x2 + 4, top_y - 3, 24, 4, bmw_cyan);
        fill_rect(r, r_roof_x2 + 28, top_y - 3, 12, 4, m_red);

        /* Authentic Left Speed Scale Ticks (220, 180, 150, 120, 90, 60, 30, 0) */
        char left_top_hdr[32];
        snprintf(left_top_hdr, sizeof(left_top_hdr), "%d%s", ui->use_mph ? 140 : 220, ui->use_mph ? "mph" : "km/h");
        draw_text(r, left_top_hdr, l_roof_x1 - 10, top_y - 18, 1, text_wh);

        /* 180 tick at y = content_y + 80, x = 340 */
        draw_text_right(r, ui->use_mph ? "110" : "180", 325, content_y + 76, 1, text_wh);
        draw_thick_line(r, 330, content_y + 80, 340, content_y + 80, 2, text_wh);

        /* 150 tick at y = content_y + 130, x = 296 */
        draw_text_right(r, ui->use_mph ? "90" : "150", 281, content_y + 126, 1, text_wh);
        draw_thick_line(r, 286, content_y + 130, 296, content_y + 130, 2, text_wh);

        /* 120 tick at elbow y = content_y + 185, x = 240 */
        draw_text_right(r, ui->use_mph ? "70" : "120", 225, content_y + 181, 1, text_wh);
        draw_thick_line(r, 230, content_y + 185, 240, content_y + 185, 2, text_wh);

        /* 90 tick at lower diagonal y = content_y + 280, x = 250 */
        draw_text_right(r, ui->use_mph ? "50" : "90", 235, content_y + 276, 1, text_wh);
        draw_thick_line(r, 240, content_y + 280, 250, content_y + 280, 2, text_wh);

        /* 60 tick at lower diagonal y = content_y + 345, x = 291 */
        draw_text_right(r, ui->use_mph ? "35" : "60", 276, content_y + 341, 1, text_wh);
        draw_thick_line(r, 281, content_y + 345, 291, content_y + 345, 2, text_wh);

        /* 30 tick at lower diagonal y = content_y + 410, x = 333 */
        draw_text_right(r, ui->use_mph ? "20" : "30", 318, content_y + 406, 1, text_wh);
        draw_thick_line(r, 323, content_y + 410, 333, content_y + 410, 2, text_wh);

        /* 0 tick at floor y = bot_y - 8, x = 375 */
        draw_text_right(r, "0", 360, bot_y - 12, 1, text_wh);
        draw_thick_line(r, 365, bot_y - 8, 375, bot_y - 8, 2, text_wh);

        /* Authentic Right Power % Scale Ticks (ePower % 100, 75, 50, 25, 0 READY) */
        draw_text(r, "ePower % 100", r_roof_x2 + 4, top_y - 18, 1, text_wh);

        /* 75 tick at y = content_y + 105, x = 962 */
        draw_thick_line(r, 962, content_y + 105, 972, content_y + 105, 2, text_wh);
        draw_text(r, "75", 977, content_y + 101, 1, text_wh);

        /* 50 tick at elbow y = content_y + 225, x = 1040 */
        draw_thick_line(r, 1040, content_y + 225, 1050, content_y + 225, 2, text_wh);
        draw_text(r, "50", 1055, content_y + 221, 1, text_wh);

        /* 25 tick at lower diagonal y = content_y + 345, x = 989 */
        draw_thick_line(r, 989, content_y + 345, 999, content_y + 345, 2, text_wh);
        draw_text(r, "25", 1004, content_y + 341, 1, text_wh);

        /* 0 READY tick at floor */
        draw_text(r, "0 READY", 915, content_y + 416, 1, text_wh);

        /* Right Floor Odometer & Slanted ADAPTIVE Pill (from Photos 1 & 2) */
        int r_plug_x = 780;
        int r_plug_y = bot_y - 14;
        draw_icon_ev_plug(r, r_plug_x, r_plug_y + 4, text_wh);
        draw_text(r, trip_str, r_plug_x + 14, r_plug_y, 2, text_wh);
        draw_thick_line(r, r_plug_x, r_plug_y + 18, r_plug_x + 95, r_plug_y + 18, 2, bmw_cyan);

        int pill_x = 880;
        int pill_y = content_y + 440;
        fill_rect(r, pill_x, pill_y, 85, 20, rgba(0, 100, 205, 220));
        draw_rect(r, pill_x, pill_y, 85, 20, 1, bmw_cyan);
        draw_text_centered(r, "ADAPTIVE", pill_x + 42, pill_y + 3, 1, text_wh);

        /* 5. TOP CENTER OFFICIAL BMW ///M LOGO & LAP/DELTA STATUS */
        char cur_t[16], bst_t[16];
        format_lap_time(state->current_lap_ms, cur_t, sizeof(cur_t));
        format_lap_time(state->best_lap_ms, bst_t, sizeof(bst_t));
        char lap_summary[48];
        snprintf(lap_summary, sizeof(lap_summary), "L%d %s",
                 (state->current_lap > 0 ? state->current_lap : 1), cur_t);
        draw_text(r, lap_summary, 445, content_y + 38, 1, text_dim);

        /* Live Delta Pill at Top Right of Center */
        int d_ms = state->delta_ms;
        if (d_ms == 0 && state->best_lap_ms > 0 && state->current_lap_ms > 0) d_ms = state->current_lap_ms - state->best_lap_ms;
        char d_buf[32];
        snprintf(d_buf, sizeof(d_buf), "DELTA %+.3fs", (float)d_ms / 1000.0f);
        fill_rect(r, 730, content_y + 34, 115, 20, (d_ms <= 0) ? rgba(20, 140, 60, 220) : rgba(180, 30, 30, 220));
        draw_rect(r, 730, content_y + 34, 115, 20, 1, text_wh);
        draw_text_centered(r, d_buf, 787, content_y + 37, 1, text_wh);

        /* Center ///M Emblem */
        draw_bmw_m_logo(r, 640, content_y + 60);

        /* 6. CENTER STACKED DUAL SPEED (Secondary mph above, massive primary km/h below) */
        char sec_spd_str[32];
        snprintf(sec_spd_str, sizeof(sec_spd_str), "%d %s", spd_sec, ui->use_mph ? "km/h" : "mph");
        draw_text_centered(r, sec_spd_str, 640, content_y + 100, 1, text_dim);

        int digits = (spd_pri >= 100) ? 3 : ((spd_pri >= 10) ? 2 : 1);
        draw_modern_number_centered(r, spd_pri, digits, false, 640, content_y + 135, 46, 84, 10, 8, text_wh);

        int spd_w = digits * 46 + (digits - 1) * 8;
        int unit_x = 640 + spd_w / 2 + 8;
        int unit_y = content_y + 135 + 84 - 18;
        draw_text(r, ui->use_mph ? "mph" : "km/h", unit_x, unit_y, 2, text_dim);

        /* 7. ///M DIGITAL RPM RIBBON & ACTIVE GEAR CENTERED IN LOWER COCKPIT */
        char rpm_m_str[32];
        snprintf(rpm_m_str, sizeof(rpm_m_str), "%.0f RPM", state->engine_rpm);
        draw_text_centered(r, rpm_m_str, 640, content_y + 382, 2, bmw_cyan);
        float m_rpm_max = (state->rpm_limiter > 1000) ? (float)state->rpm_limiter : 8500.0f;
        int m_segs = 16;
        int m_lit = (int)((state->engine_rpm / m_rpm_max) * m_segs);
        for (int s = 0; s < m_segs; ++s) {
            rgba_t sc = (s < 8) ? m_blue : ((s < 12) ? rgba(245, 175, 20, 255) : m_red);
            fill_rect(r, 528 + s * 14, content_y + 402, 11, 6, (s < m_lit) ? sc : rgba(20, 30, 48, 255));
        }

        draw_modern_char(r, gear_ch, 640 - 15, content_y + 436, 30, 42, 6, text_wh);

        /* 8. MOTORSPORT TELEMETRY: FLANKING SIDES & LOWER CENTER */
        /* Left Flank: Warning light column + Turbo Boost Gauge + Fluid Temps + Chassis Dynamics */
        char bst_str[32];
        snprintf(bst_str, sizeof(bst_str), "BOOST %+.2f BAR", state->turbo_bar);
        draw_text(r, bst_str, 46, content_y + 68, 1, text_wh);
        draw_rect(r, 46, content_y + 84, 130, 8, 1, rgba(60, 95, 140, 200));
        float bst_norm = (state->turbo_bar + 1.0f) / 3.0f;
        if (bst_norm < 0.0f) bst_norm = 0.0f;
        if (bst_norm > 1.0f) bst_norm = 1.0f;
        fill_rect(r, 48, content_y + 86, (int)(bst_norm * 126), 4, is_dynamic_flare ? m_red : bmw_cyan);

        char fluid_str[48];
        snprintf(fluid_str, sizeof(fluid_str), "OIL %.0f°C  H2O %.0f°C",
                 (state->oil_temp > 0 ? state->oil_temp : 95.0f),
                 (state->water_temp > 0 ? state->water_temp : 88.0f));
        draw_text(r, fluid_str, 46, content_y + 100, 1, text_dim);

        /* Left Flank Throttle trace */
        draw_text(r, "THROTTLE", 46, content_y + 118, 1, text_dim);
        draw_rect(r, 112, content_y + 119, 75, 7, 1, rgba(60, 95, 140, 200));
        fill_rect(r, 112, content_y + 119, (int)(state->throttle_pct * 0.75f), 7, rgba(40, 220, 100, 255));

        /* Lower-Left Flank ///M Chassis & Drivetrain Telemetry Card (True OLED Black) */
        draw_panel(r, 44, content_y + 138, 152, 162, rgba(0, 0, 0, 255), rgba(0, 163, 224, 150), 8);
        draw_text(r, "///M CHASSIS", 54, content_y + 146, 1, m_blue);
        snprintf(fluid_str, sizeof(fluid_str), "SUSP %.0f/%.0f/%.0f/%.0f",
                 state->susp_height[0], state->susp_height[1],
                 state->susp_height[2], state->susp_height[3]);
        draw_text(r, fluid_str, 54, content_y + 166, 1, text_wh);
        snprintf(fluid_str, sizeof(fluid_str), "SLIP %.0f/%.0f/%.0f/%.0f%%",
                 state->tire_slip_ratio[0] * 100.0f, state->tire_slip_ratio[1] * 100.0f,
                 state->tire_slip_ratio[2] * 100.0f, state->tire_slip_ratio[3] * 100.0f);
        draw_text(r, fluid_str, 54, content_y + 186, 1, bmw_cyan);
        snprintf(fluid_str, sizeof(fluid_str), "STEER %+.1fD  RH %.0f", state->steer_angle_fl_deg, state->body_height);
        draw_text(r, fluid_str, 54, content_y + 206, 1, text_dim);
        float m_laps = (state->fuel_burn_per_lap > 0.1f) ? (state->fuel_pct / state->fuel_burn_per_lap) : (state->fuel_pct / 3.2f);
        snprintf(fluid_str, sizeof(fluid_str), "FUEL %.0f%% (%.1fL)", state->fuel_pct, m_laps);
        draw_text(r, fluid_str, 54, content_y + 226, 1, rgba(40, 220, 100, 255));
        snprintf(fluid_str, sizeof(fluid_str), "CLU %.0f%%  GBX %.0f", (100.0f - state->clutch_pct), state->engine_rpm);
        draw_text(r, fluid_str, 54, content_y + 246, 1, text_wh);
        snprintf(fluid_str, sizeof(fluid_str), "AERO %.0f/%.0fKG", state->downforce_front_kg, state->downforce_rear_kg);
        draw_text(r, fluid_str, 54, content_y + 266, 1, text_dim);
        snprintf(fluid_str, sizeof(fluid_str), "RAD %.0f/%.0f R/S",
                 (state->wheel_angular_speed[0] + state->wheel_angular_speed[1]) * 0.5f,
                 (state->wheel_angular_speed[2] + state->wheel_angular_speed[3]) * 0.5f);
        draw_text(r, fluid_str, 54, content_y + 284, 1, bmw_cyan);

        /* Right Flank: G-Force Dynamics & Hybrid Energy + ///M Lap & Sector Split Card */
        char gf_str[32];
        snprintf(gf_str, sizeof(gf_str), "G %+.2fL / %+.2fN", state->g_lat, state->g_lon);
        draw_text(r, gf_str, 1095, content_y + 68, 1, text_wh);

        int g_cx = 1210, g_cy = content_y + 108, g_r = 16;
        draw_circle(r, g_cx, g_cy, g_r, 1, rgba(60, 95, 140, 180));
        draw_line(r, g_cx - g_r, g_cy, g_cx + g_r, g_cy, rgba(60, 95, 140, 180));
        draw_line(r, g_cx, g_cy - g_r, g_cx, g_cy + g_r, rgba(60, 95, 140, 180));
        int gdot_x = g_cx + (int)(state->g_lat * 9.0f);
        int gdot_y = g_cy - (int)(state->g_lon * 9.0f);
        fill_circle(r, gdot_x, gdot_y, 3, m_red);

        char hbd_str[48];
        snprintf(hbd_str, sizeof(hbd_str), "REGEN %+.1fkW", state->energy_recovery_kw);
        draw_text(r, hbd_str, 1095, content_y + 100, 1, bmw_cyan);

        /* Right Flank Brake trace */
        draw_text(r, "BRAKE", 1095, content_y + 118, 1, text_dim);
        draw_rect(r, 1145, content_y + 119, 85, 7, 1, rgba(60, 95, 140, 200));
        fill_rect(r, 1145, content_y + 119, (int)(state->brake_pct * 0.85f), 7, m_red);

        /* Lower-Right Flank ///M Lap & Sector Split Card (True OLED Black) */
        draw_panel(r, 1084, content_y + 138, 156, 162, rgba(0, 0, 0, 255), rgba(230, 26, 35, 160), 8);
        draw_text(r, "///M LAP TIMER", 1094, content_y + 146, 1, m_red);
        snprintf(hbd_str, sizeof(hbd_str), "BEST %s", bst_t);
        draw_text(r, hbd_str, 1094, content_y + 166, 1, rgba(195, 85, 255, 255));
        char lst_t[16];
        format_lap_time(state->last_lap_ms, lst_t, sizeof(lst_t));
        snprintf(hbd_str, sizeof(hbd_str), "LAST %s", lst_t);
        draw_text(r, hbd_str, 1094, content_y + 186, 1, text_wh);
        char s1_t[16], s2_t[16], s3_t[16];
        format_sector_time(state->cur_sector_ms[0], s1_t, sizeof(s1_t));
        format_sector_time(state->cur_sector_ms[1], s2_t, sizeof(s2_t));
        format_sector_time(state->cur_sector_ms[2], s3_t, sizeof(s3_t));
        snprintf(hbd_str, sizeof(hbd_str), "S1 %s  S2 %s", s1_t, s2_t);
        draw_text(r, hbd_str, 1094, content_y + 206, 1, bmw_cyan);
        snprintf(hbd_str, sizeof(hbd_str), "S3 %s  POS P%d", s3_t, state->race_position > 0 ? state->race_position : 2);
        draw_text(r, hbd_str, 1094, content_y + 226, 1, text_wh);
        snprintf(hbd_str, sizeof(hbd_str), "CAT %s  TOP %.0fK",
                 state->car_category[0] ? state->car_category : "GR3", state->session_top_speed_kmh);
        draw_text(r, hbd_str, 1094, content_y + 246, 1, gold);
        snprintf(hbd_str, sizeof(hbd_str), "TV %.0f/%.0fNM", state->torque_vector_nm[0], state->torque_vector_nm[2]);
        draw_text(r, hbd_str, 1094, content_y + 266, 1, text_dim);
        snprintf(hbd_str, sizeof(hbd_str), "BB %+d  TCS %d  MAP %d",
                 state->gt7_brake_bal, state->gt7_tcs_level, state->gt7_fuel_map);
        draw_text(r, hbd_str, 1094, content_y + 284, 1, bmw_cyan);

        /* Lower Center Flanking Gear: 4-Tire Heat Badges with dynamic heat coloring */
        char fl_str[32], fr_str[32], rl_str[32], rr_str[32];
        snprintf(fl_str, sizeof(fl_str), "FL %.0f°", state->tire_temp[0] > 0 ? state->tire_temp[0] : 82.0f);
        snprintf(fr_str, sizeof(fr_str), "FR %.0f°", state->tire_temp[1] > 0 ? state->tire_temp[1] : 85.0f);
        snprintf(rl_str, sizeof(rl_str), "RL %.0f°", state->tire_temp[2] > 0 ? state->tire_temp[2] : 88.0f);
        snprintf(rr_str, sizeof(rr_str), "RR %.0f°", state->tire_temp[3] > 0 ? state->tire_temp[3] : 86.0f);

        rgba_t t_col0 = (state->tire_temp[0] > 95.0f) ? m_red : ((state->tire_temp[0] > 70.0f) ? rgba(40, 220, 100, 255) : bmw_cyan);
        rgba_t t_col1 = (state->tire_temp[1] > 95.0f) ? m_red : ((state->tire_temp[1] > 70.0f) ? rgba(40, 220, 100, 255) : bmw_cyan);
        rgba_t t_col2 = (state->tire_temp[2] > 95.0f) ? m_red : ((state->tire_temp[2] > 70.0f) ? rgba(40, 220, 100, 255) : bmw_cyan);
        rgba_t t_col3 = (state->tire_temp[3] > 95.0f) ? m_red : ((state->tire_temp[3] > 70.0f) ? rgba(40, 220, 100, 255) : bmw_cyan);

        draw_text(r, fl_str, 540, content_y + 438, 1, t_col0);
        draw_text(r, fr_str, 705, content_y + 438, 1, t_col1);
        draw_text(r, rl_str, 540, content_y + 460, 1, t_col2);
        draw_text(r, rr_str, 705, content_y + 460, 1, t_col3);
    }

    /* =========================================================================
     * VIEW 2: BMW 3D NAVIGATION MAP COCKPIT (Photo 3)
     * Full 3D Night Navigation Map filling center between chevrons,
     * Left Crook: Stacked speed (0 mph over 0 km/h) & outer speed scale ticks,
     * Right Crook: Giant Gear, READY 0, Trip 1503.7 km, ADAPTIVE pill & power scale.
     * ========================================================================= */
    else if (ui->bmw_view == BMW_VIEW_MAP_COCKPIT) {
        int top_y = content_y + 40;
        int bot_y = content_y + content_h - 55;
        int mid_y_top = content_y + 195;
        int mid_y_bot = content_y + 265;

        /* Symmetrical outer chevrons matching Photo 3 */
        int l_elb_x = 135;
        int l_roof_x1 = 265, l_roof_x2 = 310;
        int l_floor_x1 = 265, l_floor_x2 = 310;

        int r_elb_x = 1280 - 135; /* 1145 */
        int r_roof_x1 = 1280 - 265, r_roof_x2 = 1280 - 310; /* 1015 to 970 */
        int r_floor_x1 = 1280 - 265, r_floor_x2 = 1280 - 310; /* 1015 to 970 */

        /* 3D Night Navigation City Map Canvas on True OLED Pitch Black (framed between chevrons) */
        int map_x = 295;
        int map_w = 690;
        fill_rect(r, map_x, content_y + 36, map_w, content_h - 90, rgba(0, 0, 0, 255));
        draw_rect(r, map_x, content_y + 36, map_w, content_h - 90, 1, rgba(20, 45, 80, 150));

        /* Realistic GPS Vector Street Grid (matching Photo 3) */
        /* Major Avenue cutting diagonally across city */
        draw_thick_line(r, map_x + 190, content_y + content_h - 60, map_x + 130, content_y + 36, 7, rgba(25, 42, 68, 255));
        draw_thick_line(r, map_x + 190, content_y + content_h - 60, map_x + 130, content_y + 36, 3, rgba(235, 242, 255, 220));

        /* Cross Boulevards */
        draw_thick_line(r, map_x + 10, content_y + 130, map_x + map_w - 10, content_y + 130, 3, rgba(160, 190, 225, 190));
        draw_thick_line(r, map_x + 10, content_y + 245, map_x + map_w - 10, content_y + 245, 4, rgba(210, 230, 255, 220));
        draw_thick_line(r, map_x + 10, content_y + 375, map_x + map_w - 10, content_y + 375, 3, rgba(160, 190, 225, 190));
        draw_thick_line(r, map_x + 10, content_y + 450, map_x + map_w - 10, content_y + 450, 2, rgba(120, 155, 195, 160));

        /* North-South Connected Streets */
        static const int map_ns_x[] = { 50, 140, 220, 310, 400, 490, 580, 650 };
        for (size_t s = 0; s < sizeof(map_ns_x) / sizeof(map_ns_x[0]); s++) {
            draw_thick_line(r, map_x + map_ns_x[s], content_y + 36, map_x + map_ns_x[s] + 12, content_y + content_h - 60, 2, rgba(135, 170, 210, 180));
        }

        /* Diagonal Residential Streets */
        draw_thick_line(r, map_x + 360, content_y + 36, map_x + 580, content_y + 245, 2, rgba(110, 145, 185, 160));
        draw_thick_line(r, map_x + 420, content_y + 245, map_x + 640, content_y + 435, 2, rgba(110, 145, 185, 160));
        draw_thick_line(r, map_x + 40, content_y + 175, map_x + 150, content_y + 295, 2, rgba(110, 145, 185, 160));
        draw_thick_line(r, map_x + 180, content_y + 245, map_x + 320, content_y + 415, 2, rgba(110, 145, 185, 160));

        /* Golden 3D Navigation Cursor */
        int nav_cx = map_x + 220;
        int nav_cy = content_y + 340;
        draw_thick_line(r, nav_cx, nav_cy - 14, nav_cx - 10, nav_cy + 10, 3, gold);
        draw_thick_line(r, nav_cx, nav_cy - 14, nav_cx + 10, nav_cy + 10, 3, gold);
        draw_thick_line(r, nav_cx - 10, nav_cy + 10, nav_cx, nav_cy + 3, 2, gold);
        draw_thick_line(r, nav_cx + 10, nav_cy + 10, nav_cx, nav_cy + 3, 2, gold);

        /* Left Chevron Outer Bezel */
        draw_thick_line(r, l_elb_x, mid_y_top, l_elb_x, mid_y_bot, 2, chevron_outer);
        draw_thick_line(r, l_elb_x, mid_y_top, l_roof_x1, top_y, 2, chevron_outer);
        draw_thick_line(r, l_roof_x1, top_y, l_roof_x2, top_y, 2, chevron_outer);
        draw_thick_line(r, l_elb_x, mid_y_bot, l_floor_x1, bot_y, 2, chevron_outer);
        draw_thick_line(r, l_floor_x1, bot_y, l_floor_x2, bot_y, 2, chevron_outer);

        /* Right Chevron Outer Bezel */
        draw_thick_line(r, r_elb_x, mid_y_top, r_elb_x, mid_y_bot, 2, chevron_outer);
        draw_thick_line(r, r_elb_x, mid_y_top, r_roof_x1, top_y, 2, chevron_outer);
        draw_thick_line(r, r_roof_x1, top_y, r_roof_x2, top_y, 2, chevron_outer);
        draw_thick_line(r, r_elb_x, mid_y_bot, r_floor_x1, bot_y, 2, chevron_outer);
        draw_thick_line(r, r_floor_x1, bot_y, r_floor_x2, bot_y, 2, chevron_outer);

        fill_rect(r, l_roof_x1 + 4, top_y - 3, 10, 4, m_blue);
        fill_rect(r, l_roof_x1 + 14, top_y - 3, 10, 4, m_dark_blue);
        fill_rect(r, l_roof_x1 + 24, top_y - 3, 10, 4, m_red);
        fill_rect(r, r_roof_x2 + 4, top_y - 3, 30, 4, bmw_cyan);

        /* Left Chevron Scale Ticks (Photo 3) */
        char left_top_hdr[32];
        snprintf(left_top_hdr, sizeof(left_top_hdr), "%d%s", ui->use_mph ? 140 : 220, ui->use_mph ? "mph" : "km/h");
        draw_text(r, left_top_hdr, l_roof_x1 - 10, top_y - 18, 1, text_wh);
        draw_text_right(r, ui->use_mph ? "110" : "180", 225, content_y + 85, 1, text_wh);
        draw_thick_line(r, 230, content_y + 89, 238, content_y + 89, 2, text_wh);
        draw_text_right(r, ui->use_mph ? "90" : "150", 185, content_y + 135, 1, text_wh);
        draw_thick_line(r, 190, content_y + 139, 198, content_y + 139, 2, text_wh);
        draw_text_right(r, ui->use_mph ? "70" : "120", 125, content_y + 190, 1, text_wh);
        draw_thick_line(r, 128, content_y + 194, 135, content_y + 194, 2, text_wh);
        draw_text_right(r, ui->use_mph ? "50" : "90", 138, content_y + 280, 1, text_wh);
        draw_thick_line(r, 142, content_y + 284, 150, content_y + 284, 2, text_wh);
        draw_text_right(r, ui->use_mph ? "35" : "60", 180, content_y + 345, 1, text_wh);
        draw_thick_line(r, 185, content_y + 349, 195, content_y + 349, 2, text_wh);
        draw_text_right(r, ui->use_mph ? "20" : "30", 220, content_y + 410, 1, text_wh);
        draw_thick_line(r, 225, content_y + 414, 235, content_y + 414, 2, text_wh);
        draw_text_right(r, "0", 252, bot_y - 12, 1, text_wh);
        draw_thick_line(r, 255, bot_y - 8, 265, bot_y - 8, 2, text_wh);

        /* Left Crook Stacked Speed: 0 mph above large 0 km/h (Photo 3) */
        int spd_cx = 200;
        int spd_y = content_y + 225;
        char sec_spd_map[32];
        snprintf(sec_spd_map, sizeof(sec_spd_map), "%d %s", spd_sec, ui->use_mph ? "km/h" : "mph");
        draw_text_centered(r, sec_spd_map, spd_cx, spd_y - 28, 1, text_dim);

        int digits = (spd_pri >= 100) ? 3 : ((spd_pri >= 10) ? 2 : 1);
        draw_modern_number_centered(r, spd_pri, digits, false, spd_cx, spd_y, 38, 70, 8, 7, text_wh);
        draw_text_centered(r, ui->use_mph ? "mph" : "km/h", spd_cx, spd_y + 76, 2, text_dim);

        /* Right Chevron Scale Ticks (Photo 3) */
        draw_text(r, "ePower % 100", r_roof_x2 + 4, top_y - 18, 1, text_wh);
        draw_thick_line(r, 1042, content_y + 105, 1052, content_y + 105, 2, text_wh);
        draw_text(r, "75", 1057, content_y + 101, 1, text_wh);
        draw_thick_line(r, 1145, content_y + 225, 1155, content_y + 225, 2, text_wh);
        draw_text(r, "50", 1160, content_y + 221, 1, text_wh);
        draw_thick_line(r, 1085, content_y + 345, 1095, content_y + 345, 2, text_wh);
        draw_text(r, "25", 1100, content_y + 341, 1, text_wh);

        /* Right Crook: Huge Gear, READY 0, Trip 1503.7 km, ADAPTIVE (Photo 3) */
        int gear_cx = 1080;
        int gear_y = content_y + 225;
        draw_modern_char(r, gear_ch, gear_cx - 19, gear_y, 38, 70, 8, text_wh);
        draw_text_centered(r, "READY 0", gear_cx, content_y + 355, 2, text_wh);

        draw_icon_ev_plug(r, gear_cx - 48, content_y + 392, text_wh);
        draw_text(r, trip_str, gear_cx - 34, content_y + 388, 2, text_wh);
        draw_thick_line(r, gear_cx - 48, content_y + 408, gear_cx + 48, content_y + 408, 2, bmw_cyan);

        int pill_x = gear_cx - 42;
        int pill_y = content_y + 428;
        fill_rect(r, pill_x, pill_y, 85, 20, rgba(0, 100, 200, 220));
        draw_rect(r, pill_x, pill_y, 85, 20, 1, bmw_cyan);
        draw_text_centered(r, "ADAPTIVE", pill_x + 42, pill_y + 3, 1, text_wh);

        /* Top Track & Motorsport Overlay Banner on True OLED Black */
        fill_rect(r, map_x + 20, content_y + 40, map_w - 40, 24, rgba(0, 0, 0, 245));
        draw_rect(r, map_x + 20, content_y + 40, map_w - 40, 24, 1, rgba(0, 160, 255, 110));
        char map_top[256];
        snprintf(map_top, sizeof(map_top), "%s   |   LAP %d/%d   |   POS %d/%d   |   BOOST %+.2f BAR",
                 state->circuit_name[0] ? state->circuit_name : "SUZUKA CIRCUIT",
                 state->current_lap > 0 ? state->current_lap : 1,
                 state->total_laps > 0 ? state->total_laps : 10,
                 state->race_position > 0 ? state->race_position : 2,
                 state->total_cars > 0 ? state->total_cars : 16,
                 state->turbo_bar);
        draw_text_centered(r, map_top, map_x + map_w / 2, content_y + 45, 1, text_wh);

        /* Bottom Map Telemetry Overlay Banner (Sector Splits, Pedals, G-Load & Tires) */
        fill_rect(r, map_x + 20, bot_y - 28, map_w - 40, 24, rgba(0, 0, 0, 245));
        draw_rect(r, map_x + 20, bot_y - 28, map_w - 40, 24, 1, rgba(0, 160, 255, 110));
        char s1_b[16], s2_b[16], s3_b[16];
        format_sector_time(state->cur_sector_ms[0], s1_b, sizeof(s1_b));
        format_sector_time(state->cur_sector_ms[1], s2_b, sizeof(s2_b));
        format_sector_time(state->cur_sector_ms[2], s3_b, sizeof(s3_b));
        snprintf(map_top, sizeof(map_top), "S1 %s  S2 %s  S3 %s  |  THR %.0f%% BRK %.0f%%  |  FL %.0f° FR %.0f° RL %.0f° RR %.0f°",
                 s1_b, s2_b, s3_b, state->throttle_pct, state->brake_pct,
                 state->tire_temp[0], state->tire_temp[1], state->tire_temp[2], state->tire_temp[3]);
        draw_text_centered(r, map_top, map_x + map_w / 2, bot_y - 23, 1, bmw_cyan);

        /* Left & Right Outer Flank Vitals in 3D Map Mode */
        snprintf(map_top, sizeof(map_top), "%.0f RPM", state->engine_rpm);
        draw_text(r, map_top, 16, content_y + 45, 1, bmw_cyan);
        snprintf(map_top, sizeof(map_top), "H2O %.0f° OIL %.0f°", state->water_temp, state->oil_temp);
        draw_text(r, map_top, 16, content_y + 62, 1, text_dim);
        snprintf(map_top, sizeof(map_top), "SUSP %.0f/%.0fMM", state->susp_height[0], state->susp_height[2]);
        draw_text(r, map_top, 16, content_y + 79, 1, text_wh);
        snprintf(map_top, sizeof(map_top), "STEER %+.1f°", state->steer_angle_fl_deg);
        draw_text(r, map_top, 16, content_y + 96, 1, bmw_cyan);

        snprintf(map_top, sizeof(map_top), "DELTA %+.3fs", (float)state->delta_ms / 1000.0f);
        draw_text_right(r, map_top, 1264, content_y + 45, 1, (state->delta_ms <= 0) ? rgba(40, 220, 100, 255) : m_red);
        snprintf(map_top, sizeof(map_top), "G %+.2fL/%+.2fN", state->g_lat, state->g_lon);
        draw_text_right(r, map_top, 1264, content_y + 62, 1, text_dim);
        snprintf(map_top, sizeof(map_top), "ERS %+.0fKW", state->energy_recovery_kw);
        draw_text_right(r, map_top, 1264, content_y + 79, 1, bmw_cyan);
        snprintf(map_top, sizeof(map_top), "FUEL %.0f%%", state->fuel_pct);
        draw_text_right(r, map_top, 1264, content_y + 96, 1, rgba(40, 220, 100, 255));
    }

    /* =========================================================================
     * VIEW 3: BMW ASSISTED DRIVING & HIGHWAY RADAR VIEW (Photo 5)
     * Single Left Diagonal Chevron, Perspective Highway Corridor, 3D Rendered
     * BMW Vehicle in Lane, Speed Corridor (135 min / 310 max), Radar Gateway Barrier (276 km).
     * ========================================================================= */
    else if (ui->bmw_view == BMW_VIEW_ASSIST_RADAR) {
        int top_y = content_y + 40;
        int bot_y = content_y + content_h - 55;
        int l_elb_x = 140;
        int l_roof_x1 = 270, l_roof_x2 = 320;
        int l_floor_x1 = 340, l_floor_x2 = 390;

        /* Single Left Diagonal Chevron Frame */
        draw_thick_line(r, l_elb_x, content_y + 195, l_elb_x, content_y + 265, 2, chevron_outer);
        draw_thick_line(r, l_elb_x, content_y + 195, l_roof_x1, top_y, 2, chevron_outer);
        draw_thick_line(r, l_roof_x1, top_y, l_roof_x2, top_y, 2, chevron_outer);
        draw_thick_line(r, l_elb_x, content_y + 265, l_floor_x1, bot_y, 2, chevron_outer);
        draw_thick_line(r, l_floor_x1, bot_y, l_floor_x2, bot_y, 2, chevron_outer);

        fill_rect(r, l_roof_x1 + 4, top_y - 3, 12, 4, m_blue);
        fill_rect(r, l_roof_x1 + 16, top_y - 3, 12, 4, m_dark_blue);
        fill_rect(r, l_roof_x1 + 28, top_y - 3, 12, 4, m_red);

        draw_text_right(r, "20", l_floor_x1 - 15, bot_y - 12, 1, text_wh);
        draw_thick_line(r, l_floor_x1 - 10, bot_y - 8, l_floor_x1 + 2, bot_y - 8, 2, text_wh);
        draw_text(r, ui->use_mph ? "mph" : "km/h", l_floor_x1 - 65, bot_y + 2, 1, text_dim);

        /* Left Stacked Speed (Photo 5) + Upper Left RPM/Gear Readout */
        char rad_l_buf[48];
        snprintf(rad_l_buf, sizeof(rad_l_buf), "GEAR %c  |  %.0f RPM", gear_ch, state->engine_rpm);
        draw_text(r, rad_l_buf, 16, content_y + 48, 1, bmw_cyan);
        snprintf(rad_l_buf, sizeof(rad_l_buf), "H2O %.0f°C  OIL %.0f°C", state->water_temp, state->oil_temp);
        draw_text(r, rad_l_buf, 16, content_y + 66, 1, text_dim);
        snprintf(rad_l_buf, sizeof(rad_l_buf), "SUSP %.0f/%.0fMM", state->susp_height[0], state->susp_height[2]);
        draw_text(r, rad_l_buf, 16, content_y + 84, 1, text_wh);
        snprintf(rad_l_buf, sizeof(rad_l_buf), "SLIP %.0f/%.0f%%", state->tire_slip_ratio[0] * 100.0f, state->tire_slip_ratio[2] * 100.0f);
        draw_text(r, rad_l_buf, 16, content_y + 102, 1, bmw_cyan);
        snprintf(rad_l_buf, sizeof(rad_l_buf), "STEER %+.1f°", state->steer_angle_fl_deg);
        draw_text(r, rad_l_buf, 16, content_y + 120, 1, text_dim);

        int spd_cx = 180;
        int spd_y = content_y + 245;
        char sec_spd_radar[32];
        snprintf(sec_spd_radar, sizeof(sec_spd_radar), "%d %s", spd_sec, ui->use_mph ? "km/h" : "mph");
        draw_text_centered(r, sec_spd_radar, spd_cx, spd_y - 28, 1, text_dim);

        int digits = (spd_pri >= 100) ? 3 : ((spd_pri >= 10) ? 2 : 1);
        draw_modern_number_centered(r, spd_pri, digits, false, spd_cx, spd_y, 42, 76, 9, 8, text_wh);
        draw_text(r, ui->use_mph ? "mph" : "km/h", spd_cx + 42, spd_y + 55, 1, text_dim);

        /* 3D PERSPECTIVE HIGHWAY CORRIDOR (from Photo 5) */
        int hwy_y = content_y + 360;
        /* Highway lane asphalt surface */
        fill_rect(r, 420, hwy_y - 4, 760, 18, rgba(22, 38, 65, 230));
        draw_thick_line(r, 420, hwy_y - 4, 1180, hwy_y - 4, 2, rgba(0, 190, 255, 220));
        draw_thick_line(r, 420, hwy_y + 14, 1180, hwy_y + 14, 2, rgba(0, 190, 255, 220));

        /* Lane dash markings */
        for (int dx = 440; dx < 1160; dx += 60) {
            draw_thick_line(r, dx, hwy_y + 5, dx + 30, hwy_y + 5, 2, rgba(140, 215, 255, 160));
        }

        /* 3D Rendered BMW Vehicle in Lane */
        draw_bmw_3d_car(r, 550, hwy_y - 14);

        /* Speed corridor text below lane (Photo 5: 135 km min / 310 km max) */
        draw_text_centered(r, "135 km", 730, hwy_y + 24, 2, text_wh);
        draw_text_centered(r, "min", 730, hwy_y + 42, 1, text_dim);

        draw_text_centered(r, "310 km", 1060, hwy_y + 24, 2, text_wh);
        draw_text_centered(r, "max", 1060, hwy_y + 42, 1, text_dim);

        /* Floating 3D Radar Target Barrier ahead (Photo 5) */
        int bar_x = 1010;
        draw_thick_line(r, bar_x, hwy_y - 85, bar_x, hwy_y + 14, 2, rgba(245, 250, 255, 240));
        draw_thick_line(r, bar_x + 36, hwy_y - 65, bar_x + 36, hwy_y + 14, 1, rgba(200, 225, 255, 180));
        draw_thick_line(r, bar_x, hwy_y - 85, bar_x + 36, hwy_y - 65, 1, rgba(200, 225, 255, 180));
        fill_rect(r, bar_x + 2, hwy_y - 75, 34, 85, rgba(0, 140, 255, 55));

        /* Range readout above radar barrier */
        draw_icon_ev_plug(r, bar_x - 14, hwy_y - 105, text_wh);
        draw_text(r, "276 km", bar_x + 2, hwy_y - 110, 2, text_wh);

        /* READY status text on right floor */
        draw_text(r, "READY", 940, bot_y - 12, 2, text_wh);

        /* Upper Motorsport Telemetry HUD on True OLED Black */
        fill_rect(r, 420, content_y + 40, 760, 50, rgba(0, 0, 0, 255));
        draw_rect(r, 420, content_y + 40, 760, 50, 1, rgba(0, 180, 255, 120));
        char rad_telem1[256], rad_telem2[256];
        snprintf(rad_telem1, sizeof(rad_telem1), "CIRCUIT: %s   |   LAP %d/%d   |   POS %d/%d   |   CAT %s",
                 state->circuit_name[0] ? state->circuit_name : "NURBURGRING",
                 state->current_lap > 0 ? state->current_lap : 1,
                 state->total_laps > 0 ? state->total_laps : 10,
                 state->race_position > 0 ? state->race_position : 2,
                 state->total_cars > 0 ? state->total_cars : 16,
                 state->car_category[0] ? state->car_category : "GR3");
        snprintf(rad_telem2, sizeof(rad_telem2), "BOOST %+.2f BAR   G: %+.2fL / %+.2fN   TIRES FL %.0f° FR %.0f° RL %.0f° RR %.0f°",
                 state->turbo_bar, state->g_lat, state->g_lon,
                 state->tire_temp[0] > 0 ? state->tire_temp[0] : 82.0f,
                 state->tire_temp[1] > 0 ? state->tire_temp[1] : 85.0f,
                 state->tire_temp[2] > 0 ? state->tire_temp[2] : 88.0f,
                 state->tire_temp[3] > 0 ? state->tire_temp[3] : 86.0f);
        draw_text(r, rad_telem1, 435, content_y + 48, 1, text_wh);
        draw_text(r, rad_telem2, 435, content_y + 68, 1, bmw_cyan);

        /* Mid-Right ///M Sector Splits & Chassis Dynamics Pods (fills open space above highway) */
        draw_panel(r, 420, content_y + 100, 365, 118, rgba(0, 0, 0, 255), rgba(0, 163, 224, 140), 8);
        draw_text(r, "///M LAP & SECTOR TIMING", 434, content_y + 108, 1, m_blue);
        char cur_r[16], bst_r[16], s1_r[16], s2_r[16], s3_r[16];
        format_lap_time(state->current_lap_ms, cur_r, sizeof(cur_r));
        format_lap_time(state->best_lap_ms, bst_r, sizeof(bst_r));
        format_sector_time(state->cur_sector_ms[0], s1_r, sizeof(s1_r));
        format_sector_time(state->cur_sector_ms[1], s2_r, sizeof(s2_r));
        format_sector_time(state->cur_sector_ms[2], s3_r, sizeof(s3_r));
        snprintf(rad_telem1, sizeof(rad_telem1), "CURR %s   BEST %s", cur_r, bst_r);
        draw_text(r, rad_telem1, 434, content_y + 128, 2, text_wh);
        snprintf(rad_telem1, sizeof(rad_telem1), "DELTA %+.3fs   |   TOP %.1f KM/H",
                 (float)state->delta_ms / 1000.0f, state->session_top_speed_kmh);
        draw_text(r, rad_telem1, 434, content_y + 158, 1, (state->delta_ms <= 0) ? rgba(40, 220, 100, 255) : m_red);
        snprintf(rad_telem1, sizeof(rad_telem1), "S1 %s   |   S2 %s   |   S3 %s", s1_r, s2_r, s3_r);
        draw_text(r, rad_telem1, 434, content_y + 182, 1, bmw_cyan);

        draw_panel(r, 795, content_y + 100, 385, 118, rgba(0, 0, 0, 255), rgba(230, 26, 35, 140), 8);
        draw_text(r, "///M DRIVETRAIN & PEDAL TRACE", 809, content_y + 108, 1, m_red);
        draw_text(r, "THR", 809, content_y + 128, 1, rgba(40, 220, 100, 255));
        fill_rect(r, 845, content_y + 129, 130, 8, rgba(16, 26, 38, 255));
        fill_rect(r, 845, content_y + 129, (int)(130.0f * state->throttle_pct / 100.0f), 8, rgba(40, 220, 100, 255));
        draw_text(r, "BRK", 995, content_y + 128, 1, m_red);
        fill_rect(r, 1030, content_y + 129, 130, 8, rgba(28, 16, 22, 255));
        fill_rect(r, 1030, content_y + 129, (int)(130.0f * state->brake_pct / 100.0f), 8, m_red);
        snprintf(rad_telem1, sizeof(rad_telem1), "SUSP %.0f/%.0f/%.0f/%.0fMM   STEER %+.1fD   RH %.0fMM",
                 state->susp_height[0], state->susp_height[1],
                 state->susp_height[2], state->susp_height[3],
                 state->steer_angle_fl_deg, state->body_height);
        draw_text(r, rad_telem1, 809, content_y + 154, 1, text_wh);
        snprintf(rad_telem1, sizeof(rad_telem1), "FUEL %.1f%% (%.1f LAPS)   ERS %+.0fKW   CLU %.0f%%",
                 state->fuel_pct, (state->fuel_burn_per_lap > 0.1f) ? (state->fuel_pct / state->fuel_burn_per_lap) : 21.2f,
                 state->energy_recovery_kw, (100.0f - state->clutch_pct));
        draw_text(r, rad_telem1, 809, content_y + 180, 1, bmw_cyan);
    }

    /* =========================================================================
     * VIEW 4: BMW MINIMALIST / PANORAMIC 3D MAP VIEW (Photo 4)
     * Single Left Diagonal Chevron with Wide Panoramic City Street Navigation Map.
     * ========================================================================= */
    else {
        int top_y = content_y + 40;
        int bot_y = content_y + content_h - 55;
        int l_elb_x = 140;
        int l_roof_x1 = 270, l_roof_x2 = 320;
        int l_floor_x1 = 340, l_floor_x2 = 390;

        /* Single Left Chevron Frame */
        draw_thick_line(r, l_elb_x, content_y + 195, l_elb_x, content_y + 265, 2, chevron_outer);
        draw_thick_line(r, l_elb_x, content_y + 195, l_roof_x1, top_y, 2, chevron_outer);
        draw_thick_line(r, l_roof_x1, top_y, l_roof_x2, top_y, 2, chevron_outer);
        draw_thick_line(r, l_elb_x, content_y + 265, l_floor_x1, bot_y, 2, chevron_outer);
        draw_thick_line(r, l_floor_x1, bot_y, l_floor_x2, bot_y, 2, chevron_outer);

        fill_rect(r, l_roof_x1 + 4, top_y - 3, 12, 4, m_blue);
        fill_rect(r, l_roof_x1 + 16, top_y - 3, 12, 4, m_dark_blue);
        fill_rect(r, l_roof_x1 + 28, top_y - 3, 12, 4, m_red);

        draw_text_right(r, "20", l_floor_x1 - 15, bot_y - 12, 1, text_wh);
        draw_thick_line(r, l_floor_x1 - 10, bot_y - 8, l_floor_x1 + 2, bot_y - 8, 2, text_wh);
        draw_text(r, ui->use_mph ? "mph" : "km/h", l_floor_x1 - 65, bot_y + 2, 1, text_dim);

        /* Left Stacked Speed + Upper Left Vitals */
        char min_l_buf[48];
        snprintf(min_l_buf, sizeof(min_l_buf), "GEAR %c  |  %.0f RPM", gear_ch, state->engine_rpm);
        draw_text(r, min_l_buf, 16, content_y + 48, 1, bmw_cyan);
        snprintf(min_l_buf, sizeof(min_l_buf), "H2O %.0f°C  OIL %.0f°C", state->water_temp, state->oil_temp);
        draw_text(r, min_l_buf, 16, content_y + 66, 1, text_dim);
        snprintf(min_l_buf, sizeof(min_l_buf), "SUSP %.0f/%.0fMM", state->susp_height[0], state->susp_height[2]);
        draw_text(r, min_l_buf, 16, content_y + 84, 1, text_wh);
        snprintf(min_l_buf, sizeof(min_l_buf), "ERS %+.0fKW", state->energy_recovery_kw);
        draw_text(r, min_l_buf, 16, content_y + 102, 1, bmw_cyan);
        snprintf(min_l_buf, sizeof(min_l_buf), "STEER %+.1f°", state->steer_angle_fl_deg);
        draw_text(r, min_l_buf, 16, content_y + 120, 1, text_dim);

        int spd_cx = 180;
        int spd_y = content_y + 245;
        char sec_spd_min[32];
        snprintf(sec_spd_min, sizeof(sec_spd_min), "%d %s", spd_sec, ui->use_mph ? "km/h" : "mph");
        draw_text_centered(r, sec_spd_min, spd_cx, spd_y - 28, 1, text_dim);

        int digits = (spd_pri >= 100) ? 3 : ((spd_pri >= 10) ? 2 : 1);
        draw_modern_number_centered(r, spd_pri, digits, false, spd_cx, spd_y, 42, 76, 9, 8, text_wh);
        draw_text(r, ui->use_mph ? "mph" : "km/h", spd_cx + 42, spd_y + 55, 1, text_dim);

        /* PANORAMIC 3D MAP CANVAS on True OLED Pitch Black (spanning across x: 330 to 1230) */
        int map_x = 330;
        int map_w = 900;
        fill_rect(r, map_x, content_y + 36, map_w, content_h - 90, rgba(0, 0, 0, 255));
        draw_rect(r, map_x, content_y + 36, map_w, content_h - 90, 1, rgba(20, 45, 80, 150));

        /* Main Avenue */
        draw_thick_line(r, map_x + 190, content_y + content_h - 60, map_x + 130, content_y + 36, 7, rgba(25, 42, 68, 255));
        draw_thick_line(r, map_x + 190, content_y + content_h - 60, map_x + 130, content_y + 36, 3, rgba(235, 242, 255, 220));

        /* Cross boulevards */
        draw_thick_line(r, map_x + 15, content_y + 130, map_x + map_w - 15, content_y + 130, 3, rgba(160, 190, 225, 190));
        draw_thick_line(r, map_x + 15, content_y + 245, map_x + map_w - 15, content_y + 245, 4, rgba(210, 230, 255, 220));
        draw_thick_line(r, map_x + 15, content_y + 375, map_x + map_w - 15, content_y + 375, 3, rgba(160, 190, 225, 190));

        /* North-South streets */
        for (int gx = map_x + 80; gx <= map_x + map_w - 60; gx += 85) {
            draw_thick_line(r, gx, content_y + 36, gx + 15, content_y + content_h - 60, 2, rgba(135, 170, 210, 180));
        }

        /* Diagonal residential streets */
        draw_thick_line(r, map_x + 240, content_y + 36, map_x + 480, content_y + 245, 2, rgba(110, 145, 185, 160));
        draw_thick_line(r, map_x + 480, content_y + 245, map_x + 720, content_y + 435, 2, rgba(110, 145, 185, 160));
        draw_thick_line(r, map_x + 360, content_y + 245, map_x + 540, content_y + 415, 2, rgba(110, 145, 185, 160));

        /* Golden Navigation Cursor */
        int nav_cx = map_x + 230;
        int nav_cy = content_y + 340;
        draw_thick_line(r, nav_cx, nav_cy - 14, nav_cx - 10, nav_cy + 10, 3, gold);
        draw_thick_line(r, nav_cx, nav_cy - 14, nav_cx + 10, nav_cy + 10, 3, gold);
        draw_thick_line(r, nav_cx - 10, nav_cy + 10, nav_cx, nav_cy + 3, 2, gold);
        draw_thick_line(r, nav_cx + 10, nav_cy + 10, nav_cx, nav_cy + 3, 2, gold);

        draw_text(r, "READY", map_x + map_w - 95, bot_y - 12, 2, text_wh);

        /* Top Motorsport Widget on True OLED Black */
        fill_rect(r, map_x + 30, content_y + 42, map_w - 60, 26, rgba(0, 0, 0, 245));
        draw_rect(r, map_x + 30, content_y + 42, map_w - 60, 26, 1, rgba(0, 180, 255, 110));
        char min_top[256];
        snprintf(min_top, sizeof(min_top), "%s   |   LAP %d/%d   |   POS %d/%d   |   BOOST %+.2f BAR   |   DELTA %+.3fs",
                 state->circuit_name[0] ? state->circuit_name : "SUZUKA CIRCUIT",
                 state->current_lap > 0 ? state->current_lap : 1,
                 state->total_laps > 0 ? state->total_laps : 10,
                 state->race_position > 0 ? state->race_position : 2,
                 state->total_cars > 0 ? state->total_cars : 16,
                 state->turbo_bar,
                 (float)state->delta_ms / 1000.0f);
        draw_text_centered(r, min_top, map_x + map_w / 2, content_y + 49, 1, text_wh);

        /* Bottom-Left Map Telemetry Overlay Pill on True OLED Black */
        fill_rect(r, map_x + 80, bot_y - 28, map_w - 200, 24, rgba(0, 0, 0, 245));
        draw_rect(r, map_x + 80, bot_y - 28, map_w - 200, 24, 1, rgba(0, 180, 255, 110));
        char s1_m[16], s2_m[16], s3_m[16];
        format_sector_time(state->cur_sector_ms[0], s1_m, sizeof(s1_m));
        format_sector_time(state->cur_sector_ms[1], s2_m, sizeof(s2_m));
        format_sector_time(state->cur_sector_ms[2], s3_m, sizeof(s3_m));
        snprintf(min_top, sizeof(min_top), "S1 %s  S2 %s  S3 %s  |  THR %.0f%% BRK %.0f%%  |  TIRES %.0f°/%.0f°/%.0f°/%.0f°",
                 s1_m, s2_m, s3_m, state->throttle_pct, state->brake_pct,
                 state->tire_temp[0], state->tire_temp[1], state->tire_temp[2], state->tire_temp[3]);
        draw_text_centered(r, min_top, map_x + 80 + (map_w - 200) / 2, bot_y - 23, 1, bmw_cyan);
    }

    /* =========================================================================
     * COMMON AUTHENTIC LEFT WARNING ICONS & BOTTOM STATUS ROW (Across All Photos)
     * ========================================================================= */
    /* 1. Far Left Warning Light Icons */
    draw_icon_headlight(r, 16, content_y + 325, rgba(40, 225, 80, 255));
    draw_icon_headlight(r, 16, content_y + 355, rgba(40, 225, 80, 255));
    draw_text(r, "A", 26, content_y + 350, 1, rgba(40, 225, 80, 255));
    bool park_active = (gear_ch == 'P') || ((state->flags & GT7_FLAG_HANDBRAKE) != 0);
    if (park_active) {
        fill_circle(r, 20, content_y + 400, 10, rgba(235, 30, 30, 255));
        fill_circle(r, 20, content_y + 400, 8, rgba(0, 0, 0, 255));
        draw_text_centered(r, "P", 20, content_y + 394, 1, rgba(235, 30, 30, 255));
    } else {
        draw_circle(r, 20, content_y + 400, 10, 1, rgba(50, 75, 105, 150));
        draw_text_centered(r, "P", 20, content_y + 394, 1, rgba(50, 75, 105, 150));
    }

    /* 2. Bottom Status Row (y = content_y + content_h - 22) */
    int stat_y = content_y + content_h - 22;

    /* Left: Battery + SOC % + Clock */
    int battery_soc = (state->battery_soc_pct > 0.0f) ? (int)state->battery_soc_pct : 71;
    draw_vector_battery(r, 65, stat_y + 2, 22, 12, battery_soc, text_wh, bmw_cyan);
    char batt_str[16];
    snprintf(batt_str, sizeof(batt_str), "%d%%", battery_soc);
    draw_text(r, batt_str, 94, stat_y + 2, 1, text_wh);

    char clock_str[16];
    get_dashboard_clock(clock_str, sizeof(clock_str), false);
    draw_text(r, clock_str, 200, stat_y, 2, text_wh);

    /* Center: Dynamic Speed limit sign (Pit / Track / Park) + Seatbelt warning + Top-down BMW silhouette */
    int lim_val = active_ev_speed_limit(state, ui);
    draw_speed_limit_sign(r, 480, stat_y + 6, lim_val, false);
    draw_icon_seatbelt(r, 580, stat_y + 5, m_red);
    draw_topdown_car(r, 620, stat_y + 6, rgba(160, 185, 215, 255));

    /* Right: Live Coolant Temp + Electric Range with Plug icon */
    char temp_str[24];
    format_ev_fluid_temp(state, ui, "H2O ", temp_str, sizeof(temp_str));
    draw_text(r, temp_str, 840, stat_y, 2, text_wh);

    draw_icon_ev_plug(r, 1045, stat_y + 8, text_wh);
    int rng_km = (int)(274.0f + (state->fuel_capacity > 0 ? (state->fuel_level / state->fuel_capacity * 100.0f) : 12.0f));
    char rng_str[24];
    snprintf(rng_str, sizeof(rng_str), "%dkm", rng_km);
    draw_text(r, rng_str, 1060, stat_y, 2, text_wh);
}









/* 3. AUDI SPORT RS VIRTUAL COCKPIT (RS Runway Tachometer, Dual Power/Torque Pods, Boost Bar) */
static void render_ev_audi_etron(SDL_Renderer *r, const gt7_state_t *state, const hud_ui_state_t *ui, uint32_t now_ms, int content_y, int content_h) {
    (void)now_ms;
    rgba_t audi_red   = rgba(235, 30, 45, 255);
    rgba_t audi_cyan  = rgba(0, 220, 245, 255);
    rgba_t audi_green = rgba(40, 225, 120, 255);
    rgba_t text_dim   = rgba(140, 160, 185, 255);
    rgba_t text_wh    = rgba(245, 250, 255, 255);
    rgba_t runway_bg  = rgba(30, 42, 60, 255);

    float speed = ui->use_mph ? state->speed_mph : state->speed_kmh;
    float rpm = state->engine_rpm;
    float max_rpm = (state->rpm_limiter > 1000) ? (float)state->rpm_limiter : 8000.0f;
    float rpm_frac = rpm / max_rpm;
    if (rpm_frac > 1.0f) rpm_frac = 1.0f;

    float power_pct = state->throttle_pct;
    float torque_pct = (state->throttle_pct > 5.0f) ? (state->throttle_pct * 0.95f) : 2.0f;
    float boost_pct = (state->turbo_bar > 0.0f) ? (state->turbo_bar / 2.0f) : 0.0f;
    if (boost_pct > 1.0f) boost_pct = 1.0f;

    int mid_y = content_y + content_h / 2;
    bool is_p = (state->speed_kmh < 0.5f && state->current_gear < 0 && state->throttle_pct < 1.0f && state->engine_rpm < 1400.0f);
    bool audi_park = is_p || ((state->flags & GT7_FLAG_HANDBRAKE) != 0);

    /* ================= TOP STATUS BAR ================= */
    int top_y = content_y + 12;

    /* Headlight & Dynamic Parking Brake */
    draw_icon_headlight(r, 410, top_y + 10, audi_green);
    draw_text(r, "(P)", 510, top_y, 2, audi_park ? audi_red : rgba(50, 65, 85, 160));

    /* Dynamic Fuel Range Red Badge */
    int audi_fuel_range = (int)(state->fuel_pct * (ui->use_mph ? 3.9f : 6.3f));
    char audi_f_buf[24];
    snprintf(audi_f_buf, sizeof(audi_f_buf), "%d %s", audi_fuel_range, ui->use_mph ? "mi" : "km");
    fill_rect(r, 400, top_y + 24, 110, 24, (state->fuel_pct < 15.0f) ? rgba(200, 30, 40, 255) : audi_red);
    draw_icon_fuel_pump(r, 410, top_y + 29, text_wh);
    draw_text_centered(r, audi_f_buf, 465, top_y + 28, 2, text_wh);

    /* Active Circuit / Category Readout */
    char aud_circ[24];
    snprintf(aud_circ, sizeof(aud_circ), "%.14s",
             state->circuit_name[0] ? state->circuit_name : "GT7 CIRCUIT");
    draw_text(r, aud_circ, 550, top_y + 28, 2, text_wh);

    /* Phone & Compass Icons */
    draw_text(r, "[T]  ^", 725, top_y + 28, 2, text_dim);
    draw_line(r, 390, top_y + 54, 930, top_y + 54, rgba(40, 55, 80, 255));

    /* ================= LEFT SIDE: Audi RS Lap Timer, Date & Time, Oil Temp, TPMS ================= */
    int left_x = 60;

    /* Upper-Left: Audi RS Lap Timer & Sector Split Card on True OLED Black */
    draw_panel(r, left_x + 30, content_y + 12, 265, 118, rgba(0, 0, 0, 255), rgba(235, 30, 45, 180), 8);
    draw_text(r, "RS LAP TIMER", left_x + 42, content_y + 20, 1, audi_red);
    if (state->car_category[0]) {
        draw_text_right(r, state->car_category, left_x + 282, content_y + 20, 1, audi_cyan);
    }
    char a_cur[24], a_bst[24], a_s1[16], a_s2[16], a_s3[16], a_tbuf[96];
    format_lap_time(state->current_lap_ms, a_cur, sizeof(a_cur));
    format_lap_time(state->best_lap_ms, a_bst, sizeof(a_bst));
    format_sector_time(state->cur_sector_ms[0], a_s1, sizeof(a_s1));
    format_sector_time(state->cur_sector_ms[1], a_s2, sizeof(a_s2));
    format_sector_time(state->cur_sector_ms[2], a_s3, sizeof(a_s3));
    snprintf(a_tbuf, sizeof(a_tbuf), "CURR %s", a_cur);
    draw_text(r, a_tbuf, left_x + 42, content_y + 38, 2, text_wh);
    snprintf(a_tbuf, sizeof(a_tbuf), "BEST %s  (%+.2fs)", a_bst, (float)state->delta_ms / 1000.0f);
    draw_text(r, a_tbuf, left_x + 42, content_y + 62, 1, (state->delta_ms <= 0) ? audi_green : audi_red);
    snprintf(a_tbuf, sizeof(a_tbuf), "S1 %s | S2 %s | S3 %s", a_s1, a_s2, a_s3);
    draw_text(r, a_tbuf, left_x + 42, content_y + 82, 1, audi_cyan);
    snprintf(a_tbuf, sizeof(a_tbuf), "FUEL %.1f%% (%.1f LAPS)  TOP %.0fK",
             state->fuel_pct, (state->fuel_burn_per_lap > 0.1f) ? (state->fuel_pct / state->fuel_burn_per_lap) : 21.2f,
             state->session_top_speed_kmh);
    draw_text(r, a_tbuf, left_x + 42, content_y + 102, 1, text_dim);

    char a_clock[16], a_wday[16], a_date[32];
    get_dashboard_clock(a_clock, sizeof(a_clock), true);
    get_dashboard_date_strings(a_wday, sizeof(a_wday), a_date, sizeof(a_date));

    draw_text(r, "Date & time", left_x + 90, mid_y - 112, 2, text_dim);
    draw_text(r, a_clock, left_x + 90, mid_y - 80, 4, text_wh);
    draw_text(r, a_wday, left_x + 90, mid_y - 44, 2, text_wh);
    draw_text(r, a_date, left_x + 90, mid_y - 22, 2, text_wh);

    /* Dynamic Oil Temperature Bar */
    int oil_y = mid_y + 90;
    float o_temp = (state->oil_temp > 20.0f) ? state->oil_temp : 102.0f;
    float o_frac = (o_temp - 60.0f) / (140.0f - 60.0f);
    if (o_frac < 0.0f) o_frac = 0.0f;
    if (o_frac > 1.0f) o_frac = 1.0f;
    int oil_fill_x = (left_x + 60) + (int)(o_frac * 170.0f);
    draw_thick_line(r, left_x + 60, oil_y, left_x + 230, oil_y, 3, rgba(30, 45, 65, 255));
    draw_thick_line(r, left_x + 60, oil_y, oil_fill_x, oil_y, 3, (o_temp > 125.0f) ? audi_red : text_wh);
    char o_buf[24];
    snprintf(o_buf, sizeof(o_buf), "OIL %.0f°C", o_temp);
    draw_text(r, o_buf, left_x + 100, oil_y + 10, 2, text_wh);

    /* Audi Sport RS TPMS Tyre Card (4-Wheel Temperatures & Contact Patches on True OLED Black) */
    draw_panel(r, left_x + 50, oil_y + 34, 220, 78, rgba(0, 0, 0, 255), rgba(35, 55, 80, 255), 8);
    draw_text(r, "SPORT RS TYRES", left_x + 62, oil_y + 42, 1, audi_red);
    char rs_tbuf[32];
    char sc_fl = (state->surface_type[0] != 0) ? (char)state->surface_type[0] : 'T';
    char sc_fr = (state->surface_type[1] != 0) ? (char)state->surface_type[1] : 'T';
    snprintf(rs_tbuf, sizeof(rs_tbuf), "FL %.0f°C [%c]  FR %.0f°C [%c]", state->tire_temp[0], sc_fl, state->tire_temp[1], sc_fr);
    draw_text(r, rs_tbuf, left_x + 62, oil_y + 60, 1, tire_temp_color(state->tire_temp[0]));
    char sc_rl = (state->surface_type[2] != 0) ? (char)state->surface_type[2] : 'T';
    char sc_rr = (state->surface_type[3] != 0) ? (char)state->surface_type[3] : 'T';
    snprintf(rs_tbuf, sizeof(rs_tbuf), "RL %.0f°C [%c]  RR %.0f°C [%c]", state->tire_temp[2], sc_rl, state->tire_temp[3], sc_rr);
    draw_text(r, rs_tbuf, left_x + 62, oil_y + 80, 1, tire_temp_color(state->tire_temp[2]));

    /* ================= CENTER: AUDI RS RUNWAY TACHOMETER ================= */
    /* Runway Hockey-stick geometry */
    int rx_start = 380, ry_start = mid_y + 115; /* READY */
    int rx_bend  = 530, ry_bend  = mid_y - 95;  /* Turn at 3.5k RPM */
    int rx_end   = 890, ry_end   = mid_y - 95;  /* 8k RPM Redline */

    /* Inactive Track */
    draw_thick_line(r, rx_start, ry_start, rx_bend, ry_bend, 14, runway_bg);
    draw_thick_line(r, rx_bend, ry_bend, rx_end, ry_end, 14, runway_bg);

    /* Redline Sector (7 to 8) on horizontal track */
    int rx_redline = rx_bend + (int)((rx_end - rx_bend) * 0.72f);
    draw_thick_line(r, rx_redline, ry_end, rx_end, ry_end, 14, rgba(140, 20, 30, 255));

    /* Graduation Labels along Runway: 1, 2, 3 along diagonal; 4, 5, 6, 7, 8 along top */
    draw_text(r, "READY", rx_start - 6, ry_start + 16, 1, text_dim);

    /* Diagonal ticks 1, 2, 3 */
    for (int i = 1; i <= 3; ++i) {
        float f = (float)i / 3.5f;
        int tx = rx_start + (int)(f * (rx_bend - rx_start));
        int ty = ry_start - (int)(f * (ry_start - ry_bend));
        draw_thick_line(r, tx - 8, ty - 6, tx + 6, ty + 8, 2, text_wh);
        char nbuf[4];
        snprintf(nbuf, sizeof(nbuf), "%d", i);
        draw_text(r, nbuf, tx - 28, ty - 20, 2, text_wh);
    }

    /* Horizontal ticks 4, 5, 6, 7, 8 */
    for (int i = 4; i <= 8; ++i) {
        float f = (float)(i - 3.5f) / 4.5f;
        int tx = rx_bend + (int)(f * (rx_end - rx_bend));
        draw_thick_line(r, tx, ry_bend - 12, tx, ry_bend + 4, 2, (i >= 7) ? audi_red : text_wh);
        char nbuf[4];
        snprintf(nbuf, sizeof(nbuf), "%d", i);
        draw_text_centered(r, nbuf, tx, ry_bend - 28, 2, (i >= 7) ? audi_red : text_wh);
    }
    draw_text_right(r, "1 / min x 1000", rx_end, ry_bend + 22, 1, text_dim);

    /* Dynamic Illuminated Runway RPM Fill */
    float diag_len = sqrtf((rx_bend - rx_start) * (rx_bend - rx_start) + (ry_start - ry_bend) * (ry_start - ry_bend));
    float horiz_len = (float)(rx_end - rx_bend);
    float total_runway = diag_len + horiz_len;
    float lit_len = rpm_frac * total_runway;

    if (lit_len > 0.0f) {
        if (lit_len <= diag_len) {
            float sub = lit_len / diag_len;
            int ex = rx_start + (int)(sub * (rx_bend - rx_start));
            int ey = ry_start - (int)(sub * (ry_start - ry_bend));
            draw_thick_line(r, rx_start, ry_start, ex, ey, 12, text_wh);
        } else {
            draw_thick_line(r, rx_start, ry_start, rx_bend, ry_bend, 12, text_wh);
            float sub = (lit_len - diag_len) / horiz_len;
            int ex = rx_bend + (int)(sub * (rx_end - rx_bend));
            draw_thick_line(r, rx_bend, ry_bend, ex, ry_end, 12, (rpm_frac > 0.85f) ? audi_red : text_wh);
        }
    }

    /* Under Runway: Large Modern Digital Speed */
    int ispd = (int)roundf(speed);
    int center_disp_x = 640;
    draw_modern_number_centered(r, ispd, 1, false, center_disp_x, mid_y - 28, 38, 62, 8, 7, text_wh);
    draw_text_centered(r, ui->use_mph ? "mph" : "km/h", center_disp_x, mid_y + 44, 1, text_dim);

    /* Dynamic Gear indicator under speed: P / R / N / D <gear> / S <gear> and DYNAMIC / AUTO / PARK */
    bool is_r = (state->current_gear == 0);
    bool is_n = (state->current_gear == -1 && !is_p);
    bool is_sport = (state->throttle_pct > 75.0f);

    if (is_p) {
        draw_modern_char(r, 'P', center_disp_x - 12, mid_y + 56, 24, 36, 5, text_wh);
    } else if (is_r) {
        draw_modern_char(r, 'R', center_disp_x - 12, mid_y + 56, 24, 36, 5, rgba(255, 195, 45, 255));
    } else if (is_n) {
        draw_modern_char(r, 'N', center_disp_x - 12, mid_y + 56, 24, 36, 5, text_wh);
    } else {
        int disp_g = (state->current_gear > 9) ? 9 : state->current_gear;
        draw_modern_char(r, is_sport ? 'S' : 'D', center_disp_x - 22, mid_y + 56, 20, 36, 5, is_sport ? audi_red : text_wh);
        draw_modern_char(r, (char)('0' + disp_g), center_disp_x + 4, mid_y + 56, 18, 36, 5, is_sport ? audi_red : text_wh);
    }
    draw_text_centered(r, is_sport ? "DYNAMIC" : (is_p ? "PARK" : "AUTO"), center_disp_x, mid_y + 98, 1, is_sport ? audi_red : text_dim);

    /* Lane Assist Icon */
    draw_icon_lane_assist(r, center_disp_x + 130, mid_y + 72, audi_green);

    /* ================= RIGHT SIDE: Audi Sport G-Meter & Dual Circular Pods (Power & Torque) ================= */
    int pod1_cx = 970;
    int pod2_cx = 1110;
    int pod_cy = mid_y + 15;
    int pod_rad = 54;

    /* Upper-Right: Audi Sport G-Meter & Chassis Dynamics Card on True OLED Black */
    draw_panel(r, pod1_cx - 45, content_y + 12, 265, 132, rgba(0, 0, 0, 255), rgba(0, 220, 245, 160), 8);
    draw_text(r, "AUDI SPORT G-METER", pod1_cx - 33, content_y + 20, 1, audi_cyan);
    int ag_cx = pod1_cx - 8, ag_cy = content_y + 66, ag_r = 22;
    draw_circle(r, ag_cx, ag_cy, ag_r, 1, rgba(60, 85, 115, 255));
    draw_line(r, ag_cx - ag_r, ag_cy, ag_cx + ag_r, ag_cy, rgba(45, 65, 95, 255));
    draw_line(r, ag_cx, ag_cy - ag_r, ag_cx, ag_cy + ag_r, rgba(45, 65, 95, 255));
    fill_circle(r, ag_cx + (int)(state->g_lat * 11.0f), ag_cy - (int)(state->g_lon * 11.0f), 4, audi_red);

    snprintf(a_tbuf, sizeof(a_tbuf), "LAT %+.2fG  LON %+.2fG", state->g_lat, state->g_lon);
    draw_text(r, a_tbuf, pod1_cx + 22, content_y + 42, 1, text_wh);
    snprintf(a_tbuf, sizeof(a_tbuf), "STEER %+.1fD  RH %.0fMM", state->steer_angle_fl_deg, state->body_height);
    draw_text(r, a_tbuf, pod1_cx + 22, content_y + 60, 1, audi_cyan);
    snprintf(a_tbuf, sizeof(a_tbuf), "SUSP %.0f/%.0f/%.0f/%.0fMM",
             state->susp_height[0], state->susp_height[1],
             state->susp_height[2], state->susp_height[3]);
    draw_text(r, a_tbuf, pod1_cx - 33, content_y + 98, 1, text_wh);
    snprintf(a_tbuf, sizeof(a_tbuf), "AERO %.0f/%.0fKG  WB %.2fM",
             state->downforce_front_kg, state->downforce_rear_kg,
             state->wheel_base_m > 0.5f ? state->wheel_base_m : 2.75f);
    draw_text(r, a_tbuf, pod1_cx - 33, content_y + 116, 1, text_dim);

    /* Pod 1: Power % (True OLED Black interior) */
    draw_text_centered(r, "Power", pod1_cx, pod_cy - pod_rad - 22, 2, text_wh);
    fill_circle(r, pod1_cx, pod_cy, pod_rad, rgba(0, 0, 0, 255));
    draw_circle(r, pod1_cx, pod_cy, pod_rad, 2, audi_cyan);
    draw_circle(r, pod1_cx, pod_cy, pod_rad - 4, 1, rgba(20, 45, 75, 255));
    draw_text_centered(r, "%", pod1_cx, pod_cy - 24, 2, text_dim);
    draw_modern_number_centered(r, (int)power_pct, 1, false, pod1_cx, pod_cy - 4, 22, 36, 5, 5, text_wh);
    draw_segmented_arc(r, pod1_cx, pod_cy, pod_rad - 6, 4, 120.0f, 420.0f, 16, power_pct / 100.0f, audi_red, rgba(0, 0, 0, 0));
    fill_rect(r, pod1_cx - 2, pod_cy + pod_rad - 8, 4, 6, audi_red);

    /* Pod 2: Torque % (True OLED Black interior) */
    draw_text_centered(r, "Torque", pod2_cx, pod_cy - pod_rad - 22, 2, text_wh);
    fill_circle(r, pod2_cx, pod_cy, pod_rad, rgba(0, 0, 0, 255));
    draw_circle(r, pod2_cx, pod_cy, pod_rad, 2, audi_cyan);
    draw_circle(r, pod2_cx, pod_cy, pod_rad - 4, 1, rgba(20, 45, 75, 255));
    draw_text_centered(r, "%", pod2_cx, pod_cy - 24, 2, text_dim);
    draw_modern_number_centered(r, (int)torque_pct, 1, false, pod2_cx, pod_cy - 4, 22, 36, 5, 5, text_wh);
    draw_segmented_arc(r, pod2_cx, pod_cy, pod_rad - 6, 4, 120.0f, 420.0f, 16, torque_pct / 100.0f, audi_red, rgba(0, 0, 0, 0));
    fill_rect(r, pod2_cx - 2, pod_cy + pod_rad - 8, 4, 6, audi_red);

    /* Horizontal Boost Bar underneath Pods */
    int bst_y = pod_cy + pod_rad + 34;
    int bst_x = pod1_cx - 40;
    int bst_w = (pod2_cx + 40) - bst_x;
    draw_text(r, "MIN", bst_x, bst_y - 2, 1, text_dim);
    draw_text_centered(r, "BOOST", bst_x + bst_w / 2, bst_y - 2, 2, text_wh);
    draw_text_right(r, "MAX", bst_x + bst_w, bst_y - 2, 1, text_dim);
    fill_rect(r, bst_x, bst_y + 16, bst_w, 4, rgba(30, 45, 65, 255));
    fill_rect(r, bst_x, bst_y + 16, (int)(bst_w * boost_pct), 4, audi_red);

    /* RS Performance & Quattro e-tron AWD Panel on True OLED Black */
    draw_panel(r, bst_x, bst_y + 28, bst_w, 48, rgba(0, 0, 0, 255), rgba(35, 55, 80, 255), 8);
    draw_text(r, "RS QUATTRO e-tron", bst_x + 12, bst_y + 34, 1, audi_red);
    char rs_perf[48];
    if (state->has_packet_tilde || fabsf(state->energy_recovery_kw) > 1.0f) {
        snprintf(rs_perf, sizeof(rs_perf), "%+.0f kW  |  AWD 46:54", state->energy_recovery_kw);
    } else {
        snprintf(rs_perf, sizeof(rs_perf), "BOOST +%.2f BAR", state->turbo_bar);
    }
    draw_text(r, rs_perf, bst_x + 12, bst_y + 54, 1, text_wh);

    /* Center Lap & Pos Info + Audi RS Pedal & Drivetrain Strip under Dynamic mode */
    char a_lap[64];
    snprintf(a_lap, sizeof(a_lap), "LAP %d/%d   P%d/%d   |   %.0f RPM",
             state->current_lap > 0 ? state->current_lap : 5,
             state->total_laps > 0 ? state->total_laps : 15,
             state->race_position > 0 ? state->race_position : 2,
             state->total_cars > 0 ? state->total_cars : 20,
             state->engine_rpm);
    draw_text_centered(r, a_lap, center_disp_x, mid_y + 114, 1, audi_cyan);

    draw_panel(r, center_disp_x - 210, mid_y + 132, 420, 62, rgba(0, 0, 0, 255), rgba(35, 55, 80, 255), 8);
    draw_text(r, "THR", center_disp_x - 196, mid_y + 140, 1, audi_green);
    fill_rect(r, center_disp_x - 164, mid_y + 141, 145, 8, rgba(16, 26, 38, 255));
    fill_rect(r, center_disp_x - 164, mid_y + 141, (int)(145.0f * state->throttle_pct / 100.0f), 8, audi_green);

    draw_text(r, "BRK", center_disp_x + 10, mid_y + 140, 1, audi_red);
    fill_rect(r, center_disp_x + 42, mid_y + 141, 145, 8, rgba(28, 16, 22, 255));
    fill_rect(r, center_disp_x + 42, mid_y + 141, (int)(145.0f * state->brake_pct / 100.0f), 8, audi_red);

    snprintf(a_lap, sizeof(a_lap), "H2O %.0f°C   OIL P %.1fB   CLUTCH %.0f%%   GBX %.0f RPM",
             state->water_temp, state->oil_pressure, (100.0f - state->clutch_pct), state->engine_rpm);
    draw_text_centered(r, a_lap, center_disp_x, mid_y + 158, 1, text_wh);
    snprintf(a_lap, sizeof(a_lap), "SLIP FL %.0f%% FR %.0f%% RL %.0f%% RR %.0f%%   |   BOOST %+.2f BAR",
             state->tire_slip_ratio[0] * 100.0f, state->tire_slip_ratio[1] * 100.0f,
             state->tire_slip_ratio[2] * 100.0f, state->tire_slip_ratio[3] * 100.0f, state->turbo_bar);
    draw_text_centered(r, a_lap, center_disp_x, mid_y + 176, 1, text_dim);

    /* ================= BOTTOM STATUS BAR ================= */
    int bot_y = content_y + content_h - 32;
    draw_thick_line(r, 180, bot_y - 14, 1100, bot_y - 14, 1, rgba(35, 50, 75, 255));

    /* Dynamic Left Odometer & Time */
    float trip_km = (state->session_distance_km > 0.05f) ?
                    state->session_distance_km :
                    ((float)state->current_lap * 4.5f + ((float)state->current_lap_ms / 60000.0f) * (state->speed_kmh * 0.015f));
    char odo_buf[24];
    snprintf(odo_buf, sizeof(odo_buf), "%d %s", (int)(8340 + trip_km), ui->use_mph ? "mi" : "km");
    draw_icon_seatbelt(r, 480, bot_y + 4, audi_red);
    draw_text(r, odo_buf, 500, bot_y - 2, 2, text_wh);
    draw_text(r, a_clock, 500, bot_y + 18, 2, text_dim);

    /* Dynamic Trip Distance */
    char trip_buf[24];
    snprintf(trip_buf, sizeof(trip_buf), "%.1f %s", ui->use_mph ? (trip_km * 0.621371f) : trip_km, ui->use_mph ? "mi" : "km");
    draw_text(r, trip_buf, 680, bot_y - 2, 2, text_wh);

    /* Live Engine Temperature */
    char a_temp_buf[24];
    format_ev_fluid_temp(state, ui, "H2O ", a_temp_buf, sizeof(a_temp_buf));
    draw_text(r, a_temp_buf, 680, bot_y + 18, 2, text_wh);
}

/* 4. VOLVO EX90 / POLESTAR PANORAMIC CLUSTER (Full-Screen Google Maps Night on OLED Black, Dual Floating Glass Pods) */
static void render_ev_volvo_ex(SDL_Renderer *r, const gt7_state_t *state, const hud_ui_state_t *ui, uint32_t now_ms, int content_y, int content_h) {
    (void)now_ms;
    rgba_t volvo_cyan  = rgba(0, 210, 240, 255);
    rgba_t volvo_green = rgba(45, 225, 115, 255);
    rgba_t volvo_gold  = rgba(245, 185, 25, 255);
    rgba_t text_dim    = rgba(145, 165, 190, 255);
    rgba_t text_wh     = rgba(245, 250, 255, 255);

    /* --- FULL-SCREEN GOOGLE MAPS NIGHT-MODE PERSPECTIVE CANVAS ON TRUE OLED BLACK --- */
    fill_rect(r, 0, content_y, 1280, content_h, rgba(0, 0, 0, 255));

    /* Building lots / neighborhood zoning blocks */
    static const SDL_Rect lot_blocks[] = {
        { 410, 160, 110, 75 },
        { 540, 170, 120, 80 },
        { 680, 165, 115, 75 },
        { 400, 380, 130, 110 },
        { 680, 390, 140, 100 }
    };
    for (size_t b = 0; b < sizeof(lot_blocks) / sizeof(lot_blocks[0]); b++) {
        fill_rect(r, lot_blocks[b].x, content_y + lot_blocks[b].y - 65, lot_blocks[b].w, lot_blocks[b].h, rgba(12, 22, 38, 175));
    }

    /* Cyan "Devonshire park" polygon */
    int park_x = 550, park_y = content_y + 270, park_w = 150, park_h = 100;
    fill_rect(r, park_x, park_y, park_w, park_h, rgba(0, 145, 190, 220));
    draw_rect(r, park_x, park_y, park_w, park_h, 1, rgba(80, 220, 255, 255));
    draw_text_centered(r, "Devonshire", park_x + park_w / 2, park_y + 35, 1, text_wh);
    draw_text_centered(r, "park", park_x + park_w / 2, park_y + 50, 1, text_wh);

    /* Dense curved residential streets with perspective fan */
    for (int off = -240; off <= 320; off += 45) {
        int x_top = 640 + off / 3;
        int x_bot = 640 + off * 2;
        draw_thick_line(r, x_top, content_y + 10, x_bot, content_y + content_h - 20, 1, rgba(32, 60, 105, 210));
    }

    /* Curving cross-arteries */
    for (int cy = 90; cy < content_h - 20; cy += 65) {
        draw_thick_line(r, 60, content_y + cy, 1220, content_y + cy + 15, 1, rgba(32, 60, 105, 210));
    }

    /* Street name labels positioned outside pods */
    draw_text(r, "Oak St", 470, content_y + 240, 1, rgba(160, 185, 215, 190));
    draw_text(r, "W 49th Ave", 720, content_y + 240, 1, rgba(160, 185, 215, 190));
    draw_text(r, "Granville St", 430, content_y + 440, 1, rgba(160, 185, 215, 190));
    draw_text(r, "W 57th Ave", 730, content_y + 440, 1, rgba(160, 185, 215, 190));

    /* Blue navigation circle puck */
    int puck_x = 610, puck_y = content_y + 450;
    fill_circle(r, puck_x, puck_y, 16, rgba(0, 125, 255, 255));
    draw_circle(r, puck_x, puck_y, 16, 2, rgba(255, 255, 255, 255));
    draw_thick_line(r, puck_x, puck_y - 8, puck_x - 5, puck_y + 6, 2, text_wh);
    draw_thick_line(r, puck_x, puck_y - 8, puck_x + 5, puck_y + 6, 2, text_wh);
    draw_thick_line(r, puck_x - 5, puck_y + 6, puck_x, puck_y + 2, 2, text_wh);
    draw_thick_line(r, puck_x + 5, puck_y + 6, puck_x, puck_y + 2, 2, text_wh);

    /* Google Logo */
    draw_text(r, "Google", 740, content_y + 510, 2, rgba(180, 200, 225, 180));

    /* --- SWEEPING OUTER BEZEL GAUGE ARCS --- */
    int arc_cy = content_y + 315;
    static const int l_speeds[] = { 200, 180, 140, 100, 60, 40, 20, 0 };
    int cur_spd = ui->use_mph ? (int)(state->speed_mph + 0.5f) : (int)(state->speed_kmh + 0.5f);
    if (cur_spd < 0) cur_spd = 0;

    for (int i = 0; i < 8; i++) {
        float theta = -0.80f + (float)i * 0.23f;
        int tx = (int)(360.0f - 305.0f * cosf(theta));
        int ty = (int)(arc_cy + 305.0f * sinf(theta));
        int mark_spd = l_speeds[i];
        rgba_t tc = (cur_spd >= mark_spd) ? text_wh : rgba(140, 165, 195, 120);

        char buf[8];
        snprintf(buf, sizeof(buf), "%d", mark_spd);
        draw_text_right(r, buf, tx - 12, ty - 6, 1, tc);
        draw_thick_line(r, tx, ty, tx + 14, ty, 3, tc);
    }

    /* Right Bezel Arc */
    for (int i = 0; i < 9; i++) {
        float theta = -0.80f + (float)i * 0.20f;
        int tx = (int)(920.0f + 305.0f * cosf(theta));
        int ty = (int)(arc_cy + 305.0f * sinf(theta));
        rgba_t tc = (i < 5) ? rgba(180, 205, 230, 200) : volvo_green;
        draw_thick_line(r, tx - 14, ty, tx, ty, 3, tc);
    }

    /* --- PROMINENT DUAL FLOATING PODS ON TRUE OLED BLACK (260x285) --- */
    int pod_w = 260, pod_h = 285;
    int pod_y = content_y + 150;
    int left_pod_x = 115;
    int right_pod_x = 905;

    /* Left Glass Pod (True OLED Black with Scandinavian Ice Border) */
    draw_panel(r, left_pod_x, pod_y, pod_w, pod_h, rgba(0, 0, 0, 255), rgba(85, 140, 195, 190), 12);

    /* Polestar Engineered Header inside Left Pod */
    draw_text(r, "POLESTAR ENGINEERED", left_pod_x + 18, pod_y + 14, 1, volvo_gold);
    char v_buf[96];
    snprintf(v_buf, sizeof(v_buf), "%.0f RPM  |  %+.2f BAR", state->engine_rpm, state->turbo_bar);
    draw_text(r, v_buf, left_pod_x + 18, pod_y + 32, 1, volvo_cyan);

    /* Dynamic Speed / Pit Limit Sign inside Left Pod */
    int v_lim = active_ev_speed_limit(state, ui);
    char v_lim_buf[8];
    snprintf(v_lim_buf, sizeof(v_lim_buf), "%d", v_lim);
    int sign_x = left_pod_x + 18, sign_y = pod_y + 82, sign_w = 42, sign_h = 44;
    fill_rect(r, sign_x, sign_y, sign_w, sign_h, rgba(250, 250, 250, 255));
    draw_rect(r, sign_x, sign_y, sign_w, sign_h, 2, (state->flags & GT7_FLAG_IN_PIT) ? rgba(225, 30, 35, 255) : rgba(20, 20, 20, 255));
    draw_text_centered(r, v_lim_buf, sign_x + sign_w / 2, sign_y + 13, 2, rgba(10, 10, 10, 255));

    /* Massive Speed Digits inside Left Pod */
    int spd_digits = (cur_spd >= 100) ? 3 : ((cur_spd >= 10) ? 2 : 1);
    draw_modern_number_centered(r, cur_spd, spd_digits, false, left_pod_x + 165, pod_y + 58, 48, 82, 10, 8, text_wh);
    draw_text_centered(r, ui->use_mph ? "mph" : "km/h", left_pod_x + 165, pod_y + 154, 2, text_dim);

    /* Lower Half of Left Pod: Pedal Bars, Temps & Chassis Ride Height */
    draw_line(r, left_pod_x + 16, pod_y + 182, left_pod_x + pod_w - 16, pod_y + 182, rgba(45, 70, 100, 255));
    draw_text(r, "THR", left_pod_x + 18, pod_y + 192, 1, volvo_green);
    fill_rect(r, left_pod_x + 50, pod_y + 193, 74, 7, rgba(18, 28, 40, 255));
    fill_rect(r, left_pod_x + 50, pod_y + 193, (int)(74.0f * state->throttle_pct / 100.0f), 7, volvo_green);
    draw_text(r, "BRK", left_pod_x + 136, pod_y + 192, 1, rgba(245, 55, 65, 255));
    fill_rect(r, left_pod_x + 168, pod_y + 193, 74, 7, rgba(28, 18, 24, 255));
    fill_rect(r, left_pod_x + 168, pod_y + 193, (int)(74.0f * state->brake_pct / 100.0f), 7, rgba(245, 55, 65, 255));

    snprintf(v_buf, sizeof(v_buf), "H2O %.0f°C   OIL %.0f°C (%.1fB)", state->water_temp, state->oil_temp, state->oil_pressure);
    draw_text(r, v_buf, left_pod_x + 18, pod_y + 214, 1, text_wh);
    snprintf(v_buf, sizeof(v_buf), "SUSP %.0f/%.0f/%.0f/%.0fMM  RH %.0f",
             state->susp_height[0], state->susp_height[1],
             state->susp_height[2], state->susp_height[3], state->body_height);
    draw_text(r, v_buf, left_pod_x + 18, pod_y + 234, 1, text_dim);
    snprintf(v_buf, sizeof(v_buf), "STEER %+.1fD  CLU %.0f%%  TOP %.0fK",
             state->steer_angle_fl_deg, (100.0f - state->clutch_pct), state->session_top_speed_kmh);
    draw_text(r, v_buf, left_pod_x + 18, pod_y + 254, 1, volvo_gold);

    /* Right Glass Pod (True OLED Black with Scandinavian Ice Border) */
    draw_panel(r, right_pod_x, pod_y, pod_w, pod_h, rgba(0, 0, 0, 255), rgba(85, 140, 195, 190), 12);

    /* Green READY badge & Active Gear Number at top right */
    draw_text(r, "READY", right_pod_x + 24, pod_y + 18, 2, volvo_green);
    snprintf(v_buf, sizeof(v_buf), "GEAR %c  (%+.0f KW)", gear_to_char(state->current_gear), state->energy_recovery_kw);
    draw_text_right(r, v_buf, right_pod_x + pod_w - 18, pod_y + 20, 1, volvo_cyan);

    /* Massive Gear Letter */
    char cur_gear = 'D';
    if (state->current_gear == 0) cur_gear = 'R';
    else if (state->current_gear >= 1) cur_gear = 'D';
    else if (state->speed_kmh < 0.2f && state->throttle_pct < 0.5f && state->engine_rpm < 1400.0f) cur_gear = 'P';
    else cur_gear = 'N';

    draw_modern_char(r, cur_gear, right_pod_x + 30, pod_y + 56, 48, 82, 10, text_wh);

    /* Vertical Gear Column */
    int col_x = right_pod_x + 100;
    rgba_t col_r = (cur_gear == 'R') ? text_wh : rgba(140, 160, 180, 120);
    rgba_t col_n = (cur_gear == 'N') ? text_wh : rgba(140, 160, 180, 120);
    rgba_t col_d = (cur_gear == 'D') ? text_wh : rgba(140, 160, 180, 120);
    rgba_t col_b = rgba(140, 160, 180, 120);

    draw_modern_char(r, 'R', col_x, pod_y + 56, 16, 22, 3, col_r);
    draw_modern_char(r, 'N', col_x, pod_y + 84, 16, 22, 3, col_n);
    draw_modern_char(r, 'D', col_x, pod_y + 112, 16, 22, 3, col_d);

    /* - B + row */
    draw_text(r, "-", col_x - 12, pod_y + 144, 2, col_b);
    draw_modern_char(r, 'B', col_x, pod_y + 140, 16, 22, 3, col_b);
    draw_text(r, "+", col_x + 20, pod_y + 144, 2, col_b);

    /* Power / Charge column on the right */
    int pwr_x = right_pod_x + 150;
    draw_text(r, "power", pwr_x, pod_y + 72, 2, rgba(225, 235, 250, 220));
    draw_thick_line(r, pwr_x - 5, pod_y + 98, pwr_x + 85, pod_y + 98, 1, rgba(140, 160, 185, 140));
    draw_text(r, "charge", pwr_x, pod_y + 110, 2, volvo_cyan);

    /* Lower Half of Right Pod: 4-Tire Temps & Contact Patches + G-Force & Lap Timing */
    draw_line(r, right_pod_x + 16, pod_y + 182, right_pod_x + pod_w - 16, pod_y + 182, rgba(45, 70, 100, 255));
    char sc_fl = (state->surface_type[0] != 0) ? (char)state->surface_type[0] : 'T';
    char sc_fr = (state->surface_type[1] != 0) ? (char)state->surface_type[1] : 'T';
    char sc_rl = (state->surface_type[2] != 0) ? (char)state->surface_type[2] : 'T';
    char sc_rr = (state->surface_type[3] != 0) ? (char)state->surface_type[3] : 'T';
    snprintf(v_buf, sizeof(v_buf), "FL %.0f°[%c] FR %.0f°[%c] RL %.0f°[%c] RR %.0f°[%c]",
             state->tire_temp[0], sc_fl, state->tire_temp[1], sc_fr,
             state->tire_temp[2], sc_rl, state->tire_temp[3], sc_rr);
    draw_text_centered(r, v_buf, right_pod_x + pod_w / 2, pod_y + 192, 1, volvo_cyan);
    snprintf(v_buf, sizeof(v_buf), "G-LOAD %+.2fL / %+.2fN   SLIP %.0f/%.0f%%",
             state->g_lat, state->g_lon, state->tire_slip_ratio[0] * 100.0f, state->tire_slip_ratio[2] * 100.0f);
    draw_text_centered(r, v_buf, right_pod_x + pod_w / 2, pod_y + 214, 1, text_wh);
    char v_cur[16], v_bst[16];
    format_lap_time(state->current_lap_ms, v_cur, sizeof(v_cur));
    format_lap_time(state->best_lap_ms, v_bst, sizeof(v_bst));
    snprintf(v_buf, sizeof(v_buf), "CURR %s  BEST %s", v_cur, v_bst);
    draw_text(r, v_buf, right_pod_x + 16, pod_y + 234, 1, volvo_green);
    snprintf(v_buf, sizeof(v_buf), "DELTA %+.3fs  |  FUEL %.1f%% (%.1fL)",
             (float)state->delta_ms / 1000.0f, state->fuel_pct,
             (state->fuel_burn_per_lap > 0.1f) ? (state->fuel_pct / state->fuel_burn_per_lap) : 21.2f);
    draw_text(r, v_buf, right_pod_x + 16, pod_y + 254, 1, volvo_gold);

    /* --- TOP STATUS BAR & POLESTAR RACE TELEMETRY PILL --- */
    draw_thick_line(r, 415, content_y + 18, 428, content_y + 13, 2, rgba(45, 225, 115, 255));
    draw_thick_line(r, 415, content_y + 24, 428, content_y + 24, 2, rgba(45, 225, 115, 255));
    draw_thick_line(r, 415, content_y + 30, 428, content_y + 35, 2, rgba(45, 225, 115, 255));
    fill_circle(r, 410, content_y + 24, 6, rgba(45, 225, 115, 255));

    char v_clock[16], v_temp_buf[24];
    get_dashboard_clock(v_clock, sizeof(v_clock), false);
    draw_text(r, v_clock, 510, content_y + 16, 2, text_wh);
    draw_text(r, "/ \\", 610, content_y + 16, 2, text_wh);
    format_ev_fluid_temp(state, ui, "H2O ", v_temp_buf, sizeof(v_temp_buf));
    draw_text(r, v_temp_buf, 680, content_y + 16, 2, text_wh);

    fill_circle(r, 815, content_y + 20, 4, rgba(225, 30, 30, 255));
    draw_thick_line(r, 813, content_y + 24, 817, content_y + 32, 2, rgba(225, 30, 30, 255));
    draw_topdown_car(r, 845, content_y + 24, text_wh);

    /* Top-Center Polestar Race Telemetry Pill on True OLED Black */
    draw_panel(r, 405, content_y + 46, 470, 40, rgba(0, 0, 0, 255), rgba(245, 185, 25, 180), 8);
    char ps_top[256], s1_v[16], s2_v[16], s3_v[16];
    format_sector_time(state->cur_sector_ms[0], s1_v, sizeof(s1_v));
    format_sector_time(state->cur_sector_ms[1], s2_v, sizeof(s2_v));
    format_sector_time(state->cur_sector_ms[2], s3_v, sizeof(s3_v));
    snprintf(ps_top, sizeof(ps_top), "%s  |  LAP %d/%d  |  POS P%d/%d  |  CAT %s",
             state->circuit_name[0] ? state->circuit_name : "CIRCUIT",
             state->current_lap > 0 ? state->current_lap : 5,
             state->total_laps > 0 ? state->total_laps : 15,
             state->race_position > 0 ? state->race_position : 2,
             state->total_cars > 0 ? state->total_cars : 20,
             state->car_category[0] ? state->car_category : "GR3");
    draw_text_centered(r, ps_top, 640, content_y + 52, 1, volvo_gold);
    snprintf(ps_top, sizeof(ps_top), "S1 %s   S2 %s   S3 %s   |   AERO %.0f/%.0fKG",
             s1_v, s2_v, s3_v, state->downforce_front_kg, state->downforce_rear_kg);
    draw_text_centered(r, ps_top, 640, content_y + 70, 1, text_wh);

    /* --- BOTTOM RIGHT: DUAL DASHED RANGE PROGRESS BARS --- */
    int rng_x = 810, rng_y = content_y + content_h - 48;
    int v_ev_fill = (int)(state->battery_soc_pct / 12.5f);
    if (v_ev_fill > 8) v_ev_fill = 8;
    draw_vector_battery(r, rng_x, rng_y, 20, 12, (int)state->battery_soc_pct, text_dim, volvo_cyan);
    draw_dashed_progress_bar(r, rng_x + 28, rng_y + 2, 8, v_ev_fill > 0 ? v_ev_fill : 7, 8, 8, 4, volvo_cyan, rgba(45, 60, 80, 200));
    int v_ev_range = (int)(state->battery_soc_pct * (ui->use_mph ? 2.8f : 4.5f));
    char v_rng_buf[24];
    snprintf(v_rng_buf, sizeof(v_rng_buf), "%d %s", v_ev_range > 0 ? v_ev_range : 420, ui->use_mph ? "mi" : "km");
    draw_text(r, v_rng_buf, rng_x + 130, rng_y, 2, text_wh);

    int v_fuel_fill = (int)(state->fuel_pct / 12.5f);
    if (v_fuel_fill > 8) v_fuel_fill = 8;
    draw_vector_fuel_pump(r, rng_x, rng_y + 22, 14, text_wh);
    draw_dashed_progress_bar(r, rng_x + 28, rng_y + 25, 8, v_fuel_fill > 0 ? v_fuel_fill : 6, 8, 8, 4, text_wh, rgba(45, 60, 80, 200));
    int v_fuel_range = (int)(state->fuel_pct * (ui->use_mph ? 3.6f : 5.8f));
    snprintf(v_rng_buf, sizeof(v_rng_buf), "%d %s", v_fuel_range > 0 ? v_fuel_range : 540, ui->use_mph ? "mi" : "km");
    draw_text(r, v_rng_buf, rng_x + 130, rng_y + 22, 2, text_wh);
}




void hud_render_theme_ev_cockpit(SDL_Renderer *r, const gt7_state_t *state, const hud_ui_state_t *ui, uint32_t now_ms, int top_y, int bot_y) {
    draw_ev_brand_selector(r, ui, top_y);
    int content_y = top_y + 40;
    int content_h = bot_y - content_y;

    switch (ui->ev_style) {
        case EV_STYLE_BENZ_EQ:    render_ev_benz_eq(r, state, ui, now_ms, content_y, content_h); break;
        case EV_STYLE_BMW_I:      render_ev_bmw_i(r, state, ui, now_ms, content_y, content_h); break;
        case EV_STYLE_AUDI_ETRON: render_ev_audi_etron(r, state, ui, now_ms, content_y, content_h); break;
        case EV_STYLE_VOLVO_EX:   render_ev_volvo_ex(r, state, ui, now_ms, content_y, content_h); break;
        default:                  render_ev_benz_eq(r, state, ui, now_ms, content_y, content_h); break;
    }
}
