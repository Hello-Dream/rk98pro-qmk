// SPDX-License-Identifier: GPL-2.0-or-later

#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

fn main() {
    rk98pro_monitor_lib::run();
}
