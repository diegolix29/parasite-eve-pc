#ifndef PE1_INVENTORY_QUERIES_H
#define PE1_INVENTORY_QUERIES_H

/* Inventory list helpers (item/active_list.c). */

/* Number of item slots Aya currently has. */
int Inv_GetAyaSlotLimit(void);
/* Selects the active item list; nonzero useOverride picks the override list. */
void Inv_SelectActiveList(int useOverride);
short Inv_GetActiveListItem(int index);

#endif /* PE1_INVENTORY_QUERIES_H */
