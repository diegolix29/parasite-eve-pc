#ifndef PE_LANG_PACK_H
#define PE_LANG_PACK_H

/* A .pelang language pack (localization/pelang.py writes them; docs/LOCALIZATION.md):
 * translations by key ("item/054", "dlg/M001/002"), already in the game's glyph code, each
 * ending in FF. Plain data: no SDL and no game state, so it is tested on its own. */

#include <stddef.h>
#include <stdint.h>

#define PE_LANG_PACK_VERSION 1u

typedef struct PeLangPack {
    unsigned char *data;    /* the whole file, owned by the pack */
    size_t size;
    unsigned count;
    char lang[16], name[32];
} PeLangPack;

/* FNV-1a 32 of the key's bytes, as pelang.py hashes it. */
uint32_t pe_lang_hash(const char *key);

/* Take `data` (malloc'd, `size` bytes) as a pack after checking every offset in it. Returns 0 and
 * owns the data, or -1 and frees it: a damaged pack is never half used. */
int pe_lang_pack_parse(PeLangPack *pack, unsigned char *data, size_t size);
int pe_lang_pack_load(PeLangPack *pack, const char *path);
void pe_lang_pack_free(PeLangPack *pack);

/* The translation of `key` (glyph bytes ending in FF), or NULL when the pack has none. */
const unsigned char *pe_lang_pack_find(const PeLangPack *pack, const char *key);

/* The translation for a resident text table lookup (PE_HOOK_TEXT_LOOKUP): table 8 is item
 * names ("item/NNN"), 12 item descriptions ("desc/NNN"); NULL for anything else. */
const unsigned char *pe_lang_pack_table(const PeLangPack *pack, unsigned table, unsigned index);

#endif
