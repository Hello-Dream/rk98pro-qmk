// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/* power_config.c 与 power.c 之间的内部接口，不对外公开。 */
void hs_power_apply_settings(void);
void hs_power_low_bat_refresh_cap(void);
