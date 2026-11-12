// SPDX-License-Identifier: GPL-2.0-or-later

//! 低电、充满、断连与重连通知状态机。

use std::collections::HashSet;
use std::time::{Duration, Instant};

use crate::status::{
    Status, CHARGING_CHARGING, CHARGING_DISCHARGING, CHARGING_FULL, FULL_BATTERY_MIN,
};

#[derive(Debug, Clone)]
pub(super) struct AppNotification {
    pub kind: &'static str,
    pub title: String,
    pub message: String,
}

pub(super) struct StatusTracker {
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
        let battery_full = status
            .battery
            .is_some_and(|battery| battery >= FULL_BATTERY_MIN);
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
                .update(Some(&status(18, CHARGING_DISCHARGING, LINK_CONNECTED)), now)
                .len(),
            1
        );
        assert_eq!(
            tracker
                .update(Some(&status(17, CHARGING_DISCHARGING, LINK_CONNECTED)), now)
                .len(),
            0
        );
        assert_eq!(
            tracker
                .update(Some(&status(14, CHARGING_DISCHARGING, LINK_CONNECTED)), now)
                .len(),
            1
        );
        assert_eq!(
            tracker
                .update(Some(&status(9, CHARGING_DISCHARGING, LINK_CONNECTED)), now)
                .len(),
            1
        );
        assert_eq!(
            tracker
                .update(Some(&status(4, CHARGING_DISCHARGING, LINK_CONNECTED)), now)
                .len(),
            1
        );
        assert_eq!(
            tracker
                .update(Some(&status(4, CHARGING_DISCHARGING, LINK_CONNECTED)), now)
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
                .update(Some(&status(10, CHARGING_DISCHARGING, LINK_CONNECTED)), now)
                .len(),
            1
        );
        assert_eq!(
            tracker
                .update(Some(&status(10, CHARGING_CHARGING, LINK_CONNECTED)), now)
                .len(),
            0
        );
        assert_eq!(
            tracker
                .update(Some(&status(10, CHARGING_DISCHARGING, LINK_CONNECTED)), now)
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
                .update(Some(&status(90, CHARGING_CHARGING, LINK_CONNECTED)), now)
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
                .update(
                    Some(&status(80, CHARGING_DISCHARGING, 3)),
                    start + Duration::from_secs(5)
                )
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
