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
# {name} draws TextboxNameGlyphs: sys/030 ("Aya") at a new game (Save_InitMetadataState), or the
# name the player gave her, at most 8 glyphs (Menu_CopyPromptCodes). The English is laid out for
# "Aya" and overflows with long names, so pages are measured with the pack's own sys/030.
NAME_WIDTH = sum(gt.ADVANCE[g] for g in gt.encode("Aya"))
NUMBER_WIDTH = 5 * gt.WIDE


def screens(raw: bytes, name_width: int = NAME_WIDTH):
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
            width += name_width
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


def bounds(box: str, screen: bool = False):
    """(width px, lines) a box holds. A styled box's text starts at its x with no inset and isn't
    clipped (textbox.c; the rectangle only sizes style 1's gradient), so its rectangle is what
    looks right and the screen edge (screen=True) is where text is lost."""
    if box == 'default':
        return DEFAULT_WIDTH, DEFAULT_LINES
    m = re.fullmatch(r'(?:styled )?(-?\d+),(-?\d+),(\d+),(\d+)', box)
    if not m:
        return None
    x, y, w, h = map(int, m.groups())
    if x < 0 or y < 0 or x >= 320 or y >= 240 or not w or not h:
        return None
    if screen:
        w, h = 320, 240
    # Styled boxes normally begin six pixels below y. Reserve that inset even
    # when a script might suppress it, and clip to the visible screen.
    return max(0, min(w, 320 - x)), max(0, min(h - 6, 240 - y - 6) // gt.LINE_HEIGHT)


def fits(pages, box: str):
    """True or False for a box with known bounds, None when its rectangle is computed."""
    limit = bounds(box)
    if limit is None:
        return None
    width, count = limit
    return all(max(p) <= width and len(p) <= count for p in pages)


def unproven(pages, source: dict):
    """A page no script ties to a box. Only opcodes 13 and 209 show a bank's pages, so it can
    only appear in a box its bank's rooms open (boxes_possible). Never an error: the page may be
    unused, or opened by a room whose scripts didn't decode (boxes_complete false)."""
    possible = source.get('boxes_possible') or []
    if not possible:
        return ['dialogue layout unknown: fit needs in-game review']
    results = {box: fits(pages, box) for box in possible}
    unsure = '' if source.get('boxes_complete') else '; some of its rooms did not decode, so the set may be larger'
    if all(results.values()):
        return [] if not unsure else [f'no script ties this page to a box; fits every box its bank opens{unsure}']
    too_big = [b for b, ok in results.items() if ok is False]
    unknown = [b for b, ok in results.items() if ok is None]
    if len(too_big) == len(results):
        return [f'no script ties this page to a box, and it fits none its bank opens '
                f'({", ".join(too_big)}){unsure}']
    note = f'too big for {", ".join(too_big)}' if too_big else ''
    if unknown:
        note += ('; ' if note else '') + f'{len(unknown)} box(es) with a computed rectangle'
    return [f'no script ties this page to a box: {note}; fit needs in-game review{unsure}']


def name_width(raw: bytes) -> int:
    """How wide {name} draws when the default name is these glyph bytes (a sys/030 entry)."""
    return sum(gt.ADVANCE[g] if g < len(gt.ADVANCE) else (gt._latin_advance(g) or gt.WIDE) for g in raw[:8])


def check(key: str, raw: bytes, source: dict, widest_table: dict, name_px: int = NAME_WIDTH):
    errors, warnings = [], []
    pages = screens(raw, name_px)
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
        return errors, warnings + unproven(pages, source)
    for box in boxes:
        limit = bounds(box)
        if limit is None:
            warnings.append(f'dialogue layout unknown ({box}): fit needs in-game review')
            continue
        width, count = limit
        edge, rows = bounds(box, screen=True)
        for n, page in enumerate(pages, 1):
            if max(page) > edge:
                errors.append(f'screen {n}: a line is {max(page)} px; dialogue box {box} fits {edge} px'
                              + ('' if box == 'default' else ' before the screen edge'))
            elif max(page) > width:
                warnings.append(f'screen {n}: a line is {max(page)} px, past the {width} px of box {box} '
                                '(the English does this in places); check it in game')
            if len(page) > rows:
                errors.append(f'screen {n}: {len(page)} lines; dialogue box {box} fits {rows} lines'
                              + ('' if box == 'default' else ' before the screen edge'))
            elif len(page) > count:
                warnings.append(f'screen {n}: {len(page)} lines, past the {count} lines of box {box}; '
                                'check it in game')
    return errors, warnings
