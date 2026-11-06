// SPDX-License-Identifier: GPL-2.0-or-later

//! 电量采样与事件记录。数据只保存在本机应用数据目录。

use std::fs;
use std::path::PathBuf;

use serde::{Deserialize, Serialize};

use crate::status::Status;
use crate::storage::{now_epoch, write_json};

const SAMPLE_INTERVAL: u64 = 5 * 60;
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
    pub fn record_status(&mut self, status: Option<&Status>, now: u64) {
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
            return;
        }

        self.samples.push(sample);
        self.prune(now);
        self.persist();
    }

    pub fn record_event(&mut self, kind: &str, title: &str, message: &str, now: u64) {
        self.events.push(HistoryEvent {
            t: now,
            kind: kind.to_string(),
            title: title.to_string(),
            message: message.to_string(),
        });
        self.prune(now);
        self.persist();
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

    fn persist(&self) {
        let file = HistoryFile {
            samples: self.samples.clone(),
            events: self.events.clone(),
        };
        if let Err(error) = write_json(&self.path, &file) {
            log::warn!("保存电量历史失败：{error}");
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

    #[test]
    fn records_state_changes_immediately() {
        let path = temp_path();
        let mut store = HistoryStore::load(path.clone());
        let now = 1_700_000_000;

        store.record_status(Some(&status(80, CHARGING_DISCHARGING)), now);
        store.record_status(Some(&status(80, CHARGING_DISCHARGING)), now + 30);
        assert_eq!(store.snapshot().samples.len(), 1);

        store.record_status(Some(&status(79, CHARGING_DISCHARGING)), now + 60);
        assert_eq!(store.snapshot().samples.len(), 2);

        store.record_status(Some(&status(79, CHARGING_CHARGING)), now + 90);
        let snapshot = store.snapshot();
        assert_eq!(snapshot.samples.len(), 3);
        assert_eq!(snapshot.samples[2].charging, CHARGING_CHARGING);

        let _ = std::fs::remove_file(path);
    }

    #[test]
    fn records_events_with_a_limit() {
        let path = temp_path();
        let mut store = HistoryStore::load(path.clone());
        let now = 1_700_000_000;

        for index in 0..(MAX_EVENTS + 5) {
            store.record_event("low_battery", "低电", &format!("第 {index} 条"), now + index as u64);
        }

        let snapshot = store.snapshot();
        assert_eq!(snapshot.events.len(), MAX_EVENTS);
        assert_eq!(snapshot.events.last().unwrap().message, "第 104 条");

        let _ = std::fs::remove_file(path);
    }
}
