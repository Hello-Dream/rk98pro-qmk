// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include "hs/wireless_glue.h"

bool hs_modeio_detection(bool update, uint8_t *mode, uint8_t lsat_btdev) {
    static uint32_t scan_timer = 0x00;

    if ((update != true) && (timer_elapsed32(scan_timer) <= (HS_MODEIO_DETECTION_TIME))) {
        return false;
    }
    scan_timer = timer_read32();

#if defined(HS_BT_DEF_PIN) && defined(HS_2G4_DEF_PIN)
    uint8_t now_mode         = 0x00;
    uint8_t hs_mode          = 0x00;
    static uint8_t last_mode = 0x00;
    bool sw_mode             = false;
    now_mode                 = (HS_GET_MODE_PIN(HS_USB_PIN_STATE) ? 3 : (HS_GET_MODE_PIN(HS_BT_PIN_STATE) ? 1 : ((HS_GET_MODE_PIN(HS_2G4_PIN_STATE) ? 2 : 0))));
    hs_mode                  = (*mode >= DEVS_BT1 && *mode <= DEVS_BT5) ? 1 : ((*mode == DEVS_2G4) ? 2 : ((*mode == DEVS_USB) ? 3 : 0));
    sw_mode                  = ((update || (last_mode == now_mode)) && (hs_mode != now_mode)) ? true : false;
    last_mode                = now_mode;

    switch (now_mode) {
        case 1:
            *mode = hs_bt;
            if (sw_mode) {
                wireless_devs_change(wireless_get_current_devs(), lsat_btdev, false);
            }
            break;
        case 2:
            *mode = hs_2g4;
            if (sw_mode) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_2G4, false);
            }
            break;
        case 3:
            *mode = hs_usb;
            if (sw_mode)
                wireless_devs_change(wireless_get_current_devs(), DEVS_USB, false);

            break;
        default:
            break;
    }

    if (sw_mode) {
        hs_rgb_blink_set_timer(timer_read32());
        suspend_wakeup_init();
        return true;
    }
#else
    *mode = hs_none;
#endif

    return false;
}

bool hs_mode_scan(bool update, uint8_t moude, uint8_t lsat_btdev) {

    if (hs_modeio_detection(update, &moude, lsat_btdev)) {

        return true;
    }
    hs_rgb_blink_hook();
    return false;
}
