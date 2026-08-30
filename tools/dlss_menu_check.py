#!/usr/bin/env python3
"""Verify the in-game Graphics menu exposes DLSS next to Graphics API."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
MENUS = [
    ROOT / "data/update/common/data/frontend_menus.xml",
    ROOT / "data/update/TLAD/common/data/frontend_menus.xml",
    ROOT / "data/update/TBoGT/common/data/frontend_menus.xml",
]


def check(path: Path) -> list[str]:
    errors = []
    text = path.read_text(encoding="utf-8", errors="replace")
    if "MENU_DISPLAY_DLSS" not in text:
        errors.append(f"{path}: missing MENU_DISPLAY_DLSS enum")
    if 'value="PREF_DLSS"' not in text:
        errors.append(f"{path}: missing PREF_DLSS row")
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

    # Each Graphics API row must be followed by a DLSS row in the file order.
    compact = re.sub(r"\s+", " ", text)
    if compact.count('value="PREF_GRAPHICSAPI"') == 0:
        errors.append(f"{path}: no Graphics API row")
    for m in re.finditer(r'value="PREF_GRAPHICSAPI"[^>]*>', compact):
        window = compact[m.end() : m.end() + 400]
        if 'value="PREF_DLSS"' not in window:
            errors.append(f"{path}: DLSS row is not next to a Graphics API row")
            break
    return errors


def main() -> int:
    errors = []
    for path in MENUS:
        if not path.exists():
            errors.append(f"missing {path}")
            continue
        errors.extend(check(path))
    if errors:
        print("\n".join(errors))
        return 1
    print("dlss_menu_check: all frontend_menus.xml files have DLSS next to Graphics API")
    return 0


if __name__ == "__main__":
    sys.exit(main())
