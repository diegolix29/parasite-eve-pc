#ifndef PE1_MENU_INVENTORY_ROOT_H
#define PE1_MENU_INVENTORY_ROOT_H

/* Callbacks and state wired up by Menu_StepInventoryRoot. */

#include "pe1/aya.h"
#include "pe1/inventory.h"
#include "pe1/menu_bonus_stats.h"
#include "pe1/menu_dialog.h"
#include "pe1/menu_equipment.h"
#include "pe1/menu_item_list_input.h"

int Menu_InventoryPageNavHandler(MenuWidgetNode *root, unsigned int flags);
void Menu_DrawBattleCommandList(MenuWidgetNode *node);
void Menu_DrawEquipSelectionList(MenuWidgetNode *node);
void Inv_BuildFilteredPackedListExcluding(int mask, int excluded);
void Menu_DrawBonusPointSlotValue(MenuWidgetNode *unused);
void Menu_DrawParasiteAbilityList(MenuWidgetNode *node);
void Menu_InitStateTables(void);
void Menu_DrawItemDetailPanel(MenuWidgetNode *panel);
int Menu_BonusPointCancelHandler(MenuWidgetNode *node, unsigned int flags);
void Menu_DrawEmptyList(MenuWidgetNode *node);
void Menu_StepEquipSlotSelect2(MenuWidgetNode *unused);
void Menu_SetEquipPanelsCursorY(void);
void Menu_AlignEquipPanels(void);

/* Whether the active list override was selected when the root opened. */
extern int g_MenuSourceItemList;
/* Per-stat preview tables: value, level, sublevel and pending bonus. */
extern int D_800A1898[7];

#endif
