// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "hs/wireless_glue.h"
#include "hs/wireless_internal.h"
#include "hs/indicators.h"
#include "hs/power.h"
#include "hs/store.h"

#include "usb_main.h"
#include "lowpower.h"

static ioline_t col_pins[MATRIX_COLS] = MATRIX_COL_PINS;

void lpwr_exti_init_hook(void) {

#ifdef HS_BT_DEF_PIN
    gpio_set_pin_input_high(HS_BT_DEF_PIN);
    waitInputPinDelay();
    palEnableLineEvent(HS_BT_DEF_PIN, PAL_EVENT_MODE_BOTH_EDGES);
#endif

#ifdef HS_2G4_DEF_PIN
    gpio_set_pin_input_high(HS_2G4_DEF_PIN);
    waitInputPinDelay();
    palEnableLineEvent(HS_2G4_DEF_PIN, PAL_EVENT_MODE_BOTH_EDGES);
#endif

    if (lower_sleep) {
#if DIODE_DIRECTION == ROW2COL
        for (uint8_t i = 0; i < ARRAY_SIZE(col_pins); i++) {
            if (col_pins[i] != NO_PIN) {
                gpio_set_pin_output(col_pins[i]);
                gpio_write_pin_high(col_pins[i]);
            }
        }
#endif
    }

    gpio_set_pin_input(HS_BAT_CABLE_PIN);
    waitInputPinDelay();
    palEnableLineEvent(HS_BAT_CABLE_PIN, PAL_EVENT_MODE_RISING_EDGE);
    palEnableLineEvent(A12,PAL_EVENT_MODE_RISING_EDGE);
    nvicEnableVector(USBP_WKUP_IRQn,6);
}

void palcallback_cb(uint8_t line) {
    switch (line) {
        case PAL_PAD(A12): {
            lpwr_set_sleep_wakeupcd(LPWR_WAKEUP_USB);
        } break;
        case PAL_PAD(HS_BAT_CABLE_PIN): {
            lpwr_set_sleep_wakeupcd(LPWR_WAKEUP_CABLE);
        } break;
#ifdef HS_2G4_DEF_PIN
        case PAL_PAD(HS_2G4_DEF_PIN): {
            lpwr_set_sleep_wakeupcd(LPWR_WAKEUP_SWITCH);
        } break;
#endif

#ifdef HS_BT_DEF_PIN
        case PAL_PAD(HS_BT_DEF_PIN): {
            lpwr_set_sleep_wakeupcd(LPWR_WAKEUP_SWITCH);
        } break;
#endif
        default: {

        } break;
    }
}

void lpwr_stop_hook_pre(void) { // wireless sleep
    hs_store_flush();

#ifdef LED_POWER_EN_PIN
    gpio_write_pin_low(LED_POWER_EN_PIN);
#endif

    // 睡眠前把指示灯置灭（LED_PIN_ON_STATE=0，写 1 为灭），对齐原厂
#ifdef LED_NUM_PIN
    gpio_write_pin(LED_NUM_PIN, !LED_PIN_ON_STATE);
#endif
#ifdef LED_SCROLL_PIN
    gpio_write_pin(LED_SCROLL_PIN, !LED_PIN_ON_STATE);
#endif
#ifdef LED_CAPS_PIN
    gpio_write_pin(LED_CAPS_PIN, !LED_PIN_ON_STATE);
#endif
#ifdef LED_WIN_PIN
    gpio_write_pin(LED_WIN_PIN, !LED_PIN_ON_STATE);
#endif
#ifdef LED_POV_PIN
    gpio_write_pin(LED_POV_PIN, !LED_PIN_ON_STATE);
#endif
#ifdef LED_MAC_PIN
    gpio_write_pin(LED_MAC_PIN, !LED_PIN_ON_STATE);
#endif

    state = false;
    if (lower_sleep) {
        md_send_devctrl(MD_SND_CMD_DEVCTRL_USB);
        wait_ms(200);
        lpwr_set_sleep_wakeupcd(LPWR_WAKEUP_UART);
    }
}

void lpwr_stop_hook_post(void) {
    if (lower_sleep) {
        switch (lpwr_get_sleep_wakeupcd()) {
            case LPWR_WAKEUP_USB:
            case LPWR_WAKEUP_CABLE: {
                lower_sleep = false;
                lpwr_set_state(LPWR_WAKEUP);
            } break;
            default: {
                lpwr_set_state(LPWR_STOP);
            } break;
        }
    }
}

void hs_set_sleep_state(void) {
    /* Set the sleep state of wireless mode */
    static uint8_t last_devs_mode = 0xFF;
    if (wireless_get_current_devs() != last_devs_mode) {
        last_devs_mode = wireless_get_current_devs();

        // 与 process_record_wls() 保持一致：蓝牙下发深度睡眠时间(0x80+n 分钟)，2.4G 下发 0xFF
        if (last_devs_mode == DEVS_2G4) {
            md_send_devctrl(0xFF);
        } else if ((last_devs_mode >= DEVS_BT1) && (last_devs_mode <= DEVS_BT3)) {
            md_send_devctrl(0x80 + HS_WIRESS_SLEEP_TIME);
        }
    }
}

void lpwr_wakeup_hook(void) { // wireless wakeup
#if defined(WIRELESS_ENABLE) && defined(RGB_MATRIX_ENABLE)
    // 睡眠期间 LED 供电已关闭；清掉旧的临时点亮状态，唤醒后按新的连接状态重新申请
    hs_wls_force_rgb        = false;
    wls_rgb_indicator_timer = 0;
#endif

    if (wireless_get_current_devs() == DEVS_USB && USB_DRIVER.state == USB_STOP) {
        usb_power_connect();
        restart_usb_driver(&USBD1);
        wireless_devs_change(!DEVS_USB, DEVS_USB, false);
    }
    hs_mode_scan(false, hs_settings.wireless_mode, hs_settings.last_bt_channel);

    hs_power_activity();
    hs_led_power_update();
    hs_indicator_rgb_release(); // 用户背光关闭且没有新的临时指示时，清掉睡眠前残留的 RGB 使能
}
