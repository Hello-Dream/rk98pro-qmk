// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "hs/wireless_glue.h"
#include "hs/indicators.h"
#include "hs/power.h"
#include "hs/store.h"

#include "usb_main.h"
#include "lowpower.h"

#define HS_WLS_FORCE_TIMEOUT (HS_PAIR_TIMEOUT + HS_SUCCEED_TIME + 1000)
#include "rgb_record/rgb_record.h"

extern bool lower_sleep;

uint32_t post_init_timer     = 0x00;
bool state                   = false;
bool linker_ok        = false;
static uint32_t hs_connect_ok_time = 0;



static ioline_t col_pins[MATRIX_COLS] = MATRIX_COL_PINS;
extern bool state;
bool hs_modeio_detection(bool update, uint8_t *mode, uint8_t lsat_btdev) {
    static uint32_t scan_timer = 0x00;

    if ((update != true) && (timer_elapsed32(scan_timer) <= (HS_MODEIO_DETECTION_TIME))) {
        return false;
    }
    scan_timer = timer_read32();

#if defined(HS_BT_DEF_PIN) && defined(HS_2G4_DEF_PIN)
    uint8_t now_mode         = 0x00;
    uint8_t hs_mode          = 0x00;
    static uint8_t last_mode = 0x00;
    bool sw_mode             = false;
    now_mode                 = (HS_GET_MODE_PIN(HS_USB_PIN_STATE) ? 3 : (HS_GET_MODE_PIN(HS_BT_PIN_STATE) ? 1 : ((HS_GET_MODE_PIN(HS_2G4_PIN_STATE) ? 2 : 0))));
    hs_mode                  = (*mode >= DEVS_BT1 && *mode <= DEVS_BT5) ? 1 : ((*mode == DEVS_2G4) ? 2 : ((*mode == DEVS_USB) ? 3 : 0));
    sw_mode                  = ((update || (last_mode == now_mode)) && (hs_mode != now_mode)) ? true : false;
    last_mode                = now_mode;

    switch (now_mode) {
        case 1:
            *mode = hs_bt;
            if (sw_mode) {
                wireless_devs_change(wireless_get_current_devs(), lsat_btdev, false);
            }
            break;
        case 2:
            *mode = hs_2g4;
            if (sw_mode) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_2G4, false);
            }
            break;
        case 3:
            *mode = hs_usb;
            if (sw_mode)
                wireless_devs_change(wireless_get_current_devs(), DEVS_USB, false);

            break;
        default:
            break;
    }

    if (sw_mode) {
        hs_rgb_blink_set_timer(timer_read32());
        suspend_wakeup_init();
        return true;
    }
#else
    *mode = hs_none;
#endif

    return false;
}

static uint32_t hs_linker_rgb_timer = 0x00;

bool hs_mode_scan(bool update, uint8_t moude, uint8_t lsat_btdev) {

    if (hs_modeio_detection(update, &moude, lsat_btdev)) {

        return true;
    }
    hs_rgb_blink_hook();
    return false;
}

void hs_rgb_blink_set_timer(uint32_t time) {
    hs_linker_rgb_timer = time;
}

uint32_t hs_rgb_blink_get_timer(void) {
    return hs_linker_rgb_timer;
}

bool hs_rgb_blink_hook() {
    static uint8_t last_status;
    uint32_t timeout = 0;
    static bool hs_connect = false;

    if (last_status != *md_getp_state()) {
        last_status = *md_getp_state();
        hs_rgb_blink_set_timer(0x00);
    }

    switch (hs_get_sleep_timeout()) {
        case hs_sleep_timeout_none: {
            timeout = 0;
        } break;
        case hs_sleep_timeout_1min: {
            timeout = 1 * 60000;
        } break;
        case hs_sleep_timeout_3min: {
            timeout = 3 * 60000;
        } break;
        case hs_sleep_timeout_5min: {
            timeout = 5 * 60000;
        } break;
        case hs_sleep_timeout_10min: {
            timeout = 10 * 60000;
        } break;
        case hs_sleep_timeout_20min: {
            timeout = 20 * 60000;
        } break;
        case hs_sleep_timeout_30min: {
            timeout = 30 * 60000;
        } break;
        case hs_sleep_timeout_vendor: {
            timeout = HS_SLEEP_TIMEOUT;
        } break;
    }

    if (wireless_get_current_devs() == DEVS_USB && USB_DRIVER.state == USB_ACTIVE) {
        hs_rgb_blink_set_timer(0x00);
        return true;
    }

    switch (*md_getp_state()) {
        case MD_STATE_NONE: {
            hs_rgb_blink_set_timer(0x00);
        } break;

        case MD_STATE_DISCONNECTED:{
            if (hs_rgb_blink_get_timer() == 0x00) {
                hs_rgb_blink_set_timer(timer_read32());
                if (hs_connect) {
                    hs_connect = false;
                    extern void wireless_devs_change_kb(uint8_t old_devs, uint8_t new_devs, bool reset);
                    wireless_devs_change_kb(wireless_get_current_devs(), wireless_get_current_devs(), false);
                }
            } else {
                if (!state){
                    if ((timer_elapsed32(hs_rgb_blink_get_timer()) >= HS_LBACK_TIMEOUT) && !rgbrec_is_started()) {
                        hs_rgb_blink_set_timer(timer_read32());
                        md_send_devctrl(MD_SND_CMD_DEVCTRL_USB);
                        wait_ms(200);
                        lpwr_set_timeout_manual(true);
                    }
                }
                else{
                    if ((timer_elapsed32(hs_rgb_blink_get_timer()) >= HS_PAIR_TIMEOUT) && !rgbrec_is_started()) {
                        hs_rgb_blink_set_timer(timer_read32());
                        md_send_devctrl(MD_SND_CMD_DEVCTRL_USB);
                        wait_ms(200);
                        lpwr_set_timeout_manual(true);
                    }
                }
            }
        } break;
        case MD_STATE_CONNECTED:{
            hs_connect = true;
            if (hs_rgb_blink_get_timer() == 0x00) {
                hs_rgb_blink_set_timer(timer_read32());
            } else {
                if (timeout && timer_elapsed32(hs_rgb_blink_get_timer()) >= timeout && !rgbrec_is_started()) {
                    hs_rgb_blink_set_timer(timer_read32());
                    clear_keyboard(); // 进睡前清键，避免唤醒后卡键
                    lpwr_set_timeout_manual(true);
                }
            }
        } break;

        case MD_STATE_PAIRING:{
            if (hs_rgb_blink_get_timer() == 0x00) {
                hs_rgb_blink_set_timer(timer_read32());

            } else {
                uint32_t hs_pair_timer = HS_PAIR_TIMEOUT;
                if (wireless_get_current_devs() == DEVS_2G4) {
                    hs_pair_timer = HS_PAIR_TIMEOUT1;
                } else if (wireless_get_current_devs() == DEVS_BT1 || wireless_get_current_devs() == DEVS_BT2 || wireless_get_current_devs() == DEVS_BT3){
                    hs_pair_timer = HS_PAIR_TIMEOUT;
                }

                if (timer_elapsed32(hs_rgb_blink_get_timer()) >= hs_pair_timer && !rgbrec_is_started()) {
                    hs_rgb_blink_set_timer(timer_read32());
                    md_send_devctrl(MD_SND_CMD_DEVCTRL_USB);
                    wait_ms(200);
                    lpwr_set_timeout_manual(true);
                }
            }
        } break;
        default:break;
    }
    return true;
}

void lpwr_exti_init_hook(void) {

#ifdef HS_BT_DEF_PIN
    gpio_set_pin_input_high(HS_BT_DEF_PIN);
    waitInputPinDelay();
    palEnableLineEvent(HS_BT_DEF_PIN, PAL_EVENT_MODE_BOTH_EDGES);
#endif

#ifdef HS_2G4_DEF_PIN
    gpio_set_pin_input_high(HS_2G4_DEF_PIN);
    waitInputPinDelay();
    palEnableLineEvent(HS_2G4_DEF_PIN, PAL_EVENT_MODE_BOTH_EDGES);
#endif

    if (lower_sleep) {
#if DIODE_DIRECTION == ROW2COL
        for (uint8_t i = 0; i < ARRAY_SIZE(col_pins); i++) {
            if (col_pins[i] != NO_PIN) {
                gpio_set_pin_output(col_pins[i]);
                gpio_write_pin_high(col_pins[i]);
            }
        }
#endif
    }

    gpio_set_pin_input(HS_BAT_CABLE_PIN);
    waitInputPinDelay();
    palEnableLineEvent(HS_BAT_CABLE_PIN, PAL_EVENT_MODE_RISING_EDGE);
    palEnableLineEvent(A12,PAL_EVENT_MODE_RISING_EDGE);
    nvicEnableVector(USBP_WKUP_IRQn,6);
}

void palcallback_cb(uint8_t line) {
    switch (line) {
        case PAL_PAD(A12): {
            lpwr_set_sleep_wakeupcd(LPWR_WAKEUP_USB);
        } break;
        case PAL_PAD(HS_BAT_CABLE_PIN): {
            lpwr_set_sleep_wakeupcd(LPWR_WAKEUP_CABLE);
        } break;
#ifdef HS_BT_DEF_PIN
        case PAL_PAD(HS_2G4_DEF_PIN): {
            lpwr_set_sleep_wakeupcd(LPWR_WAKEUP_SWITCH);
        } break;
#endif

#ifdef HS_2G4_DEF_PIN
        case PAL_PAD(HS_BT_DEF_PIN): {
            lpwr_set_sleep_wakeupcd(LPWR_WAKEUP_SWITCH);
        } break;
#endif
        default: {

        } break;
    }
}

void lpwr_stop_hook_pre(void) { // wireless sleep
    hs_store_flush();

#ifdef LED_POWER_EN_PIN
    gpio_write_pin_low(LED_POWER_EN_PIN);
#endif

    // 睡眠前把指示灯置灭（LED_PIN_ON_STATE=0，写 1 为灭），对齐原厂
#ifdef LED_NUM_PIN
    gpio_write_pin(LED_NUM_PIN, !LED_PIN_ON_STATE);
#endif
#ifdef LED_SCROLL_PIN
    gpio_write_pin(LED_SCROLL_PIN, !LED_PIN_ON_STATE);
#endif
#ifdef LED_CAPS_PIN
    gpio_write_pin(LED_CAPS_PIN, !LED_PIN_ON_STATE);
#endif
#ifdef LED_WIN_PIN
    gpio_write_pin(LED_WIN_PIN, !LED_PIN_ON_STATE);
#endif
#ifdef LED_POV_PIN
    gpio_write_pin(LED_POV_PIN, !LED_PIN_ON_STATE);
#endif
#ifdef LED_MAC_PIN
    gpio_write_pin(LED_MAC_PIN, !LED_PIN_ON_STATE);
#endif

    state = false;
    if (lower_sleep) {
        md_send_devctrl(MD_SND_CMD_DEVCTRL_USB);
        wait_ms(200);
        lpwr_set_sleep_wakeupcd(LPWR_WAKEUP_UART);
    }
}

void lpwr_stop_hook_post(void) {
    if (lower_sleep) {
        switch (lpwr_get_sleep_wakeupcd()) {
            case LPWR_WAKEUP_USB:
            case LPWR_WAKEUP_CABLE: {
                lower_sleep = false;
                lpwr_set_state(LPWR_WAKEUP);
            } break;
            default: {
                lpwr_set_state(LPWR_STOP);
            } break;
        }
    }
}

// VIA label 控件要求返回以 '\0' 结尾的 UTF-8 字符串


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


void wireless_post_task(void) {

    // auto switching devs
    if (post_init_timer && timer_elapsed32(post_init_timer) >= 100) {

        md_send_devctrl(MD_SND_CMD_DEVCTRL_FW_VERSION);   // get the module fw version.
        md_send_devctrl(MD_SND_CMD_DEVCTRL_SLEEP_BT_EN);  // timeout 30min to sleep in bt mode, enable
        md_send_devctrl(MD_SND_CMD_DEVCTRL_SLEEP_2G4_EN); // timeout 30min to sleep in 2.4g mode, enable
        wireless_devs_change(!hs_settings.wireless_mode, hs_settings.wireless_mode, false);
        post_init_timer = 0x00;
    }

    hs_mode_scan(false, hs_settings.wireless_mode, hs_settings.last_bt_channel);
}

void hs_set_sleep_state(void) {
    /* Set the sleep state of wireless mode */
    static uint8_t last_devs_mode = 0xFF;
    if (wireless_get_current_devs() != last_devs_mode) {
        last_devs_mode = wireless_get_current_devs();

        // 与 process_record_wls() 保持一致：蓝牙下发深度睡眠时间(0x80+n 分钟)，2.4G 下发 0xFF
        if (last_devs_mode == DEVS_2G4) {
            md_send_devctrl(0xFF);
        } else if ((last_devs_mode >= DEVS_BT1) && (last_devs_mode <= DEVS_BT3)) {
            md_send_devctrl(0x80 + HS_WIRESS_SLEEP_TIME);
        }
    }
}

void lpwr_wakeup_hook(void) { // wireless wakeup
#if defined(WIRELESS_ENABLE) && defined(RGB_MATRIX_ENABLE)
    // 睡眠期间 LED 供电已关闭；清掉旧的临时点亮状态，唤醒后按新的连接状态重新申请
    hs_wls_force_rgb        = false;
    wls_rgb_indicator_timer = 0;
#endif

    if (wireless_get_current_devs() == DEVS_USB && USB_DRIVER.state == USB_STOP) {
        usb_power_connect();
        restart_usb_driver(&USBD1);
        wireless_devs_change(!DEVS_USB, DEVS_USB, false);
    }
    hs_mode_scan(false, hs_settings.wireless_mode, hs_settings.last_bt_channel);

    hs_power_activity();
    hs_led_power_update();
    hs_indicator_rgb_release(); // 用户背光关闭且没有新的临时指示时，清掉睡眠前残留的 RGB 使能
}

void hs_rgb_blink_hook_user(void) {

    if (wireless_get_current_devs() == DEVS_USB) {
        if (USB_DRIVER.state == USB_ACTIVE) {
            if (linker_ok == false && timer_elapsed32(hs_connect_ok_time) >= HS_SUCCEED_TIME) {
                linker_ok          = true;
                hs_connect_ok_time = timer_read32();
            }
        } else {
            linker_ok          = false;
            hs_connect_ok_time = timer_read32();
        }
        return;
    }

    switch (*md_getp_state()) {
        case MD_STATE_DISCONNECTED:
        case MD_STATE_PAIRING: {
            linker_ok          = false;
            hs_connect_ok_time = timer_read32();
        } break;
        case MD_STATE_CONNECTED: {
            if (linker_ok == false) {
                if (timer_elapsed32(hs_connect_ok_time) < HS_SUCCEED_TIME) {
                    hs_wls_indicator_force_begin(); // 背光关闭时也要显示连接成功灯
                } else {
                    linker_ok          = true;
                    hs_connect_ok_time = timer_read32();
                }
            }
        } break;
        default:
            break;
    }
}


#ifdef RGB_MATRIX_ENABLE

#    ifdef WIRELESS_ENABLE
bool wls_rgb_indicator_reset        = false;
uint32_t wls_rgb_indicator_timer    = 0x00;
uint32_t wls_rgb_indicator_interval = 0;
uint32_t wls_rgb_indicator_times    = 0;
uint32_t wls_rgb_indicator_index    = 0;
RGB wls_rgb_indicator_rgb           = {0};

void rgb_matrix_wls_indicator_set(uint8_t index, RGB rgb, uint32_t interval, uint8_t times) {

    hs_wls_force_timer = timer_read32();
    hs_wls_indicator_force_begin(); // 背光关闭时也要显示连接/配对指示

    wls_rgb_indicator_timer = timer_read32();

    wls_rgb_indicator_index    = index;
    wls_rgb_indicator_interval = interval;
    wls_rgb_indicator_times    = times * 2;
    wls_rgb_indicator_rgb      = rgb;
}

void wireless_devs_change_kb(uint8_t old_devs, uint8_t new_devs, bool reset) {

    wls_rgb_indicator_reset = reset;

    if (hs_settings.wireless_mode != wireless_get_current_devs()) {
        hs_settings.wireless_mode = wireless_get_current_devs();
        if (hs_settings.wireless_mode > 0 && hs_settings.wireless_mode < 4) hs_settings.last_bt_channel = hs_settings.wireless_mode;
        hs_settings_update();
    }

    switch (new_devs) {
        case DEVS_BT1: {
            if (reset) {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_BT1, (RGB){HS_LBACK_COLOR_BT1}, 200, 1);
            } else {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_BT1, (RGB){HS_PAIR_COLOR_BT1}, 500, 1);
            }
        } break;
        case DEVS_BT2: {
            if (reset) {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_BT2, (RGB){HS_LBACK_COLOR_BT2}, 200, 1);
            } else {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_BT2, (RGB){HS_PAIR_COLOR_BT2}, 500, 1);
            }
        } break;
        case DEVS_BT3: {
            if (reset) {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_BT3, (RGB){HS_LBACK_COLOR_BT3}, 200, 1);
            } else {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_BT3, (RGB){HS_PAIR_COLOR_BT3}, 500, 1);
            }
        } break;
        case DEVS_2G4: {
            if (reset) {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_2G4, (RGB){HS_LBACK_COLOR_2G4}, 200, 1);
            } else {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_2G4, (RGB){HS_LBACK_COLOR_2G4}, 500, 1);
            }
        } break;
        // case DEVS_USB: {
        //     if (reset) {
        //         rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_USB, (RGB){HS_LBACK_COLOR_2G4}, 200, 1);
        //     } else {
        //         rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_USB, (RGB){HS_LBACK_COLOR_2G4}, 500, 1);
        //     }
        // } break;
        default:
            break;
    }
}

bool rgb_matrix_wls_indicator_cb(void) {

    if (*md_getp_state() != MD_STATE_CONNECTED) {
        wireless_devs_change_kb(wireless_get_current_devs(), wireless_get_current_devs(), wls_rgb_indicator_reset);
        return true;
    }

    // refresh led
    led_wakeup();

    return false;
}

void rgb_matrix_wls_indicator(void) {

    if (wls_rgb_indicator_timer) {

        if (timer_elapsed32(wls_rgb_indicator_timer) >= wls_rgb_indicator_interval) {
            wls_rgb_indicator_timer = timer_read32();

            if (wls_rgb_indicator_times) {
                wls_rgb_indicator_times--;
            }

            if (wls_rgb_indicator_times <= 0) {
                wls_rgb_indicator_timer = 0x00;
                if (rgb_matrix_wls_indicator_cb() != true) {
                    return;
                }
            }
        }

        if (wls_rgb_indicator_times % 2) {
            rgb_matrix_set_color(wls_rgb_indicator_index, wls_rgb_indicator_rgb.r, wls_rgb_indicator_rgb.g, wls_rgb_indicator_rgb.b);
        } else {
            rgb_matrix_set_color(wls_rgb_indicator_index, 0x00, 0x00, 0x00);
        }
    }
}

// 连接成功后在 HS_SUCCEED_TIME 内常亮当前通道灯；放在渲染路径中，背光关闭时也能显示
void hs_wls_connected_indicator(void) {
    if (linker_ok || timer_elapsed32(hs_connect_ok_time) >= HS_SUCCEED_TIME) return;

    if (wireless_get_current_devs() == DEVS_USB) {
        if (USB_DRIVER.state != USB_ACTIVE) return;
#ifdef HS_RGB_BLINK_INDEX_USB
        rgb_matrix_set_color(HS_RGB_BLINK_INDEX_USB, HS_LBACK_COLOR_USB);
#endif
        return;
    }

    if (*md_getp_state() != MD_STATE_CONNECTED) return;

    switch (wireless_get_current_devs()) {
        case DEVS_2G4: rgb_matrix_set_color(HS_RGB_BLINK_INDEX_2G4, HS_LBACK_COLOR_2G4); break;
        case DEVS_BT1: rgb_matrix_set_color(HS_RGB_BLINK_INDEX_BT1, HS_LBACK_COLOR_BT1); break;
        case DEVS_BT2: rgb_matrix_set_color(HS_RGB_BLINK_INDEX_BT2, HS_LBACK_COLOR_BT2); break;
        case DEVS_BT3: rgb_matrix_set_color(HS_RGB_BLINK_INDEX_BT3, HS_LBACK_COLOR_BT3); break;
        default: break;
    }
}

// 配对闪烁与连接成功提示都结束后，若背光是临时点亮的则恢复关闭
void hs_wls_indicator_force_task(void) {
    if (!hs_wls_force_rgb || hs_bat_force_rgb) return;

    bool expired = timer_elapsed32(hs_wls_force_start) >= HS_WLS_FORCE_TIMEOUT;
    if (!expired) {
        if (!linker_ok) return;
        // 正常闪烁会在连接提示期内归零；若切换设备时残留计时，最多再等 HS_SUCCEED_TIME 强制结束
        if (wls_rgb_indicator_timer != 0 && timer_elapsed32(hs_wls_force_timer) < HS_SUCCEED_TIME) return;
    }

    hs_wls_force_rgb        = false;
    wls_rgb_indicator_timer = 0; // 配对失败后停止重启闪烁，避免唤醒后重复临时点亮
    hs_indicator_rgb_release();
}

#    endif

#endif

