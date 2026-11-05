// SPDX-License-Identifier: GPL-2.0-or-later

//! 上位机本地文件的读写工具。

use std::fs;
use std::path::Path;
use std::time::{SystemTime, UNIX_EPOCH};

use serde::Serialize;

pub fn now_epoch() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_secs())
        .unwrap_or(0)
}

/// 先写临时文件再替换，避免程序退出时留下半个 JSON。
pub fn write_json<T: Serialize>(path: &Path, value: &T) -> Result<(), String> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|error| format!("创建目录失败：{error}"))?;
    }
    let data = serde_json::to_vec(value).map_err(|error| format!("序列化失败：{error}"))?;
    let temp = path.with_extension("json.tmp");
    fs::write(&temp, data).map_err(|error| format!("写入临时文件失败：{error}"))?;
    if fs::rename(&temp, path).is_err() {
        // Windows 不允许 rename 覆盖已存在的文件，删掉旧文件后重试一次。
        let _ = fs::remove_file(path);
        fs::rename(&temp, path).map_err(|error| format!("替换文件失败：{error}"))?;
    }
    Ok(())
}
