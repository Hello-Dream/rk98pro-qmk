#!/usr/bin/env python3
# Copyright 2026
# SPDX-License-Identifier: GPL-2.0-or-later
"""校验 C / Rust / TypeScript 三端状态快照协议契约。"""

from __future__ import annotations

import argparse
import ast
import re
import sys
from pathlib import Path

FIRMWARE_DIR = "keyboards/royalkludge/rk98pro_us_triple_mode"

FILES = {
    "status_h": f"{FIRMWARE_DIR}/hs/status.h",
    "status_c": f"{FIRMWARE_DIR}/hs/status.c",
    "config_h": f"{FIRMWARE_DIR}/config.h",
    "battery_c": f"{FIRMWARE_DIR}/hs/battery.c",
    "wireless_glue_h": f"{FIRMWARE_DIR}/hs/wireless_glue.h",
    "module_h": "keyboards/linker/wireless/module.h",
    "via_c": f"{FIRMWARE_DIR}/hs/via.c",
    "rust_status": "host/src-tauri/src/status.rs",
    "rust_device": "host/src-tauri/src/device.rs",
    "rust_runner": "host/src-tauri/src/monitor/runner.rs",
    "ts_types": "host/src/lib/types.ts",
}

# C 快照结构体字段顺序与类型；偏移由类型宽度累加得到。
C_STRUCT_FIELDS = [
    ("version", "uint8_t"),
    ("seq", "uint8_t"),
    ("fw_major", "uint8_t"),
    ("fw_minor", "uint8_t"),
    ("fw_patch", "uint8_t"),
    ("settings_version", "uint8_t"),
    ("battery", "uint8_t"),
    ("charging", "uint8_t"),
    ("link", "uint8_t"),
    ("mode", "uint8_t"),
    ("power_state", "uint8_t"),
    ("flags", "uint8_t"),
    ("locks", "uint8_t"),
    ("layer", "uint8_t"),
    ("default_layer", "uint8_t"),
    ("wpm", "uint8_t"),
    ("idle_seconds", "uint16_t"),
    ("rgbrec_channel", "uint8_t"),
    ("rgb_mode", "uint8_t"),
    ("rgb_brightness", "uint8_t"),
    ("wireless_version", "uint8_t"),
    ("reserved", "uint8_t"),
]

C_TYPE_SIZES = {
    "uint8_t": 1,
    "int8_t": 1,
    "uint16_t": 2,
    "int16_t": 2,
    "uint32_t": 4,
    "int32_t": 4,
}

C_MACRO_EXPECT = {
    "status_h": {
        "HS_STATUS_SNAPSHOT_SIZE": 23,
        "HS_STATUS_FLAG_ONLINE": 1,
        "HS_STATUS_FLAG_LOW_BATTERY": 2,
        "HS_STATUS_FLAG_USB": 4,
        "HS_STATUS_FLAG_RGBREC": 8,
        "HS_STATUS_FLAG_RGB_ON": 16,
        "HS_STATUS_FLAG_ANTI_BOUNCE": 32,
        "HS_STATUS_FLAG_MAC": 64,
        "HS_STATUS_LOCK_CAPS": 1,
        "HS_STATUS_LOCK_NUM": 2,
        "HS_STATUS_LOCK_SCROLL": 4,
        "HS_STATUS_LOCK_WIN": 8,
        "HS_STATUS_LOCK_CAPS_WORD": 16,
        "HS_CHARGING_DISCHARGING": 0,
        "HS_CHARGING_CHARGING": 1,
        "HS_CHARGING_FULL": 2,
        "HS_CHARGING_UNKNOWN": 0xFF,
    },
    "status_c": {
        "HS_STATUS_EVENT_MAGIC": 0xE0,
        "HS_STATUS_EVENT_STATUS": 0x01,
    },
    "config_h": {
        "HS_STATUS_SNAPSHOT_VERSION": 1,
    },
    "battery_c": {
        "HS_BATTERY_FULL_MIN": 98,
    },
}

C_ENUM_EXPECT = {
    "wireless_glue_h": {
        "id_host_status": 0x22,
        "HS_POWER_STATE_ACTIVE": 0,
        "HS_POWER_STATE_CHARGING": 1,
        "HS_POWER_STATE_FULL": 2,
        "HS_POWER_STATE_LOW_BATTERY": 3,
        "HS_POWER_STATE_DIM": 4,
        "HS_POWER_STATE_OFF": 5,
        "HS_POWER_STATE_UNKNOWN": 0xFF,
    },
    "module_h": {
        "DEVS_USB": 0,
        "DEVS_BT1": 1,
        "DEVS_BT2": 2,
        "DEVS_BT3": 3,
        "DEVS_2G4": 6,
        "MD_STATE_NONE": 0,
        "MD_STATE_PAIRING": 1,
        "MD_STATE_CONNECTED": 2,
        "MD_STATE_DISCONNECTED": 3,
        "MD_STATE_REJECT": 4,
    },
}

# Rust Status 结构体字段与类型。
RUST_STATUS_FIELDS = {
    "seq": "u8",
    "version": "u8",
    "firmware_version": "String",
    "settings_version": "u8",
    "battery": "Option<u8>",
    "charging": "u8",
    "link": "u8",
    "mode": "u8",
    "power_state": "u8",
    "flags": "u8",
    "locks": "u8",
    "layer": "u8",
    "default_layer": "u8",
    "wpm": "u8",
    "idle_seconds": "u16",
    "rgbrec_channel": "Option<u8>",
    "rgb_mode": "u8",
    "rgb_brightness": "u8",
    "wireless_version": "u8",
    "online": "bool",
    "updated_at": "u64",
}

RUST_CONST_EXPECT = {
    "rust_status": {
        "CHARGING_DISCHARGING": 0,
        "CHARGING_CHARGING": 1,
        "CHARGING_FULL": 2,
        "CHARGING_UNKNOWN": 0xFF,
        "LINK_CONNECTED": 2,
        "MODE_USB": 0,
        "MODE_2G4": 6,
        "FULL_BATTERY_MIN": 98,
        "SNAPSHOT_VERSION": 1,
    },
    "rust_device": {
        "ID_HOST_STATUS": 0x22,
        "EVENT_MAGIC": 0xE0,
        "EVENT_STATUS": 0x01,
    },
    "rust_runner": {
        "EVENT_MAGIC": 0xE0,
        "EVENT_STATUS": 0x01,
    },
}

# Rust Status::parse 的简单字段 -> value[N] 映射。
RUST_PARSE_INDEX = {
    "version": 0,
    "seq": 1,
    "settings_version": 5,
    "link": 8,
    "mode": 9,
    "power_state": 10,
    "flags": 11,
    "locks": 12,
    "layer": 13,
    "default_layer": 14,
    "wpm": 15,
    "rgb_mode": 19,
    "rgb_brightness": 20,
    "wireless_version": 21,
}

TS_CONST_EXPECT = {
    "CHARGING_DISCHARGING": 0,
    "CHARGING_CHARGING": 1,
    "CHARGING_FULL": 2,
    "CHARGING_UNKNOWN": 0xFF,
    "LINK_NONE": 0,
    "LINK_PAIRING": 1,
    "LINK_CONNECTED": 2,
    "LINK_DISCONNECTED": 3,
    "LINK_REJECT": 4,
    "LINK_UNKNOWN": 0xFF,
    "MODE_USB": 0,
    "MODE_BT1": 1,
    "MODE_BT2": 2,
    "MODE_BT3": 3,
    "MODE_2G4": 6,
    "MODE_UNKNOWN": 0xFF,
    "POWER_ACTIVE": 0,
    "POWER_CHARGING": 1,
    "POWER_FULL": 2,
    "POWER_LOW_BATTERY": 3,
    "POWER_DIM": 4,
    "POWER_OFF": 5,
    "POWER_UNKNOWN": 0xFF,
    "FLAG_ONLINE": 1,
    "FLAG_LOW_BATTERY": 2,
    "FLAG_USB": 4,
    "FLAG_RGBREC": 8,
    "FLAG_RGB_ON": 16,
    "FLAG_ANTI_BOUNCE": 32,
    "FLAG_MAC": 64,
    "LOCK_CAPS": 1,
    "LOCK_NUM": 2,
    "LOCK_SCROLL": 4,
    "LOCK_WIN": 8,
    "LOCK_CAPS_WORD": 16,
    "FULL_BATTERY_MIN": 98,
}

TS_STATUS_FIELDS = {
    "seq": "number",
    "version": "number",
    "firmwareVersion": "string",
    "settingsVersion": "number",
    "battery": "number | null",
    "charging": "number",
    "link": "number",
    "mode": "number",
    "powerState": "number",
    "flags": "number",
    "locks": "number",
    "layer": "number",
    "defaultLayer": "number",
    "wpm": "number",
    "idleSeconds": "number",
    "rgbrecChannel": "number | null",
    "rgbMode": "number",
    "rgbBrightness": "number",
    "wirelessVersion": "number",
    "online": "boolean",
    "updatedAt": "number",
}


def strip_comments(text: str) -> str:
    """把注释替换为等长空白，保留换行以便错误定位。"""

    def blank(match: re.Match[str]) -> str:
        return re.sub(r"[^\n]", " ", match.group(0))

    text = re.sub(r"/\*.*?\*/", blank, text, flags=re.S)
    return re.sub(r"//[^\n]*", blank, text)


def _eval_node(node: ast.AST) -> int:
    if isinstance(node, ast.Constant) and isinstance(node.value, int):
        return node.value
    if isinstance(node, ast.BinOp):
        left = _eval_node(node.left)
        right = _eval_node(node.right)
        if isinstance(node.op, ast.BitOr):
            return left | right
        if isinstance(node.op, ast.BitAnd):
            return left & right
        if isinstance(node.op, ast.LShift):
            return left << right
        if isinstance(node.op, ast.RShift):
            return left >> right
        if isinstance(node.op, ast.Add):
            return left + right
        if isinstance(node.op, ast.Sub):
            return left - right
    if isinstance(node, ast.UnaryOp):
        value = _eval_node(node.operand)
        if isinstance(node.op, ast.USub):
            return -value
        if isinstance(node.op, ast.Invert):
            return ~value
        if isinstance(node.op, ast.UAdd):
            return value
    raise ValueError("仅支持整数常量、括号和位运算")


def eval_int(expr: str) -> int:
    """安全求值 C/Rust/TS 中的整数常量表达式。"""
    cleaned = expr.strip().rstrip(";").strip()
    # 只剥离完整数字字面量后的 C 整型后缀，兼容 0xFFu / 0xE0UL，且不误伤标识符
    cleaned = re.sub(r"\b(0[xX][0-9A-Fa-f]+|0[bB][01]+|\d+)(?:[uUlL]+)\b", r"\1", cleaned)
    try:
        tree = ast.parse(cleaned, mode="eval")
    except SyntaxError as exc:
        raise ValueError(f"无法解析表达式 `{expr}`") from exc
    return _eval_node(tree.body)


def normalize_space(text: str) -> str:
    """折叠空白，并消除括号/逗号/分号旁的排版差异。"""
    text = re.sub(r"\s+", " ", text).strip()
    text = re.sub(r"\(\s+", "(", text)
    text = re.sub(r"\s+\)", ")", text)
    text = re.sub(r"\s+([,;])", r"\1", text)
    return text


def function_body(text: str, name: str) -> str | None:
    """按大括号配对截取函数体，找不到时返回 None。"""
    match = re.search(
        r"\b"
        + re.escape(name)
        + r"\s*\([^;{]*\)\s*(?:(?:->|:)\s*[^;{]+)?\{",
        text,
    )
    if not match:
        return None
    start = match.end() - 1
    depth = 0
    for index in range(start, len(text)):
        char = text[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return text[start : index + 1]
    return None


def parse_c_defines(text: str) -> dict[str, tuple[str, int]]:
    result: dict[str, tuple[str, int]] = {}
    for match in re.finditer(
        r"^[ \t]*#define[ \t]+([A-Za-z_]\w*)[ \t]+([^\n]+)", text, re.M
    ):
        result[match.group(1)] = (match.group(2).strip(), match.start())
    return result


def parse_c_enums(text: str) -> dict[str, tuple[int, int]]:
    result: dict[str, tuple[int, int]] = {}
    for block in re.finditer(r"\benum\s*(?:\w+\s*)?\{([^}]*)\}", text):
        body_start = block.start(1)
        body = block.group(1)
        next_value = 0
        offset = 0
        for raw in body.split(","):
            entry = raw.strip()
            entry_pos = body_start + offset + (len(raw) - len(raw.lstrip()))
            offset += len(raw) + 1
            if not entry:
                continue
            match = re.fullmatch(r"([A-Za-z_]\w*)\s*(?:=\s*(.+))?", entry)
            if not match:
                continue
            name = match.group(1)
            expr = match.group(2)
            if expr is None:
                value = next_value
            else:
                try:
                    value = eval_int(expr)
                except ValueError:
                    continue
            result[name] = (value, entry_pos)
            next_value = value + 1
    return result


def parse_rust_consts(text: str) -> dict[str, tuple[str, int]]:
    result: dict[str, tuple[str, int]] = {}
    for match in re.finditer(
        r"\bconst\s+([A-Za-z_]\w*)\s*:\s*[^=]+=\s*([^;]+);", text
    ):
        result[match.group(1)] = (match.group(2).strip(), match.start())
    return result


def parse_ts_consts(text: str) -> dict[str, tuple[str, int]]:
    result: dict[str, tuple[str, int]] = {}
    for match in re.finditer(
        r"export\s+const\s+([A-Za-z_]\w*)\s*=\s*([^;]+);", text
    ):
        result[match.group(1)] = (match.group(2).strip(), match.start())
    return result


def camel_case(name: str) -> str:
    head, *rest = name.split("_")
    return head + "".join(part.capitalize() for part in rest)


class Contract:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.text: dict[str, str] = {}
        self.failures: list[tuple[str, int, str]] = []
        self.checks = 0

        for key, relative in FILES.items():
            path = root / relative
            if not path.is_file():
                self.failures.append((relative, 1, "文件不存在，无法校验协议契约"))
                continue
            self.text[key] = strip_comments(path.read_text(encoding="utf-8"))

    def fail(self, key: str, message: str, token: str | None = None) -> None:
        line = 1
        text = self.text.get(key, "")
        if token:
            index = text.find(token)
            if index >= 0:
                line = text.count("\n", 0, index) + 1
        self.failures.append((FILES[key], line, message))

    def fail_at(self, key: str, position: int, message: str) -> None:
        text = self.text.get(key, "")
        line = text.count("\n", 0, position) + 1 if text else 1
        self.failures.append((FILES[key], line, message))

    def check(self, condition: bool, key: str, message: str, token: str | None = None) -> None:
        self.checks += 1
        if not condition:
            self.fail(key, message, token)

    def check_defines(self, key: str, expected: dict[str, int]) -> None:
        if key not in self.text:
            return
        defines = parse_c_defines(self.text[key])
        for name, want in expected.items():
            if name not in defines:
                self.fail(key, f"缺少宏 {name}（期望 {want}）", name)
                continue
            expr, position = defines[name]
            try:
                got = eval_int(expr)
            except ValueError as exc:
                self.fail_at(key, position, f"{name} 无法求值：{exc}")
                continue
            if got != want:
                self.fail_at(key, position, f"{name} 期望 {want}，实际 {got}")

    def check_enums(self, key: str, expected: dict[str, int]) -> None:
        if key not in self.text:
            return
        values = parse_c_enums(self.text[key])
        for name, want in expected.items():
            if name not in values:
                self.fail(key, f"缺少枚举项 {name}（期望 {want}）", name)
                continue
            got, position = values[name]
            if got != want:
                self.fail_at(key, position, f"{name} 期望 {want}，实际 {got}")

    def check_consts(
        self, key: str, expected: dict[str, int], parser
    ) -> None:
        if key not in self.text:
            return
        values = parser(self.text[key])
        for name, want in expected.items():
            if name not in values:
                self.fail(key, f"缺少常量 {name}（期望 {want}）", name)
                continue
            expr, position = values[name]
            try:
                got = eval_int(expr)
            except ValueError as exc:
                self.fail_at(key, position, f"{name} 无法求值：{exc}")
                continue
            if got != want:
                self.fail_at(key, position, f"{name} 期望 {want}，实际 {got}")

    def require_function(self, key: str, name: str) -> str | None:
        if key not in self.text:
            return None
        body = function_body(self.text[key], name)
        if body is None:
            self.fail(key, f"缺少函数 {name}", name)
        return body

    def require_snippets(
        self,
        key: str,
        body: str | None,
        snippets: list[tuple[str, str]],
        owner: str,
    ) -> None:
        if body is None:
            return
        normalized = normalize_space(body)
        for needle, label in snippets:
            self.check(
                normalize_space(needle) in normalized,
                key,
                f"{owner} 缺少/偏移 {label}：期望 `{normalize_space(needle)}`",
                owner,
            )

    def require_regexes(
        self,
        key: str,
        body: str | None,
        patterns: list[tuple[str, str]],
        owner: str,
    ) -> None:
        """按正则匹配函数体，要求每个模式至少命中一次。"""
        if body is None:
            return
        normalized = normalize_space(body)
        for pattern, label in patterns:
            self.check(
                re.search(pattern, normalized) is not None,
                key,
                f"{owner} 缺少/偏移 {label}：期望匹配 /{pattern}/",
                owner,
            )


def check_c_snapshot(contract: Contract) -> None:
    key = "status_h"
    if key not in contract.text:
        return
    text = contract.text[key]

    match = re.search(
        r"typedef\s+struct\s+([^{;]*)\{([^}]*)\}\s*hs_status_snapshot_t\s*;",
        text,
        re.S,
    )
    if not match:
        contract.fail(key, "缺少 hs_status_snapshot_t 结构体", "hs_status_snapshot_t")
        return

    contract.check(
        "PACKED" in match.group(1),
        key,
        "hs_status_snapshot_t 必须使用 PACKED 声明",
        "typedef struct",
    )

    fields = [
        (field.group(1), field.group(2))
        for field in re.finditer(
            r"([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*;", match.group(2)
        )
    ]
    expected_names = [name for name, _ in C_STRUCT_FIELDS]
    actual_names = [name for _, name in fields]
    if actual_names != expected_names:
        contract.fail(
            key,
            f"hs_status_snapshot_t 字段顺序/集合不一致：期望 {expected_names}，实际 {actual_names}",
            "hs_status_snapshot_t",
        )
        return

    offset = 0
    for (expected_name, expected_type), (actual_type, _) in zip(
        C_STRUCT_FIELDS, fields
    ):
        if actual_type != expected_type:
            contract.fail(
                key,
                f"{expected_name} 类型期望 {expected_type}，实际 {actual_type}",
                expected_name,
            )
        size = C_TYPE_SIZES.get(actual_type)
        if size is None:
            contract.fail(key, f"{expected_name} 使用了未知类型 {actual_type}", expected_name)
            return
        offset += size

    if offset != 23:
        contract.fail(
            key,
            f"hs_status_snapshot_t 实际大小 {offset}，期望 23",
            "hs_status_snapshot_t",
        )

    contract.check(
        re.search(
            r"_Static_assert\s*\(\s*sizeof\s*\(\s*hs_status_snapshot_t\s*\)\s*==\s*HS_STATUS_SNAPSHOT_SIZE\b",
            text,
        )
        is not None,
        key,
        "缺少 `_Static_assert(sizeof(hs_status_snapshot_t) == HS_STATUS_SNAPSHOT_SIZE)`",
        "_Static_assert",
    )


def check_c_assignments(contract: Contract) -> None:
    build = contract.require_function("status_c", "hs_status_build_snapshot")
    contract.require_snippets(
        "status_c",
        build,
        [
            ("snapshot->version = HS_STATUS_SNAPSHOT_VERSION;", "version 赋值"),
            ("snapshot->fw_major = HS_FIRMWARE_VERSION_MAJOR;", "fw_major 赋值"),
            ("snapshot->fw_minor = HS_FIRMWARE_VERSION_MINOR;", "fw_minor 赋值"),
            ("snapshot->fw_patch = HS_FIRMWARE_VERSION_PATCH;", "fw_patch 赋值"),
            ("snapshot->settings_version = HS_SETTINGS_VERSION;", "settings_version 赋值"),
            ("snapshot->battery = battery > 100 ? 0xFF : battery;", "battery 哨兵值"),
            (
                "snapshot->charging = hs_battery_charging() ? (hs_battery_is_full() ? HS_CHARGING_FULL : HS_CHARGING_CHARGING) : HS_CHARGING_DISCHARGING;",
                "charging 命名常量",
            ),
            ("snapshot->power_state = hs_power_state_code();", "power_state 来源"),
            ("snapshot->layer = get_highest_layer(layer_state);", "layer 来源"),
            (
                "snapshot->default_layer = get_highest_layer(default_layer_state);",
                "default_layer 来源",
            ),
            ("snapshot->idle_seconds = hs_power_idle_seconds();", "idle_seconds 来源"),
            ("snapshot->link = *md_getp_state();", "link 来源"),
            ("snapshot->mode = wireless_get_current_devs();", "mode 来源"),
            ("snapshot->wireless_version = md_get_version();", "wireless_version 来源"),
            (
                "snapshot->rgbrec_channel = rgbrec_is_started() ? hs_get_rgbrec_channel() : 0xFF;",
                "rgbrec_channel 哨兵值",
            ),
            ("snapshot->wpm = get_current_wpm();", "wpm 来源"),
            ("snapshot->rgb_mode = rgb_matrix_get_mode();", "rgb_mode 来源"),
            ("snapshot->rgb_brightness = rgb_matrix_get_val();", "rgb_brightness 来源"),
        ],
        "hs_status_build_snapshot",
    )

    get_body = contract.require_function("status_c", "hs_status_get")
    contract.require_snippets(
        "status_c",
        get_body,
        [("snapshot.seq = hs_status_seq;", "GET seq 来源")],
        "hs_status_get",
    )

    task_body = contract.require_function("status_c", "hs_status_task")
    contract.require_snippets(
        "status_c",
        task_body,
        [("snapshot.seq = ++hs_status_seq;", "事件 seq 自增")],
        "hs_status_task",
    )


def check_via(contract: Contract) -> None:
    via = contract.text.get("via_c", "")
    normalized = normalize_space(via)
    contract.check(
        "case id_host_status:" in normalized,
        "via_c",
        "via.c 缺少 `case id_host_status` 分发",
        "id_host_status",
    )
    contract.check(
        "hs_status_get(value_data, (uint8_t)(length - 3))" in normalized,
        "via_c",
        "via.c 的 GET 3 字节头偏移不是 length - 3",
        "hs_status_get",
    )

    get_body = contract.require_function("status_c", "hs_status_get")
    contract.require_snippets(
        "status_c",
        get_body,
        [("if (value_data == NULL || value_len < HS_STATUS_SNAPSHOT_SIZE) {", "GET 长度守卫")],
        "hs_status_get",
    )

    if "rust_device" in contract.text:
        text = normalize_space(contract.text["rust_device"])
        contract.check(
            "let value = self.read_custom_value(ID_HOST_STATUS)?;" in text,
            "rust_device",
            "device.rs 的 get_status 未使用 ID_HOST_STATUS",
            "ID_HOST_STATUS",
        )
        contract.check(
            "Status::parse(&value)" in text,
            "rust_device",
            "device.rs 的 get_status 未调用 Status::parse",
            "Status::parse",
        )


def check_c_events(contract: Contract) -> None:
    if "status_c" not in contract.text:
        return
    send_body = contract.require_function("status_c", "hs_status_send_event")
    contract.require_snippets(
        "status_c",
        send_body,
        [
            ("report[0] = HS_STATUS_EVENT_MAGIC;", "事件 magic 字节"),
            ("report[1] = HS_STATUS_EVENT_STATUS;", "事件状态字节"),
            ("report[2] = HS_STATUS_SNAPSHOT_SIZE;", "事件长度字节"),
            ("memcpy(&report[3], snapshot, sizeof(*snapshot));", "快照事件偏移"),
        ],
        "hs_status_send_event",
    )


def check_battery_threshold(contract: Contract) -> None:
    """校验充满阈值宏实际传入电池状态机，而不只是宏定义存在。"""
    task = contract.require_function("battery_c", "hs_battery_task")
    contract.require_snippets(
        "battery_c",
        task,
        [("now, HS_BATTERY_FULL_MIN, HS_BATTERY_FULL_HYSTERESIS", "充满阈值实参")],
        "hs_battery_task",
    )


def check_rust_status(contract: Contract) -> None:
    key = "rust_status"
    if key not in contract.text:
        return
    text = contract.text[key]

    contract.check(
        re.search(
            r'#\[serde\(\s*rename_all\s*=\s*"camelCase"\s*\)\]\s*(?:#\[[^\]]*\]\s*)*pub\s+struct\s+Status',
            text,
        )
        is not None,
        key,
        'Status 缺少 #[serde(rename_all = "camelCase")]',
        "pub struct Status",
    )

    match = re.search(r"pub\s+struct\s+Status\s*\{([^}]*)\}", text, re.S)
    if not match:
        contract.fail(key, "缺少 pub struct Status", "pub struct Status")
        return

    fields = {
        field.group(1): field.group(2).strip()
        for field in re.finditer(r"pub\s+([A-Za-z_]\w*)\s*:\s*([^,]+),", match.group(1))
    }
    if set(fields) != set(RUST_STATUS_FIELDS):
        missing = sorted(set(RUST_STATUS_FIELDS) - set(fields))
        extra = sorted(set(fields) - set(RUST_STATUS_FIELDS))
        contract.fail(
            key,
            f"Status 字段集合不一致：缺少 {missing}，多出 {extra}",
            "pub struct Status",
        )
    for name, expected_type in RUST_STATUS_FIELDS.items():
        if name in fields and fields[name] != expected_type:
            contract.fail(
                key,
                f"Status.{name} 类型期望 {expected_type}，实际 {fields[name]}",
                f"pub {name}",
            )

    parse = contract.require_function(key, "parse")
    if parse is None:
        return

    snippets: list[tuple[str, str]] = [
        (
            'firmware_version: format!("{}.{}.{}", value[2], value[3], value[4]),',
            "firmware_version 映射",
        ),
        (
            "let battery = if value[6] <= 100 { Some(value[6]) } else { None };",
            "battery 映射",
        ),
        (
            "let mut charging = if value[7] <= CHARGING_FULL { value[7] } else { CHARGING_UNKNOWN };",
            "charging 映射",
        ),
        (
            "if charging == CHARGING_FULL && (battery.is_none() || battery.unwrap_or(0) < FULL_BATTERY_MIN) { charging = CHARGING_CHARGING; }",
            "FULL 降级",
        ),
        (
            "let online = flags & 1 != 0 || mode == MODE_USB || link == LINK_CONNECTED;",
            "online 表达式",
        ),
        (
            "idle_seconds: u16::from_le_bytes([value[16], value[17]]),",
            "idle_seconds 映射",
        ),
        (
            "rgbrec_channel: if value[18] <= 3 { Some(value[18]) } else { None },",
            "rgbrec_channel 映射",
        ),
    ]
    contract.require_snippets(key, parse, snippets, "pub fn parse")
    # 字段名加前置边界，避免 layer 被 default_layer 之类的子串误命中
    contract.require_regexes(
        key,
        parse,
        [
            (
                rf"(?:(?:^|[\s,{{]){re.escape(field)}:\s*value\[{index}\]|\blet\s+{re.escape(field)}\s*=\s*value\[{index}\];)",
                f"{field} -> value[{index}]",
            )
            for field, index in RUST_PARSE_INDEX.items()
        ],
        "pub fn parse",
    )

    contract.check(
        "if value.len() < 23 {" in normalize_space(parse),
        key,
        "Status::parse 缺少 `if value.len() < 23 {` 长度守卫",
        "value.len() < 23",
    )

    indices = [int(index) for index in re.findall(r"value\[(\d+)\]", parse)]
    if any(index > 22 for index in indices):
        contract.fail(
            key,
            f"Status::parse 读取了越界索引：{sorted(set(i for i in indices if i > 22))}",
            "value[",
        )
    missing_indices = sorted(set(range(0, 22)) - set(indices))
    if missing_indices:
        contract.fail(
            key,
            f"Status::parse 未读取快照索引 {missing_indices}",
            "Status::parse",
        )

    for name, expected in (("mode_label", {0, 1, 2, 3, 6}), ("link_label", {0, 1, 2, 3, 4})):
        body = contract.require_function(key, name)
        if body is None:
            continue
        branches = {int(value) for value in re.findall(r"(?m)^\s*(\d+)\s*=>", body)}
        if branches != expected:
            contract.fail(
                key,
                f"{name} 数字分支期望 {sorted(expected)}，实际 {sorted(branches)}",
                name,
            )


def check_rust_device(contract: Contract) -> None:
    key = "rust_device"
    if key not in contract.text:
        return
    poll = contract.require_function(key, "poll_events")
    contract.require_snippets(
        key,
        poll,
        [
            (
                "if response[0] == EVENT_MAGIC && response.len() >= 2 && response[1] == EVENT_STATUS {",
                "poll_events magic/status/长度条件",
            ),
        ],
        "poll_events",
    )
    read = contract.require_function(key, "read_custom_value")
    contract.require_snippets(
        key,
        read,
        [
            ("if response[0] == EVENT_MAGIC {", "read_custom_value 事件入队"),
            ("return Ok(response[3..].to_vec());", "read_custom_value 3 字节头偏移"),
        ],
        "read_custom_value",
    )

    get_status = contract.require_function(key, "get_status")
    contract.require_snippets(
        key,
        get_status,
        [
            ("let value = self.read_custom_value(ID_HOST_STATUS)?;", "get_status 使用 0x22"),
            ("Status::parse(&value)", "get_status 解析快照"),
        ],
        "get_status",
    )


def check_rust_runner(contract: Contract) -> None:
    key = "rust_runner"
    if key not in contract.text:
        return
    text = normalize_space(contract.text[key])
    contract.check(
        "if event.len() >= 3 && event[0] == EVENT_MAGIC && event[1] == EVENT_STATUS {"
        in text,
        key,
        "runner.rs 缺少事件长度/magic/status 条件",
        "event.len()",
    )
    contract.check(
        "Status::parse(&event[3..])" in text,
        key,
        "runner.rs 的快照偏移不是 event[3..]",
        "Status::parse",
    )


def check_ts_status(contract: Contract) -> None:
    key = "ts_types"
    if key not in contract.text:
        return
    text = contract.text[key]

    match = re.search(r"export\s+interface\s+Status\s*\{([^}]*)\}", text, re.S)
    if not match:
        contract.fail(key, "缺少 export interface Status", "export interface Status")
        return

    fields = {
        field.group(1): normalize_space(field.group(2))
        for field in re.finditer(r"([A-Za-z_]\w*)\s*:\s*([^;]+);", match.group(1))
    }
    if set(fields) != set(TS_STATUS_FIELDS):
        missing = sorted(set(TS_STATUS_FIELDS) - set(fields))
        extra = sorted(set(fields) - set(TS_STATUS_FIELDS))
        contract.fail(
            key,
            f"TS Status 字段集合不一致：缺少 {missing}，多出 {extra}",
            "export interface Status",
        )
    for name, expected_type in TS_STATUS_FIELDS.items():
        if name in fields and fields[name] != expected_type:
            contract.fail(
                key,
                f"TS Status.{name} 类型期望 `{expected_type}`，实际 `{fields[name]}`",
                f"{name}:",
            )

    rust_names = {camel_case(name) for name in RUST_STATUS_FIELDS}
    if rust_names != set(TS_STATUS_FIELDS):
        contract.fail(
            key,
            f"Rust camelCase 字段与 TS Status 不一致：Rust 独有 {sorted(rust_names - set(TS_STATUS_FIELDS))}，TS 独有 {sorted(set(TS_STATUS_FIELDS) - rust_names)}",
            "export interface Status",
        )

    body = contract.require_function(key, "effectiveCharging")
    contract.require_snippets(
        key,
        body,
        [
            (
                "if (status.charging === CHARGING_FULL && (status.battery == null || status.battery < FULL_BATTERY_MIN)) {",
                "FULL 降级条件",
            ),
            ("return CHARGING_CHARGING;", "FULL 降级结果"),
            ("return status.charging;", "原始 charging 返回"),
        ],
        "effectiveCharging",
    )


def run_checks(contract: Contract) -> None:
    for key, expected in C_MACRO_EXPECT.items():
        contract.check_defines(key, expected)
    for key, expected in C_ENUM_EXPECT.items():
        contract.check_enums(key, expected)
    for key, expected in RUST_CONST_EXPECT.items():
        contract.check_consts(key, expected, parse_rust_consts)
    contract.check_consts("ts_types", TS_CONST_EXPECT, parse_ts_consts)

    check_c_snapshot(contract)
    check_c_assignments(contract)
    check_c_events(contract)
    check_battery_threshold(contract)
    check_via(contract)
    check_rust_status(contract)
    check_rust_device(contract)
    check_rust_runner(contract)
    check_ts_status(contract)


def main() -> int:
    parser = argparse.ArgumentParser(description="校验状态快照协议契约")
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="仓库根目录，默认为脚本所在仓库",
    )
    args = parser.parse_args()

    contract = Contract(args.root.resolve())
    run_checks(contract)

    if contract.failures:
        for relative, line, message in contract.failures:
            print(f"{relative}:{line}: {message}", file=sys.stderr)
        print(
            f"状态协议契约校验失败：{len(contract.failures)} 项",
            file=sys.stderr,
        )
        return 1

    print("状态协议契约校验通过：23 字节快照 v1")
    print(
        "覆盖：C 布局/关键赋值/事件/电池阈值/VIA 0x22，Rust 字段映射/online/FULL 降级/label，TS 字段/常量/effectiveCharging"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
