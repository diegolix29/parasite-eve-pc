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
| Accented letters (Latin Extended: French, German, Spanish, Italian, Portuguese, Polish, Czech, Turkish) | Done |
| Cyrillic | Not started |
| Chinese, Japanese, Korean | Needs the per-area glyph pages (phase 5) |

A pack can use the US font's glyphs (A-Z, a-z, 0-9, space and `+ - % & ! ? " ' . : , /`) and the
Latin Extended set below. The compiler names any other character it finds.

## The Latin Extended letters

111 glyphs, defined in `localization/petext/latin.py`:

- **Letters with marks**, lowercase and capital: à á â ã ä å ç è é ê ë ì í î ï ñ ò ó ô õ ö ø ù ú
  û ü ý ÿ, ą ć ę ł ń ś ź ż, č ď ě ň ř š ť ů ž, ğ ı İ ş, ő ű.
- **Signs**: ß æ Æ œ Œ « » € ° ¿ ¡.

They are made from the game's own letters at run time, so no game art is in the repo or in a pack:
- When the dialogue font goes to VRAM (`PE_HOOK_TIM_UPLOAD`), `pe_lang` composes each glyph from its
  base letter plus a mark drawn in `latin.py` (é is the font's own `e` plus an acute), into the
  font's empty cells, codes 0x4D-0xBB.
- Over a capital it first drops one repeated stem row, so the mark fits without moving the baseline.
- The few signs that aren't letter + mark (ß, æ, «, €) are drawn in `latin.py` in the font's style.
  ¿ and ¡ are the font's ? and ! turned over.
- Widths come from the base letter: the dialogue box reads them through `g_TextboxGlyphWidths` (the
  original table unless a mod points it at a longer one), the menus from the resident glyph
  metrics, for those codes only.

`python -I preview_latin.py US1.cue` draws the set from your disc to `out/latin.png`, with sample
words. After changing `latin.py`, run `python -I pelang.py latin-header` to regenerate
`mods/pe_lang/pe_lang_latin.h` and the test vectors; the tests check both are current, and
`mod-lang-font` checks the C composer against the Python one pixel for pixel.

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
   the boxes it shows in, `banks.json` (each message bank's CRC32), `menu_glyphs.json` (the menus'
   glyph widths), plus the item reports. Keys: `sys/NNN`, `item/NNN`, `desc/NNN`, `misc/NNN`
   for the resident text tables, `dlg/MNNN/PPP` for room dialogue (the message bank first used by
   room MNNN, page PPP). The 148 table slots that hold filler the game never shows are left out.
2. **Template.** `pelang.py template` writes the translator CSV: key, English, Japanese (for
   reference), translation, box, widest line, rooms, notes. `--merge old.csv` carries the
   translations of an earlier template over, by key, and notes rows whose English changed.
3. **Translate.** In any spreadsheet. Keep the tags: `{br}` line break, `{wait}` next screen,
   `{name}` Aya's name, `{choice N}` a choice of N options, `{red}`/`{/red}` highlight, `{num}` a
   number (see `petext/gametext.py`). An empty translation keeps the game's text.
4. **Build.** `pelang.py build` checks every row and writes the pack.
   - Errors stop it: unknown/duplicate keys, unsupported characters, malformed command operands,
     embedded terminators, changed control sequences, or overflow in a known dialogue box.
     Commands are compared after encoding, so raw tags cannot bypass validation. Line breaks may
     move; waits, choices, names, numbers, pauses, highlighting and extra-page glyphs keep their
     count, order and operands. The compiler supplies the original page ending itself.
   - Dialogue checks live in `petext/layout.py`. The default box allows 297 px and four lines per
     screen. A styled box neither insets nor clips its text, so going past its rectangle is a
     warning and only the screen edge is an error. Waits reset the line count. `{name}` is
     measured as the pack's `sys/030` (Aya's default name, which the dialogue uses until the
     player renames her; the English is laid out for "Aya"), and numbers reserve five 12-pixel
     digits. With these rules the original English passes every check.
   - A page no script ties to a box is checked against every box its bank's rooms open (only
     opcodes 13 and 209 show a bank's pages), as a warning, since the page may be unused or opened
     from a room whose scripts don't decode.
   - Menu checks live in `petext/menu.py` and `data/menu_layouts.json`. Menu text is measured as the
     menus draw it (the game's glyph metrics, which `extract_text.py` saves as `menu_glyphs.json`).
     Each context names the code that draws it and its limits:
     - help line: 294 px (all descriptions, most `misc`, the option hints);
     - item list: 104 px, or 86 px for item kinds whose count is drawn after the name;
     - dialog lines and confirm labels: 63 bytes (they are copied unbounded into 64-byte buffers)
       and the screen width;
     - the item message: 31 bytes, an item's name and `sys/016`/`017` together (more and the game
       shows nothing);
     - the rename grids (`sys/115`–`117`): the same number of cells as the English;
     - the default name `sys/030`: 8 letters.

     Byte limits and the screen edge are errors. The help line and item list widths were
     confirmed in game, and the other widths are warnings. Text the game joins (a name and its
     suffix) is checked with the pack's longest of each part. Keys no context names yet (mostly
     option labels centred in grid cells, and location names) are compared with the widest
     English entry of their table.
   - Every choice's option meaning/order also needs human review; matching control bytes cannot
     prove that.
   - For deliberate overflow/pseudo testing only, `--allow-overflow` turns known layout failures
     into warnings. It never permits malformed or changed controls.
5. **Play.** Put the `.pelang` in `lang/` beside the game, or in `lang/` in the user folder (beside
   `pe_mods.ini`). Build the game with `pe_lang` in `PE_MOD_LIST`, turn All mods on, and pick the
   language under F1 > Language.

`pelang.py pseudo` makes a test language from every entry: letters swap case, and each entry is
marked with `!` and padded about 30%. Build it like any pack (`--lang qps`) to see what text a pack
reaches and where longer text overflows. Use `--allow-overflow` when compiling this deliberately
expanded test text. Extracted rows containing unsupported control bytes are left untranslated
with a note, retaining the game's own text rather than bypassing validation.

Language selection is stored separately in `pe_lang_selection.dat` beside `pe_mods.ini`, using
the pack's language code and display/edition name. Filenames and list positions are not identity.
Keep these two header fields stable across pack updates; give different editions distinct names.
An identical identity in the user folder overrides the copy beside the game. If a selected pack
is missing, Game text is used and the identity is retained until a new selection is made.
Old numeric `Language/Language` preferences are intentionally ignored: select your language once
after upgrading, because the old index cannot establish which pack was intended.

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
- **Choices** are read back by position. The compiler preserves choice commands and their counts;
  translators must also preserve the option meanings and order. A page that ends in F9 keeps that end.

## Pack format

Little-endian, version 1 (`pelang.py` writes it, `mods/pe_lang/pe_lang_pack.c` reads it):

| Part | Layout |
| --- | --- |
| Header, 64 bytes | `"PELG"`, u32 version, u32 entry count, u32 offset of the text, char lang[16], char name[32] (UTF-8, NUL-padded) |
| Entries | count × {u32 key hash, u32 text offset from the text start}, sorted by hash |
| Text | each entry's glyph bytes followed by `FF` (a page that ends in F9 keeps it before the `FF`) |

The key hash is 32-bit FNV-1a of the key's UTF-8 (`item/054`). The compiler refuses a collision,
and the reader checks contiguous entry spans, complete commands and exact terminators at load,
refusing a damaged pack whole. Entries use the same order in the table and text payload, without gaps. Besides
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
- `ctest -R mod-lang-font`: the C composer against `latin.py` on synthetic letters, page fill
  (only empty cells change), widths.
- In game: pe-native-debug's `scenarios/lang_items.py` loads a save with a pack and without, and
  takes pictures of the item screen and a phone's dialogue box.
