// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "settings.h"

#include "hs/wireless_glue.h"
#include "hs/store.h"
#include "hs/layers.h"
#include "hs/power.h"
#include "hs/rgb.h"

#include "rgb_record/rgb_record.h"
#include "rgb_record/rgb_rgblight.h"

void hs_settings_update(void) {
    hs_store_settings_touch();
}

bool mm_get_rgb_enable(void) {
    return hs_settings.rgb_enabled;
}

bool hs_keymap_is_mac(void) {
    return (get_highest_layer(default_layer_state) == _MBL) || (get_highest_layer(default_layer_state) == _MFL);
}

// 此函数不需要改动
void mm_set_rgb_enable(bool state) {
    if ((hs_settings.rgb_enabled != 0) == state) {
        return; // 同值不重复标记 dirty
    }
    hs_settings.rgb_enabled = state;
    hs_settings_update();
}

void hs_settings_reset(void) {

    hs_store_reset();
    hs_settings.wireless_mode = wireless_get_current_devs();
    hs_store_settings_flush();

#ifdef RGB_MATRIX_ENABLE
    eeconfig_update_rgb_matrix_default(); // 核心灯效配置一并复位，避免旧灯效编号错位
#endif

#if defined(NKRO_ENABLE) && defined(FORCE_NKRO)
    keymap_config.nkro = 1;
    eeconfig_update_keymap(&keymap_config);
#endif

#ifdef RGBLIGHT_ENABLE
    rgblight_mode(buff[0]);
#endif

    eeconfig_power_default();
}

void hs_settings_init(void) {
    hs_store_init();
    if (hs_store_reset_occurred()) {
        hs_settings.wireless_mode = wireless_get_current_devs();
        hs_store_settings_flush();
    }
    eeconfig_power_init();
    hs_user_datablock_reset = false;
}
