// SPDX-License-Identifier: GPL-2.0-or-later

//! 电量采样与事件记录。数据只保存在本机应用数据目录。

use std::fs;
use std::path::PathBuf;

use serde::{Deserialize, Serialize};

use crate::status::Status;
use crate::storage::{now_epoch, write_json};

const SAMPLE_INTERVAL: u64 = 5 * 60;
const PERSIST_INTERVAL: u64 = 30;
const MAX_AGE: u64 = 7 * 24 * 60 * 60;
const MAX_SAMPLES: usize = 4096;
const MAX_EVENTS: usize = 100;

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Sample {
    pub t: u64,
    pub battery: Option<u8>,
    pub charging: u8,
    pub link: i16,
    pub mode: i16,
    pub online: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct HistoryEvent {
    pub t: u64,
    pub kind: String,
    pub title: String,
    pub message: String,
}

#[derive(Debug, Clone, Default, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct HistorySnapshot {
    pub samples: Vec<Sample>,
    pub events: Vec<HistoryEvent>,
}

#[derive(Debug, Default, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
struct HistoryFile {
    samples: Vec<Sample>,
    events: Vec<HistoryEvent>,
}

pub struct HistoryStore {
    path: PathBuf,
    samples: Vec<Sample>,
    events: Vec<HistoryEvent>,
    dirty: bool,
    last_persist: u64,
    next_retry_at: u64,
}

impl HistoryStore {
    pub fn load(path: PathBuf) -> Self {
        let file = fs::read(&path)
            .ok()
            .and_then(|bytes| serde_json::from_slice::<HistoryFile>(&bytes).ok())
            .unwrap_or_default();
        let mut store = Self {
            path,
            samples: file.samples,
            events: file.events,
            dirty: false,
            last_persist: 0,
            next_retry_at: 0,
        };
        store.prune(now_epoch());
        store
    }

    pub fn snapshot(&self) -> HistorySnapshot {
        HistorySnapshot {
            samples: self.samples.clone(),
            events: self.events.clone(),
        }
    }

    /// 在线时按 5 分钟或状态变化采样；离线时只记录状态变化，避免重试刷屏。
    /// 返回是否真正追加了样本，供监视线程决定是否推送历史更新事件。
    pub fn record_status(&mut self, status: Option<&Status>, now: u64) -> bool {
        let sample = match status {
            Some(status) if status.online => Sample {
                t: now,
                battery: status.battery,
                charging: status.charging,
                link: status.link as i16,
                mode: status.mode as i16,
                online: true,
            },
            _ => Sample {
                t: now,
                battery: None,
                charging: 0,
                link: -1,
                mode: -1,
                online: false,
            },
        };

        let last = self.samples.last();
        let changed = last
            .map(|last| {
                last.online != sample.online
                    || last.charging != sample.charging
                    || last.link != sample.link
                    || last.mode != sample.mode
                    || last.battery != sample.battery
            })
            .unwrap_or(true);
        let due = last
            .map(|last| now.saturating_sub(last.t) >= SAMPLE_INTERVAL)
            .unwrap_or(true);
        if !changed && !due {
            return false;
        }

        self.samples.push(sample);
        self.prune(now);
        self.dirty = true;
        true
    }

    /// 记录一条通知事件；当前总会追加，返回 `true` 便于调用方统一聚合。
    pub fn record_event(&mut self, kind: &str, title: &str, message: &str, now: u64) -> bool {
        self.events.push(HistoryEvent {
            t: now,
            kind: kind.to_string(),
            title: title.to_string(),
            message: message.to_string(),
        });
        self.prune(now);
        self.dirty = true;
        true
    }

    fn prune(&mut self, now: u64) {
        let cutoff = now.saturating_sub(MAX_AGE);
        self.samples.retain(|sample| sample.t >= cutoff);
        if self.samples.len() > MAX_SAMPLES {
            let excess = self.samples.len() - MAX_SAMPLES;
            self.samples.drain(..excess);
        }

        self.events.retain(|event| event.t >= cutoff);
        if self.events.len() > MAX_EVENTS {
            let excess = self.events.len() - MAX_EVENTS;
            self.events.drain(..excess);
        }
    }

    fn persist(&self) -> Result<(), String> {
        let file = HistoryFile {
            samples: self.samples.clone(),
            events: self.events.clone(),
        };
        write_json(&self.path, &file)
    }

    /// 按 30 秒间隔把内存历史写入磁盘；失败时保留 dirty 并延后重试。
    /// 返回本次是否成功落盘（没有待写数据时返回 `false`）。
    pub fn flush_if_due(&mut self, now: u64) -> bool {
        if !self.dirty || now < self.next_retry_at {
            return false;
        }
        if self.last_persist != 0 && now.saturating_sub(self.last_persist) < PERSIST_INTERVAL {
            return false;
        }
        self.flush(now)
    }

    /// 立即落盘未保存的历史；退出前调用，失败不阻塞退出。
    pub fn flush(&mut self, now: u64) -> bool {
        if !self.dirty {
            return true;
        }
        match self.persist() {
            Ok(()) => {
                self.dirty = false;
                self.last_persist = now;
                self.next_retry_at = now.saturating_add(PERSIST_INTERVAL);
                true
            }
            Err(error) => {
                log::warn!("保存电量历史失败：{error}");
                self.next_retry_at = now.saturating_add(PERSIST_INTERVAL);
                false
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::status::{CHARGING_CHARGING, CHARGING_DISCHARGING, LINK_CONNECTED, MODE_2G4};
    use std::sync::atomic::{AtomicU64, Ordering};

    static NEXT_ID: AtomicU64 = AtomicU64::new(0);

    fn status(battery: u8, charging: u8) -> Status {
        Status {
            seq: 1,
            version: 1,
            firmware_version: "1.0.1".to_string(),
            settings_version: 1,
            battery: Some(battery),
            charging,
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

    fn temp_path() -> PathBuf {
        std::env::temp_dir().join(format!(
            "rk98pro-monitor-history-{}-{}-{}.json",
            std::process::id(),
            now_epoch(),
            NEXT_ID.fetch_add(1, Ordering::Relaxed)
        ))
    }

    fn read_history(path: &PathBuf) -> HistoryFile {
        let bytes = std::fs::read(path).expect("history file should exist");
        serde_json::from_slice(&bytes).expect("history json should parse")
    }

    #[test]
    fn debounces_writes_and_flushes_on_demand() {
        let path = temp_path();
        let mut store = HistoryStore::load(path.clone());
        let now = 1_700_000_000;

        assert!(store.record_status(Some(&status(80, CHARGING_DISCHARGING)), now));
        assert!(store.flush_if_due(now));
        assert_eq!(read_history(&path).samples.len(), 1);

        assert!(store.record_status(Some(&status(79, CHARGING_DISCHARGING)), now + 1));
        assert!(!store.flush_if_due(now + 2));
        assert_eq!(read_history(&path).samples.len(), 1);

        assert!(store.flush_if_due(now + PERSIST_INTERVAL));
        assert_eq!(read_history(&path).samples.len(), 2);

        assert!(store.record_status(Some(&status(78, CHARGING_DISCHARGING)), now + 31));
        assert!(store.flush(now + 32));
        assert_eq!(read_history(&path).samples.len(), 3);

        let _ = std::fs::remove_file(path);
    }

    #[test]
    fn failed_write_keeps_dirty_and_backs_off() {
        let parent = temp_path();
        std::fs::write(&parent, b"not a directory").expect("create blocking file");
        let path = parent.join("history.json");
        let mut store = HistoryStore::load(path.clone());
        let now = 1_700_000_000;

        assert!(store.record_status(Some(&status(80, CHARGING_DISCHARGING)), now));
        assert!(!store.flush(now));
        assert!(store.record_status(Some(&status(79, CHARGING_DISCHARGING)), now + 1));

        // 失败后 30 秒内不重试，避免每秒轮询持续重写。
        assert!(!store.flush_if_due(now + 2));
        std::fs::remove_file(&parent).expect("remove blocking file");
        assert!(store.flush_if_due(now + PERSIST_INTERVAL));
        assert_eq!(read_history(&path).samples.len(), 2);

        let _ = std::fs::remove_file(path);
        let _ = std::fs::remove_dir(parent);
    }

    #[test]
    fn loads_existing_camel_case_history() {
        let path = temp_path();
        let now = now_epoch();
        let json = format!(
            r#"{{"samples":[{{"t":{now},"battery":77,"charging":0,"link":2,"mode":6,"online":true}}],"events":[{{"t":{now},"kind":"low_battery","title":"低电","message":"请充电"}}]}}"#
        );
        std::fs::write(&path, json).expect("write legacy history");

        let store = HistoryStore::load(path.clone());
        let snapshot = store.snapshot();
        assert_eq!(snapshot.samples.len(), 1);
        assert_eq!(snapshot.samples[0].battery, Some(77));
        assert_eq!(snapshot.events.len(), 1);
        assert_eq!(snapshot.events[0].kind, "low_battery");

        let _ = std::fs::remove_file(path);
    }

    #[test]
    fn records_state_changes_immediately() {
        let path = temp_path();
        let mut store = HistoryStore::load(path.clone());
        let now = 1_700_000_000;

        assert!(store.record_status(Some(&status(80, CHARGING_DISCHARGING)), now));
        assert!(!store.record_status(Some(&status(80, CHARGING_DISCHARGING)), now + 30));
        assert_eq!(store.snapshot().samples.len(), 1);

        assert!(store.record_status(Some(&status(79, CHARGING_DISCHARGING)), now + 60));
        assert_eq!(store.snapshot().samples.len(), 2);

        assert!(store.record_status(Some(&status(79, CHARGING_CHARGING)), now + 90));
        let snapshot = store.snapshot();
        assert_eq!(snapshot.samples.len(), 3);
        assert_eq!(snapshot.samples[2].charging, CHARGING_CHARGING);

        let _ = std::fs::remove_file(path);
    }

    #[test]
    fn records_due_and_offline_samples() {
        let path = temp_path();
        let mut store = HistoryStore::load(path.clone());
        let now = 1_700_000_000;
        let online = status(80, CHARGING_DISCHARGING);

        assert!(store.record_status(Some(&online), now));
        assert!(!store.record_status(Some(&online), now + SAMPLE_INTERVAL - 1));
        assert!(store.record_status(Some(&online), now + SAMPLE_INTERVAL));
        assert!(store.record_status(None, now + SAMPLE_INTERVAL + 1));

        let _ = std::fs::remove_file(path);
    }

    #[test]
    fn records_events_with_a_limit() {
        let path = temp_path();
        let mut store = HistoryStore::load(path.clone());
        let now = 1_700_000_000;

        for index in 0..(MAX_EVENTS + 5) {
            assert!(store.record_event(
                "low_battery",
                "低电",
                &format!("第 {index} 条"),
                now + index as u64
            ));
        }

        let snapshot = store.snapshot();
        assert_eq!(snapshot.events.len(), MAX_EVENTS);
        assert_eq!(snapshot.events.last().unwrap().message, "第 104 条");

        let _ = std::fs::remove_file(path);
    }
}
