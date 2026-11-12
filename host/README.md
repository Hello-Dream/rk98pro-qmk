# RK98PRO Monitor v1.0.2

RK98PRO 的只读桌面监控。上位机只显示电量、键盘功能状态和本机通知，不写键盘；键位、灯效、侧灯模式、电源和 RGB 录制仍由 VIA 配置。`Side Light` 持久模式当前只保存模式值，不驱动持久灯光；充电、连接和低电状态提示仍由固件通过 RGB Matrix 使用侧灯。

## 功能

- 电量、充电、链路、模式、电源阶段、层、锁定键、WPM、RGB/录制状态。
- 6 小时 / 24 小时 / 7 天电量历史，显示平滑并保留离线空档；历史按 30 秒防抖落盘，正常退出前刷新。
- WPM 当前、峰值、5 分钟平均和 5 分钟曲线；统计只保存在本机内存，可重置。
- 低电、充满、断连/重连桌面通知，可分别关闭。
- 托盘电池图标、状态摘要、打开面板、立即刷新和退出。
- 找设备失败时显示可操作诊断：旧固件不支持、Linux 权限/udev、未找到设备或 HID 访问异常。

## 不做

- 不提供改键、灯光编辑、键盘设置、无线切换、备份恢复或诊断写入。
- 不发送任何键盘写命令；状态通道只实现 GET。
- 不支持蓝牙模式下的状态读取（蓝牙没有 raw HID 通道）。

## VIA

上位机不提供键盘配置写入；键位、灯效、侧灯模式、电源和 RGB 录制仍由 VIA 修改。VIA 的 `System` 菜单分为：

- `Lighting → Side Light`：保存 6 颗侧灯的模式值；当前构建未启用持久侧灯驱动，该选项只保存模式。充电、连接和低电状态提示仍由固件通过 RGB Matrix 使用侧灯。
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
    ├── monitor/
    │   ├── mod.rs          共享状态与命令侧方法
    │   ├── tracker.rs      低电/充满/断连通知状态机
    │   ├── runner.rs       心跳、事件循环与重连
    │   └── publisher.rs    状态、历史、WPM 发布与落盘
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
pnpm exec playwright install --with-deps chromium
pnpm test:e2e
cargo fmt --manifest-path src-tauri/Cargo.toml -- --check
cargo clippy --manifest-path src-tauri/Cargo.toml --locked --all-targets -- -D warnings
cargo test --manifest-path src-tauri/Cargo.toml
```

固件电源/电池纯逻辑测试在仓库根目录运行：

```bash
cc -std=c11 -Wall -Wextra -Werror \
  -I keyboards/royalkludge/rk98pro_us_triple_mode/hs \
  keyboards/royalkludge/rk98pro_us_triple_mode/hs/battery_policy.c \
  tests/firmware/test_battery_policy.c -o /tmp/test_battery_policy
/tmp/test_battery_policy
cc -std=c11 -Wall -Wextra -Werror \
  -I keyboards/royalkludge/rk98pro_us_triple_mode/hs \
  keyboards/royalkludge/rk98pro_us_triple_mode/hs/power_policy.c \
  tests/firmware/test_power_policy.c -o /tmp/test_power_policy
/tmp/test_power_policy
```

## Linux 安装与 HID 权限

- deb / rpm 会把 `99-rk98pro.rules` 安装到 `/usr/lib/udev/rules.d/`，安装后重新插拔键盘或接收器即可。
- AppImage 不会自动安装系统规则，需要手动执行：

```bash
sudo cp 99-rk98pro.rules /etc/udev/rules.d/99-rk98pro.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

然后重新插拔键盘或 2.4G 接收器。规则文件位于仓库 `host/packaging/99-rk98pro.rules`，匹配 VID `342d`、PID `e451`，并给本地活动会话用户授予 hidraw 访问权限（`TAG+="uaccess"`）。

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
[0]  version             快照协议版本，当前 1（上位机只接受 1）
[1]  seq                 状态变化序号
[2]  fw_major            固件发布版本主版本（与 keyboard.json version 一致，当前 1.0.2）
[3]  fw_minor            固件发布版本次版本
[4]  fw_patch            固件发布版本修订号
[5]  settings_version    HS_SETTINGS_VERSION（EEPROM 设置布局版本，当前 1）
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

`fw_*` 仅用于显示固件发布版本，不用于协议兼容判断；协议兼容性看 `version`（快照协议版本）。`settings_version` 是 EEPROM 设置布局版本，与发布版本无关。上位机只接受快照版本 `1`；快照版本 `3` 的旧固件会被拒绝并提示更新固件。

### 事件

```text
[0] 0xE0
[1] 0x01（状态事件）
[2] 23
[3..25] snapshot
```

GET `0x22` 同时把主机会话续期 60 秒。只有会话有效时固件才推送事件；主机停止请求 60 秒后停止推送。

事件触发字段：充电、链路、模式、电源阶段、flags、锁定键、层、录制通道、RGB 模式/亮度；电量变化在 60 秒节流窗口允许时推送。其他状态事件会携带最新电量，WPM 和空闲秒数不单独触发事件。

上位机处理规则：

- GET 响应永远采纳，并把 `seq` 设为新基线；
- 事件只在 `seq` 比基线新时采纳；
- 事件丢失时由下一次心跳 GET 校正；
- 窗口聚焦时电量在 5 秒内通过事件或 1 秒心跳校正；失焦或托盘隐藏时由 30 秒心跳校正。

### 心跳

- 窗口聚焦：每 1 秒一次 GET，同时为 WPM 提供采样；
- 窗口失焦或托盘隐藏：每 30 秒一次 GET；
- 事件轮询：聚焦时最多等待 200ms，失焦/托盘时最多 1 秒；立即刷新会打断等待；
- `status-updated` 是 UI 主路径，React Query 的 30 秒读取只作事件丢失/重连兜底；
- `useWpmStats` 的 2 秒查询只读本机内存，不产生 HID 或磁盘 I/O，托盘态保留该节拍；
- 断连判定：连续 60 秒无响应。

状态 GET 不重置空闲计时器、无线 blink 计时器，也不按用户活动处理；监控不会阻止键盘调光、关灯或休眠。

### 只读约束

Tauri 命令只有：

```text
get_status
get_monitor_error
refresh
get_history
get_wpm_stats
reset_wpm_stats
get_preferences
set_preferences
test_notification
```

`reset_wpm_stats` 和 `set_preferences` 只写本机内存/JSON，不写键盘。`get_monitor_error` 与 `monitor-error` 事件只读本机诊断状态，不改变协议；`status-updated` payload 保持不变。

## 故障排查

- **旧固件**：提示“固件不支持只读状态通道”或快照版本不兼容时，刷入与上位机同版本的固件；不要降级上位机硬连旧快照。
- **Linux 权限/udev**：提示无法访问 HID 接口时，确认 deb/rpm 已安装规则，或按上面的 AppImage 步骤手动安装；运行 `sudo udevadm control --reload-rules` 后重新插拔。
- **未找到设备**：确认使用 USB 有线或 2.4G 接收器；蓝牙模式没有 raw HID。接收器未插入、键盘处于蓝牙模式或接口被其他程序独占时都会显示该提示。
- **HID 超时**：重新插拔键盘/接收器；若仍失败，检查是否有其他 VIA/监控程序占用 `usage_page=0xFF60, usage=0x61` 接口。

## 已知限制

- 蓝牙模式没有 raw HID，上位机显示离线；请使用 USB 或 2.4G。
- WPM 在窗口聚焦时按 1 秒采样；失焦或托盘隐藏时降为 30 秒，曲线会变稀疏。
- 历史按 30 秒防抖落盘；存储可写且进程持续调度时，异常退出最多丢失 30 秒未落盘采样。
- 充满判定由固件统一处理；上位机在电量低于 98% 时会把 `FULL` 降级为充电中作为第二道防线。
