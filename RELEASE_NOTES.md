# RK98PRO 三模 QMK 固件与上位机 v1.0.1

Royal Kludge **RK98PRO**（98 键 / 100 配列）的 QMK 固件与只读桌面监控，支持 USB / 蓝牙 ×3 / 2.4G 三模、RGB Matrix 背光、按键 RGB 录制与 VIA 改键。

v1.0.1 是缺陷修复版本：修复关机重开后主键背光变成单色常亮，以及自定义设置每次上电复位的问题。

---

## 本次修复

### 固件

- 修复关机重开后主键灯效被侧灯恢复逻辑覆盖、变成单色常亮的问题；主键灯效、亮度、速度和颜色现在跨重启保持。
- 修复自定义用户数据区版本标记未写入，导致每次上电恢复默认设置、清空 VIA 键位 / 宏和按键 RGB 录制数据的问题。
- VIA 键位、宏、睡眠超时、防连击、背光总开关、录制通道和录制数据现在只在首次刷入或设置版本变化时初始化。
- 当前构建未启用独立侧灯驱动，`Lighting → Side Light` 仅保存模式值，不再影响主键灯效。

### 上位机

- 版本号同步为 `1.0.1`，功能与 v1.0.0 一致；安装包重新构建。

### VIA

- 保持 `Lighting → Side Light` 菜单和自定义命令 ID 不变；当前固件只保存模式值，不驱动独立侧灯。

---

## 刷入说明

- 从 v1.0.0 升级不需要手动清空 EEPROM。
- 升级到本版本后首次启动会执行一次默认恢复并写入有效版本标记；之后普通关机重开不再复位设置。如有自定义键位，建议先在 VIA 中记录。
- 只有降级或 EEPROM 数据异常时，才建议按 [README.md](README.md#upgrade-eeprom) 手动清空。

---

## 下载

- `royalkludge_rk98pro_us_triple_mode_default.bin`：刷入键盘的固件。
- `rk98pro_us_triple_mode.json`：VIA 定义。
- Windows 安装包 / 便携版：x64 与 ARM64。
- Linux AppImage / deb / rpm：amd64 与 arm64。

以上附件均在本 Release 中公开下载。

完整变更见 [CHANGELOG.md](CHANGELOG.md)，协议说明见 [host/README.md](host/README.md)。
