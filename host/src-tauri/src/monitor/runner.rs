// SPDX-License-Identifier: GPL-2.0-or-later

//! HID 轮询主循环、事件处理与重连退避。

use std::sync::mpsc::{Receiver, RecvTimeoutError};
use std::time::{Duration, Instant};

use tauri::{AppHandle, Manager};

use super::publisher;
use super::tracker::StatusTracker;
use super::{Action, MonitorError, MonitorState};
use crate::device::{find_monitor_device, ViaDevice};
use crate::status::Status;

const EVENT_MAGIC: u8 = 0xE0;
const EVENT_STATUS: u8 = 0x01;
const ACTIVE_POLL: Duration = Duration::from_secs(1);
const TRAY_POLL: Duration = Duration::from_secs(30);
const ACTIVE_WAIT: Duration = Duration::from_millis(200);
const TRAY_WAIT: Duration = Duration::from_secs(1);

pub fn start(app: AppHandle, rx: Receiver<Action>) {
    std::thread::spawn(move || run_loop(app, rx));
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum StatusSource {
    Poll,
    Event,
}

fn poll_interval(window_active: bool) -> Duration {
    if window_active {
        ACTIVE_POLL
    } else {
        TRAY_POLL
    }
}

/// 主循环单次等待上限：聚焦 200ms、失焦/托盘 1s；刷新请求仍会立即打断等待。
fn wait_limit(window_active: bool, next_poll: Instant, now: Instant) -> Duration {
    let limit = if window_active {
        ACTIVE_WAIT
    } else {
        TRAY_WAIT
    };
    next_poll.saturating_duration_since(now).min(limit)
}

fn should_apply_event(last_seq: Option<u8>, seq: u8) -> bool {
    match last_seq {
        None => true,
        Some(last) => {
            let diff = seq.wrapping_sub(last);
            diff != 0 && diff < 128
        }
    }
}

fn run_loop(app: AppHandle, rx: Receiver<Action>) {
    let mut tracker = StatusTracker::new();
    let mut device: Option<ViaDevice> = None;
    let mut next_poll = Instant::now();
    let mut backoff = Duration::from_secs(1);
    let mut last_had_status = false;
    let mut last_seq: Option<u8> = None;

    loop {
        publisher::flush_history_if_due(&app);

        let now = Instant::now();
        if now >= next_poll {
            let window_active = app
                .try_state::<MonitorState>()
                .map(|state| state.window_active())
                .unwrap_or(true);
            let interval = poll_interval(window_active);

            if device.is_none() {
                match find_monitor_device(true, 800) {
                    Ok(found) => {
                        device = Some(found);
                        backoff = Duration::from_secs(1);
                    }
                    Err(error) => {
                        log::debug!("连接 RK98PRO 失败：{error}");
                        publisher::publish_monitor_error(
                            &app,
                            Some(MonitorError::from_via(&error)),
                        );
                        handle_offline(&app, &mut tracker, &mut last_had_status);
                        next_poll = now + backoff;
                        backoff = (backoff * 2).min(Duration::from_secs(10));
                        continue;
                    }
                }
            }

            let result = device.as_ref().map(|handle| handle.get_status());
            match result {
                Some(Ok(status)) => {
                    publisher::publish_monitor_error(&app, None);
                    apply_status(
                        &app,
                        &mut tracker,
                        status,
                        StatusSource::Poll,
                        &mut last_seq,
                    );
                    last_had_status = true;
                    next_poll = now + interval;
                }
                Some(Err(error)) => {
                    log::warn!("读取 RK98PRO 状态失败：{error}");
                    publisher::publish_monitor_error(&app, Some(MonitorError::from_via(&error)));
                    device = None;
                    handle_offline(&app, &mut tracker, &mut last_had_status);
                    last_had_status = false;
                    next_poll = now + Duration::from_secs(1);
                }
                None => {}
            }
        }

        let mut drop_device = false;
        if let Some(handle) = device.as_ref() {
            if let Err(error) = handle.poll_events(1) {
                log::debug!("读取状态事件失败：{error}");
                drop_device = true;
            } else {
                while let Some(event) = handle.pop_event() {
                    if event.len() >= 3 && event[0] == EVENT_MAGIC && event[1] == EVENT_STATUS {
                        if let Ok(status) = Status::parse(&event[3..]) {
                            apply_status(
                                &app,
                                &mut tracker,
                                status,
                                StatusSource::Event,
                                &mut last_seq,
                            );
                        }
                    }
                }
            }
        }
        if drop_device {
            device = None;
            handle_offline(&app, &mut tracker, &mut last_had_status);
            last_had_status = false;
            next_poll = Instant::now();
        }

        let wait = wait_limit(
            app.try_state::<MonitorState>()
                .map(|state| state.window_active())
                .unwrap_or(true),
            next_poll,
            Instant::now(),
        );
        match rx.recv_timeout(wait) {
            Ok(Action::Refresh) => {
                next_poll = Instant::now();
            }
            Err(RecvTimeoutError::Timeout) => {}
            Err(RecvTimeoutError::Disconnected) => break,
        }
    }
}

fn apply_status(
    app: &AppHandle,
    tracker: &mut StatusTracker,
    status: Status,
    source: StatusSource,
    last_seq: &mut Option<u8>,
) {
    if source == StatusSource::Event && !should_apply_event(*last_seq, status.seq) {
        return;
    }
    *last_seq = Some(status.seq);

    publisher::publish_status(app, Some(status.clone()));
    let sample_appended = publisher::record_status(app, Some(&status));
    publisher::record_wpm(app, &status);
    let events = tracker.update(Some(&status), Instant::now());
    let event_appended = publisher::notify_all(app, &events);
    if publisher::history_event_needed(sample_appended, event_appended) {
        publisher::emit_history_updated(app);
    }
}

fn handle_offline(app: &AppHandle, tracker: &mut StatusTracker, last_had_status: &mut bool) {
    let events = tracker.update(None, Instant::now());
    let event_appended = publisher::notify_all(app, &events);
    let sample_appended = publisher::record_status(app, None);
    if let Some(state) = app.try_state::<MonitorState>() {
        if let Ok(mut wpm) = state.wpm.lock() {
            wpm.mark_offline();
        }
    }
    if *last_had_status {
        publisher::publish_status(app, None);
        *last_had_status = false;
    }
    if publisher::history_event_needed(sample_appended, event_appended) {
        publisher::emit_history_updated(app);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn event_seq_must_be_newer() {
        assert!(should_apply_event(None, 1));
        assert!(should_apply_event(Some(1), 2));
        assert!(!should_apply_event(Some(2), 2));
        assert!(!should_apply_event(Some(2), 1));
        assert!(should_apply_event(Some(255), 0));
    }

    #[test]
    fn poll_interval_is_focused_or_tray() {
        assert_eq!(poll_interval(true), Duration::from_secs(1));
        assert_eq!(poll_interval(false), Duration::from_secs(30));
    }

    #[test]
    fn wait_limit_uses_focus_aware_cap() {
        let now = Instant::now();
        assert_eq!(
            wait_limit(true, now + Duration::from_secs(1), now),
            Duration::from_millis(200)
        );
        assert_eq!(
            wait_limit(false, now + Duration::from_secs(30), now),
            Duration::from_secs(1)
        );
        assert_eq!(
            wait_limit(false, now + Duration::from_millis(50), now),
            Duration::from_millis(50)
        );
    }
}
