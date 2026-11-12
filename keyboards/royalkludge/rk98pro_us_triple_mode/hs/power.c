// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "power.h"

#include "power_internal.h"

#include "hs/wireless_glue.h"
#include "hs/store.h"
#include "hs/indicators.h"

#include "wireless.h"
#include "rgb_record/rgb_record.h"

static void hs_low_bat_apply_cap(void);
static void hs_low_bat_clear_cap(void);

static hs_power_stage_t hs_power_stage = HS_POWER_ACTIVE;
static uint32_t hs_last_activity = 0;
static HSV hs_saved_rgb_hsv;
static HSV hs_saved_side_hsv;
static bool hs_saved_rgb_enabled = false;
static bool hs_saved_side_enabled = false;
static bool hs_saved_valid = false;
static power_policy_low_bat_state_t hs_low_bat_state;
static bool hs_low_bat_capped = false;
static uint8_t hs_low_bat_saved_val = 0;
static bool hs_low_bat_side_capped = false;
static uint8_t hs_low_bat_saved_side_val = 0;
static uint8_t hs_last_devs = 0xFF;
static bool hs_power_devs_initialized = false;
static bool hs_last_charging = false;
static bool hs_last_full = false;

static uint32_t hs_power_dim_ms(void) {
    return power_policy_dim_ms(hs_power.idle_dim);
}

static uint32_t hs_power_off_ms(void) {
    return power_policy_off_ms(hs_power.idle_off);
}

uint8_t hs_power_low_bat_threshold(void) {
    return power_policy_low_bat_threshold(hs_power.low_bat_level);
}

static uint8_t hs_power_low_bat_brightness_pct(void) {
    return power_policy_low_bat_brightness_pct(hs_power.low_bat_brightness);
}

static uint8_t hs_low_bat_cap_value(void) {
    return power_policy_low_bat_cap_value(hs_power_low_bat_brightness_pct(), RGB_MATRIX_MAXIMUM_BRIGHTNESS);
}

void hs_power_init(void) {
    hs_power_stage     = HS_POWER_ACTIVE;
    hs_last_activity   = timer_read32();
    hs_saved_valid     = false;
    hs_power_rgb_off   = false;
    power_policy_low_bat_reset(&hs_low_bat_state);
    hs_low_bat_capped  = false;
    hs_low_bat_saved_val = 0;
    hs_low_bat_side_capped = false;
    hs_low_bat_saved_side_val = 0;
    hs_status          = HS_STATUS_NONE;
    hs_status_force_rgb = false;
    hs_indicator_rgb_acquired = false;

    hs_last_devs             = hs_settings.wireless_mode;
    hs_power_devs_initialized = false;
    hs_last_charging   = charging_state;
    hs_last_full       = bat_full_flag;
}

static bool hs_power_should_save(void) {
    return power_policy_should_save(wireless_get_current_devs() == DEVS_USB || charging_state, hs_power.usb_save);
}

static void hs_power_save_snapshot(void) {
    hs_saved_rgb_enabled = rgb_matrix_is_enabled();
    hs_saved_rgb_hsv     = rgb_matrix_get_hsv();
    hs_saved_side_enabled = rgblight_is_enabled();
    hs_saved_side_hsv     = rgblight_get_hsv();
    hs_saved_valid        = true;
}

static void hs_power_restore(void) {
    if (hs_power_stage == HS_POWER_ACTIVE) return;

    hs_power_stage   = HS_POWER_ACTIVE;
    hs_power_rgb_off = false;

    // 空闲期间用户可能通过上位机改了总开关或侧灯模式，恢复时以最新设置为准
    if (hs_saved_rgb_enabled && mm_get_rgb_enable()) {
        rgb_matrix_enable_noeeprom();
        rgb_matrix_sethsv_noeeprom(hs_saved_rgb_hsv.h, hs_saved_rgb_hsv.s, hs_saved_rgb_hsv.v);
        hs_indicator_rgb_acquired = false;
    }
    if (hs_saved_side_enabled && mm_get_rgb_enable() && hs_settings.side_light_mode != 10) {
        rgblight_enable_noeeprom();
        rgblight_sethsv_noeeprom(hs_saved_side_hsv.h, hs_saved_side_hsv.s, hs_saved_side_hsv.v);
    }

    hs_saved_valid = false;
    if (hs_low_bat_state.active) hs_low_bat_apply_cap();
    hs_led_power_update();
}

static void hs_power_enter_dim(void) {
    if (hs_power_stage != HS_POWER_ACTIVE) return;

    hs_power_save_snapshot();
    hs_power_stage = HS_POWER_DIM;

    if (hs_saved_rgb_enabled) {
        HSV hsv = hs_saved_rgb_hsv;
        if (hsv.v > 0) {
            hsv.v = (uint8_t)((uint16_t)hsv.v * 30 / 100);
            if (hsv.v == 0) hsv.v = 1;
        }
        rgb_matrix_sethsv_noeeprom(hsv.h, hsv.s, hsv.v);
    }
    if (hs_saved_side_enabled) {
        HSV hsv = hs_saved_side_hsv;
        if (hsv.v > 0) {
            hsv.v = (uint8_t)((uint16_t)hsv.v * 30 / 100);
            if (hsv.v == 0) hsv.v = 1;
        }
        rgblight_sethsv_noeeprom(hsv.h, hsv.s, hsv.v);
    }

    hs_led_power_update();
}

static void hs_power_enter_off(void) {
    if (hs_power_stage == HS_POWER_OFF) return;

    if (hs_power_stage == HS_POWER_ACTIVE) {
        hs_power_save_snapshot();
    }
    hs_power_stage   = HS_POWER_OFF;
    hs_power_rgb_off = true;

    if (hs_saved_rgb_enabled) rgb_matrix_disable_noeeprom();
    if (hs_saved_side_enabled) rgblight_disable_noeeprom();

    hs_led_power_update();
}

static void hs_low_bat_apply_cap(void) {
    if (!hs_low_bat_state.active || hs_power_stage != HS_POWER_ACTIVE) return;

    uint8_t cap = hs_low_bat_cap_value();

    if (mm_get_rgb_enable() && rgb_matrix_is_enabled()) {
        HSV hsv = rgb_matrix_get_hsv();
        if (hsv.v > cap) {
            if (!hs_low_bat_capped) {
                hs_low_bat_saved_val = hsv.v;
                hs_low_bat_capped    = true;
            }
            rgb_matrix_sethsv_noeeprom(hsv.h, hsv.s, cap);
        }
    }

    if (rgblight_is_enabled()) {
        HSV hsv = rgblight_get_hsv();
        if (hsv.v > cap) {
            if (!hs_low_bat_side_capped) {
                hs_low_bat_saved_side_val = hsv.v;
                hs_low_bat_side_capped    = true;
            }
            rgblight_sethsv_noeeprom(hsv.h, hsv.s, cap);
        }
    }
}

static void hs_low_bat_clear_cap(void) {
    uint8_t cap = hs_low_bat_cap_value();

    if (hs_low_bat_capped) {
        if (hs_power_stage == HS_POWER_ACTIVE) {
            HSV hsv = rgb_matrix_get_hsv();
            if (hsv.v <= cap) {
                hsv.v = hs_low_bat_saved_val;
                rgb_matrix_sethsv_noeeprom(hsv.h, hsv.s, hsv.v);
            }
        } else if (hs_saved_valid) {
            // 空闲调光/关灯期间解除低电限制时，恢复值也要回到用户原亮度
            hs_saved_rgb_hsv.v = hs_low_bat_saved_val;
        }
        hs_low_bat_capped = false;
    }

    if (hs_low_bat_side_capped) {
        if (hs_power_stage == HS_POWER_ACTIVE) {
            HSV hsv = rgblight_get_hsv();
            if (hsv.v <= cap) {
                hsv.v = hs_low_bat_saved_side_val;
                rgblight_sethsv_noeeprom(hsv.h, hsv.s, hsv.v);
            }
        } else if (hs_saved_valid) {
            hs_saved_side_hsv.v = hs_low_bat_saved_side_val;
        }
        hs_low_bat_side_capped = false;
    }
}

void hs_power_low_bat_refresh_cap(void) {
    if (!hs_low_bat_state.active) return;

    uint8_t cap = hs_low_bat_cap_value();

    if (hs_low_bat_capped) {
        uint8_t target = hs_low_bat_saved_val < cap ? hs_low_bat_saved_val : cap;
        if (hs_power_stage == HS_POWER_ACTIVE) {
            HSV hsv = rgb_matrix_get_hsv();
            if (hsv.v != target) {
                rgb_matrix_sethsv_noeeprom(hsv.h, hsv.s, target);
            }
        } else if (hs_saved_valid) {
            hs_saved_rgb_hsv.v = target;
        }
    }

    if (hs_low_bat_side_capped) {
        uint8_t target = hs_low_bat_saved_side_val < cap ? hs_low_bat_saved_side_val : cap;
        if (hs_power_stage == HS_POWER_ACTIVE) {
            HSV hsv = rgblight_get_hsv();
            if (hsv.v != target) {
                rgblight_sethsv_noeeprom(hsv.h, hsv.s, target);
            }
        } else if (hs_saved_valid) {
            hs_saved_side_hsv.v = target;
        }
    }
}

void hs_power_activity(void) {
    hs_last_activity = timer_read32();
    if (hs_power_stage != HS_POWER_ACTIVE) {
        hs_power_restore();
    }
    if (hs_low_bat_state.active) hs_low_bat_apply_cap();
}

void hs_power_task(void) {
    uint32_t now     = timer_read32();
    uint8_t  battery = hs_battery_percent();

    if (hs_status != HS_STATUS_NONE && timer_elapsed32(hs_status_start) >= hs_status_duration) {
        hs_status_end();
    }
    if (hs_status == HS_STATUS_NONE && hs_status_force_rgb) {
        hs_status_end();
    }

    // 低电保护倒计时：侧灯持续红色闪烁，直到进入保护深睡
    if (!charging_state && hs_battery_low_confirmed() && hs_status != HS_STATUS_LOW_BAT) {
        hs_status_show(HS_STATUS_LOW_BAT, 20000, (RGB){150, 0, 0});
    }

    // 切换设备时用侧灯短暂提示通道颜色（通道键自身仍有原有闪烁提示）
    uint8_t devs = wireless_get_current_devs();
    if (!hs_power_devs_initialized) {
        // 启动时无线模式可能在 post_init 之后才确定；首轮不比较，避免把首次模式切换当成用户切换
        hs_power_devs_initialized = true;
    } else if (devs != hs_last_devs) {
        hs_last_devs = devs;
        RGB color = {0};
        switch (devs) {
            case DEVS_BT1: color = (RGB){HS_LBACK_COLOR_BT1}; break;
            case DEVS_BT2: color = (RGB){HS_LBACK_COLOR_BT2}; break;
            case DEVS_BT3: color = (RGB){HS_LBACK_COLOR_BT3}; break;
            case DEVS_2G4: color = (RGB){HS_LBACK_COLOR_2G4}; break;
            case DEVS_USB: color = (RGB){HS_LBACK_COLOR_USB}; break;
            default: break;
        }
        // 连续切换时直接重启提示，让侧灯颜色始终跟随最后一次通道
        hs_status_show(HS_STATUS_CONNECT, 1000, color);
    }

    // 插入充电线 / 充满时短暂提示，之后只保留 POV 指示灯
    if (charging_state != hs_last_charging || bat_full_flag != hs_last_full) {
        hs_last_charging = charging_state;
        hs_last_full     = bat_full_flag;
        switch (power_policy_charge_toast(charging_state, bat_full_flag, hs_status != HS_STATUS_NONE)) {
            case POWER_POLICY_CHARGE_TOAST_CHARGING:
                hs_status_show(HS_STATUS_CHARGING, 3000, (RGB){0, 150, 0});
                break;
            case POWER_POLICY_CHARGE_TOAST_FULL:
                hs_status_show(HS_STATUS_FULL, 3000, (RGB){0, 150, 0});
                break;
            default:
                break;
        }
    }

    uint8_t low_bat_actions = power_policy_low_bat_update(&hs_low_bat_state, battery, charging_state, hs_power.low_bat_level, now, hs_status != HS_STATUS_NONE);
    if (low_bat_actions & POWER_POLICY_LOW_BAT_ACTION_WARN) {
        hs_status_show(HS_STATUS_LOW_BAT, 3000, (RGB){150, 0, 0});
    }
    if (low_bat_actions & POWER_POLICY_LOW_BAT_ACTION_CLEAR_CAP) {
        hs_low_bat_clear_cap();
    }
    if (low_bat_actions & POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP) {
        hs_low_bat_apply_cap();
    }

    if (!hs_power_should_save()) {
        if (hs_power_stage != HS_POWER_ACTIVE) hs_power_restore();
        return;
    }
    if (hs_indicators_power_blocked()) return;

    uint32_t idle = timer_elapsed32(hs_last_activity);
    hs_power_stage_t target = power_policy_idle_target(hs_power_stage, idle, hs_power_dim_ms(), hs_power_off_ms());

    if (target == HS_POWER_OFF) {
        hs_power_enter_off();
    } else if (target == HS_POWER_DIM) {
        hs_power_enter_dim();
    }
}

void hs_power_apply_settings(void) {
    hs_last_activity = timer_read32();

    if (!hs_power.status_led && (hs_status != HS_STATUS_NONE || hs_status_force_rgb)) {
        hs_status_end();
    }
    if (!hs_power.low_bat_level && hs_status == HS_STATUS_LOW_BAT) {
        hs_status_end();
    }
    if (!hs_power.low_bat_level) {
        power_policy_low_bat_disable(&hs_low_bat_state);
        hs_low_bat_clear_cap();
    }
    if (!hs_power_should_save() && hs_power_stage != HS_POWER_ACTIVE) {
        hs_power_restore();
    }
    if (hs_power_stage == HS_POWER_DIM && hs_power_dim_ms() == 0) {
        hs_power_restore();
    }
    if (hs_power_stage == HS_POWER_OFF && hs_power_off_ms() == 0) {
        hs_power_restore();
    }
    if (hs_low_bat_state.active) hs_low_bat_apply_cap();
}

const char *hs_power_state_name(void) {
    if (charging_state && bat_full_flag) return "Full";
    if (charging_state) return "Charging";
    if (hs_low_bat_state.active) return "Low Battery";
    if (hs_power_stage == HS_POWER_DIM) return "Dim";
    if (hs_power_stage == HS_POWER_OFF) return "Off";
    return "Active";
}

uint8_t hs_power_state_code(void) {
    if (charging_state && bat_full_flag) return HS_POWER_STATE_FULL;
    if (charging_state) return HS_POWER_STATE_CHARGING;
    if (hs_low_bat_state.active) return HS_POWER_STATE_LOW_BATTERY;
    if (hs_power_stage == HS_POWER_DIM) return HS_POWER_STATE_DIM;
    if (hs_power_stage == HS_POWER_OFF) return HS_POWER_STATE_OFF;
    return HS_POWER_STATE_ACTIVE;
}

bool hs_power_low_battery_active(void) {
    return hs_low_bat_state.active;
}

uint16_t hs_power_idle_seconds(void) {
    return power_policy_idle_seconds(timer_elapsed32(hs_last_activity));
}
