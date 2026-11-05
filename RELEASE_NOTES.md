# RK98PRO 三模 QMK 固件与上位机 v1.0.0

Royal Kludge **RK98PRO**（98 键 / 100 配列）的 QMK 固件与只读桌面监控，支持 USB / 蓝牙 ×3 / 2.4G 三模、RGB Matrix 背光、按键 RGB 录制与 VIA 改键。

v1.0.0 是项目的首个版本：固件提供只读状态通道，上位机只做监控，VIA 作为唯一配置入口。

---

## 主要功能

### 固件

- 三模连接：USB / 蓝牙 ×3 / 2.4G，支持无线配对、通道切换与状态提示。
- 98% 配列（100 键）RGB Matrix 背光 + 6 颗侧灯；20 个内置灯效、7 个自定义灯效与按键 RGB 录制。
- 6 层默认键位，Windows / macOS 双布局；VIA 改键、灯效、侧灯、电源与录制配置。
- 只读状态通道 `0x22`：固定 23 字节快照，GET 续期 60 秒主机会话，状态变化通过 raw HID 事件推送。
- 电量消抖与平滑：充电线 500ms、满电引脚 2s、5 点中值 + EMA；充满判定要求稳定满电信号和接近满值的电量。
- 未知电量保持未知，不显示为 100%。
- 电源管理：空闲 30 秒调光、2 分钟关闭 RGB 与侧灯；低电 15% 限制亮度并提示；3% 进入保护深睡。
- 防连击短按键过滤（`Fn + Z`）、10ms 去抖、鼠标模式、动态宏与 Caps Word。
- 固件按 `hs/status`、`hs/power`、`hs/battery`、`hs/indicators`、`hs/keys`、`hs/rgb`、`hs/wireless`、`hs/via`、`hs/store` 模块拆分。
- VIA 自定义 ID `0x10`–`0x21`，与 `rk98pro_us_triple_mode.json` 配合使用。

### 上位机

- 只读仪表盘：电量/充电、模式/链路、电源阶段、层、锁定键、WPM、RGB/录制状态。
- 6 小时 / 24 小时 / 7 天电量历史，曲线平滑并在离线处断开。
- WPM 当前、峰值、5 分钟平均和 5 分钟曲线，可手动重置。
- 低电、充满、断连/重连桌面通知，可分别开关。
- 托盘电池图标、状态摘要、打开面板、立即刷新和退出。
- 只发送只读的 `GET 0x22`，不写键盘。

### VIA

- `System` 菜单分为 `General` 和 `Power`，分别管理键盘行为与电源策略。
- 键位、灯效、侧灯、电源和 RGB 录制由 VIA 配置。
- `System → General` 提供睡眠超时、防连击、电量/连接状态和“在键盘上显示电量”。

---

## 刷入说明

- 首次刷入本固件会初始化自定义设置、RGB 录制和动态键位；如有自定义键位，建议先在 VIA 中记录。
- VIA 自定义 ID `0x10`–`0x21` 和 `rk98pro_us_triple_mode.json` 可直接使用。
- 上位机通过 USB 或 2.4G raw HID 读取状态；蓝牙模式没有 raw HID，上位机显示离线。
- 状态读取不会重置键盘空闲计时器，不会阻止调光、关灯或休眠。

---

## 下载

- `royalkludge_rk98pro_us_triple_mode_default.bin`：刷入键盘的固件。
- `rk98pro_us_triple_mode.json`：VIA 定义。
- Windows 安装包 / 便携版：x64 与 ARM64。
- Linux AppImage / deb / rpm：amd64 与 arm64。

以上附件均在本 Release 中公开下载。

完整变更见 [CHANGELOG.md](CHANGELOG.md)，协议说明见 [host/README.md](host/README.md)。
