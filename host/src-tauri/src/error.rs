// SPDX-License-Identifier: GPL-2.0-or-later

use std::fmt;

#[derive(Debug, Clone)]
pub enum ViaError {
    NoDevice(String),
    Hid(String),
    Timeout(String),
    Unsupported(u8),
    Protocol(String),
}

impl fmt::Display for ViaError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::NoDevice(message) => write!(f, "{message}"),
            Self::Hid(message) => write!(f, "HID 错误：{message}"),
            Self::Timeout(message) => write!(f, "{message}"),
            Self::Unsupported(value) => write!(f, "固件不支持 0x{value:02X}"),
            Self::Protocol(message) => write!(f, "协议错误：{message}"),
        }
    }
}

impl std::error::Error for ViaError {}

pub type ViaResult<T> = Result<T, ViaError>;
