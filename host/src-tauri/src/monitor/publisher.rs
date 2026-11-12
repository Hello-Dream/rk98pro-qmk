// SPDX-License-Identifier: GPL-2.0-or-later

//! 状态、错误、历史、WPM 与桌面通知的发布和落盘。

use tauri::{AppHandle, Emitter, Manager};
use tauri_plugin_notification::NotificationExt;

use super::tracker::AppNotification;
use super::MonitorState;
use crate::status::Status;
use crate::storage::now_epoch;
use crate::tray;

pub(super) fn history_event_needed(sample_appended: bool, event_appended: bool) -> bool {
    sample_appended || event_appended
}

pub(super) fn flush_history_if_due(app: &AppHandle) {
    if let Some(state) = app.try_state::<MonitorState>() {
        if let Ok(mut history) = state.history.lock() {
            history.flush_if_due(now_epoch());
        }
    }
}

pub(super) fn publish_monitor_error(app: &AppHandle, error: Option<super::MonitorError>) {
    if let Some(state) = app.try_state::<MonitorState>() {
        if state.set_monitor_error(error.clone()) {
            let _ = app.emit("monitor-error", error);
        }
    }
}

pub(super) fn emit_history_updated(app: &AppHandle) {
    log::debug!("history-updated：历史数据已追加");
    let _ = app.emit("history-updated", ());
}

pub(super) fn record_status(app: &AppHandle, status: Option<&Status>) -> bool {
    if let Some(state) = app.try_state::<MonitorState>() {
        if let Ok(mut history) = state.history.lock() {
            return history.record_status(status, now_epoch());
        }
    }
    false
}

pub(super) fn record_wpm(app: &AppHandle, status: &Status) {
    if let Some(state) = app.try_state::<MonitorState>() {
        if let Ok(mut wpm) = state.wpm.lock() {
            wpm.record(status.wpm, now_epoch());
        }
    }
}

fn record_event(app: &AppHandle, event: &AppNotification) -> bool {
    if let Some(state) = app.try_state::<MonitorState>() {
        if let Ok(mut history) = state.history.lock() {
            return history.record_event(event.kind, &event.title, &event.message, now_epoch());
        }
    }
    false
}

pub(super) fn notify_all(app: &AppHandle, events: &[AppNotification]) -> bool {
    let preferences = app
        .try_state::<MonitorState>()
        .and_then(|state| state.preferences.lock().ok().map(|store| store.get()))
        .unwrap_or_default();

    let mut recorded = false;
    for event in events {
        recorded |= record_event(app, event);
        if !preferences.allows(event.kind) {
            continue;
        }
        if let Err(error) = app
            .notification()
            .builder()
            .title(event.title.as_str())
            .body(event.message.as_str())
            .show()
        {
            log::warn!("发送 {} 通知失败：{error}", event.kind);
        }
    }
    recorded
}

pub(super) fn publish_status(app: &AppHandle, status: Option<Status>) {
    if let Some(state) = app.try_state::<MonitorState>() {
        if let Ok(mut current) = state.status.lock() {
            *current = status.clone();
        }
    }
    let _ = app.emit("status-updated", status.clone());
    tray::apply_status(app, status.as_ref());
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn history_event_only_when_history_appended() {
        assert!(!history_event_needed(false, false));
        assert!(history_event_needed(true, false));
        assert!(history_event_needed(false, true));
        assert!(history_event_needed(true, true));
    }
}
