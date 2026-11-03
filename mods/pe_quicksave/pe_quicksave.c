/* Quick save slots, from the Pathogen Eve port (where they were save states).
 *
 * A quick save is the game's own memory-card save: the same 8 KB card file Save_BuildCardFile
 * writes (with every mod's save records), kept beside the user's preferences as
 * quicksave_<slot>.mcd. Loading one is the card load (Save_LoadCardFileIntoRuntime without its
 * dialogs) followed by the field script's own room transfer to the saved room, so the game
 * restarts that room the way a loaded save does. A quick save holds what a save holds: Aya, items,
 * storage, story and the room, not her place in it or a fight in progress.
 *
 * F10 saves to the slot, F11 loads it, F12 picks the next slot; also F1 > Quick save, which closes
 * the panel. Both run on the next frame of any scene once a game is running (a load also from the
 * title, its logos and its intro movie): a battle, an event, a menu or a movie is left for the
 * saved room. Only a room change already under way is waited for.
 *
 * A save made during a cutscene (a story movie or a scripted event) can't be the moment itself:
 * the event has changed script variables, the screen fade and Aya's movement lock part way, and
 * the reloaded room would start from that half-done state (an event skipped or run twice, a black
 * screen). Such a save is the last moment before the cutscene instead: a copy of the save made when
 * the room started and again every second of free walking, which the reloaded room plays the
 * cutscene from as it did. */
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "pe_game.h"
#include "pe_mod_host.h"
#include "pe_mod_menu.h"
#include "pe1/battle_modifiers.h"
#include "pe1/battle_saved_state.h"
#include "pe1/game_state.h"
#include "pe1/inventory.h"
#include "pe1/boot.h"
#include "pe1/akao_send.h"
#include "pe1/memcard.h"
#include "pe1/menu_memcard_display.h"
#include "pe1/menu_widget.h"
#include "pe1/save.h"
#include "pe1/save_blob.h"
#include "pe1/scene_dispatch.h"
#include "pe1/script_queue.h"

#define MOD_ID "pe_quicksave"
#define PE_PAGE_QUICKSAVE "Quick save"
#define PE_SLOTS 10
#define PE_CARD_SIZE 0x2000
#define PE_CARD_CRC (0x100 + 0x12E4 + 0x8A8)   /* the checksum after the tail */
#define PE_RELOAD_SAME 0x800u                  /* g_GameState.flags: the same room again */
#define PE_SCRIPTED 0x2200u                    /* g_GameState.flags: a scripted event, a movie */
#define PE_TRANSFER 0x2000u                    /* g_GameStateFlags: a room transfer under way */
#define PE_SNAPSHOT_FRAMES 30                  /* free-walking frames between before-cutscene copies */

#define PE_TOKEN_NEW_GAME 0xAA108448u
#define PE_TOKEN_BOOT     0xA8000048u
#define PE_TOKEN_PROLOGUE 0xA80830C8u

enum { PE_NONE, PE_SAVE, PE_LOAD };
static int s_slot, s_pending, s_hotkeys = 1;
static bool s_keys_down[3];
/* The card of the last moment a save could be made as it is: the room's start, then free walking. */
static u8 s_before_card[PE_CARD_SIZE];
static unsigned s_before_token;
static int s_before_have, s_free_frames;

static void pe_slot_path(char *out, size_t size, int slot) {
    char name[32];
    snprintf(name, sizeof name, "quicksave_%d.mcd", slot);
    if (pe_mod_path(out, size, name) != 0) snprintf(out, size, "%s", name);
}

/* 1 now, 0 wait for a room change under way, -1 not possible here. */
static int pe_ready(int load) {
    const unsigned token = (unsigned)g_SceneDispatchToken;
    if (!token || token == PE_TOKEN_BOOT) return -1;
    if (!load && (!pe_save_loaded() || token == PE_TOKEN_NEW_GAME || token == PE_TOKEN_PROLOGUE)) return -1;
    return (unsigned)g_SceneDispatchCur == token;
}

/* A story movie or a scripted event: the moment itself can't be saved (see the top). */
static int pe_in_cutscene(void) {
    return Draw_CheckDrawStatus() != 0 || (g_GameStateFlagsWord & PE_SCRIPTED) || (g_GameStateFlags & PE_TRANSFER);
}

static u32 pe_card_crc(const u8 *card) {
    u32 crc = 0xFFFF;
    for (size_t i = 0; i < PE_CARD_SIZE; ++i) {
        const u8 value = i >= PE_CARD_CRC && i < PE_CARD_CRC + 4 ? 0 : card[i];
        crc ^= (u32)value << 8;
        for (unsigned bit = 0; bit < 8; ++bit) crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
    }
    return ~crc & 0xFFFF;
}

/* The card file of a save made now, into g_MemCardFileBuffer, as the crash report's room snapshot
 * makes it: the save menu's header fields for the copy only, and Aya's two record links set back
 * to the saved records for the copy when something changed them. */
static int pe_build_card(void) {
    static SaveRuntimeState header;
    char path[0x40];
    BattleStateTail *action = g_AyaBattleState.action;
    BattleAttributes *attributes = g_AyaBattleState.attributes;
    const int transfer = D_800A5D50;
    int r;

    memset(path, ' ', sizeof path);
    path[0x18] = 'A';                       /* the title's slot letter */
    path[sizeof path - 1] = 0;
    memcpy(&header, &g_SaveRuntimeState, sizeof header);
    Save_BuildHeader();
    if (action && action != &g_SavedBattleStateTail) g_AyaBattleState.action = &g_SavedBattleStateTail;
    if (attributes && attributes != &g_BattleEquipStateBlock) g_AyaBattleState.attributes = &g_BattleEquipStateBlock;
    r = Save_BuildCardFile(path);
    g_AyaBattleState.action = action;
    g_AyaBattleState.attributes = attributes;
    memcpy(&g_SaveRuntimeState, &header, sizeof header);
    D_800A5D50 = transfer;
    return r;
}

/* The before-cutscene copy, taken whole: the card buffer and the save cursor are left as they were. */
static void pe_take_before(void) {
    static u8 card[PE_CARD_SIZE];
    const unsigned token = (unsigned)g_SceneDispatchToken;
    u8 *cursor = g_SaveIoCursor;
    if (!pe_save_loaded() || pe_in_battle() || !token || token == PE_TOKEN_BOOT ||
        token == PE_TOKEN_NEW_GAME || token == PE_TOKEN_PROLOGUE)
        return;
    memcpy(card, g_MemCardFileBuffer, sizeof card);
    if (pe_build_card() == 0) {
        memcpy(s_before_card, g_MemCardFileBuffer, sizeof s_before_card);
        s_before_token = token;
        s_before_have = 1;
    }
    memcpy(g_MemCardFileBuffer, card, sizeof card);
    g_SaveIoCursor = cursor;
}

/* during: the save is the before-cutscene copy. */
static void pe_save(int slot, int during) {
    char path[1024], temp[1040], msg[64];
    const u8 *card = g_MemCardFileBuffer;
    FILE *f;
    int ok;
    if (during) {
        if (!s_before_have || s_before_token != (unsigned)g_SceneDispatchToken) {
            pe_toast("Quick save: not during this cutscene, try after it");
            return;
        }
        card = s_before_card;
    } else if (pe_build_card() < 0) {
        pe_toast("Quick save failed: the game can't be saved right now");
        return;
    }
    pe_slot_path(path, sizeof path, slot);
    snprintf(temp, sizeof temp, "%s.tmp", path);
    f = fopen(temp, "wb");
    ok = f && fwrite(card, 1, PE_CARD_SIZE, f) == PE_CARD_SIZE;
    if (f) ok = fclose(f) == 0 && ok;
    if (ok) {
        SDL_RemovePath(path);
        ok = SDL_RenamePath(temp, path);
    }
    if (!ok) {
        SDL_RemovePath(temp);
        pe_mod_log(MOD_ID, "slot %d: can't write %s", slot, path);
        pe_toast("Quick save failed: the file couldn't be written");
        return;
    }
    if (during) snprintf(msg, sizeof msg, "Quick saved to slot %d: from before this cutscene", slot);
    else snprintf(msg, sizeof msg, "Quick saved to slot %d", slot);
    pe_toast(msg);
    pe_log("quicksave", "slot=%d room=%d before_cutscene=%d", slot, pe_room_number(), during);
}

/* title: from the title menu, which then goes on as Continue does. 1 when the save was loaded. */
static int pe_load(int slot, int title) {
    char path[1024], msg[64];
    FILE *f;
    size_t n;
    u32 crc;
    int token, *args[1] = {&token};

    pe_slot_path(path, sizeof path, slot);
    f = fopen(path, "rb");
    if (!f) { snprintf(msg, sizeof msg, "Slot %d is empty", slot); pe_toast(msg); return 0; }
    n = fread(g_MemCardFileBuffer, 1, PE_CARD_SIZE, f);
    fclose(f);
    memcpy(&crc, g_MemCardFileBuffer + PE_CARD_CRC, sizeof crc);
    if (n != PE_CARD_SIZE || pe_card_crc(g_MemCardFileBuffer) != crc ||
        !Inv_ValidateSavedInventory((const SaveRuntimeState *)&g_MemCardSaveStateBuffer)) {
        snprintf(msg, sizeof msg, "Slot %d is damaged", slot);
        pe_toast(msg);
        return 0;
    }
    /* A movie stops as a skipped one does; open menus go with the scene. */
    if (!title) {
        if (Draw_CheckDrawStatus() != 0) {
            MenuCardFirst_StopVideo();
            Script_StopMovieMusic();
        }
        MenuWidget_InitPool();
    }
    /* Save_LoadCardFileIntoRuntime's commit, without the card menu's dialogs. */
    g_SaveIoCursor = g_MemCardFileBuffer + 0x100 + 0x12E4;
    if (Save_DeserializeTail() != 0) { pe_toast("Quick load failed: the save's records don't match"); return 0; }
    memcpy(&g_SaveRuntimeState, &g_MemCardSaveStateBuffer, sizeof g_SaveRuntimeState);
    g_SaveIoCursor += 4;
    Save_RestoreHeader();
    PE_HOOK(PE_HOOK_SAVE_READ, g_MemCardFileBuffer);
    token = g_SceneDispatchToken;
    snprintf(msg, sizeof msg, "Quick loaded slot %d", slot);
    pe_toast(msg);
    pe_log("quickload", "slot=%d token=%08X", slot, (unsigned)token);
    if (title) return 1;                    /* the title starts the saved room itself */
    /* Into the saved room, the way a field script changes rooms. The frame loop only ends for a
     * new token, so a save of the room Aya is in also asks for the game's same-room reload (the
     * Wardrobe's, script command 2101). */
    (void)Script_EncodeBarcode(args);
    if (token == g_SceneDispatchCur) g_GameState.flags |= PE_RELOAD_SAME;
    return 1;
}

static void pe_keys(void) {
    static const SDL_Scancode keys[3] = {SDL_SCANCODE_F10, SDL_SCANCODE_F11, SDL_SCANCODE_F12};
    const bool *state = SDL_GetKeyboardState(NULL);
    if (!state) return;
    for (int i = 0; i < 3; ++i) {
        const bool down = state[keys[i]];
        if (down && !s_keys_down[i] && s_hotkeys) {
            if (i == 0) s_pending = PE_SAVE;
            else if (i == 1) s_pending = PE_LOAD;
            else {
                char msg[32];
                s_slot = (s_slot + 1) % PE_SLOTS;
                snprintf(msg, sizeof msg, "Quick save slot %d", s_slot);
                pe_toast(msg);
            }
        }
        s_keys_down[i] = down;
    }
}

static int pe_on_frame(void *ctx, void *user) {
    int ready;
    (void)ctx; (void)user;
    pe_keys();
    /* Free walking: a fresh before-cutscene copy now and then, and on its first frame. */
    if (pe_free_field() && pe_game_loaded()) {
        if (++s_free_frames >= PE_SNAPSHOT_FRAMES) {
            s_free_frames = 0;
            pe_take_before();
        }
    } else {
        s_free_frames = PE_SNAPSHOT_FRAMES - 1;
    }
    if (!s_pending) return 0;
    ready = pe_ready(s_pending == PE_LOAD);
    if (ready < 0) {
        pe_toast(s_pending == PE_LOAD ? "Quick load: after the title screen appears" : "Quick save: start or load a game first");
        s_pending = PE_NONE;
        return 0;
    }
    if (!ready) return 0;
    if (s_pending == PE_SAVE) pe_save(s_slot, !pe_game_loaded() || pe_in_cutscene());
    else pe_load(s_slot, 0);
    s_pending = PE_NONE;
    return 0;
}
static int pe_on_scene_start(void *ctx, void *user) {
    (void)ctx; (void)user;
    s_before_have = 0;
    s_free_frames = 0;
    pe_take_before();
    return 0;
}
static int pe_on_title(void *ctx, void *user) {
    int *loaded = ctx;
    (void)user;
    pe_keys();
    if (s_pending == PE_SAVE) pe_toast("Quick save: start or load a game first");
    else if (s_pending == PE_LOAD && pe_load(s_slot, 1)) *loaded = 1;
    s_pending = PE_NONE;
    return 0;
}
static int pe_on_new_game(void *ctx, void *user) {
    (void)ctx; (void)user;
    s_pending = PE_NONE;
    s_before_have = 0;
    return 0;
}

static void pe_ask_save(void) { s_pending = PE_SAVE; pe_mod_panel_close(); }
static void pe_ask_load(void) { s_pending = PE_LOAD; pe_mod_panel_close(); }
static void pe_slot_info(char *out, unsigned size) {
    char path[1024];
    FILE *f;
    pe_slot_path(path, sizeof path, s_slot);
    f = fopen(path, "rb");
    snprintf(out, size, "%s", f ? "Saved" : "Empty");
    if (f) fclose(f);
}

static int quicksave_init(void) {
    pe_hook_add(PE_HOOK_FRAME, pe_on_frame, NULL, PE_PRIORITY_LATE, MOD_ID);
    pe_hook_add(PE_HOOK_SCENE_START, pe_on_scene_start, NULL, PE_PRIORITY_LATE, MOD_ID);
    pe_hook_add(PE_HOOK_TITLE_FRAME, pe_on_title, NULL, PE_PRIORITY_NORMAL, MOD_ID);
    pe_hook_add(PE_HOOK_NEW_GAME, pe_on_new_game, NULL, PE_PRIORITY_NORMAL, MOD_ID);

    pe_menu_begin(PE_PAGE_QUICKSAVE);
    pe_menu_top_page(PE_PAGE_QUICKSAVE, "Save and load at any moment, 10 slots.", 40);
    pe_menu_register_int(0, "Slot", "Which of the 10 slots F10 / F11 and the rows below use.",
                         &s_slot, 0, PE_SLOTS - 1, 1);
    pe_menu_register_info(0, "This slot", "Whether the slot holds a quick save.", pe_slot_info);
    pe_menu_register_action(0, "Save", "Save to the slot and return to the game.", pe_ask_save);
    pe_menu_register_action(0, "Load", "Load the slot: the saved room starts again, as a loaded save does.",
                            pe_ask_load);
    pe_menu_register_toggle(0, "Hotkeys", "F10 saves, F11 loads, F12 picks the next slot.", &s_hotkeys, NULL);
    pe_menu_begin(0);
    return 0;
}

const PeMod pe_mod_pe_quicksave = {MOD_ID, "Quick save", "0.1", NULL, quicksave_init, NULL};
