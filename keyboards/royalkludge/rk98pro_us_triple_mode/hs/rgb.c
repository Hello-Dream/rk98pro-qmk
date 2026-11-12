// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rgb.h"

#include "hs/wireless_glue.h"
#include "hs/store.h"
#include "hs/indicators.h"

#include "rgb_record/rgb_record.h"
#include "rgb_record/rgb_rgblight.h"

uint8_t buff[11] = {2, 3, 4, 5, 6, 7, 8, 1, 1, 1, 0};
uint8_t hs_rl_mode = 0;
bool    no_record_fg;

// 应用侧灯模式：索引对应 buff[]，10 为关闭
static void hs_rl_apply_mode_internal(uint8_t mode, bool persist) {
    if (mode >= ARRAY_SIZE(buff)) {
        mode = 0;
    }

    hs_rl_mode = mode;

    /* RGBLIGHT_ENABLE 未启用时，rgblight_* 会被 rgb_matrix.h 映射到主键矩阵；
     * 这里必须隔离，否则恢复侧灯会覆盖用户保存的主键灯效。 */
#ifdef RGBLIGHT_ENABLE
    // 全程用 noeeprom 版本，最后只持久化一次，避免一次切换产生多次 EEPROM 写
    if (mode == 10) {
        HSV current = rgblight_get_hsv();
        rgblight_sethsv_noeeprom(0, 255, current.v);
        rgblight_disable_noeeprom();
    } else {
        rgblight_enable_noeeprom();
        rgblight_mode_noeeprom(buff[mode]);
    }

    HSV rgb = rgblight_get_hsv();
    switch (mode) {
        case 3: rgblight_sethsv_noeeprom(0, 255, rgb.v); break;
        case 4: rgblight_sethsv_noeeprom(40, 255, rgb.v); break;
        case 5: rgblight_sethsv_noeeprom(80, 255, rgb.v); break;
        case 6: rgblight_sethsv_noeeprom(120, 255, rgb.v); break;
        case 7: rgblight_sethsv_noeeprom(160, 255, rgb.v); break;
        case 8: rgblight_sethsv_noeeprom(200, 255, rgb.v); break;
        case 9: rgblight_sethsv_noeeprom(0, 0, rgb.v); break;
        case 0: rgblight_set_speed_noeeprom(255); break;
        default: rgblight_set_speed_noeeprom(200); break;
    }

    if (persist) {
        eeconfig_update_rgblight_current();
    }
#else
    (void)persist;
#endif
}

static void hs_rl_apply_mode(uint8_t mode) {
    hs_rl_apply_mode_internal(mode, true);
}

static void hs_rl_apply_mode_noeeprom(uint8_t mode) {
    hs_rl_apply_mode_internal(mode, false);
}

// 启动时按设置块重放侧灯模式与背光总开关；未启用 rgblight 时侧灯部分为空操作
void hs_settings_apply_boot(void) {
    hs_rl_apply_mode_noeeprom(hs_rl_mode);

    if (!mm_get_rgb_enable()) {
        if (rgb_matrix_is_enabled()) rgb_matrix_disable_noeeprom();
        if (rgblight_is_enabled()) rgblight_disable_noeeprom();
    }

    hs_led_power_update();
}

void im_rgblight_increase(void) {
    if (hs_rl_mode >= ARRAY_SIZE(buff)) {
        hs_rl_mode = 0;
    }

#ifdef RGBLIGHT_ENABLE
    // 从侧灯当前状态反推模式索引；未启用 rgblight 时读到的会是主键矩阵状态，必须隔离。
    uint8_t mode = rgblight_get_mode();

    if (mode == 1) {
        HSV rgb = rgblight_get_hsv();
        if (rgb.h == 0 && rgb.s != 0) hs_rl_mode = 3;
        else hs_rl_mode = 9;
        switch (rgb.h){
            case 40:hs_rl_mode = 4;break;
            case 80:hs_rl_mode = 5;break;
            case 120:hs_rl_mode = 6;break;
            case 160:hs_rl_mode = 7;break;
            case 200:hs_rl_mode = 8;break;
            default :break;
        }
    }
#endif

    hs_rl_mode++;

    if (hs_rl_mode >= ARRAY_SIZE(buff)) hs_rl_mode = 0;

    hs_rl_apply_mode(hs_rl_mode);
}

uint8_t hs_get_side_light_mode(void) {
    return hs_rl_mode;
}

// 设置并保存侧灯模式（VIA 菜单使用）
void hs_set_side_light_mode(uint8_t mode) {
    if (mode >= ARRAY_SIZE(buff)) {
        mode = 0;
    }

    // 同值仍重放硬件，保证唤醒/复位后灯光与设置一致；只跳过持久化。
    bool changed = hs_settings.side_light_mode != mode;
    hs_rl_apply_mode(mode);
    if (changed) {
        hs_settings.side_light_mode = hs_rl_mode;
        hs_settings_update();
    }
}

#ifdef RGB_MATRIX_ENABLE
// 选择自定义灯效前先确保背光已打开
void hs_rgb_select_effect(uint8_t mode) {
    bool was_enabled = mm_get_rgb_enable();

    hs_settings.last_rgb_mode = mode;
    hs_power_rgb_user_enabled();

    // 原本已开时 hs_power_rgb_user_enabled() 不会写，这里补一次统一保存；
    // 原本关闭时 mm_set_rgb_enable(true) 已连同 last_rgb_mode 一起写入。
    if (was_enabled) {
        hs_settings_update();
    }

    // 开关与模式先改 RAM，最后统一保存一次
    rgb_matrix_enable_noeeprom();
    rgb_matrix_mode_noeeprom(mode);
    eeconfig_update_rgb_matrix(&rgb_matrix_config);
#    ifdef LED_POWER_EN_PIN
    gpio_write_pin_high(LED_POWER_EN_PIN);
#    endif
}
#endif

// 记住最近一次实际灯效（包括 VIA 和 EF_* 直达键），退出 RGB 预览时据此恢复。
// 只更新 RAM，返回是否变化；由调用方合并成一次 EEPROM 写入。
bool hs_record_last_mode_update(void) {
    uint8_t mode = rgb_matrix_get_mode();

    if (mode == RGB_MATRIX_NONE || mode == RGB_MATRIX_CUSTOM_RGBR_PLAY || mode >= RGB_MATRIX_EFFECT_MAX) {
        return false;
    }
    if (hs_settings.last_rgb_mode == mode) {
        return false;
    }

    hs_settings.last_rgb_mode = mode;
    return true;
}

// 切换录制通道：录制中先保存当前通道，再读取并预览新通道
void hs_rgbrec_switch_channel(uint8_t channel) {
    if (channel >= RGBREC_CHANNEL_NUM) {
        return;
    }

    if (rgbrec_is_started() && hs_settings.rgbrec_channel == channel) {
        return; // 已在该通道录制，不打断当前录制
    }

    // 未录制且已是当前通道时保留预览，只跳过 EEPROM 持久化。
    if (!rgbrec_is_started() && hs_settings.rgbrec_channel == channel) {
        hs_record_last_mode_update();
        rgbrec_show(channel);
        return;
    }

    hs_record_last_mode_update();
    if (rgbrec_is_started()) {
        rgbrec_end_current(true);
        ind_set_blink(ind_state_none);
    }

    hs_settings.rgbrec_channel = channel;
    rgbrec_show(channel);
    hs_settings_update();
}

// VIA：读取 / 切换当前录制通道
uint8_t hs_get_rgbrec_channel(void) {
    return hs_settings.rgbrec_channel;
}

void hs_set_rgbrec_channel(uint8_t channel) {
    hs_rgbrec_switch_channel(channel);
}

// VIA：开始 / 结束当前通道录制（结束即保存）
void hs_rgbrec_toggle_record(void) {
    if (rgbrec_is_started()) {
        rgbrec_end_current(true);
        ind_set_blink(ind_state_none);
    } else {
        hs_record_last_mode_update();
        if (!rgbrec_start(hs_settings.rgbrec_channel)) {
            return;
        }
        no_record_fg = false;
        rgbrec_set_close_all(HSV_BLACK);
        ind_set_blink(ind_state_blinkon);
    }
    hs_settings_update();
}

// VIA：清空当前录制通道并进入预览
void hs_rgbrec_clear_current(void) {
    uint8_t channel = hs_settings.rgbrec_channel;
    if (channel >= RGBREC_CHANNEL_NUM) {
        channel = 0;
    }

    if (rgbrec_is_started()) {
        rgbrec_end_current(false); // 清空优先，丢弃本次未完成的录制
    }
    hs_record_last_mode_update();
    rgbrec_clear_channel(channel);
    hs_settings.rgbrec_channel = channel;
    rgbrec_show(channel);
    hs_settings_update();
    ind_set_blink(ind_state_blink3);
}

// VIA：复制当前通道到下一通道并进入预览
void hs_rgbrec_copy_next(void) {
    uint8_t from = hs_settings.rgbrec_channel;
    if (from >= RGBREC_CHANNEL_NUM) {
        from = 0;
    }
    uint8_t to = (from + 1) % RGBREC_CHANNEL_NUM;

    if (rgbrec_is_started()) {
        rgbrec_end_current(true); // 先保存当前录制，再复制
    }
    hs_record_last_mode_update();
    rgbrec_copy_channel(from, to);
    hs_settings.rgbrec_channel = to;
    rgbrec_show(to);
    hs_settings_update();
    ind_set_blink(ind_state_blink3);
}

