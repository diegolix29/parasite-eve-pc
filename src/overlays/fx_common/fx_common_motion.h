#ifndef FX_COMMON_MOTION_H
#define FX_COMMON_MOTION_H

#include "pe1/display_env.h"
#include "pe1/gpu_image.h"
#include "pe1/gte_types.h"
#include "pe1/psyq_gpu.h"
#include "pe1/psyq_libc.h"
#include "pe1/signed_rect.h"

#include "fx_common_setup.h"

extern FxCommonVec3 g_FxCommonMotionAccum1;
extern FxCommonVec3 g_FxCommonMotionAccum0;
extern FxCommonShortVec3 g_FxCommonMotionOrigin1;
extern FxCommonShortVec3 g_FxCommonMotionTarget1;
extern FxCommonShortVec3 g_FxCommonMotionOrigin0;
extern FxCommonShortVec3 g_FxCommonMotionTarget0;
/* Both scalar access and vector access name the same native lane state. */
extern FxCommonVec3 g_FxCommonMotionStep0,g_FxCommonMotionStep1;
extern s16 g_FxCommonMotionShift0,g_FxCommonMotionShift1;
extern s16 g_FxCommonMotionRemaining0,g_FxCommonMotionRemaining1;
#define g_FxCommonMotionStep0X (g_FxCommonMotionStep0.x)
#define g_FxCommonMotionStep0Y (g_FxCommonMotionStep0.y)
#define g_FxCommonMotionStep0Z (g_FxCommonMotionStep0.z)
#define g_FxCommonMotionAccum0X (g_FxCommonMotionAccum0.x)
#define g_FxCommonMotionAccum0Y (g_FxCommonMotionAccum0.y)
#define g_FxCommonMotionAccum0Z (g_FxCommonMotionAccum0.z)
#define g_FxCommonMotionOrigin0X (g_FxCommonMotionOrigin0.x)
#define g_FxCommonMotionTarget0X (g_FxCommonMotionTarget0.x)
#define g_FxCommonMotionStep1X (g_FxCommonMotionStep1.x)
#define g_FxCommonMotionStep1Y (g_FxCommonMotionStep1.y)
#define g_FxCommonMotionStep1Z (g_FxCommonMotionStep1.z)
#define g_FxCommonMotionAccum1X (g_FxCommonMotionAccum1.x)
#define g_FxCommonMotionAccum1Y (g_FxCommonMotionAccum1.y)
#define g_FxCommonMotionAccum1Z (g_FxCommonMotionAccum1.z)
#define g_FxCommonMotionOrigin1X (g_FxCommonMotionOrigin1.x)
#define g_FxCommonMotionTarget1X (g_FxCommonMotionTarget1.x)
extern u32 g_FieldPadPressed;
extern u32 g_FieldPadBits;
#include "pe1/akao/bgm.h"
extern s32 g_FxCommonPageChanged;
extern s32 g_FxCommonPageOpen;
extern s16 g_FxCommonExitRequested;
extern s16 g_FxCommonExitCountdown;
extern s16 g_FxCommonPageId;
extern s16 g_FxCommonScreenFade;
extern u16 g_FxCommonDemoFrame;
extern s16 g_FxCommonDrawTintedCopies;
extern s32 g_FxCommonTailPathPosition;
extern s32 g_FxCommonFlyerPathPosition;
extern s32 g_FxCommonCameraPathPosition;
extern s32 g_FxCommonDemoTicks;
extern s32 g_FxCommonDemoExiting;
extern s16 g_FxCommonDemoResource;
extern s32 g_FxCommonDemoResourceTicks;
extern u8 g_FxCommonDemoShade;
extern u8 g_FxCommonDemoFadePhase;
extern s16 g_FxCommonInputLock;
extern s16 g_FxCommonTailViewEntered;
extern s32 g_FxCommonExitVariant;

/* Double-buffered frame: packet buffer and ordering table, then the
 * DRAWENV and DISPENV handed to PutDrawEnv / PutDispEnv. */
typedef struct FxCommonFrame {
    FxCommonBuffer buffer;
    DRAWENV drawEnv;
    DISPENV dispEnv;
} FxCommonFrame;
typedef RECT FxCommonRect;
extern FxCommonFrame g_FxCommonFrames[2];
extern s32 g_ActiveDrawSlot;
extern u8 g_FxCommonResetRequested;
extern s32 g_FxCommonHsyncCount;
/* Plain views of the camera position and its look-at target. */
extern FxCommonVec3 g_FxCommonCameraPosition;
extern FxCommonVec3 g_FxCommonCameraTarget;
extern RoomSpriteMatrix g_FxCommonIdentityMatrix;
/* Transform nodes animated along the camera paths (0x801EA578..0x801EA58C). */
extern FxCommonTransformNode *g_FxCommonPathNodes[6];
/* Camera rotation angles. */
extern GteShortVector g_FxCommonCameraAngles[];
extern RoomSpriteMatrix g_FxCommonCameraMatrix;

void Akao_Cmd_C1_WithSlot(int channel, int value, int mode);
int FxCommon_InitScene(void);
void FxCommon_SetAkao3dVolumeRange(int enabled);
s32 VSync(s32 value);
void Field_HandleStateTransition(void);
void SetDispMask(s32 value);
void Boot_Shutdown(void);
void FxCommon_StepEffects(void);
void FxCommon_UpdateCameraMatrices(void);
void FxCommon_UpdateFrustumPlanes(FxCommonVec3 *eye);

/* View frustum side planes rebuilt by FxCommon_UpdateFrustumPlanes: the four rotated
 * frustum corners, each plane normal, its distance, the opposite corner
 * distance and the normal length. */
extern GteShortVector g_FxCommonFrustumCorner0, g_FxCommonFrustumCorner1, g_FxCommonFrustumCorner2, g_FxCommonFrustumCorner3;
extern GteVector g_FxCommonFrustumNormal0, g_FxCommonFrustumNormal1, g_FxCommonFrustumNormal2, g_FxCommonFrustumNormal3;
extern s32 g_FxCommonFrustumDistance0, g_FxCommonFrustumDistance1, g_FxCommonFrustumDistance2, g_FxCommonFrustumDistance3;
extern s32 g_FxCommonFrustumSide0, g_FxCommonFrustumSide1, g_FxCommonFrustumSide2, g_FxCommonFrustumSide3;
extern s32 g_FxCommonFrustumLength0, g_FxCommonFrustumLength1, g_FxCommonFrustumLength2, g_FxCommonFrustumLength3;
GteVector *OuterProduct0(GteVector *a, GteVector *b, GteVector *out);
s16 FxCommon_StepFadeIntensity(s16 value);
void FxCommon_DrawNightShadeAndMarkers(void);
void FxCommon_DrawSceneNodes(void);

/* Per-frame scene draw (FxCommon_DrawSceneNodes): depth bias for the next node,
 * echo copies of the camera path nodes and the flicker colour. */
extern s32 g_FxCommonDepthBias;
/* USA 801EA588 is the final two slots of the array at 801EA578. */
#define g_FxCommonPathTailNodes (g_FxCommonPathNodes + 4)
extern s16 D_8019C0D0[2];
extern u8 g_FxCommonFlatColor[4];
extern FxCommonTransformNode *g_FxCommonEchoNodes[4];
void FxCommon_DrawTransformNodeCulled(FxCommonTransformNode *node, FxCommonBuffer *context, u8 pass,
                   u8 force, u8 mirrored);
void FxCommon_DrawLodNode(FxCommonLodNode *node, FxCommonBuffer *context, u8 force, u8 mode);
void FxCommon_DrawTransformNode(FxCommonTransformNode *node, void *context);
void FxCommon_StepPathNodes(void);
int FxCommon_SendTableCommand3d(int id, s32 *state);
void FxCommon_Apply3dPan(int handle, volatile s32 *state);
void Menu_DrawTextboxEntries(void);
void FxCommon_StepMotionBlend(void);
void ResetGraph(s32 value);
void FxCommon_EndSequence(void);
void FxCommon_BuildLookAtMatrix(GteMatrix *out, GteShortVector *eye,
                   GteShortVector *target, GteVector *up);
void Akao_Cmd_F0(void);
void Akao_Cmd_F1(void);
int Render_ConsumeFontShuffleSeed(void);
s16 FxCommon_SwitchResource(s16 resourceId, u8 action);
void FxCommon_SetMotionEndpoints(FxCommonShortVec3 *a, FxCommonShortVec3 *b, int bi, int ai);
int FxCommon_StepCameraPath(void);
int FxCommon_InitCameraPath(int index,int unused1,int unused2,int source);

PE1_STATIC_ASSERT(PE1_OFFSETOF(FxCommonFrame, drawEnv) == sizeof(FxCommonBuffer), fx_common_frame_draw_offset);
PE1_STATIC_ASSERT(sizeof(FxCommonVec3) == 12, fx_motion_position_size);
PE1_STATIC_ASSERT(sizeof(FxCommonShortVec3) == 6, fx_motion_endpoint_size);

#endif
