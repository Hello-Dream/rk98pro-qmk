// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "battery_policy.h"

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

static void test_filter_dampens_jitter(void) {
    battery_filter_t filter;
    battery_filter_reset(&filter);

    CHECK(battery_filter_percent(&filter) == 0xFF);
    battery_filter_update(&filter, 80, 3, 3);
    battery_filter_update(&filter, 82, 3, 3);
    CHECK(battery_filter_percent(&filter) == 0xFF);
    battery_filter_update(&filter, 79, 3, 3);
    CHECK(battery_filter_percent(&filter) == 80);

    battery_filter_update(&filter, 81, 3, 3);
    CHECK(battery_filter_percent(&filter) == 80);

    battery_filter_update(&filter, 100, 3, 3);
    CHECK(battery_filter_percent(&filter) == 80);
}

static void test_unknown_value_resets_filter(void) {
    battery_filter_t filter;
    battery_filter_reset(&filter);
    battery_filter_update(&filter, 80, 3, 3);
    battery_filter_update(&filter, 80, 3, 3);
    battery_filter_update(&filter, 80, 3, 3);
    CHECK(battery_filter_percent(&filter) == 80);
    CHECK(filter.sample_count == 3);

    battery_filter_update(&filter, 0xFF, 3, 3);
    CHECK(battery_filter_percent(&filter) == 0xFF);
    CHECK(filter.sample_count == 0);
    CHECK(filter.low_confirm == 0);
}

static void test_low_battery_confirmation(void) {
    battery_filter_t filter;
    battery_filter_reset(&filter);

    battery_filter_update(&filter, 3, 3, 3);
    battery_filter_update(&filter, 3, 3, 3);
    CHECK(!battery_low_confirmed(&filter, 3));
    battery_filter_update(&filter, 3, 3, 3);
    CHECK(battery_low_confirmed(&filter, 3));

    battery_filter_update(&filter, 4, 3, 3);
    CHECK(!battery_low_confirmed(&filter, 3));
}

static void test_full_latch_requires_stable_signal(void) {
    battery_full_t full;
    battery_full_reset(&full);

    battery_full_update(&full, true, true, 100, 1000, 98, 95, 3000);
    CHECK(!battery_full_latched(&full));
    battery_full_update(&full, true, true, 100, 3999, 98, 95, 3000);
    CHECK(!battery_full_latched(&full));
    battery_full_update(&full, true, true, 100, 4000, 98, 95, 3000);
    CHECK(battery_full_latched(&full));

    battery_full_update(&full, true, false, 100, 4100, 98, 95, 3000);
    CHECK(battery_full_latched(&full));
    battery_full_update(&full, true, true, 95, 4200, 98, 95, 3000);
    CHECK(battery_full_latched(&full));
    battery_full_update(&full, true, true, 94, 4300, 98, 95, 3000);
    CHECK(!battery_full_latched(&full));
}

static void test_full_requires_minimum_percent(void) {
    battery_full_t full;
    battery_full_reset(&full);

    battery_full_update(&full, true, true, 97, 1000, 98, 95, 3000);
    battery_full_update(&full, true, true, 97, 4000, 98, 95, 3000);
    CHECK(!battery_full_latched(&full));

    battery_full_update(&full, true, true, 98, 5000, 98, 95, 3000);
    battery_full_update(&full, true, true, 98, 8000, 98, 95, 3000);
    CHECK(battery_full_latched(&full));
}

static void test_unplug_clears_latch(void) {
    battery_full_t full;
    battery_full_reset(&full);

    battery_full_update(&full, true, true, 100, 1000, 98, 95, 3000);
    battery_full_update(&full, true, true, 100, 4000, 98, 95, 3000);
    CHECK(battery_full_latched(&full));

    battery_full_update(&full, false, false, 100, 4100, 98, 95, 3000);
    CHECK(!battery_full_latched(&full));
}

int main(void) {
    test_filter_dampens_jitter();
    test_unknown_value_resets_filter();
    test_low_battery_confirmation();
    test_full_latch_requires_stable_signal();
    test_full_requires_minimum_percent();
    test_unplug_clears_latch();

    if (failures) {
        fprintf(stderr, "%d battery policy check(s) failed\n", failures);
        return 1;
    }
    puts("battery policy tests passed");
    return 0;
}
