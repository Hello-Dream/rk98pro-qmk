// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>

void hs_settings_update(void);
bool mm_get_rgb_enable(void);
void mm_set_rgb_enable(bool state);
bool hs_keymap_is_mac(void);
void hs_settings_init(void);
void hs_settings_reset(void);
