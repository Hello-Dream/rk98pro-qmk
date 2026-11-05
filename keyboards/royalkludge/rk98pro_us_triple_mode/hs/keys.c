// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "keys.h"

#include "hs/wireless_glue.h"
#include "hs/store.h"
#include "hs/layers.h"
#include "rgb_matrix_blink.h"

#include "rgb_record/rgb_record.h"

extern bool no_record_fg;

/* 短按键过滤（防连击）：某个物理键松开后的 HS_SKF_LOCKOUT_MS 内忽略该键再次按下；
 * 被忽略的那次按下，其对应的“松开”也会一并吞掉，避免向动作系统发出孤儿松开。
 * 由 Fn+Z 开关，状态存 hs_settings.anti_bounce。 */
#define HS_SKF_LOCKOUT_MS 20 // 锁定窗口：短于人手同键连打的最快间隔，只挡机械抖动

static uint16_t hs_skf_release[MATRIX_ROWS][MATRIX_COLS]; // 各物理键最近一次松开时刻（timer_read）
static bool     hs_skf_dropped[MATRIX_ROWS][MATRIX_COLS]; // 该键当前这次按下被丢弃，松开时需吞掉

// 清空锁定记录（重置设置或关闭过滤时调用）
static void hs_skf_reset(void) {
    uint16_t stale = timer_read() - (HS_SKF_LOCKOUT_MS + 1); // 让首次判断一定落在窗口外
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
        for (uint8_t c = 0; c < MATRIX_COLS; c++) {
            hs_skf_release[r][c] = stale;
            hs_skf_dropped[r][c] = false;
        }
    }
}

// 该物理键是否刚松开、仍处于锁定窗口内（true 表示这次按下应被忽略）
static bool hs_skf_is_locked(keypos_t pos) {
    return timer_elapsed(hs_skf_release[pos.row][pos.col]) < HS_SKF_LOCKOUT_MS;
}


void hs_keys_init(void) {
    hs_skf_reset();
}

/* 短按键过滤：在按键进入处理流程前拦下「刚松开又立刻按下」的同一物理键。
 * 只对普通字符键与修饰键生效，Fn 组合 / 自定义键一律直通。 */
bool pre_process_record_kb(uint16_t keycode, keyrecord_t *record) {
    // UG_TOGG 在 process_record_kb 里单独处理唤醒；其余按键（含电量键）立即恢复空闲状态
    if (keycode != UG_TOGG) {
        hs_power_activity();
    }

    if (!hs_settings.anti_bounce || !IS_KEYEVENT(record->event)) {
        return true;
    }
    if (!(IS_BASIC_KEYCODE(keycode) || IS_MODIFIER_KEYCODE(keycode))) {
        return true;
    }
#ifdef IS_MOUSE_KEYCODE
    if (IS_MOUSE_KEYCODE(keycode)) { // 鼠标模式不参与短按键过滤
        return true;
    }
#endif
    if (rgbrec_is_started()) { // 录制按键 RGB 时不拦截，否则录制中按键改色会失灵
        return true;
    }
    if (record->event.key.row >= MATRIX_ROWS || record->event.key.col >= MATRIX_COLS) {
        return true;
    }

    uint8_t row = record->event.key.row;
    uint8_t col = record->event.key.col;

    if (record->event.pressed) {
        if (hs_skf_is_locked(record->event.key)) {
            hs_skf_dropped[row][col] = true; // 记下，等它松开时一并吞掉
            return false;                    // 仍在锁定窗口内：丢弃这次按下
        }
        return true;
    }

    hs_skf_release[row][col] = timer_read(); // 松开：开启新一轮锁定窗口
    if (hs_skf_dropped[row][col]) {
        hs_skf_dropped[row][col] = false;
        return false; // 被丢弃按下的“松开”，直接吞掉
    }
    return true;
}

// 开 / 关短按键过滤；VIA toggle 与 Fn+Z 共用
bool hs_get_anti_bounce(void) {
    return hs_settings.anti_bounce;
}

void hs_set_anti_bounce(bool enabled) {
    hs_settings.anti_bounce = enabled;
    if (!enabled) {
        hs_skf_reset(); // 关闭时清掉锁定状态
    }
    hs_settings_update();

    // 指示灯：开 = 白闪 3 次，关 = 红闪 3 次
    rgb_matrix_blink_set_color(IM_MM_RGB_BLINK_INDEX_DEB, enabled ? WIITE_B : 0xFF, enabled ? WIITE_B : 0x00, enabled ? WIITE_B : 0x00);
    rgb_matrix_blink_set_interval_times(IM_MM_RGB_BLINK_INDEX_DEB, 250, 3);
    rgb_matrix_blink_set(IM_MM_RGB_BLINK_INDEX_DEB);
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {

    if (*md_getp_state() == MD_STATE_CONNECTED) {
        hs_rgb_blink_set_timer(timer_read32());
    }

    switch (keycode) {
        case MO(_FL):
        case MO(_MFL): {
            if (!record->event.pressed && rgbrec_is_started()) {
                if (no_record_fg == true) {
                    no_record_fg = false;
                    rgbrec_register_record(keycode, record);
                }
                no_record_fg = true;
            }
            break;
        }
        case RP_END:
        case RP_P0:
        case RP_P1:
        case RP_P2:
        case UG_NEXT:
            break;
        default: {
            if (rgbrec_is_started()) {
                if (!IS_QK_MOMENTARY(keycode) && record->event.pressed) {
                    rgbrec_register_record(keycode, record);

                    return false;
                }
            }
        } break;
    }

    if (rgbrec_is_started() && (!(keycode == RP_P0 || keycode == RP_P1 || keycode == RP_P2 || keycode == RP_END || keycode == UG_NEXT || keycode == MO(_FL) || keycode == MO(_MFL)))) {

        return false;
    }

    return true;
}

