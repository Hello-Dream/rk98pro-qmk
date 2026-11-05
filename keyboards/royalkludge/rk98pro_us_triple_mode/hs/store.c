// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "hs/store.h"

#include "eeprom.h"
#include "nvm_eeprom_eeconfig_internal.h"
#include "rgb_record/rgb_record.h"

#ifdef DYNAMIC_KEYMAP_ENABLE
#    include "dynamic_keymap.h"
#endif

#include <string.h>

#define HS_SETTINGS_EECONFIG_ADDR (EECONFIG_USER_DATABLOCK)
#define HS_RGBREC_EECONFIG_ADDR   (EECONFIG_USER_DATABLOCK + HS_SETTINGS_BLOB_SIZE)
#define HS_SETTINGS_SAVE_DELAY_MS 500

hs_settings_t hs_settings;
hs_power_t    hs_power;

static hs_rgbrec_blob_t hs_rgb_blob;
static bool             hs_settings_dirty;
static bool             hs_rgb_dirty;
static uint32_t         hs_settings_dirty_at;
static bool             hs_store_reset_flag;

/* 4 字节粒度的 dirty 标记：RGB 通道保存时只写真正变化的块。 */
#define HS_RGB_CHUNK 4
#define HS_RGB_CHUNKS (HS_RGBREC_DATA_SIZE / HS_RGB_CHUNK)
static uint8_t hs_rgb_chunk_dirty[(HS_RGB_CHUNKS + 7) / 8];

uint16_t hs_crc16_update(uint16_t crc, const void *data, uint16_t length) {
    const uint8_t *bytes = (const uint8_t *)data;

    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)bytes[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }

    return crc;
}

uint16_t hs_crc16(const void *data, uint16_t length) {
    return hs_crc16_update(0xFFFF, data, length);
}

static void hs_rgb_dirty_clear(void) {
    memset(hs_rgb_chunk_dirty, 0, sizeof(hs_rgb_chunk_dirty));
    hs_rgb_dirty = false;
}

static void hs_rgb_mark_dirty(uint16_t offset, uint16_t size) {
    if (size == 0 || offset >= HS_RGBREC_DATA_SIZE) {
        return;
    }
    if ((uint32_t)offset + size > HS_RGBREC_DATA_SIZE) {
        size = (uint16_t)(HS_RGBREC_DATA_SIZE - offset);
    }

    uint16_t first = offset / HS_RGB_CHUNK;
    uint16_t last  = (uint16_t)((offset + size - 1) / HS_RGB_CHUNK);
    for (uint16_t chunk = first; chunk <= last; chunk++) {
        hs_rgb_chunk_dirty[chunk >> 3] |= (uint8_t)(1u << (chunk & 7));
    }
    hs_rgb_dirty = true;
}

static void hs_settings_defaults(void) {
    memset(&hs_settings, 0, sizeof(hs_settings));
    hs_settings.rgbrec_channel  = 0;
    hs_settings.last_rgb_mode   = 0xFF;
    hs_settings.last_bt_channel = 1;
    hs_settings.side_light_mode = 0;
    hs_settings.rgb_enabled     = true;
    hs_settings.sleep_timeout   = 7;
    hs_settings.anti_bounce     = true;

    memset(&hs_power, 0, sizeof(hs_power));
    hs_power.idle_dim           = 2;
    hs_power.idle_off           = 2;
    hs_power.low_bat_level      = 3;
    hs_power.low_bat_brightness = 1;
    hs_power.usb_save           = 1;
    hs_power.status_led         = 1;
}

void hs_store_settings_flush(void) {
    hs_settings_blob_t blob;
    memset(&blob, 0, sizeof(blob));
    blob.magic    = HS_STORE_MAGIC;
    blob.version  = HS_SETTINGS_VERSION;
    blob.size     = sizeof(blob);
    blob.settings = hs_settings;
    blob.power    = hs_power;
    blob.crc      = hs_crc16(&blob.settings, sizeof(blob.settings) + sizeof(blob.power));

    uint8_t raw[HS_SETTINGS_BLOB_SIZE] = {0};
    memcpy(raw, &blob, sizeof(blob));
    eeprom_update_block(raw, (void *)HS_SETTINGS_EECONFIG_ADDR, sizeof(raw));

    hs_settings_dirty    = false;
    hs_settings_dirty_at = 0;
}

void hs_store_settings_touch(void) {
    hs_settings_dirty    = true;
    hs_settings_dirty_at = timer_read32();
}

void hs_store_rgb_commit(void) {
    if (!hs_rgb_dirty) {
        return;
    }

    hs_rgb_blob.magic    = HS_STORE_MAGIC;
    hs_rgb_blob.version  = HS_SETTINGS_VERSION;
    hs_rgb_blob.reserved = 0;
    hs_rgb_blob.crc      = hs_crc16(hs_rgb_blob.data, sizeof(hs_rgb_blob.data));

    /* 头部单独更新，数据只写 dirty 的 4 字节块。 */
    eeprom_update_block(&hs_rgb_blob, (void *)HS_RGBREC_EECONFIG_ADDR, HS_RGBREC_HEADER_SIZE);
    for (uint16_t chunk = 0; chunk < HS_RGB_CHUNKS; chunk++) {
        if (hs_rgb_chunk_dirty[chunk >> 3] & (uint8_t)(1u << (chunk & 7))) {
            uint16_t offset = (uint16_t)(chunk * HS_RGB_CHUNK);
            eeprom_update_block(&hs_rgb_blob.data[offset], (void *)(HS_RGBREC_EECONFIG_ADDR + HS_RGBREC_HEADER_SIZE + offset), HS_RGB_CHUNK);
        }
    }

    hs_rgb_dirty_clear();
}

void hs_store_flush(void) {
    if (hs_settings_dirty) {
        hs_store_settings_flush();
    }
    if (hs_rgb_dirty) {
        hs_store_rgb_commit();
    }
}

void hs_store_task(void) {
    if (hs_settings_dirty && timer_elapsed32(hs_settings_dirty_at) >= HS_SETTINGS_SAVE_DELAY_MS) {
        hs_store_settings_flush();
    }
}

bool hs_store_reset_occurred(void) {
    return hs_store_reset_flag;
}

void hs_store_rgb_defaults(void) {
    extern const uint16_t PROGMEM rgbrec_default_effects[RGBREC_CHANNEL_NUM][MATRIX_ROWS][MATRIX_COLS];
    memcpy(hs_rgb_blob.data, rgbrec_default_effects, sizeof(hs_rgb_blob.data));
    memset(hs_rgb_chunk_dirty, 0xFF, sizeof(hs_rgb_chunk_dirty));
    hs_rgb_dirty = true;
}

void hs_store_reset(void) {
    hs_settings_defaults();
    hs_store_settings_flush();
    hs_store_rgb_defaults();
    hs_store_rgb_commit();
#ifdef DYNAMIC_KEYMAP_ENABLE
    dynamic_keymap_reset();
#endif
    hs_store_reset_flag = true;
}

void hs_store_init(void) {
    bool settings_ok = false;
    bool rgb_ok      = false;

    uint8_t raw[HS_SETTINGS_BLOB_SIZE] = {0};
    eeprom_read_block(raw, (void *)HS_SETTINGS_EECONFIG_ADDR, sizeof(raw));

    hs_settings_blob_t blob;
    memcpy(&blob, raw, sizeof(blob));
    if (blob.magic == HS_STORE_MAGIC && blob.version == HS_SETTINGS_VERSION && blob.size == sizeof(hs_settings_blob_t) && blob.crc == hs_crc16(&blob.settings, sizeof(blob.settings) + sizeof(blob.power))) {
        hs_settings = blob.settings;
        hs_power    = blob.power;
        settings_ok = true;
    }

    eeprom_read_block(&hs_rgb_blob, (void *)HS_RGBREC_EECONFIG_ADDR, HS_RGBREC_HEADER_SIZE);
    if (hs_rgb_blob.magic == HS_STORE_MAGIC && hs_rgb_blob.version == HS_SETTINGS_VERSION) {
        eeprom_read_block(hs_rgb_blob.data, (void *)(HS_RGBREC_EECONFIG_ADDR + HS_RGBREC_HEADER_SIZE), sizeof(hs_rgb_blob.data));
        if (hs_rgb_blob.crc == hs_crc16(hs_rgb_blob.data, sizeof(hs_rgb_blob.data))) {
            rgb_ok = true;
        }
    }

    hs_settings_dirty = false;
    hs_rgb_dirty_clear();

    if (!settings_ok || !rgb_ok) {
        hs_store_reset_flag = true;
        if (!settings_ok) {
            hs_settings_defaults();
            hs_store_settings_flush();
        }
        if (!rgb_ok) {
            hs_store_rgb_defaults();
            hs_store_rgb_commit();
        }
#ifdef DYNAMIC_KEYMAP_ENABLE
        dynamic_keymap_reset();
#endif
    }
}

uint8_t hs_store_hsv_get(uint8_t index) {
    if (index >= sizeof(hs_settings.rgbrec_hsv)) {
        return 0;
    }
    return hs_settings.rgbrec_hsv[index];
}

void hs_store_hsv_set(uint8_t index, uint8_t value) {
    if (index >= sizeof(hs_settings.rgbrec_hsv)) {
        return;
    }
    if (hs_settings.rgbrec_hsv[index] != value) {
        hs_settings.rgbrec_hsv[index] = value;
        hs_store_settings_touch();
    }
}

uint16_t hs_store_rgb_size(void) {
    return sizeof(hs_rgb_blob.data);
}

uint16_t hs_store_rgb_crc(void) {
    return hs_crc16(hs_rgb_blob.data, sizeof(hs_rgb_blob.data));
}

void hs_store_rgb_read(uint16_t offset, uint16_t size, uint8_t *data) {
    if (offset >= sizeof(hs_rgb_blob.data)) {
        memset(data, 0, size);
        return;
    }
    if ((uint32_t)offset + size > sizeof(hs_rgb_blob.data)) {
        size = (uint16_t)(sizeof(hs_rgb_blob.data) - offset);
    }
    memcpy(data, &hs_rgb_blob.data[offset], size);
}

void hs_store_rgb_write(uint16_t offset, uint16_t size, const uint8_t *data) {
    if (offset >= sizeof(hs_rgb_blob.data)) {
        return;
    }
    if ((uint32_t)offset + size > sizeof(hs_rgb_blob.data)) {
        size = (uint16_t)(sizeof(hs_rgb_blob.data) - offset);
    }
    if (memcmp(&hs_rgb_blob.data[offset], data, size) != 0) {
        memcpy(&hs_rgb_blob.data[offset], data, size);
        hs_rgb_mark_dirty(offset, size);
    }
}
