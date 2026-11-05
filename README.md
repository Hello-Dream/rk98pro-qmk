# RK98PRO 三模 QMK 固件

Royal Kludge **RK98PRO**（QMK 路径 `royalkludge/rk98pro_us_triple_mode`）的 QMK 固件、VIA 定义与只读桌面监控。

本仓库以 overlay 形式提供键盘固件：需要叠加到官方 `qmk/qmk_firmware` 的固定提交后编译，不包含完整 QMK 源码树。当前版本 **v1.0.0**，是仓库的首个版本，固件、VIA 定义与上位机版本号一致。

## 特性

- 98% 配列（100 键），三模：USB / 蓝牙 ×3 / 2.4G
- RGB Matrix 逐键背光 + 6 颗侧灯；20 个内置灯效、7 个自定义灯效（`HS_*`）与按键 RGB 录制
- 6 层默认键位，Windows / macOS 双布局；VIA 改键、灯效、侧灯、电源与录制配置
- 实用功能：`Fn + M` 鼠标模式、`Fn + X/C/V` 动态宏、`Fn + Caps Lock` Caps Word
- 防连击：`Fn + Z` 开关短按键过滤（同键松开后 20ms 内的重复按下会被忽略）
- 去抖固定 **10ms**
- 配套上位机 [host/](host/README.md)：Tauri 2 + React 19 只读监控，托盘 + 状态 / 电量 / WPM 统计 / 通知；不写键盘，配置仍由 VIA 完成

### 电源与续航

- 空闲 30 秒调光，2 分钟关闭 RGB 与侧灯；USB / 无线默认启用，按键立即恢复
- 低电 15% 时限制亮度，并用侧灯提示
- 电量降到 3% 时数字排显示电量条，侧灯红色闪烁约 20 秒后进入保护深睡；深睡后按键和模式开关无效，插入充电线 / USB 才能唤醒
- 电量链路消抖与平滑：充电线 500ms、满电引脚 2s、原始电量 5 点中值 + EMA(1/4)；充满判定要求稳定满电信号且平滑电量不低于 98%
- 未知电量保持未知，不显示为 100%

## 下载

最新版本：**[v1.0.0](https://github.com/Hello-Dream/rk98pro-qmk/releases/latest)**

| 文件 | 用途 |
| --- | --- |
| `royalkludge_rk98pro_us_triple_mode_default.bin` | 刷入键盘的固件 |
| `rk98pro_us_triple_mode.json` | VIA 定义，可在 VIA 中直接加载 |
| Windows 安装包 / 便携版（x64、ARM64） | `host/` 只读桌面监控 |
| Linux AppImage / deb / rpm（amd64、arm64） | `host/` 只读桌面监控 |

固件、VIA JSON 和桌面端安装包都在 [Releases](https://github.com/Hello-Dream/rk98pro-qmk/releases/latest) 的附件里。普通 push 的 Actions 只做校验和编译，不上传产物；推送 `v*` tag 时才生成发布附件。

> ℹ️ 从厂商原版固件或其他固件刷入时，固件会在首次启动自动识别并初始化设置，正常无需手动清空 EEPROM；详见[刷入说明](#upgrade-eeprom)。

## 文档

| 文档 | 内容 |
| --- | --- |
| [快捷键说明.md](快捷键说明.md) | Fn 组合键、无线连接、灯光、RGB 录制、鼠标模式与 VIA 菜单 |
| [CHANGELOG.md](CHANGELOG.md) | 版本变更记录 |
| [RELEASE_NOTES.md](RELEASE_NOTES.md) | 当前版本发布说明（GitHub Release 正文） |
| [host/README.md](host/README.md) | 上位机功能、协议与开发说明 |
| [AGENTS.md](AGENTS.md) | 仓库维护与兼容性约定 |

## 分支说明

- `main`：固件、VIA 定义与上位机的开发与发布分支。
- [`vendor`](https://github.com/Hello-Dream/rk98pro-qmk/tree/vendor)：厂商原版 RK98PRO 与 linker 无线模块（未修改），用于对比、排查差异和回退参考，不参与构建。

## 目录结构

```
.
├── README.md
├── CHANGELOG.md                # 版本变更记录
├── RELEASE_NOTES.md            # 当前版本发布说明（GitHub Release 正文）
├── 快捷键说明.md               # Fn 组合键与 VIA 菜单说明
├── AGENTS.md                   # 仓库维护与兼容性约定
├── LICENSE
├── .gitignore
├── .gitattributes
├── .github/workflows/build.yml # CI：校验、编译与发布
├── scripts/                    # 约束检查与 VIA JSON 校验
├── host/                       # Tauri 2 + React 只读监控（状态 / 电量 / WPM / 通知）
└── keyboards/
    ├── linker/wireless/        # 无线模块（随仓库提供）
    └── royalkludge/
        └── rk98pro_us_triple_mode/
            ├── keyboard.json               # QMK 键盘配置（去抖 / 矩阵 / 引脚 / RGB…）
            ├── config.h / halconf.h / mcuconf.h
            ├── post_rules.mk
            ├── rk98pro_us_triple_mode.c    # 键盘主逻辑
            ├── rgb_matrix_user.inc         # 自定义灯效（HS_*）
            ├── rk98pro_us_triple_mode.json # VIA 定义
            ├── keymaps/default/            # 默认键位
            ├── hs/                         # 状态 / 电源 / 指示灯 / 按键 / RGB / 无线 / VIA / 存储模块
            └── rgb_record/                 # 按键 RGB 录制
```

## 依赖

- 无线模块：`keyboards/linker/wireless/`，由 `post_rules.mk` 通过 `include keyboards/linker/wireless/wireless.mk` 引入，已随本仓库提供。
- QMK 的 WB32 支持（`platforms/chibios/boards/GENERIC_WB32_FQ95XX`、`bootloaders/wb32_dfu.c`）由官方 `qmk_firmware` 提供。
- QMK 基座固定在上游 `qmk/qmk_firmware` 的 `f5db9621`（2026-10-29），包含 NVM 重构；键盘侧适配新的 EEPROM / GPIO / `eeconfig` / raw HID 接口。

## 编译

将 `keyboards/` 目录放入 QMK 源码树后，在 QMK 根目录执行：

```
make royalkludge/rk98pro_us_triple_mode:default
```

## 云端编译（GitHub Actions）

仓库自带 `.github/workflows/build.yml`：push 或在 Actions 页手动触发后，会在 QMK 官方容器（`ghcr.io/qmk/qmk_cli`）里检出 **`qmk/qmk_firmware`** 的固定提交 `f5db9621`、叠加本仓库的 `keyboards/` 并编译固件；同时运行上位机的 lint、类型检查、单元测试与 Rust 测试。

推送 `v*` tag 时，workflow 额外在 Windows / Linux 的 x64 与 ARM64 上构建桌面安装包，并把固件、VIA JSON 和全部上位机产物作为 Release 附件发布。普通 push 不上传固件或安装包。

> 该基座包含 QMK 的 NVM 重构；从厂商原版固件刷入时，首次启动会自动初始化一次自定义设置（RGB 录制、电源配置、VIA 缓存），之后保持稳定。本仓库另带无线模块 `keyboards/linker/wireless/`。

## 烧录

### 进入 bootloader

本键盘**没有 Reset 按键**，按优先级任选一种：

1. **软复位（推荐）**：按住 `Fn` + `右Shift`，再点一下 `Esc` → 直接重启进入 bootloader（保留当前设置）。
   （对应默认 keymap：`MO(_DEFA)` = Fn+右Shift，`_DEFA` 层左上角是 `QK_BOOT`。）
2. **Bootmagic**：按住 `Esc` 键再插 USB 线（会同时清除持久设置）。
3. **兜底**：拆开 PCB，找到标 `RST` / `RESET` 的两个焊盘（或圆孔），插着 USB 时短接一下。

进入后设备管理器会出现 DFU 设备：**`VID:PID = 342D:DFA0`**。

### 刷入

```
make royalkludge/rk98pro_us_triple_mode:default:flash
```

或用烧录工具（QMK Toolbox / `wb32-dfu-updater_cli`）刷入 `royalkludge_rk98pro_us_triple_mode_default.bin`。
刷完若仍停在 DFU（`342D:DFA0`），**拔插一次 USB** 即可进入应用模式。

## 刷入说明：EEPROM 自动处理

<a id="upgrade-eeprom"></a>

本固件会为自定义持久化设置（`hs_settings`、按键 RGB 录制数据与侧灯 HSV 槽位）写入版本号和有效性标记。设置版本为 1（`HS_SETTINGS_VERSION = 1`）。首次启动或检测到版本 / CRC 不符时，固件会自动：

- 恢复 QMK 灯效模式 / 亮度、睡眠超时、侧灯模式、背光开关、录制通道等默认设置；
- 恢复按键 RGB 录制的默认数据；
- 让 VIA 的键位缓存失效并恢复默认键位。

从厂商原版固件首次刷入本固件时**不需要手动清空 EEPROM**。只有降级或 EEPROM 数据异常时，才建议手动清空（任选其一）：

1. **Bootmagic（最稳）**：按住 `Esc` 键再插 USB 线（会一并清除持久设置）。
2. **Fn + Space 按住 3 秒**（`EE_CLR` 键在 Fn 层的空格；背光关闭时同样有效）。
3. **VIA**：用 VIA 里的 EEPROM Reset 复位。

> 首次刷入本固件时，VIA 键位会恢复默认；如有自定义键位，请先记录。

## VIA

用 `keyboards/royalkludge/rk98pro_us_triple_mode/rk98pro_us_triple_mode.json` 作为 VIA 定义加载，即可改键与调节灯效。`System` 菜单分为 `General` 和 `Power` 两个分组；`General → Sleep Timeout` 可直接设置睡眠超时（无需重新编译）。

VIA 中还提供：

- `Lighting → Side Light`：切换 6 颗侧灯的模式或关闭侧灯。
- `Lighting → Key RGB Record`：选择录制通道（1~4）、开始/结束录制、清空当前通道、复制到下一通道。
- `System → General`：睡眠超时、防连击、电量/连接状态和“在键盘上显示电量”。
- `System → General → Anti-Bounce Filter`：开 / 关短按键过滤（等同 `Fn + Z`）。
- `System → Power`：空闲调光/关灯、USB 省电、低电阈值与低电亮度、状态侧灯开关；默认 30 秒调光、2 分钟关灯，USB 和无线都启用。`Low Battery Warning` 选 `Off` 会同时关闭低电提示与亮度限制。
- `System → General → Battery / Connection`：只读显示电量与当前连接状态；`System → General → Show Battery on Keyboard` 等同 `Fn + Enter`。

> 建议使用支持自定义标签的 VIA 版本；不支持时可能不显示 `Battery` / `Connection` 这类只读字段。

## 上位机

`host/` 提供 Tauri 2 只读桌面监控（Windows / Linux），通过 VIA raw HID 读取并显示：

- 电量、充电、模式/链路、电源阶段、层、锁定键、WPM、RGB/录制状态；
- 6 小时 / 24 小时 / 7 天电量历史，曲线平滑并保留离线空档；
- WPM 当前、峰值、5 分钟平均和曲线，统计只保存在本机内存，可手动重置；
- 低电、充满、断连/重连桌面通知，可在上位机中分别开关；
- 托盘电池图标、状态摘要、打开面板、立即刷新和退出。

上位机不提供改键、灯光、键盘设置、无线切换、备份或诊断写入；这些配置仍由 VIA 完成。上位机只发送只读的 `GET 0x22`，不向键盘发送任何 SET 或控制命令。

2.4G 模式下，键盘把 raw HID 报告经无线模块转发给接收器，上位机直接和接收器的 `usage_page=0xFF60` / `usage=0x61` 接口通信。USB 有线模式下则由键盘自身暴露同一接口。蓝牙模式只提供标准 HID 键盘报告，没有 VIA raw HID 通道，因此上位机无法通过蓝牙读取状态；需要改用 USB 线或 2.4G 接收器。

v1.0.0 固件与上位机使用只读状态通道 `0x22` 和 raw HID 事件推送，上位机只发送 GET，不写键盘。`HS_SETTINGS_VERSION` 为 1，首次刷入会初始化自定义设置、RGB 录制和动态键位；VIA 自定义 ID `0x10`–`0x21` 与 `rk98pro_us_triple_mode.json` 配合使用。协议说明见 [host/README.md](host/README.md)。

## 自定义灯效

| 宏 | 名称 | 直达键 |
| --- | --- | --- |
| `HS_PLASMA` | Plasma 等离子 | Fn+4 |
| `HS_SPARKLE` | Sparkle 星光 | Fn+5 |
| `HS_RIPPLE` | Ripple 涟漪 | Fn+6 |
| `HS_BREATH_GRADIENT` | Breath Gradient 彩虹呼吸 | Fn+7 |
| `HS_FIRE` | Fire 火焰 | Fn+8 |
| `HS_CYBER_SCAN` | Cyber Scan 赛博扫描 | Fn+9 |
| `HS_RAINDROPS` | Raindrops 雨滴 | Fn+0 |

- 直达键位于 Fn 层（Windows `_FL` / macOS `_MFL`），可在 VIA 中改绑到其它键。
- 另有 **20 个经典内置动效**（单色常亮 / 呼吸 / 渐变 / 彩虹 / 按键反应等），全部可在 VIA 的 Lighting → Effect 里选择；其中常用动效也可用 `RGB_MOD`（Fn+PrtSc）循环。
- 背光开关快捷键：**Fn+Backspace**（Windows `_FL` / macOS `_MFL`）。
- **电量显示**：按一下 **Fn+Enter**（`HS_BATQ`）→ 点亮 LED 64–73 共 10 颗电量灯，约 3 秒后自动熄灭（有线 / 无线均可用）。

## 快捷键

完整的 Fn 组合键与使用说明（无线连接、灯光、RGB 录制、电量、刷机等）见 [快捷键说明.md](快捷键说明.md)。

## 许可

GPL-2.0-or-later，详见 [LICENSE](LICENSE)。
