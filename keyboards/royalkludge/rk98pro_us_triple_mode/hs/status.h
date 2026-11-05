// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include QMK_KEYBOARD_H
#include <stdbool.h>
#include <stdint.h>

#define HS_STATUS_SNAPSHOT_SIZE 23

#define HS_STATUS_FLAG_ONLINE      (1u << 0)
#define HS_STATUS_FLAG_LOW_BATTERY (1u << 1)
#define HS_STATUS_FLAG_USB         (1u << 2)
#define HS_STATUS_FLAG_RGBREC      (1u << 3)
#define HS_STATUS_FLAG_RGB_ON      (1u << 4)
#define HS_STATUS_FLAG_ANTI_BOUNCE (1u << 5)
#define HS_STATUS_FLAG_MAC         (1u << 6)

#define HS_STATUS_LOCK_CAPS      (1u << 0)
#define HS_STATUS_LOCK_NUM       (1u << 1)
#define HS_STATUS_LOCK_SCROLL    (1u << 2)
#define HS_STATUS_LOCK_WIN       (1u << 3)
#define HS_STATUS_LOCK_CAPS_WORD (1u << 4)

typedef struct PACKED {
    uint8_t  version;
    uint8_t  seq;
    uint8_t  fw_major;
    uint8_t  fw_minor;
    uint8_t  fw_patch;
    uint8_t  settings_version;
    uint8_t  battery;
    uint8_t  charging;
    uint8_t  link;
    uint8_t  mode;
    uint8_t  power_state;
    uint8_t  flags;
    uint8_t  locks;
    uint8_t  layer;
    uint8_t  default_layer;
    uint8_t  wpm;
    uint16_t idle_seconds;
    uint8_t  rgbrec_channel;
    uint8_t  rgb_mode;
    uint8_t  rgb_brightness;
    uint8_t  wireless_version;
    uint8_t  reserved;
} hs_status_snapshot_t;

_Static_assert(sizeof(hs_status_snapshot_t) == HS_STATUS_SNAPSHOT_SIZE, "status snapshot must be 23 bytes");

void hs_status_task(void);
bool hs_status_get(uint8_t *value_data, uint8_t value_len);
