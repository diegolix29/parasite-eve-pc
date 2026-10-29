/* Language packs: the game's text from a .pelang pack (docs/LOCALIZATION.md).
 *
 * Packs are read from lang/ beside the game and lang/ in the user folder (beside pe_mods.ini);
 * F1 > Language picks one, or the game's own text. Anything a pack doesn't translate keeps the
 * game's text, so a half-finished pack is still playable.
 *
 * So far the item names and descriptions are replaced, through PE_HOOK_TEXT_LOOKUP. System
 * messages, the misc table and room dialogue need hooks the game doesn't have yet (phase 2). */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pe_hooks.h"
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
}

const PeMod pe_mod_pe_lang = {MOD_ID, "Language packs", "0.1", NULL, lang_init, lang_shutdown};
