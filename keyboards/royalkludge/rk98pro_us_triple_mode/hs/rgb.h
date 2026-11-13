// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

void hs_rgb_side_light_mode_set_boot(uint8_t mode);
uint8_t hs_rgb_side_light_mode_count(void);
uint8_t hs_rgb_side_light_default_mode(void);
bool hs_rgb_record_flag(void);
void hs_rgb_record_flag_set(bool value);

void hs_settings_apply_boot(void);
void im_rgblight_increase(void);
void hs_rgb_select_effect(uint8_t mode);
bool hs_record_last_mode_update(void);
void hs_rgbrec_switch_channel(uint8_t channel);
