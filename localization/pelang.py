"""Language packs for the pe_lang mod: translator templates, a test language, and the compiler.

  python -I pelang.py template out/text/strings.json -o fr.csv [--merge old-fr.csv]
  python -I pelang.py pseudo   out/text/strings.json -o qps.csv
  python -I pelang.py build    fr.csv --strings out/text/strings.json --lang fr --name "French" -o fr.pelang

`strings.json` comes from extract_text.py, run on the translator's own discs (add --ja for the
Japanese column). A template row is: key, the English and Japanese text (for reference: they come
from the translator's discs and are never put in a pack), the translation, and layout notes.
Text uses extract_text's tags ({br}, {wait}, {name}, {choice 2}, {red} ...; see
petext/gametext.py). An empty translation keeps the game's own text.

`build` checks every translated row and writes a .pelang pack holding only the translations,
already in the game's glyph code. Errors stop the build: an unknown key, a character the font
has no glyph for, malformed commands, changed control sequences, overflow in a known dialogue
box, or menu text past a buffer or the screen edge (data/menu_layouts.json). Line breaks can move;
waits and other commands retain their count, order and operands. Unknown layouts and unconfirmed
menu widths are warnings for in-game review. --allow-overflow is an explicit layout override for
testing (it never bypasses control validation).

Pack format (little-endian), read by mods/pe_lang/pe_lang_pack.c:
  header  64 bytes: "PELG", u32 version (1), u32 entry count, u32 offset of the text,
          char lang[16] and char name[32] (UTF-8, NUL-padded)
  entries count x {u32 key hash, u32 text offset from the text start}, sorted by hash
          (FNV-1a 32 of the key's UTF-8, e.g. "item/054"; the build refuses a collision)
  text    each entry's glyph bytes followed by FF (a dialogue page that ends in F9 keeps it
          before the FF)
Besides the translations, a pack holds one "bank/<crc32>" entry per message bank: the bank's key
("M001", ASCII), so pe_lang can tell which bank a room has loaded (the CRC32 of the bytes its
pages span, as banks.json lists them) and find that bank's dlg/ keys.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from petext import gametext as gt, latin, validation, layout, menu  # noqa: E402
from petext import modtext
from petext import wrap as wrapping  # noqa: E402

MAGIC, VERSION = b"PELG", 1
HEADER = struct.Struct("<4sIII16s32s")
ENTRY = struct.Struct("<II")
COLUMNS = ["key", "english", "japanese", "translation", "box", "widest_px", "rooms", "notes"]


def fnv1a(key: str) -> int:
    h = 0x811C9DC5
    for b in key.encode("utf-8"):
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


def load_strings(path: Path) -> dict[str, dict]:
    rows = json.loads(Path(path).read_text(encoding="utf-8"))
    return {r["key"]: r for r in rows if not r.get("unused")}


def menu_limits(key: str, layouts: dict) -> str:
    """The template's "box" for a menu entry: where it is drawn and how much room there is."""
    out = []
    for c in menu.contexts_of(key, layouts):
        room = [f"{c[k]} px" for k in ("width",) if k in c] + [f"{c['bytes'] - 1} bytes" for _ in [0] if "bytes" in c]
        if c.get("same_length"):
            room.append("same length as the English")
        if "max_glyphs" in c:
            room.append(f"{c['max_glyphs']} letters")
        out.append(f"{c['id']}: " + ", ".join(room))
    return "; ".join(out)


def template_rows(strings: dict[str, dict]) -> list[dict]:
    out, layouts = [], menu.load_layouts()
    for key, r in sorted(strings.items()):
        dialogue = key.startswith("dlg/")
        if dialogue:
            box = "; ".join(r.get("boxes") or [])
        elif r.get("mod_label"):
            box = f"mod label: {r['max_glyphs']} glyphs, {r['width']} px, single line"
        else:
            box = menu_limits(key, layouts)
        out.append({"key": key, "english": r["text"], "japanese": r.get("japanese") or "", "translation": "",
                    "box": box,
                    "widest_px": r["menu_px"] if "menu_px" in r else max(r.get("lines_px") or [0]),
                    "rooms": " ".join((r.get("rooms") or [])[:6]), "notes": ""})
    return out


def read_csv(path: Path) -> list[dict]:
    with open(path, newline="", encoding="utf-8-sig") as f:
        return list(csv.DictReader(f))


def write_csv(path: Path, rows: list[dict]) -> None:
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=COLUMNS, extrasaction="ignore")
        w.writeheader()
        w.writerows(rows)


def cmd_template(a) -> int:
    rows = template_rows(modtext.load(getattr(a, "mod_strings", None), load_strings(a.strings)))
    if a.merge:
        old = {r["key"]: r for r in read_csv(a.merge)}
        kept = 0
        for r in rows:
            prev = old.get(r["key"])
            if prev and prev.get("translation"):
                r["translation"], r["notes"] = prev["translation"], prev.get("notes", "")
                if prev.get("english") and prev["english"] != r["english"]:
                    r["notes"] = ("English changed; check. " + r["notes"]).strip()
                kept += 1
        print(f"kept {kept} translations from {a.merge}")
    write_csv(a.out, rows)
    print(f"wrote {len(rows)} rows to {a.out}")
    return 0


PSEUDO_ACCENTS = str.maketrans("aeiouyncszAEIOUYNCSZ", "áéîõüÿñçšżÀÉÎÕÜŸÑÇŠŻ")


def pseudo(text: str) -> str:
    """A visibly translated, about 30% longer version of `text`: letters swap case and take accents
    (from the Latin Extended set, so the composed glyphs show up), and the text is marked and padded
    with '!' and '+'. Tags stay as they are."""
    parts = re.split(r"(\{[^}]*\})", text)
    plain = sum(len(p) for p in parts if not p.startswith("{"))
    body = "".join(p if p.startswith("{") else p.swapcase().translate(PSEUDO_ACCENTS) for p in parts)
    return f"!{body}{'+' * max(1, plain * 3 // 10)}!" if plain else body


def cmd_pseudo(a) -> int:
    rows = template_rows(modtext.load(getattr(a, "mod_strings", None), load_strings(a.strings)))
    skipped = 0
    for r in rows:
        try:
            validation.check(gt.encode(r["english"], extended=True), r["english"])
        except ValueError:
            # A few extracted source rows contain unsupported control bytes.
            # Keep fallback instead of making an unsafe pack or bypassing checks.
            r["notes"] = "Unsupported source controls: kept as game text."
            skipped += 1
            continue
        r["translation"] = pseudo(r["english"])
    if skipped:
        print(f"kept {skipped} rows with unsupported source controls as game text")
    write_csv(a.out, rows)
    print(f"wrote {len(rows)} pseudo-translated rows to {a.out}")
    return 0


def check_row(key: str, text: str, source: dict, widest_name: dict[str, int], name_px: int = layout.NAME_WIDTH,
              metrics: bytes | None = None, layouts: dict | None = None, wrap: bool = True):
    """(glyph bytes or None, errors, warnings) for one translated row. name_px: how wide {name}
    draws, from the pack's sys/030 (Aya's default name); metrics: the menus' glyph widths
    (menu_glyphs.json), without which menu text is only compared with the English. wrap: a
    dialogue page that overflows its box gets line breaks and screens (petext/wrap.py)."""
    errors, warnings = [], []
    try:
        raw = gt.encode(text, extended=True)
    except (ValueError, TypeError, OverflowError) as e:
        msg = "invalid text/tag: " + str(e)
        if "no glyph" in msg:
            msg += " (the font has A-Z, a-z, digits, basic punctuation, Latin Extended and Russian/Ukrainian/Belarusian Cyrillic)"
        return None, [msg], warnings
    try:
        validation.check(raw, source["text"])
    except ValueError as e:
        return None, [str(e)], warnings
    if source.get("mod_label"):
        if any(c >= 0xF1 for c in raw):
            return None, ["mod labels cannot contain line breaks, waits or commands"], []
        return raw, modtext.check(raw, source, metrics), []
    note = None
    limit = layout.box_limit(source) if wrap and key.startswith("dlg/") else None
    if limit and any(max(s) > limit[0] or len(s) > limit[1] for s in layout.screens(raw, name_px)):
        english = bytes.fromhex(source["raw"]) if source.get("raw") else gt.encode(source["text"])
        wrapped, added = wrapping.wrap(raw, limit[0], limit[1], english, name_px)
        if wrapped != raw:
            raw = wrapped
            note = "wrapped to fit the box" + (f", {added} screen(s) added" if added else "") + "; check the breaks"
    layout_errors, warnings = layout.check(key, raw, source, widest_name, name_px, metrics, layouts)
    errors.extend(layout_errors)
    if note:
        warnings.insert(0, note)
    return raw, errors, warnings


def cmd_build(a) -> int:
    strings = modtext.load(getattr(a, "mod_strings", None), load_strings(a.strings))
    metrics, layouts = menu.load_metrics(a.strings), menu.load_layouts()
    if metrics is None:
        print(f"note: no menu_glyphs.json beside {a.strings} (rerun extract_text.py): "
              "menu text is compared with the English only", file=sys.stderr)
    widest_name: dict[str, int] = {}
    for key, r in strings.items():
        if not key.startswith("dlg/"):
            t = key.split("/")[0]
            px = r["menu_px"] if metrics is not None and "menu_px" in r else max(r.get("lines_px") or [0])
            widest_name[t] = max(widest_name.get(t, 0), px)
    entries, errors, warnings = {}, [], []
    seen = set()
    rows = read_csv(a.csv)
    name_px = layout.NAME_WIDTH
    for r in rows:
        if r.get("key") == "sys/030" and r.get("translation"):
            try:
                name_px = layout.name_width(gt.encode(r["translation"], extended=True))
            except (ValueError, TypeError, OverflowError):
                pass                                  # reported with the row below
    for r in rows:
        key, text = r.get("key", ""), r.get("translation", "")
        if not text:
            continue
        if key in seen:
            errors.append(f"{key}: duplicate translated key")
            continue
        seen.add(key)
        if key not in strings:
            errors.append(f"{key}: no such key in {a.strings}")
            continue
        raw, errs, warns = check_row(key, text, strings[key], widest_name, name_px, metrics, layouts,
                                     wrap=not a.no_wrap)
        if a.allow_overflow:
            # check_row returns raw only after control validation succeeds.
            if raw is not None:
                warns += errs
                errs = []
        errors += [f"{key}: {e}" for e in errs]
        warnings += [f"{key}: {w}" for w in warns]
        if raw is not None and not errs:
            entries[key] = raw
    if metrics is not None:
        # Text the game joins (an item's name and a suffix in one buffer): the pack's longest of each.
        resident = {k: bytes.fromhex(r["raw"]) for k, r in strings.items() if not k.startswith("dlg/") and r.get("raw")}
        resident.update({k: v for k, v in entries.items() if not k.startswith("dlg/")})
        joined = menu.check_combined(resident, metrics, layouts)
        (warnings if a.allow_overflow else errors).extend(f"joined text: {e}" for e in joined)
    # Dialogue pages keep the page end the game had (F9 instead of FF closes some boxes), and the
    # pack says which bank is which: "bank/<crc32>" holds that bank's key ("M001") in ASCII.
    for key, raw in list(entries.items()):
        end = strings[key].get("end", "FF")
        if key.startswith("dlg/") and end != "FF":
            entries[key] = raw + bytes([int(end, 16)])
    banks = json.loads(Path(a.banks or a.strings.with_name("banks.json")).read_text(encoding="utf-8"))
    for b in banks:
        entries[f"bank/{b['crc32']}"] = b["key"].encode("ascii")
    hashes: dict[int, str] = {}
    for key in entries:
        h = fnv1a(key)
        if h in hashes:
            errors.append(f"{key}: key hash collides with {hashes[h]}")
        hashes[h] = key
    for w in warnings[: a.max_messages]:
        print(f"warning: {w}")
    for e in errors[: a.max_messages]:
        print(f"error: {e}")
    translated = sum(1 for k in entries if not k.startswith("bank/"))
    print(f"{translated} of {len(strings)} keys translated, {len(banks)} banks; "
          f"{len(warnings)} warning(s), {len(errors)} error(s)")
    if errors:
        return 1
    write_pack(a.out, a.lang, a.name, entries)
    print(f"wrote {a.out}")
    return 0


def write_pack(path: Path, lang: str, name: str, entries: dict[str, bytes]) -> None:
    lang_b, name_b = lang.encode("utf-8"), name.encode("utf-8")
    if not lang_b or not name_b or b"\0" in lang_b + name_b or len(lang_b) > 15 or len(name_b) > 31:
        raise SystemExit("--lang and --name must be nonempty, without NUL; at most 15 and 31 bytes")
    order = sorted(entries, key=fnv1a)
    text, table = bytearray(), bytearray()
    for key in order:
        table += ENTRY.pack(fnv1a(key), len(text))
        text += entries[key] + b"\xff"
    head = HEADER.pack(MAGIC, VERSION, len(order), HEADER.size + len(table), lang_b, name_b)
    Path(path).write_bytes(head + table + text)


def read_pack(path: Path) -> tuple[str, str, dict[int, bytes]]:
    """(lang, name, hash -> glyph bytes) of a pack: the reader the tests check the writer with."""
    data = Path(path).read_bytes()
    magic, version, count, text_at, lang, name = HEADER.unpack_from(data, 0)
    if magic != MAGIC or version != VERSION:
        raise ValueError("not a version 1 .pelang pack")
    out = {}
    for i in range(count):
        h, off = ENTRY.unpack_from(data, HEADER.size + i * ENTRY.size)
        start = text_at + off
        out[h] = data[start:data.index(b"\xff", start)]
    return lang.rstrip(b"\0").decode(), name.rstrip(b"\0").decode(), out


def main(argv=None) -> int:
    # Messages quote the translator's text; a console that can't show a character gets an escape
    # (Б) instead of stopping the build.
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(errors="backslashreplace")
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    t = sub.add_parser("template", help="a translator CSV from strings.json")
    t.add_argument("strings", type=Path)
    t.add_argument("-o", "--out", type=Path, required=True)
    t.add_argument("--merge", type=Path, help="carry the translations of an older CSV over, by key")
    p = sub.add_parser("pseudo", help="a test language: every entry, marked and ~30% longer")
    p.add_argument("strings", type=Path)
    p.add_argument("-o", "--out", type=Path, required=True)
    b = sub.add_parser("build", help="check a translated CSV and write a .pelang pack")
    b.add_argument("csv", type=Path)
    b.add_argument("--strings", type=Path, required=True, help="strings.json from extract_text.py")
    b.add_argument("--banks", type=Path, help="banks.json from extract_text.py (default: beside strings.json)")
    b.add_argument("--lang", required=True, help="language code, e.g. fr, pt-BR, en-x-jp, qps")
    b.add_argument("--name", required=True, help="the name the Language row shows, e.g. French")
    b.add_argument("-o", "--out", type=Path, required=True)
    b.add_argument("--max-messages", type=int, default=50)
    b.add_argument("--allow-overflow", action="store_true",
                   help="emit known layout overflows as warnings for deliberate in-game testing; controls stay strict")
    b.add_argument("--no-wrap", action="store_true",
                   help="don't rewrap dialogue pages that overflow their box (by default they get line breaks at "
                        "spaces and, if still too tall, more screens)")
    h = sub.add_parser("latin-header", help="write the Latin Extended table the pe_lang mod composes "
                                            "from, and the test vectors its composer is checked with")
    h.add_argument("out", type=Path, nargs="?", default=ROOT.parent / "mods" / "pe_lang" / "pe_lang_latin.h")
    h.add_argument("--vectors", type=Path, default=ROOT.parent / "tests" / "pe_lang_font_vectors.h")
    for command in (t, p, b):
        command.add_argument("--mod-strings", type=Path, help="optional mod-owned key and layout manifest")
    c = sub.add_parser("cyrillic-header", help="regenerate authored Cyrillic glyph data")
    c.add_argument("out", type=Path, nargs="?", default=ROOT.parent / "mods/pe_lang/pe_lang_cyrillic_data.h")
    a = ap.parse_args(argv)
    return {"template": cmd_template, "pseudo": cmd_pseudo, "build": cmd_build,
            "latin-header": cmd_latin_header, "cyrillic-header": cmd_cyrillic_header}[a.cmd](a)


def cmd_cyrillic_header(a) -> int:
    from petext import cyrillic
    cyrillic.write_c_header(a.out)
    print(f"wrote {a.out}: {len(cyrillic.GLYPHS)} authored Cyrillic glyphs")
    return 0


def cmd_latin_header(a) -> int:
    latin.write_c_header(str(a.out))
    latin.write_test_vectors(str(a.vectors))
    print(f"wrote {a.out}: {len(latin.GLYPHS)} glyphs, codes 0x{latin.FIRST_CODE:02X}-0x{latin.GLYPHS[-1].code:02X}; "
          f"test vectors {a.vectors}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
