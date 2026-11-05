# RK98PRO

> 本仓库在 QMK 默认实现上增加了三模无线、RGB 录制、VIA 自定义菜单、只读状态通道与电源管理；完整说明见仓库根目录的 `README.md` 与 `快捷键说明.md`。

* Keyboard Maintainer: [yangzheng20003](https://github.com/yangzheng20003)
* Hardware Supported: RK98PRO
* Hardware Availability: [RK98PRO](https://www.rkgaming.com)

Make example for this keyboard (after setting up your build environment):

    make royalkludge/rk98pro_us_triple_mode:default
        
Flashing example for this keyboard:

    make royalkludge/rk98pro_us_triple_mode:default:flash

To reset the board into bootloader mode, do one of the following:

* Hold the Reset switch mounted on the bottom side of the PCB while connecting the USB cable
* Hold the Escape key while connecting the USB cable (also erases persistent settings)
* Fn+R_Shift+Esc will reset the board to bootloader mode if you have flashed the default QMK keymap

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).
