#ifndef PE1_MEMCARD_SAVE_STATE_H
#define PE1_MEMCARD_SAVE_STATE_H

/* Declarations used only by the memory card save state machine
 * (MemCard_UpdateSaveState). */

#include "pe1/memcard.h"
#include "pe1/menu_inventory.h"
#include "pe1/menu_notification_dialog.h"
#include "pe1/psyq_bios.h"
#include "pe1/psyq_libc.h"

/* PSY-Q struct DIRENTRY as filled by firstfile/nextfile. */
typedef struct MemCardDirEntry {
    char name[20];
    s32 attr;
    s32 size;
    struct MemCardDirEntry *next;
    s32 head;
    char system[4];
} MemCardDirEntry;

/* SDK firstfile wrapper (FIRST.OBJ); returns the entry or 0. */
void *firstfile(const char *name, void *entry);
void *nextfile(void *entry);
long format(char *device);


unsigned int MemCard_GetDialogMode(void);
int Menu_CreateSaveSlotListView(int port, int count);
void Menu_ShowMemCardErrorDialog(int port);
void Menu_DestroyMemCardProgressWidget(void);
void Save_LoadCardFileIntoRuntime(void);

/* Card directory path "bu00:" whose port digit is patched before scans. */
extern char *g_MemCardSearchPatternPtr;

void MemCard_UpdateSaveState(int port);

#endif
