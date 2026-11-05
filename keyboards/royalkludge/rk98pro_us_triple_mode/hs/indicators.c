// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "indicators.h"

#include "hs/wireless_glue.h"
#include "hs/store.h"
#include "hs/rgb.h"

#include "rgb_record/rgb_record.h"
#include "rgb_matrix_blink.h"
#include "lowpower.h"

extern bool lower_sleep;
extern bool rk_bat_req_flag;
extern uint8_t hs_power_low_bat_threshold(void);

ind_status_t ind_status      = ind_state_none;
uint32_t    hs_bat_show_timer = 0;
bool hs_bat_force_rgb     = false; // 背光原本关闭时，为显示电量条临时点亮的标记
bool hs_wls_force_rgb     = false; // 背光原本关闭时，为显示无线连接指示临时点亮的标记
bool hs_status_force_rgb  = false; // 侧灯状态覆盖层临时点亮的标记
bool hs_power_rgb_off     = false; // 空闲关灯期间强制关闭 RGB / LED 供电
bool hs_indicator_rgb_acquired = false; // 状态灯是否临时启用过 RGB 矩阵
bool hs_indicator_rgb_was_enabled = false; // 临时启用前的矩阵开关状态
uint32_t hs_wls_force_timer = 0;   // 最近一次通道指示动画时刻（用于清理残留闪烁计时）
uint32_t hs_wls_force_start = 0;   // 本轮临时背光的起始时刻（不随闪烁重启）
hs_status_t hs_status = HS_STATUS_NONE;
uint32_t hs_status_start = 0;
uint32_t hs_status_duration = 0;
RGB hs_status_color = {0};

blink_rgb_t blink_rgbs[RGB_MATRIX_BLINK_COUNT] = {
    {.index = HS_RGB_BLINK_INDEX_WIN, .interval = 250, .times = 1, .color = {.r = 0xFF, .g = 0xFF, .b = 0xFF}, .blink_cb = NULL},
    {.index = HS_RGB_BLINK_INDEX_MAC, .interval = 250, .times = 1, .color = {.r = 0xFF, .g = 0xFF, .b = 0xFF}, .blink_cb = NULL},
    {.index = IM_MM_RGB_BLINK_INDEX_DEB, .interval = 250, .times = 3, .color = {.r = WIITE_B, .g = WIITE_B, .b = WIITE_B}, .blink_cb = NULL},
    {.index = RGB_MATRIX_BLINK_INDEX_BAT, .interval = 250, .times = 3, .color = {.r = 0x00, .g = 0x00, .b = 0x00}, .blink_cb = NULL},
    {.index = RGB_MATRIX_BLINK_INDEX_IND, .interval = 250, .times = 3, .color = {.r = 0x00, .g = 0x00, .b = 0x00}, .blink_cb = ind_indicators_hook},
    {.index = RGB_MATRIX_BLINK_INDEX_ALL, .interval = 250, .times = 3, .color = {.r = WIITE_B, .g = 0x00, .b = 0x00}, .blink_cb = nkr_indicators_hook},
};




void nkr_indicators_hook(uint8_t index) {
    if (index == RGB_MATRIX_BLINK_INDEX_ALL) {
        if ((blink_rgbs[RGB_MATRIX_BLINK_COUNT - 1].color.r == WIITE_B) && (blink_rgbs[RGB_MATRIX_BLINK_COUNT - 1].color.g == 0x00) && (blink_rgbs[RGB_MATRIX_BLINK_COUNT - 1].color.b == 0x00)) {
            if (rgb_matrix_blink_set_color(index, 0x00, 0x00, WIITE_B) != true) {

            }
            rgb_matrix_blink_set_times(index, 1);
            rgb_matrix_blink_set(index);
        } else if ((blink_rgbs[RGB_MATRIX_BLINK_COUNT - 1].color.r == 0x00) && (blink_rgbs[RGB_MATRIX_BLINK_COUNT - 1].color.g == 0x00) && (blink_rgbs[RGB_MATRIX_BLINK_COUNT - 1].color.b == WIITE_B)) {
            if (rgb_matrix_blink_set_color(index, 0x00, WIITE_B, 0x00) != true) {

            }
            rgb_matrix_blink_set_times(index, 1);
            rgb_matrix_blink_set(index);
        }
    }
}

void hs_bat_indicators_task(void) {
    static uint32_t battery_process_time = 0;
    uint8_t         battery              = hs_battery_percent();

    if (charging_state && bat_full_flag) { // 充满
        battery_process_time = 0;
        gpio_write_pin(LED_POV_PIN, 1);
    } else if (charging_state) { // 充电中
        battery_process_time = 0;
        gpio_write_pin(LED_POV_PIN, 0);
    } else if (hs_power.low_bat_level > 0 && battery != 0xFF && battery <= hs_power_low_bat_threshold()) { // 低电：侧灯提示，Win 灯只保留 Win 锁定
        gpio_write_pin(LED_POV_PIN, 1);
        if (hs_battery_low_confirmed()) { // 低电保护：切 USB 并进入深睡，防止过放
            if (!battery_process_time) {
                battery_process_time = timer_read32();
                // 进入保护倒计时时用数字排电量条明确提示当前电量
                hs_bat_show();
            }
            if (battery_process_time && timer_elapsed32(battery_process_time) > 20000) {
                battery_process_time = 0;
                clear_keyboard();
                lower_sleep = true;
                gpio_write_pin(LED_MAC_PIN, 0);
                gpio_write_pin(LED_POV_PIN, 1);
                md_send_devctrl(MD_SND_CMD_DEVCTRL_USB);
                lpwr_set_timeout_manual(true);
            }
        }
    } else { // 正常
        battery_process_time = 0;
        gpio_write_pin(LED_POV_PIN, 1);
    }
}

void ind_indicators_hook(uint8_t index) {

    switch(ind_status) {
        case ind_state_none: {
            rgb_matrix_blink_set_color(index, 0x00, 0x00, 0x00);
            rgb_matrix_blink_set_interval_times(index, 50, 0xFF);
        } break;
        case ind_state_blink3: {
            rgb_matrix_blink_set_color(index, 0xFF, 0xFF, 0xFF);
            rgb_matrix_blink_set_interval_times(index, 250, 3);
            ind_status = ind_state_exec;
        } break;
        case ind_state_blinkon: {
            RGB rgb;
            HSV hsv = {.h = 0, .s = 0};
            hsv.v = RGB_MATRIX_MAXIMUM_BRIGHTNESS;
            rgb = hsv_to_rgb(hsv);
            rgb_matrix_blink_set_color(index, rgb.r , rgb.g, rgb.b);
            rgb_matrix_blink_set_interval_times(index, 250, 1);
        } break;
        case ind_state_blinkoff: {
            ind_status = ind_state_none;
            ind_indicators_hook(index);
        } break;
        case ind_state_exec: {
            ind_status = ind_state_none;
            ind_indicators_hook(index);
        } break;
        default: break;
    }

    rgb_matrix_blink_set(index);
}

void ind_set_blink(ind_status_t status) {
    ind_status = status;
    rgb_matrix_blink_set_remain_time(RGB_MATRIX_BLINK_INDEX_IND, 0x00);
    ind_indicators_hook(RGB_MATRIX_BLINK_INDEX_IND);
}

bool rgb_matrix_blink_user(blink_rgb_t *blink_rgb) {

    switch (blink_rgb->index) {
        case RGB_MATRIX_BLINK_INDEX_BAT: {
            if (ind_status != ind_state_none) {
                return false;
            }

            if (rgbrec_is_started()) {
                return false;
            }

            // if (blink_rgb->flip) {
            //     rgb_matrix_set_color(2, blink_rgb->color.r, blink_rgb->color.g, blink_rgb->color.b);
            // } else {
            //     rgb_matrix_set_color(2,0,0,0);
            // }

            return false;
        } break;
        case RGB_MATRIX_BLINK_INDEX_IND: {

            switch(ind_status) {
                case ind_state_none: {
                    return false;
                } break;
                case ind_state_blinkon:
                case ind_state_blinkoff:
                case ind_state_exec: {
                    // if (rgbrec_is_started()) {
                    //     return false;
                    // }

                    if (blink_rgb->flip) {
                        gpio_write_pin(LED_WIN_PIN, 1);
                        gpio_write_pin(LED_CAPS_PIN, 1);
                        gpio_write_pin(LED_SCROLL_PIN, 1);
                        gpio_write_pin(LED_NUM_PIN, 1);
                    } else {
                        gpio_write_pin(LED_WIN_PIN, 0);
                        gpio_write_pin(LED_CAPS_PIN, 0);
                        gpio_write_pin(LED_SCROLL_PIN, 0);
                        gpio_write_pin(LED_NUM_PIN, 0);
                    }
                } break;
                default: break;
            }
            return false;
        } break;
#undef HS_RGB_BLINK_INDEX_REC
#ifdef HS_RGB_BLINK_INDEX_REC
        case HS_RGB_BLINK_INDEX_REC: {
        } break;
#endif
        default: {

            if (rgbrec_is_started()) {
                return false;
            }
        } break;
    }

    return true;
}

void hs_led_power_update(void) {
#ifdef LED_POWER_EN_PIN
    if ((mm_get_rgb_enable() && !hs_power_rgb_off) || hs_bat_force_rgb || hs_wls_force_rgb || hs_status_force_rgb) {
        gpio_write_pin_high(LED_POWER_EN_PIN);
    } else {
        gpio_write_pin_low(LED_POWER_EN_PIN);
    }
#endif
}

void hs_power_rgb_user_enabled(void) {
    hs_indicator_rgb_acquired = false;
    if (!mm_get_rgb_enable()) {
        mm_set_rgb_enable(true);
        hs_led_power_update();
    }
}

void hs_indicator_rgb_acquire(void) {
    if (mm_get_rgb_enable() && !hs_power_rgb_off && rgb_matrix_is_enabled()) return;

    if (!hs_indicator_rgb_acquired) {
        hs_indicator_rgb_was_enabled = rgb_matrix_is_enabled();
        hs_indicator_rgb_acquired    = true;
    }
    if (!rgb_matrix_is_enabled()) rgb_matrix_enable_noeeprom();
#ifdef LED_POWER_EN_PIN
    gpio_write_pin_high(LED_POWER_EN_PIN);
#endif
}

void hs_indicator_rgb_release(void) {
    if (hs_bat_force_rgb || hs_wls_force_rgb || hs_status_force_rgb) return;
    if (!hs_indicator_rgb_acquired) return;

    bool master_on = mm_get_rgb_enable() && !hs_power_rgb_off;

    // 用户原本就开着矩阵时保持现状；只撤销状态灯临时打开的矩阵
    if (!(master_on && hs_indicator_rgb_was_enabled)) {
        rgb_matrix_disable_noeeprom();
    }
    if (!master_on) {
#ifdef LED_POWER_EN_PIN
        gpio_write_pin_low(LED_POWER_EN_PIN);
#endif
    }
    hs_indicator_rgb_acquired = false;
}

void hs_wls_indicator_force_begin(void) {
    if (mm_get_rgb_enable() && !hs_power_rgb_off && rgb_matrix_is_enabled()) return;
    if (!hs_wls_force_rgb) {
        hs_wls_force_rgb   = true;
        hs_wls_force_start = timer_read32();
    }
    hs_indicator_rgb_acquire();
}

void hs_status_show(hs_status_t type, uint32_t duration, RGB color) {
    if (!hs_power.status_led) return;

    hs_status          = type;
    hs_status_start    = timer_read32();
    hs_status_duration = duration;
    hs_status_color    = color;

    if (!mm_get_rgb_enable() || hs_power_rgb_off || !rgb_matrix_is_enabled()) {
        hs_status_force_rgb = true;
        hs_indicator_rgb_acquire();
    }
}

void hs_status_end(void) {
    if (hs_status == HS_STATUS_NONE && !hs_status_force_rgb) return;

    hs_status          = HS_STATUS_NONE;
    hs_status_duration = 0;
    if (hs_status_force_rgb) {
        hs_status_force_rgb = false;
        hs_indicator_rgb_release();
    }
}

void hs_status_render(uint8_t led_min, uint8_t led_max) {
    if (hs_status == HS_STATUS_NONE || !hs_power.status_led) return;

    uint32_t elapsed = timer_elapsed32(hs_status_start);
    RGB      color   = hs_status_color;

    for (uint8_t i = 0; i < RGBLED_NUM; i++) {
        uint8_t index = RGB_MATRIX_LED_COUNT - RGBLED_NUM + i;
        if (index < led_min || index >= led_max) continue;

        bool on = false;
        switch (hs_status) {
            case HS_STATUS_CHARGING: {
                uint8_t pos = (uint8_t)((elapsed / 300) % RGBLED_NUM);
                color = (i == pos) ? (RGB){0, 150, 0} : (RGB){0, 45, 0};
                on = true;
            } break;
            case HS_STATUS_FULL: {
                color = (RGB){0, 150, 0};
                on    = true;
            } break;
            case HS_STATUS_LOW_BAT: {
                color = (RGB){150, 0, 0};
                on    = ((elapsed / 300) % 2) == 0;
            } break;
            case HS_STATUS_CONNECT: {
                color = hs_status_color;
                on    = true;
            } break;
            default: break;
        }

        if (on) {
            rgb_matrix_set_color(index, color.r, color.g, color.b);
        } else {
            rgb_matrix_set_color(index, 0, 0, 0);
        }
    }
}

bool hs_indicators_power_blocked(void) {
    if (rgbrec_is_started()) return true;
    if (rgb_matrix_get_mode() == RGB_MATRIX_CUSTOM_RGBR_PLAY) return true;
    if (ind_status != ind_state_none) return true;
    if (hs_bat_force_rgb || hs_wls_force_rgb || hs_status_force_rgb || hs_status != HS_STATUS_NONE) return true;
    if (*md_getp_state() == MD_STATE_PAIRING) return true;
    return false;
}

void hs_bat_show(void) {
    extern bool rk_bat_req_flag;
    hs_power_activity();
    rk_bat_req_flag   = true;
    hs_bat_show_timer = timer_read32();
    // 背光被关闭、矩阵被 VIA 关闭或处于空闲关灯时，临时打开 LED 供电与背光，保证电量条仍能显示，3 秒后再恢复
    if (!mm_get_rgb_enable() || hs_power_rgb_off || !rgb_matrix_is_enabled()) {
        hs_bat_force_rgb = true;
        hs_indicator_rgb_acquire();
    }
}

void hs_update_indicators(void) {
    if (rgbrec_is_started() || ind_status != ind_state_none) return;

    bool caps_on = host_keyboard_led_state().caps_lock;
#ifdef CAPS_WORD_ENABLE
    caps_on = caps_on || is_caps_word_on();
#endif
    if (caps_on) {
        gpio_write_pin(LED_CAPS_PIN, LED_PIN_ON_STATE);
    } else {
        gpio_write_pin(LED_CAPS_PIN, !LED_PIN_ON_STATE);
    }

    if (host_keyboard_led_state().num_lock) {
        gpio_write_pin(LED_NUM_PIN, LED_PIN_ON_STATE);
    } else {
        gpio_write_pin(LED_NUM_PIN, !LED_PIN_ON_STATE);
    }

    if (host_keyboard_led_state().scroll_lock) {
        gpio_write_pin(LED_SCROLL_PIN, LED_PIN_ON_STATE);
    } else {
        gpio_write_pin(LED_SCROLL_PIN, !LED_PIN_ON_STATE);
    }

    if (!hs_keymap_is_mac() && keymap_config.no_gui) {
        gpio_write_pin(LED_WIN_PIN, LED_PIN_ON_STATE);
    } else {
        gpio_write_pin(LED_WIN_PIN, !LED_PIN_ON_STATE);
    }
}
