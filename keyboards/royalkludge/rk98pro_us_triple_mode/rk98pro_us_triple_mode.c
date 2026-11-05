// Copyright 2024 SDK (@sdk66)
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "hs/wireless_glue.h"
#include "hs/store.h"
#include "hs/status.h"
#include "hs/keys.h"
#include "hs/rgb.h"
#include "hs/indicators.h"
#include "hs/power.h"
#include "hs/layers.h"
#include "rgb_record/rgb_record.h"
#include "rgb_record/rgb_rgblight.h"

#include <string.h>

#ifdef VIA_ENABLE
#    include "via.h"
#endif

#ifdef WIRELESS_ENABLE
#    include "wireless.h"
#    include "usb_main.h"
#    include "lowpower.h"
#    include "rgb_matrix_blink.h"
#endif

_Static_assert(HS_SETTINGS_VERSION <= 0xFF, "HS_SETTINGS_VERSION must fit in 8 bits");
_Static_assert(EECONFIG_USER_DATA_SIZE <= 0xFFFF, "EECONFIG_USER_DATA_SIZE must fit in 16 bits");

#define keymap_is_mac_system() ((get_highest_layer(default_layer_state) == _MBL) || (get_highest_layer(default_layer_state) == _MFL))
#define keymap_is_base_layer() ((get_highest_layer(default_layer_state) == _BL) || (get_highest_layer(default_layer_state) == _FL))

static uint32_t ee_clr_timer = 0;
bool           lower_sleep    = false;
HSV            start_hsv;
bool           no_record_fg;


void hs_settings_update(void) {
    hs_store_settings_touch();
}

// 把 VIA 的睡眠超时档位换算为毫秒，0 表示不自动休眠

bool mm_get_rgb_enable(void) {
    return hs_settings.rgb_enabled;
}

bool hs_keymap_is_mac(void) {
    return keymap_is_mac_system();
}

// 此函数不需要改动
void mm_set_rgb_enable(bool state) {
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

#ifdef VIA_ENABLE
// 自定义设置版本变化时让 VIA 的缓存一并失效，避免同一构建日期下残留旧层数据
void via_init_kb(void) {
    if (!eeconfig_is_user_datablock_valid()) {
        via_eeprom_set_valid(false);
    }
}
#endif

void keyboard_post_init_kb(void) {

#ifdef CONSOLE_ENABLE
    debug_enable = true;
#endif

    hs_settings_init();
    hs_power_init();
    lpwr_set_timeout_value(hs_sleep_timeout_ms()); // 应用 VIA 的睡眠超时设置

#ifdef LED_POWER_EN_PIN
    gpio_set_pin_output(LED_POWER_EN_PIN);
#endif
    hs_led_power_update(); // 按用户保存的背光开关决定是否供电

#ifdef LED_NUM_PIN
    gpio_set_pin_output(LED_NUM_PIN);
    gpio_write_pin_high(LED_NUM_PIN); // 亮=低电平，先置高=灭
#endif

#ifdef LED_CAPS_PIN
    gpio_set_pin_output(LED_CAPS_PIN);
    gpio_write_pin_high(LED_CAPS_PIN);
#endif

#ifdef LED_SCROLL_PIN
    gpio_set_pin_output(LED_SCROLL_PIN);
    gpio_write_pin_high(LED_SCROLL_PIN);
#endif

#ifdef LED_WIN_PIN
    gpio_set_pin_output(LED_WIN_PIN);
    gpio_write_pin_high(LED_WIN_PIN);
#endif

#ifdef LED_POV_PIN
    gpio_set_pin_output(LED_POV_PIN);
    gpio_write_pin_high(LED_POV_PIN);
#endif

#ifdef LED_MAC_PIN
    gpio_set_pin_output(LED_MAC_PIN);
    gpio_write_pin_high(LED_MAC_PIN);
#endif

#ifdef MM_BT_DEF_PIN
    gpio_set_pin_input_high(MM_BT_DEF_PIN);
#endif

#ifdef MM_2G4_DEF_PIN
    gpio_set_pin_input_high(MM_2G4_DEF_PIN);
#endif

#ifdef USB_POWER_EN_PIN
    gpio_write_pin_low(USB_POWER_EN_PIN);
    gpio_set_pin_output(USB_POWER_EN_PIN);
#endif

#ifdef HS_BAT_CABLE_PIN
    gpio_set_pin_input(HS_BAT_CABLE_PIN);
#endif

#ifdef BAT_FULL_PIN
    gpio_set_pin_input_high(BAT_FULL_PIN);
#endif

#ifdef RGB_MATRIX_BLINK_INDEX_BAT
    rgb_matrix_blink_set_interval_times(RGB_MATRIX_BLINK_INDEX_BAT, 200, 0xFF);
    rgb_matrix_blink_set(RGB_MATRIX_BLINK_INDEX_BAT);
#endif

#ifdef RGB_MATRIX_BLINK_INDEX_IND
    rgb_matrix_blink_set_interval_times(RGB_MATRIX_BLINK_INDEX_IND, 200, 0xFF);
    rgb_matrix_blink_set(RGB_MATRIX_BLINK_INDEX_IND);
#endif

#ifdef HS_RGB_BLINK_INDEX_REC
    rgb_matrix_blink_set_interval_times(HS_RGB_BLINK_INDEX_REC, 200, 0xFF);
    rgb_matrix_blink_set(HS_RGB_BLINK_INDEX_REC);
#endif

#ifdef WIRELESS_ENABLE
    wireless_init();
#if !(defined(MM_BT_DEF_PIN) && defined(MM_2G4_DEF_PIN))
    wireless_devs_change(!DEVS_USB, hs_settings.wireless_mode, false);
#endif
    post_init_timer = timer_read32();
#endif

    keyboard_post_init_user();

    ind_status = ind_state_none;
    if (hs_settings.rgbrec_channel >= RGBREC_CHANNEL_NUM) {
        hs_settings.rgbrec_channel = 0;
        hs_settings_update();
    }
    rgbrec_init(hs_settings.rgbrec_channel);
    hs_rl_mode = hs_settings.side_light_mode;
    if (hs_rl_mode >= ARRAY_SIZE(buff)) {
        hs_rl_mode              = 0;
        hs_settings.side_light_mode = 0;
        hs_settings_update();
    }
    hs_settings_apply_boot();
    start_hsv = rgb_matrix_get_hsv();

    hs_keys_init();

    // 若 EEPROM 里存的动效号越界，回落到默认动效
    if (rgb_matrix_config.mode >= RGB_MATRIX_EFFECT_MAX) {
        rgb_matrix_config.mode = RGB_MATRIX_DEFAULT_MODE;
    }
}

#ifdef WIRELESS_ENABLE

void usb_power_connect(void) {

    hs_power_activity();
#ifdef USB_POWER_EN_PIN
    gpio_write_pin_low(USB_POWER_EN_PIN);
#endif
}

void usb_power_disconnect(void) {

    hs_power_activity();
#ifdef USB_POWER_EN_PIN
    gpio_write_pin_high(USB_POWER_EN_PIN);
#endif
}

void suspend_power_down_kb(void) { // usb power down

#ifdef LED_POWER_EN_PIN
    gpio_write_pin_low(LED_POWER_EN_PIN);
#endif

#ifdef LED_CAPS_PIN
    gpio_write_pin_high(LED_CAPS_PIN);
#endif

#ifdef LED_NUM_PIN
    gpio_write_pin_high(LED_NUM_PIN);
#endif

#ifdef LED_SCROLL_PIN
    gpio_write_pin_high(LED_SCROLL_PIN);
#endif

#ifdef LED_POV_PIN
    gpio_write_pin_high(LED_POV_PIN);
#endif

#ifdef LED_MAC_PIN
    gpio_write_pin_low(LED_MAC_PIN);
#endif

#ifdef LED_WIN_PIN
    gpio_write_pin_high(LED_WIN_PIN);
#endif

    suspend_power_down_user();
}

void suspend_wakeup_init_kb(void) { // usb wakeup

    hs_led_power_update();

#ifdef LED_MAC_PIN
    gpio_write_pin_high(LED_MAC_PIN);
#endif

    wireless_devs_change(wireless_get_current_devs(), wireless_get_current_devs(), false);
    hs_indicator_rgb_release(); // 背光关闭且没有临时指示时，清掉唤醒前残留的 RGB 使能
    suspend_wakeup_init_user();
    hs_rgb_blink_set_timer(timer_read32());
}

bool lpwr_is_allow_timeout_hook(void) {
    // 低电时不要进入普通休眠：让主循环继续运行，由 hs_bat_indicators_task()
    // 完成 20 秒保护提示后再进入只允许充电唤醒的保护深睡，
    // 避免普通休眠中电量耗尽到 0%，触发电池保护板锁死。
    if (!lower_sleep && !charging_state && hs_battery_low_confirmed()) {
        return false;
    }

    return true;
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

    switch (keycode) {
        case QK_BOOT: {
            if (record->event.pressed) {
                // 软复位只跳转 bootloader，不清 EEPROM；需要恢复默认请用 Bootmagic / EE_CLR / VIA
                dprintf("into boot!!!\r\n");
                bootloader_jump();
            }
        } break;

        case NK_TOGG: {
            if (record->event.pressed) {
                rgb_matrix_blink_set_color(RGB_MATRIX_BLINK_INDEX_ALL, WIITE_B, 0x00, 0x00);
                rgb_matrix_blink_set_times(RGB_MATRIX_BLINK_INDEX_ALL, 1);
                rgb_matrix_blink_set(RGB_MATRIX_BLINK_INDEX_ALL);
            }
        } break;
        case RL_MOD: {
            if (record->event.pressed) {
                im_rgblight_increase();
                hs_settings.side_light_mode = hs_rl_mode;
                hs_settings_update();
            }

            return false;
        } break;
        case EE_CLR: {
            if (record->event.pressed) {
                ee_clr_timer = timer_read32();
            } else {
                ee_clr_timer = 0;
            }

            return false;
        } break;
        case UG_TOGG: {
            if (record->event.pressed) {
                // 空闲关灯时：逻辑背光原为开则第一次按键只负责唤醒；原为关则恢复后正常打开
                if (hs_power_rgb_off) {
                    bool wake_only = mm_get_rgb_enable();
                    hs_power_activity();
                    if (wake_only) {
                        return false;
                    }
                }
                hs_power_activity();
                mm_set_rgb_enable(!mm_get_rgb_enable());
                if (mm_get_rgb_enable()) {
                    // enable
                    hs_power_rgb_user_enabled();
                    rgb_matrix_enable();
#        ifdef LED_POWER_EN_PIN
                    gpio_write_pin_high(LED_POWER_EN_PIN);
#        endif
                } else {
                    rgb_matrix_disable();
#        ifdef LED_POWER_EN_PIN
                    gpio_write_pin_low(LED_POWER_EN_PIN);
#        endif
                }
            }
            return false;
        } break;
        case UG_SPDU: { // Fn+右方向键：提速（原厂参数：SPD_STEP*5=满速时提示；提速由 QMK 在松开时执行）
            if (rgbrec_is_started()) return false;
            if (record->event.pressed) {
                if (rgb_matrix_get_speed() >= (RGB_MATRIX_SPD_STEP * 5)) {
                    ind_set_blink(ind_state_blink3);
                }
            }
        } break;
        case UG_SPDD: { // Fn+左方向键：减速（原厂参数；修复松开时越界再减一档的问题）
            if (rgbrec_is_started()) return false;
            if (record->event.pressed) {
                if (rgb_matrix_get_speed() <= RGB_MATRIX_SPD_STEP) {
                    ind_set_blink(ind_state_blink3);
                }
            } else if (rgb_matrix_get_speed() <= RGB_MATRIX_SPD_STEP) {
                // QMK 在“松开”时才真正减速：已在最低档时拦截这次松开并归位，
                // 避免松手后又多减一档越界（修复前会掉到 0）
                rgb_matrix_set_speed(RGB_MATRIX_SPD_STEP);
                return false;
            }
        } break;
        case UG_VALU: {
            if (rgbrec_is_started()) return false;
            if (record->event.pressed) {
                if (!mm_get_rgb_enable() || !rgb_matrix_is_enabled()) {
                    // enable
                    hs_power_rgb_user_enabled();
                    mm_set_rgb_enable(true);
                    rgb_matrix_enable();
    #        ifdef LED_POWER_EN_PIN
                    gpio_write_pin_high(LED_POWER_EN_PIN);
    #        endif
                    return false;
                }

                start_hsv = rgb_matrix_get_hsv();
                if (rgb_matrix_get_val() < (RGB_MATRIX_MAXIMUM_BRIGHTNESS)) {
                    start_hsv.v = rgb_matrix_get_val() + RGB_MATRIX_VAL_STEP;
                    if (start_hsv.v >= RGB_MATRIX_MAXIMUM_BRIGHTNESS) {
                        ind_set_blink(ind_state_blink3);
                    }
                } else {
                    ind_set_blink(ind_state_blink3);
                    start_hsv.v = RGB_MATRIX_MAXIMUM_BRIGHTNESS;
                }
                rgb_matrix_sethsv(start_hsv.h, start_hsv.s, start_hsv.v);
            }
            return false;
        } break;
        case UG_VALD: {
            if (rgbrec_is_started()) return false;
            if (record->event.pressed) {
                if (rgb_matrix_get_val() <= RGB_MATRIX_VAL_STEP) {
                    ind_set_blink(ind_state_blink3);
                    if (!keymap_config.no_gui && !host_keyboard_led_state().caps_lock) {
                        if (rgb_matrix_get_val() != 0) {
                            for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
                                rgb_matrix_set_color(i, 0, 0, 0);
                            }
                        }
                    }
                }
            }
        } break;
        case TO(_BL): {
            if (record->event.pressed) {
                rgb_matrix_blink_set_remain_time(HS_RGB_BLINK_INDEX_MAC, 0);
                rgb_matrix_blink_set_interval_times(HS_RGB_BLINK_INDEX_WIN, 250, 3);
                rgb_matrix_blink_set(HS_RGB_BLINK_INDEX_WIN);
                if (keymap_is_mac_system()) {
                    set_single_persistent_default_layer(_BL);
                    layer_move(0);
                }
            }

            return false;
        } break;
        case TO(_MBL): {
            if (record->event.pressed) {
                rgb_matrix_blink_set_remain_time(HS_RGB_BLINK_INDEX_WIN, 0);
                rgb_matrix_blink_set_interval_times(HS_RGB_BLINK_INDEX_MAC, 250, 3);
                rgb_matrix_blink_set(HS_RGB_BLINK_INDEX_MAC);
                if (!keymap_is_mac_system()) {
                    set_single_persistent_default_layer(_MBL);
                    layer_move(0);
                }
            }

            return false;
        } break;
        case RP_P0: {
            if (record->event.pressed) {
                hs_rgbrec_switch_channel(0);
                dprintf("hs_settings.last_rgb_mode = %d\r\n", hs_settings.last_rgb_mode);
            }

            return false;
        } break;
        case RP_P1: {
            if (record->event.pressed) {
                hs_rgbrec_switch_channel(1);
            }

            return false;
        } break;
        case RP_P2: {
            if (record->event.pressed) {
                hs_rgbrec_switch_channel(2);
            }

            return false;
        } break;
        case RP_END: {
            if (record->event.pressed) {
                if (rgb_matrix_get_mode() != RGB_MATRIX_CUSTOM_RGBR_PLAY) {

                    return false;
                }
                if (!rgbrec_is_started()) {
                    rgbrec_start(hs_settings.rgbrec_channel);
                    no_record_fg = false;
                    rgbrec_set_close_all(HSV_BLACK);
                    ind_set_blink(ind_state_blinkon);
                } else {
                    rgbrec_end_current(true);
                    ind_set_blink(ind_state_none);
                }
                hs_settings_update();
            }

            return false;
        } break;
        case UG_NEXT: {
            if (record->event.pressed) {
                if (rgb_matrix_get_mode() == RGB_MATRIX_CUSTOM_RGBR_PLAY) {
                    if (rgbrec_is_started()) {
                        rgbrec_end_current(false); // 退出预览不保存本次未完成的录制
                        no_record_fg = false;
                        if (ind_status == ind_state_blinkon) ind_set_blink(ind_state_none);
                    }
                    uint8_t last_mode = hs_settings.last_rgb_mode;
                    if (last_mode > 0 && last_mode < RGB_MATRIX_EFFECT_MAX && last_mode != RGB_MATRIX_CUSTOM_RGBR_PLAY) {
                        rgb_matrix_mode(last_mode);
                    } else {
                        rgb_matrix_mode(RGB_MATRIX_DEFAULT_MODE);
                    }
                    hs_settings_update();
                    dprintf("hs_settings.last_rgb_mode = %d\r\n", hs_settings.last_rgb_mode);
                    start_hsv = rgb_matrix_get_hsv();
                    return false;
                }
                record_rgbmatrix_increase(&(hs_settings.last_rgb_mode));
                hs_settings_update();
                start_hsv = rgb_matrix_get_hsv();
            }

            return false;
        } break;
        case UG_HUEU: {
            if (record->event.pressed) {
                record_color_hsv(true);
                start_hsv = rgb_matrix_get_hsv();
            }

            return false;
        } break;
        case KC_LCMD: {
            if (keymap_is_mac_system()) {
                if (keymap_config.no_gui && !rgbrec_is_started()) {
                    if (record->event.pressed) {
                        register_code16(KC_LCMD);
                    } else {
                        unregister_code16(KC_LCMD);
                    }
                }
            }

            return true;
        } break;
        case KC_RCMD: {
            if (keymap_is_mac_system()) {
                if (keymap_config.no_gui && !rgbrec_is_started()) {
                    if (record->event.pressed) {
                        register_code16(KC_RCMD);
                    } else {
                        unregister_code16(KC_RCMD);
                    }
                }
            }

            return true;
        } break;
        case DEB_TOG: { // Fn+Z：开 / 关「短按键过滤」（防连击）
            if (record->event.pressed) {
                hs_set_anti_bounce(!hs_settings.anti_bounce);
            }
            return false;
        } break;
#ifdef RGB_MATRIX_ENABLE
        case EF_PLASMA: {
            if (record->event.pressed) hs_rgb_select_effect(RGB_MATRIX_CUSTOM_HS_PLASMA);
            return false;
        } break;
        case EF_SPARKLE: {
            if (record->event.pressed) hs_rgb_select_effect(RGB_MATRIX_CUSTOM_HS_SPARKLE);
            return false;
        } break;
        case EF_RIPPLE: {
            if (record->event.pressed) hs_rgb_select_effect(RGB_MATRIX_CUSTOM_HS_RIPPLE);
            return false;
        } break;
        case EF_BREATH: {
            if (record->event.pressed) hs_rgb_select_effect(RGB_MATRIX_CUSTOM_HS_BREATH_GRADIENT);
            return false;
        } break;
        case EF_FIRE: {
            if (record->event.pressed) hs_rgb_select_effect(RGB_MATRIX_CUSTOM_HS_FIRE);
            return false;
        } break;
        case EF_SCAN: {
            if (record->event.pressed) hs_rgb_select_effect(RGB_MATRIX_CUSTOM_HS_CYBER_SCAN);
            return false;
        } break;
        case EF_RAIN: {
            if (record->event.pressed) hs_rgb_select_effect(RGB_MATRIX_CUSTOM_HS_RAINDROPS);
            return false;
        } break;
#endif
        case HS_BATQ: { // 按一下显示电量约 3 秒（有线/无线都显示）
            if (record->event.pressed) {
                hs_bat_show();
            }
            return false;
        } break;
        default:
            break;
    }

    return true;
}

void housekeeping_task_user(void) { // loop

    // 用户中途真正打开背光（不是空闲关灯/状态灯临时点亮）时，临时状态灯标记才失效
    if (mm_get_rgb_enable() && !hs_power_rgb_off && !hs_indicator_rgb_acquired) {
        hs_bat_force_rgb = false;
        hs_wls_force_rgb = false;
    }

    // 记录最近一次实际灯效，退出录制预览 / VIA 切换后都能恢复正确效果
    if (hs_record_last_mode_update()) {
        hs_settings_update();
    }

    // 录制已结束但指示灯仍停在闪烁状态时（如 VIA 切通道 / EEPROM 复位）自动清理
    if (ind_status == ind_state_blinkon && !rgbrec_is_started()) {
        ind_set_blink(ind_state_none);
    }

    // EE_CLR 的 3 秒计时放在主循环，背光关闭时也能生效（原先在 RGB 渲染回调里）
    if (ee_clr_timer && timer_elapsed32(ee_clr_timer) > 3000) {
        ee_clr_timer = 0;
        hs_reset_settings();
    }

    hs_store_task();
    hs_battery_task();
    hs_status_task();
    hs_send_bat_status();
    hs_bat_indicators_task();
    hs_power_task();
    hs_rgb_blink_hook_user();
#if defined(WIRELESS_ENABLE) && defined(RGB_MATRIX_ENABLE)
    hs_wls_indicator_force_task();
#endif
    hs_set_sleep_state();
    hs_update_indicators();

    // 电量条显示约 3 秒后自动关掉
    extern bool rk_bat_req_flag;
    if (rk_bat_req_flag && timer_elapsed32(hs_bat_show_timer) > 3000) {
        rk_bat_req_flag = false;
        // 若显示前背光是关闭的，恢复关闭状态（避免因看电量把背光一直打开）
        if (hs_bat_force_rgb) {
            hs_bat_force_rgb = false;
            hs_indicator_rgb_release();
        }
    }
}

bool hs_reset_settings_user(void) {
    rgb_matrix_blink_set_color(RGB_MATRIX_BLINK_INDEX_ALL, WIITE_B, WIITE_B, WIITE_B);
    rgb_matrix_blink_set_interval_times(RGB_MATRIX_BLINK_INDEX_ALL, 250, 3);
    rgb_matrix_blink_set(RGB_MATRIX_BLINK_INDEX_ALL);

    return true;
}

bool rgb_matrix_indicators_advanced_kb(uint8_t led_min, uint8_t led_max) {

#ifdef RGBLIGHT_ENABLE
    if (rgb_matrix_indicators_advanced_user(led_min, led_max) != true) {

        return false;
    }
#endif

    // 指示灯（Caps/Num/Scroll/Win）已移到 housekeeping_task_user() 里每轮刷新，
    // 避免 RGB 渲染未运行时引脚卡在某个状态（厂商原版也是在主循环里驱动）。

#ifdef RGBLIGHT_ENABLE
    if (rgb_matrix_indicators_advanced_rgblight(led_min, led_max) != true) {

        return false;
    }
#endif

    // 背光关闭、仅为无线指示 / 状态灯临时点亮时，清掉灯效与侧灯，只保留状态指示。
    // 电量条只覆盖主键灯，不能把仍开启的侧灯一起清黑，因此这里不包含 hs_bat_force_rgb。
    if (hs_wls_force_rgb || hs_status_force_rgb) {
        rgb_matrix_set_color_all(0x00, 0x00, 0x00);
    }

#ifdef WIRELESS_ENABLE
    if (!linker_ok) {
        rgb_matrix_wls_indicator();
        hs_wls_connected_indicator();
    }
#endif

    rgb_matrix_blink_task(led_min, led_max);
    query();
    hs_status_render(led_min, led_max);
    return true;
}

void hs_reset_settings(void) {
    hs_store_flush();
    ind_status = ind_state_none;
    rgb_matrix_blink_set_remain_time(RGB_MATRIX_BLINK_INDEX_IND, 0);
    hs_bat_force_rgb = false;
    hs_wls_force_rgb = false;
#if defined(WIRELESS_ENABLE) && defined(RGB_MATRIX_ENABLE)
    wls_rgb_indicator_timer = 0;
#endif

    eeconfig_init();

#ifdef RGBLIGHT_ENABLE
    extern void rgblight_init(void);
    is_rgblight_initialized = false;
    rgblight_init();
    eeconfig_update_rgblight_default();
    rgblight_enable();
#endif

    eeconfig_update_rgb_matrix_default();
    eeconfig_read_keymap(&keymap_config);

    if (hs_reset_settings_user() != true) {
        return;
    }

    keyboard_post_init_kb();
}
