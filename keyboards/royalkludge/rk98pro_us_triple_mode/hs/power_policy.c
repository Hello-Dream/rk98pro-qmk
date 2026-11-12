// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "power_policy.h"

static const uint32_t power_policy_dim_table[]         = {0, 15000, 30000, 60000, 120000};
static const uint32_t power_policy_off_table[]         = {0, 60000, 120000, 300000, 600000};
static const uint8_t  power_policy_low_bat_thresholds[] = {0, 5, 10, 15, 20};
static const uint8_t  power_policy_brightness_table[]  = {10, 30, 50, 100};

uint32_t power_policy_dim_ms(uint8_t level) {
    return power_policy_dim_table[level <= 4 ? level : 2];
}

uint32_t power_policy_off_ms(uint8_t level) {
    return power_policy_off_table[level <= 4 ? level : 2];
}

uint8_t power_policy_low_bat_threshold(uint8_t level) {
    return power_policy_low_bat_thresholds[level <= 4 ? level : 3];
}

uint8_t power_policy_low_bat_brightness_pct(uint8_t level) {
    return power_policy_brightness_table[level & 0x03];
}

uint8_t power_policy_low_bat_cap_value(uint8_t brightness_pct, uint8_t max_brightness) {
    uint8_t cap = (uint8_t)((uint16_t)max_brightness * brightness_pct / 100);
    return cap ? cap : 1;
}

bool power_policy_should_save(bool usb_or_charging, bool usb_save) {
    if (usb_or_charging) {
        return usb_save;
    }
    return true;
}

uint16_t power_policy_idle_seconds(uint32_t idle_ms) {
    uint32_t seconds = idle_ms / 1000;
    return seconds > 0xFFFF ? 0xFFFF : (uint16_t)seconds;
}

hs_power_stage_t power_policy_idle_target(hs_power_stage_t stage, uint32_t idle_ms, uint32_t dim_ms, uint32_t off_ms) {
    if (off_ms && idle_ms >= off_ms) {
        return HS_POWER_OFF;
    }
    if (dim_ms && idle_ms >= dim_ms && stage == HS_POWER_ACTIVE) {
        return HS_POWER_DIM;
    }
    return stage;
}

void power_policy_low_bat_reset(power_policy_low_bat_state_t *state) {
    state->active       = false;
    state->warn_pending = false;
    state->last_warn_ms = 0;
}

void power_policy_low_bat_disable(power_policy_low_bat_state_t *state) {
    state->active       = false;
    state->warn_pending = false;
}

bool power_policy_low_bat_active(const power_policy_low_bat_state_t *state) {
    return state->active;
}

uint8_t power_policy_low_bat_update(power_policy_low_bat_state_t *state, uint8_t battery, bool charging, uint8_t low_bat_level, uint32_t now_ms, bool status_busy) {
    if (low_bat_level == 0) {
        state->active       = false;
        state->warn_pending = false;
        return POWER_POLICY_LOW_BAT_ACTION_CLEAR_CAP;
    }

    uint8_t threshold  = power_policy_low_bat_threshold(low_bat_level);
    uint8_t hysteresis = state->active ? POWER_POLICY_LOW_BAT_HYSTERESIS : 0;
    bool    low        = battery != 0xFF && !charging && battery <= (uint8_t)(threshold + hysteresis);

    if (low && !state->active) {
        state->active = true;
        if (status_busy) {
            // 首次提示被连接/充电动画占用时，等动画结束后补一次
            state->warn_pending = true;
            return POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP;
        }
        state->last_warn_ms = now_ms;
        return POWER_POLICY_LOW_BAT_ACTION_WARN | POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP;
    }

    if (low && state->active) {
        uint32_t interval = battery <= POWER_POLICY_LOW_BAT_FAST_THRESHOLD ? POWER_POLICY_LOW_BAT_WARN_FAST_MS : POWER_POLICY_LOW_BAT_WARN_SLOW_MS;
        if ((state->warn_pending || (uint32_t)(now_ms - state->last_warn_ms) >= interval) && !status_busy) {
            state->warn_pending = false;
            state->last_warn_ms = now_ms;
            return POWER_POLICY_LOW_BAT_ACTION_WARN | POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP;
        }
        return POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP;
    }

    if (!low && state->active) {
        state->active       = false;
        state->warn_pending = false;
        return POWER_POLICY_LOW_BAT_ACTION_CLEAR_CAP;
    }

    return 0;
}

power_policy_charge_toast_t power_policy_charge_toast(bool charging, bool full, bool status_busy) {
    if (status_busy || !charging) {
        return POWER_POLICY_CHARGE_TOAST_NONE;
    }
    return full ? POWER_POLICY_CHARGE_TOAST_FULL : POWER_POLICY_CHARGE_TOAST_CHARGING;
}
