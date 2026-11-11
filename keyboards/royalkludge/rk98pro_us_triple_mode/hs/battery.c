// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "battery.h"
#include "battery_policy.h"
#include QMK_KEYBOARD_H

#include "timer.h"
#include "wireless.h"

bool charging_state = false;
bool bat_full_flag  = false;

#define HS_BATTERY_FILTER_MS              1000
#define HS_BATTERY_PIN_DEBOUNCE_MS        500
#define HS_BATTERY_FULL_PIN_DEBOUNCE_MS   2000
#define HS_BATTERY_FULL_STABLE_MS         3000
#define HS_BATTERY_FULL_MIN               98
#define HS_BATTERY_FULL_HYSTERESIS        95
#define HS_BATTERY_LOW_CONFIRM            3

static battery_filter_t hs_battery_filter = {.filtered = 0xFF};
static battery_full_t   hs_battery_full;

static uint32_t hs_battery_last_tick;
static uint32_t hs_battery_last_filter;
static uint32_t hs_battery_cable_changed_at;
static uint32_t hs_battery_full_changed_at;
static bool     hs_battery_cable_raw;
static bool     hs_battery_full_raw;
static bool     hs_battery_full_signal;

uint8_t hs_battery_percent(void) {
    return battery_filter_percent(&hs_battery_filter);
}

bool hs_battery_low_confirmed(void) {
    return battery_low_confirmed(&hs_battery_filter, HS_BATTERY_LOW_CONFIRM);
}

bool hs_battery_is_full(void) {
    return charging_state && battery_full_latched(&hs_battery_full);
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
        battery_filter_update(&hs_battery_filter, raw, BATTERY_CAPACITY_STOP, HS_BATTERY_LOW_CONFIRM);
    }

    battery_full_update(&hs_battery_full, charging_state, hs_battery_full_signal, battery_filter_percent(&hs_battery_filter), now, HS_BATTERY_FULL_MIN, HS_BATTERY_FULL_HYSTERESIS, HS_BATTERY_FULL_STABLE_MS);

    bat_full_flag = battery_full_latched(&hs_battery_full);
}
