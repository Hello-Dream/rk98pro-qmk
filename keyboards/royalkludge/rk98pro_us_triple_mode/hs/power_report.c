// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "power.h"

#include "hs/battery.h"
#include "hs/wireless_glue.h"

#include "wireless.h"

void hs_send_bat_status(void) {
    uint8_t hs_now_mode;
    static uint32_t hs_current_time;

    hs_battery_task();

    if (charging_state && (bat_full_flag)) {
        hs_now_mode = MD_SND_CMD_DEVCTRL_CHARGING_DONE;
    } else if (charging_state) {
        hs_now_mode = MD_SND_CMD_DEVCTRL_CHARGING;
    } else {
        hs_now_mode = MD_SND_CMD_DEVCTRL_CHARGING_STOP;
    }

    if (!hs_current_time || timer_elapsed32(hs_current_time) > 1000) {
        hs_current_time = timer_read32();
        md_send_devctrl(hs_now_mode);
        md_send_devctrl(MD_SND_CMD_DEVCTRL_INQVOL);
    }

    // 蓝牙下电量由无线模块直接上报给主机，需先按主机系统设置模块的上报方式，否则系统显示不准：
    // macOS -> 0x80，Windows -> 0x81（同厂商 r75_pro 的做法），首次及系统切换时各下发一次
    static uint8_t hs_bat_report_mode = 0xFF;
    uint8_t        hs_now_bat_mode    = hs_keymap_is_mac() ? 0x80 : 0x81;
    if (hs_bat_report_mode != hs_now_bat_mode) {
        hs_bat_report_mode = hs_now_bat_mode;
        md_send_devctrl_bat(hs_now_bat_mode);
    }
}
