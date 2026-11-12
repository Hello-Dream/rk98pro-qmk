// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "session.h"

#include "hs/wireless_glue.h"
#include "hs/store.h"
#include "hs/indicators.h"
#include "hs/battery.h"
#include "hs/power.h"

#include "rgb_record/rgb_record.h"
#include "rgb_matrix_blink.h"

static uint32_t ee_clr_timer = 0;
bool            lower_sleep  = false;

void hs_session_ee_clr_event(bool pressed) {
    if (pressed) {
        ee_clr_timer = timer_read32();
    } else {
        ee_clr_timer = 0;
    }
}

void hs_session_task(void) {
    // EE_CLR 的 3 秒计时放在主循环，背光关闭时也能生效（原先在 RGB 渲染回调里）
    if (ee_clr_timer && timer_elapsed32(ee_clr_timer) > 3000) {
        ee_clr_timer = 0;
        hs_reset_settings();
    }
}

void hs_reset_settings(void) {
    hs_store_flush();
    ind_status = ind_state_none;
    rgb_matrix_blink_set_remain_time(RGB_MATRIX_BLINK_INDEX_IND, 0);
    hs_bat_force_rgb = false;
    hs_wls_force_rgb = false;
#if defined(WIRELESS_ENABLE) && defined(RGB_MATRIX_ENABLE)
    wls_rgb_indicator_timer = 0;
#endif

    eeconfig_init();

#ifdef RGBLIGHT_ENABLE
    extern void rgblight_init(void);
    is_rgblight_initialized = false;
    rgblight_init();
    eeconfig_update_rgblight_default();
    rgblight_enable();
#endif

    eeconfig_update_rgb_matrix_default();
    eeconfig_read_keymap(&keymap_config);

    if (hs_reset_settings_user() != true) {
        return;
    }

    keyboard_post_init_kb();
}

bool hs_reset_settings_user(void) {
    rgb_matrix_blink_set_color(RGB_MATRIX_BLINK_INDEX_ALL, WIITE_B, WIITE_B, WIITE_B);
    rgb_matrix_blink_set_interval_times(RGB_MATRIX_BLINK_INDEX_ALL, 250, 3);
    rgb_matrix_blink_set(RGB_MATRIX_BLINK_INDEX_ALL);

    return true;
}

#ifdef WIRELESS_ENABLE

void hs_session_usb_connect(void) {
    hs_power_activity();
#ifdef USB_POWER_EN_PIN
    gpio_write_pin_low(USB_POWER_EN_PIN);
#endif
}

void hs_session_usb_disconnect(void) {
    hs_power_activity();
#ifdef USB_POWER_EN_PIN
    gpio_write_pin_high(USB_POWER_EN_PIN);
#endif
}

void hs_session_suspend(void) { // usb power down

#ifdef LED_POWER_EN_PIN
    gpio_write_pin_low(LED_POWER_EN_PIN);
#endif

#ifdef LED_CAPS_PIN
    gpio_write_pin_high(LED_CAPS_PIN);
#endif

#ifdef LED_NUM_PIN
    gpio_write_pin_high(LED_NUM_PIN);
#endif

#ifdef LED_SCROLL_PIN
    gpio_write_pin_high(LED_SCROLL_PIN);
#endif

#ifdef LED_POV_PIN
    gpio_write_pin_high(LED_POV_PIN);
#endif

#ifdef LED_MAC_PIN
    gpio_write_pin_low(LED_MAC_PIN);
#endif

#ifdef LED_WIN_PIN
    gpio_write_pin_high(LED_WIN_PIN);
#endif

    suspend_power_down_user();
}

void hs_session_wakeup(void) { // usb wakeup

    hs_led_power_update();

#ifdef LED_MAC_PIN
    gpio_write_pin_high(LED_MAC_PIN);
#endif

    wireless_devs_change(wireless_get_current_devs(), wireless_get_current_devs(), false);
    hs_indicator_rgb_release(); // 背光关闭且没有临时指示时，清掉唤醒前残留的 RGB 使能
    suspend_wakeup_init_user();
    hs_rgb_blink_set_timer(timer_read32());
}

bool hs_session_allow_timeout(void) {
    // 低电时不要进入普通休眠：让主循环继续运行，由 hs_bat_indicators_task()
    // 完成 20 秒保护提示后再进入只允许充电唤醒的保护深睡，
    // 避免普通休眠中电量耗尽到 0%，触发电池保护板锁死。
    if (!lower_sleep && !charging_state && hs_battery_low_confirmed()) {
        return false;
    }

    return true;
}

#endif
