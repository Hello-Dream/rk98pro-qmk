// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include "color.h"
#include <stdint.h>

typedef enum {
    ind_state_none = 0,
    ind_state_exec,
    ind_state_blink3,
    ind_state_blinkon,
    ind_state_blinkoff,
} ind_status_t;

void ind_set_blink(ind_status_t status);
ind_status_t ind_status_get(void);
void ind_status_reset(void);
void ind_indicators_hook(uint8_t index);
void nkr_indicators_hook(uint8_t index);
void hs_led_power_update(void);

typedef enum {
    HS_STATUS_NONE = 0,
    HS_STATUS_CHARGING,
    HS_STATUS_FULL,
    HS_STATUS_LOW_BAT,
    HS_STATUS_CONNECT,
} hs_status_t;

void hs_status_show(hs_status_t type, uint32_t duration, RGB color);
void hs_status_end(void);
bool hs_status_busy(void);
bool hs_status_is(hs_status_t type);
bool hs_status_force_active(void);
void hs_status_overlay_task(void);
void hs_status_reset(void);
void hs_status_render(uint8_t led_min, uint8_t led_max);
bool hs_indicators_power_blocked(void);
void hs_power_rgb_user_enabled(void);
bool hs_indicator_power_off(void);
void hs_indicator_power_off_set(bool off);
void hs_indicator_rgb_acquired_clear(void);
void hs_indicator_user_rgb_on(void);
bool hs_bat_force_active(void);
void hs_bat_force_clear(void);
void hs_indicator_rgb_acquire(void);
void hs_indicator_rgb_release(void);
bool hs_wls_force_active(void);
void hs_wls_force_clear(void);
uint32_t hs_wls_force_start_time(void);
uint32_t hs_wls_force_last_time(void);
void hs_wls_force_mark(void);
void hs_wls_indicator_force_begin(void);
void hs_bat_indicators_task(void);
void hs_update_indicators(void);
void hs_bat_show(void);
void hs_bat_show_task(void);
