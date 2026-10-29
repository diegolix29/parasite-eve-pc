#ifndef FX_COMMON_SETUP_H
#define FX_COMMON_SETUP_H

#include "pe1/boot_services.h"
#include "pe1/field_bg_load.h"
#include "pe1/game_state.h"
#include "pe1/psyq_gpu.h"
#include "pe1/script_queue.h"

#include "fx_common.h"

extern s32 g_FxCommonConfirmExit;
extern s32 g_FxCommonConfirmExitVariant;
#include "pe1/game_state.h"
int Save_GetMetadataWindowIndex(void);
void Menu_SetEquipSlotIndex(int enabled);
void FxCommon_InitDisplay(void);
int FxCommon_LoadEffectSectors(void);
extern s32 g_FxCommonStartPreset;
extern s32 g_FxCommonAutoPlay;
extern s32 g_FxCommonAutoVariant;
extern s32 g_FxCommonAltFogLayer;
extern s8 D_8019C00D;
extern u8 g_FxCommonStartCameraPath;
extern s32 g_FxCommonRouteNodeCount;
extern s16 D_8019C02E;
extern s16 g_FxCommonViewMode;
extern u8 g_FxCommonCameraPathRemaining;
extern u8 g_FxCommonCameraPathStep;
extern u8 g_FxCommonCameraPathIndex;
extern u8 g_FxCommonPlaying;
extern u8 g_FxCommonPlayAdvance;
extern u8 g_FxCommonReplayCamera;
extern s32 g_FxCommonPlayTicks;
extern s16 g_FxCommonRouteFarDepths[10];
extern s16 g_FxCommonRouteNearDepths[10];
extern GteMatrix g_FxCommonColorMatrix;
extern GteMatrix g_FxCommonLightMatrix;
/* Nine authored route groups contain 66 points, with two nodes per point. */
extern FxCommonNode *g_FxCommonRouteNodes[132];
extern s32 g_FxCommonPlayFrame;
extern s16 g_FxCommonRoomVariant;
void Render_SetupFogLayer(void *);
void SetFogNearFar(long,long,long);
void SetBackColor(long,long,long);
void SetFarColor(long,long,long);
void FxCommon_InitNodePools(void);
FxCommonNode *FxCommon_CreateBareNode(FxCommonNode *,void *);
FxCommonNode *FxCommon_CreateTypedNode(FxCommonNode *,s16,void *);
FxCommonNode *FxCommon_CreateConfiguredNode(FxCommonNode *,s16,s16,s16,s32,s32,void *);
int FxCommon_SelectRoomVariant(void);
/* The two camera lanes advance for 1 << durationShift frames. */
void FxCommon_StartCameraMove(int setupIndex, int targetDurationShift,
                   int eyeDurationShift, u32 pathPosition);
void FxCommon_InitFadeQuads(void);

/* Double-buffered full-screen gradient overlays and their draw-mode
 * packets, built once by FxCommon_InitFadeQuads. */
extern FxCommonGradientQuad g_FxCommonSkyQuads[2];
extern FxCommonGradientQuad g_FxCommonShadeQuads[2];
extern FxCommonGradientQuad g_FxCommonFloorFadeQuads[2];
extern FxCommonGradientQuad g_FxCommonTopFadeQuads[2];
extern FxCommonDrawModePacket g_FxCommonDrawModes0[2];
extern FxCommonDrawModePacket g_FxCommonDrawModes1[2];
extern FxCommonDrawModePacket g_FxCommonDrawModes2[2];

/* Effect markers drawn by FxCommon_DrawEffectMarkers: a line from the effect position
 * to its label and two textured label halves. */
typedef struct FxCommonLinePacket {
    RenderNativeTag tag;
    u8 r, g, b, code;
    u32 xy0;
    u32 xy1;
} FxCommonLinePacket;

typedef struct FxCommonMarkerQuad {
    RenderNativeTag tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    u8 r1, g1, b1, pad13;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    u8 r2, g2, b2, pad1F;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad26;
    u8 r3, g3, b3, pad2B;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad32;
} FxCommonMarkerQuad;

PE1_STATIC_ASSERT(offsetof(FxCommonLinePacket,xy0) == sizeof(RenderNativeTag)+4, fx_common_line_xy0);
PE1_STATIC_ASSERT(sizeof(FxCommonMarkerQuad) == sizeof(RenderNativeTag)+48, fx_common_marker_quad_size);

long RotTransPers3(GteShortVector *v0, GteShortVector *v1, GteShortVector *v2,
                  s32 *sxy0, s32 *sxy1, s32 *sxy2, s32 *p, s32 *flag);

extern FxCommonNode *g_FxCommonType26Nodes[4];
/* Scene nodes by creation: type 0x51; types 0x28 to 0x32 in order; two of type 0x21;
 * type 0x54; two LOD-configured pairs (type 0x22). */
extern FxCommonNode *g_FxCommonType51Node;
extern FxCommonNode *g_FxCommonSceneNodes[11];
extern FxCommonNode *g_FxCommonType21Nodes[2];
extern FxCommonNode *g_FxCommonType54Node;
extern FxCommonNode *g_FxCommonLodNodes[2];
extern FxCommonNode *g_FxCommonPairedNodes[2];
typedef struct FxCommonRuntime {
    FxCommonNode *node25;
    u32 unknown04;
    FxCommonIndexLink links[202];
    FxCommonNode *pairedNodes[20];
    FxCommonNode pool[200];
    FxCommonNode *node0;
} FxCommonRuntime;
/* Runtime data uses native pointers; no serialized PS1 offsets apply. */
extern FxCommonRuntime g_FxCommonRuntime;
#define g_FxCommonRecords (g_FxCommonRuntime.pool)
#define g_FxCommonIndexLinks (g_FxCommonRuntime.links)

typedef struct FxCommonEffectSetupRecord {
    s16 position[3];
    s16 reserved06;
    u8 motionIndices[4];
    s32 value0c;
    s32 value10;
    s16 subtype;
    s16 type;
    s16 variant;
    s16 rotation;
    s16 parameter3c;
    s16 parameter38;
    s16 reserved20;
    u8 value22, value23, enabled, value25;
    s16 reserved26;
    s32 value28, value2c;
    s16 value30, reserved32;
} FxCommonEffectSetupRecord;
extern FxCommonEffectSetupRecord g_FxCommonEffectSetup[10];

typedef struct FxCommonPoint8 { s16 x,y,z,pad; } FxCommonPoint8;
typedef struct FxCommonPointList { u8 header[6]; s16 count; FxCommonPoint8 points[1]; } FxCommonPointList;

PE1_STATIC_ASSERT(sizeof(FxCommonEffectSetupRecord) == 52, fx_setup_record_size);
PE1_STATIC_ASSERT(PE1_OFFSETOF(FxCommonEffectSetupRecord, motionIndices) == 8,
                  fx_setup_motion_indices_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(FxCommonEffectSetupRecord, subtype) == 0x14, fx_setup_subtype);
PE1_STATIC_ASSERT(sizeof(FxCommonPoint8) == 8, fx_point_size);
PE1_STATIC_ASSERT(PE1_OFFSETOF(FxCommonPointList, points) == 8, fx_point_list_header);

#endif
