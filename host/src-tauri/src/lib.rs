// SPDX-License-Identifier: GPL-2.0-or-later

mod device;
mod error;
mod history;
mod icon;
mod monitor;
mod preferences;
mod status;
mod storage;
mod tray;
mod wpm;

use std::sync::mpsc::channel;

use tauri::{Manager, State, WindowEvent};

use history::HistorySnapshot;
use monitor::{Action, MonitorError, MonitorState};
use preferences::Preferences;
use status::Status;
use wpm::WpmStats;

#[tauri::command]
fn get_status(state: State<'_, MonitorState>) -> Option<Status> {
    state.snapshot()
}

#[tauri::command]
fn get_monitor_error(state: State<'_, MonitorState>) -> Option<MonitorError> {
    state.monitor_error()
}

#[tauri::command]
fn refresh(state: State<'_, MonitorState>) -> Result<(), String> {
    state.send(Action::Refresh)
}

#[tauri::command]
fn get_history(state: State<'_, MonitorState>) -> Result<HistorySnapshot, String> {
    state.history_snapshot()
}

#[tauri::command]
fn get_wpm_stats(state: State<'_, MonitorState>) -> Result<WpmStats, String> {
    state.wpm_snapshot()
}

#[tauri::command]
fn reset_wpm_stats(state: State<'_, MonitorState>) -> Result<WpmStats, String> {
    state.reset_wpm()
}

#[tauri::command]
fn get_preferences(state: State<'_, MonitorState>) -> Result<Preferences, String> {
    state.preferences_snapshot()
}

#[tauri::command]
fn set_preferences(
    state: State<'_, MonitorState>,
    preferences: Preferences,
) -> Result<Preferences, String> {
    state.set_preferences(preferences)
}

#[tauri::command]
fn test_notification(app: tauri::AppHandle) -> Result<(), String> {
    use tauri_plugin_notification::NotificationExt;

    app.notification()
        .builder()
        .title("RK98PRO Monitor")
        .body("通知工作正常。")
        .show()
        .map_err(|error| error.to_string())
}

pub fn run() {
    let _ = env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info"))
        .try_init();

    tauri::Builder::default()
        .plugin(tauri_plugin_notification::init())
        .invoke_handler(tauri::generate_handler![
            get_status,
            get_monitor_error,
            refresh,
            get_history,
            get_wpm_stats,
            reset_wpm_stats,
            get_preferences,
            set_preferences,
            test_notification
        ])
        .setup(|app| {
            let data_dir = app.path().app_data_dir().unwrap_or_else(|error| {
                log::warn!("无法获取应用数据目录：{error}");
                std::env::temp_dir().join("rk98pro-monitor")
            });

            let (tx, rx) = channel();
            app.manage(MonitorState::new(tx, data_dir));

            if let Err(error) = tray::setup(app) {
                log::error!("托盘初始化失败：{error}");
                if let Some(window) = app.get_webview_window("main") {
                    let _ = window.show();
                }
            }

            monitor::start(app.handle().clone(), rx);
            Ok(())
        })
        .on_window_event(|window, event| {
            let state = window.app_handle().try_state::<MonitorState>();
            match event {
                WindowEvent::Focused(focused) => {
                    if let Some(state) = state {
                        state.set_window_active(*focused);
                        if *focused {
                            let _ = state.send(Action::Refresh);
                        }
                    }
                }
                WindowEvent::CloseRequested { api, .. } => {
                    if window.app_handle().try_state::<tray::TrayState>().is_some() {
                        api.prevent_close();
                        let _ = window.hide();
                        if let Some(state) = state {
                            state.set_window_active(false);
                        }
                    }
                }
                _ => {}
            }
        })
        .build(tauri::generate_context!())
        .expect("error while building tauri application")
        .run(|app, event| {
            if let tauri::RunEvent::Exit = event {
                if let Some(state) = app.try_state::<MonitorState>() {
                    state.flush_history();
                }
            }
        });
}

#[cfg(test)]
mod tests {
    const ALLOWED: &[&str] = &[
        "get_status",
        "get_monitor_error",
        "refresh",
        "get_history",
        "get_wpm_stats",
        "reset_wpm_stats",
        "get_preferences",
        "set_preferences",
        "test_notification",
    ];

    const FORBIDDEN: &[&str] = &[
        concat!("set_device", "_setting"),
        concat!("set_device", "_settings"),
        concat!("write", "_block"),
        concat!("read", "_block"),
        concat!("list", "_blocks"),
        concat!("rgb_write", "_channel"),
        concat!("rgb_read", "_channel"),
        concat!("rgb_", "preview"),
        concat!("keymap_", "write"),
        concat!("keymap_", "read"),
        concat!("keymap_", "reset"),
        concat!("keymap_", "info"),
        concat!("capture", "_key"),
        concat!("wireless_", "switch"),
        concat!("wireless_", "pair"),
        concat!("wireless_", "clear"),
        concat!("import", "_config"),
        concat!("export", "_config"),
        concat!("show", "_battery"),
    ];

    #[test]
    fn command_surface_is_read_only() {
        let source = include_str!("lib.rs");
        for name in ALLOWED {
            assert!(
                source.contains(&format!("fn {name}")),
                "缺少只读/本地命令 {name}"
            );
        }
        for name in FORBIDDEN {
            assert!(
                !source.contains(&format!("fn {name}")),
                "发现键盘写命令 {name}"
            );
        }
    }
}
