// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    HS_POWER_ACTIVE = 0,
    HS_POWER_DIM,
    HS_POWER_OFF,
} hs_power_stage_t;

uint8_t  hs_power_get_config(uint8_t value_id);
void     hs_power_set_config(uint8_t value_id, uint8_t value);
const char *hs_power_state_name(void);
uint8_t  hs_power_state_code(void);
bool     hs_power_low_battery_active(void);
uint16_t hs_power_idle_seconds(void);
uint8_t  hs_power_low_bat_threshold(void);
uint32_t hs_sleep_timeout_ms(void);
uint8_t  hs_get_sleep_timeout(void);
void     hs_set_sleep_timeout(uint8_t sleep_timeout);
void     hs_power_init(void);
void     hs_power_activity(void);
void     hs_power_task(void);
void     hs_send_bat_status(void);
void     eeconfig_power_default(void);
void     eeconfig_power_init(void);
