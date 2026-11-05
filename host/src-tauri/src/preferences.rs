// SPDX-License-Identifier: GPL-2.0-or-later

//! 上位机通知偏好，保存在应用数据目录。

use std::fs;
use std::path::PathBuf;

use serde::{Deserialize, Serialize};

use crate::storage::write_json;

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", default)]
pub struct Preferences {
    pub notify_low_battery: bool,
    pub notify_full: bool,
    pub notify_disconnect: bool,
}

impl Default for Preferences {
    fn default() -> Self {
        Self {
            notify_low_battery: true,
            notify_full: true,
            notify_disconnect: true,
        }
    }
}

impl Preferences {
    pub fn allows(&self, kind: &str) -> bool {
        match kind {
            "low_battery" => self.notify_low_battery,
            "full" => self.notify_full,
            "disconnect" | "reconnect" => self.notify_disconnect,
            _ => true,
        }
    }
}

pub struct PreferenceStore {
    path: PathBuf,
    value: Preferences,
}

impl PreferenceStore {
    pub fn load(path: PathBuf) -> Self {
        let value = fs::read(&path)
            .ok()
            .and_then(|bytes| serde_json::from_slice::<Preferences>(&bytes).ok())
            .unwrap_or_default();
        Self { path, value }
    }

    pub fn get(&self) -> Preferences {
        self.value.clone()
    }

    pub fn set(&mut self, value: Preferences) -> Result<(), String> {
        self.value = value;
        write_json(&self.path, &self.value)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn notification_switches_are_independent() {
        let preferences = Preferences {
            notify_low_battery: false,
            notify_full: true,
            notify_disconnect: false,
        };
        assert!(!preferences.allows("low_battery"));
        assert!(preferences.allows("full"));
        assert!(!preferences.allows("disconnect"));
        assert!(!preferences.allows("reconnect"));
        assert!(preferences.allows("unknown"));
    }

    #[test]
    fn defaults_enable_all_notifications() {
        let preferences = Preferences::default();
        assert!(preferences.allows("low_battery"));
        assert!(preferences.allows("full"));
        assert!(preferences.allows("disconnect"));
    }
}
