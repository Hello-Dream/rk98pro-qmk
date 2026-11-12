// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include QMK_KEYBOARD_H
#include <stdbool.h>
#include <stdint.h>

void hs_key_handlers_init(void);
bool hs_key_handlers_process(uint16_t keycode, keyrecord_t *record);
