// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

void hs_runtime_task(void);
bool hs_runtime_indicators(uint8_t led_min, uint8_t led_max);
