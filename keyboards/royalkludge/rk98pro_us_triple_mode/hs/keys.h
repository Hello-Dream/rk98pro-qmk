// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

void hs_keys_init(void);
bool hs_get_anti_bounce(void);
void hs_set_anti_bounce(bool enabled);
