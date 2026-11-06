# 厂商原版基线

本分支保存 RK98PRO 的厂商原版 QMK 代码与 linker 无线模块，保持导入时的原始状态，仅用于与 `main` 分支对比、排查差异和回退参考。

## 内容

- `keyboards/rk/RK98PRO/`：厂商原版 RK98PRO 键盘代码（默认 / VIA 键位、RGB 录制、无线胶水等）。
- `keyboards/linker/wireless/`：厂商无线模块，`main` 分支在此基础上集成。

## 说明

- 本分支不参与固件构建，也不是发布分支。
- 固件、VIA 定义与上位机请使用 [`main`](https://github.com/Hello-Dream/rk98pro-qmk) 分支。
- 对比示例：

```bash
git diff vendor main -- keyboards/
```
