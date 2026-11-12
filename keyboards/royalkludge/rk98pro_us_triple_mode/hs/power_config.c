// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "power.h"

#include "power_internal.h"

#include "hs/store.h"
#include "hs/wireless_glue.h"

#include "lowpower.h"

static void eeconfig_power_update(void) {
    hs_store_settings_touch();
}

void eeconfig_power_default(void) {
    hs_power.idle_dim            = 2; // 30 秒
    hs_power.idle_off            = 2; // 2 分钟
    hs_power.low_bat_level      = 3; // 15%
    hs_power.low_bat_brightness = 1; // 30%
    hs_power.usb_save           = 1; // USB 默认也省电
    hs_power.status_led         = 1;
    eeconfig_power_update();
}

void eeconfig_power_init(void) {
    // hs_store_init() 已加载完整设置块，这里只做范围钳制。
    if (hs_power.idle_dim > 4) hs_power.idle_dim = 2;
    if (hs_power.idle_off > 4) hs_power.idle_off = 2;
    if (hs_power.low_bat_level > 4) hs_power.low_bat_level = 3;
    if (hs_power.low_bat_brightness > 3) hs_power.low_bat_brightness = 1;
}

uint8_t hs_power_get_config(uint8_t value_id) {
    switch (value_id) {
        case id_power_idle_dim: return hs_power.idle_dim <= 4 ? hs_power.idle_dim : 2;
        case id_power_idle_off: return hs_power.idle_off <= 4 ? hs_power.idle_off : 2;
        case id_power_usb_save: return hs_power.usb_save ? 1 : 0;
        case id_power_low_bat_warn: return hs_power.low_bat_level;
        case id_power_low_bat_brightness: return hs_power.low_bat_brightness;
        case id_power_status_led: return hs_power.status_led ? 1 : 0;
        default: return 0;
    }
}

void hs_power_set_config(uint8_t value_id, uint8_t value) {
    switch (value_id) {
        case id_power_idle_dim: {
            uint8_t next = value <= 4 ? value : 2;
            if (hs_power.idle_dim == next) return;
            hs_power.idle_dim = next;
        } break;
        case id_power_idle_off: {
            uint8_t next = value <= 4 ? value : 2;
            if (hs_power.idle_off == next) return;
            hs_power.idle_off = next;
        } break;
        case id_power_usb_save: {
            uint8_t next = value ? 1 : 0;
            if (hs_power.usb_save == next) return;
            hs_power.usb_save = next;
        } break;
        case id_power_low_bat_warn: {
            uint8_t next = value <= 4 ? value : 3;
            if (hs_power.low_bat_level == next) return;
            hs_power.low_bat_level = next;
        } break;
        case id_power_low_bat_brightness: {
            uint8_t next = value & 0x03;
            if (hs_power.low_bat_brightness == next) return;
            hs_power.low_bat_brightness = next;
        } break;
        case id_power_status_led: {
            uint8_t next = value ? 1 : 0;
            if (hs_power.status_led == next) return;
            hs_power.status_led = next;
        } break;
        default:
            return;
    }

    eeconfig_power_update();
    hs_power_apply_settings();
    if (value_id == id_power_low_bat_brightness) {
        hs_power_low_bat_refresh_cap();
    }
}

uint8_t hs_get_sleep_timeout(void){
    return hs_settings.sleep_timeout;
}

uint32_t hs_sleep_timeout_ms(void) {
    switch (hs_get_sleep_timeout()) {
        case hs_sleep_timeout_1min:   return 1 * 60000;
        case hs_sleep_timeout_3min:   return 3 * 60000;
        case hs_sleep_timeout_5min:   return 5 * 60000;
        case hs_sleep_timeout_10min:  return 10 * 60000;
        case hs_sleep_timeout_20min:  return 20 * 60000;
        case hs_sleep_timeout_30min:  return 30 * 60000;
        case hs_sleep_timeout_vendor: return HS_SLEEP_TIMEOUT;
        default:                      return 0; // hs_sleep_timeout_none
    }
}

void hs_set_sleep_timeout(uint8_t sleep_timeout){
    if (hs_settings.sleep_timeout == sleep_timeout) {
        return; // 值未变不标记 dirty，避免 VIA 重复写入触发 EEPROM
    }
    hs_settings.sleep_timeout = sleep_timeout;
    lpwr_set_timeout_value(hs_sleep_timeout_ms()); // 让 VIA 设置真正生效（原来从未下发，恒为 5 分钟）
    hs_settings_update();
}
