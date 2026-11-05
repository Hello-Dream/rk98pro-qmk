// Copyright 2024 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#if RAW_ENABLE

#    include "quantum.h"
#    include "wireless.h"
#    include "usb_descriptor.h"

/* 上游 host_driver_t 新增 send_raw_hid；
 * 无线模式下由 host_raw_hid_send 分发到这里，替代 md_raw.h 的按行号重命名 */
void md_send_raw_hid(uint8_t *data, uint8_t length) {
    if (length != RAW_EPSIZE) {
        return;
    }

    md_send_raw(data, length);
}

void md_receive_raw_cb(uint8_t *data, uint8_t length) {
    void raw_hid_receive(uint8_t * data, uint8_t length);

    raw_hid_receive(data, length);
}

#endif
