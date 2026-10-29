#ifndef FX_COMMON_H
#define FX_COMMON_H

#include "common.h"
#include "pe1/akao/pan3d.h"
#include "pe1/akao_send.h"
#include "pe1/asset_tim.h"
#include "pe1/gte_types.h"
#include "pe1/projection_slot.h"
#include "pe1/psyq_gpu.h"
#include "pe1/render_matrix_slot.h"
#include "pe1/render_packets.h"
#include "pe1/room_fx.h"

#include "fx_common_vectors.h"

typedef struct FxCommonSelection {
    s16 record;
    s16 pad2;
} FxCommonSelection;

typedef struct FxCommonIndexLink {
    s16 previous;
    s16 next;
} FxCommonIndexLink;

typedef union FxCommonTransformSeed {
    RoomFxSeed8 room;
    s16 component[4];
    GteShortVector angles;
} FxCommonTransformSeed;

/* One native runtime record for ordinary and level-of-detail nodes. */
typedef struct FxCommonNode FxCommonNode;
typedef FxCommonNode FxCommonRecord;
typedef FxCommonNode FxCommonTransformNode;
typedef FxCommonNode FxCommonLodNode;
struct FxCommonNode {
    s16 id;
    union {s16 type, kind, farKind;};
    s16 middleKind, nearKind;
    void *resource;
    /* The two scalar views overlap in the original runtime. Keep that
     * relationship on reuse; pointer ownership is separate and native. */
    union {
        struct {
            RoomSpriteMatrix matrix;
            FxCommonTransformSeed seed;
            s16 reserved, margin;
        };
        struct {
            u32 reserved;
            RoomSpriteMatrix matrix;
            FxCommonTransformSeed seed;
            s16 reserved34, margin;
            s32 farDepth, nearDepth;
        } lod;
    };
    s16 bucket;
    FxCommonNode *parent, *previous, *next;
};

typedef struct FxCommonBuffer {
    u8 *data;
    RenderNativeTag *allocation;
    u8 *end;
    int error;
} FxCommonBuffer;

typedef struct FxCommonBufferRegion {
    /* F3 has the largest native/PS1 size ratio among model packets. */
    u8 data[(0x15F90u / 20u) * (sizeof(RenderNativeTag) + 16u)];
} FxCommonBufferRegion;

typedef struct FxCommonGradientQuad {
    RenderNativeTag tag;
    u8 color0[4];
    s16 x0;
    s16 y0;
    u8 color1[4];
    s16 x1;
    s16 y1;
    u8 color2[4];
    s16 x2;
    s16 y2;
    u8 color3[4];
    s16 x3;
    s16 y3;
} FxCommonGradientQuad;

typedef struct FxCommonDrawModePacket {
    RenderNativeTag tag;
    uintptr_t command;
} FxCommonDrawModePacket;

PE1_STATIC_ASSERT(sizeof(FxCommonGradientQuad) == sizeof(RenderNativeTag)+32, fx_common_gradient_quad_size);
PE1_STATIC_ASSERT(sizeof(FxCommonDrawModePacket) == sizeof(RenderNativeTag)+sizeof(uintptr_t), fx_common_draw_mode_packet_size);

typedef struct FxCommonShortVec3 {
    s16 x;
    s16 y;
    s16 z;
} FxCommonShortVec3;

typedef struct FxCommonOffsetTable {
    s32 offsets[1];
} FxCommonOffsetTable;

typedef struct FxCommonOffsetEntry {
    u8 pad00[0x30];
    s32 value;
} FxCommonOffsetEntry;

typedef struct FxCommonOffsetByte {
    u8 value;
} FxCommonOffsetByte;

/* One record of a sampled path. The first record is the header, whose last
 * halfword holds the point count; the points follow it. */
typedef struct FxCommonPathPoint {
    s16 x, y, z;
    u16 count;                  /* 0x06: header record only */
} FxCommonPathPoint;

/* Fixed-stride motion storage; the sampler writes only the vector prefix. */
typedef struct FxCommonMotionRecord {
    FxCommonVec3 vector;
    s32 pad0C;
} FxCommonMotionRecord;
PE1_STATIC_ASSERT(sizeof(FxCommonMotionRecord) == 16, fx_motion_record_size);
PE1_STATIC_ASSERT(PE1_OFFSETOF(FxCommonMotionRecord, vector) == 0,
                  fx_motion_record_vector_offset);

typedef struct FxCommonResourceState {
    s32 active;
    union {
        s16 half;
        s32 word;
    } current;
} FxCommonResourceState;

extern s16 g_FxCommonMotionBlendFrames;
extern u8 g_FxCommonPathWrapped;
extern GteMatrix g_FxCommonMirrorYMatrix;
extern FxCommonMotionRecord g_FxCommonMotionPositions[10];
extern FxCommonMotionRecord g_FxCommonMotionDeltas[10];
extern FxCommonResourceState g_FxCommonResourceState;

extern s16 g_FxCommonRecordCursor;
extern s16 g_FxCommonIndexFreeNext[];
extern s16 g_FxCommonIndexLast;
extern u16 g_FxCommonIndexFreeHead;

#define g_Akao3dVolumeMin (g_GameState.bank_value_f6)
#define g_Akao3dVolumeMax (g_GameState.bank_value_f7)
extern u8 g_FxCommonNight;
extern s16 g_FxCommonMarkerFade;
extern FxCommonBuffer *g_FxCommonActiveBuffer;
extern FxCommonNode g_FxCommonDefaultNode;
extern RoomSpriteMatrix g_FxCommonViewMatrix;
/* Native storage for the original resource banks (largest authored bank: 100 sectors). */
#define FX_COMMON_RESOURCE_BYTES 0x40000u
extern TimRelativeTable *const g_FxCommonTimTable, *const g_FxCommonModelTable;

void Akao_Cmd_A9(int mode, int arg1);
void SetSemiTrans(void *packet, int enabled);
s16 FxCommon_DrawEffectMarkers(s16 value);
int FxCommon_SamplePath(u32 position, int radius, void *allocation,
                   FxCommonVec3 *vector, GteShortVector *angles);
void PushMatrix(void);
void PopMatrix(void);
void Akao_Cmd_A0(int handle, int index, int value);
void Akao_Cmd_A2(int handle, int index, int value);
void Draw_CloseTextboxPage(int resourceId);
void Menu_SetHighlightColor(int resourceId, int red, int green, int blue);
void Render_SetupColorTable(int resourceId, int mode, s16 *result);
void FxCommon_DrawFullScreenFade(int value);
void FxCommon_InitNodePools(void);
void FxCommon_ResetTransformNode(FxCommonTransformNode *node);
void FxCommon_ReleaseNode(FxCommonNode *node);
int FxCommon_AllocIndexLink(void);
void FxCommon_FreeIndexLink(s16 id);
void FxCommon_BeginRecordIteration(void);
FxCommonRecord *FxCommon_NextRecord(void);
int FxCommon_IsPointInView(int *point);
int FxCommon_IsShortPointInView(short *point);
void FxCommon_UnlinkNode(FxCommonNode *node);
void FxCommon_SelectBufferAllocation(FxCommonBuffer *buffer);
void FxCommon_ClearActiveOrdering(void);
typedef struct FxCommonPolyResource FxCommonPolyResource;
void FxCommon_DrawPolyResource(void *context, FxCommonPolyResource *resource);
int FxCommon_GetOffsetTableValue(FxCommonOffsetTable *table, int index);
int FxCommon_CheckBoundsWithMargin(int *point, int margin);
int FxCommon_CheckFourBoundsWithMargin(int *point, int margin);
typedef union FxCommonPolyModel FxCommonPolyModel;
void FxCommon_DrawModel(void *context, FxCommonPolyModel *model, int index);
void FxCommon_DrawModelTinted(void *context, FxCommonPolyModel *model, int index);
void FxCommon_DrawModelFlat(void *context, FxCommonPolyModel *model, int index);

#endif
