/* Language packs: the game's text from a .pelang pack (docs/LOCALIZATION.md).
 *
 * Packs are read from lang/ beside the game and lang/ in the user folder (beside pe_mods.ini);
 * F1 > Language picks one, or the game's own text. Anything a pack doesn't translate keeps the
 * game's text, so a half-finished pack is still playable.
 *
 * The resident text tables (system messages, item names and descriptions, the rest) come through
 * PE_HOOK_TEXT_LOOKUP. Room dialogue comes through PE_HOOK_TEXT_PAGE: the pack names each English
 * message bank by its CRC32 ("bank/<crc32>"), so the bank a box reads from is measured once per
 * room and its pages looked up as dlg/<bank>/<page>. A bank the pack doesn't know (another disc
 * version, a modded room) keeps the game's text.
 *
 * Packs may use the Latin Extended letters (é, ñ, ß, « ...): when the dialogue font goes to VRAM
 * (PE_HOOK_TIM_UPLOAD) they are composed from the game's own letters into its empty cells, codes
 * 0x4D on; the dialogue box gets their widths through g_TextboxGlyphWidths, and the menus through
 * the resident glyph metrics (only those codes' entries, which the game's text never uses). */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "pe1/psyq_tim.h"
#include "pe1/resident_tables.h"
#include "pe1/textbox.h"
#include "pe_hooks.h"
#include "pe_lang_font.h"
#include "pe_lang_pack.h"
#include "pe_mod_host.h"
#include "pe_mod_menu.h"

#define MOD_ID "pe_lang"
#define MAX_PACKS 16

static PeLangPack s_packs[MAX_PACKS];
static unsigned s_pack_count;
static char s_labels[MAX_PACKS][64];
static const char *s_names[MAX_PACKS + 1];
static int s_choice;                    /* 0: the game's own text, else s_packs[s_choice - 1] */

static const PeLangPack *active_pack(void) {
    return s_choice > 0 && (unsigned)s_choice <= s_pack_count ? &s_packs[s_choice - 1] : NULL;
}

static int on_text_lookup(void *ctx, void *user) {
    PeHookTextLookup *lookup = ctx;
    const unsigned char *text = pe_lang_pack_table(active_pack(), lookup->table, lookup->index);
    (void)user;
    if (text) *lookup->text = (void *)text;
    return 0;
}

/* The bank the last box read from and its CRC32, measured once per room. */
#define MAX_BANK_BYTES 0x20000u
static const unsigned char *s_bank;
static uint32_t s_bank_crc;
static int s_bank_known;

static int on_text_page(void *ctx, void *user) {
    PeHookTextPage *box = ctx;
    const unsigned char *text;
    (void)user;
    if (!active_pack() || !box->bank) return 0;
    if (box->bank != s_bank) {
        size_t size = pe_lang_bank_size(box->bank, MAX_BANK_BYTES);
        s_bank = box->bank;
        s_bank_known = size != 0;
        s_bank_crc = s_bank_known ? pe_lang_crc32(box->bank, size) : 0;
    }
    if (s_bank_known && (text = pe_lang_pack_page(active_pack(), s_bank_crc, box->page)))
        *box->message = (unsigned char *)text;
    return 0;
}

/* ---- the Latin Extended glyphs ---- */

#define FONT_CODES 0xF1u                 /* single-byte glyph codes the box can draw: 0x00-0xF0 */
static TextboxGlyphSpacing s_widths[FONT_CODES];
static int s_font_added;

/* The US dialogue font page: 4-bit, its image at VRAM 0x140,0 (64 x 256 words), its palette at
 * 0x140,0xFC (src/main/text/tables.c, g_FieldFontTextureState). */
static unsigned char *font_pixels(TimFile *tim) {
    if ((tim->flags & 7) != 0 || !(tim->flags & 8)) return NULL;
    TimBlock *clut = &tim->first_block;
    TimBlock *image = (TimBlock *)&clut->bytes[clut->header.length];
    const RECT *c = &clut->header.rect, *r = &image->header.rect;
    if (c->x != 0x140 || c->y != 0xFC || r->x != 0x140 || r->y != 0 || r->w != 64 || r->h != 256) return NULL;
    return (unsigned char *)image->header.pixels;
}

/* The widths of the new codes, for the dialogue box and for the menus. */
static void install_widths(void) {
    unsigned char advance[78], *metrics;
    for (unsigned i = 0; i < 78; ++i) advance[i] = g_TextboxGlyphSpacing[i].right;
    memset(s_widths, 0, sizeof s_widths);
    memcpy(s_widths, g_TextboxGlyphSpacing, sizeof g_TextboxGlyphSpacing);
    for (unsigned k = 0; k < PE_LATIN_COUNT; ++k)
        s_widths[kPeLatinGlyphs[k].code].right = (u8)pe_lang_font_advance(&kPeLatinGlyphs[k], advance);
    g_TextboxGlyphWidths = s_widths;
    g_TextboxGlyphCount = FONT_CODES;
    if (!g_ResidentDataPack.directory.glyphMetrics) return;     /* not loaded yet: next scene */
    metrics = &g_ResidentDataPack.bytes[g_ResidentDataPack.directory.glyphMetrics];
    for (unsigned k = 0; k < PE_LATIN_COUNT; ++k)
        metrics[kPeLatinGlyphs[k].code] = pe_lang_font_metric(&kPeLatinGlyphs[k], metrics);
}

static int on_tim_upload(void *ctx, void *user) {
    unsigned char *pixels = font_pixels(ctx);
    (void)user;
    if (!pixels) return 0;
    int added = pe_lang_font_fill(pixels, 128, 256);
    if (!s_font_added && added) pe_mod_log(MOD_ID, "added %d Latin Extended glyphs to the dialogue font", added);
    if (added) {
        s_font_added = 1;
        install_widths();
    }
    return 0;
}

/* A new room can load its bank where the last one was: measure again. The resident pack can be
 * read again too, so the menu widths go back in. */
static int on_scene_start(void *ctx, void *user) {
    (void)ctx;
    (void)user;
    s_bank = NULL;
    if (s_font_added) install_widths();
    return 0;
}

static int by_name(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* Load the packs in `dir` (ending in a separator), in file-name order. */
static void load_dir(const char *dir) {
    int count = 0;
    char **files = SDL_GlobDirectory(dir, "*.pelang", 0, &count);
    if (!files) return;
    qsort(files, (size_t)count, sizeof *files, by_name);
    for (int i = 0; i < count && s_pack_count < MAX_PACKS; ++i) {
        char path[4096];
        PeLangPack *pack = &s_packs[s_pack_count];
        snprintf(path, sizeof path, "%s%s", dir, files[i]);
        if (pe_lang_pack_load(pack, path)) {
            pe_mod_log(MOD_ID, "skipped %s: not a valid version %u language pack", path, PE_LANG_PACK_VERSION);
            continue;
        }
        snprintf(s_labels[s_pack_count], sizeof s_labels[0], "%s (%s)", pack->name, pack->lang);
        pe_mod_log(MOD_ID, "%s: %s, %u entries", path, s_labels[s_pack_count], pack->count);
        ++s_pack_count;
    }
    SDL_free(files);
}

static void active_text(char *out, unsigned size) {
    const PeLangPack *pack = active_pack();
    if (pack) snprintf(out, size, "%s, %u entries", pack->name, pack->count);
    else snprintf(out, size, s_pack_count ? "the game's own text" : "no packs found");
}

static int lang_init(void) {
    char dir[4096];
    const char *base = SDL_GetBasePath();
    if (base) {
        snprintf(dir, sizeof dir, "%slang/", base);
        load_dir(dir);
    }
    if (!pe_mod_path(dir, sizeof dir, "lang/")) load_dir(dir);

    s_names[0] = "Game text";
    for (unsigned i = 0; i < s_pack_count; ++i) s_names[i + 1] = s_labels[i];
    pe_hook_add(PE_HOOK_TEXT_LOOKUP, on_text_lookup, NULL, PE_PRIORITY_EARLY, MOD_ID);
    pe_hook_add(PE_HOOK_TEXT_PAGE, on_text_page, NULL, PE_PRIORITY_EARLY, MOD_ID);
    pe_hook_add(PE_HOOK_SCENE_START, on_scene_start, NULL, PE_PRIORITY_NORMAL, MOD_ID);
    pe_hook_add(PE_HOOK_TIM_UPLOAD, on_tim_upload, NULL, PE_PRIORITY_NORMAL, MOD_ID);

    pe_menu_begin("Language");
    pe_menu_top_page("Language", "Play with the game's text from a language pack.", 0);
    pe_menu_register_choice(0, "Language",
                            "A pack from lang/ beside the game or in the user folder; Game text turns it off.",
                            &s_choice, s_names, s_pack_count + 1);
    pe_menu_register_info(0, "In use", "Untranslated text stays as the game has it.", active_text);
    pe_menu_begin(0);
    return 0;
}

static void lang_shutdown(void) {
    for (unsigned i = 0; i < s_pack_count; ++i) pe_lang_pack_free(&s_packs[i]);
    s_pack_count = 0;
    s_choice = 0;
    s_bank = NULL;
    if (s_font_added) {
        g_TextboxGlyphWidths = g_TextboxGlyphSpacing;
        g_TextboxGlyphCount = sizeof g_TextboxGlyphSpacing / sizeof *g_TextboxGlyphSpacing;
        s_font_added = 0;
    }
}

const PeMod pe_mod_pe_lang = {MOD_ID, "Language packs", "0.1", NULL, lang_init, lang_shutdown};
