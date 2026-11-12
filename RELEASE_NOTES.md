# RK98PRO 三模 QMK 固件与上位机 v1.0.3

> **稳定版**：面向 v1.0.2 用户的固件模块拆分与内部结构调整版本。固件、VIA 定义与上位机版本号统一为 `1.0.3`。

本次版本不改变键盘配置、状态快照协议或 EEPROM 布局，重点是按职责拆分固件板级、wireless 和 power 模块，并把电源策略纯逻辑纳入主机测试；同时完成上位机 monitor 模块拆分。

---

## 本次变更

### 固件结构

- 板级文件 `rk98pro_us_triple_mode.c` 缩为 QMK hook 接线，GPIO/启动编排、设置读写、按键处理、会话和主循环分别迁入 `hs/board_init`、`hs/settings`、`hs/key_handlers`、`hs/session`、`hs/runtime`。
- `hs/wireless.c` 拆为 `wireless_mode`、`wireless_blink`、`wireless_keys`、`wireless_sleep`、`wireless_indicator`，原文件只保留 base glue 与共享状态。
- `hs/power.c` 拆出 `power_config.c`（VIA 配置、默认值、睡眠超时）与 `power_report.c`（无线上报）；空闲调光/低电状态机仍留在 `power.c`，共享状态和副作用不变。
- 修复 `palcallback_cb` 中 `#ifdef` 与唤醒引脚不匹配的潜在编译问题；当前板型同时定义两个模式引脚，不影响现有运行时行为。

### 电源策略与测试

- 调光/关灯档位、低电状态机和充电提示抽取为 `hs/power_policy.c/h` 纯逻辑。
- 新增 `tests/firmware/test_power_policy.c`，CI 与电池策略测试一起覆盖低电和空闲电源边界。

### 上位机

- 将原 `monitor.rs` 拆分为 `monitor/mod.rs`、`publisher.rs`、`runner.rs`、`tracker.rs`，协议与行为不变。
- 收敛 monitor 模块引用风格，减少重复的完整路径引用。

### 兼容性

- 不修改 EEPROM 布局、`HS_SETTINGS_VERSION`、`HS_STATUS_SNAPSHOT_VERSION`、VIA ID、键码顺序、灯效顺序或 `process_record_kb` 的调用链。
- 固件、VIA 定义和上位机安装包仍使用原有只读状态通道 `0x22`。

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

- 从 v1.0.2 升级不需要手动清空 EEPROM；`HS_SETTINGS_VERSION` 仍为 `1`。
- 固件、VIA 定义和上位机安装包均在本 Release 附件中。
- Linux deb/rpm 会自动安装 udev 规则；AppImage 用户需要按 README 手动安装规则并重新插拔设备。

## 下载

- `royalkludge_rk98pro_us_triple_mode_default.bin`：刷入键盘的固件。
- `rk98pro_us_triple_mode.json`：VIA 定义。
- Windows 安装包 / 便携版：x64 与 ARM64。
- Linux AppImage / deb / rpm：amd64 与 arm64。

完整变更见 [CHANGELOG.md](CHANGELOG.md)，协议说明见 [host/README.md](host/README.md)。
