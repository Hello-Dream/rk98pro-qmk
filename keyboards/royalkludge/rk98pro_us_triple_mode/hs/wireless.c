// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "hs/wireless_glue.h"
#include "hs/store.h"

static uint32_t post_init_timer = 0x00;
static bool     state           = false;

/* 上电后延迟 100ms 再下发无线模块的初始化命令。 */
void hs_wireless_post_init_begin(void) {
    post_init_timer = timer_read32();
}

bool hs_wireless_state_get(void) {
    return state;
}

void hs_wireless_state_set(bool value) {
    state = value;
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
