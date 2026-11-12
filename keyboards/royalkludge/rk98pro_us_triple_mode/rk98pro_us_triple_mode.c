// Copyright 2024 SDK (@sdk66)
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "hs/wireless_glue.h"
#include "hs/settings.h"
#include "hs/board_init.h"
#include "hs/key_handlers.h"
#include "hs/runtime.h"
#include "hs/session.h"

#ifdef VIA_ENABLE
#    include "via.h"
#endif

_Static_assert(HS_SETTINGS_VERSION <= 0xFF, "HS_SETTINGS_VERSION must fit in 8 bits");
_Static_assert(EECONFIG_USER_DATA_SIZE <= 0xFFFF, "EECONFIG_USER_DATA_SIZE must fit in 16 bits");

#ifdef VIA_ENABLE
// 自定义设置版本变化时让 VIA 的缓存一并失效，避免同一构建日期下残留旧层数据
void via_init_kb(void) {
    if (!eeconfig_is_user_datablock_valid()) {
        via_eeprom_set_valid(false);
    }
}
#endif

void keyboard_post_init_kb(void) {
    hs_board_init();
    keyboard_post_init_user();
    hs_board_init_post();
}

#ifdef WIRELESS_ENABLE

void usb_power_connect(void) {
    hs_session_usb_connect();
}

void usb_power_disconnect(void) {
    hs_session_usb_disconnect();
}

void suspend_power_down_kb(void) { // usb power down
    hs_session_suspend();
}

void suspend_wakeup_init_kb(void) { // usb wakeup
    hs_session_wakeup();
}

bool lpwr_is_allow_timeout_hook(void) {
    return hs_session_allow_timeout();
}

#endif

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (process_record_user(keycode, record) != true) {
        return false;
    }

#ifdef WIRELESS_ENABLE
    if (process_record_wls(keycode, record) != true) {
        return false;
    }
#endif

    return hs_key_handlers_process(keycode, record);
}

void housekeeping_task_user(void) { // loop
    hs_runtime_task();
}

bool rgb_matrix_indicators_advanced_kb(uint8_t led_min, uint8_t led_max) {
    return hs_runtime_indicators(led_min, led_max);
}
