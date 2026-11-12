// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "power_policy.h"

#include <stdio.h>

static int failures;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__,    \
                    #condition);                                                \
            failures++;                                                         \
        }                                                                       \
    } while (0)

static void test_dim_off_tables(void) {
    const uint32_t dim[] = {0, 15000, 30000, 60000, 120000};
    const uint32_t off[] = {0, 60000, 120000, 300000, 600000};

    for (uint8_t level = 0; level <= 4; level++) {
        CHECK(power_policy_dim_ms(level) == dim[level]);
        CHECK(power_policy_off_ms(level) == off[level]);
    }
    CHECK(power_policy_dim_ms(5) == 30000);
    CHECK(power_policy_dim_ms(0xFF) == 30000);
    CHECK(power_policy_off_ms(5) == 120000);
    CHECK(power_policy_off_ms(0xFF) == 120000);
}

static void test_low_bat_tables(void) {
    const uint8_t threshold[] = {0, 5, 10, 15, 20};
    const uint8_t brightness[] = {10, 30, 50, 100};

    for (uint8_t level = 0; level <= 4; level++) {
        CHECK(power_policy_low_bat_threshold(level) == threshold[level]);
    }
    CHECK(power_policy_low_bat_threshold(5) == 15);
    CHECK(power_policy_low_bat_threshold(0xFF) == 15);

    for (uint8_t level = 0; level <= 3; level++) {
        CHECK(power_policy_low_bat_brightness_pct(level) == brightness[level]);
    }
    CHECK(power_policy_low_bat_brightness_pct(4) == 10);
    CHECK(power_policy_low_bat_brightness_pct(0xFF) == 100);
}

static void test_cap_value(void) {
    CHECK(power_policy_low_bat_cap_value(10, 255) == 25);
    CHECK(power_policy_low_bat_cap_value(30, 255) == 76);
    CHECK(power_policy_low_bat_cap_value(50, 255) == 127);
    CHECK(power_policy_low_bat_cap_value(100, 255) == 255);

    CHECK(power_policy_low_bat_cap_value(10, 0) == 1);
    CHECK(power_policy_low_bat_cap_value(10, 1) == 1);
    CHECK(power_policy_low_bat_cap_value(50, 2) == 1);
    CHECK(power_policy_low_bat_cap_value(100, 1) == 1);
}

static void test_should_save(void) {
    CHECK(power_policy_should_save(true, true));
    CHECK(!power_policy_should_save(true, false));
    CHECK(power_policy_should_save(false, true));
    CHECK(power_policy_should_save(false, false));
}

static void test_idle_seconds_clamp(void) {
    CHECK(power_policy_idle_seconds(0) == 0);
    CHECK(power_policy_idle_seconds(999) == 0);
    CHECK(power_policy_idle_seconds(1000) == 1);
    CHECK(power_policy_idle_seconds(65535999) == 65535);
    CHECK(power_policy_idle_seconds(65536000) == 65535);
    CHECK(power_policy_idle_seconds(0xFFFFFFFF) == 65535);
}

static void test_idle_target(void) {
    CHECK(power_policy_idle_target(HS_POWER_ACTIVE, 999, 1000, 5000) == HS_POWER_ACTIVE);
    CHECK(power_policy_idle_target(HS_POWER_ACTIVE, 1000, 1000, 5000) == HS_POWER_DIM);
    CHECK(power_policy_idle_target(HS_POWER_ACTIVE, 4999, 1000, 5000) == HS_POWER_DIM);
    CHECK(power_policy_idle_target(HS_POWER_ACTIVE, 5000, 1000, 5000) == HS_POWER_OFF);

    CHECK(power_policy_idle_target(HS_POWER_DIM, 1000, 1000, 5000) == HS_POWER_DIM);
    CHECK(power_policy_idle_target(HS_POWER_DIM, 5000, 1000, 5000) == HS_POWER_OFF);
    CHECK(power_policy_idle_target(HS_POWER_OFF, 6000, 1000, 5000) == HS_POWER_OFF);

    CHECK(power_policy_idle_target(HS_POWER_ACTIVE, 60000, 0, 0) == HS_POWER_ACTIVE);
    CHECK(power_policy_idle_target(HS_POWER_ACTIVE, 60000, 0, 5000) == HS_POWER_OFF);
    CHECK(power_policy_idle_target(HS_POWER_ACTIVE, 60000, 1000, 0) == HS_POWER_DIM);
    CHECK(power_policy_idle_target(HS_POWER_DIM, 60000, 1000, 0) == HS_POWER_DIM);
    CHECK(power_policy_idle_target(HS_POWER_OFF, 60000, 1000, 0) == HS_POWER_OFF);
}

static void test_low_bat_first_warning(void) {
    power_policy_low_bat_state_t state;
    power_policy_low_bat_reset(&state);

    uint8_t actions = power_policy_low_bat_update(&state, 10, false, 3, 1000, false);
    CHECK(actions == (POWER_POLICY_LOW_BAT_ACTION_WARN | POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP));
    CHECK(state.active);
    CHECK(!state.warn_pending);
    CHECK(state.last_warn_ms == 1000);
    CHECK(power_policy_low_bat_active(&state));
}

static void test_low_bat_busy_pending(void) {
    power_policy_low_bat_state_t state;
    power_policy_low_bat_reset(&state);

    uint8_t actions = power_policy_low_bat_update(&state, 10, false, 3, 1000, true);
    CHECK(actions == POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP);
    CHECK(state.active);
    CHECK(state.warn_pending);
    CHECK(state.last_warn_ms == 0);

    actions = power_policy_low_bat_update(&state, 10, false, 3, 5000, false);
    CHECK(actions == (POWER_POLICY_LOW_BAT_ACTION_WARN | POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP));
    CHECK(!state.warn_pending);
    CHECK(state.last_warn_ms == 5000);
}

static void test_low_bat_slow_interval(void) {
    power_policy_low_bat_state_t state;
    power_policy_low_bat_reset(&state);
    power_policy_low_bat_update(&state, 10, false, 3, 1000, false);

    CHECK(power_policy_low_bat_update(&state, 10, false, 3, 600999, false) == POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP);
    CHECK(state.last_warn_ms == 1000);

    CHECK(power_policy_low_bat_update(&state, 10, false, 3, 601000, false) == (POWER_POLICY_LOW_BAT_ACTION_WARN | POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP));
    CHECK(state.last_warn_ms == 601000);
}

static void test_low_bat_fast_interval(void) {
    power_policy_low_bat_state_t state;
    power_policy_low_bat_reset(&state);
    power_policy_low_bat_update(&state, 5, false, 3, 0, false);

    CHECK(power_policy_low_bat_update(&state, 5, false, 3, 299999, false) == POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP);
    CHECK(power_policy_low_bat_update(&state, 5, false, 3, 300000, false) == (POWER_POLICY_LOW_BAT_ACTION_WARN | POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP));

    // 6% 走 10 分钟慢速间隔
    CHECK(power_policy_low_bat_update(&state, 6, false, 3, 300000 + 599999, false) == POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP);
    CHECK(power_policy_low_bat_update(&state, 6, false, 3, 300000 + 600000, false) == (POWER_POLICY_LOW_BAT_ACTION_WARN | POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP));
}

static void test_low_bat_busy_suppresses_interval_warning(void) {
    power_policy_low_bat_state_t state;
    power_policy_low_bat_reset(&state);
    power_policy_low_bat_update(&state, 10, false, 3, 1000, false);

    CHECK(power_policy_low_bat_update(&state, 10, false, 3, 700000, true) == POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP);
    CHECK(state.last_warn_ms == 1000);

    CHECK(power_policy_low_bat_update(&state, 10, false, 3, 700000, false) == (POWER_POLICY_LOW_BAT_ACTION_WARN | POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP));
    CHECK(state.last_warn_ms == 700000);
}

static void test_low_bat_hysteresis(void) {
    power_policy_low_bat_state_t state;
    power_policy_low_bat_reset(&state);

    CHECK(power_policy_low_bat_update(&state, 10, false, 2, 0, false) == (POWER_POLICY_LOW_BAT_ACTION_WARN | POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP));
    CHECK(power_policy_low_bat_update(&state, 13, false, 2, 1, false) == POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP);
    CHECK(state.active);

    CHECK(power_policy_low_bat_update(&state, 14, false, 2, 2, false) == POWER_POLICY_LOW_BAT_ACTION_CLEAR_CAP);
    CHECK(!state.active);
    CHECK(!state.warn_pending);

    CHECK(power_policy_low_bat_update(&state, 11, false, 2, 3, false) == 0);
    CHECK(power_policy_low_bat_update(&state, 10, false, 2, 4, false) == (POWER_POLICY_LOW_BAT_ACTION_WARN | POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP));
}

static void test_low_bat_charging_and_unknown(void) {
    power_policy_low_bat_state_t state;
    power_policy_low_bat_reset(&state);
    power_policy_low_bat_update(&state, 10, false, 3, 0, false);
    CHECK(state.active);

    CHECK(power_policy_low_bat_update(&state, 10, true, 3, 1, false) == POWER_POLICY_LOW_BAT_ACTION_CLEAR_CAP);
    CHECK(!state.active);

    power_policy_low_bat_reset(&state);
    CHECK(power_policy_low_bat_update(&state, 0xFF, false, 3, 2, false) == 0);
    CHECK(!state.active);

    CHECK(power_policy_low_bat_update(&state, 0, false, 3, 3, false) == (POWER_POLICY_LOW_BAT_ACTION_WARN | POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP));
}

static void test_low_bat_level_zero_keeps_last_warn(void) {
    power_policy_low_bat_state_t state;
    power_policy_low_bat_reset(&state);
    state.active = true;
    state.warn_pending = true;
    state.last_warn_ms = 1234;

    CHECK(power_policy_low_bat_update(&state, 0, false, 0, 5000, false) == POWER_POLICY_LOW_BAT_ACTION_CLEAR_CAP);
    CHECK(!state.active);
    CHECK(!state.warn_pending);
    CHECK(state.last_warn_ms == 1234);
}

static void test_low_bat_disable_and_reset(void) {
    power_policy_low_bat_state_t state;
    power_policy_low_bat_reset(&state);
    state.active = true;
    state.warn_pending = true;
    state.last_warn_ms = 1234;

    power_policy_low_bat_disable(&state);
    CHECK(!state.active);
    CHECK(!state.warn_pending);
    CHECK(state.last_warn_ms == 1234);

    power_policy_low_bat_reset(&state);
    CHECK(!state.active);
    CHECK(!state.warn_pending);
    CHECK(state.last_warn_ms == 0);
}

static void test_low_bat_time_wrap(void) {
    power_policy_low_bat_state_t state;
    power_policy_low_bat_reset(&state);
    state.active = true;

    state.last_warn_ms = 0xFFFFF000u;
    CHECK(power_policy_low_bat_update(&state, 10, false, 3, 0x00001000u, false) == POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP);
    CHECK(state.last_warn_ms == 0xFFFFF000u);

    state.last_warn_ms = 0xFFFF0000u;
    CHECK(power_policy_low_bat_update(&state, 10, false, 3, 0x00010000u, false) == POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP);

    state.last_warn_ms = 0xFFF00000u;
    CHECK(power_policy_low_bat_update(&state, 10, false, 3, 0x00001000u, false) == (POWER_POLICY_LOW_BAT_ACTION_WARN | POWER_POLICY_LOW_BAT_ACTION_APPLY_CAP));
    CHECK(state.last_warn_ms == 0x00001000u);
}

static void test_charge_toast(void) {
    CHECK(power_policy_charge_toast(true, false, false) == POWER_POLICY_CHARGE_TOAST_CHARGING);
    CHECK(power_policy_charge_toast(true, true, false) == POWER_POLICY_CHARGE_TOAST_FULL);
    CHECK(power_policy_charge_toast(false, true, false) == POWER_POLICY_CHARGE_TOAST_NONE);
    CHECK(power_policy_charge_toast(false, false, false) == POWER_POLICY_CHARGE_TOAST_NONE);
    CHECK(power_policy_charge_toast(true, false, true) == POWER_POLICY_CHARGE_TOAST_NONE);
    CHECK(power_policy_charge_toast(true, true, true) == POWER_POLICY_CHARGE_TOAST_NONE);
}

int main(void) {
    test_dim_off_tables();
    test_low_bat_tables();
    test_cap_value();
    test_should_save();
    test_idle_seconds_clamp();
    test_idle_target();
    test_low_bat_first_warning();
    test_low_bat_busy_pending();
    test_low_bat_slow_interval();
    test_low_bat_fast_interval();
    test_low_bat_busy_suppresses_interval_warning();
    test_low_bat_hysteresis();
    test_low_bat_charging_and_unknown();
    test_low_bat_level_zero_keeps_last_warn();
    test_low_bat_disable_and_reset();
    test_low_bat_time_wrap();
    test_charge_toast();

    if (failures) {
        fprintf(stderr, "%d power policy check(s) failed\n", failures);
        return 1;
    }
    puts("power policy tests passed");
    return 0;
}
