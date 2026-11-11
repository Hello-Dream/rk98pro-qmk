// SPDX-License-Identifier: GPL-2.0-or-later

use std::collections::VecDeque;
use std::sync::Mutex;
use std::time::{Duration, Instant};

use hidapi::{DeviceInfo, HidApi, HidDevice};

use crate::error::{ViaError, ViaResult};
use crate::status::Status;

const USAGE_PAGE: u16 = 0xFF60;
const USAGE: u16 = 0x61;
const REPORT_ID: u8 = 0;
const REPORT_SIZE: usize = 32;
const CMD_CUSTOM_GET_VALUE: u8 = 0x08;
const CHANNEL_KEYBOARD: u8 = 0;
const ID_HOST_STATUS: u8 = 0x22;
const EVENT_MAGIC: u8 = 0xE0;
const EVENT_STATUS: u8 = 0x01;

const DEFAULT_VID: u16 = 0x342D;
const DEFAULT_PID: u16 = 0xE451;

pub struct ViaDevice {
    device: HidDevice,
    timeout_ms: i32,
    events: Mutex<VecDeque<Vec<u8>>>,
}

fn enqueue_event(events: &mut VecDeque<Vec<u8>>, event: Vec<u8>) {
    events.push_back(event);
    while events.len() > 32 {
        events.pop_front();
    }
}

fn normalize_report(buffer: &[u8], read: usize) -> &[u8] {
    if read == REPORT_SIZE + 1 && buffer[0] == REPORT_ID {
        &buffer[1..1 + REPORT_SIZE]
    } else if read > REPORT_SIZE {
        &buffer[..REPORT_SIZE]
    } else {
        &buffer[..read]
    }
}

fn classify_open_error(message: String) -> ViaError {
    let lower = message.to_ascii_lowercase();
    let denied = [
        "permission",
        "access denied",
        "accessdenied",
        "udev",
        "not permitted",
        "operation not allowed",
    ]
    .iter()
    .any(|needle| lower.contains(needle));
    if denied {
        ViaError::AccessDenied(message)
    } else {
        ViaError::Hid(message)
    }
}

fn error_priority(error: &ViaError) -> u8 {
    match error {
        ViaError::Protocol(_) | ViaError::Unsupported(_) => 3,
        ViaError::AccessDenied(_) => 2,
        ViaError::Timeout(_) | ViaError::Hid(_) => 1,
        ViaError::NoDevice(_) => 0,
    }
}

fn error_with_context(error: ViaError, name: &str) -> ViaError {
    match error {
        ViaError::NoDevice(message) => ViaError::NoDevice(format!("{name}：{message}")),
        ViaError::AccessDenied(message) => ViaError::AccessDenied(format!("{name}：{message}")),
        ViaError::Hid(message) => ViaError::Hid(format!("{name}：{message}")),
        ViaError::Timeout(message) => ViaError::Timeout(format!("{name}：{message}")),
        ViaError::Unsupported(value) => ViaError::Unsupported(value),
        ViaError::Protocol(message) => ViaError::Protocol(format!("{name}：{message}")),
    }
}

fn keep_most_informative(best: &mut Option<ViaError>, error: ViaError) {
    if best
        .as_ref()
        .map(|current| error_priority(&error) > error_priority(current))
        .unwrap_or(true)
    {
        *best = Some(error);
    }
}

impl ViaDevice {
    pub fn open(api: &HidApi, info: &DeviceInfo, timeout_ms: i32) -> ViaResult<Self> {
        let device = info
            .open_device(api)
            .map_err(|error| classify_open_error(error.to_string()))?;
        Ok(Self {
            device,
            timeout_ms,
            events: Mutex::new(VecDeque::new()),
        })
    }

    pub fn pop_event(&self) -> Option<Vec<u8>> {
        self.events.lock().ok()?.pop_front()
    }

    /// 读取设备推送的状态事件；不会阻塞等待命令响应。
    pub fn poll_events(&self, timeout_ms: i32) -> ViaResult<()> {
        let deadline = Instant::now() + Duration::from_millis(timeout_ms.max(1) as u64);
        let mut first = true;

        loop {
            let remaining = deadline.saturating_duration_since(Instant::now());
            if remaining.is_zero() {
                break;
            }
            let wait_ms = if first {
                remaining.as_millis().min(i32::MAX as u128) as i32
            } else {
                1
            };
            let mut buffer = [0u8; 64];
            let read = self
                .device
                .read_timeout(&mut buffer, wait_ms.max(1))
                .map_err(|error| ViaError::Hid(error.to_string()))?;
            if read == 0 {
                break;
            }
            first = false;

            let response = normalize_report(&buffer, read);
            if response.is_empty() {
                continue;
            }
            if response[0] == EVENT_MAGIC && response.len() >= 2 && response[1] == EVENT_STATUS {
                if let Ok(mut events) = self.events.lock() {
                    enqueue_event(&mut events, response.to_vec());
                }
            }
        }
        Ok(())
    }

    fn read_custom_value(&self, value_id: u8) -> ViaResult<Vec<u8>> {
        let payload = [CMD_CUSTOM_GET_VALUE, CHANNEL_KEYBOARD, value_id];
        let mut request = [0u8; REPORT_SIZE + 1];
        request[0] = REPORT_ID;
        request[1..1 + payload.len()].copy_from_slice(&payload);
        self.device
            .write(&request)
            .map_err(|error| ViaError::Hid(error.to_string()))?;

        let deadline = Instant::now() + Duration::from_millis(self.timeout_ms.max(1) as u64);
        loop {
            let remaining = deadline.saturating_duration_since(Instant::now());
            if remaining.is_zero() {
                return Err(ViaError::Timeout(format!(
                    "等待状态值 0x{value_id:02X} 超时"
                )));
            }
            let mut buffer = [0u8; 64];
            let timeout_ms = remaining.as_millis().min(i32::MAX as u128) as i32;
            let read = self
                .device
                .read_timeout(&mut buffer, timeout_ms.max(1))
                .map_err(|error| ViaError::Hid(error.to_string()))?;
            if read == 0 {
                return Err(ViaError::Timeout(format!(
                    "等待状态值 0x{value_id:02X} 超时"
                )));
            }

            let response = normalize_report(&buffer, read);
            if response.is_empty() {
                continue;
            }
            if response[0] == EVENT_MAGIC {
                if let Ok(mut events) = self.events.lock() {
                    enqueue_event(&mut events, response.to_vec());
                }
                continue;
            }
            if response[0] == 0xFF {
                return Err(ViaError::Unsupported(value_id));
            }
            if response.len() < 3
                || response[0] != CMD_CUSTOM_GET_VALUE
                || response[1] != CHANNEL_KEYBOARD
                || response[2] != value_id
            {
                continue;
            }
            return Ok(response[3..].to_vec());
        }
    }

    pub fn get_status(&self) -> ViaResult<Status> {
        let value = self.read_custom_value(ID_HOST_STATUS)?;
        Status::parse(&value)
    }
}

fn is_via_interface(info: &DeviceInfo) -> bool {
    info.usage_page() == USAGE_PAGE && info.usage() == USAGE
}

fn is_known_rk98pro(info: &DeviceInfo) -> bool {
    info.vendor_id() == DEFAULT_VID && info.product_id() == DEFAULT_PID
}

fn normalized_device_text(value: &str) -> String {
    value.to_ascii_lowercase().replace([' ', '-', '_'], "")
}

fn looks_like_rk98pro(info: &DeviceInfo) -> bool {
    let product = normalized_device_text(info.product_string().unwrap_or(""));
    let manufacturer = normalized_device_text(info.manufacturer_string().unwrap_or(""));
    product.contains("rk98") || (product.is_empty() && manufacturer.contains("royalkludge"))
}

pub fn select_rk98pro_interfaces(api: &HidApi) -> Vec<DeviceInfo> {
    let all: Vec<DeviceInfo> = api
        .device_list()
        .filter(|info| is_via_interface(info))
        .cloned()
        .collect();
    let exact: Vec<DeviceInfo> = all
        .iter()
        .filter(|info| is_known_rk98pro(info))
        .cloned()
        .collect();
    if !exact.is_empty() {
        return exact;
    }
    all.into_iter().filter(looks_like_rk98pro).collect()
}

fn interface_name(info: &DeviceInfo) -> String {
    info.product_string()
        .filter(|value| !value.is_empty())
        .or_else(|| info.manufacturer_string().filter(|value| !value.is_empty()))
        .unwrap_or("HID 接口")
        .to_string()
}

pub fn find_monitor_device(prefer_2g4: bool, timeout_ms: i32) -> ViaResult<ViaDevice> {
    let api = HidApi::new().map_err(|error| ViaError::Hid(error.to_string()))?;
    let interfaces = select_rk98pro_interfaces(&api);
    if interfaces.is_empty() {
        return Err(ViaError::NoDevice(
            "未找到 RK98PRO 的 VIA HID 接口（usage_page=0xFF60, usage=0x61）。请确认键盘在 2.4G 或 USB 有线模式。".into(),
        ));
    }

    let mut online: Option<ViaDevice> = None;
    let mut fallback: Option<ViaDevice> = None;
    let mut best_error: Option<ViaError> = None;

    for info in &interfaces {
        let name = interface_name(info);
        match ViaDevice::open(&api, info, timeout_ms.max(1200)) {
            Ok(device) => match device.get_status() {
                Ok(status) => {
                    if prefer_2g4 && status.mode == crate::status::MODE_2G4 && status.online {
                        return Ok(device);
                    }
                    if status.online && online.is_none() {
                        online = Some(device);
                    } else if fallback.is_none() {
                        fallback = Some(device);
                    }
                }
                Err(error) => {
                    keep_most_informative(&mut best_error, error_with_context(error, &name))
                }
            },
            Err(error) => keep_most_informative(&mut best_error, error_with_context(error, &name)),
        }
    }

    online.or(fallback).ok_or_else(|| {
        best_error.unwrap_or_else(|| {
            ViaError::NoDevice("找到 HID 接口但没有只读状态响应，请重新插拔键盘或接收器".into())
        })
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn events_are_fifo_and_keep_latest_32() {
        let mut events = VecDeque::new();
        enqueue_event(&mut events, vec![1]);
        enqueue_event(&mut events, vec![2]);
        assert_eq!(events.pop_front(), Some(vec![1]));
        assert_eq!(events.pop_front(), Some(vec![2]));

        for index in 0..40u8 {
            enqueue_event(&mut events, vec![index]);
        }
        assert_eq!(events.len(), 32);
        assert_eq!(events.front(), Some(&vec![8]));
        assert_eq!(events.back(), Some(&vec![39]));
    }

    #[test]
    fn event_status_constant_is_used() {
        assert_eq!(EVENT_STATUS, 0x01);
    }

    #[test]
    fn open_errors_classify_permission_and_udev() {
        assert!(matches!(
            classify_open_error("Permission denied".to_string()),
            ViaError::AccessDenied(_)
        ));
        assert!(matches!(
            classify_open_error("could not open device: access denied".to_string()),
            ViaError::AccessDenied(_)
        ));
        assert!(matches!(
            classify_open_error("udev rule missing".to_string()),
            ViaError::AccessDenied(_)
        ));
        assert!(matches!(
            classify_open_error("No such device".to_string()),
            ViaError::Hid(_)
        ));
    }

    #[test]
    fn error_priority_prefers_specific_causes() {
        assert!(
            error_priority(&ViaError::Protocol("old".into()))
                > error_priority(&ViaError::AccessDenied("permission".into()))
        );
        assert!(
            error_priority(&ViaError::AccessDenied("permission".into()))
                > error_priority(&ViaError::Timeout("timeout".into()))
        );
        assert!(
            error_priority(&ViaError::Hid("hid".into()))
                > error_priority(&ViaError::NoDevice("missing".into()))
        );

        let mut best = None;
        keep_most_informative(&mut best, ViaError::NoDevice("missing".into()));
        keep_most_informative(&mut best, ViaError::Timeout("timeout".into()));
        keep_most_informative(&mut best, ViaError::AccessDenied("permission".into()));
        keep_most_informative(&mut best, ViaError::Protocol("old".into()));
        assert!(matches!(best.as_ref(), Some(ViaError::Protocol(_))));

        keep_most_informative(&mut best, ViaError::AccessDenied("later".into()));
        assert!(matches!(best.as_ref(), Some(ViaError::Protocol(_))));
    }
}
