#ifndef PE1_FIELD_MODEL_DRAW_H
#define PE1_FIELD_MODEL_DRAW_H

#include "common.h"
#include "pe1/gte_types.h"

/* Textured model packet helpers (fieldfx/model.c). The model
 * layout is FieldTexturedModel in pe1/field_textured_model.h. */
union FieldTexturedModel;

void FieldFx_ModelSetTexture(int tpage, int clut);
/* Scrolls the model's texture coordinates by the given offsets. */
void FieldFx_ModelScrollUv(union FieldTexturedModel *model, u8 x_offset, u8 y_offset);
void FieldFx_ModelSetSemiTrans(short value);
void FieldFx_ModelSetDepthBias(short value);
void FieldFx_ModelSaveColors(char *data);
void FieldFx_ModelRestoreColors(char *data);
void FieldFx_ModelScaleColors(char *data, u16 factor);
void FieldFx_ModelShiftColors(char *data, int r, int g, int b);
void FieldFx_ModelSetColors(char *data, u8 r, u8 g, u8 b);
/* Draws the model through placement. */
void FieldFx_ModelDraw(union FieldTexturedModel *model, GteMatrix *placement);

#endif
