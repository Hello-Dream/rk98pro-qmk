// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* 电池滤波与充满判定由 battery 模块维护。 */
extern bool charging_state;
extern bool bat_full_flag;

uint8_t hs_battery_percent(void);
bool    hs_battery_low_confirmed(void);
bool    hs_battery_is_full(void);
void    hs_battery_task(void);
