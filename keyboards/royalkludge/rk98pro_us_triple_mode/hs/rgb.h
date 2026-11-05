// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

extern uint8_t hs_rl_mode;
extern uint8_t buff[11];

void hs_settings_apply_boot(void);
void im_rgblight_increase(void);
void hs_rgb_select_effect(uint8_t mode);
bool hs_record_last_mode_update(void);
void hs_rgbrec_switch_channel(uint8_t channel);
