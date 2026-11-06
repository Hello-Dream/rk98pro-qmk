// SPDX-License-Identifier: GPL-2.0-or-later

use serde::Serialize;

use crate::error::{ViaError, ViaResult};
use crate::storage::now_epoch;

pub const CHARGING_DISCHARGING: u8 = 0;
pub const CHARGING_CHARGING: u8 = 1;
pub const CHARGING_FULL: u8 = 2;
pub const CHARGING_UNKNOWN: u8 = 0xff;

pub const LINK_CONNECTED: u8 = 2;
pub const MODE_USB: u8 = 0;
pub const MODE_2G4: u8 = 6;
pub const FULL_BATTERY_MIN: u8 = 98;
pub const SNAPSHOT_VERSION: u8 = 1;

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Status {
    pub seq: u8,
    pub version: u8,
    pub firmware_version: String,
    pub settings_version: u8,
    pub battery: Option<u8>,
    pub charging: u8,
    pub link: u8,
    pub mode: u8,
    pub power_state: u8,
    pub flags: u8,
    pub locks: u8,
    pub layer: u8,
    pub default_layer: u8,
    pub wpm: u8,
    pub idle_seconds: u16,
    pub rgbrec_channel: Option<u8>,
    pub rgb_mode: u8,
    pub rgb_brightness: u8,
    pub wireless_version: u8,
    pub online: bool,
    pub updated_at: u64,
}

impl Status {
    pub fn parse(value: &[u8]) -> ViaResult<Self> {
        if value.len() < 23 {
            return Err(ViaError::Protocol(format!(
                "状态快照过短：{} / 23",
                value.len()
            )));
        }
        if value[0] == 0 {
            return Err(ViaError::Protocol("状态快照版本无效".into()));
        }
        if value[0] != SNAPSHOT_VERSION {
            return Err(ViaError::Protocol(format!(
                "状态快照版本 {} 不受支持，请更新固件",
                value[0]
            )));
        }

        let battery = if value[6] <= 100 { Some(value[6]) } else { None };
        let mut charging = if value[7] <= CHARGING_FULL {
            value[7]
        } else {
            CHARGING_UNKNOWN
        };
        if charging == CHARGING_FULL
            && (battery.is_none() || battery.unwrap_or(0) < FULL_BATTERY_MIN)
        {
            charging = CHARGING_CHARGING;
        }

        let mode = value[9];
        let link = value[8];
        let flags = value[11];
        let online = flags & 1 != 0 || mode == MODE_USB || link == LINK_CONNECTED;

        Ok(Self {
            seq: value[1],
            version: value[0],
            firmware_version: format!("{}.{}.{}", value[2], value[3], value[4]),
            settings_version: value[5],
            battery,
            charging,
            link,
            mode,
            power_state: value[10],
            flags,
            locks: value[12],
            layer: value[13],
            default_layer: value[14],
            wpm: value[15],
            idle_seconds: u16::from_le_bytes([value[16], value[17]]),
            rgbrec_channel: if value[18] <= 3 { Some(value[18]) } else { None },
            rgb_mode: value[19],
            rgb_brightness: value[20],
            wireless_version: value[21],
            online,
            updated_at: now_epoch(),
        })
    }

    pub fn summary(&self) -> String {
        if !self.online {
            return "等待键盘连接".to_string();
        }

        let mut parts = vec![format!(
            "{} {}",
            mode_label(self.mode),
            link_label(self.link)
        )];
        if let Some(battery) = self.battery {
            parts.push(format!("电量 {battery}%"));
        }
        if self.charging == CHARGING_CHARGING {
            parts.push("充电中".to_string());
        } else if self.charging == CHARGING_FULL {
            parts.push("已充满".to_string());
        }
        parts.join(" · ")
    }
}

pub fn mode_label(mode: u8) -> &'static str {
    match mode {
        0 => "USB",
        1 => "BT1",
        2 => "BT2",
        3 => "BT3",
        6 => "2.4G",
        _ => "未知",
    }
}

pub fn link_label(link: u8) -> &'static str {
    match link {
        0 => "未连接",
        1 => "配对中",
        2 => "已连接",
        3 => "已断开",
        4 => "被拒绝",
        _ => "未知",
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn snapshot(battery: u8, charging: u8) -> Vec<u8> {
        let mut value = vec![0u8; 23];
        value[0] = SNAPSHOT_VERSION;
        value[1] = 7;
        value[2] = 1;
        value[3] = 0;
        value[4] = 1;
        value[5] = 1;
        value[6] = battery;
        value[7] = charging;
        value[8] = 2;
        value[9] = 6;
        value[10] = 0;
        value[11] = 1;
        value[12] = 1;
        value[13] = 2;
        value[14] = 0;
        value[15] = 64;
        value[16] = 3;
        value[17] = 0;
        value[18] = 0xff;
        value[19] = 6;
        value[20] = 90;
        value[21] = 1;
        value
    }

    #[test]
    fn parses_snapshot() {
        let status = Status::parse(&snapshot(82, CHARGING_DISCHARGING)).unwrap();
        assert_eq!(status.seq, 7);
        assert_eq!(status.version, SNAPSHOT_VERSION);
        assert_eq!(status.firmware_version, "1.0.1");
        assert_eq!(status.settings_version, 1);
        assert_eq!(status.battery, Some(82));
        assert_eq!(status.mode, MODE_2G4);
        assert_eq!(status.layer, 2);
        assert_eq!(status.wpm, 64);
        assert_eq!(status.idle_seconds, 3);
        assert_eq!(status.rgbrec_channel, None);
        assert!(status.online);
        assert_eq!(status.summary(), "2.4G 已连接 · 电量 82%");
    }

    #[test]
    fn downgrades_full_below_threshold() {
        let status = Status::parse(&snapshot(93, CHARGING_FULL)).unwrap();
        assert_eq!(status.charging, CHARGING_CHARGING);
        assert!(status.summary().contains("充电中"));
        assert!(!status.summary().contains("已充满"));
    }

    #[test]
    fn keeps_full_at_threshold() {
        let status = Status::parse(&snapshot(98, CHARGING_FULL)).unwrap();
        assert_eq!(status.charging, CHARGING_FULL);
    }

    #[test]
    fn rejects_short_snapshot() {
        assert!(Status::parse(&[0u8; 22]).is_err());
    }

    #[test]
    fn rejects_invalid_snapshot_version() {
        let mut value = snapshot(82, CHARGING_DISCHARGING);
        value[0] = 0;
        let error = Status::parse(&value).unwrap_err().to_string();
        assert!(error.contains("状态快照版本无效"));
    }

    #[test]
    fn rejects_unsupported_snapshot_version() {
        let mut value = snapshot(82, CHARGING_DISCHARGING);
        value[0] = 3;
        let error = Status::parse(&value).unwrap_err().to_string();
        assert!(error.contains("状态快照版本 3 不受支持，请更新固件"));
    }
}
