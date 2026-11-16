# RK98PRO 三模 QMK 固件与上位机 v1.0.4

> **稳定版**：面向 v1.0.3 用户的固件运行时状态封装与协议契约校验版本。固件、VIA 定义与上位机版本号统一为 `1.0.4`。

本次版本不改变键盘配置、状态快照协议或 EEPROM 布局，重点是移除固件模块间的 `extern` 全局状态、把 C/Rust/TS 三端状态协议纳入 CI 契约校验，并命名充电状态常量。

---

## 本次变更

### 固件运行时状态封装

- `hs/indicators`、`hs/rgb`、`hs/wireless` 等模块的 `extern` 全局变量改为访问器函数，调用方不再直接读写共享状态。
- 状态灯、电量强制显示、无线指示、RGB 录制标记与侧灯模式分别由所属模块提供接口，读写路径和副作用保持原行为。
- 充电状态改用 `HS_CHARGING_DISCHARGING`、`HS_CHARGING_CHARGING`、`HS_CHARGING_FULL`、`HS_CHARGING_UNKNOWN` 命名常量，快照字节值不变。

### 协议契约校验

- 新增 `scripts/check_status_protocol.py`，校验 23 字节快照的 C 布局与 `_Static_assert`、关键字段赋值、raw HID 事件偏移、电池满电阈值和 VIA `0x22` 分发。
- 同一脚本校验 Rust `Status` 字段类型与 `value[N]` 映射、`online` 推导、`FULL` 降级，以及 TS 字段、常量和 `effectiveCharging`，防止三端协议漂移。
- CI 的 verify 任务在依赖安装前运行该脚本，AGENTS.md 的提交前校验同步加入。

### 兼容性

- 不修改 EEPROM 布局、`HS_SETTINGS_VERSION`、`HS_STATUS_SNAPSHOT_VERSION`、VIA ID、键码顺序、灯效顺序或 `process_record_kb` 的调用链。
- 状态快照仍为 23 字节 v1，固件、VIA 定义和上位机安装包继续使用只读状态通道 `0x22`。

---

## 请重点验证

1. **三模连接**：USB、BT1–BT3、2.4G 的切换、配对、重连与连接提示。
2. **休眠与复位**：空闲调光/关灯、睡眠唤醒、`EE_CLR`、Bootloader 入口。
3. **按键与 VIA**：普通按键、Fn 组合、鼠标模式、动态宏、VIA 改键和设置读写。
4. **电源与电量**：充电/充满提示、低电告警与亮度限制、电量条显示。
5. **RGB**：内置/自定义灯效、侧灯状态提示、RGB 录制与 `RGBR_PLAY` 预览。
6. **上位机**：状态、电量、历史、WPM、托盘摘要和通知。

---

## 刷入说明

- 从 v1.0.3 升级不需要手动清空 EEPROM；`HS_SETTINGS_VERSION` 仍为 `1`。
- 固件、VIA 定义和上位机安装包均在本 Release 附件中。
- Linux deb/rpm 会自动安装 udev 规则；AppImage 用户需要按 README 手动安装规则并重新插拔设备。

## 下载

- `royalkludge_rk98pro_us_triple_mode_default.bin`：刷入键盘的固件。
- `rk98pro_us_triple_mode.json`：VIA 定义。
- Windows 安装包 / 便携版：x64 与 ARM64。
- Linux AppImage / deb / rpm：amd64 与 arm64。

完整变更见 [CHANGELOG.md](CHANGELOG.md)，协议说明见 [host/README.md](host/README.md)。
