// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* 电池保护状态机：只做纯计算，不依赖 QMK。
 * 滤波、低电确认和充满锁存的阈值由调用方传入，避免复制 config.h 常量语义。 */

#define BATTERY_POLICY_FILTER_SIZE 5

typedef struct {
    uint8_t samples[BATTERY_POLICY_FILTER_SIZE];
    uint8_t sample_count;
    uint8_t sample_index;
    uint8_t filtered;
    uint8_t low_confirm;
} battery_filter_t;

typedef struct {
    bool     latched;
    uint32_t full_since_ms;
} battery_full_t;

void    battery_filter_reset(battery_filter_t *filter);
void    battery_filter_update(battery_filter_t *filter, uint8_t raw, uint8_t stop_threshold, uint8_t low_confirm_target);
uint8_t battery_filter_percent(const battery_filter_t *filter);
bool    battery_low_confirmed(const battery_filter_t *filter, uint8_t low_confirm_target);

void battery_full_reset(battery_full_t *full);
void battery_full_update(battery_full_t *full, bool charging, bool full_signal, uint8_t filtered, uint32_t now_ms, uint8_t full_min, uint8_t full_hysteresis, uint32_t stable_ms);
bool battery_full_latched(const battery_full_t *full);
