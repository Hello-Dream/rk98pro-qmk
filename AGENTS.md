# AGENTS.md

本仓库是单个 QMK 键盘的固件 overlay，不是完整 QMK 源码树。以下约定用于避免破坏用户已保存的设置和发布产物。

## 项目定位

- 唯一固件目标：`royalkludge/rk98pro_us_triple_mode`（RK98PRO，WB32FQ95，USB / 蓝牙 ×3 / 2.4G）。
- 根目录不能直接 `make`。本仓库只提供 `keyboards/` overlay，需叠加到上游 `qmk/qmk_firmware` 的 `f5db962110615fd263c3ebf9f0402e7c2b08fbe4`（`f5db9621`）后再构建。
- 叠加时必须复制整个 `keyboards/`（含 `keyboards/linker/`），不能只复制 `royalkludge/`；`post_rules.mk` 会按固定路径 include `keyboards/linker/wireless/wireless.mk`，不要移动或删除该模块。
- 该基座包含 QMK 的 NVM 重构；代码依赖新的 `nvm_eeprom_eeconfig_internal.h`、GPIO、`eeconfig` 和 raw HID 接口。升级基座要同步 `.github/workflows/build.yml` 的 `ref` 与 `README.md` 的提交号，并重新核对 EEPROM 偏移，不能按普通 QMK 升级处理。

## 验证与构建

仓库有 `tests/firmware` 下的主机侧纯 C 测试；host 有 Vitest 前端测试、Playwright e2e 和 Rust 单元测试。提交前按 CI 顺序执行：

```bash
python3 scripts/check_constraints.py --base origin/main --strict
python3 scripts/validate_via_json.py
git diff --check origin/main...HEAD -- '*.c' '*.h'
(cd host && pnpm lint && pnpm typecheck && pnpm test && pnpm build)
(cd host/src-tauri && cargo fmt -- --check)
(cd host/src-tauri && cargo clippy --locked --all-targets -- -D warnings)
cargo test --manifest-path host/src-tauri/Cargo.toml
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
(cd host && pnpm exec playwright install --with-deps chromium && pnpm test:e2e)
```

- 发布 tag 还需运行 `python3 scripts/check_release_tag.py --tag "$GITHUB_REF_NAME"`，校验 tag 为 `v<产品版本>` 或 `v<产品版本>-<prerelease>`；`verify` job 在 tag 构建同样运行，`release` 依赖它。

- `--base` 必须是本地已存在的提交；本地用 `origin/main`，CI 的 push 事件用 `github.event.before`（手动触发时回退 `origin/main`）。基线过旧会产生误报。
- 本地编译：先在本仓库根目录把 `keyboards/` 叠加到 QMK 树（注意末尾的 `/.`），再在 QMK 根目录执行：

```bash
# 在本仓库根目录
cp -r keyboards/. /path/to/qmk_firmware/keyboards/

# 在 QMK 根目录
make royalkludge/rk98pro_us_triple_mode:default
make royalkludge/rk98pro_us_triple_mode:default:flash
```

- 产物为 `royalkludge_rk98pro_us_triple_mode_default.bin`。
- 每次 push 都会在 `ghcr.io/qmk/qmk_cli` 容器中运行上述校验并编译固件，但不上传固件产物；上位机安装包和 Release 只在推送 `v*` tag 时构建/发布。没有本地 QMK 树时应依赖 Actions 构建。
- CI 中本仓库检出在 `_keyboard/`，QMK 在根目录；workflow 新增步骤引用本仓库文件时要加 `_keyboard/` 前缀。
- CI 容器默认 shell 是 dash；workflow 步骤用到 `mapfile` 或进程替换时必须显式 `shell: bash`，否则会因 `mapfile: not found` 失败。

## 代码地图

- `keyboard.json`：硬件与构建配置的单一事实来源，包括 6×19 矩阵、100 颗主键灯、6 层（`dynamic_keymap.layer_count`）、启用的内置灯效、USB VID/PID、10ms 去抖；6 颗侧灯由 `config.h` 的 `RGBLED_NUM` 定义。`features.rgblight=false` 时 `Side Light` 持久模式只保存模式值，充电、连接和低电状态提示仍通过 RGB Matrix 使用这些侧灯。
- `keymaps/default/keymap.c`：6 层含义：`_BL`/`_FL` 为 Windows 基础/Fn，`_MBL`/`_MFL` 为 macOS 基础/Fn，`_DEFA` 为 Fn+右Shift 刷机层（左上角 `QK_BOOT`），`_MOUSE` 由 Fn+M 切换；增删层要同步 `keyboard.json` 的 `dynamic_keymap.layer_count`。
- `rk98pro_us_triple_mode.c`：板级初始化、QMK 钩子和模块编排；电源/续航、指示灯、无线、VIA、RGB 录制和 EEPROM 复位主要由 `hs/` 模块实现。主要钩子：`pre_process_record_kb`、`process_record_kb`、`housekeeping_task_user`、`rgb_matrix_indicators_advanced_kb`、`keyboard_post_init_kb`。
- `hs/status.c/h`：只读状态快照、`seq`、raw HID 事件推送和 VIA `0x22` GET；状态检查按最近 GET 自适应 250ms/1s。
- `hs/power.c/h`、`hs/battery.c/h`：电源/续航状态机、电量滤波与充满判定。
- `hs/battery_policy.c/h`：不依赖 QMK 的电池滤波、低电确认与充满锁存纯逻辑；`tests/firmware/test_battery_policy.c` 覆盖其边界。
- `hs/power_policy.c/h`：不依赖 QMK 的调光/关灯档位、低电状态机与充电提示纯逻辑；`tests/firmware/test_power_policy.c` 覆盖其边界。
- `hs/indicators.c/h`：Caps/Num/Scroll/Win、侧灯状态提示、电量条和连接提示。
- `hs/keys.c/h`：短按键过滤、`process_record_user` 与按键初始化。
- `hs/rgb.c/h`：持久侧灯模式（当前只保存模式）、灯效选择与 RGB 录制集成；持久侧灯逻辑用 `#ifdef RGBLIGHT_ENABLE` 隔离，避免覆盖主键灯效。
- `hs/wireless.c` + `hs/wireless_glue.h`：无线模式、配对、指示灯和休眠胶水。
- `hs/via.c/h`、`hs/store.c/h`、`hs/layers.h`：VIA 分发、设置/录制存储和图层定义。
- `rgb_record/`：按键 RGB 录制与 `RGBR_PLAY` 预览；共 4 个通道，默认快捷键只使用前 3 个，第 4 个为预留。
- `rgb_matrix_user.inc`：`HS_*` 自定义灯效实现；实际生效顺序会影响已保存的灯效编号。键位和 `keyboard.json` 使用 `EF_*` 键码，C 实现使用 `HS_*`，不要混用。
- RGB 控制键位使用新基座的 `UG_*` 键码；旧 `RGB_*` 已迁移为 `UG_*`，改 keymap 时不要写回旧键码。
- 根目录 `README.md`、`快捷键说明.md` 是用户文档；`keyboards/.../readme.md` 是 QMK 模板，行为文档不要只改后者。

## 兼容性红线

- EEPROM 布局或灯效顺序变化必须递增 `config.h` 的 `HS_SETTINGS_VERSION`。包括 `hs_settings_t` / `hs_power_t` 字段、`EECONFIG_*_SIZE`、`RGBREC_CHANNEL_NUM`、`keyboard.json` 启用的内置灯效集合、`rgb_matrix_user.inc` 的实际生效顺序。`scripts/check_constraints.py` 会强制检查；版本不符会让用户升级后恢复默认设置。
- `EECONFIG_USER_DATA_VERSION` 由 `HS_SETTINGS_VERSION` 和 `EECONFIG_USER_DATA_SIZE` 自动拼接，不要手改；只改版本号或 `EECONFIG_*_SIZE`。
- `keyboard.json` 的 `keycodes` 按位置映射到 `QK_KB_n`：只能追加到末尾，不能删除或重排；`rk98pro_us_triple_mode.json` 的 `customKeycodes` 数量必须一致、顺序按位置对应。VIA 中的 `name`/`title` 只是显示文本，改显示名不影响键码。
- 改 `keyboard.json` 的 `usb.vid`/`pid` 要同步 `rk98pro_us_triple_mode.json` 的 `vendorId`/`productId`；`validate_via_json.py` 会检查。
- `hs/wireless_glue.h` 的 `enum via_custom_defined_id` 是外部工具 ABI：已有数值不可修改或复用，只能追加；`0x01` 是已废弃的旧上位机 RPC，`0x10`–`0x21` 为 VIA 控件，`0x22` 为只读状态。
- 内置灯效只由 `keyboard.json` 的 `rgb_matrix.animations` 控制；自定义灯效还要同步 `config.h` 的 `ENABLE_RGB_MATRIX_*` 与 `rgb_matrix_user.inc` 的 `RGB_MATRIX_EFFECT`。两者都会改变 VIA 灯效下拉框编号，增删/排序后要同步值（从 0 连续递增，数量为 `1 + 内置灯效数 + 自定义灯效数`，自定义含 `RGBR_PLAY`）并递增 `HS_SETTINGS_VERSION`。
- `keyboard.json` 的 `debounce` 固定为 10ms；`rgb_record.c` 的 `rgbmatrix_buff` 只能引用 `keyboard.json` 已启用的灯效。
- `QK_BOOT` 软复位只允许 `bootloader_jump()`，不要恢复旧版的 `eeconfig_disable()`，否则会清空用户设置。
- EEPROM 写入保持单次持久化：批量修改用 `*_noeeprom` 接口，最后只写一次，避免写放大。

## 版本与发布

- 发版时同步产品版本：`keyboard.json` 的 `version`、`config.h` 的 `HS_FIRMWARE_VERSION_*`、`host/package.json`、`host/src-tauri/Cargo.toml`，以及 `CHANGELOG.md`、`README.md` 和 `RELEASE_NOTES.md`；`scripts/check_constraints.py` 会校验这些版本一致。
- `HS_SETTINGS_VERSION` 和 `HS_STATUS_SNAPSHOT_VERSION` 是兼容性版本，不随产品版本递增；前者只在 EEPROM 布局变化时递增，后者只在状态快照字段或事件格式变化时递增。
- 发布产物是 `royalkludge_rk98pro_us_triple_mode_default.bin` 和 `rk98pro_us_triple_mode.json`，以及 Windows / Linux 的 x64、ARM64 上位机安装包；推送 `v*` tag 会触发 Release，正文取自 `RELEASE_NOTES.md`。普通 push 只跑校验、测试和固件编译，不生成桌面安装包。
- Linux deb/rpm 安装包必须包含 `/usr/lib/udev/rules.d/99-rk98pro.rules`；`tauri.conf.json` 的 `bundle.linux.deb.files` / `rpm.files` 负责映射，CI 用 `dpkg-deb -c` 和 `rpm -qpl` 校验。
- `*.bin`、`*.hex`、`*.uf2`、`*.elf` 已被 `.gitignore` 忽略；根目录同名产物是本地文件，不要提交。
- 用户可见的快捷键或行为变化需同步 `README.md` 与 `快捷键说明.md`；文档和代码注释保持中文。
