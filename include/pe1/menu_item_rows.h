#ifndef PE1_MENU_ITEM_ROWS_H
#define PE1_MENU_ITEM_ROWS_H

#include "pe1/inventory.h"
#include "pe1/menu_draw_list.h"
#include "pe1/menu_widget.h"

void Menu_DrawWeaponList(MenuWidgetNode *node);
void Menu_DrawArmorList(MenuWidgetNode *node);
void Menu_DrawInventoryItemRow(int index);
void Menu_DrawArmorItemRow(int index);
void Menu_DrawArmorItemDetail(int index);
void Menu_DrawActiveListSlot(int index);
void Inv_ShowItemDescriptionPanel(int index);
void Draw_SetTextDimmed(int dimmed);
void Draw_DropShadow(void);
void MenuWidget_DrawCenteredTableText(int textId);
void Menu_DrawEquipSlotListItem(int index);
void Menu_DrawBlendColorChannelRow(int index);
void Menu_DrawBlendColorOptionRow(int index);
void Menu_DrawStatAllocationList(MenuWidgetNode *node);
void Menu_UpdateStatBarAnimation(int stat);
void Menu_DrawWeaponComparisonPanel(void);
void Menu_DrawSlotRow(ItemDataRecord *entry, u8 *text);
void Menu_DrawSaveSlotEntry(int index);
void Menu_DrawContextActionList(MenuWidgetNode *node);
void Menu_DrawEquipModList(MenuWidgetNode *node);
void Menu_DrawUsableItemActionList2(MenuWidgetNode *node);
void Menu_DrawItemIconList(MenuWidgetNode *node);
void Menu_DrawMemCardSlot1List(MenuWidgetNode *node);
void Menu_DrawMemCardSlot2List(MenuWidgetNode *node);
void Menu_DrawMemCardSlot3List(MenuWidgetNode *node);
/* List adapters binding a widget list to its row drawer. */
void Menu_DrawMainMenuList(MenuWidgetNode *node);
void Menu_DrawActionSubmenuList(MenuWidgetNode *node);
void Menu_DrawCursorModeList(MenuWidgetNode *node);

#endif
