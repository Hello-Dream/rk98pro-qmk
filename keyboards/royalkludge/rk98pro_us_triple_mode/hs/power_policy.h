// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* 电源策略：调光/关灯档位、低电状态机和充电提示的纯决策，不依赖 QMK。
 * HSV 快照/恢复、GPIO、状态灯和无线上报等副作用仍留在 power.c。 */

typedef enum {
    HS_POWER_ACTIVE = 0,
    HS_POWER_DIM,
    HS_POWER_OFF,
} hs_power_stage_t;

#define POWER_POLICY_LOW_BAT_HYSTERESIS     3
#define POWER_POLICY_LOW_BAT_FAST_THRESHOLD 5
#define POWER_POLICY_LOW_BAT_WARN_FAST_MS   300000u
#define POWER_POLICY_LOW_BAT_WARN_SLOW_MS   600000u

#define POWER_POLICY_LOW_BAT_ACTION_WARN      (1u << 0)
#define POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP (1u << 1)
#define POWER_POLICY_LOW_BAT_ACTION_CLEAR_CAP (1u << 2)

typedef struct {
    bool     active;
    bool     warn_pending;
    uint32_t last_warn_ms;
} power_policy_low_bat_state_t;

typedef enum {
    POWER_POLICY_CHARGE_TOAST_NONE = 0,
    POWER_POLICY_CHARGE_TOAST_CHARGING,
    POWER_POLICY_CHARGE_TOAST_FULL,
} power_policy_charge_toast_t;

uint32_t         power_policy_dim_ms(uint8_t level);
uint32_t         power_policy_off_ms(uint8_t level);
uint8_t          power_policy_low_bat_threshold(uint8_t level);
uint8_t          power_policy_low_bat_brightness_pct(uint8_t level);
uint8_t          power_policy_low_bat_cap_value(uint8_t brightness_pct, uint8_t max_brightness);
bool             power_policy_should_save(bool usb_or_charging, bool usb_save);
uint16_t         power_policy_idle_seconds(uint32_t idle_ms);
hs_power_stage_t power_policy_idle_target(hs_power_stage_t stage, uint32_t idle_ms, uint32_t dim_ms, uint32_t off_ms);

void    power_policy_low_bat_reset(power_policy_low_bat_state_t *state);
void    power_policy_low_bat_disable(power_policy_low_bat_state_t *state);
bool    power_policy_low_bat_active(const power_policy_low_bat_state_t *state);

/* 低电状态机：返回 POWER_POLICY_LOW_BAT_ACTION_* 动作位，调用方按位执行副作用。
 * status_busy 表示调用点已有状态提示占用，必须在调用点即时读取。 */
uint8_t power_policy_low_bat_update(power_policy_low_bat_state_t *state, uint8_t battery, bool charging, uint8_t low_bat_level, uint32_t now_ms, bool status_busy);

power_policy_charge_toast_t power_policy_charge_toast(bool charging, bool full, bool status_busy);
