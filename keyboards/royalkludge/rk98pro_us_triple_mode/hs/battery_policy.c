// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "battery_policy.h"

static uint8_t battery_median(const uint8_t *values, uint8_t count) {
    uint8_t sorted[BATTERY_POLICY_FILTER_SIZE];
    for (uint8_t i = 0; i < count; i++) {
        sorted[i] = values[i];
    }
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

void battery_filter_reset(battery_filter_t *filter) {
    filter->sample_count = 0;
    filter->sample_index = 0;
    filter->filtered     = 0xFF;
    filter->low_confirm  = 0;
}

void battery_filter_update(battery_filter_t *filter, uint8_t raw, uint8_t stop_threshold, uint8_t low_confirm_target) {
    if (raw > 100) {
        battery_filter_reset(filter);
        return;
    }

    if (filter->sample_count < BATTERY_POLICY_FILTER_SIZE) {
        filter->samples[filter->sample_count++] = raw;
    } else {
        filter->samples[filter->sample_index] = raw;
        filter->sample_index = (uint8_t)((filter->sample_index + 1) % BATTERY_POLICY_FILTER_SIZE);
    }

    if (filter->sample_count >= 3) {
        uint8_t median = battery_median(filter->samples, filter->sample_count);
        if (filter->filtered == 0xFF) {
            filter->filtered = median;
        } else {
            filter->filtered = (uint8_t)(((uint16_t)filter->filtered * 3 + median + 2) / 4);
        }
    }

    if (raw <= stop_threshold) {
        if (filter->low_confirm < low_confirm_target) {
            filter->low_confirm++;
        }
    } else {
        filter->low_confirm = 0;
    }
}

uint8_t battery_filter_percent(const battery_filter_t *filter) {
    return filter->filtered;
}

bool battery_low_confirmed(const battery_filter_t *filter, uint8_t low_confirm_target) {
    return filter->low_confirm >= low_confirm_target;
}

void battery_full_reset(battery_full_t *full) {
    full->latched       = false;
    full->full_since_ms = 0;
}

void battery_full_update(battery_full_t *full, bool charging, bool full_signal, uint8_t filtered, uint32_t now_ms, uint8_t full_min, uint8_t full_hysteresis, uint32_t stable_ms) {
    if (!charging) {
        full->latched       = false;
        full->full_since_ms = 0;
    } else if (!full->latched) {
        if (full_signal && filtered != 0xFF && filtered >= full_min) {
            if (!full->full_since_ms) {
                full->full_since_ms = now_ms;
            } else if ((uint32_t)(now_ms - full->full_since_ms) >= stable_ms) {
                full->latched = true;
            }
        } else {
            full->full_since_ms = 0;
        }
    } else if (filtered == 0xFF || filtered < full_hysteresis) {
        full->latched = false;
    }
}

bool battery_full_latched(const battery_full_t *full) {
    return full->latched;
}
