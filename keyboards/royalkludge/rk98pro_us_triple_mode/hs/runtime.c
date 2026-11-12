// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "runtime.h"

#include "hs/wireless_glue.h"
#include "hs/store.h"
#include "hs/status.h"
#include "hs/rgb.h"
#include "hs/indicators.h"
#include "hs/power.h"
#include "hs/session.h"

#include "rgb_record/rgb_record.h"
#include "rgb_matrix_blink.h"

void hs_runtime_task(void) { // loop

    // 用户中途真正打开背光（不是空闲关灯/状态灯临时点亮）时，临时状态灯标记才失效
    if (mm_get_rgb_enable() && !hs_power_rgb_off && !hs_indicator_rgb_acquired) {
        hs_bat_force_rgb = false;
        hs_wls_force_rgb = false;
    }

    // 记录最近一次实际灯效，退出录制预览 / VIA 切换后都能恢复正确效果
    if (hs_record_last_mode_update()) {
        hs_settings_update();
    }

    // 录制已结束但指示灯仍停在闪烁状态时（如 VIA 切通道 / EEPROM 复位）自动清理
    if (ind_status == ind_state_blinkon && !rgbrec_is_started()) {
        ind_set_blink(ind_state_none);
    }

    // EE_CLR 的 3 秒计时放在主循环，背光关闭时也能生效（原先在 RGB 渲染回调里）
    hs_session_task();

    hs_store_task();
    hs_status_task();
    // hs_send_bat_status() 内部已调用 hs_battery_task()，这里不再重复采样
    hs_send_bat_status();
    hs_bat_indicators_task();
    hs_power_task();
    hs_rgb_blink_hook_user();
#if defined(WIRELESS_ENABLE) && defined(RGB_MATRIX_ENABLE)
    hs_wls_indicator_force_task();
#endif
    hs_set_sleep_state();
    hs_update_indicators();

    // 电量条显示约 3 秒后自动关掉
    extern bool rk_bat_req_flag;
    if (rk_bat_req_flag && timer_elapsed32(hs_bat_show_timer) > 3000) {
        rk_bat_req_flag = false;
        // 若显示前背光是关闭的，恢复关闭状态（避免因看电量把背光一直打开）
        if (hs_bat_force_rgb) {
            hs_bat_force_rgb = false;
            hs_indicator_rgb_release();
        }
    }
}

bool hs_runtime_indicators(uint8_t led_min, uint8_t led_max) {

#ifdef RGBLIGHT_ENABLE
    if (rgb_matrix_indicators_advanced_user(led_min, led_max) != true) {

        return false;
    }
#endif

    // 指示灯（Caps/Num/Scroll/Win）已移到 housekeeping_task_user() 里每轮刷新，
    // 避免 RGB 渲染未运行时引脚卡在某个状态（厂商原版也是在主循环里驱动）。

#ifdef RGBLIGHT_ENABLE
    if (rgb_matrix_indicators_advanced_rgblight(led_min, led_max) != true) {

        return false;
    }
#endif

    // 背光关闭、仅为无线指示 / 状态灯临时点亮时，清掉灯效与侧灯，只保留状态指示。
    // 电量条只覆盖主键灯，不能把仍开启的侧灯一起清黑，因此这里不包含 hs_bat_force_rgb。
    if (hs_wls_force_rgb || hs_status_force_rgb) {
        rgb_matrix_set_color_all(0x00, 0x00, 0x00);
    }

#ifdef WIRELESS_ENABLE
    if (!linker_ok) {
        rgb_matrix_wls_indicator();
        hs_wls_connected_indicator();
    }
#endif

    rgb_matrix_blink_task(led_min, led_max);
    query();
    hs_status_render(led_min, led_max);
    return true;
}
