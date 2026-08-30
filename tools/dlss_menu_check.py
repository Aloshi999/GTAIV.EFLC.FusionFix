#!/usr/bin/env python3
"""Verify the locked DLSS Graphics-menu look copy."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
MENUS = [
    ROOT / "data/update/common/data/frontend_menus.xml",
    ROOT / "data/update/TLAD/common/data/frontend_menus.xml",
    ROOT / "data/update/TBoGT/common/data/frontend_menus.xml",
]
GXT = list((ROOT / "text").glob("*FF.txt"))
BANNED = ("UltraPerf", "ULTRAPERF", "DLSS_AUTO", "DLSS_FG", "DLAA", "FrameGen", "FRAMEGEN")


def enum_block(text: str, name: str) -> str:
    m = re.search(rf'<menupc\s+enum="{name}">(.*?)</menupc>', text, re.S)
    return m.group(1) if m else ""


def check_menu(path: Path) -> list[str]:
    errors = []
    text = path.read_text(encoding="utf-8", errors="replace")
    if "MENU_DISPLAY_DLSS" not in text:
        errors.append(f"{path}: missing MENU_DISPLAY_DLSS enum")
    if 'value="PREF_DLSS"' not in text:
        errors.append(f"{path}: missing PREF_DLSS row")

    for m in re.finditer(r'<optionspc[^>]*value="PREF_DLSS"[^/]*/>', text):
        row = m.group(0)
        if 'label="DLSS"' not in row:
            errors.append(f"{path}: PREF_DLSS row label must be exactly DLSS: {row}")
        if "UltraPerf" in row or "Auto" in row or "FG" in row or "DLAA" in row:
            errors.append(f"{path}: PREF_DLSS row has extra values: {row}")

    block = enum_block(text, "MENU_DISPLAY_DLSS")
    if not block:
        errors.append(f"{path}: missing MENU_DISPLAY_DLSS block")
        return errors

    keys = re.findall(r'text="([^"]+)"', block)
    if keys != ["MO_OFF", "DLSS_BAL", "DLSS_QUAL"]:
        errors.append(f"{path}: MENU_DISPLAY_DLSS values must be Off/Balanced/Quality only, got {keys}")
    for banned in BANNED:
        if banned in block:
            errors.append(f"{path}: forbidden {banned} on MENU_DISPLAY_DLSS")

    if not re.search(r'DLSS_BAL.*\n.*DLSS_QUAL', text):
        errors.append(f"{path}: missing Balanced/Quality values")

    rows = re.findall(
        r'<optionspc[^>]*value="(PREF_GRAPHICSAPI|PREF_DLSS)"[^/]*/>',
        text,
    )
    if rows.count("PREF_GRAPHICSAPI") < 2:
        errors.append(f"{path}: expected Graphics API on both graphics screens, got {rows.count('PREF_GRAPHICSAPI')}")
    if rows.count("PREF_DLSS") < 2:
        errors.append(f"{path}: expected DLSS on both graphics screens, got {rows.count('PREF_DLSS')}")

    compact = re.sub(r"\s+", " ", text)
    for m in re.finditer(r'value="PREF_GRAPHICSAPI"[^>]*>', compact):
        window = compact[m.end() : m.end() + 400]
        if 'value="PREF_DLSS"' not in window:
            errors.append(f"{path}: DLSS row is not next to a Graphics API row")
            break
    return errors


def check_gxt(path: Path) -> list[str]:
    errors = []
    text = path.read_text(encoding="utf-8", errors="replace")
    m = re.search(r"\[DLSS\]\n(.*)\n", text)
    if not m or m.group(1).strip() != "DLSS":
        errors.append(f"{path}: [DLSS] must be exactly 'DLSS'")
    for key in ("FF_DLSS_NOVULKAN", "FF_DLSS_NONGX", "FF_DLSS_NOSWAP", "FF_DLSS_NGXERR"):
        if f"[{key}]" not in text:
            errors.append(f"{path}: missing {key}")
    if "[DLSS_BAL]" not in text or "[DLSS_QUAL]" not in text:
        errors.append(f"{path}: missing Balanced/Quality GXT keys")
    return errors


def check_source() -> list[str]:
    errors = []
    settings = (ROOT / "source/settings.ixx").read_text(encoding="utf-8", errors="replace")
    if 'GetHash("DLSS")' in settings and "GetMenuLabel" in settings:
        # The row must use GXT [DLSS], not a dynamic suffix label.
        if "return FusionFixDLSS_GetMenuLabel()" in settings:
            errors.append("settings.ixx: DLSS row still uses GetMenuLabel() (suffixes are banned)")
    dlss = (ROOT / "source/dlss.ixx").read_text(encoding="utf-8", errors="replace")
    if "DLSS: no Vulkan" in dlss or "DLSS: no NGX" in dlss:
        errors.append("dlss.ixx: status suffixes must not be written onto the DLSS row")
    return errors


def main() -> int:
    errors = []
    for path in MENUS:
        if not path.exists():
            errors.append(f"missing {path}")
            continue
        errors.extend(check_menu(path))
    if not GXT:
        errors.append("missing text/*FF.txt")
    for path in GXT:
        errors.extend(check_gxt(path))
    errors.extend(check_source())
    if errors:
        print("\n".join(errors))
        return 1
    print("dlss_menu_check: DLSS row is Off/Balanced/Quality only; extra-info keys present")
    return 0


if __name__ == "__main__":
    sys.exit(main())
