// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "board_init.h"

#include "hs/wireless_glue.h"
#include "hs/store.h"
#include "hs/settings.h"
#include "hs/keys.h"
#include "hs/rgb.h"
#include "hs/indicators.h"
#include "hs/power.h"
#include "hs/key_handlers.h"

#include "rgb_record/rgb_record.h"
#include "rgb_matrix_blink.h"

void hs_board_init(void) {
#ifdef CONSOLE_ENABLE
    debug_enable = true;
#endif

    hs_settings_init();
    hs_power_init();
    lpwr_set_timeout_value(hs_sleep_timeout_ms()); // 应用 VIA 的睡眠超时设置

#ifdef LED_POWER_EN_PIN
    gpio_set_pin_output(LED_POWER_EN_PIN);
#endif
    hs_led_power_update(); // 按用户保存的背光开关决定是否供电

#ifdef LED_NUM_PIN
    gpio_set_pin_output(LED_NUM_PIN);
    gpio_write_pin_high(LED_NUM_PIN); // 亮=低电平，先置高=灭
#endif

#ifdef LED_CAPS_PIN
    gpio_set_pin_output(LED_CAPS_PIN);
    gpio_write_pin_high(LED_CAPS_PIN);
#endif

#ifdef LED_SCROLL_PIN
    gpio_set_pin_output(LED_SCROLL_PIN);
    gpio_write_pin_high(LED_SCROLL_PIN);
#endif

#ifdef LED_WIN_PIN
    gpio_set_pin_output(LED_WIN_PIN);
    gpio_write_pin_high(LED_WIN_PIN);
#endif

#ifdef LED_POV_PIN
    gpio_set_pin_output(LED_POV_PIN);
    gpio_write_pin_high(LED_POV_PIN);
#endif

#ifdef LED_MAC_PIN
    gpio_set_pin_output(LED_MAC_PIN);
    gpio_write_pin_high(LED_MAC_PIN);
#endif

#ifdef MM_BT_DEF_PIN
    gpio_set_pin_input_high(MM_BT_DEF_PIN);
#endif

#ifdef MM_2G4_DEF_PIN
    gpio_set_pin_input_high(MM_2G4_DEF_PIN);
#endif

#ifdef USB_POWER_EN_PIN
    gpio_write_pin_low(USB_POWER_EN_PIN);
    gpio_set_pin_output(USB_POWER_EN_PIN);
#endif

#ifdef HS_BAT_CABLE_PIN
    gpio_set_pin_input(HS_BAT_CABLE_PIN);
#endif

#ifdef BAT_FULL_PIN
    gpio_set_pin_input_high(BAT_FULL_PIN);
#endif

#ifdef RGB_MATRIX_BLINK_INDEX_BAT
    rgb_matrix_blink_set_interval_times(RGB_MATRIX_BLINK_INDEX_BAT, 200, 0xFF);
    rgb_matrix_blink_set(RGB_MATRIX_BLINK_INDEX_BAT);
#endif

#ifdef RGB_MATRIX_BLINK_INDEX_IND
    rgb_matrix_blink_set_interval_times(RGB_MATRIX_BLINK_INDEX_IND, 200, 0xFF);
    rgb_matrix_blink_set(RGB_MATRIX_BLINK_INDEX_IND);
#endif

#ifdef HS_RGB_BLINK_INDEX_REC
    rgb_matrix_blink_set_interval_times(HS_RGB_BLINK_INDEX_REC, 200, 0xFF);
    rgb_matrix_blink_set(HS_RGB_BLINK_INDEX_REC);
#endif

#ifdef WIRELESS_ENABLE
    wireless_init();
#if !(defined(MM_BT_DEF_PIN) && defined(MM_2G4_DEF_PIN))
    wireless_devs_change(!DEVS_USB, hs_settings.wireless_mode, false);
#endif
    post_init_timer = timer_read32();
#endif
}

void hs_board_init_post(void) {
    ind_status = ind_state_none;
    if (hs_settings.rgbrec_channel >= RGBREC_CHANNEL_NUM) {
        hs_settings.rgbrec_channel = 0;
        hs_settings_update();
    }
    rgbrec_init(hs_settings.rgbrec_channel);
    hs_rl_mode = hs_settings.side_light_mode;
    if (hs_rl_mode >= ARRAY_SIZE(buff)) {
        hs_rl_mode              = 0;
        hs_settings.side_light_mode = 0;
        hs_settings_update();
    }
    hs_settings_apply_boot();
    hs_key_handlers_init();
    hs_keys_init();

    // 若 EEPROM 里存的动效号越界，回落到默认动效
    if (rgb_matrix_config.mode >= RGB_MATRIX_EFFECT_MAX) {
        rgb_matrix_config.mode = RGB_MATRIX_DEFAULT_MODE;
    }
}
