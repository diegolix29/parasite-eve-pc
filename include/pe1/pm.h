#ifndef PE1_PM_H
#define PE1_PM_H

#include "pe1/pe_image.h"

#include "pe1/pm_types.h"

/* Process manager: 11 primary slots (0..10) and 11 secondary slots (11..21).
 * Commands 0..84 dispatch through g_PmCmdHandlerTable; commands 85..191 all
 * use handler 85, which runs field-animation task program (command - 85). */

/* The three tables are adjacent words; this record views them together.
 * Asset_LoadTimTextures stores the slot bank pointers through it and reads
 * them back on every iteration, which a record member requires and a
 * scalar does not. */
typedef struct PmTables {
    PmCommand **handlers;
    PmPrimarySlot *primary;
    PmSecondarySlot *secondary;
} PmTables;

extern PmCommand *g_PmCmdHandlerArray[86];
extern PmTables g_PmTables;
#define g_PmCmdHandlerTable (g_PmTables.handlers)
#define g_PmSlotTable (g_PmTables.primary)
#define g_PmSlotTable2 (g_PmTables.secondary)
void Pm_InitScene(void);

/* Original on-demand effect bank is linked statically as seven native programs. */
extern FieldFxTaskProgram g_FieldFxTaskProgramCmd108, g_FieldFxTaskProgramCmd109, g_FieldFxTaskProgramCmd110;
extern FieldFxTaskProgram g_FieldFxTaskProgramCmd111, g_FieldFxTaskProgramCmd112, g_FieldFxTaskProgramCmd113, g_FieldFxTaskProgramCmd114;

int Pm_AllocSlot(unsigned int command);
int Pm_FreeSlot(int slot);
int Scene_LoadRoomAssets(u32 command, void *owner);
/* arg3..arg5 are values, or result addresses when arg1 == 1 and arg2 == 0. */
int Pm_SendCmd(int slot, int arg1, int arg2, intptr_t arg3, intptr_t arg4, intptr_t arg5);
/* set == 0 stores value (< 6) as the slot state; otherwise value is the
 * address that receives the state. Returns the state. */
int Pm_SetGetState(int slot, int set, intptr_t value);
int Pm_Start(int slot);
int Pm_Exec(int slot);
/* Stops the process in a slot; unless forced, only when owner matches. */
int Pm_Stop(int slot, void *owner, int force);
int Scene_FreeEntityTable(void *owner);
/* Return the first failing Pm_Stop result. */
s32 Pm_StopAll(void);
s32 Pm_StopLowerHalf(void);
s32 Pm_StopUpperHalf(void);
void Pm_StopAllBoth(void);
int Pm_RunPrimary(void);
int Pm_RunBatch(void);

#endif
