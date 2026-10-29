"""Conservative dialogue layout checks for the native textbox renderer.

No automatic reflow: moving waits or choice rows could change game interaction.
Unknown layouts remain explicit warnings rather than invented fit guarantees.
"""
import re
from . import gametext as gt
from .validation import commands

# textbox.c: default origin (20,174), fog bottom 224, line pitch 12.
DEFAULT_WIDTH = 297
DEFAULT_LINES = 4
# TextboxNameGlyphs has nine slots; use the widest supported base glyph.
NAME_WIDTH = 9 * max(gt.ADVANCE)
NUMBER_WIDTH = 5 * gt.WIDE


def screens(raw: bytes):
    pages, lines, width = [], [], 0
    for c in commands(raw):
        op = c[0]
        if op in (0xF7, 0xF8):
            lines.append(width)
            width = 0
            if op == 0xF8:
                pages.append(lines)
                lines = []
        elif op == 0xFA:
            width += NAME_WIDTH
        elif op == 0xFB:
            if c[1] <= 3:
                width += gt.WIDE
            elif c[1] == 8:
                width += NUMBER_WIDTH
        elif op in (0xFC, 0xFD):
            width += gt.WIDE
        elif op < 0xF1:
            width += gt.ADVANCE[op] if op < len(gt.ADVANCE) else (gt._latin_advance(op) or gt.WIDE)
    lines.append(width)
    pages.append(lines)
    return pages


def bounds(box: str):
    if box == 'default':
        return DEFAULT_WIDTH, DEFAULT_LINES
    m = re.fullmatch(r'(?:styled )?(-?\d+),(-?\d+),(\d+),(\d+)', box)
    if not m:
        return None
    x, y, w, h = map(int, m.groups())
    if x < 0 or y < 0 or x >= 320 or y >= 240 or not w or not h:
        return None
    # Styled boxes normally begin six pixels below y. Reserve that inset even
    # when a script might suppress it, and clip to the visible screen.
    return max(0, min(w, 320 - x)), max(0, min(h - 6, 240 - y - 6) // gt.LINE_HEIGHT)


def check(key: str, raw: bytes, source: dict, widest_table: dict):
    errors, warnings = [], []
    pages = screens(raw)
    widest = max(max(p) for p in pages)
    if not key.startswith('dlg/'):
        table = key.split('/')[0]
        if widest > widest_table.get(table, 0):
            warnings.append(f'{widest} px, wider than any English {table} entry '
                            f'({widest_table.get(table, 0)} px); menu layout needs in-game review')
        if any(len(p) > 1 for p in pages):
            warnings.append('multiline resident text: menu layout needs in-game review')
        return errors, warnings
    boxes = source.get('boxes') or []
    if not boxes:
        warnings.append('dialogue layout unknown: fit needs in-game review')
    for box in boxes:
        limit = bounds(box)
        if limit is None:
            warnings.append(f'dialogue layout unknown ({box}): fit needs in-game review')
            continue
        width, count = limit
        for n, page in enumerate(pages, 1):
            if max(page) > width:
                errors.append(f'screen {n}: a line is {max(page)} px; dialogue box {box} fits {width} px')
            if len(page) > count:
                errors.append(f'screen {n}: {len(page)} lines; dialogue box {box} fits {count} lines')
    return errors, warnings
