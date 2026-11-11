// SPDX-License-Identifier: GPL-2.0-or-later

//! 系统托盘、菜单事件与窗口显示。

use std::sync::Mutex;

use tauri::{
    menu::{IsMenuItem, Menu, MenuItem},
    tray::{MouseButton, MouseButtonState, TrayIcon, TrayIconBuilder, TrayIconEvent},
    AppHandle, Manager, Wry,
};

use crate::icon::battery_icon;
use crate::monitor::{Action, MonitorState};
use crate::status::Status;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct TrayKey {
    present: bool,
    online: bool,
    mode: u8,
    link: u8,
    battery: Option<u8>,
    charging: u8,
}

impl TrayKey {
    /// 把状态快照转换为托盘摘要键；`present` 用来区分无状态与离线状态。
    fn from_status(status: Option<&Status>) -> Self {
        match status {
            Some(value) => Self {
                present: true,
                online: value.online,
                mode: value.mode,
                link: value.link,
                battery: value.battery,
                charging: value.charging,
            },
            None => Self {
                present: false,
                online: false,
                mode: 0,
                link: 0,
                battery: None,
                charging: 0,
            },
        }
    }
}

pub struct TrayState {
    pub tray: TrayIcon<Wry>,
    pub status_item: MenuItem<Wry>,
    last_key: Mutex<Option<TrayKey>>,
}

pub fn setup(app: &mut tauri::App) -> tauri::Result<()> {
    let status_item = MenuItem::with_id(app, "status", "等待设备…", false, None::<&str>)?;
    let open_item = MenuItem::with_id(app, "open", "打开面板", true, None::<&str>)?;
    let refresh_item = MenuItem::with_id(app, "refresh", "立即刷新", true, None::<&str>)?;
    let quit_item = MenuItem::with_id(app, "quit", "退出", true, None::<&str>)?;
    let items: [&dyn IsMenuItem<Wry>; 4] = [&status_item, &open_item, &refresh_item, &quit_item];
    let menu = Menu::with_items(app, &items)?;

    let tray = TrayIconBuilder::<Wry>::new()
        .icon(battery_icon(None))
        .tooltip("RK98PRO Monitor · 未连接")
        .menu(&menu)
        .show_menu_on_left_click(true)
        .on_menu_event(|app, event| match event.id().0.as_str() {
            "open" => show_main_window(app),
            "refresh" => {
                let _ = app.state::<MonitorState>().send(Action::Refresh);
            }
            "quit" => app.exit(0),
            _ => {}
        })
        .on_tray_icon_event(|tray, event| {
            if let TrayIconEvent::Click {
                button: MouseButton::Left,
                button_state: MouseButtonState::Up,
                ..
            } = event
            {
                show_main_window(tray.app_handle());
            }
        })
        .build(app)?;

    app.manage(TrayState {
        tray,
        status_item,
        last_key: Mutex::new(None),
    });
    Ok(())
}

pub fn apply_status(app: &AppHandle, status: Option<&Status>) {
    let Some(state) = app.try_state::<TrayState>() else {
        return;
    };

    let key = TrayKey::from_status(status);
    if state
        .last_key
        .lock()
        .map(|last| *last == Some(key))
        .unwrap_or(false)
    {
        return;
    }

    let text = status
        .map(|value| value.summary())
        .unwrap_or_else(|| "未连接".to_string());
    let mut updated = true;
    if let Err(error) = state.status_item.set_text(text) {
        log::warn!("更新托盘菜单失败：{error}");
        updated = false;
    }
    if let Err(error) = state.tray.set_tooltip(Some(tooltip(status))) {
        log::warn!("更新托盘 tooltip 失败：{error}");
        updated = false;
    }
    if let Err(error) = state.tray.set_icon(Some(battery_icon(status))) {
        log::warn!("更新托盘图标失败：{error}");
        updated = false;
    }
    if !updated {
        return;
    }

    if let Ok(mut last) = state.last_key.lock() {
        *last = Some(key);
    } else {
        log::warn!("缓存托盘摘要失败：状态锁已损坏");
        return;
    }
    log::debug!("托盘摘要更新：{key:?}");
}

fn tooltip(status: Option<&Status>) -> String {
    match status {
        Some(value) if value.online => format!("RK98PRO Monitor · {}", value.summary()),
        _ => "RK98PRO Monitor · 未连接".to_string(),
    }
}

fn show_main_window<R: tauri::Runtime>(app: &AppHandle<R>) {
    if let Some(state) = app.try_state::<MonitorState>() {
        state.set_window_active(true);
        let _ = state.send(Action::Refresh);
    }
    if let Some(window) = app.get_webview_window("main") {
        let _ = window.show();
        let _ = window.unminimize();
        let _ = window.set_focus();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::status::{CHARGING_CHARGING, CHARGING_DISCHARGING, LINK_CONNECTED, MODE_2G4};

    fn status() -> Status {
        Status {
            seq: 1,
            version: 1,
            firmware_version: "1.0.1".to_string(),
            settings_version: 1,
            battery: Some(80),
            charging: CHARGING_DISCHARGING,
            link: LINK_CONNECTED,
            mode: MODE_2G4,
            power_state: 0,
            flags: 1,
            locks: 0,
            layer: 0,
            default_layer: 0,
            wpm: 0,
            idle_seconds: 0,
            rgbrec_channel: None,
            rgb_mode: 0,
            rgb_brightness: 90,
            wireless_version: 1,
            online: true,
            updated_at: 1_700_000_000,
        }
    }

    #[test]
    fn key_ignores_fields_not_shown_in_tray() {
        let base = status();
        let mut changed = base.clone();
        changed.seq = 9;
        changed.settings_version = 2;
        changed.power_state = 4;
        changed.flags = 0;
        changed.locks = 3;
        changed.layer = 4;
        changed.default_layer = 2;
        changed.wpm = 99;
        changed.idle_seconds = 42;
        changed.rgbrec_channel = Some(2);
        changed.rgb_mode = 5;
        changed.rgb_brightness = 10;
        changed.wireless_version = 2;
        changed.updated_at = 1_800_000_000;

        assert_eq!(
            TrayKey::from_status(Some(&base)),
            TrayKey::from_status(Some(&changed))
        );
    }

    #[test]
    fn key_tracks_summary_and_icon_inputs() {
        let base = status();
        let key = TrayKey::from_status(Some(&base));

        let mut lower_battery = base.clone();
        lower_battery.battery = Some(79);
        assert_ne!(key, TrayKey::from_status(Some(&lower_battery)));

        let mut no_battery = base.clone();
        no_battery.battery = None;
        assert_ne!(key, TrayKey::from_status(Some(&no_battery)));

        let mut charging = base.clone();
        charging.charging = CHARGING_CHARGING;
        assert_ne!(key, TrayKey::from_status(Some(&charging)));

        let mut mode = base.clone();
        mode.mode = 0;
        assert_ne!(key, TrayKey::from_status(Some(&mode)));

        let mut link = base.clone();
        link.link = 3;
        assert_ne!(key, TrayKey::from_status(Some(&link)));

        let mut offline = base.clone();
        offline.online = false;
        assert_ne!(key, TrayKey::from_status(Some(&offline)));
    }

    #[test]
    fn key_keeps_none_distinct_from_offline_status() {
        let mut offline = status();
        offline.online = false;
        offline.mode = 0;
        offline.link = 0;
        offline.battery = None;
        offline.charging = CHARGING_DISCHARGING;

        assert_ne!(
            TrayKey::from_status(None),
            TrayKey::from_status(Some(&offline))
        );
    }
}
