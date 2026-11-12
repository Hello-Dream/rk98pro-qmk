// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "hs/wireless_glue.h"
#include "hs/indicators.h"
#include "hs/store.h"

#include "usb_main.h"

#define HS_WLS_FORCE_TIMEOUT (HS_PAIR_TIMEOUT + HS_SUCCEED_TIME + 1000)

static uint32_t hs_connect_ok_time = 0;

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
