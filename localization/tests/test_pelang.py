"""pelang.py: templates, the pseudo language and the compiler, on a small synthetic strings.json
(no game data needed).

  python -I -m unittest discover -s localization/tests
"""
import csv
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import pelang  # noqa: E402
from petext import gametext as gt, latin  # noqa: E402

STRINGS = [
    {"key": "item/000", "text": "Ammo +6", "lines_px": [44], "boxes": [], "rooms": [], "unused": False},
    {"key": "item/054", "text": "Chrysler Key1", "lines_px": [80], "boxes": [], "rooms": [], "unused": False},
    {"key": "dlg/M001/002", "text": "Drop an item here?{br}{choice 2}    Yes{br}    No",
     "lines_px": [110, 40, 40], "boxes": ["default"], "rooms": ["M001"], "unused": False,
     "japanese": "手持ちのアイテムをここに置きますか?"},
    {"key": "dlg/M001/003", "text": "Bye.", "lines_px": [24], "boxes": ["default"], "rooms": ["M001"],
     "unused": False, "end": "F9"},
    {"key": "misc/100", "text": "filler", "lines_px": [30], "boxes": [], "rooms": [], "unused": True},
]
BANKS = [{"key": "M001", "crc32": "1a2b3c4d", "size": 120, "rooms": ["M001", "M002"]}]


class PelangTest(unittest.TestCase):
    def setUp(self):
        self.dir = Path(tempfile.mkdtemp())
        self.strings = self.dir / "strings.json"
        self.strings.write_text(json.dumps(STRINGS), encoding="utf-8")
        (self.dir / "banks.json").write_text(json.dumps(BANKS), encoding="utf-8")

    def csv_with(self, translations: dict) -> Path:
        path = self.dir / "t.csv"
        rows = pelang.template_rows(pelang.load_strings(self.strings))
        for r in rows:
            r["translation"] = translations.get(r["key"], "")
        pelang.write_csv(path, rows)
        return path

    def build(self, translations: dict) -> tuple[int, Path]:
        out = self.dir / "t.pelang"
        code = pelang.main(["build", str(self.csv_with(translations)), "--strings", str(self.strings),
                            "--lang", "xx", "--name", "Test", "-o", str(out)])
        return code, out

    def test_fnv1a_matches_the_c_reader(self):
        # Reference values of 32-bit FNV-1a; mods/pe_lang/pe_lang_pack.c uses the same function.
        self.assertEqual(pelang.fnv1a(""), 0x811C9DC5)
        self.assertEqual(pelang.fnv1a("a"), 0xE40C292C)
        self.assertEqual(pelang.fnv1a("item/054"), pelang.fnv1a("item/054"))

    def test_template_lists_used_keys_with_both_languages(self):
        path = self.dir / "tpl.csv"
        pelang.main(["template", str(self.strings), "-o", str(path)])
        with open(path, encoding="utf-8") as f:
            rows = list(csv.DictReader(f))
        self.assertEqual([r["key"] for r in rows], ["dlg/M001/002", "dlg/M001/003", "item/000", "item/054"])
        self.assertTrue(rows[0]["japanese"].startswith("手持ち"))
        self.assertEqual(rows[0]["box"], "default")

    def test_template_merge_keeps_translations(self):
        old = self.csv_with({"item/000": "Balles +6"})
        path = self.dir / "new.csv"
        pelang.main(["template", str(self.strings), "-o", str(path), "--merge", str(old)])
        with open(path, encoding="utf-8") as f:
            rows = {r["key"]: r for r in csv.DictReader(f)}
        self.assertEqual(rows["item/000"]["translation"], "Balles +6")

    def test_pack_round_trip(self):
        code, out = self.build({"item/000": "Balles +6", "dlg/M001/002": "Laisser un objet?{br}{choice 2}    Oui{br}    Non"})
        self.assertEqual(code, 0)
        lang, name, entries = pelang.read_pack(out)
        self.assertEqual((lang, name), ("xx", "Test"))
        self.assertEqual(entries[pelang.fnv1a("item/000")], gt.encode("Balles +6"))
        self.assertEqual(entries[pelang.fnv1a("bank/1a2b3c4d")], b"M001")   # the bank names itself
        self.assertEqual(len(entries), 3)

    def test_dialogue_keeps_its_page_end(self):
        code, out = self.build({"dlg/M001/003": "Salut."})
        self.assertEqual(code, 0)
        _, _, entries = pelang.read_pack(out)
        self.assertEqual(entries[pelang.fnv1a("dlg/M001/003")], gt.encode("Salut.") + b"\xf9")

    def test_build_refuses_missing_glyphs_and_changed_choices(self):
        code, _ = self.build({"item/000": "Боеприпасы"})          # Cyrillic: not in the font yet
        self.assertEqual(code, 1)
        code, _ = self.build({"item/000": "Munitions é"})          # Latin Extended: fine
        self.assertEqual(code, 0)
        code, _ = self.build({"dlg/M001/002": "Laisser un objet?{br}    Oui{br}    Non"})
        self.assertEqual(code, 1)
        path = self.csv_with({})
        with open(path, "a", newline="", encoding="utf-8") as f:
            csv.writer(f).writerow(["dlg/M999/000", "", "", "Bonjour", "", "", "", ""])
        code = pelang.main(["build", str(path), "--strings", str(self.strings), "--lang", "xx",
                            "--name", "Test", "-o", str(self.dir / "u.pelang")])
        self.assertEqual(code, 1)

    def test_build_warns_but_writes_a_long_line(self):
        long = "A very long line that will not fit in the standard dialogue box at all{br}{choice 2}  Y{br}  N"
        raw, errors, warnings = pelang.check_row("dlg/M001/002", long, STRINGS[2], {"item": 80})
        self.assertFalse(errors)
        self.assertTrue(any("dialogue box" in w for w in warnings))

    def test_pseudo_keeps_tags_and_grows(self):
        text = "Drop an item here?{br}{choice 2}    Yes{br}    No"
        p = pelang.pseudo(text)
        self.assertEqual(pelang.STRUCTURE.findall(p), pelang.STRUCTURE.findall(text))
        self.assertGreater(len(p), len(text))
        self.assertIn("Ÿ", pelang.pseudo("yes"))        # case swapped, then accented
        gt.encode(p, extended=True)    # only glyphs the font has, with the Latin Extended set

    def test_latin_codes_and_encoding(self):
        codes = [g.code for g in latin.GLYPHS]
        self.assertEqual(len(codes), len(set(codes)))
        self.assertTrue(all(latin.FIRST_CODE <= c <= 0xF0 for c in codes))
        self.assertEqual(gt.encode("é", extended=True), bytes([latin.CODE_OF["é"]]))
        with self.assertRaises(ValueError):
            gt.encode("é")              # the plain encoder stays the US font's
        self.assertEqual(gt.lines(gt.encode("é", extended=True)), gt.lines(gt.encode("e")))

    def test_generated_c_files_are_current(self):
        # mods/pe_lang/pe_lang_latin.h and tests/pe_lang_font_vectors.h come from petext/latin.py.
        repo = Path(__file__).resolve().parents[2]
        for name, write in (("mods/pe_lang/pe_lang_latin.h", latin.write_c_header),
                            ("tests/pe_lang_font_vectors.h", latin.write_test_vectors)):
            fresh = self.dir / Path(name).name
            write(str(fresh))
            self.assertEqual((repo / name).read_bytes().replace(b"\r\n", b"\n"), fresh.read_bytes(),
                             f"{name} is stale: run `python -I localization/pelang.py latin-header`")


if __name__ == "__main__":
    unittest.main()
