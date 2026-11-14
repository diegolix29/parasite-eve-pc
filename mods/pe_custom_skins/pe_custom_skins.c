/* Custom skins: every folder pe_skins/<folder>/ (beside the preferences) that holds a skin.ini becomes
 * a free Wardrobe costume, so a player-made Aya skin needs no C code. The folder holds the game's own
 * player formats (model.bin, texture.bin, day1-8.bin, meta.bin), made by mods/pe_custom_skins/pe_aya_skin.py
 * from the player's own disc; the Wardrobe (pe_wardrobe) loads and swaps it in, and hides it while
 * model.bin is missing. skin.ini, one "name = value" per line ('#' starts a comment):
 *   name  = Aya: My look        shown in the Wardrobe (default: the folder name)
 *   key   = custom/my_look      saved in players' profiles: never change it once shared
 *                               (default: custom/<folder>)
 *   group = Custom skins        the Wardrobe filter group (default: Custom skins)
 * Folders are read once at start: a new skin shows after a restart. */
#include <SDL3/SDL.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "pe_mod_host.h"
#include "pe_services.h"

#define MOD_ID     "pe_custom_skins"
#define MAX_SKINS  64
#define TEXT_MAX   64

typedef struct {
    char folder[TEXT_MAX], key[TEXT_MAX], name[TEXT_MAX], group[TEXT_MAX];
    PeCostume costume;
} CustomSkin;

static CustomSkin s_skin[MAX_SKINS];
static int s_count;
static char s_root[512];

static void trim(char *s) {
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = 0;
    size_t i = 0;
    while (s[i] && isspace((unsigned char)s[i])) ++i;
    if (i) memmove(s, s + i, strlen(s + i) + 1);
}

static int folder_ok(const char *f) {
    size_t n = strlen(f);
    if (!n || n >= TEXT_MAX - 8) return 0;
    for (; *f; ++f)
        if (!isalnum((unsigned char)*f) && *f != '_' && *f != '-') return 0;
    return 1;
}

static void read_skin(const char *folder) {
    char path[640], line[256];
    FILE *f;
    CustomSkin *k;
    if (s_count == MAX_SKINS || !folder_ok(folder)) return;
    snprintf(path, sizeof path, "%s/%s/skin.ini", s_root, folder);
    if (!(f = fopen(path, "r"))) return;
    k = &s_skin[s_count];
    memset(k, 0, sizeof *k);
    snprintf(k->folder, sizeof k->folder, "%s", folder);
    snprintf(k->key, sizeof k->key, "custom/%s", folder);
    snprintf(k->name, sizeof k->name, "%s", folder);
    snprintf(k->group, sizeof k->group, "Custom skins");
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '='), *hash = strchr(line, '#');
        if (hash && (!eq || hash < eq)) continue;
        if (!eq) continue;
        *eq = 0;
        trim(line);
        trim(eq + 1);
        if (!eq[1]) continue;
        if (!strcmp(line, "name")) snprintf(k->name, sizeof k->name, "%s", eq + 1);
        else if (!strcmp(line, "key")) snprintf(k->key, sizeof k->key, "%s", eq + 1);
        else if (!strcmp(line, "group")) snprintf(k->group, sizeof k->group, "%s", eq + 1);
    }
    fclose(f);
    k->costume.key = k->key;
    k->costume.name = k->name;
    k->costume.group = k->group;
    k->costume.price = 0;          /* free: owned from the start */
    k->costume.bank = -1;          /* a skin folder */
    k->costume.bundle = k->folder;
    ++s_count;
}

static SDL_EnumerationResult on_entry(void *user, const char *dirname, const char *fname) {
    (void)user;
    (void)dirname;
    read_skin(fname);
    return SDL_ENUM_CONTINUE;
}

static int custom_skins_init(void) {
    const PeCostumesApi *c = pe_service_get("costumes", 1);
    int i, added = 0;
    if (!c) {
        pe_mod_log(MOD_ID, "no costumes service: is pe_wardrobe on?");
        return 1;
    }
    if (pe_mod_path(s_root, sizeof s_root, "pe_skins") != 0) return 0;
    SDL_EnumerateDirectory(s_root, on_entry, NULL);
    for (i = 0; i < s_count; ++i) {
        if (c->add(&s_skin[i].costume) < 0) {
            pe_mod_log(MOD_ID, "skipped %s: key %s taken or Wardrobe full", s_skin[i].folder, s_skin[i].key);
            continue;
        }
        pe_mod_log(MOD_ID, "costume %s (pe_skins/%s)", s_skin[i].name, s_skin[i].folder);
        ++added;
    }
    pe_mod_log(MOD_ID, "%d custom skin%s", added, added == 1 ? "" : "s");
    return 0;
}

static const char *const s_requires[] = {"pe_wardrobe", NULL};
const PeMod pe_mod_pe_custom_skins = {MOD_ID, "Custom skins", "1.0", s_requires, custom_skins_init, NULL};
