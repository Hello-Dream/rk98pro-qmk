# RK98PRO 三模 QMK 固件

Royal Kludge **RK98PRO**（QMK 路径 `royalkludge/rk98pro_us_triple_mode`）的 QMK 固件与 VIA 定义。

## 硬件与特性

- 98% 配列（100 键），三模：USB / 蓝牙 ×3 / 2.4G
- RGB Matrix 背光（每键）+ 8 颗侧灯
- 灯效：仅保留 7 个自定义效果（内置 44 个 QMK 动效已全部关闭）
- 支持 VIA 改键、按键 RGB 录制
- 去抖固定 **10ms**（已移除 1ms 低延迟模式，避免连击）

## 目录结构

```
.
├── README.md
├── LICENSE
├── .gitignore
├── .gitattributes
└── keyboards/
    └── royalkludge/
        └── rk98pro_us_triple_mode/
            ├── keyboard.json               # QMK 键盘配置（去抖/矩阵/引脚/RGB…）
            ├── config.h / halconf.h / mcuconf.h
            ├── post_rules.mk
            ├── rk98pro_us_triple_mode.c    # 键盘主逻辑
            ├── rgb_matrix_user.inc         # 自定义灯效（HS_*）
            ├── rk98pro_us_triple_mode.json # VIA 定义
            ├── keymaps/default/            # 默认键位
            ├── rgb_record/                 # 按键 RGB 录制
            └── wls/                        # 无线相关
```

## 依赖

- 无线模块：`keyboards/linker/wireless/`，由 `post_rules.mk` 通过 `include keyboards/linker/wireless/wireless.mk` 引入，**已随本仓库提供**。
- QMK 的 WB32 支持（`platforms/chibios/boards/GENERIC_WB32_FQ95XX`、`bootloaders/wb32_dfu.c`）由官方 `qmk_firmware` 提供。

## 编译

将 `keyboards/` 目录放入 QMK 源码树后，在 QMK 根目录执行：

```
make royalkludge/rk98pro_us_triple_mode:default
```

## 云端编译（GitHub Actions）

仓库自带 `.github/workflows/build.yml`：push 或在 Actions 页手动触发后，会在 QMK 官方容器（`ghcr.io/qmk/qmk_cli`）里检出 **`yangzheng20003/qmk_firmware`**、叠加本仓库的 `keyboards/` 并编译，产物在该次运行的 **Artifacts** 里下载。

> 基座使用维护者的 QMK fork **`yangzheng20003/qmk_firmware`**（它保留了本固件依赖的 `EECONFIG_USER_DATABLOCK` 等接口）；本仓库另带无线模块 `keyboards/linker/wireless/`。

## 烧录

### 进入 bootloader

本键盘**没有 Reset 按键**，按优先级任选一种：

1. **软复位（推荐）**：按住 `Fn` + `右Shift`，再点一下 `Esc` → 直接重启进入 bootloader。
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

## VIA

用 `keyboards/royalkludge/rk98pro_us_triple_mode/rk98pro_us_triple_mode.json` 作为 VIA 定义加载，即可改键与调节灯效。

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
- 也可用 `RGB_MOD`（Fn+PrtSc）逐个循环到这些效果。
- 背光开关快捷键：**Fn+Backspace**（Windows `_FL` / macOS `_MFL`）。

## 许可

GPL-2.0-or-later，详见 [LICENSE](LICENSE)。
