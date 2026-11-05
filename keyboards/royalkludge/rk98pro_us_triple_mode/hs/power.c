// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "power.h"

#include "hs/wireless_glue.h"
#include "hs/store.h"
#include "hs/indicators.h"

#include "wireless.h"
#include "rgb_record/rgb_record.h"

static void hs_power_apply_settings(void);
static void hs_low_bat_refresh_cap(void);
static void hs_low_bat_apply_cap(void);
static void hs_low_bat_clear_cap(void);

static const uint32_t hs_power_dim_table[]   = {0, 15000, 30000, 60000, 120000};
static const uint32_t hs_power_off_table[]   = {0, 60000, 120000, 300000, 600000};
static const uint8_t  hs_low_bat_thresholds[] = {0, 5, 10, 15, 20};
static const uint8_t  hs_low_bat_brightness_table[] = {10, 30, 50, 100};

static hs_power_stage_t hs_power_stage = HS_POWER_ACTIVE;
static uint32_t hs_last_activity = 0;
static HSV hs_saved_rgb_hsv;
static HSV hs_saved_side_hsv;
static bool hs_saved_rgb_enabled = false;
static bool hs_saved_side_enabled = false;
static bool hs_saved_valid = false;
static bool hs_low_bat_active = false;
static bool hs_low_bat_capped = false;
static uint8_t hs_low_bat_saved_val = 0;
static bool hs_low_bat_side_capped = false;
static uint8_t hs_low_bat_saved_side_val = 0;
static bool hs_low_bat_warn_pending = false;
static uint32_t hs_low_bat_last_warn = 0;
static uint8_t hs_last_devs = 0xFF;
static bool hs_power_devs_initialized = false;
static bool hs_last_charging = false;
static bool hs_last_full = false;

static uint32_t hs_power_dim_ms(void) {
    return hs_power_dim_table[hs_power.idle_dim < ARRAY_SIZE(hs_power_dim_table) ? hs_power.idle_dim : 2];
}

static uint32_t hs_power_off_ms(void) {
    return hs_power_off_table[hs_power.idle_off < ARRAY_SIZE(hs_power_off_table) ? hs_power.idle_off : 2];
}

uint8_t hs_power_low_bat_threshold(void) {
    uint8_t level = hs_power.low_bat_level;
    if (level > 4) {
        level = 3;
    }
    return hs_low_bat_thresholds[level];
}

static uint8_t hs_power_low_bat_brightness_pct(void) {
    return hs_low_bat_brightness_table[hs_power.low_bat_brightness & 0x03];
}

static uint8_t hs_low_bat_cap_value(void) {
    uint8_t cap = (uint8_t)((uint16_t)RGB_MATRIX_MAXIMUM_BRIGHTNESS * hs_power_low_bat_brightness_pct() / 100);
    return cap ? cap : 1;
}

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
        case id_power_idle_dim:
            hs_power.idle_dim = value <= 4 ? value : 2;
            break;
        case id_power_idle_off:
            hs_power.idle_off = value <= 4 ? value : 2;
            break;
        case id_power_usb_save:
            hs_power.usb_save = value ? 1 : 0;
            break;
        case id_power_low_bat_warn:
            hs_power.low_bat_level = value <= 4 ? value : 3;
            break;
        case id_power_low_bat_brightness:
            hs_power.low_bat_brightness = value & 0x03;
            break;
        case id_power_status_led:
            hs_power.status_led = value ? 1 : 0;
            break;
        default:
            return;
    }

    eeconfig_power_update();
    hs_power_apply_settings();
    if (value_id == id_power_low_bat_brightness) {
        hs_low_bat_refresh_cap();
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
    hs_settings.sleep_timeout = sleep_timeout;
    lpwr_set_timeout_value(hs_sleep_timeout_ms()); // 让 VIA 设置真正生效（原来从未下发，恒为 5 分钟）
}

void hs_power_init(void) {
    hs_power_stage     = HS_POWER_ACTIVE;
    hs_last_activity   = timer_read32();
    hs_saved_valid     = false;
    hs_power_rgb_off   = false;
    hs_low_bat_active  = false;
    hs_low_bat_capped  = false;
    hs_low_bat_saved_val = 0;
    hs_low_bat_side_capped = false;
    hs_low_bat_saved_side_val = 0;
    hs_low_bat_warn_pending = false;
    hs_low_bat_last_warn = 0;
    hs_status          = HS_STATUS_NONE;
    hs_status_force_rgb = false;
    hs_indicator_rgb_acquired = false;

    hs_last_devs             = hs_settings.wireless_mode;
    hs_power_devs_initialized = false;
    hs_last_charging   = charging_state;
    hs_last_full       = bat_full_flag;
}

static bool hs_power_should_save(void) {
    if (wireless_get_current_devs() == DEVS_USB || charging_state) {
        return hs_power.usb_save;
    }
    return true;
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
    if (hs_low_bat_active) hs_low_bat_apply_cap();
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
    if (!hs_low_bat_active || hs_power_stage != HS_POWER_ACTIVE) return;

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

static void hs_low_bat_refresh_cap(void) {
    if (!hs_low_bat_active) return;

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
    if (hs_low_bat_active) hs_low_bat_apply_cap();
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
        if (hs_status == HS_STATUS_NONE) {
            if (charging_state) {
                hs_status_show(bat_full_flag ? HS_STATUS_FULL : HS_STATUS_CHARGING, 3000, (RGB){0, 150, 0});
            }
        }
    }

    if (!hs_power.low_bat_level) {
        hs_low_bat_warn_pending = false;
        hs_low_bat_active       = false;
        hs_low_bat_clear_cap();
    } else {
        bool low = battery != 0xFF && !charging_state && (battery <= (uint8_t)(hs_power_low_bat_threshold() + (hs_low_bat_active ? 3 : 0)));
        if (low) {
            if (!hs_low_bat_active) {
                hs_low_bat_active = true;
                if (hs_status == HS_STATUS_NONE) {
                    hs_low_bat_last_warn = now;
                    hs_status_show(HS_STATUS_LOW_BAT, 3000, (RGB){150, 0, 0});
                } else {
                    // 首次提示被连接/充电动画占用时，等动画结束后补一次
                    hs_low_bat_warn_pending = true;
                }
            } else {
                uint32_t interval = (battery != 0xFF && battery <= 5) ? 300000 : 600000;
                if ((hs_low_bat_warn_pending || timer_elapsed32(hs_low_bat_last_warn) >= interval) && hs_status == HS_STATUS_NONE) {
                    hs_low_bat_warn_pending = false;
                    hs_low_bat_last_warn    = now;
                    hs_status_show(HS_STATUS_LOW_BAT, 3000, (RGB){150, 0, 0});
                }
            }
            hs_low_bat_apply_cap();
        } else if (hs_low_bat_active) {
            hs_low_bat_active       = false;
            hs_low_bat_warn_pending = false;
            hs_low_bat_clear_cap();
        }
    }

    if (!hs_power_should_save()) {
        if (hs_power_stage != HS_POWER_ACTIVE) hs_power_restore();
        return;
    }
    if (hs_indicators_power_blocked()) return;

    uint32_t idle = timer_elapsed32(hs_last_activity);
    uint32_t off  = hs_power_off_ms();
    uint32_t dim  = hs_power_dim_ms();

    if (off && idle >= off) {
        hs_power_enter_off();
    } else if (dim && idle >= dim) {
        hs_power_enter_dim();
    }
}

static void hs_power_apply_settings(void) {
    hs_last_activity = timer_read32();

    if (!hs_power.status_led && (hs_status != HS_STATUS_NONE || hs_status_force_rgb)) {
        hs_status_end();
    }
    if (!hs_power.low_bat_level && hs_status == HS_STATUS_LOW_BAT) {
        hs_status_end();
    }
    if (!hs_power.low_bat_level) {
        hs_low_bat_warn_pending = false;
        hs_low_bat_active       = false;
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
    if (hs_low_bat_active) hs_low_bat_apply_cap();
}

const char *hs_power_state_name(void) {
    if (charging_state && bat_full_flag) return "Full";
    if (charging_state) return "Charging";
    if (hs_low_bat_active) return "Low Battery";
    if (hs_power_stage == HS_POWER_DIM) return "Dim";
    if (hs_power_stage == HS_POWER_OFF) return "Off";
    return "Active";
}

uint8_t hs_power_state_code(void) {
    if (charging_state && bat_full_flag) return HS_POWER_STATE_FULL;
    if (charging_state) return HS_POWER_STATE_CHARGING;
    if (hs_low_bat_active) return HS_POWER_STATE_LOW_BATTERY;
    if (hs_power_stage == HS_POWER_DIM) return HS_POWER_STATE_DIM;
    if (hs_power_stage == HS_POWER_OFF) return HS_POWER_STATE_OFF;
    return HS_POWER_STATE_ACTIVE;
}

bool hs_power_low_battery_active(void) {
    return hs_low_bat_active;
}

uint16_t hs_power_idle_seconds(void) {
    uint32_t idle = timer_elapsed32(hs_last_activity) / 1000;
    return idle > 0xFFFF ? 0xFFFF : (uint16_t)idle;
}

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
