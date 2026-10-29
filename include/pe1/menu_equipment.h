#ifndef PE1_MENU_EQUIPMENT_H
#define PE1_MENU_EQUIPMENT_H

#include "pe1/inventory_slots.h"
#include "pe1/menu_context_help.h"
#include "pe1/menu_dialog.h"
#include "pe1/menu_state.h"
#include "pe1/menu_widget.h"

void Menu_StepSaveLoadScreen(void);
int Menu_SaveLoadNavigate(MenuWidgetNode *node, unsigned int flags);
int Menu_InventoryInputHandler(MenuWidgetNode *node, unsigned int flags);
void Menu_DrawArmorListContainer(MenuWidgetNode *node);
void Menu_DrawArmorSelectionList(MenuWidgetNode *node);
void Menu_DrawSelectableEquipSlotList(MenuWidgetNode *node);
void Menu_RebuildSelectableMask(void);
int Inv_TransferItem(int arg0, int arg1, int arg2, int arg3);
int Menu_GetBattleCount(void);
void MenuWidget_ClampCursor(MenuWidgetNode *node, int x, int y);

PE1_STATIC_ASSERT(PE1_OFFSETOF(MenuWidgetNode, itemAction) == PE1_OFFSETOF(MenuWidgetNode, popup_node) + sizeof(void *),
                  menu_widget_item_action_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(MenuWidgetNode, refreshItems) == PE1_OFFSETOF(MenuWidgetNode, itemAction) + sizeof(MenuWidgetItemAction),
                  menu_widget_refresh_items_offset);

void Menu_OpenEquipScreen(int mode);
int Menu_StepInventoryCategory(MenuWidgetNode *node, unsigned int flags);
void Menu_DrawEquipListContainer(MenuWidgetNode *node);

int Menu_EquipOptionsInputHandler(MenuWidgetNode *node, unsigned int flags);
void Menu_DrawEquipOptionsList(MenuWidgetNode *node);
void Menu_OpenInventoryScreen(void);
/* Equipment selection state and stat preview. */
extern int g_MenuItemUseMode, g_MenuItemRenameMode, g_MenuItemDiscardMode;
extern int g_MenuSourceItemSlot, g_MenuStatItemStatIndex, g_MenuStatItemAmount;
/* Ammo counts of the four ammo categories (999 when a category is
 * unlimited); a nonzero sum enables the ammo transfer actions. */
extern int g_StatBaseTable[4];
extern int g_MenuSelectedItemList, g_MenuSelectedItemSlot, D_8009CFBC, g_MenuSelectedItemIndex;
/* Item put back into Aya's inventory after a storage swap (0: none). */
extern int g_AyaFullInvHold;

void Menu_DrawEquipScreenHeader(MenuWidgetNode *unused);
int Menu_InventoryItemHandler(MenuWidgetNode *node, unsigned int flags);
void Menu_DrawScreenModeList(MenuWidgetNode *node);
int Menu_GetStatBaseValue(int index);
void Menu_DrawItemActionSubmenu(MenuWidgetNode *unused);
int Menu_StepSkillSelect(MenuWidgetNode *node, unsigned int flags);

void Menu_StepEquipSlotSelect(MenuWidgetNode *unused);
void Menu_DrawEquipStatsDelta(ItemDataRecord *preview);
void Menu_StepItemGridScroll(ItemDataRecord *item, ItemDataRecord *other);

int Menu_StepEquipScreen(MenuWidgetNode *node, unsigned int flags);
void Menu_OnInventoryItemConfirm(int index);
extern int g_MenuActiveItemList, g_MenuActiveItemSlot, g_MenuItemActionDisabled, g_MenuItemActionContext;
/* Three action IDs per menu configuration, indexed by the selected child cell. */
extern int g_MenuItemActionStateTable[][3];

void Menu_DrawAmmoTypeHeader(MenuWidgetNode *node);
void Menu_DrawEquipSortToggleRow(int index);
void Menu_DrawEquipInfoOptionRow(int index);

/* Sort option rows: the first row index per view mode, then the first,
 * second and third glyph of each row. */
/* Equipment slot glyphs for equip modes 1 and 2 (native rows 0 and 1). */
extern unsigned char g_MenuEquipSlotGlyphs[2][8];
/* Sort toggle row glyphs: the label, then the off and on markers. */
extern short g_MenuSortToggleGlyphs[];
/* Item icon glyph table for the current list (Text_LookupTable4 entry). */
extern unsigned char *g_MenuItemIconGlyphTable;
extern u8 g_MenuModSlotIconIndexOffsetByView[];
extern u8 g_MenuModSlotFirstGlyphs[];
extern u8 g_MenuModSlotSecondGlyphs[];
extern u8 g_MenuModSlotThirdGlyphs[];

#endif
