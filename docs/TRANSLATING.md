# Переводы / Translating

## Как устроено

- `locales/languages.json` — порядок языков в интерфейсе, язык по умолчанию (`default`) и резервный (`fallback`, из него берутся строки, которых нет в переводе). Сейчас оба — `ru`.
- `locales/<код>.json` — строки одного языка. Ключи вложенные: `"panel": {"title": "…"}` → `panel.title`.
- `{0}`, `{1}` — подстановки; набор подстановок в переводе должен совпадать с русским.
- Блок `meta`: самоназвание языка (`name`), английское название, правила записи чисел:
  - `group_separator` — разделитель групп разрядов (` ` — неразрывный пробел, ` ` — узкий неразрывный);
  - `decimal_separator` — десятичный разделитель;
  - `grouping` — `"3"` (1,000,000) или `"3;2"` (индийская: 10,00,000);
  - `group_min_digits` — с какой длины числа начинать группировку (`5` для ru/es: «6000», но «12 000»).

## Добавление языка

1. Скопируйте `locales/ru.json` в `locales/<код>.json` (код ISO 639-1: `pt`, `ja`, `ar`, …) и переведите все строки.
2. Добавьте код в массив `order` файла `locales/languages.json`.
3. Если язык использует новую письменность, добавьте шрифт Noto для неё в `tools/subset_fonts.py` и в цепочку `TextShaper::loadDefaultFonts`.
4. Пересоберите шрифты под новые строки: `python3 tools/subset_fonts.py --src <папка с исходными шрифтами Noto>`.
5. Запустите тесты: `ctest --test-dir build --output-on-failure`. Они проверят, что в переводе есть все ключи, совпадают подстановки и для каждого символа есть глиф во встроенных шрифтах.

Письменности справа налево (арабская, иврит) потребуют дополнительно алгоритма BiDi (например, FriBiDi): HarfBuzz формирует глифы, но не меняет порядок фрагментов в строке.

## English summary

Add `locales/<code>.json` (copy `ru.json`), list the code in `locales/languages.json`, regenerate the bundled font subsets with `tools/subset_fonts.py`, and run `ctest` — the tests verify key completeness, matching placeholders and glyph coverage for every character.
