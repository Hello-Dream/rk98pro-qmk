// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* EE_CLR 计时、USB/挂起/唤醒与设置复位。 */
void hs_session_ee_clr_event(bool pressed);
void hs_session_task(void);
void hs_reset_settings(void);
bool hs_reset_settings_user(void);

void hs_session_usb_connect(void);
void hs_session_usb_disconnect(void);
void hs_session_suspend(void);
void hs_session_wakeup(void);
bool hs_session_allow_timeout(void);
