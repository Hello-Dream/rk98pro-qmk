#!/usr/bin/env python3
"""校验 keyboard.json 与 VIA 定义文件的一致性。"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Optional

REPO_ROOT = Path(__file__).resolve().parents[1]
KEYBOARD_DIR = REPO_ROOT / "keyboards/royalkludge/rk98pro_us_triple_mode"
DEFAULT_KEYBOARD = KEYBOARD_DIR / "keyboard.json"
DEFAULT_VIA = KEYBOARD_DIR / "rk98pro_us_triple_mode.json"


def hex_id(value: object) -> int | None:
    """把 VID/PID 转成整数；数字保持原值，字符串按十六进制解析。"""
    if isinstance(value, bool):
        return None
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        text = value.strip()
        if not text:
            return None
        try:
            return int(text, 16)
        except ValueError:
            return None
    return None


def load_json(path: Path) -> dict[str, Any]:
    """读取 JSON 对象。"""
    with path.open(encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise ValueError(f"{path} 的根节点必须是对象")
    return data


VIA_CONTROL_TYPES = {"button", "toggle", "range", "dropdown", "color", "keycode", "label"}


def _menu_path(path: str, index: int) -> str:
    """生成菜单错误路径。"""
    return f"{path}[{index}]"


def _validate_control(node: dict[str, Any], path: str) -> list[str]:
    """校验单个 VIA 控件对象。"""
    errors: list[str] = []
    control_type = node.get("type")
    if control_type not in VIA_CONTROL_TYPES:
        errors.append(f"{path}: 不支持的控件 type {control_type!r}")
        return errors

    content = node.get("content")
    if not isinstance(content, list):
        errors.append(f"{path}: 控件 content 必须是数组")
        return errors

    for index, item in enumerate(content):
        if isinstance(item, dict):
            errors.append(f"{path}.content[{index}]: 控件 content 不能是对象")

    if "showIf" in node and not isinstance(node["showIf"], str):
        errors.append(f"{path}: showIf 必须是字符串")

    options = node.get("options")
    if control_type == "dropdown":
        if not isinstance(options, list) or not options:
            errors.append(f"{path}: dropdown 必须有非空 options 数组")
        else:
            for index, option in enumerate(options):
                if (
                    not isinstance(option, list)
                    or len(option) < 2
                    or not isinstance(option[0], str)
                    or isinstance(option[1], bool)
                    or not isinstance(option[1], int)
                ):
                    errors.append(f"{path}.options[{index}]: 必须是 [字符串, 整数]")
    elif control_type == "range":
        if (
            not isinstance(options, list)
            or len(options) != 2
            or any(isinstance(value, bool) or not isinstance(value, int) for value in options)
        ):
            errors.append(f"{path}: range 的 options 必须是 [最小整数, 最大整数]")
    elif "options" in node and not isinstance(options, list):
        errors.append(f"{path}: options 必须是数组")

    return errors


def _validate_group(node: dict[str, Any], path: str, top_level: bool) -> list[str]:
    """校验分组对象；顶层菜单项只能包含 label/content。"""
    errors: list[str] = []
    if "label" not in node or not isinstance(node["label"], str):
        errors.append(f"{path}: 分组缺少字符串 label")
    if "content" not in node or not isinstance(node["content"], list):
        errors.append(f"{path}: 分组缺少数组 content")
        return errors

    extra = set(node) - {"label", "content"}
    if extra:
        errors.append(f"{path}: 分组只能包含 label/content，发现 {sorted(extra)}")

    for index, child in enumerate(node["content"]):
        child_path = _menu_path(f"{path}.content", index)
        if not isinstance(child, dict):
            errors.append(f"{child_path}: 分组内容必须是对象")
            continue
        if "type" in child:
            errors.extend(_validate_control(child, child_path))
        else:
            errors.extend(_validate_group(child, child_path, top_level=False))

    return errors


def validate_menu_structure(via: dict[str, Any]) -> list[str]:
    """校验 VIA v3 的 menus 层级、控件类型和 content 形状。"""
    errors: list[str] = []
    menus = via.get("menus")
    if menus is None:
        return errors
    if not isinstance(menus, list):
        return ["menus: 必须是数组"]

    for menu_index, menu in enumerate(menus):
        menu_path = _menu_path("menus", menu_index)
        if not isinstance(menu, dict):
            errors.append(f"{menu_path}: 菜单必须是对象")
            continue
        if "label" not in menu or not isinstance(menu["label"], str):
            errors.append(f"{menu_path}: 菜单缺少字符串 label")
        if "content" not in menu or not isinstance(menu["content"], list):
            errors.append(f"{menu_path}: 菜单缺少数组 content")
            continue
        extra = set(menu) - {"label", "content"}
        if extra:
            errors.append(f"{menu_path}: 菜单只能包含 label/content，发现 {sorted(extra)}")

        for index, group in enumerate(menu["content"]):
            group_path = _menu_path(f"{menu_path}.content", index)
            if not isinstance(group, dict):
                errors.append(f"{group_path}: 顶层菜单项必须是分组对象")
                continue
            errors.extend(_validate_group(group, group_path, top_level=True))

    return errors


def find_effect_dropdown(node: Any) -> Optional[dict[str, Any]]:
    """递归查找 VIA 灯效下拉框。"""
    if isinstance(node, dict):
        content = node.get("content")
        if (
            node.get("type") == "dropdown"
            and isinstance(content, list)
            and content
            and content[0] == "id_qmk_rgb_matrix_effect"
        ):
            return node
        for value in node.values():
            found = find_effect_dropdown(value)
            if found is not None:
                return found
    elif isinstance(node, list):
        for value in node:
            found = find_effect_dropdown(value)
            if found is not None:
                return found
    return None


def validate_via_files(
    keyboard_path: Path = DEFAULT_KEYBOARD,
    via_path: Path = DEFAULT_VIA,
) -> list[str]:
    """返回 keyboard.json 与 VIA JSON 的不一致项。"""
    errors: list[str] = []

    try:
        keyboard = load_json(keyboard_path)
        via = load_json(via_path)
    except (OSError, ValueError) as exc:
        return [str(exc)]

    errors.extend(validate_menu_structure(via))

    usb = keyboard.get("usb", {}) if isinstance(keyboard.get("usb"), dict) else {}
    for key, label in (("vid", "VID"), ("pid", "PID")):
        keyboard_value = hex_id(usb.get(key, keyboard.get(f"vendorId" if key == "vid" else "productId")))
        via_value = hex_id(via.get("vendorId" if key == "vid" else "productId"))
        if keyboard_value is None:
            errors.append(f"keyboard.json 的 {label} 无法解析：{usb.get(key)!r}")
        elif via_value is None:
            errors.append(f"VIA JSON 的 {label} 无法解析：{via.get('vendorId' if key == 'vid' else 'productId')!r}")
        elif keyboard_value != via_value:
            errors.append(
                f"{label} 不一致：keyboard.json 为 {keyboard_value:#06x}，"
                f"VIA JSON 为 {via_value:#06x}"
            )

    keyboard_keycodes = [
        item.get("key") for item in keyboard.get("keycodes", []) if isinstance(item, dict) and item.get("key")
    ]
    via_keycodes = via.get("customKeycodes", [])
    # VIA 的 customKeycodes 按位置映射到 QK_KB_n，name 只是界面显示文本
    if len(keyboard_keycodes) != len(via_keycodes):
        errors.append(
            f"keyboard.json 有 {len(keyboard_keycodes)} 个自定义键码，"
            f"VIA 有 {len(via_keycodes)} 个 customKeycodes，两边数量不一致。"
        )

    dropdown = find_effect_dropdown(via)
    if dropdown is None:
        errors.append("VIA JSON 中找不到 id_qmk_rgb_matrix_effect 下拉框。")
    else:
        options = dropdown.get("options")
        if not isinstance(options, list) or not options:
            errors.append("VIA 灯效下拉框没有有效选项。")
        else:
            values = [option[1] for option in options if isinstance(option, list) and len(option) >= 2]
            if len(values) != len(options) or not all(isinstance(value, int) for value in values):
                errors.append("VIA 灯效下拉框存在无法解析的选项。")
            elif values != list(range(len(values))):
                errors.append(f"VIA 灯效编号必须从 0 开始连续递增，当前为 {values}。")

    return errors


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="校验 keyboard.json 与 VIA 定义文件")
    parser.add_argument("--keyboard", type=Path, default=DEFAULT_KEYBOARD, help="keyboard.json 路径")
    parser.add_argument("--via", type=Path, default=DEFAULT_VIA, help="VIA JSON 路径")
    args = parser.parse_args(argv)

    errors = validate_via_files(args.keyboard, args.via)
    if errors:
        print("VIA 定义校验失败：", file=sys.stderr)
        for message in errors:
            print(f"  - {message}", file=sys.stderr)
        return 1

    print("VIA 定义校验通过。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
