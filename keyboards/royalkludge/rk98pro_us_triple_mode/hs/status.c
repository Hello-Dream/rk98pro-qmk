// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "status.h"

#include "hs/wireless_glue.h"
#include "hs/battery.h"
#include "rgb_record/rgb_record.h"

#include "raw_hid.h"
#include "timer.h"

#ifdef WIRELESS_ENABLE
#    include "wireless.h"
#endif

#ifdef RGB_MATRIX_ENABLE
#    include "rgb_matrix.h"
#endif

#include <string.h>

#define HS_STATUS_EVENT_MAGIC       0xE0
#define HS_STATUS_EVENT_STATUS      0x01
#define HS_STATUS_SESSION_MS        60000
#define HS_STATUS_BATTERY_MS        60000
#define HS_STATUS_GET_ACTIVE_MS     2000
#define HS_STATUS_CHECK_ACTIVE_MS   250
#define HS_STATUS_CHECK_IDLE_MS     1000

extern uint8_t  hs_power_state_code(void);
extern bool     hs_power_low_battery_active(void);
extern uint16_t hs_power_idle_seconds(void);
extern bool     charging_state;

static uint8_t  hs_status_seq;
static uint32_t hs_status_session_at;
static uint32_t hs_status_last_get_at;
static uint32_t hs_status_last_check;
static uint32_t hs_status_last_battery_event;
static bool     hs_status_sent_valid;
static hs_status_snapshot_t hs_status_last_sent;

static void hs_status_build_snapshot(hs_status_snapshot_t *snapshot) {
    memset(snapshot, 0, sizeof(*snapshot));

    uint8_t battery = hs_battery_percent();
    snapshot->version          = HS_STATUS_SNAPSHOT_VERSION;
    snapshot->fw_major         = HS_FIRMWARE_VERSION_MAJOR;
    snapshot->fw_minor         = HS_FIRMWARE_VERSION_MINOR;
    snapshot->fw_patch         = HS_FIRMWARE_VERSION_PATCH;
    snapshot->settings_version = HS_SETTINGS_VERSION;
    snapshot->battery          = battery > 100 ? 0xFF : battery;
    snapshot->charging         = charging_state ? (hs_battery_is_full() ? 2 : 1) : 0;
    snapshot->power_state      = hs_power_state_code();
    snapshot->layer            = get_highest_layer(layer_state);
    snapshot->default_layer    = get_highest_layer(default_layer_state);
    snapshot->idle_seconds     = hs_power_idle_seconds();

#ifdef WIRELESS_ENABLE
    snapshot->link  = *md_getp_state();
    snapshot->mode  = wireless_get_current_devs();
    snapshot->wireless_version = md_get_version();
    if (snapshot->mode == DEVS_USB || snapshot->link == MD_STATE_CONNECTED) {
        snapshot->flags |= HS_STATUS_FLAG_ONLINE;
    }
    if (snapshot->mode == DEVS_USB) {
        snapshot->flags |= HS_STATUS_FLAG_USB;
    }
#else
    snapshot->link = 0xFF;
    snapshot->mode = 0xFF;
#endif

    if (hs_power_low_battery_active()) {
        snapshot->flags |= HS_STATUS_FLAG_LOW_BATTERY;
    }
    if (rgbrec_is_started()) {
        snapshot->flags |= HS_STATUS_FLAG_RGBREC;
    }
    if (mm_get_rgb_enable()) {
        snapshot->flags |= HS_STATUS_FLAG_RGB_ON;
    }
    if (hs_get_anti_bounce()) {
        snapshot->flags |= HS_STATUS_FLAG_ANTI_BOUNCE;
    }
    if (hs_keymap_is_mac()) {
        snapshot->flags |= HS_STATUS_FLAG_MAC;
    }

    if (host_keyboard_led_state().caps_lock) {
        snapshot->locks |= HS_STATUS_LOCK_CAPS;
    }
    if (host_keyboard_led_state().num_lock) {
        snapshot->locks |= HS_STATUS_LOCK_NUM;
    }
    if (host_keyboard_led_state().scroll_lock) {
        snapshot->locks |= HS_STATUS_LOCK_SCROLL;
    }
    if (!hs_keymap_is_mac() && keymap_config.no_gui) {
        snapshot->locks |= HS_STATUS_LOCK_WIN;
    }
#ifdef CAPS_WORD_ENABLE
    if (is_caps_word_on()) {
        snapshot->locks |= HS_STATUS_LOCK_CAPS_WORD;
    }
#endif

#ifdef WPM_ENABLE
    snapshot->wpm = get_current_wpm();
#endif

    snapshot->rgbrec_channel = rgbrec_is_started() ? hs_get_rgbrec_channel() : 0xFF;

#ifdef RGB_MATRIX_ENABLE
    snapshot->rgb_mode       = rgb_matrix_get_mode();
    snapshot->rgb_brightness = rgb_matrix_get_val();
#endif
}

static bool hs_status_non_battery_changed(const hs_status_snapshot_t *left, const hs_status_snapshot_t *right) {
    return left->charging != right->charging || left->link != right->link || left->mode != right->mode || left->power_state != right->power_state || left->flags != right->flags || left->locks != right->locks || left->layer != right->layer || left->default_layer != right->default_layer || left->rgbrec_channel != right->rgbrec_channel || left->rgb_mode != right->rgb_mode || left->rgb_brightness != right->rgb_brightness;
}

static void hs_status_send_event(const hs_status_snapshot_t *snapshot) {
    uint8_t report[32] = {0};
    report[0]          = HS_STATUS_EVENT_MAGIC;
    report[1]          = HS_STATUS_EVENT_STATUS;
    report[2]          = HS_STATUS_SNAPSHOT_SIZE;
    memcpy(&report[3], snapshot, sizeof(*snapshot));
    raw_hid_send(report, sizeof(report));
}

bool hs_status_get(uint8_t *value_data, uint8_t value_len) {
    if (value_data == NULL || value_len < HS_STATUS_SNAPSHOT_SIZE) {
        return false;
    }

    hs_status_session_at = timer_read32();
    hs_status_last_get_at = hs_status_session_at;

    hs_status_snapshot_t snapshot;
    hs_status_build_snapshot(&snapshot);
    snapshot.seq = hs_status_seq;

    memset(value_data, 0, value_len);
    memcpy(value_data, &snapshot, sizeof(snapshot));
    return true;
}

void hs_status_task(void) {
    uint32_t now = timer_read32();

    if (timer_elapsed32(hs_status_session_at) >= HS_STATUS_SESSION_MS) {
        return;
    }

    // 最近 2 秒有 GET 说明上位机聚焦（约 1 秒轮询），用 250ms 保持界面灵敏；
    // 失焦或托盘只靠 30 秒心跳续期，降到 1 秒即可，避免无线主循环持续构建快照。
    bool     host_active = hs_status_last_get_at && timer_elapsed32(hs_status_last_get_at) < HS_STATUS_GET_ACTIVE_MS;
    uint32_t check_ms    = host_active ? HS_STATUS_CHECK_ACTIVE_MS : HS_STATUS_CHECK_IDLE_MS;
    if (hs_status_last_check && timer_elapsed32(hs_status_last_check) < check_ms) {
        return;
    }
    hs_status_last_check = now;

    hs_status_snapshot_t snapshot;
    hs_status_build_snapshot(&snapshot);

    if (!hs_status_sent_valid) {
        hs_status_last_sent  = snapshot;
        hs_status_sent_valid = true;
        return;
    }

    bool non_battery_changed = hs_status_non_battery_changed(&snapshot, &hs_status_last_sent);
    bool battery_changed     = snapshot.battery != hs_status_last_sent.battery;
    bool battery_due         = battery_changed && (!hs_status_last_battery_event || timer_elapsed32(hs_status_last_battery_event) >= HS_STATUS_BATTERY_MS);

    if (!non_battery_changed && !battery_due) {
        return;
    }

    snapshot.seq = ++hs_status_seq;
    hs_status_last_sent = snapshot;
    if (battery_changed) {
        hs_status_last_battery_event = now;
    }
    hs_status_send_event(&snapshot);
}
