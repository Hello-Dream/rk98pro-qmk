# RK98PRO 三模 QMK 固件与上位机 v1.0.2

> **稳定版**：面向 v1.0.1 用户的运行时、Linux 交付与诊断优化版本。固件、VIA 定义与上位机版本号统一为 `1.0.2`。

本次版本不改变键盘配置、状态快照协议或 EEPROM 布局，重点降低空闲轮询开销、补齐 Linux HID 访问规则、改善历史写入与故障提示，并把格式化、静态检查、e2e 和发布 tag 校验纳入 CI。

---

## 本次变更

### 固件运行时

- 状态检查改为自适应：最近 2 秒有状态 GET 时 250ms，否则 1 秒；隐藏到托盘时不再固定 50ms 构建快照。
- 移除主循环中重复的电池采样调用，`hs_send_bat_status()` 内保留唯一采样点。
- 设置入口增加“值未变不写 EEPROM”短路：电源设置、睡眠超时、侧灯模式、防连击、背光总开关和同通道录制只跳过持久化，不改变必要的硬件重放/预览行为。
- 电池滤波、3% 停止阈值确认和充满锁存抽取为 `hs/battery_policy.c/h` 纯逻辑，并新增主机侧单元测试。

### 上位机

- 事件轮询改为聚焦 200ms、失焦/托盘 1 秒；`useStatus` 的 5 秒轮询改为 30 秒兜底，界面主路径仍是 `status-updated` 事件。
- 历史查询由后端 `history-updated` 事件驱动；历史数据没有追加时不再重复读取整份文件。
- 历史 JSON 改为内存更新 + 30 秒防抖落盘，正常退出时刷新；写入失败保留待写数据并延后 30 秒重试。
- 托盘摘要继续使用去重缓存，状态摘要未变化时跳过菜单、tooltip 和图标重绘。
- 新增只读 `get_monitor_error` 命令与 `monitor-error` 事件，区分钟不兼容、权限/udev、未找到设备和 HID 超时，并显示可操作提示；不改变 `status-updated` 契约。
- WPM 统计查询保持 2 秒本机内存轮询，不产生 HID 或磁盘 I/O。

### Linux 交付

- deb / rpm 自动把 `99-rk98pro.rules` 安装到 `/usr/lib/udev/rules.d/`，安装后重新插拔键盘或接收器即可。
- AppImage 在 README 中提供手动安装到 `/etc/udev/rules.d/`、重新加载 udev 和重新插拔的步骤。
- CI 在 Linux 打包后用 `dpkg-deb -c` 和 `rpm -qpl` 校验包内规则路径。

### 固件其他

- RGB 录制预览（`RGBR_PLAY`）初始化时建立 LED 索引到矩阵行列的查找表，播放时改为 O(1) 查表，移除每帧矩阵扫描。
- 查找表使用 `0xFFFF` 哨兵并显式初始化；未映射 LED 保持熄灭。

### 验证与发布门禁

- CI 增加 Rust `cargo fmt --check`、`cargo clippy -- -D warnings`、Playwright chromium e2e、固件电池策略测试和 tag/版本一致性校验。
- `verify` 在 tag 构建同样运行，`release` 依赖 `verify`，打 tag 不会绕过门禁。
- 现有 VIA/EEPROM/只读文案检查保持不变。

### 兼容性

- 不修改 EEPROM 布局、状态快照版本、VIA ID、键码顺序、灯效顺序或产品兼容关系。
- `HS_SETTINGS_VERSION` 保持 `1`；`HS_STATUS_SNAPSHOT_VERSION` 保持 `1`。
- 旧上位机忽略 `monitor-error` 事件；新上位机对旧固件显示“不支持只读状态通道”。

---

## 请重点验证

1. **状态与托盘**：连接键盘后观察状态、电量和托盘摘要；切换层、锁定键或电源状态时及时更新，失焦/托盘时不应持续高频重绘。
2. **电量历史**：确认历史按 5 分钟/状态变化采样，页面只在历史追加时刷新；正常退出后重新打开，最近的采样和事件仍在。
3. **Linux 权限**：deb/rpm 安装后普通用户可直接连接；AppImage 按文档安装 udev 规则后，权限错误提示应消失。
4. **诊断提示**：分别验证旧固件、权限不足、未找到设备和 HID 超时四类提示，确认文案可操作且不会反复弹出。
5. **RGB 录制**：进入/退出 `RGBR_PLAY`，录制按键、清空通道、复制到下一通道；确认各 LED 颜色正确，侧灯/主键灯边界没有串色。

---

## 刷入说明

- 从 v1.0.1 或 v1.0.2-test.1 升级不需要手动清空 EEPROM；`HS_SETTINGS_VERSION` 仍为 `1`。
- 固件、VIA 定义和上位机安装包均在本 Release 附件中。
- Linux 包升级后规则随包安装；AppImage 用户需要按 README 手动安装规则并重新插拔设备。

## 下载

- `royalkludge_rk98pro_us_triple_mode_default.bin`：刷入键盘的固件。
- `rk98pro_us_triple_mode.json`：VIA 定义。
- Windows 安装包 / 便携版：x64 与 ARM64。
- Linux AppImage / deb / rpm：amd64 与 arm64。

完整变更见 [CHANGELOG.md](CHANGELOG.md)，协议说明见 [host/README.md](host/README.md)。
