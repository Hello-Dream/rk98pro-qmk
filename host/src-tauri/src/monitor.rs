// SPDX-License-Identifier: GPL-2.0-or-later

//! 后台轮询、低电/充满/断连通知与状态发布。

use std::collections::HashSet;
use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::{Receiver, RecvTimeoutError, Sender};
use std::sync::Mutex;
use std::time::{Duration, Instant};

use tauri::{AppHandle, Emitter, Manager};
use tauri_plugin_notification::NotificationExt;

use crate::device::{find_monitor_device, ViaDevice};
use crate::history::{HistorySnapshot, HistoryStore};
use crate::preferences::{PreferenceStore, Preferences};
use crate::status::{Status, CHARGING_CHARGING, CHARGING_DISCHARGING, CHARGING_FULL, FULL_BATTERY_MIN};
use crate::storage::now_epoch;
use crate::tray;
use crate::wpm::{WpmStats, WpmTracker};

const EVENT_MAGIC: u8 = 0xE0;
const EVENT_STATUS: u8 = 0x01;
const VISIBLE_POLL: Duration = Duration::from_secs(1);
const TRAY_POLL: Duration = Duration::from_secs(30);

#[derive(Debug)]
pub enum Action {
    Refresh,
}

pub struct MonitorState {
    pub status: Mutex<Option<Status>>,
    pub history: Mutex<HistoryStore>,
    pub preferences: Mutex<PreferenceStore>,
    wpm: Mutex<WpmTracker>,
    window_visible: AtomicBool,
    tx: Mutex<Sender<Action>>,
}

impl MonitorState {
    pub fn new(tx: Sender<Action>, data_dir: PathBuf) -> Self {
        Self {
            status: Mutex::new(None),
            history: Mutex::new(HistoryStore::load(data_dir.join("history.json"))),
            preferences: Mutex::new(PreferenceStore::load(data_dir.join("preferences.json"))),
            wpm: Mutex::new(WpmTracker::new()),
            window_visible: AtomicBool::new(true),
            tx: Mutex::new(tx),
        }
    }

    pub fn snapshot(&self) -> Option<Status> {
        self.status.lock().ok().and_then(|status| status.clone())
    }

    pub fn history_snapshot(&self) -> Result<HistorySnapshot, String> {
        self.history
            .lock()
            .map(|history| history.snapshot())
            .map_err(|_| "读取电量历史失败".to_string())
    }

    pub fn preferences_snapshot(&self) -> Result<Preferences, String> {
        self.preferences
            .lock()
            .map(|preferences| preferences.get())
            .map_err(|_| "读取通知设置失败".to_string())
    }

    pub fn set_preferences(&self, preferences: Preferences) -> Result<Preferences, String> {
        let mut store = self
            .preferences
            .lock()
            .map_err(|_| "保存通知设置失败".to_string())?;
        store.set(preferences)?;
        Ok(store.get())
    }

    pub fn wpm_snapshot(&self) -> Result<WpmStats, String> {
        self.wpm
            .lock()
            .map(|wpm| wpm.snapshot())
            .map_err(|_| "读取 WPM 统计失败".to_string())
    }

    pub fn reset_wpm(&self) -> Result<WpmStats, String> {
        self.wpm
            .lock()
            .map(|mut wpm| wpm.reset())
            .map_err(|_| "重置 WPM 统计失败".to_string())
    }

    pub fn set_window_visible(&self, visible: bool) {
        self.window_visible.store(visible, Ordering::SeqCst);
    }

    pub fn window_visible(&self) -> bool {
        self.window_visible.load(Ordering::SeqCst)
    }

    pub fn send(&self, action: Action) -> Result<(), String> {
        self.tx
            .lock()
            .map_err(|_| "内部状态锁失败".to_string())?
            .send(action)
            .map_err(|_| "监视线程已停止".to_string())
    }
}

#[derive(Debug, Clone)]
pub struct AppNotification {
    pub kind: &'static str,
    pub title: String,
    pub message: String,
}

pub struct StatusTracker {
    low_thresholds: Vec<u8>,
    disconnect_grace: Duration,
    low_notified: HashSet<u8>,
    last_charging: Option<u8>,
    online_ever: bool,
    offline_since: Option<Instant>,
    disconnect_notified: bool,
}

impl StatusTracker {
    pub fn new() -> Self {
        Self {
            low_thresholds: vec![20, 15, 10, 5],
            disconnect_grace: Duration::from_secs(60),
            low_notified: HashSet::new(),
            last_charging: None,
            online_ever: false,
            offline_since: None,
            disconnect_notified: false,
        }
    }

    pub fn update(&mut self, status: Option<&Status>, now: Instant) -> Vec<AppNotification> {
        let mut events = Vec::new();
        let Some(status) = status else {
            self.handle_offline(now, &mut events);
            return events;
        };

        if status.online {
            if self.disconnect_notified {
                events.push(AppNotification {
                    kind: "reconnect",
                    title: "RK98PRO 已重新连接".to_string(),
                    message: status.summary(),
                });
                self.disconnect_notified = false;
            }
            self.offline_since = None;
            self.online_ever = true;
        } else {
            self.handle_offline(now, &mut events);
        }

        if status.online {
            self.handle_charging(status, &mut events);
            self.handle_low_battery(status, &mut events);
        }

        events
    }

    fn handle_offline(&mut self, now: Instant, events: &mut Vec<AppNotification>) {
        let since = *self.offline_since.get_or_insert(now);
        if self.online_ever
            && !self.disconnect_notified
            && now.saturating_duration_since(since) >= self.disconnect_grace
        {
            events.push(AppNotification {
                kind: "disconnect",
                title: "RK98PRO 连接已断开".to_string(),
                message: "按任意键唤醒键盘，或检查 2.4G 接收器是否正常。".to_string(),
            });
            self.disconnect_notified = true;
        }
    }

    fn handle_charging(&mut self, status: &Status, events: &mut Vec<AppNotification>) {
        let battery_full = status.battery.is_some_and(|battery| battery >= FULL_BATTERY_MIN);
        if status.charging == CHARGING_FULL
            && battery_full
            && self.last_charging.is_some()
            && self.last_charging != Some(CHARGING_FULL)
        {
            let message = match status.battery {
                Some(battery) => format!("当前电量 {battery}%。"),
                None => "电池已充满。".to_string(),
            };
            events.push(AppNotification {
                kind: "full",
                title: "RK98PRO 已充满".to_string(),
                message,
            });
        }
        if status.charging == CHARGING_CHARGING || status.charging == CHARGING_FULL {
            self.low_notified.clear();
        }
        self.last_charging = Some(status.charging);
    }

    fn handle_low_battery(&mut self, status: &Status, events: &mut Vec<AppNotification>) {
        if status.charging != CHARGING_DISCHARGING {
            return;
        }
        let Some(battery) = status.battery else {
            return;
        };
        if self.low_thresholds.is_empty() {
            return;
        }

        let max_threshold = self.low_thresholds.iter().copied().max().unwrap_or(100);
        if battery > max_threshold {
            self.low_notified.clear();
            return;
        }

        let threshold = self
            .low_thresholds
            .iter()
            .filter(|value| battery <= **value)
            .min()
            .copied()
            .unwrap_or(max_threshold);
        if self.low_notified.contains(&threshold) {
            return;
        }

        for value in &self.low_thresholds {
            if *value >= threshold {
                self.low_notified.insert(*value);
            }
        }

        let message = if battery <= 5 {
            format!("电量仅剩 {battery}%，请尽快充电。")
        } else {
            format!("电量已降至 {battery}%，请及时充电。")
        };
        events.push(AppNotification {
            kind: "low_battery",
            title: format!("RK98PRO 电量低（{battery}%）"),
            message,
        });
    }
}

pub fn start(app: AppHandle, rx: Receiver<Action>) {
    std::thread::spawn(move || run_loop(app, rx));
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum StatusSource {
    Poll,
    Event,
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
        let now = Instant::now();
        if now >= next_poll {
            let interval = app
                .try_state::<MonitorState>()
                .map(|state| if state.window_visible() { VISIBLE_POLL } else { TRAY_POLL })
                .unwrap_or(VISIBLE_POLL);

            if device.is_none() {
                match find_monitor_device(true, 800) {
                    Ok(found) => {
                        device = Some(found);
                        backoff = Duration::from_secs(1);
                    }
                    Err(error) => {
                        log::debug!("连接 RK98PRO 失败：{error}");
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
                    apply_status(&app, &mut tracker, status, StatusSource::Poll, &mut last_seq);
                    last_had_status = true;
                    next_poll = now + interval;
                }
                Some(Err(error)) => {
                    log::warn!("读取 RK98PRO 状态失败：{error}");
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

        let wait = next_poll
            .saturating_duration_since(Instant::now())
            .min(Duration::from_millis(100));
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

    publish_status(app, Some(status.clone()));
    record_status(app, Some(&status));
    record_wpm(app, &status);
    let events = tracker.update(Some(&status), Instant::now());
    notify_all(app, &events);
}

fn handle_offline(app: &AppHandle, tracker: &mut StatusTracker, last_had_status: &mut bool) {
    let events = tracker.update(None, Instant::now());
    notify_all(app, &events);
    record_status(app, None);
    if let Some(state) = app.try_state::<MonitorState>() {
        if let Ok(mut wpm) = state.wpm.lock() {
            wpm.mark_offline();
        }
    }
    if *last_had_status {
        publish_status(app, None);
        *last_had_status = false;
    }
}

fn record_status(app: &AppHandle, status: Option<&Status>) {
    if let Some(state) = app.try_state::<MonitorState>() {
        if let Ok(mut history) = state.history.lock() {
            history.record_status(status, now_epoch());
        }
    }
}

fn record_wpm(app: &AppHandle, status: &Status) {
    if let Some(state) = app.try_state::<MonitorState>() {
        if let Ok(mut wpm) = state.wpm.lock() {
            wpm.record(status.wpm, now_epoch());
        }
    }
}

fn record_event(app: &AppHandle, event: &AppNotification) {
    if let Some(state) = app.try_state::<MonitorState>() {
        if let Ok(mut history) = state.history.lock() {
            history.record_event(event.kind, &event.title, &event.message, now_epoch());
        }
    }
}

fn notify_all(app: &AppHandle, events: &[AppNotification]) {
    let preferences = app
        .try_state::<MonitorState>()
        .and_then(|state| state.preferences.lock().ok().map(|store| store.get()))
        .unwrap_or_default();

    for event in events {
        record_event(app, event);
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
}

fn publish_status(app: &AppHandle, status: Option<Status>) {
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
    use crate::status::LINK_CONNECTED;

    fn status(battery: u8, charging: u8, link: u8) -> Status {
        Status {
            seq: 1,
            version: 1,
            firmware_version: "1.0.1".to_string(),
            settings_version: 1,
            battery: Some(battery),
            charging,
            link,
            mode: 6,
            power_state: 0,
            flags: if link == LINK_CONNECTED { 1 } else { 0 },
            locks: 0,
            layer: 0,
            default_layer: 0,
            wpm: 0,
            idle_seconds: 0,
            rgbrec_channel: None,
            rgb_mode: 0,
            rgb_brightness: 90,
            wireless_version: 1,
            online: link == LINK_CONNECTED,
            updated_at: 1_700_000_000,
        }
    }

    #[test]
    fn low_battery_bands_fire_once() {
        let mut tracker = StatusTracker::new();
        let now = Instant::now();

        assert_eq!(
            tracker
                .update(
                    Some(&status(18, CHARGING_DISCHARGING, LINK_CONNECTED)),
                    now
                )
                .len(),
            1
        );
        assert_eq!(
            tracker
                .update(
                    Some(&status(17, CHARGING_DISCHARGING, LINK_CONNECTED)),
                    now
                )
                .len(),
            0
        );
        assert_eq!(
            tracker
                .update(
                    Some(&status(14, CHARGING_DISCHARGING, LINK_CONNECTED)),
                    now
                )
                .len(),
            1
        );
        assert_eq!(
            tracker
                .update(
                    Some(&status(9, CHARGING_DISCHARGING, LINK_CONNECTED)),
                    now
                )
                .len(),
            1
        );
        assert_eq!(
            tracker
                .update(
                    Some(&status(4, CHARGING_DISCHARGING, LINK_CONNECTED)),
                    now
                )
                .len(),
            1
        );
        assert_eq!(
            tracker
                .update(
                    Some(&status(4, CHARGING_DISCHARGING, LINK_CONNECTED)),
                    now
                )
                .len(),
            0
        );
    }

    #[test]
    fn charging_resets_low_battery_bands() {
        let mut tracker = StatusTracker::new();
        let now = Instant::now();

        assert_eq!(
            tracker
                .update(
                    Some(&status(10, CHARGING_DISCHARGING, LINK_CONNECTED)),
                    now
                )
                .len(),
            1
        );
        assert_eq!(
            tracker
                .update(
                    Some(&status(10, CHARGING_CHARGING, LINK_CONNECTED)),
                    now
                )
                .len(),
            0
        );
        assert_eq!(
            tracker
                .update(
                    Some(&status(10, CHARGING_DISCHARGING, LINK_CONNECTED)),
                    now
                )
                .len(),
            1
        );
    }

    #[test]
    fn full_notification_only_on_transition() {
        let mut tracker = StatusTracker::new();
        let now = Instant::now();

        assert_eq!(
            tracker
                .update(
                    Some(&status(90, CHARGING_CHARGING, LINK_CONNECTED)),
                    now
                )
                .len(),
            0
        );
        let events = tracker.update(Some(&status(100, CHARGING_FULL, LINK_CONNECTED)), now);
        assert_eq!(events.len(), 1);
        assert_eq!(events[0].kind, "full");
        assert_eq!(
            tracker
                .update(Some(&status(100, CHARGING_FULL, LINK_CONNECTED)), now)
                .len(),
            0
        );
    }

    #[test]
    fn event_seq_must_be_newer() {
        assert!(should_apply_event(None, 1));
        assert!(should_apply_event(Some(1), 2));
        assert!(!should_apply_event(Some(2), 2));
        assert!(!should_apply_event(Some(2), 1));
        assert!(should_apply_event(Some(255), 0));
    }

    #[test]
    fn disconnect_after_grace_and_reconnect() {
        let mut tracker = StatusTracker::new();
        let start = Instant::now();

        assert_eq!(
            tracker
                .update(
                    Some(&status(80, CHARGING_DISCHARGING, LINK_CONNECTED)),
                    start
                )
                .len(),
            0
        );
        assert_eq!(
            tracker
                .update(Some(&status(80, CHARGING_DISCHARGING, 3)), start + Duration::from_secs(5))
                .len(),
            0
        );
        let events = tracker.update(
            Some(&status(80, CHARGING_DISCHARGING, 3)),
            start + Duration::from_secs(66),
        );
        assert_eq!(events.len(), 1);
        assert_eq!(events[0].kind, "disconnect");
        assert_eq!(
            tracker
                .update(
                    Some(&status(80, CHARGING_DISCHARGING, 3)),
                    start + Duration::from_secs(67)
                )
                .len(),
            0
        );

        let events = tracker.update(
            Some(&status(80, CHARGING_DISCHARGING, LINK_CONNECTED)),
            start + Duration::from_secs(68),
        );
        assert_eq!(events.len(), 1);
        assert_eq!(events[0].kind, "reconnect");
    }
}
