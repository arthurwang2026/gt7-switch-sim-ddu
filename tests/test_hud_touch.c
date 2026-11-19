#include "hud_internal.h"
#include "gt7_telemetry.h"

#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void press_key_idx(hud_ui_state_t *ui, gt7_client_t *client, gt7_state_t *state, int idx, int y_off) {
    int row = idx / 3;
    int col = idx % 3;
    int x = IP_KEY_GX + col * (IP_KEY_W + IP_KEY_GAP) + IP_KEY_W / 2;
    int y = IP_KEY_GY + row * (IP_KEY_H + IP_KEY_GAP) + y_off;
    bool handled = hud_handle_touch_down(ui, client, state, x, y, 1000);
    assert(handled);
}

static void test_ip_modal_keypad_and_protocols(void) {
    gt7_client_t client;
    gt7_state_t state;
    hud_ui_state_t ui;

    assert(gt7_init(&client, &state, "AUTO"));
    hud_init(&ui, "AUTO");

    /* Open IP modal by tapping IP button on bottom dock */
    assert(!ui.show_ip_modal);
    assert(hud_handle_touch_down(&ui, &client, &state, 793, 694, 1000));
    assert(ui.show_ip_modal);
    /* Opening modal in AUTO mode should prefill detected subnet prefix + '.' when available */
    size_t prefill_len = strlen(ui.edit_ip);
    if (client.subnet_prefix[0]) {
        assert(strncmp(ui.edit_ip, client.subnet_prefix, strlen(client.subnet_prefix)) == 0);
    }

    /* Clear any prefilled prefix via DEL key (tests DEL hitbox & cursor decrement) */
    for (size_t c = 0; c < prefill_len + 1; ++c) {
        press_key_idx(&ui, &client, &state, 11, IP_KEY_H / 2);
    }
    assert(strcmp(ui.edit_ip, "") == 0);

    /* Ensure top pixel of rendered key height responds accurately (fixes 132 vs 134 offset) */
    assert(hud_handle_touch_down(&ui, &client, &state, IP_KEY_GX + 10, IP_KEY_GY, 1010));
    assert(strcmp(ui.edit_ip, "1") == 0);
    /* Tap DEL (idx 11) and verify empty */
    press_key_idx(&ui, &client, &state, 11, IP_KEY_H / 2);
    assert(strcmp(ui.edit_ip, "") == 0);

    /* Key indices: 0:'1', 1:'2', 2:'3', 3:'4', 4:'5', 5:'6', 6:'7', 7:'8', 8:'9', 9:'.', 10:'0', 11:'DEL' */
    const int seq[] = {0, 8, 1, 9, 0, 5, 7, 9, 10, 9, 0, 5, 3, 11, 4}; /* 192.168.0.164 -> DEL -> 5 */
    for (size_t i = 0; i < sizeof(seq) / sizeof(seq[0]); ++i) {
        press_key_idx(&ui, &client, &state, seq[i], IP_KEY_H / 2);
    }
    assert(strcmp(ui.edit_ip, "192.168.0.165") == 0);

    /* Test all 6 protocol buttons match gt7_heartbeat_mode_t 0..5 */
    for (int p = 0; p < IP_PROTO_COUNT; ++p) {
        int px = IP_PROTO_X_START + p * (IP_PROTO_W + IP_PROTO_GAP) + IP_PROTO_W / 2;
        int py = IP_PROTO_BTN_Y + IP_PROTO_H / 2;
        assert(hud_handle_touch_down(&ui, &client, &state, px, py, 1020 + (uint32_t)p));
        assert(client.heartbeat_mode == (gt7_heartbeat_mode_t)p);
    }

    /* Reset protocol back to AUTO (mode 0) */
    assert(hud_handle_touch_down(&ui, &client, &state,
                                 IP_PROTO_X_START + IP_PROTO_W / 2,
                                 IP_PROTO_BTN_Y + IP_PROTO_H / 2, 1030));
    assert(client.heartbeat_mode == GT7_HEARTBEAT_AUTO);

    /* Tap LOCK MANUAL IP button */
    assert(hud_handle_touch_down(&ui, &client, &state,
                                 IP_KEY_GX + 272, IP_ACTION_BTN_Y + IP_ACTION_BTN_H / 2, 1040));
    assert(!ui.show_ip_modal);
    assert(!client.auto_detect);
    assert(strcmp(client.ps5_ip, "192.168.0.165") == 0);

    /* Reopen modal and tap AUTO DETECT button */
    assert(hud_handle_touch_down(&ui, &client, &state, 793, 694, 1050));
    assert(ui.show_ip_modal);
    assert(hud_handle_touch_down(&ui, &client, &state,
                                 IP_KEY_GX + 90, IP_ACTION_BTN_Y + IP_ACTION_BTN_H / 2, 1060));
    assert(!ui.show_ip_modal);
    assert(client.auto_detect);
    assert(strcmp(client.ps5_ip, "AUTO") == 0);

    gt7_close(&client);
    printf("PASS: test_ip_modal_keypad_and_protocols\n");
}

static void test_offscreen_render_all_themes(void) {
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init(dummy) skipped/failed: %s\n", SDL_GetError());
        exit(1);
    }
    SDL_Window *win = SDL_CreateWindow("HUD Test", 0, 0, SCREEN_W, SCREEN_H, SDL_WINDOW_HIDDEN);
    assert(win != NULL);
    SDL_Renderer *r = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    assert(r != NULL);

    gt7_client_t client;
    gt7_state_t state;
    hud_ui_state_t ui;
    assert(gt7_init(&client, &state, "AUTO"));
    gt7_update_demo(&state, 2000, 2);
    state.flags |= GT7_FLAG_IN_PIT | GT7_FLAG_HANDBRAKE;
    state.water_temp = 91.0f;
    state.oil_temp = 98.0f;
    snprintf(state.circuit_name, sizeof(state.circuit_name), "SUZUKA CIRCUIT");
    hud_init(&ui, "AUTO");

    for (int t = 0; t < HUD_THEME_COUNT; ++t) {
        ui.theme = (hud_theme_t)t;
        ui.use_mph = (t % 2 == 1);
        if (t == HUD_THEME_EV_COCKPIT) {
            for (int e = 0; e < EV_STYLE_COUNT; ++e) {
                ui.ev_style = (ev_style_t)e;
                for (int b = 0; b < (e == EV_STYLE_BMW_I ? BMW_VIEW_COUNT : 1); ++b) {
                    ui.bmw_view = (bmw_view_t)b;
                    hud_render(r, &state, &client, &ui, 2500 + (uint32_t)(e * 10 + b));
                }
            }
        } else {
            hud_render(r, &state, &client, &ui, 2500 + (uint32_t)t);
        }
    }

    /* Also render IP modal overlay + fullscreen mode */
    ui.show_ip_modal = true;
    ui.fullscreen_canvas = true;
    hud_render(r, &state, &client, &ui, 3000);

    gt7_close(&client);
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(win);
    SDL_Quit();
    printf("PASS: test_offscreen_render_all_themes\n");
}

int main(void) {
    test_ip_modal_keypad_and_protocols();
    test_offscreen_render_all_themes();
    printf("ALL HUD & TOUCH TESTS PASSED (2/2 suites)\n");
    return 0;
}
