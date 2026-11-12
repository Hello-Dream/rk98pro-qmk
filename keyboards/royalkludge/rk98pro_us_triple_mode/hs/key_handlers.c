// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "key_handlers.h"

#include "hs/wireless_glue.h"
#include "hs/store.h"
#include "hs/keys.h"
#include "hs/rgb.h"
#include "hs/indicators.h"
#include "hs/power.h"
#include "hs/layers.h"
#include "hs/settings.h"
#include "hs/session.h"

#include "rgb_record/rgb_record.h"
#include "rgb_record/rgb_rgblight.h"
#include "rgb_matrix_blink.h"

static HSV  start_hsv;

void hs_key_handlers_init(void) {
    start_hsv = rgb_matrix_get_hsv();
}

bool hs_key_handlers_process(uint16_t keycode, keyrecord_t *record) {
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
            hs_session_ee_clr_event(record->event.pressed);

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
                if (hs_keymap_is_mac()) {
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
                if (!hs_keymap_is_mac()) {
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
            if (hs_keymap_is_mac()) {
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
            if (hs_keymap_is_mac()) {
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
