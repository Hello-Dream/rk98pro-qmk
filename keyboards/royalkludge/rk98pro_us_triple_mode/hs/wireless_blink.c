// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "hs/wireless_glue.h"
#include "hs/wireless_internal.h"

#include "usb_main.h"
#include "lowpower.h"

#include "rgb_record/rgb_record.h"

static uint32_t hs_linker_rgb_timer = 0x00;

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
