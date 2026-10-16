# RK98PRO 三模 QMK 固件 v1.1.0

Royal Kludge **RK98PRO**（98 键 / 100 配列）的 QMK 固件，支持 USB / 蓝牙 ×3 / 2.4G 三模、RGB Matrix 背光、按键 RGB 录制与 VIA 改键。

本版本在 v1.0.0 基础上新增「短按键过滤」（防连击），并修复 VIA 的 Color 控件与 keycodes 定义问题。

---

## 本版本更新（v1.1.0）

### 新增

- `Fn + Z` 开关「短按键过滤」（防连击）：开启后，某个物理键松开后 20ms 内再次按下会被忽略，用于压掉偶发的双击/连击。开关状态存入 EEPROM，切换时对应指示灯白闪 3 次（开）/ 红闪 3 次（关）。

### 修复

- VIA 的 Lighting → Color 控件改为始终显示（仅 All Off 时隐藏），Ripple / Raindrops 等自定义效果现在可在 VIA 中改色。
- VIA 定义的 `keycodes` 改用显式模块 `qmk_rgb_matrix_keycodes`，消除 `lighting subsystem cannot be determined` 警告。

---

## ⚠️ 升级前必读：必须清空一次 EEPROM

本固件持久化设置（EEPROM / `confinfo`）的存储结构与**厂商原版固件不同**，且没有版本号与校验。

**从厂商原版固件（或本项目的旧测试版本）升级后，请务必清空一次 EEPROM**，否则可能出现：

- 睡眠超时档位错乱；
- 侧灯模式 / 颜色不对；
- 退出按键 RGB 录制后动效跳错，甚至背光全灭。

清空方式（任选其一）：

1. **Bootmagic（推荐，最稳）**：按住 `Esc` 键再插 USB 线，会一并清除持久设置并进入 bootloader。
2. **`Fn + 空格` 按住约 3 秒**：需背光处于开启状态才会执行。
3. **VIA**：使用 VIA 的 EEPROM Reset 复位。

全新设备、或已经清过一次的设备无需重复。

## 下载

- 固件：`royalkludge_rk98pro_us_triple_mode_default.bin`（本 Release 附件）
- VIA 定义：[`rk98pro_us_triple_mode.json`](https://github.com/Hello-Dream/rk98pro-qmk/blob/main/keyboards/royalkludge/rk98pro_us_triple_mode/rk98pro_us_triple_mode.json)（在 VIA 中作为定义加载，即可改键与调灯效）
- 完整文档：仓库 [README.md](https://github.com/Hello-Dream/rk98pro-qmk/blob/main/README.md) 与 [快捷键说明.md](https://github.com/Hello-Dream/rk98pro-qmk/blob/main/%E5%BF%AB%E6%8D%B7%E9%94%AE%E8%AF%B4%E6%98%8E.md)

## 进入 bootloader（DFU）

本键盘**没有 Reset 按键**，按优先级任选一种：

1. **软复位（推荐）**：按住 `Fn` + `右Shift`，再点一下 `Esc`。
2. **Bootmagic**：按住 `Esc` 键再插 USB 线（会同时清除持久设置）。
3. **兜底**：拆开 PCB，短接标 `RST` / `RESET` 的两个焊盘。

进入后设备管理器会出现 DFU 设备：**`VID:PID = 342D:DFA0`**。

## 刷写

```
make royalkludge/rk98pro_us_triple_mode:default:flash
```

或使用 QMK Toolbox / `wb32-dfu-updater_cli` 刷入 `royalkludge_rk98pro_us_triple_mode_default.bin`。
刷完若仍停在 DFU（`342D:DFA0`），**拔插一次 USB** 即可进入应用模式。

## 主要功能

- 三模：USB / 蓝牙 ×3 / 2.4G，`Fn + Q/W/E` 切蓝牙、`Fn + P` 切 2.4G，长按约 3 秒重新配对。
- 灯效：20 个经典内置动效 + 7 个自定义效果（`Fn + 4` ~ `Fn + 0` 直达），`Fn + PrtSc` 循环切换。
- 背光：`Fn + Backspace` 开关，`Fn + ↑/↓` 调亮度，`Fn + ←/→` 调速度。
- 按键 RGB 录制：`Fn + 1/2/3` 选通道，`Fn + Home` 开始 / 结束录制，`Fn + PrtSc` 退出预览。
- 电量显示：`Fn + Enter` 点亮约 3 秒，低于 30% 显示红色。
- 双布局：`Fn + S` 切 macOS 布局，`Fn + A` 切回 Windows 布局，掉电不丢。
- 全键无冲：`Fn + B` 切换；Win 键锁定：`Fn + 左Win`。
- 防连击：`Fn + Z` 开关「短按键过滤」（同键松开后 20ms 内重复按下会被忽略），状态掉电不丢。

完整 Fn 组合键列表见 [快捷键说明.md](https://github.com/Hello-Dream/rk98pro-qmk/blob/main/%E5%BF%AB%E6%8D%B7%E9%94%AE%E8%AF%B4%E6%98%8E.md)。

## 已知限制

- 去抖固定 **10ms**（为避免连击，已移除 1ms 低延迟模式）。
- 防连击：`Fn + Z` 开 / 关「短按键过滤」（同键松开后 20ms 内的重复按下会被忽略）；窗口为编译期常量，如需更强可调大。
- 编译依赖维护者的 QMK fork `yangzheng20003/qmk_firmware`（保留本固件依赖的 `EECONFIG_USER_DATABLOCK` 等接口），CI 已固定该基座。

## 许可

GPL-2.0-or-later，详见仓库 [LICENSE](https://github.com/Hello-Dream/rk98pro-qmk/blob/main/LICENSE)。
