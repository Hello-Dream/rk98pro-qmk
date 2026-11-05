// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "via.h"

#include "hs/wireless_glue.h"
#include "hs/status.h"
#include "hs/store.h"
#include "hs/battery.h"

static void via_label_write(uint8_t *value_data, uint8_t length, const char *text) {
    uint8_t max = (length > 3) ? (uint8_t)(length - 3) : 0;
    uint8_t i   = 0;

    if (max == 0) {
        return;
    }
    while ((i + 1) < max && text[i] != '\0') {
        value_data[i] = (uint8_t)text[i];
        i++;
    }
    value_data[i] = '\0';
}

static void via_battery_label(uint8_t *value_data, uint8_t length) {
    char    text[16];
    uint8_t pos = 0;

    if (charging_state && bat_full_flag) {
        const char *prefix = "Full ";
        while (*prefix) text[pos++] = *prefix++;
    } else if (charging_state) {
        const char *prefix = "Charging ";
        while (*prefix) text[pos++] = *prefix++;
    }

    uint8_t bat = hs_battery_percent();
    if (bat > 100) {
        via_label_write(value_data, length, "Unknown");
        return;
    }
    if (bat >= 100) {
        text[pos++] = '1';
        text[pos++] = '0';
        text[pos++] = '0';
    } else if (bat >= 10) {
        text[pos++] = (char)('0' + bat / 10);
        text[pos++] = (char)('0' + bat % 10);
    } else {
        text[pos++] = (char)('0' + bat);
    }
    text[pos++] = '%';
    text[pos]   = '\0';

    via_label_write(value_data, length, text);
}

static void via_connection_label(uint8_t *value_data, uint8_t length) {
    const char *devs;
    switch (wireless_get_current_devs()) {
        case DEVS_BT1: devs = "BT1"; break;
        case DEVS_BT2: devs = "BT2"; break;
        case DEVS_BT3: devs = "BT3"; break;
        case DEVS_2G4: devs = "2.4G"; break;
        case DEVS_USB: devs = "USB"; break;
        default: devs = "None"; break;
    }

    const char *state;
    switch (*md_getp_state()) {
        case MD_STATE_CONNECTED: state = "Connected"; break;
        case MD_STATE_PAIRING: state = "Pairing"; break;
        case MD_STATE_DISCONNECTED: state = "Disconnected"; break;
        default: state = "Unknown"; break;
    }

    char    text[24];
    uint8_t pos = 0;
    while (*devs && pos < (sizeof(text) - 2)) text[pos++] = *devs++;
    text[pos++] = ' ';
    while (*state && pos < (sizeof(text) - 1)) text[pos++] = *state++;
    text[pos] = '\0';

    via_label_write(value_data, length, text);
}

void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    // data = [ command_id, channel_id, value_id, value_data ]
    if (length < 4) {
        return;
    }

    uint8_t *command_id = &(data[0]);
    uint8_t *channel_id = &(data[1]);
    uint8_t *value_id   = &(data[2]);
    uint8_t *value_data = &(data[3]);

    if (*channel_id != 0) {
        *command_id = id_unhandled;
        return;
    }

    switch (*command_id) {
        case id_custom_get_value: {
            switch (*value_id) {
                case id_sleep_timeout: value_data[0] = hs_get_sleep_timeout(); break;
                case id_side_light_mode: value_data[0] = hs_get_side_light_mode(); break;
                case id_anti_bounce: value_data[0] = hs_get_anti_bounce() ? 1 : 0; break;
                case id_rgbrec_channel: value_data[0] = (uint8_t)RGBREC_CHANNEL_NUM; break;
                case id_rgbrec_active_channel: value_data[0] = hs_get_rgbrec_channel(); break;
                case id_rgbrec_record:
                case id_rgbrec_clear:
                case id_rgbrec_copy:
                case id_battery_check: value_data[0] = 0; break;
                case id_power_idle_dim:
                case id_power_idle_off:
                case id_power_usb_save:
                case id_power_low_bat_warn:
                case id_power_low_bat_brightness:
                case id_power_status_led: value_data[0] = hs_power_get_config(*value_id); break;
                case id_power_state: via_label_write(value_data, length, hs_power_state_name()); break;
                case id_battery_level: via_battery_label(value_data, length); break;
                case id_connection_status: via_connection_label(value_data, length); break;
                case id_host_status:
                    if (!hs_status_get(value_data, (uint8_t)(length - 3))) {
                        *command_id = id_unhandled;
                    }
                    break;
                default: *command_id = id_unhandled; break;
            }
        } break;
        case id_custom_set_value: {
            switch (*value_id) {
                case id_sleep_timeout:
                    if (value_data[0] <= hs_sleep_timeout_vendor) {
                        hs_set_sleep_timeout(value_data[0]);
                        hs_settings_update();
                    }
                    break;
                case id_side_light_mode: hs_set_side_light_mode(value_data[0]); break;
                case id_anti_bounce: hs_set_anti_bounce(value_data[0] != 0); break;
                case id_rgbrec_active_channel: hs_set_rgbrec_channel(value_data[0]); break;
                case id_rgbrec_record:
                    if (value_data[0]) hs_rgbrec_toggle_record();
                    break;
                case id_rgbrec_clear:
                    if (value_data[0]) hs_rgbrec_clear_current();
                    break;
                case id_rgbrec_copy:
                    if (value_data[0]) hs_rgbrec_copy_next();
                    break;
                case id_battery_check:
                    if (value_data[0]) hs_bat_show();
                    break;
                case id_power_idle_dim:
                case id_power_idle_off:
                case id_power_usb_save:
                case id_power_low_bat_warn:
                case id_power_low_bat_brightness:
                case id_power_status_led: hs_power_set_config(*value_id, value_data[0]); break;
                case id_power_state: break;
                case id_battery_level:
                case id_connection_status: break;
                default: *command_id = id_unhandled; break;
            }
        } break;
        case id_custom_save: {
            hs_store_flush();
        } break;
        default: {
            *command_id = id_unhandled;
        } break;
    }
}

bool via_command_kb(uint8_t *data, uint8_t length) {
    if (length < 1) {
        return false;
    }

    uint8_t *command_id = &(data[0]);

    // 只有 VIA 写操作才视为用户活动并重置 blink 计时器；
    // 状态 GET 必须无副作用，不能阻止键盘休眠。
    if (*command_id == id_custom_set_value || *command_id == id_custom_save) {
        hs_power_activity();
        hs_rgb_blink_set_timer(timer_read32());
    }

    // 用户在 VIA 中主动打开灯效时，取消状态灯的临时点亮标记
    if (length >= 4 && *command_id == id_custom_set_value && data[1] == id_qmk_rgb_matrix_channel && data[2] == id_qmk_rgb_matrix_effect && data[3] != 0) {
        hs_power_rgb_user_enabled();
    }

    // if (hs_via_custom_value_command_kb(data, length) != false){
    //     replaced_hid_send(data, length);
    //     return true;
    // }
    if (*command_id == id_eeprom_reset) {
        hs_reset_settings();
    }
    return false;
}
