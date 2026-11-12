// SPDX-License-Identifier: GPL-2.0-or-later

//! 后台轮询、低电/充满/断连通知与状态发布。

mod publisher;
mod runner;
mod tracker;

pub use runner::start;

use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::Sender;
use std::sync::Mutex;

use crate::error::ViaError;
use crate::history::{HistorySnapshot, HistoryStore};
use crate::preferences::{PreferenceStore, Preferences};
use crate::status::Status;
use crate::storage::now_epoch;
use crate::wpm::{WpmStats, WpmTracker};

#[derive(Debug, Clone, PartialEq, Eq, serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct MonitorError {
    pub kind: String,
    pub message: String,
}

impl MonitorError {
    /// 把底层 HID/协议错误转换为可操作的界面提示。
    fn from_via(error: &ViaError) -> Self {
        match error {
            ViaError::Unsupported(value) => Self {
                kind: "unsupported".to_string(),
                message: format!(
                    "固件不支持只读状态通道 0x{value:02X}，请更新固件后再连接。"
                ),
            },
            ViaError::Protocol(message) => Self {
                kind: "unsupported".to_string(),
                message: format!("固件状态协议不兼容：{message}。请更新固件。"),
            },
            ViaError::AccessDenied(detail) => Self {
                kind: "access_denied".to_string(),
                message: format!(
                    "无法访问键盘 HID 接口（权限不足）。请安装或检查 udev 规则，运行 `sudo udevadm control --reload-rules` 后重新插拔设备。详情：{detail}"
                ),
            },
            ViaError::NoDevice(detail) => Self {
                kind: "not_found".to_string(),
                message: format!(
                    "未找到键盘的只读 HID 接口。请使用 USB 有线或 2.4G 接收器；蓝牙模式不提供 raw HID。详情：{detail}"
                ),
            },
            ViaError::Hid(detail) | ViaError::Timeout(detail) => Self {
                kind: "hid".to_string(),
                message: format!("HID 访问异常：{detail}。请重新插拔键盘或接收器。"),
            },
        }
    }
}

#[derive(Debug)]
pub enum Action {
    Refresh,
}

pub struct MonitorState {
    pub status: Mutex<Option<Status>>,
    pub history: Mutex<HistoryStore>,
    pub preferences: Mutex<PreferenceStore>,
    wpm: Mutex<WpmTracker>,
    window_active: AtomicBool,
    last_error: Mutex<Option<MonitorError>>,
    tx: Mutex<Sender<Action>>,
}

impl MonitorState {
    pub fn new(tx: Sender<Action>, data_dir: PathBuf) -> Self {
        Self {
            status: Mutex::new(None),
            history: Mutex::new(HistoryStore::load(data_dir.join("history.json"))),
            preferences: Mutex::new(PreferenceStore::load(data_dir.join("preferences.json"))),
            wpm: Mutex::new(WpmTracker::new()),
            window_active: AtomicBool::new(true),
            last_error: Mutex::new(None),
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

    pub fn set_window_active(&self, active: bool) {
        self.window_active.store(active, Ordering::SeqCst);
    }

    pub fn window_active(&self) -> bool {
        self.window_active.load(Ordering::SeqCst)
    }

    pub fn monitor_error(&self) -> Option<MonitorError> {
        self.last_error.lock().ok().and_then(|error| error.clone())
    }

    /// 写入诊断错误；相同内容不重复触发事件，瞬时 HID 错误不覆盖旧固件/权限错误。
    /// 返回是否需要向界面推送 `monitor-error`。
    pub fn set_monitor_error(&self, error: Option<MonitorError>) -> bool {
        let Ok(mut current) = self.last_error.lock() else {
            return false;
        };
        if let (Some(next), Some(previous)) = (error.as_ref(), current.as_ref()) {
            let protected = matches!(previous.kind.as_str(), "unsupported" | "access_denied");
            if next.kind == "hid" && protected {
                return false;
            }
        }
        if *current == error {
            return false;
        }
        *current = error;
        true
    }

    pub fn flush_history(&self) {
        if let Ok(mut history) = self.history.lock() {
            history.flush(now_epoch());
        }
    }

    pub fn send(&self, action: Action) -> Result<(), String> {
        self.tx
            .lock()
            .map_err(|_| "内部状态锁失败".to_string())?
            .send(action)
            .map_err(|_| "监视线程已停止".to_string())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn monitor_error_deduplicates_and_protects_protocol_errors() {
        let (tx, _rx) = std::sync::mpsc::channel();
        let state = MonitorState::new(tx, std::env::temp_dir().join("rk98pro-monitor-error-test"));

        let protocol = MonitorError {
            kind: "unsupported".to_string(),
            message: "旧固件".to_string(),
        };
        assert!(state.set_monitor_error(Some(protocol.clone())));
        assert!(!state.set_monitor_error(Some(protocol.clone())));

        let transient = MonitorError {
            kind: "hid".to_string(),
            message: "超时".to_string(),
        };
        assert!(!state.set_monitor_error(Some(transient)));
        assert_eq!(state.monitor_error(), Some(protocol));

        assert!(state.set_monitor_error(None));
        assert!(!state.set_monitor_error(None));
        assert_eq!(state.monitor_error(), None);
    }
}
