// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include QMK_KEYBOARD_H
#include <stdbool.h>
#include <stdint.h>

/* 当前存储布局：显式字段 + magic + 版本 + CRC。
 * 字段新增只能追加，布局变化必须递增 HS_SETTINGS_VERSION。 */

#define HS_STORE_MAGIC       0x4B52u /* 'R' 'K' */

#define HS_SETTINGS_FIELDS_SIZE 24
#define HS_POWER_FIELDS_SIZE    6
#define HS_RGBREC_BLOB_SIZE     (HS_RGBREC_HEADER_SIZE + HS_RGBREC_DATA_SIZE)

typedef struct PACKED {
    uint8_t rgb_enabled;
    uint8_t side_light_mode;
    uint8_t sleep_timeout;
    uint8_t anti_bounce;
    uint8_t wireless_mode;
    uint8_t last_bt_channel;
    uint8_t rgbrec_channel;
    uint8_t last_rgb_mode;
    uint8_t rgbrec_hsv[16];
} hs_settings_t;

typedef struct PACKED {
    uint8_t idle_dim;
    uint8_t idle_off;
    uint8_t low_bat_level;
    uint8_t low_bat_brightness;
    uint8_t usb_save;
    uint8_t status_led;
} hs_power_t;

typedef struct PACKED {
    uint16_t magic;
    uint8_t  version;
    uint8_t  size;
    uint16_t crc;
    uint16_t reserved;
    hs_settings_t settings;
    hs_power_t    power;
} hs_settings_blob_t;

typedef struct PACKED {
    uint16_t magic;
    uint8_t  version;
    uint8_t  reserved;
    uint16_t crc;
    uint16_t reserved2;
    uint8_t  data[HS_RGBREC_DATA_SIZE];
} hs_rgbrec_blob_t;

_Static_assert(sizeof(hs_settings_t) == HS_SETTINGS_FIELDS_SIZE, "hs_settings_t layout changed");
_Static_assert(sizeof(hs_power_t) == HS_POWER_FIELDS_SIZE, "hs_power_t layout changed");
_Static_assert(sizeof(hs_settings_blob_t) == 38, "hs_settings_blob_t must be 38 bytes");
_Static_assert(HS_SETTINGS_BLOB_SIZE >= sizeof(hs_settings_blob_t), "settings blob size too small");
_Static_assert(sizeof(hs_rgbrec_blob_t) == HS_RGBREC_BLOB_SIZE, "hs_rgbrec_blob_t layout changed");
_Static_assert((HS_SETTINGS_BLOB_SIZE % 4) == 0, "settings blob must stay 4-byte aligned");

extern hs_settings_t hs_settings;
extern hs_power_t    hs_power;

uint16_t hs_crc16(const void *data, uint16_t length);
uint16_t hs_crc16_update(uint16_t crc, const void *data, uint16_t length);

void hs_store_init(void);
void hs_store_reset(void);
bool hs_store_reset_occurred(void);

void hs_store_settings_touch(void);
void hs_store_settings_flush(void);
void hs_store_flush(void);
void hs_store_task(void);

uint8_t hs_store_hsv_get(uint8_t index);
void    hs_store_hsv_set(uint8_t index, uint8_t value);

uint16_t hs_store_rgb_size(void);
uint16_t hs_store_rgb_crc(void);
void     hs_store_rgb_read(uint16_t offset, uint16_t size, uint8_t *data);
void     hs_store_rgb_write(uint16_t offset, uint16_t size, const uint8_t *data);
void     hs_store_rgb_commit(void);
void     hs_store_rgb_defaults(void);
