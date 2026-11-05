// SPDX-License-Identifier: GPL-2.0-or-later

//! 系统托盘、菜单事件与窗口显示。

use tauri::{
    menu::{IsMenuItem, Menu, MenuItem},
    tray::{MouseButton, MouseButtonState, TrayIcon, TrayIconBuilder, TrayIconEvent},
    AppHandle, Manager, Wry,
};

use crate::icon::battery_icon;
use crate::monitor::{Action, MonitorState};
use crate::status::Status;

pub struct TrayState {
    pub tray: TrayIcon<Wry>,
    pub status_item: MenuItem<Wry>,
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

    app.manage(TrayState { tray, status_item });
    Ok(())
}

pub fn apply_status(app: &AppHandle, status: Option<&Status>) {
    let Some(state) = app.try_state::<TrayState>() else {
        return;
    };

    let text = status
        .map(|value| value.summary())
        .unwrap_or_else(|| "未连接".to_string());
    let _ = state.status_item.set_text(text);
    let _ = state.tray.set_tooltip(Some(tooltip(status)));
    let _ = state.tray.set_icon(Some(battery_icon(status)));
}

fn tooltip(status: Option<&Status>) -> String {
    match status {
        Some(value) if value.online => format!("RK98PRO Monitor · {}", value.summary()),
        _ => "RK98PRO Monitor · 未连接".to_string(),
    }
}

fn show_main_window<R: tauri::Runtime>(app: &AppHandle<R>) {
    if let Some(state) = app.try_state::<MonitorState>() {
        state.set_window_visible(true);
        let _ = state.send(Action::Refresh);
    }
    if let Some(window) = app.get_webview_window("main") {
        let _ = window.show();
        let _ = window.unminimize();
        let _ = window.set_focus();
    }
}
