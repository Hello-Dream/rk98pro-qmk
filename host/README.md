# RK98PRO Monitor v1.0.1

RK98PRO 的只读桌面监控。上位机只显示电量、键盘功能状态和本机通知，不写键盘；键位、灯效、侧灯、电源和 RGB 录制仍由 VIA 配置。

## 功能

- 电量、充电、链路、模式、电源阶段、层、锁定键、WPM、RGB/录制状态。
- 6 小时 / 24 小时 / 7 天电量历史，显示平滑并保留离线空档。
- WPM 当前、峰值、5 分钟平均和 5 分钟曲线；统计只保存在本机内存，可重置。
- 低电、充满、断连/重连桌面通知，可分别关闭。
- 托盘电池图标、状态摘要、打开面板、立即刷新和退出。

## 不做

- 不提供改键、灯光编辑、键盘设置、无线切换、备份恢复或诊断写入。
- 不发送任何键盘写命令；状态通道只实现 GET。
- 不支持蓝牙模式下的状态读取（蓝牙没有 raw HID 通道）。

## VIA

上位机不提供键盘配置写入；键位、灯效、侧灯、电源和 RGB 录制仍由 VIA 修改。VIA 的 `System` 菜单分为：

- `System → General`：睡眠超时、防连击、电量/连接状态和“在键盘上显示电量”。
- `System → Power`：空闲调光/关灯、USB 省电、低电阈值与低电亮度、状态灯和电源状态。

## 开发

前端使用 React 19 + Vite + TypeScript strict + Tailwind CSS 4，Rust 侧使用 Tauri 2 + hidapi。

```text
host/
├── src/
│   ├── components/         应用外壳与基础 UI
│   ├── features/
│   │   ├── overview/       状态仪表盘
│   │   ├── battery/        电量历史与显示平滑
│   │   ├── wpm/            WPM 统计
│   │   └── notifications/  通知设置
│   ├── lib/api.ts          唯一 Tauri invoke 入口
│   ├── lib/queries.ts      TanStack Query
│   ├── lib/types.ts        状态快照类型与标签
│   └── store/ui.ts         Toast
└── src-tauri/src/
    ├── device.rs           打开 VIA HID、GET 0x22、读取 0xE0 事件
    ├── status.rs           快照解析与显示归一化
    ├── monitor.rs          心跳、事件循环、通知、历史采样
    ├── wpm.rs              WPM 统计
    ├── history.rs          电量采样与事件
    ├── preferences.rs      通知偏好
    └── tray.rs / icon.rs / storage.rs
```

### 验证

```bash
cd host
pnpm lint
pnpm typecheck
pnpm test
pnpm build
cargo test --manifest-path src-tauri/Cargo.toml
```

## 协议

传输使用 VIA raw HID：32 字节报告，`usage_page=0xFF60`、`usage=0x61`。2.4G 模式由接收器透传；USB 模式由键盘自身提供。

上位机只发送一个只读请求：

```text
[CMD_CUSTOM_GET_VALUE, 0, 0x22]
```

固件不实现 `0x22` 的 SET；如果收到写请求会返回 `unhandled`。VIA 自己的 `0x10`–`0x21` 配置 ID 不受影响。

### 状态快照

GET 响应的 `value_data[3..]` 和事件 payload 使用同一份 23 字节快照：

```text
[0]  version             快照布局版本
[1]  seq                 状态变化序号
[2]  fw_major
[3]  fw_minor
[4]  fw_patch
[5]  settings_version    HS_SETTINGS_VERSION
[6]  battery             0~100，0xFF 未知
[7]  charging            0=放电 1=充电 2=充满 0xFF 未知
[8]  link                0=none 1=pairing 2=connected 3=disconnected 4=reject
[9]  mode                0=USB 1~3=BT 6=2.4G
[10] power_state         0=Active 1=Charging 2=Full 3=Low 4=Dim 5=Off
[11] flags               online/low_bat/usb/rgbrec/rgb_on/anti_bounce/mac
[12] locks               caps/num/scroll/win/caps_word
[13] layer
[14] default_layer
[15] wpm
[16] idle_seconds_lo
[17] idle_seconds_hi
[18] rgbrec_channel      0~3，0xFF 未录制
[19] rgb_mode
[20] rgb_brightness
[21] wireless_version
[22] reserved
```

### 事件

```text
[0] 0xE0
[1] 0x01（状态事件）
[2] 23
[3..25] snapshot
```

GET `0x22` 同时把主机会话续期 60 秒。只有会话有效时固件才推送事件；主机停止请求 60 秒后停止推送。

事件触发字段：电量（60 秒节流）、充电、链路、模式、电源阶段、flags、锁定键、层、录制通道、RGB 模式/亮度。WPM 和空闲秒数不单独触发事件。

上位机处理规则：

- GET 响应永远采纳，并把 `seq` 设为新基线；
- 事件只在 `seq` 比基线新时采纳；
- 事件丢失时由下一次心跳 GET 校正。

### 心跳

- 窗口可见：每 1 秒一次 GET，同时为 WPM 提供采样；
- 托盘隐藏：每 30 秒一次 GET；
- 断连判定：连续 60 秒无响应。

状态 GET 不重置空闲计时器、无线 blink 计时器，也不按用户活动处理；监控不会阻止键盘调光、关灯或休眠。

### 只读约束

Tauri 命令只有：

```text
get_status
refresh
get_history
get_wpm_stats
reset_wpm_stats
get_preferences
set_preferences
test_notification
```

`reset_wpm_stats` 和 `set_preferences` 只写本机内存/JSON，不写键盘。

## 已知限制

- 蓝牙模式没有 raw HID，上位机显示离线；请使用 USB 或 2.4G。
- WPM 在窗口打开时按 1 秒采样；托盘隐藏时降为 30 秒，曲线会变稀疏。
- 充满判定由固件统一处理；上位机在电量低于 98% 时会把 `FULL` 降级为充电中作为第二道防线。
