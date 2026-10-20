# 更新日志

本项目的版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)（SemVer）。

> [!WARNING]
> **本固件的持久化设置（EEPROM / `confinfo`）存储结构与厂商原版固件不同，且不含版本号与校验。**
> 从厂商原版固件或旧版本升级时，**必须清空一次 EEPROM**，否则可能出现睡眠超时档位错乱、侧灯模式 / 颜色异常、按键 RGB 录制动效跳错甚至背光全灭。
> 清空方式见 [README.md](README.md#upgrade-eeprom)。

## [Unreleased]

此处记录尚未发布的变更。发布时把内容归入新的版本号段落，并同步 `keyboard.json` 的 `version` 字段与 GitHub Release tag。

## [1.1.1] - 2026-10-20

### 修复

- 蓝牙 / 2.4G 连接状态指示灯（`Fn + Q/W/E/P` 对应按键）不再受 `Fn + Backspace` 背光开关影响：背光关闭时临时点亮 LED 供电，只显示连接 / 配对指示，提示结束后自动恢复关闭。

## [1.1.0] - 2026-10-16

### 新增

- `Fn + Z` 开关「短按键过滤」（防连击）：开启后，某个物理键松开后 20ms 内再次按下会被忽略，用于压掉偶发的双击/连击。开关状态存入 EEPROM，切换时对应指示灯白闪 3 次（开）/ 红闪 3 次（关）；经 `pre_process_record_kb` 在按键处理前拦截，只作用于普通字符键与修饰键，不影响 Fn 组合与自定义键。

### 修复

- VIA 的 Lighting → Color 控件改为始终显示（仅 All Off 时隐藏）。原条件把 Ripple(24) 与 Raindrops(28) 一并隐藏，但这两个自定义效果的颜色取自 `rgb_matrix_config.hsv`，基色完全可调，导致 VIA 里改不了色；条件里的 29 / 32 越界，永远为真。
- VIA 定义的 `keycodes` 由兼容回退值 `qmk_lighting` 改为显式模块 `qmk_rgb_matrix_keycodes`，消除 VIA 的 `lighting subsystem cannot be determined` 警告，键码列表只显示 RGB Matrix 的 `RM_*`，不再混入无关的 `UG_*`。

## [1.0.0] - 2026-10-15

首个正式发布版本。基于厂商原版固件行为对齐，提供完整三模支持、RGB Matrix 背光、自定义灯效与按键 RGB 录制。

### 新增

- 三模支持：USB / 蓝牙 ×3 / 2.4G，配对与切换见 [快捷键说明.md](快捷键说明.md)。
- 20 个经典内置动效（单色常亮 / 呼吸 / 渐变 / 彩虹 / 按键反应等），`Fn + PrtSc` 循环切换，VIA 的 Lighting → Effect 同步为 29 项（0..28）。
- 7 个自定义灯效（`HS_*`）：Plasma、Sparkle、Ripple、Breath Gradient、Fire、Cyber Scan、Raindrops，直达键 `Fn + 4` ~ `Fn + 0`。
- 按键 RGB 录制：三通道独立、掉电不丢，`Fn + 1/2/3` 选通道、`Fn + Home` 开始 / 结束录制。
- 电量显示 `Fn + Enter`：点亮电量灯约 3 秒，低于 30% 显示红色，有线 / 无线均可用。
- Windows / macOS 双布局，`Fn + S` / `Fn + A` 切换，保存到键盘掉电不丢。
- GitHub Actions 云端编译，产物在每次运行的 Artifacts 中下载。
- 完整中文文档：[README.md](README.md) 与 [快捷键说明.md](快捷键说明.md)。

### 变更

- 去抖固定 **10ms**，移除 1ms 低延迟模式（原实现会导致连击）。
- 侧灯数量 `RGBLED_NUM` 由 8 修正为 6，对齐原厂。
- 灯效速度上下限对齐原厂（`SPD_STEP * 5` / `SPD_STEP`），修复减速到底后松开仍会再减一档的越界。
- 背光亮度 / 速度等 Fn 组合键改用原厂 `QK_UNDERGLOW_*` 键码。
- 指示灯改由主循环每轮刷新（与原厂一致），并在上电、进睡前统一置灭。
- 接入 `lpwr_set_timeout_value`，“不睡眠”档位不再秒睡，长档位不再被 5min 覆盖。
- 模块睡眠命令统一为与原厂一致（蓝牙 `0x80 + n`，2.4G `0xFF`）。
- CI 编译基座固定为维护者的 QMK fork `yangzheng20003/qmk_firmware`（保留 `EECONFIG_USER_DATABLOCK` 等接口）。
- `keyboard.json` 增加 `version` 字段。

### 修复

- 指示灯上电常亮（引脚未置灭 + 只在 RGB 渲染钩子里驱动），恢复随 Caps / Num / Scroll / Win 锁状态点亮。
- 背光关闭时 `Fn + Enter` 无法显示电量，改为临时点亮显示 3 秒后恢复。
- 蓝牙下不跟随主机系统下发的电量（macOS `0x80` / Windows `0x81`）。
- 低电指示与低电自动关机依赖 RGB 渲染而失效，已移入主循环。
- 修复 rainbow swirl 动效硬编码索引越界写。
- 移除 `BAT_FULL_PIN` 多余中断源，恢复进睡前 `clear_keyboard()`。
- 删除误加的 `HS_LED_BOOSTING_PIN`（与 `LED_MAC_PIN` 共用 B7）与遗留的 `KC_TEST` 白灯测试。
- macOS 布局 `_MFL` 层 row1 多出的一个占位键（导致 LAYOUT 参数数量不匹配而编译失败）。
- 修复 CI 中 `qmk hello` 失败（容器自带依赖与 fork 的 `requirements.txt` 不一致）。

### 破坏性变更

- **升级必须清空 EEPROM**：持久化设置结构与厂商原版不同且无版本校验，不清空可能出现睡眠档位错乱、侧灯异常、RGB 录制动效错误或背光全灭。
  - 按住 `Esc` 再插 USB 线（Bootmagic，推荐）
  - `Fn + 空格` 按住约 3 秒（需背光处于开启状态）
  - VIA 的 EEPROM Reset
- 从厂商原版固件升级的设备，`Fn + Z` 不再是低延迟开关（功能已移除），可用 `Fn + B` 切换全键无冲。
