#!/usr/bin/env python3
"""检查会破坏 EEPROM / VIA 兼容性的改动。

检查项：
- EEPROM 布局（设置块 / RGB 录制 / HSV 槽位）变化必须递增 HS_SETTINGS_VERSION；
- 自定义键码顺序、VIA customKeycodes 与 keyboard.json 必须保持一致；
- VIA ID 0x01 固定为上位机 RPC，0x10 起连续分配；主版本升级允许断代重排；
- RGB 灯效实际启用顺序变化必须递增 HS_SETTINGS_VERSION，VIA 效果编号必须连续；
- debounce 必须保持 10ms。
"""

from __future__ import annotations

import argparse
import ast
import json
import re
import subprocess
import sys
from pathlib import Path
from typing import Any, Optional

REPO_ROOT = Path(__file__).resolve().parents[1]
KEYBOARD_DIR = REPO_ROOT / "keyboards/royalkludge/rk98pro_us_triple_mode"
CONFIG_H = KEYBOARD_DIR / "config.h"
KEYBOARD_JSON = KEYBOARD_DIR / "keyboard.json"
VIA_JSON = KEYBOARD_DIR / "rk98pro_us_triple_mode.json"
MAIN_C = KEYBOARD_DIR / "rk98pro_us_triple_mode.c"
RGB_USER_INC = KEYBOARD_DIR / "rgb_matrix_user.inc"
RGB_RECORD_C = KEYBOARD_DIR / "rgb_record/rgb_record.c"
WLS_H = KEYBOARD_DIR / "hs/wireless_glue.h"
OLD_WLS_H = KEYBOARD_DIR / "wls/wls.h"
STORE_H = KEYBOARD_DIR / "hs/store.h"
OLD_STORE_H = KEYBOARD_DIR / "wls/hs_store.h"
HOST_PACKAGE_JSON = REPO_ROOT / "host/package.json"
HOST_CARGO_TOML = REPO_ROOT / "host/src-tauri/Cargo.toml"
HOST_TAURI_CONF = REPO_ROOT / "host/src-tauri/tauri.conf.json"

ZERO_SHA_RE = re.compile(r"^0+$")
INT_SUFFIX_RE = re.compile(r"(?<=\d)(?:ULL|UL|LL|U|L)\b")
CAST_RE = re.compile(r"\(\s*(?:u?int(?:8|16|32|64)_t|unsigned\s+long)\s*\)")
IDENT_RE = re.compile(r"\b[A-Za-z_]\w*\b")


class GitError(RuntimeError):
    """git 命令失败。"""


class Report:
    """收集检查结果。"""

    def __init__(self) -> None:
        self.errors: list[str] = []
        self.notes: list[str] = []

    def error(self, message: str) -> None:
        self.errors.append(message)

    def note(self, message: str) -> None:
        self.notes.append(message)


def git(*args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    """在仓库根目录执行 git 命令。"""
    proc = subprocess.run(
        ["git", "-C", str(REPO_ROOT), *args],
        text=True,
        capture_output=True,
    )
    if check and proc.returncode != 0:
        detail = proc.stderr.strip() or proc.stdout.strip() or "git 命令执行失败"
        raise GitError(detail)
    return proc


def resolve_commit(ref: str) -> Optional[str]:
    """把引用解析成完整提交哈希；引用不存在时返回 None。"""
    if not ref or ZERO_SHA_RE.fullmatch(ref):
        return None
    proc = git("rev-parse", "--verify", f"{ref}^{{commit}}", check=False)
    if proc.returncode != 0:
        return None
    return proc.stdout.strip()


def read_text(commit: Optional[str], path: Path) -> Optional[str]:
    """读取某个提交里的文件；commit 为 None 时读取当前工作区。"""
    if commit is None:
        return path.read_text(encoding="utf-8") if path.is_file() else None

    rel_path = path.relative_to(REPO_ROOT).as_posix()
    proc = git("show", f"{commit}:{rel_path}", check=False)
    if proc.returncode != 0:
        return None
    return proc.stdout


def read_text_any(commit: Optional[str], paths: tuple[Path, ...]) -> Optional[str]:
    """按顺序读取多个候选路径，兼容重构前后的文件位置。"""
    for path in paths:
        text = read_text(commit, path)
        if text is not None:
            return text
    return None


def read_json(commit: Optional[str], path: Path) -> Optional[dict[str, Any]]:
    """读取并解析 JSON 文件。"""
    text = read_text(commit, path)
    if text is None:
        return None
    try:
        data = json.loads(text)
    except json.JSONDecodeError as exc:
        raise ValueError(f"{path.relative_to(REPO_ROOT)} 不是合法 JSON：{exc}") from exc
    return data if isinstance(data, dict) else None


def parse_defines(text: str) -> dict[str, str]:
    """提取简单 #define 的名称和值，忽略函数式宏和行尾注释。"""
    defines: dict[str, str] = {}
    for line in text.splitlines():
        match = re.match(r"\s*#\s*define\s+([A-Za-z_]\w*)(?:\s+(.*))?$", line)
        if not match:
            continue
        name = match.group(1)
        value = (match.group(2) or "").split("//", 1)[0].strip()
        defines[name] = value or "1"
    return defines


def _eval_ast(node: ast.AST) -> int:
    """安全地计算只包含整数运算的表达式。"""
    if isinstance(node, ast.Expression):
        return _eval_ast(node.body)
    if isinstance(node, ast.Constant) and isinstance(node.value, int):
        return node.value
    if isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.UAdd, ast.USub, ast.Invert)):
        value = _eval_ast(node.operand)
        if isinstance(node.op, ast.UAdd):
            return value
        if isinstance(node.op, ast.USub):
            return -value
        return ~value
    if isinstance(node, ast.BinOp):
        left = _eval_ast(node.left)
        right = _eval_ast(node.right)
        operators = {
            ast.Add: lambda a, b: a + b,
            ast.Sub: lambda a, b: a - b,
            ast.Mult: lambda a, b: a * b,
            ast.FloorDiv: lambda a, b: a // b,
            ast.Mod: lambda a, b: a % b,
            ast.LShift: lambda a, b: a << b,
            ast.RShift: lambda a, b: a >> b,
            ast.BitOr: lambda a, b: a | b,
            ast.BitAnd: lambda a, b: a & b,
            ast.BitXor: lambda a, b: a ^ b,
        }
        for op_type, func in operators.items():
            if isinstance(node.op, op_type):
                return func(left, right)
    raise ValueError("不支持的整数表达式")


def eval_int(
    expression: str,
    defines: dict[str, str],
    dimensions: tuple[int, int],
    seen: Optional[set[str]] = None,
) -> int:
    """展开宏后计算整数表达式，用于比较 EEPROM 布局。"""
    resolved = eval_expr_text(expression, defines, dimensions, seen)
    return _eval_ast(ast.parse(resolved, mode="eval"))


def eval_expr_text(
    expression: str,
    defines: dict[str, str],
    dimensions: tuple[int, int],
    seen: Optional[set[str]] = None,
) -> str:
    """把表达式中的宏递归替换成数字，返回可继续求值的文本。"""
    seen = set() if seen is None else seen
    expression = CAST_RE.sub("", expression)
    expression = INT_SUFFIX_RE.sub("", expression)

    rows, cols = dimensions

    def replace(name_match: re.Match[str]) -> str:
        name = name_match.group(0)
        if name == "MATRIX_ROWS":
            return str(rows)
        if name == "MATRIX_COLS":
            return str(cols)
        if name in defines and name not in seen:
            return f"({eval_expr_text(defines[name], defines, dimensions, seen | {name})})"
        return name

    return IDENT_RE.sub(replace, expression)


def struct_signature(source: Optional[str], name: str) -> Optional[str]:
    """提取结构体 / 联合体正文，用于检测 EEPROM 字段布局变化。"""
    if not source:
        return None

    for match in re.finditer(r"typedef\s+(?:union|struct)\s*(?:PACKED\s*)?\{", source):
        start = match.end() - 1
        depth = 0
        for index in range(start, len(source)):
            if source[index] == "{":
                depth += 1
            elif source[index] == "}":
                depth -= 1
                if depth != 0:
                    continue
                tail = source[index + 1 : source.find(";", index) + 1]
                ident = re.search(r"(\w+)\s*;", tail)
                if ident and ident.group(1) == name:
                    body = source[start + 1 : index]
                    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
                    body = re.sub(r"//[^\n]*", "", body)
                    return re.sub(r"\s+", " ", body).strip()
                break
    return None


def settings_version(commit: Optional[str], keyboard: Optional[dict[str, Any]]) -> Optional[int]:
    """读取 HS_SETTINGS_VERSION。"""
    config = read_text(commit, CONFIG_H)
    if config is None:
        return None
    defines = parse_defines(config)
    raw = defines.get("HS_SETTINGS_VERSION")
    if raw is None:
        return None
    rows = len((keyboard or {}).get("matrix_pins", {}).get("rows", []))
    cols = len((keyboard or {}).get("matrix_pins", {}).get("cols", []))
    try:
        return eval_int(raw, defines, (rows, cols))
    except (ValueError, SyntaxError):
        return None


def eeprom_layout(
    commit: Optional[str], keyboard: Optional[dict[str, Any]]
) -> Optional[dict[str, Any]]:
    """生成 EEPROM 布局签名；配置文件缺失时返回 None。"""
    config = read_text(commit, CONFIG_H)
    source = "\n".join(
        text
        for text in (
            read_text(commit, MAIN_C),
            read_text_any(commit, (STORE_H, OLD_STORE_H)),
        )
        if text is not None
    )
    if config is None or keyboard is None:
        return None

    defines = parse_defines(config)
    rows = len(keyboard.get("matrix_pins", {}).get("rows", []))
    cols = len(keyboard.get("matrix_pins", {}).get("cols", []))
    dimensions = (rows, cols)

    layout: dict[str, Any] = {}
    macros = (
        "RGBREC_CHANNEL_NUM",
        "EECONFIG_CONFINFO_USE_SIZE",
        "EECONFIG_POWER_USE_SIZE",
        "EECONFIG_RGBREC_USE_SIZE",
        "HS_SETTINGS_BLOB_SIZE",
        "HS_RGBREC_DATA_SIZE",
        "EECONFIG_USER_DATA_SIZE",
        "EECONFIG_USER_DATA_VERSION",
    )
    for macro in macros:
        raw = defines.get(macro)
        if raw is None:
            continue
        try:
            layout[macro] = eval_int(raw, defines, dimensions)
        except (ValueError, SyntaxError):
            layout[macro] = raw

    for struct in ("confinfo_t", "power_config_t", "hs_settings_t", "hs_power_t"):
        signature = struct_signature(source, struct)
        if signature is not None:
            layout[struct] = signature

    return layout


def enabled_builtin_effects(keyboard: dict[str, Any]) -> tuple[str, ...]:
    """返回 keyboard.json 中启用的内置灯效名称。"""
    animations = keyboard.get("rgb_matrix", {}).get("animations", {})
    return tuple(sorted(f"RGB_MATRIX_{name.upper()}" for name, enabled in animations.items() if enabled))


def _eval_preprocessor_if(expression: str, defines: dict[str, str]) -> bool:
    """计算 rgb_matrix_user.inc 里简单的 #if 条件。"""
    expression = expression.split("//", 1)[0].strip()
    expression = re.sub(
        r"defined\s*\(\s*(\w+)\s*\)",
        lambda match: "1" if match.group(1) in defines else "0",
        expression,
    )
    expression = re.sub(
        r"defined\s+(\w+)",
        lambda match: "1" if match.group(1) in defines else "0",
        expression,
    )

    def replace(name_match: re.Match[str]) -> str:
        name = name_match.group(0)
        if name in {"and", "or", "not"}:
            return name
        return "1" if name in defines else "0"

    expression = IDENT_RE.sub(replace, expression)
    expression = expression.replace("&&", " and ").replace("||", " or ")
    expression = expression.replace("!", " not ")
    try:
        return bool(eval(expression, {"__builtins__": {}}, {}))
    except (SyntaxError, TypeError, NameError):
        return False


def active_custom_effects(config_text: str, inc_text: str) -> tuple[str, ...]:
    """按预处理条件计算实际参与 RGB_MATRIX_EFFECT 枚举的自定义灯效顺序。"""
    defines = parse_defines(config_text)
    # QMK 会根据 RGB_MATRIX_KEYPRESSES 定义 RGB_MATRIX_KEYREACTIVE_ENABLED
    if "RGB_MATRIX_KEYPRESSES" in defines:
        defines.setdefault("RGB_MATRIX_KEYREACTIVE_ENABLED", "1")

    effects: list[str] = []
    # 每项为 (父级是否启用, 当前分支是否已命中, 当前是否启用)
    stack: list[tuple[bool, bool, bool]] = []
    active = True

    for raw_line in inc_text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("//"):
            continue

        directive = re.match(r"#\s*(ifdef|ifndef|if|elif|else|endif)\b(.*)", line)
        if directive:
            keyword = directive.group(1)
            rest = directive.group(2).strip()
            if keyword in {"ifdef", "ifndef", "if"}:
                if keyword == "ifdef":
                    condition = bool(rest) and rest.split()[0] in defines
                elif keyword == "ifndef":
                    condition = not rest or rest.split()[0] not in defines
                else:
                    condition = _eval_preprocessor_if(rest, defines)
                stack.append((active, condition, active and condition))
                active = active and condition
            elif keyword == "else" and stack:
                parent, condition, _ = stack[-1]
                active = parent and not condition
                stack[-1] = (parent, not condition, active)
            elif keyword == "elif" and stack:
                parent, condition, _ = stack[-1]
                new_condition = _eval_preprocessor_if(rest, defines)
                active = parent and not condition and new_condition
                stack[-1] = (parent, condition or new_condition, active)
            elif keyword == "endif" and stack:
                parent, _, _ = stack.pop()
                active = parent
            continue

        effect = re.match(r"RGB_MATRIX_EFFECT\((\w+)\)", line)
        if effect and active:
            effects.append(effect.group(1))

    return tuple(effects)


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


def via_effect_values(via: dict[str, Any]) -> Optional[list[int]]:
    """返回 VIA 灯效下拉框的数值顺序。"""
    dropdown = find_effect_dropdown(via)
    if dropdown is None:
        return None
    options = dropdown.get("options")
    if not isinstance(options, list):
        return None

    values: list[int] = []
    for option in options:
        if not isinstance(option, list) or len(option) < 2 or not isinstance(option[1], int):
            return None
        values.append(option[1])
    return values


def parse_enum_values(source: Optional[str], enum_name: str) -> dict[str, int]:
    """解析 C 枚举里的名称和数值（支持显式赋值和自动递增）。"""
    if not source:
        return {}
    match = re.search(rf"enum\s+{re.escape(enum_name)}\s*\{{(.*?)\}};", source, re.S)
    if not match:
        return {}

    body = re.sub(r"/\*.*?\*/", "", match.group(1), flags=re.S)
    body = re.sub(r"//[^\n]*", "", body)

    values: dict[str, int] = {}
    next_value = 0
    for raw_entry in body.split(","):
        entry = raw_entry.strip()
        if not entry:
            continue
        if "=" in entry:
            name, raw_value = (part.strip() for part in entry.split("=", 1))
            try:
                value = int(raw_value, 0)
            except ValueError:
                continue
        else:
            name = entry
            value = next_value
        values[name] = value
        next_value = value + 1
    return values


def check_eeprom(report: Report, base_commit: str, base_keyboard: dict[str, Any], keyboard: dict[str, Any]) -> None:
    """EEPROM 布局变化必须递增 HS_SETTINGS_VERSION；版本重置视为新一代起点。"""
    old_layout = eeprom_layout(base_commit, base_keyboard)
    new_layout = eeprom_layout(None, keyboard)
    if old_layout is None or new_layout is None:
        report.error("无法读取 EEPROM 布局相关文件，不能完成兼容性检查。")
        return

    old_version = settings_version(base_commit, base_keyboard)
    new_version = settings_version(None, keyboard)
    version_reset = (
        old_version is not None and new_version is not None and new_version < old_version
    )
    if version_reset:
        report.note(
            f"HS_SETTINGS_VERSION 已重置：{old_version} -> {new_version}，"
            "首次刷入会恢复默认设置。"
        )

    changed = {
        key: (old_layout.get(key), new_layout.get(key))
        for key in sorted(set(old_layout) | set(new_layout))
        if old_layout.get(key) != new_layout.get(key)
    }
    if not changed:
        return

    if old_version is None or new_version is None:
        report.error("检测到 EEPROM 布局变化，但无法读取 HS_SETTINGS_VERSION。")
        return
    if version_reset:
        return
    if new_version <= old_version:
        details = ", ".join(changed)
        report.error(
            f"EEPROM 布局变化（{details}）但没有递增 HS_SETTINGS_VERSION："
            f"{old_version} -> {new_version}。"
        )
    else:
        report.note(
            f"EEPROM 布局有变化，HS_SETTINGS_VERSION 已从 {old_version} 递增到 {new_version}。"
        )


def version_major(keyboard: dict[str, Any]) -> int:
    """读取 keyboard.json 的主版本号，用于识别断代升级。"""
    try:
        return int(str(keyboard.get("version", "0")).split(".", 1)[0])
    except ValueError:
        return 0


def check_keycodes(
    report: Report,
    base_commit: str,
    base_keyboard: dict[str, Any],
    keyboard: dict[str, Any],
    via: dict[str, Any],
) -> None:
    """键码顺序必须保持稳定，且 keyboard.json 与 VIA 定义同步。"""
    old_keycodes = [item.get("key") for item in base_keyboard.get("keycodes", []) if item.get("key")]
    new_keycodes = [item.get("key") for item in keyboard.get("keycodes", []) if item.get("key")]
    new_via = [item.get("name") for item in via.get("customKeycodes", []) if item.get("name")]
    breaking_major = version_major(keyboard) > version_major(base_keyboard)
    version_reset = version_major(keyboard) < version_major(base_keyboard)

    # VIA 的 customKeycodes 按位置映射到 QK_KB_n，name 只是界面显示文本
    if len(new_keycodes) != len(new_via):
        report.error(
            f"keyboard.json 有 {len(new_keycodes)} 个自定义键码，"
            f"VIA 有 {len(new_via)} 个 customKeycodes，两边数量不一致。"
        )

    for index, name in enumerate(old_keycodes):
        if name not in new_keycodes:
            report.error(f"自定义键码 {name} 被删除，已保存的键位会失效。")
        elif new_keycodes.index(name) != index:
            report.error(f"自定义键码 {name} 的位置发生变化，已保存的键位会错位；新键码只能追加到末尾。")

    old_ids = parse_enum_values(
        read_text_any(base_commit, (WLS_H, OLD_WLS_H)), "via_custom_defined_id"
    )
    new_ids = parse_enum_values(
        read_text_any(None, (WLS_H, OLD_WLS_H)), "via_custom_defined_id"
    )
    if breaking_major or version_reset:
        report.note("主版本升级或版本重置，允许 VIA 自定义 ID 断代重排；本次只检查新 ID 合法性和连续性。")
    else:
        for name, value in old_ids.items():
            if name not in new_ids:
                report.error(f"VIA 自定义 ID {name} 被删除，外部工具会失效。")
            elif new_ids[name] != value:
                report.error(
                    f"VIA 自定义 ID {name} 从 {value:#04x} 改成 {new_ids[name]:#04x}，"
                    "外部工具会失效；新 ID 只能追加。"
                )

    used: dict[int, str] = {}
    for name, value in new_ids.items():
        if value in used and used[value] != name:
            report.error(f"VIA 自定义 ID 冲突：{name} 与 {used[value]} 都使用 {value:#04x}。")
        used[value] = name

    if new_ids.get("id_host_status") != 0x22:
        report.error("id_host_status 必须固定为 0x22。")
    via_values = sorted(
        value
        for name, value in new_ids.items()
        if name not in {"id_host_rpc", "id_host_status"}
    )
    if via_values and via_values != list(range(via_values[0], via_values[-1] + 1)):
        report.error("VIA 控件 ID 必须从 0x10 开始连续分配。")
    elif via_values and via_values[0] != 0x10:
        report.error(f"VIA 控件 ID 必须从 0x10 开始，当前为 {via_values[0]:#04x}。")



def check_rgb_effects(
    report: Report,
    base_commit: str,
    base_keyboard: dict[str, Any],
    base_via: dict[str, Any],
    keyboard: dict[str, Any],
    via: dict[str, Any],
) -> None:
    """实际生效的灯效序列变化必须递增版本，并同步 VIA 下拉框。"""
    base_config = read_text(base_commit, CONFIG_H)
    base_inc = read_text(base_commit, RGB_USER_INC)
    config = read_text(None, CONFIG_H)
    inc = read_text(None, RGB_USER_INC)
    if base_config is None or base_inc is None or config is None or inc is None:
        report.error("无法读取 RGB 灯效定义，不能完成兼容性检查。")
        return

    old_builtins = enabled_builtin_effects(base_keyboard)
    new_builtins = enabled_builtin_effects(keyboard)
    old_customs = active_custom_effects(base_config, base_inc)
    new_customs = active_custom_effects(config, inc)

    old_signature = (old_builtins, old_customs)
    new_signature = (new_builtins, new_customs)
    if old_signature != new_signature:
        old_version = settings_version(base_commit, base_keyboard)
        new_version = settings_version(None, keyboard)
        version_reset = (
            old_version is not None and new_version is not None and new_version < old_version
        )
        if old_version is None or new_version is None or (new_version <= old_version and not version_reset):
            report.error(
                "RGB 灯效的实际启用顺序发生变化，但没有递增 HS_SETTINGS_VERSION；"
                "已保存的 VIA 效果编号会错位。"
                f" 自定义灯效：{list(old_customs)} -> {list(new_customs)}；"
                f"内置灯效数量：{len(old_builtins)} -> {len(new_builtins)}。"
            )

    values = via_effect_values(via)
    if values is None:
        report.error("VIA 定义中找不到 id_qmk_rgb_matrix_effect 下拉框。")
        return

    expected_count = 1 + len(new_builtins) + len(new_customs)
    if values != list(range(len(values))):
        report.error(f"VIA 灯效编号必须从 0 开始连续递增，当前为 {values}。")
    if len(values) != expected_count:
        report.error(
            f"VIA 灯效数量（{len(values)}）与实际启用数量（{expected_count}）不一致；"
            "请同步更新效果下拉框。"
        )

    old_values = via_effect_values(base_via)
    if old_values is not None and old_values != values and old_signature == new_signature:
        report.error("RGB 灯效没有变化，但 VIA 灯效下拉框的编号顺序被修改了。")

    record_source = read_text(None, RGB_RECORD_C)
    if record_source:
        buff_match = re.search(r"rgbmatrix_buff\[\]\s*=\s*\{(.*?)\};", record_source, re.S)
        if buff_match:
            used_effects = set(re.findall(r"RGB_MATRIX_\w+", buff_match.group(1)))
            disabled = used_effects - set(new_builtins)
            if disabled:
                report.error(
                    "Fn+PrtSc 循环灯效引用了未在 keyboard.json 启用的效果："
                    + ", ".join(sorted(disabled))
                )


def check_debounce(report: Report, keyboard: dict[str, Any]) -> None:
    """debounce 固定为 10ms。"""
    debounce = keyboard.get("debounce")
    if debounce != 10:
        report.error(f"debounce 必须保持 10ms，当前为 {debounce!r}。")


def cargo_package_version(text: Optional[str]) -> Optional[str]:
    """读取 Cargo.toml 中 [package] 段的 version。"""
    if text is None:
        return None
    package = re.search(r"(?ms)^\[package\]\s*$(.*?)(?=^\[|\Z)", text)
    if package is None:
        return None
    version = re.search(r'(?m)^version\s*=\s*"([^"]+)"', package.group(1))
    return version.group(1) if version else None


def check_release_versions(report: Report, keyboard: dict[str, Any]) -> None:
    """固件和上位机的产品版本必须与 keyboard.json 一致。"""
    release = str(keyboard.get("version", ""))
    if not release:
        report.error("keyboard.json 缺少 version。")
        return

    config = read_text(None, CONFIG_H)
    if config is None:
        report.error("无法读取 config.h，不能检查固件版本。")
        return

    defines = parse_defines(config)
    values: list[int] = []
    for part in ("MAJOR", "MINOR", "PATCH"):
        name = f"HS_FIRMWARE_VERSION_{part}"
        raw = defines.get(name)
        if raw is None:
            report.error(f"config.h 缺少 {name}。")
            return
        try:
            values.append(eval_int(raw, defines, (0, 0)))
        except (ValueError, SyntaxError):
            report.error(f"{name} 不是整数：{raw!r}。")
            return

    sources: list[tuple[str, str]] = [
        ("config.h HS_FIRMWARE_VERSION_*", ".".join(str(value) for value in values))
    ]

    host_package = read_json(None, HOST_PACKAGE_JSON)
    if host_package is None:
        report.error("无法读取 host/package.json。")
    else:
        sources.append(("host/package.json", str(host_package.get("version", ""))))

    cargo_version = cargo_package_version(read_text(None, HOST_CARGO_TOML))
    if cargo_version is None:
        report.error("无法读取 host/src-tauri/Cargo.toml 的 [package] version。")
    else:
        sources.append(("host/src-tauri/Cargo.toml", cargo_version))

    tauri = read_json(None, HOST_TAURI_CONF)
    if tauri is None:
        report.error("无法读取 host/src-tauri/tauri.conf.json。")
    elif "version" in tauri:
        sources.append(("host/src-tauri/tauri.conf.json", str(tauri.get("version", ""))))

    for source, version in sources:
        if version != release:
            report.error(
                f"{source} 的产品版本 {version!r} 与 keyboard.json 的 version {release!r} 不一致；"
                "发版时必须同步。"
            )


def check_via_definition(report: Report) -> None:
    """调用 VIA 定义校验脚本。"""
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from validate_via_json import validate_via_files

    for error in validate_via_files():
        report.error(error)


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="检查 RK98PRO 固件的兼容性约束")
    parser.add_argument("--base", default="origin/main", help="用于比较的 git 基线，默认 origin/main")
    parser.add_argument("--head", default="HEAD", help="当前版本，默认 HEAD")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="基线不存在时直接失败，而不是跳过检查",
    )
    args = parser.parse_args(argv)

    base_commit = resolve_commit(args.base)
    if base_commit is None:
        message = f"找不到兼容性检查基线 {args.base!r}（可能没有 fetch 到对应提交）。"
        if args.strict:
            print(f"error: {message}", file=sys.stderr)
            return 2
        print(f"warning: {message}，已跳过兼容性检查。", file=sys.stderr)
        return 0

    head_commit = resolve_commit(args.head)
    if head_commit is None:
        print(f"error: 找不到当前版本 {args.head!r}。", file=sys.stderr)
        return 2

    changed_files = [
        line
        for line in git("diff", "--name-only", base_commit, head_commit, check=False).stdout.splitlines()
        if line
    ]

    base_keyboard = read_json(base_commit, KEYBOARD_JSON)
    base_via = read_json(base_commit, VIA_JSON)
    keyboard = read_json(None, KEYBOARD_JSON)
    via = read_json(None, VIA_JSON)

    if base_keyboard is None or base_via is None:
        message = "基线中缺少 keyboard.json 或 VIA JSON，无法完成兼容性检查。"
        if args.strict:
            print(f"error: {message}", file=sys.stderr)
            return 2
        print(f"warning: {message}，已跳过兼容性检查。", file=sys.stderr)
        return 0
    if keyboard is None or via is None:
        print("error: 当前版本缺少 keyboard.json 或 VIA JSON。", file=sys.stderr)
        return 2

    report = Report()
    check_eeprom(report, base_commit, base_keyboard, keyboard)
    check_keycodes(report, base_commit, base_keyboard, keyboard, via)
    check_rgb_effects(report, base_commit, base_keyboard, base_via, keyboard, via)
    check_debounce(report, keyboard)
    check_release_versions(report, keyboard)
    check_via_definition(report)

    for note in report.notes:
        print(f"note: {note}")

    if report.errors:
        print("兼容性检查失败：", file=sys.stderr)
        for message in report.errors:
            print(f"  - {message}", file=sys.stderr)
        return 1

    print(
        f"兼容性检查通过（基线 {base_commit[:12]}，"
        f"本次比较 {len(changed_files)} 个文件）。"
    )
    if not changed_files:
        print("提示：基线与当前提交相同，没有可比较的变更。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
