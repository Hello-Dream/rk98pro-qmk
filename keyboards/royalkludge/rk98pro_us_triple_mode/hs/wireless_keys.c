// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "hs/wireless_glue.h"
#include "hs/wireless_internal.h"
#include "hs/store.h"

#include "deferred_exec.h"

typedef struct {
    uint16_t       keycode;
    deferred_token token;
} wls_long_press_ctx_t;

// 每个通道独立计时，避免多键同时操作时共用同一个 keycode/延迟任务
static wls_long_press_ctx_t wls_long_press_ctxs[] = {
    {.keycode = KC_BT1, .token = INVALID_DEFERRED_TOKEN},
    {.keycode = KC_BT2, .token = INVALID_DEFERRED_TOKEN},
    {.keycode = KC_BT3, .token = INVALID_DEFERRED_TOKEN},
    {.keycode = KC_2G4, .token = INVALID_DEFERRED_TOKEN},
    {.keycode = KC_USB, .token = INVALID_DEFERRED_TOKEN},
};

static wls_long_press_ctx_t *wls_long_press_ctx_get(uint16_t keycode) {
    for (uint8_t i = 0; i < ARRAY_SIZE(wls_long_press_ctxs); i++) {
        if (wls_long_press_ctxs[i].keycode == keycode) {
            return &wls_long_press_ctxs[i];
        }
    }

    return NULL;
}

uint32_t wls_process_long_press(uint32_t trigger_time, void *cb_arg) {
    wls_long_press_ctx_t *ctx = (wls_long_press_ctx_t *)cb_arg;

    if (ctx == NULL) {
        return 0;
    }

    uint16_t keycode = ctx->keycode;
    ctx->token       = INVALID_DEFERRED_TOKEN;

    switch (keycode) {
        case KC_BT1: {
            uint8_t mode = hs_settings.wireless_mode;
            hs_modeio_detection(true, &mode, hs_settings.last_bt_channel);
            if ((mode == hs_bt) || (mode == hs_wireless) || (mode == hs_none)) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_BT1, true);
                state = true;
                hs_rgb_blink_set_timer(timer_read32());
            }

        } break;
        case KC_BT2: {
            uint8_t mode = hs_settings.wireless_mode;
            hs_modeio_detection(true, &mode, hs_settings.last_bt_channel);
            if ((mode == hs_bt) || (mode == hs_wireless) || (mode == hs_none)) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_BT2, true);
                state = true;
                hs_rgb_blink_set_timer(timer_read32());
            }
        } break;
        case KC_BT3: {
            uint8_t mode = hs_settings.wireless_mode;
            hs_modeio_detection(true, &mode, hs_settings.last_bt_channel);
            if ((mode == hs_bt) || (mode == hs_wireless) || (mode == hs_none)) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_BT3, true);
                state = true;
                hs_rgb_blink_set_timer(timer_read32());
            }
        } break;
        case KC_2G4: {
            uint8_t mode = hs_settings.wireless_mode;
            hs_modeio_detection(true, &mode, hs_settings.last_bt_channel);
            if ((mode == hs_2g4) || (mode == hs_wireless) || (mode == hs_none)) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_2G4, true);
                state = true;
                hs_rgb_blink_set_timer(timer_read32());
            }
        } break;
        default:
            break;
    }
    return 0;
}

bool process_record_wls(uint16_t keycode, keyrecord_t *record) {

#    ifndef WLS_KEYCODE_PAIR_TIME
#        define WLS_KEYCODE_PAIR_TIME 3000
#    endif

#    define WLS_KEYCODE_EXEC(wls_dev)                                                                                          \
        do {                                                                                                                   \
            wls_long_press_ctx_t *ctx = wls_long_press_ctx_get(keycode);                                                       \
            if (ctx == NULL) break;                                                                                            \
            if (record->event.pressed) {                                                                                       \
                if (wireless_get_current_devs() != wls_dev)                                                                    \
                    wireless_devs_change(wireless_get_current_devs(), wls_dev, false);                                         \
                if (ctx->token == INVALID_DEFERRED_TOKEN) {                                                                    \
                    ctx->token = defer_exec(WLS_KEYCODE_PAIR_TIME, wls_process_long_press, ctx);                               \
                }                                                                                                              \
            } else {                                                                                                           \
                cancel_deferred_exec(ctx->token);                                                                              \
                ctx->token = INVALID_DEFERRED_TOKEN;                                                                           \
            }                                                                                                                  \
        } while (false)

    switch (keycode) {
        case KC_BT1: {
            uint8_t mode = hs_settings.wireless_mode;
            hs_modeio_detection(true, &mode, hs_settings.last_bt_channel);
            if ((mode == hs_bt) || (mode == hs_wireless) || (mode == hs_none)) {
                WLS_KEYCODE_EXEC(DEVS_BT1);

                if (record->event.pressed) {
                    suspend_wakeup_init();
                    state = false;
                    hs_rgb_blink_set_timer(timer_read32());
                }
            }
            md_send_devctrl(0x80+HS_WIRESS_SLEEP_TIME);
        } break;
        case KC_BT2: {
            uint8_t mode = hs_settings.wireless_mode;
            hs_modeio_detection(true, &mode, hs_settings.last_bt_channel);
            if ((mode == hs_bt) || (mode == hs_wireless) || (mode == hs_none)) {
                WLS_KEYCODE_EXEC(DEVS_BT2);

                if (record->event.pressed) {
                    suspend_wakeup_init();
                    state = false;
                    hs_rgb_blink_set_timer(timer_read32());
                }
            }
            md_send_devctrl(0x80+HS_WIRESS_SLEEP_TIME);
        } break;
        case KC_BT3: {
            uint8_t mode = hs_settings.wireless_mode;
            hs_modeio_detection(true, &mode, hs_settings.last_bt_channel);
            if ((mode == hs_bt) || (mode == hs_wireless) || (mode == hs_none)) {
                WLS_KEYCODE_EXEC(DEVS_BT3);

                if (record->event.pressed) {
                    suspend_wakeup_init();
                    state = false;
                    hs_rgb_blink_set_timer(timer_read32());
                }
            }
            md_send_devctrl(0x80+HS_WIRESS_SLEEP_TIME);
        } break;
        case KC_2G4: {
            uint8_t mode = hs_settings.wireless_mode;
            hs_modeio_detection(true, &mode, hs_settings.last_bt_channel);
            if ((mode == hs_2g4) || (mode == hs_wireless) || (mode == hs_none)) {
                WLS_KEYCODE_EXEC(DEVS_2G4);

                if (record->event.pressed) {
                    suspend_wakeup_init();
                    state = false;
                    hs_rgb_blink_set_timer(timer_read32());
                }
            }
            md_send_devctrl(0xFF);
        } break;
        case KC_USB: {
            uint8_t mode = hs_settings.wireless_mode;
            hs_modeio_detection(true, &mode, hs_settings.last_bt_channel);
            if ((mode == hs_2g4) || (mode == hs_wireless) || (mode == hs_none)) {
                WLS_KEYCODE_EXEC(DEVS_USB);
                state = false;
                hs_rgb_blink_set_timer(timer_read32());
            }
        } break;
        default:
            return true;
    }

    return false;
}
