#ifndef PE1_ASSET_FIND_H
#define PE1_ASSET_FIND_H

#include "pe1/asset_table.h"

/* Positional triggers use the original signed/unsigned halfword coordinates. */
int Asset_Find08w(int id, int mode, s16 x, s16 y, s16 z);
int Asset_Find08Alt(int id, int mode, s16 x, s16 y, s16 z);
int Akao_SetPos3D(int id, int mode, u16 x, u16 y, u16 z);

#endif
