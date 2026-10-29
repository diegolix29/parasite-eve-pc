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
has no glyph for, or a {choice}/{num}/{name} sequence that differs from the English (scripts
read a choice's result by position). Warnings don't: a dialogue line wider than its box, a name
wider than the widest English one, a changed number of {wait} screens.

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
from petext import gametext as gt  # noqa: E402

MAGIC, VERSION = b"PELG", 1
HEADER = struct.Struct("<4sIII16s32s")
ENTRY = struct.Struct("<II")
DEFAULT_BOX_PX = 297            # the standard dialogue box's line width
COLUMNS = ["key", "english", "japanese", "translation", "box", "widest_px", "rooms", "notes"]
STRUCTURE = re.compile(r"\{(choice \d+|num|name)\}")
WAIT = "{wait}"


def fnv1a(key: str) -> int:
    h = 0x811C9DC5
    for b in key.encode("utf-8"):
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


def load_strings(path: Path) -> dict[str, dict]:
    rows = json.loads(Path(path).read_text(encoding="utf-8"))
    return {r["key"]: r for r in rows if not r.get("unused")}


def template_rows(strings: dict[str, dict]) -> list[dict]:
    out = []
    for key, r in sorted(strings.items()):
        out.append({"key": key, "english": r["text"], "japanese": r.get("japanese") or "", "translation": "",
                    "box": "; ".join(r.get("boxes") or []), "widest_px": max(r.get("lines_px") or [0]),
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
    rows = template_rows(load_strings(a.strings))
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


def pseudo(text: str) -> str:
    """A visibly translated, about 30% longer version of `text`, using only glyphs the font has:
    letters swap case, and the text is marked and padded with '!' and '+'. Tags stay as they are."""
    parts = re.split(r"(\{[^}]*\})", text)
    plain = sum(len(p) for p in parts if not p.startswith("{"))
    body = "".join(p if p.startswith("{") else p.swapcase() for p in parts)
    return f"!{body}{'+' * max(1, plain * 3 // 10)}!" if plain else body


def cmd_pseudo(a) -> int:
    rows = template_rows(load_strings(a.strings))
    for r in rows:
        r["translation"] = pseudo(r["english"])
    write_csv(a.out, rows)
    print(f"wrote {len(rows)} pseudo-translated rows to {a.out}")
    return 0


def check_row(key: str, text: str, source: dict, widest_name: dict[str, int]):
    """(glyph bytes or None, errors, warnings) for one translated row."""
    errors, warnings = [], []
    try:
        raw = gt.encode(text)
    except ValueError as e:
        msg = str(e)
        if msg.startswith("no glyph"):
            msg += " (the font has only A-Z, a-z, digits and basic punctuation until the accented-letter font lands)"
        return None, [msg], warnings
    english = source["text"]
    if STRUCTURE.findall(english) != STRUCTURE.findall(text):
        errors.append(f"{{choice}}/{{num}}/{{name}} must match the English in count and order: "
                      f"{STRUCTURE.findall(english)} vs {STRUCTURE.findall(text)}")
    if english.count(WAIT) != text.count(WAIT):
        warnings.append(f"{english.count(WAIT) + 1} screen(s) in English, {text.count(WAIT) + 1} here")
    widest = max(gt.lines(raw))
    table = key.split("/")[0]
    if key.startswith("dlg/"):
        if "default" in (source.get("boxes") or []) and widest > DEFAULT_BOX_PX:
            warnings.append(f"a line is {widest} px; the dialogue box fits {DEFAULT_BOX_PX}")
    elif widest > widest_name.get(table, 0):
        warnings.append(f"{widest} px, wider than any English {table} entry ({widest_name.get(table, 0)} px)")
    return raw, errors, warnings


def cmd_build(a) -> int:
    strings = load_strings(a.strings)
    widest_name: dict[str, int] = {}
    for key, r in strings.items():
        if not key.startswith("dlg/"):
            t = key.split("/")[0]
            widest_name[t] = max(widest_name.get(t, 0), max(r.get("lines_px") or [0]))
    entries, errors, warnings = {}, [], []
    rows = read_csv(a.csv)
    for r in rows:
        key, text = r.get("key", ""), r.get("translation", "")
        if not text:
            continue
        if key not in strings:
            errors.append(f"{key}: no such key in {a.strings}")
            continue
        raw, errs, warns = check_row(key, text, strings[key], widest_name)
        errors += [f"{key}: {e}" for e in errs]
        warnings += [f"{key}: {w}" for w in warns]
        if raw is not None and not errs:
            entries[key] = raw
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
    if len(lang_b) > 15 or len(name_b) > 31:
        raise SystemExit("--lang is at most 15 bytes and --name at most 31")
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
    a = ap.parse_args(argv)
    return {"template": cmd_template, "pseudo": cmd_pseudo, "build": cmd_build}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
