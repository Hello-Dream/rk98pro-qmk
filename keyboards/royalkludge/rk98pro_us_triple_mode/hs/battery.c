// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "battery.h"
#include QMK_KEYBOARD_H

#include "timer.h"
#include "wireless.h"

bool charging_state = false;
bool bat_full_flag  = false;

#define HS_BATTERY_FILTER_SIZE            5
#define HS_BATTERY_FILTER_MS              1000
#define HS_BATTERY_PIN_DEBOUNCE_MS        500
#define HS_BATTERY_FULL_PIN_DEBOUNCE_MS   2000
#define HS_BATTERY_FULL_STABLE_MS         3000
#define HS_BATTERY_FULL_MIN               98
#define HS_BATTERY_FULL_HYSTERESIS        95
#define HS_BATTERY_LOW_CONFIRM            3

static uint8_t  hs_battery_samples[HS_BATTERY_FILTER_SIZE];
static uint8_t  hs_battery_sample_count;
static uint8_t  hs_battery_sample_index;
static uint8_t  hs_battery_filtered = 0xFF;
static uint8_t  hs_battery_low_confirm;
static uint32_t hs_battery_last_tick;
static uint32_t hs_battery_last_filter;
static uint32_t hs_battery_cable_changed_at;
static uint32_t hs_battery_full_changed_at;
static uint32_t hs_battery_full_since;
static bool     hs_battery_cable_raw;
static bool     hs_battery_full_raw;
static bool     hs_battery_full_signal;
static bool     hs_battery_full_latched;

static uint8_t hs_battery_median(uint8_t *values, uint8_t count) {
    uint8_t sorted[HS_BATTERY_FILTER_SIZE];
    memcpy(sorted, values, count);
    for (uint8_t i = 0; i < count; i++) {
        for (uint8_t j = (uint8_t)(i + 1); j < count; j++) {
            if (sorted[j] < sorted[i]) {
                uint8_t tmp = sorted[i];
                sorted[i]  = sorted[j];
                sorted[j]  = tmp;
            }
        }
    }
    return sorted[count / 2];
}

uint8_t hs_battery_percent(void) {
    return hs_battery_filtered;
}

bool hs_battery_low_confirmed(void) {
    return hs_battery_low_confirm >= HS_BATTERY_LOW_CONFIRM;
}

bool hs_battery_is_full(void) {
    return charging_state && hs_battery_full_latched;
}

void hs_battery_task(void) {
    uint32_t now = timer_read32();
    if (hs_battery_last_tick && timer_elapsed32(hs_battery_last_tick) < 100) {
        return;
    }
    hs_battery_last_tick = now;

    bool cable = gpio_read_pin(HS_BAT_CABLE_PIN);
    if (cable != hs_battery_cable_raw) {
        hs_battery_cable_raw       = cable;
        hs_battery_cable_changed_at = now;
    }
    if (timer_elapsed32(hs_battery_cable_changed_at) >= HS_BATTERY_PIN_DEBOUNCE_MS) {
        charging_state = cable;
    }

    bool full = gpio_read_pin(BAT_FULL_PIN);
    if (full != hs_battery_full_raw) {
        hs_battery_full_raw        = full;
        hs_battery_full_changed_at = now;
    }
    if (full != hs_battery_full_signal && timer_elapsed32(hs_battery_full_changed_at) >= HS_BATTERY_FULL_PIN_DEBOUNCE_MS) {
        hs_battery_full_signal = full;
    }

    if (timer_elapsed32(hs_battery_last_filter) >= HS_BATTERY_FILTER_MS) {
        hs_battery_last_filter = now;
        uint8_t raw            = *md_getp_bat();
        if (raw <= 100) {
            if (hs_battery_sample_count < HS_BATTERY_FILTER_SIZE) {
                hs_battery_samples[hs_battery_sample_count++] = raw;
            } else {
                hs_battery_samples[hs_battery_sample_index] = raw;
                hs_battery_sample_index = (uint8_t)((hs_battery_sample_index + 1) % HS_BATTERY_FILTER_SIZE);
            }

            if (hs_battery_sample_count >= 3) {
                uint8_t median = hs_battery_median(hs_battery_samples, hs_battery_sample_count);
                if (hs_battery_filtered == 0xFF) {
                    hs_battery_filtered = median;
                } else {
                    hs_battery_filtered = (uint8_t)(((uint16_t)hs_battery_filtered * 3 + median + 2) / 4);
                }
            }

            if (raw <= BATTERY_CAPACITY_STOP) {
                if (hs_battery_low_confirm < HS_BATTERY_LOW_CONFIRM) {
                    hs_battery_low_confirm++;
                }
            } else {
                hs_battery_low_confirm = 0;
            }
        } else {
            hs_battery_filtered    = 0xFF;
            hs_battery_sample_count = 0;
            hs_battery_sample_index = 0;
            hs_battery_low_confirm  = 0;
        }
    }

    if (!charging_state) {
        hs_battery_full_latched = false;
        hs_battery_full_since   = 0;
    } else if (!hs_battery_full_latched) {
        if (hs_battery_full_signal && hs_battery_filtered != 0xFF && hs_battery_filtered >= HS_BATTERY_FULL_MIN) {
            if (!hs_battery_full_since) {
                hs_battery_full_since = now;
            } else if (timer_elapsed32(hs_battery_full_since) >= HS_BATTERY_FULL_STABLE_MS) {
                hs_battery_full_latched = true;
            }
        } else {
            hs_battery_full_since = 0;
        }
    } else if (hs_battery_filtered == 0xFF || hs_battery_filtered < HS_BATTERY_FULL_HYSTERESIS) {
        hs_battery_full_latched = false;
    }

    bat_full_flag = hs_battery_full_latched;
}
