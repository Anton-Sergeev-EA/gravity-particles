#!/usr/bin/env python3
"""Пересборка встроенных шрифтов (подмножеств Noto) под строки локализаций.

Шрифты Noto полного размера весят десятки мегабайт (только китайский — ~8 МБ
на начертание), поэтому в репозиторий кладутся подмножества:

* Noto Sans          — латиница, латиница-1, расширенная латиница A, кириллица,
                       общая пунктуация, стрелки, символы валют;
* Noto Sans Devanagari — блок деванагари целиком (небольшой) + знаки ZWJ/ZWNJ;
* Noto Sans SC       — только иероглифы, реально встречающиеся в locales/*.json,
                       плюс CJK-пунктуация и полноширинные формы.

Все OpenType-таблицы раскладки (GSUB/GPOS) сохраняются — без них HarfBuzz
не сможет корректно собрать лигатуры и огласовки деванагари.

Запуск после добавления/изменения переводов:

    pip install fonttools
    python3 tools/subset_fonts.py --src /путь/к/исходным/шрифтам

Исходные шрифты: https://github.com/notofonts/notofonts.github.io (Noto Sans,
Noto Sans Devanagari) и https://github.com/notofonts/noto-cjk (Noto Sans SC,
SubsetOTF/SC). Лицензия — SIL Open Font License 1.1 (assets/fonts/OFL.txt).
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from fontTools import subset

ROOT = Path(__file__).resolve().parent.parent
LOCALES = ROOT / "locales"
OUT = ROOT / "assets" / "fonts"

# (исходный файл, итоговый файл, роль)
FONTS: list[tuple[str, str, str]] = [
    ("NotoSans-Regular.ttf", "NotoSans-Regular.ttf", "latin"),
    ("NotoSans-SemiBold.ttf", "NotoSans-SemiBold.ttf", "latin"),
    ("NotoSansDevanagari-Regular.ttf", "NotoSansDevanagari-Regular.ttf", "deva"),
    ("NotoSansDevanagari-SemiBold.ttf", "NotoSansDevanagari-SemiBold.ttf", "deva"),
    ("NotoSansSC-Regular.otf", "NotoSansSC-Regular.otf", "cjk"),
    ("NotoSansSC-Medium.otf", "NotoSansSC-Medium.otf", "cjk"),
]

LATIN_RANGES: list[tuple[int, int]] = [
    (0x0020, 0x007E),  # Basic Latin
    (0x00A0, 0x00FF),  # Latin-1 Supplement
    (0x0100, 0x017F),  # Latin Extended-A
    (0x0400, 0x04FF),  # Cyrillic
    (0x2000, 0x206F),  # General Punctuation (тире, NBSP-варианты, кавычки)
    (0x20A0, 0x20BF),  # Currency Symbols
    (0x2190, 0x21FF),  # Arrows
    (0x2212, 0x2212),  # Minus sign
]
DEVA_RANGES: list[tuple[int, int]] = [
    (0x0900, 0x097F),  # Devanagari
    (0xA8E0, 0xA8FF),  # Devanagari Extended
    (0x200C, 0x200D),  # ZWNJ / ZWJ
    (0x0964, 0x0965),  # danda
    (0x25CC, 0x25CC),  # пунктирный круг для одиночных огласовок
]
CJK_RANGES: list[tuple[int, int]] = [
    (0x3000, 0x303F),  # CJK Symbols and Punctuation
    (0xFF00, 0xFFEF),  # Halfwidth and Fullwidth Forms
]


def collect_strings(node: object) -> list[str]:
    if isinstance(node, str):
        return [node]
    if isinstance(node, dict):
        return [s for v in node.values() for s in collect_strings(v)]
    if isinstance(node, list):
        return [s for v in node for s in collect_strings(v)]
    return []


def locale_codepoints() -> set[int]:
    cps: set[int] = set()
    for path in sorted(LOCALES.glob("*.json")):
        data = json.loads(path.read_text(encoding="utf-8"))
        for s in collect_strings(data):
            cps.update(ord(ch) for ch in s)
    return cps


def expand(ranges: list[tuple[int, int]]) -> set[int]:
    return {cp for lo, hi in ranges for cp in range(lo, hi + 1)}


def is_cjk(cp: int) -> bool:
    return (
        0x4E00 <= cp <= 0x9FFF
        or 0x3400 <= cp <= 0x4DBF
        or 0x3000 <= cp <= 0x303F
        or 0xFF00 <= cp <= 0xFFEF
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--src", type=Path, required=True, help="папка с исходными шрифтами Noto")
    args = parser.parse_args()

    used = locale_codepoints()
    roles = {
        "latin": expand(LATIN_RANGES) | {cp for cp in used if cp < 0x0900 or 0x2000 <= cp < 0x2E00},
        "deva": expand(DEVA_RANGES) | {cp for cp in used if 0x0900 <= cp <= 0x097F},
        "cjk": expand(CJK_RANGES) | {cp for cp in used if is_cjk(cp)} | expand([(0x0020, 0x007E)]),
    }

    OUT.mkdir(parents=True, exist_ok=True)
    for src_name, dst_name, role in FONTS:
        src = args.src / src_name
        if not src.exists():
            print(f"нет исходного шрифта: {src}", file=sys.stderr)
            return 1
        options = subset.Options()
        options.layout_features = ["*"]
        options.name_IDs = ["*"]
        options.name_languages = ["*"]
        options.notdef_outline = True
        options.glyph_names = False
        options.hinting = True
        options.desubroutinize = False
        font = subset.load_font(str(src), options)
        subsetter = subset.Subsetter(options)
        subsetter.populate(unicodes=sorted(roles[role]))
        subsetter.subset(font)
        dst = OUT / dst_name
        subset.save_font(font, str(dst), options)
        print(f"{dst.relative_to(ROOT)}: {dst.stat().st_size / 1024:.0f} КБ")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
