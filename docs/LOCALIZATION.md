# Localization

The `pe_lang` mod shows the game's text from a **language pack**: translations by key, already in
the game's glyph code, loaded at start and swapped in when the game looks the text up. Nothing on
the disc changes and nothing is saved, so packs can be switched at any time and saves stay the
same in every language. Anything a pack doesn't translate keeps the game's own text.

The full plan, with the font work, the languages and the Japanese-to-English re-localization, is
the [Parasite Eve Localization Mod Plan](https://claude.ai/artifact/2NpmecRhhgMB1Zx7vxQojM).

## Status

| Piece | State |
| --- | --- |
| Text extraction, English and Japanese (`localization`) | Done |
| Translator template, test language, pack compiler (`pelang.py`) | Done |
| `pe_lang` mod: F1 > Language, item names and descriptions | Done |
| System messages, misc table, room dialogue | Done: `PE_HOOK_TEXT_LOOKUP` from every table, `PE_HOOK_TEXT_PAGE` |
| Text drawn into images, text compiled into the port, other mods' messages | Not keyed yet |
| Accented letters, Cyrillic | Needs the font upload (phase 3) |
| Chinese, Japanese, Korean | Needs the per-area glyph pages (phase 5) |

Until the accented-letter font lands, a pack can only use the glyphs the US font has: A-Z, a-z,
0-9, space and `+ - % & ! ? " ' . : , /`. The compiler says so when a character is missing.

## Making a pack

Every step reads the translator's own discs; no game text goes into git or into a pack.

```
cd localization
python -I extract_text.py US1.cue US2.cue [--ja JP1.cue JP2.cue]
python -I pelang.py template out/text/strings.json -o fr.csv
python -I pelang.py build fr.csv --strings out/text/strings.json --lang fr --name French -o fr.pelang
```

1. **Extract.** `extract_text.py` writes `out/text/` (git-ignored): `strings.json` and
   `strings.csv` with every entry's key, English, Japanese (with `--ja`), line widths in pixels and
   the boxes it shows in, `banks.json` (each message bank's CRC32), plus the item reports. Keys: `sys/NNN`, `item/NNN`, `desc/NNN`, `misc/NNN`
   for the resident text tables, `dlg/MNNN/PPP` for room dialogue (the message bank first used by
   room MNNN, page PPP). The 148 table slots that hold filler the game never shows are left out.
2. **Template.** `pelang.py template` writes the translator CSV: key, English, Japanese (for
   reference), translation, box, widest line, rooms, notes. `--merge old.csv` carries the
   translations of an earlier template over, by key, and notes rows whose English changed.
3. **Translate.** In any spreadsheet. Keep the tags: `{br}` line break, `{wait}` next screen,
   `{name}` Aya's name, `{choice N}` a choice of N options, `{red}`/`{/red}` highlight, `{num}` a
   number (see `petext/gametext.py`). An empty translation keeps the game's text.
4. **Build.** `pelang.py build` checks every row and writes the pack.
   - Errors stop it: an unknown key, a character the font lacks, or a `{choice}`/`{num}`/`{name}`
     sequence that differs from the English. Scripts read a choice's result by position.
   - Warnings don't: a dialogue line wider than its box (297 px), a name or description wider than
     the widest English one, a different number of `{wait}` screens.
5. **Play.** Put the `.pelang` in `lang/` beside the game, or in `lang/` in the user folder (beside
   `pe_mods.ini`). Build the game with `pe_lang` in `PE_MOD_LIST`, turn All mods on, and pick the
   language under F1 > Language.

`pelang.py pseudo` makes a test language from every entry: letters swap case, and each entry is
marked with `!` and padded about 30%. Build it like any pack (`--lang qps`) to see what text a pack
reaches and where longer text overflows.

## How the text is swapped

The mod never changes the game's data. At the moment the game takes a piece of text, a hook hands
the mod the pointer, and the mod gives back a pointer into the pack: the loaded text, the scripts
and the saves stay as they are, and turning a pack off (or leaving an entry out) lets the original
through.

- **Text tables.** `Text_LookupTable4` / `8` / `C` / `10` and the map location names send
  `PE_HOOK_TEXT_LOOKUP` with the table (4, 8, 12, 16) and index; keys `sys/`, `item/`, `desc/`,
  `misc/`.
- **Dialogue.** When a box finds its page in the room's message bank (`textbox.c`), it sends
  `PE_HOOK_TEXT_PAGE` with the bank and the page. Banks are shared by an area's rooms and keyed by
  the first room that uses one, so the pack names each bank by the CRC32 of its English bytes
  (`banks.json` from the extractor), and the mod measures the bank a box reads from once per room.
  A bank the pack doesn't know (another disc version, a modded room) keeps the game's text.
- **Choices** are read back by position, so a translation keeps the English's choices in the same
  order; the compiler refuses anything else. A page that ends in F9 keeps that end.

## Pack format

Little-endian, version 1 (`pelang.py` writes it, `mods/pe_lang/pe_lang_pack.c` reads it):

| Part | Layout |
| --- | --- |
| Header, 64 bytes | `"PELG"`, u32 version, u32 entry count, u32 offset of the text, char lang[16], char name[32] (UTF-8, NUL-padded) |
| Entries | count × {u32 key hash, u32 text offset from the text start}, sorted by hash |
| Text | each entry's glyph bytes followed by `FF` (a page that ends in F9 keeps it before the `FF`) |

The key hash is 32-bit FNV-1a of the key's UTF-8 (`item/054`). The compiler refuses a collision,
and the reader checks every offset and terminator at load, refusing a damaged pack whole. Besides
the translations, a pack holds one `bank/<crc32>` entry per message bank, whose value is the bank's
key in ASCII (`M001`).

## The Japanese source

`extract_text.py --ja` reads the Japanese release (SLPS-01230 / 01231) and puts each Japanese line
on the same key as its US line. The Japanese scripts open the same pages in the same order as the
US ones in 806 of 810 rooms; M195 and the debug room M360 differ, and their pages are flagged
`ja_aligned: false`.

Decoding uses `data/ja_glyphs.tsv`, which maps each glyph of the Japanese font to its character.
It holds character identities only, no game text or art.
- `build_glyph_table.py` rebuilds it with Windows' Japanese OCR (`ocr.ps1`; needs the Japanese
  language pack), reading glyphs in rows and then every real line for context.
- A character in the `fixed` column wins over OCR and survives a rebuild; 181 glyphs are read by
  eye.
- `out/font/review.png` shows what still needs an eye, including any glyph read as a rare kanji
  (OCR tends to pick variant forms such as 逹 for 達).

The Japanese needs a proofread by a Japanese reader before it is used as more than a translator's
reference.

## Tests

- `python -I -m unittest discover -s localization/tests`: templates, the test language, the
  compiler's checks, and the pack round trip.
- `ctest -R mod-lang-pack`: the C reader's lookups by key, table and bank page, the bank measure
  and CRC, and damaged packs refused whole.
- In game: pe-native-debug's `scenarios/lang_items.py` loads a save with a pack and without, and
  takes pictures of the item screen and a phone's dialogue box.
