/*
 * The effect sequence: a configurable sequence of sprite effects (four
 * draw variants with their setups, paired sprites, sparkles, a glow and a
 * drifting effect) that the room's module drives.
 *
 * room_m049, m050, m090, m344 and m374 link the same twenty-five
 * functions in this order, at the same addresses, from the module's own
 * registration and start methods (which act whatever the field engine's
 * status, and close through the module's no-op) to the drifting effect
 * update,
 * with five seeds as their only read-only data (0x40 bytes after the room
 * header). This unit is that object, compiled into each of them. The
 * sprite records and packets it fills, and the module's lists
 * (pe1/room_module.h), are room data at the same addresses in all five
 * rooms.
 */

#include "common.h"
#include <string.h>
#include "pe1/field_model_draw.h"
#include "pe1/field_script_context.h"
#include "pe1/gte_types.h"
#include "pe1/psyq_gpu.h"
#include "pe1/psyq_libc.h"
#include "pe1/room_effect_sequence.h"
#include "pe1/room_fx.h"
#include "pe1/room_module.h"

#include "room_lib.h"

/* The records the sequence's handlers read and write. */
typedef GteMatrix RoomOverlay024Matrix;

typedef GteVector RoomOverlay024Vec4;

typedef RoomFxSeed8 RoomOverlay024MatrixSeed8;

typedef GteMatrix RoomOverlay024ViewMatrix;

typedef struct RoomOverlay024ViewPosition {
    char transform[0x14];
    s32 base_x;
    s32 base_y;
    s32 base_z;
} RoomOverlay024ViewPosition;

typedef union RoomOverlay024ViewTail {
    RoomOverlay024ViewMatrix matrix;
    RoomOverlay024ViewPosition position;
} RoomOverlay024ViewTail;

typedef struct RoomOverlay024View {
    char pad0[0x1A0];
    RoomOverlay024ViewTail tail;
} RoomOverlay024View;

typedef union RoomOverlay024VectorSeed {
    s32 words[4];
    GteVector vector;
} RoomOverlay024VectorSeed;

typedef struct RoomOverlay024Effect {
    s16 x;
    s16 y;
    s16 z;
    char pad6[0x2];
    s16 half8;
    s16 velocity_y;
    s16 halfC;
    char padE[0x2];
    s16 timer;
    s16 duration;
    s16 half14;
} RoomOverlay024Effect;

typedef struct RoomOverlay024Transform {
    s32 pad0[5];
    s32 x;
    s32 y;
    s32 z;
} RoomOverlay024Transform;

typedef struct RoomOverlay024Root {
    char pad0[PE1_OFFSETOF(FieldActor, render_object) + PE1_OFFSETOF(RenderObjectEntity, matrices)];
    void *view;
} RoomOverlay024Root;

typedef struct RoomOverlay024VariantControl {
    char pad0[2];
    s16 frame;
} RoomOverlay024VariantControl;

typedef struct RoomOverlay024Variant38State {
    s16 final_x;
    s16 final_y;
    s16 final_z;
    char pad6[0xA];
    s16 final_alpha;
    s16 final_scale;
    char pad14[4];
    s16 sparkle_x;
    s16 sparkle_y;
    s16 sparkle_z;
    char pad1E[2];
    s16 sparkle_alpha;
    s16 sparkle_timer;
    s16 resource_selector;
    s16 active_flag;
    s16 drift_value;
    s16 fade_alpha;
    u8 phase;
    char pad2D;
    s16 transform_index;
    s32 field30;
} RoomOverlay024Variant38State;

/* Shared setup-time view used by Variant 38 and Variant 290 initializers. */
typedef struct RoomOverlay024Variant38SetupState {
    char pad0[0x10];
    s16 random_mod;
    char pad12[2];
    s16 sparkle_timer;
    char pad16[0xC];
    s16 field22;
    s16 resource_selector;
    s16 active_flag;
    s16 height;
    s16 width;
    s16 field2C;
    s16 transform_index;
    s32 field30;
} RoomOverlay024Variant38SetupState;

typedef struct RoomOverlay024Variant290State {
    s16 x;
    s16 y;
    s16 z;
    char pad6[2];
    s16 velocity_x;
    s16 velocity_y;
    s16 velocity_z;
    char padE[2];
    s16 core_alpha;
    s16 core_timer;
    s16 spawn_timer;
    char pad16[2];
    s16 sparkle_x;
    s16 sparkle_y;
    s16 sparkle_z;
    char pad1E[2];
    s16 sparkle_alpha;
    s16 sparkle_timer;
    s16 life_timer;
    s16 active_flag;
    s16 drift_value;
    s16 fade_alpha;
    u8 phase;
    char pad2D;
    s16 transform_index;
    s32 field30;
} RoomOverlay024Variant290State;

typedef RoomOverlay024Variant38SetupState RoomOverlay024Variant290SetupState;

/* Record view written by the room-specific Variant D setup functions. */
typedef struct RoomOverlay024VariantDSetupRecord {
    char pad0[0x10];
    s16 random_mod;
    char pad12[2];
    s16 half14;
    char pad16[0xC];
    s16 half22;
    s16 resource2;
    s16 half26;
    s16 resource1_again;
    s16 half2A;
    s16 half2C;
    s16 half2E;
    s32 word30;
    s16 resource1;
} RoomOverlay024VariantDSetupRecord;

typedef struct RoomOverlay024VariantDState {
    s16 x;
    s16 y;
    s16 z;
    char pad6[2];
    s16 velocity_x;
    s16 velocity_y;
    s16 velocity_z;
    char padE[2];
    s16 core_alpha;
    s16 core_timer;
    s16 spawn_timer;
    char pad16[2];
    s16 sparkle_x;
    s16 sparkle_y;
    s16 sparkle_z;
    char pad1E[2];
    s16 sparkle_alpha;
    s16 sparkle_timer;
    s16 life_timer;
    s16 active_flag;
    s16 drift_value;
    s16 fade_alpha;
    u8 phase;
    char pad2D;
    s16 transform_index;
    s32 field30;
    s16 vertical_offset;
} RoomOverlay024VariantDState;

PE1_STATIC_ASSERT(sizeof(RoomOverlay024ViewTail) == 0x20,
                  overlay024_view_tail_size);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024View, tail) == 0x1A0,
                  overlay024_view_tail_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Root, view) == PE1_OFFSETOF(FieldActor, render_object) + PE1_OFFSETOF(RenderObjectEntity, matrices),
                  overlay024_root_view_offset);
PE1_STATIC_ASSERT(sizeof(RoomOverlay024Variant38State) == 0x34,
                  overlay024_variant38_state_size);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Variant38SetupState,
                               random_mod) == 0x10,
                  overlay024_variant38_setup_random_mod_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Variant38SetupState,
                               sparkle_timer) == 0x14,
                  overlay024_variant38_setup_sparkle_timer_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Variant38SetupState, field22) == 0x22,
                  overlay024_variant38_setup_field22_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Variant38SetupState,
                               resource_selector) == 0x24,
                  overlay024_variant38_setup_resource_selector_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Variant38SetupState, active_flag) == 0x26,
                  overlay024_variant38_setup_active_flag_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Variant38SetupState, height) == 0x28,
                  overlay024_variant38_setup_height_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Variant38SetupState, width) == 0x2A,
                  overlay024_variant38_setup_width_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Variant38SetupState, field2C) == 0x2C,
                  overlay024_variant38_setup_field2c_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Variant38SetupState,
                               transform_index) == 0x2E,
                  overlay024_variant38_setup_transform_index_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Variant38SetupState, field30) == 0x30,
                  overlay024_variant38_setup_field30_offset);
PE1_STATIC_ASSERT(sizeof(RoomOverlay024Variant38SetupState) == 0x34,
                  overlay024_variant38_setup_state_size);
PE1_STATIC_ASSERT(sizeof(RoomOverlay024Variant290State) == 0x34,
                  overlay024_variant290_state_size);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Variant290SetupState,
                               random_mod) == 0x10,
                  overlay024_variant290_setup_random_mod_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Variant290SetupState,
                               sparkle_timer) == 0x14,
                  overlay024_variant290_setup_sparkle_timer_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Variant290SetupState, field22) == 0x22,
                  overlay024_variant290_setup_field22_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Variant290SetupState,
                               resource_selector) == 0x24,
                  overlay024_variant290_setup_resource_selector_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Variant290SetupState, active_flag) == 0x26,
                  overlay024_variant290_setup_active_flag_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Variant290SetupState, height) == 0x28,
                  overlay024_variant290_setup_height_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Variant290SetupState, width) == 0x2A,
                  overlay024_variant290_setup_width_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Variant290SetupState, field2C) == 0x2C,
                  overlay024_variant290_setup_field2c_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Variant290SetupState,
                               transform_index) == 0x2E,
                  overlay024_variant290_setup_transform_index_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024Variant290SetupState, field30) == 0x30,
                  overlay024_variant290_setup_field30_offset);
PE1_STATIC_ASSERT(sizeof(RoomOverlay024Variant290SetupState) == 0x34,
                  overlay024_variant290_setup_state_size);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024VariantDSetupRecord,
                               random_mod) == 0x10,
                  overlay024_variantd_setup_random_mod_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024VariantDSetupRecord,
                               half14) == 0x14,
                  overlay024_variantd_setup_half14_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024VariantDSetupRecord,
                               half22) == 0x22,
                  overlay024_variantd_setup_half22_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024VariantDSetupRecord,
                               resource2) == 0x24,
                  overlay024_variantd_setup_resource2_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024VariantDSetupRecord,
                               half26) == 0x26,
                  overlay024_variantd_setup_half26_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024VariantDSetupRecord,
                               resource1_again) == 0x28,
                  overlay024_variantd_setup_resource1_again_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024VariantDSetupRecord,
                               half2A) == 0x2A,
                  overlay024_variantd_setup_half2a_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024VariantDSetupRecord,
                               half2C) == 0x2C,
                  overlay024_variantd_setup_half2c_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024VariantDSetupRecord,
                               half2E) == 0x2E,
                  overlay024_variantd_setup_half2e_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024VariantDSetupRecord,
                               word30) == 0x30,
                  overlay024_variantd_setup_word30_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomOverlay024VariantDSetupRecord,
                               resource1) == 0x34,
                  overlay024_variantd_setup_resource1_offset);
PE1_STATIC_ASSERT(sizeof(RoomOverlay024VariantDSetupRecord) == 0x38,
                  overlay024_variantd_setup_record_size);
PE1_STATIC_ASSERT(sizeof(RoomOverlay024VariantDState) == 0x38,
                  overlay024_variantd_state_size);

/* Seeds and scales copied into the draw passes' stack frames. */
FieldFxHandler g_RoomEffectSequenceDrawList[8], g_RoomEffectSequenceUpdateList[8], g_RoomEffectSequenceInitList[8];
u16 g_RoomEffectSequenceSizes[8];

static const RoomOverlay024MatrixSeed8 s_EffectBaseSeed = {{0x00, 0x04, 0, 0, 0, 0, 0, 0}};
static const RoomOverlay024Vec4 s_EffectWideScale = {0x96, 0x12C, 0x12C, 0};
static const RoomOverlay024Vec4 s_EffectNarrowScale = {0x46, 0x78, 0x46, 0};
static const RoomOverlay024MatrixSeed8 s_EffectProjectSeed = {{0x00, 0x00, 0x10, 0xFF, 0, 0, 0, 0}};
static const RoomOverlay024VectorSeed s_EffectVectorSeed = {{0x1388, 0xC18, 0x1388, 0}};

/* The original seven class methods use the native PM callback ABI. */
int RoomEffect_EffectSequenceRegister(PmSlotHeader *o) {
    FieldFx_Register((FieldFxObject *)o, g_RoomEffectSequenceDrawList);
    return 0;
}
int RoomEffect_EffectSequenceStart(PmSlotHeader *o) {
    int status=FieldFx_UpdateSlots((FieldFxObject *)o, g_RoomEffectSequenceUpdateList);
    status |= FieldFx_RunScript((FieldFxObject *)o, g_RoomEffectSequenceInitList, g_RoomEffectSequenceSizes);
    if(status == -1) RoomEffect_EffectSequenceNop5(o);
    return 0;
}
int RoomEffect_EffectSequenceNop5(PmSlotHeader *o) { return 0; }
void RoomEffect_EffectSequenceNop6(void) {}

int RoomEffect_InitEffectSequenceSprites(void *object_, void *slot_, void *data_) {
    RoomEnt *ent = object_;
    void *unused = slot_;
    RoomLibFxMatrixState *state = data_;
    u8 offsetValue;
    RoomLink *link;
    RenderMatrix *matrix;
    int depth;
    u8 c80b;
    int c40;
    int value;
    u8 clut;
    RoomEffectSequenceSprite *primary;

    FieldFx_SetScriptContext(state);
    g_EffectSequenceModel = Asset_FindTable08ByU32Key(g_LoadedSceneAssetBlock[0], 0x498704);
    link = ent->link;
    state->link = link;
    matrix = link->p238;
    memcpy(&state->matrix, matrix, sizeof(state->matrix));
    value = Asset_SearchByKeyType(0xE);
    state->asset.index = value;

    g_EffectSequenceGlowSprite.cell = 0;
    g_EffectSequenceGlowSprite.clut = 0;
    offsetValue = 2;
    g_EffectSequenceGlowSprite.offset = 1;
    if (*FieldFx_VarPtr(0) == 2 || *FieldFx_VarPtr(0) == 5) {
        g_EffectSequenceGlowSprite.offset = -100;
        depth = 0x80;
    } else {
        depth = 0x80;
    }
    c80b = 0x80;
    c40 = 0x40;
    value = 0x20;
    g_EffectSequenceGlowSpriteSecondary.clut = 3;
    g_EffectSequenceGlowSpriteSecondary.offset = -100;
    g_EffectSequenceFinalGlowSprite.cell = 8;
    clut = 2;
    primary = &g_EffectSequenceGlowSprite;
    primary->g = c40;
    g_EffectSequenceGlowSpriteSecondary.g = c40;
    c40 = offsetValue;
    g_EffectSequenceFinalGlowSprite.clut = clut;
    g_EffectSequenceFinalGlowSprite.r = 0x90;
    g_EffectSequenceFinalGlowSprite.g = 0x90;
    g_EffectSequenceFinalGlowSprite.b = 0x90;
    g_EffectSequenceGlowSprite.depth = depth;
    g_EffectSequenceGlowSprite.flip = 0;
    g_EffectSequenceGlowSprite.r = c80b;
    g_EffectSequenceGlowSprite.b = value;
    g_EffectSequenceGlowSpriteSecondary.cell = c80b;
    g_EffectSequenceGlowSpriteSecondary.depth = depth;
    g_EffectSequenceGlowSpriteSecondary.flip = 0;
    g_EffectSequenceGlowSpriteSecondary.r = c80b;
    g_EffectSequenceGlowSpriteSecondary.b = value;
    g_EffectSequenceFinalGlowSprite.offset = c40;
    g_EffectSequenceFinalGlowSprite.depth = depth;
    g_EffectSequenceFinalGlowSprite.flip = 0;
    g_EffectSequenceSparkleSprite.cell = 0;
    g_EffectSequenceSparkleSprite.clut = 0;
    g_EffectSequenceSparkleSprite.offset = c40;
    g_EffectSequenceSparkleSprite.flip = 0;
    g_EffectSequenceSparkleSprite.depth = depth;
    g_EffectSequenceSparkleSprite.r = c80b;
    g_EffectSequenceSparkleSprite.g = value;
    g_EffectSequenceSparkleSprite.b = 0x10;
    ent->pad1[1] = *FieldFx_VarPtr(0);

    return 0;
}

int RoomEffect_EffectSequenceNopA(void *object_, void *slot_, void *data_) {


    return 0;
}

#define ROOMLIB_EFFECT_CALL(command) \
    FieldFx_StartSlot((void *)owner, (command), g_RoomEffectSequenceSizes, \
                      g_RoomEffectSequenceInitList)
#define ROOMLIB_EFFECT_SET(index, value, command) \
    *FieldFx_RegPtr(index) = (value); \
    ROOMLIB_EFFECT_CALL(command)

int RoomEffect_ConfigureEffectSequence(void *object_, void *slot_, void *data_) {
    intptr_t owner=(intptr_t)object_;
    RoomFxControl *signal = slot_;
    short counter;
    int mode3;
    int mode4;

    if (*FieldFx_VarPtr(0) == 0) {
        if (signal->frame == 0) {
            *FieldFx_RegPtr(1) = 0;
            ROOMLIB_EFFECT_SET(2, -1, 1);
        }
        counter = signal->frame;
        if (counter == 10) {
            *FieldFx_RegPtr(1) = counter;
            ROOMLIB_EFFECT_SET(2, -1, 1);
        }
        if (signal->frame == 20) {
            *FieldFx_RegPtr(1) = 8;
            ROOMLIB_EFFECT_SET(2, -1, 1);
        }
        if (signal->frame == 30) {
            *FieldFx_RegPtr(1) = 15;
            ROOMLIB_EFFECT_SET(2, -1, 1);
        }
        if (signal->frame == 40) {
            *FieldFx_RegPtr(1) = 4;
            ROOMLIB_EFFECT_SET(2, -1, 1);
        }
        if (signal->frame == 50) {
            *FieldFx_RegPtr(1) = 18;
            ROOMLIB_EFFECT_SET(2, -1, 1);
        }
        if (signal->frame == 60) {
            *FieldFx_RegPtr(1) = 2;
            ROOMLIB_EFFECT_SET(2, -1, 1);
        }
        if (signal->frame == 70) {
            *FieldFx_RegPtr(1) = 9;
            ROOMLIB_EFFECT_SET(2, -1, 1);
        }
        if (signal->frame == 80) {
            *FieldFx_RegPtr(1) = 3;
            ROOMLIB_EFFECT_SET(2, -1, 1);
        }
        if (signal->frame == 90) {
            *FieldFx_RegPtr(1) = 19;
            ROOMLIB_EFFECT_SET(2, -1, 1);
        }
        if (signal->frame == 100) {
            *FieldFx_RegPtr(1) = 20;
            ROOMLIB_EFFECT_SET(2, -1, 1);
        }
        if (signal->frame == 110) {
            *FieldFx_RegPtr(1) = 22;
            ROOMLIB_EFFECT_SET(2, -1, 1);
        }
        if (signal->frame == 120) {
            *FieldFx_RegPtr(1) = 12;
            ROOMLIB_EFFECT_SET(2, -1, 1);
        }
        if (signal->frame == 130) {
            *FieldFx_RegPtr(1) = 17;
            ROOMLIB_EFFECT_SET(2, -1, 1);
        }
    }

    if (*FieldFx_VarPtr(0) == 1) {
        if (signal->frame == 0) {
            *FieldFx_RegPtr(1) = 5;
            ROOMLIB_EFFECT_SET(2, 80, 1);
        }
        if (signal->frame == 3) {
            *FieldFx_RegPtr(1) = 6;
            ROOMLIB_EFFECT_SET(2, 80, 1);
        }
        if (signal->frame == 6) {
            *FieldFx_RegPtr(1) = 5;
            ROOMLIB_EFFECT_SET(2, 40, 1);
        }
        if (signal->frame == 9) {
            *FieldFx_RegPtr(1) = 6;
            ROOMLIB_EFFECT_SET(2, 40, 1);
        }
        if (signal->frame == 12) {
            *FieldFx_RegPtr(1) = 5;
            ROOMLIB_EFFECT_SET(2, 20, 1);
        }
        if (signal->frame == 15) {
            *FieldFx_RegPtr(1) = 6;
            ROOMLIB_EFFECT_SET(2, 20, 1);
        }
    }

    if (*FieldFx_VarPtr(0) == 2 && signal->frame == 0) {
        ROOMLIB_EFFECT_CALL(6);
        ROOMLIB_EFFECT_CALL(6);
        ROOMLIB_EFFECT_CALL(6);
        ROOMLIB_EFFECT_CALL(6);
        ROOMLIB_EFFECT_CALL(6);
        ROOMLIB_EFFECT_CALL(6);
        ROOMLIB_EFFECT_CALL(5);
        ROOMLIB_EFFECT_SET(1, 800, 4);
        ROOMLIB_EFFECT_SET(1, 700, 4);
        ROOMLIB_EFFECT_SET(1, 600, 4);
        ROOMLIB_EFFECT_SET(1, 500, 4);
        ROOMLIB_EFFECT_SET(1, 300, 4);
        ROOMLIB_EFFECT_SET(1, 300, 4);
        ROOMLIB_EFFECT_SET(1, 200, 4);
        ROOMLIB_EFFECT_SET(1, 100, 4);
    }

    if (*FieldFx_VarPtr(0) == 5 && signal->frame == 0) {
        ROOMLIB_EFFECT_CALL(6);
        ROOMLIB_EFFECT_CALL(6);
        ROOMLIB_EFFECT_CALL(6);
        ROOMLIB_EFFECT_CALL(6);
        ROOMLIB_EFFECT_CALL(6);
        ROOMLIB_EFFECT_CALL(6);
        ROOMLIB_EFFECT_CALL(5);
        ROOMLIB_EFFECT_SET(1, 1000, 4);
        ROOMLIB_EFFECT_SET(1, 800, 4);
        ROOMLIB_EFFECT_SET(1, 600, 4);
        ROOMLIB_EFFECT_SET(1, 500, 4);
        ROOMLIB_EFFECT_SET(1, 400, 4);
        ROOMLIB_EFFECT_SET(1, 300, 4);
        ROOMLIB_EFFECT_SET(1, 200, 4);
        ROOMLIB_EFFECT_SET(1, 100, 4);
    }

    mode3 = *FieldFx_VarPtr(0);
    if (mode3 == 3 && signal->frame == 0) {
        *FieldFx_RegPtr(2) = -1;
        ROOMLIB_EFFECT_SET(1, 1, 3);
        ROOMLIB_EFFECT_SET(1, mode3, 3);
        ROOMLIB_EFFECT_SET(1, 5, 3);
        ROOMLIB_EFFECT_SET(1, 7, 3);
        ROOMLIB_EFFECT_SET(1, 9, 3);
        ROOMLIB_EFFECT_SET(1, 11, 3);
        ROOMLIB_EFFECT_SET(1, 13, 3);
        ROOMLIB_EFFECT_SET(1, 15, 3);
        ROOMLIB_EFFECT_SET(1, 17, 3);
        ROOMLIB_EFFECT_SET(1, 19, 3);
        ROOMLIB_EFFECT_SET(1, 21, 3);
        ROOMLIB_EFFECT_SET(1, 0, 3);
        ROOMLIB_EFFECT_SET(1, 2, 3);
        ROOMLIB_EFFECT_SET(1, 4, 3);
        ROOMLIB_EFFECT_SET(1, 6, 3);
        ROOMLIB_EFFECT_SET(1, 8, 3);
    }

    mode4 = *FieldFx_VarPtr(0);
    if (mode4 == 4 && signal->frame == 0) {
        *FieldFx_RegPtr(2) = -1;
        ROOMLIB_EFFECT_SET(1, 2, 2);
        ROOMLIB_EFFECT_SET(1, mode4, 2);
        ROOMLIB_EFFECT_SET(1, 6, 2);
        ROOMLIB_EFFECT_SET(1, 8, 2);
        ROOMLIB_EFFECT_SET(1, 10, 2);
        ROOMLIB_EFFECT_SET(1, 12, 2);
        ROOMLIB_EFFECT_SET(1, 14, 2);
        ROOMLIB_EFFECT_SET(1, 16, 2);
        ROOMLIB_EFFECT_SET(1, 17, 2);
        ROOMLIB_EFFECT_SET(1, 18, 2);
        ROOMLIB_EFFECT_SET(1, 20, 2);
        ROOMLIB_EFFECT_SET(1, 22, 2);
        ROOMLIB_EFFECT_SET(1, mode4, 2);
        ROOMLIB_EFFECT_SET(1, 6, 2);
        ROOMLIB_EFFECT_SET(1, 8, 2);
        ROOMLIB_EFFECT_SET(1, 10, 2);
        ROOMLIB_EFFECT_SET(1, 12, 2);
        ROOMLIB_EFFECT_SET(1, 14, 2);
        ROOMLIB_EFFECT_SET(1, 16, 2);
        ROOMLIB_EFFECT_SET(1, 17, 2);
        ROOMLIB_EFFECT_SET(1, 18, 2);
        ROOMLIB_EFFECT_SET(1, 20, 2);
        ROOMLIB_EFFECT_SET(1, 21, 2);
    }

    if (FieldFx_IsLastSlotHalted() == 1) {
        signal->state = 2;
    }

    return 0;
}

s32 *FieldFx_RegPtr(s32 index);

int RoomEffect_InitVariant38(void *object_, void *slot_, void *data_) {
    void *arg0 = object_;
    void *arg1 = slot_;
    RoomOverlay024Variant38SetupState *state = data_;
    state->transform_index = *FieldFx_RegPtr(1);
    state->resource_selector = *FieldFx_RegPtr(2);
    state->active_flag = 1;

    if (state->resource_selector == -1) {
        state->width = 0;
        state->height = 0;
    } else {
        state->width = 0x80;
        state->height = 0x38;
    }

    state->field2C = 0;
    state->field30 = 0;
    state->field22 = 0;

    return 0;
}

int RoomEffect_DrawVariant38(void *object_, void *slot_, void *data_) {
    void *arg0 = object_;
    RoomOverlay024VariantControl *control = slot_;
    RoomOverlay024Variant38State *state = data_;
    RoomOverlay024Root **owner_ptr;
    RoomOverlay024Root *owner;
    RoomOverlay024Transform *transform;
    RoomOverlay024Matrix matrix;
    RoomOverlay024MatrixSeed8 base_seed;
    RoomOverlay024Vec4 scratch_scale;
    RoomOverlay024Vec4 draw_scale;
    RoomOverlay024Vec4 second_scale;
    RoomOverlay024Vec4 third_scale;
    s32 draw_zero;

    owner_ptr = FieldFx_GetScriptContext();
    owner = *owner_ptr;
    transform = &((RoomOverlay024Transform *)owner->view)[state->transform_index];
    base_seed = s_EffectBaseSeed;

    FieldFx_SetSpriteTexturePreset(((RoomLibFxMatrixState *)owner_ptr)->asset.page);
    FieldFx_SetSpriteCellSize(0x20, 0x20);
    FieldFx_SetSpriteColorMode(0x10);
    FieldFx_SetSpriteBlendMode(2);

    if (state->active_flag == 0) {
        return 0;
    }

    matrix.m[2][2] = 0x1000;
    matrix.m[1][1] = 0x1000;
    matrix.m[0][0] = 0x1000;
    matrix.t[2] = 0;
    matrix.t[1] = 0;
    matrix.t[0] = 0;
    matrix.m[2][1] = 0;
    matrix.m[2][0] = 0;
    matrix.m[1][2] = 0;
    matrix.m[1][0] = 0;
    matrix.m[0][2] = 0;
    matrix.m[0][1] = 0;

    memset(&draw_scale, 0, 0x10);
    draw_scale.x = state->drift_value;
    draw_scale.y = state->drift_value;
    draw_scale.z = state->drift_value;
    scratch_scale = draw_scale;
    ScaleMatrix(&matrix, &scratch_scale);

    g_EffectSequenceGlowSprite.depth = state->fade_alpha;

    matrix.t[0] = transform->x;
    matrix.t[1] = transform->y - 0x64;
    matrix.t[2] = transform->z;

    if (state->resource_selector == -1) {
        FieldFx_InterpolateColorRamp(g_EffectSequenceLimeGlowRamp, state->field30, (u8 *)&g_EffectSequenceGlowSprite);
    } else {
        FieldFx_InterpolateColorRamp(g_EffectSequenceAmberGlowRamp, state->field30, (u8 *)&g_EffectSequenceGlowSprite);
    }
    g_EffectSequenceGlowSprite.cell = state->phase * 2;
    FieldFx_DrawGlowSprite((FieldGlowSprite *)&g_EffectSequenceGlowSprite, (GteMatrix *)&matrix, 1);
    RotMatrix(&base_seed.vector, &matrix);

    memset(&second_scale, 0, 0x10);
    second_scale.x = state->drift_value;
    second_scale.y = state->drift_value;
    second_scale.z = state->drift_value;
    draw_scale = second_scale;
    ScaleMatrix(&matrix, &draw_scale);

    g_EffectSequenceGlowSprite.depth = state->fade_alpha >> 2;
    matrix.t[0] = transform->x;
    matrix.t[1] = g_RoomFloorY->y;
    draw_zero = 0;
    matrix.t[2] = transform->z;
    g_EffectSequenceGlowSprite.cell = 8;
    FieldFx_DrawGlowSprite((FieldGlowSprite *)&g_EffectSequenceGlowSprite, (GteMatrix *)&matrix, draw_zero);

    if (control->frame >= 0x5B && state->sparkle_timer > 0) {
        matrix.m[2][2] = 0x1000;
        matrix.m[1][1] = 0x1000;
        matrix.m[0][0] = 0x1000;
        matrix.t[2] = 0;
        matrix.t[1] = 0;
        matrix.t[0] = 0;
        matrix.m[2][1] = 0;
        matrix.m[2][0] = 0;
        matrix.m[1][2] = 0;
        matrix.m[1][0] = 0;
        matrix.m[0][2] = 0;
        matrix.m[0][1] = 0;

        second_scale = s_EffectWideScale;
        ScaleMatrix(&matrix, &second_scale);

        matrix.t[0] = state->sparkle_x;
        matrix.t[1] = state->sparkle_y;
        matrix.t[2] = state->sparkle_z;
        g_EffectSequenceSparkleSprite.depth = state->sparkle_alpha;
        FieldFx_DrawGlowSprite((FieldGlowSprite *)&g_EffectSequenceSparkleSprite, (GteMatrix *)&matrix, 0);
    }

    if (control->frame < 0x1F) {
        return 0;
    }

    FieldFx_SetSpriteBlendMode(3);

    matrix.m[2][2] = 0x1000;
    matrix.m[1][1] = 0x1000;
    matrix.m[0][0] = 0x1000;
    matrix.t[2] = 0;
    matrix.t[1] = 0;
    matrix.t[0] = 0;
    matrix.m[2][1] = 0;
    matrix.m[2][0] = 0;
    matrix.m[1][2] = 0;
    matrix.m[1][0] = 0;
    matrix.m[0][2] = 0;
    matrix.m[0][1] = 0;

    memset(&third_scale, 0, 0x10);
    third_scale.x = state->final_scale >> 1;
    third_scale.y = state->final_scale;
    third_scale.z = state->final_scale;
    second_scale = third_scale;
    ScaleMatrix(&matrix, &second_scale);

    matrix.t[0] = state->final_x;
    matrix.t[1] = state->final_y;
    matrix.t[2] = state->final_z;
    g_EffectSequenceFinalGlowSprite.depth = state->final_alpha;
    FieldFx_DrawGlowSprite((FieldGlowSprite *)&g_EffectSequenceFinalGlowSprite, (GteMatrix *)&matrix, 1);

    return 0;
}

int RoomEffect_UpdatePairedSprite(void *object_, void *slot_, void *data_) {
    void *unused = object_;
    RoomFxControl *control = slot_;
    RoomFxPairedSpriteState *state = data_;
    RoomFxTransformOwner **ownerPtr;
    RoomFxTransform *transform;

    ownerPtr = FieldFx_GetScriptContext();
    transform = &(*ownerPtr)->transforms[state->transformIndex];

    state->phase++;
    state->phase %= 4;

    if (state->counter < 200) {
        state->counter++;
    }

    if (state->resourceSelector > 0) {
        state->resourceSelector--;
        if (state->resourceSelector == 0) {
            state->active = 0;
        }
    }

    if ((control->frame % 30) == 0) {
        state->x = transform->x;
        state->y = transform->y;
        state->z = transform->z;
        state->velocityX = 0;
        state->velocityY = -5;
        state->velocityZ = 0;
        state->alpha = 0x80;
        state->alphaStep = 0x96;
    }

    state->x += state->velocityX;
    state->y += state->velocityY;
    state->z += state->velocityZ;
    state->alphaStep += 60;
    state->alpha -= 4;
    if (state->alpha < 0) {
        state->alpha = 0;
    }

    if ((control->frame % 300) == 0) {
        state->sparkleX = transform->x;
        state->sparkleY = transform->y;
        state->sparkleZ = transform->z;
        state->sparkleAlpha = 0xFF;
        state->sparkleLife = 20;
    }

    if (state->sparkleLife > 0) {
        int random;
        int life;
        int alpha;
        int y;

        random = rand();
        life = (unsigned short)state->sparkleLife;
        alpha = state->sparkleAlpha;
        y = state->sparkleY;
        life--;
        alpha -= 8;
        y -= 5;
        state->sparkleAlpha = alpha;
        state->sparkleLife = life;
        y -= random % 20;
        state->sparkleY = y;
    }

    if (state->height < 1000) {
        state->height += rand() % 50;
    }

    if (state->height >= 501) {
        state->height -= rand() % 30;
    }

    if (state->resourceSelector == -1 && state->width != 0x80) {
        state->width += 8;
    }

    if ((unsigned short)(state->resourceSelector - 1) < 31) {
        state->width -= 4;
        if (state->width < 0) {
            state->width = 0;
        }
    }

    return 0;
}

s32 rand(void);

int RoomEffect_InitVariant38Pulse(void *object_, void *slot_, void *data_) {
    void *arg0 = object_;
    void *arg1 = slot_;
    RoomOverlay024Variant38SetupState *state = data_;
    s32 random;

    state->transform_index = *FieldFx_RegPtr(1);
    state->resource_selector = *FieldFx_RegPtr(2);
    state->active_flag = 1;

    if (state->resource_selector == -1) {
        state->width = 0;
        state->height = 0;
    } else {
        state->width = 0x80;
        state->height = 0x38;
    }

    state->field2C = 0;
    state->field30 = 0;
    state->field22 = 0;
    state->sparkle_timer = 0;
    random = rand();
    state->random_mod = random % 40;

    return 0;
}

int RoomEffect_DrawVariant38Pulse(void *object_, void *slot_, void *data_) {
    void *arg0 = object_;
    RoomOverlay024VariantControl *control = slot_;
    RoomOverlay024Variant38State *state = data_;
    RoomOverlay024Root **owner_ptr;
    RoomOverlay024Root *owner;
    RoomOverlay024Transform *transform;
    RoomOverlay024Matrix matrix;
    RoomOverlay024MatrixSeed8 base_seed;
    RoomOverlay024Vec4 scratch_scale;
    RoomOverlay024Vec4 draw_scale;
    RoomOverlay024Vec4 second_scale;
    RoomOverlay024Vec4 third_scale;
    s32 draw_zero;

    owner_ptr = FieldFx_GetScriptContext();
    owner = *owner_ptr;
    transform = &((RoomOverlay024Transform *)owner->view)[state->transform_index];
    base_seed = s_EffectBaseSeed;

    FieldFx_SetSpriteTexturePreset(((RoomLibFxMatrixState *)owner_ptr)->asset.page);
    FieldFx_SetSpriteCellSize(0x20, 0x20);
    FieldFx_SetSpriteColorMode(0x10);
    FieldFx_SetSpriteBlendMode(2);

    if (state->active_flag == 0) {
        return 0;
    }

    matrix.m[2][2] = 0x1000;
    matrix.m[1][1] = 0x1000;
    matrix.m[0][0] = 0x1000;
    matrix.t[2] = 0;
    matrix.t[1] = 0;
    matrix.t[0] = 0;
    matrix.m[2][1] = 0;
    matrix.m[2][0] = 0;
    matrix.m[1][2] = 0;
    matrix.m[1][0] = 0;
    matrix.m[0][2] = 0;
    matrix.m[0][1] = 0;

    memset(&draw_scale, 0, 0x10);
    draw_scale.x = state->drift_value;
    draw_scale.y = state->drift_value >> 1;
    draw_scale.z = state->drift_value;
    scratch_scale = draw_scale;
    ScaleMatrix(&matrix, &scratch_scale);

    g_EffectSequenceGlowSprite.depth = state->fade_alpha >> 1;

    matrix.t[0] = transform->x + (rand() % 20) - 10;
    matrix.t[1] = transform->y;
    matrix.t[2] = transform->z + (rand() % 20) - 10;

    if (state->resource_selector == -1) {
        FieldFx_InterpolateColorRamp(g_EffectSequenceLimeGlowRamp, state->field30, (u8 *)&g_EffectSequenceGlowSprite);
    } else {
        FieldFx_InterpolateColorRamp(g_EffectSequenceAmberGlowRamp, state->field30, (u8 *)&g_EffectSequenceGlowSprite);
    }
    g_EffectSequenceGlowSprite.cell = state->phase * 2;
    FieldFx_DrawGlowSprite((FieldGlowSprite *)&g_EffectSequenceGlowSprite, (GteMatrix *)&matrix, 1);
    RotMatrix(&base_seed.vector, &matrix);

    memset(&second_scale, 0, 0x10);
    second_scale.x = state->drift_value * 2;
    second_scale.y = state->drift_value;
    second_scale.z = state->drift_value * 2;
    draw_scale = second_scale;
    ScaleMatrix(&matrix, &draw_scale);

    g_EffectSequenceGlowSprite.depth = state->fade_alpha >> 2;
    matrix.t[0] = transform->x;
    matrix.t[1] = g_RoomFloorY->y;
    draw_zero = 0;
    matrix.t[2] = transform->z;
    g_EffectSequenceGlowSprite.cell = 8;
    FieldFx_DrawGlowSprite((FieldGlowSprite *)&g_EffectSequenceGlowSprite, (GteMatrix *)&matrix, draw_zero);

    if (control->frame >= 0x5B && state->sparkle_timer > 0) {
        matrix.m[2][2] = 0x1000;
        matrix.m[1][1] = 0x1000;
        matrix.m[0][0] = 0x1000;
        matrix.t[2] = 0;
        matrix.t[1] = 0;
        matrix.t[0] = 0;
        matrix.m[2][1] = 0;
        matrix.m[2][0] = 0;
        matrix.m[1][2] = 0;
        matrix.m[1][0] = 0;
        matrix.m[0][2] = 0;
        matrix.m[0][1] = 0;

        second_scale = s_EffectNarrowScale;
        ScaleMatrix(&matrix, &second_scale);

        matrix.t[0] = state->sparkle_x;
        matrix.t[1] = state->sparkle_y;
        matrix.t[2] = state->sparkle_z;
        g_EffectSequenceSparkleSprite.depth = state->sparkle_alpha;
        FieldFx_DrawGlowSprite((FieldGlowSprite *)&g_EffectSequenceSparkleSprite, (GteMatrix *)&matrix, 0);
    }

    FieldFx_SetSpriteBlendMode(3);

    matrix.m[2][2] = 0x1000;
    matrix.m[1][1] = 0x1000;
    matrix.m[0][0] = 0x1000;
    matrix.t[2] = 0;
    matrix.t[1] = 0;
    matrix.t[0] = 0;
    matrix.m[2][1] = 0;
    matrix.m[2][0] = 0;
    matrix.m[1][2] = 0;
    matrix.m[1][0] = 0;
    matrix.m[0][2] = 0;
    matrix.m[0][1] = 0;

    memset(&third_scale, 0, 0x10);
    third_scale.x = state->final_scale;
    third_scale.y = state->final_scale;
    third_scale.z = state->final_scale;
    second_scale = third_scale;
    ScaleMatrix(&matrix, &second_scale);

    matrix.t[0] = state->final_x;
    matrix.t[1] = state->final_y;
    matrix.t[2] = state->final_z;
    g_EffectSequenceFinalGlowSprite.depth = state->final_alpha;
    FieldFx_DrawGlowSprite((FieldGlowSprite *)&g_EffectSequenceFinalGlowSprite, (GteMatrix *)&matrix, 1);

    return 0;
}

int RoomEffect_UpdatePairedSpriteBurst(void *object_, void *slot_, void *data_) {
    void *unused = object_;
    void *unusedControl = slot_;
    RoomFxPairedSpriteState *state = data_;
    RoomFxTransformOwner **ownerPtr;
    RoomFxTransform *transform;

    ownerPtr = FieldFx_GetScriptContext();
    transform = &(*ownerPtr)->transforms[state->transformIndex];

    state->phase++;
    state->phase %= 4;

    if (state->counter < 200) {
        state->counter++;
    }

    if (state->resourceSelector > 0) {
        state->resourceSelector--;
        if (state->resourceSelector == 0) {
            state->active = 0;
        }
    }

    state->sparkleTimer++;
    if (state->alpha == 0) {
        int random;

        state->sparkleTimer = 0;
        state->x = transform->x;
        state->y = transform->y;
        state->z = transform->z;
        state->velocityX = 0;
        state->velocityY = -8;
        state->velocityZ = 0;
        random = rand();
        state->alphaStep = 150;
        state->alpha = (random % 88) + 40;
    }

    state->x += state->velocityX;
    state->y += state->velocityY;
    state->z += state->velocityZ;

    {
        int random;
        int alpha;
        int alphaStep;

        random = rand();
        alpha = *(unsigned short *)&state->alpha;
        alphaStep = state->alphaStep;
        alpha -= 4;
        alphaStep += 50;
        state->alpha = alpha;
        alphaStep += random % 30;
        state->alphaStep = alphaStep;
        if ((short)alpha < 0) {
            state->alpha = 0;
        }
    }

    if ((rand() % 200) == 0) {
        state->sparkleX = transform->x;
        state->sparkleY = transform->y;
        state->sparkleZ = transform->z;
        state->sparkleAlpha = 0xFF;
        state->sparkleLife = 20;
    }

    if (state->sparkleLife > 0) {
        int random;
        int life;
        int alpha;
        int y;

        random = rand();
        life = (unsigned short)state->sparkleLife;
        alpha = state->sparkleAlpha;
        y = state->sparkleY;
        life--;
        alpha -= 8;
        y -= 2;
        state->sparkleAlpha = alpha;
        state->sparkleLife = life;
        y -= random % 5;
        state->sparkleY = y;
    }

    if (state->height < 500) {
        state->height += rand() % 20;
    }

    if (state->height >= 401) {
        state->height -= rand() % 15;
    }

    if (state->resourceSelector == -1 && state->width != 0x80) {
        state->width += 8;
    }

    if ((unsigned short)(state->resourceSelector - 1) < 31) {
        state->width -= 4;
        if (state->width < 0) {
            state->width = 0;
        }
    }

    return 0;
}

int RoomEffect_InitVariant290(void *object_, void *slot_, void *data_) {
    void *arg0 = object_;
    void *arg1 = slot_;
    RoomOverlay024Variant290SetupState *state = data_;
    s32 random;
    s32 height;

    state->transform_index = *FieldFx_RegPtr(1);
    state->resource_selector = *FieldFx_RegPtr(2);
    state->active_flag = 1;

    if (state->resource_selector == -1) {
        height = 0x290;
        state->width = 0;
    } else {
        state->width = 0x80;
        height = 0x290;
    }
    state->height = height;

    state->field2C = 0;
    state->field30 = 0;
    state->field22 = 0;
    state->sparkle_timer = 0;
    random = rand();
    state->random_mod = random % 40;

    return 0;
}

int RoomEffect_DrawVariant290(void *object_, void *slot_, void *data_) {
    void *arg0 = object_;
    RoomOverlay024VariantControl *control = slot_;
    RoomOverlay024Variant290State *state = data_;
    RoomOverlay024Root **owner_ptr;
    RoomOverlay024Root *owner;
    RoomOverlay024Transform *transform;
    RoomOverlay024Matrix matrix;
    RoomOverlay024MatrixSeed8 base_seed;
    RoomOverlay024Vec4 scratch_scale;
    RoomOverlay024Vec4 draw_scale;
    RoomOverlay024Vec4 second_scale;

    owner_ptr = FieldFx_GetScriptContext();
    owner = *owner_ptr;
    transform = &((RoomOverlay024Transform *)owner->view)[state->transform_index];
    base_seed = s_EffectBaseSeed;

    FieldFx_SetSpriteTexturePreset(((RoomLibFxMatrixState *)owner_ptr)->asset.page);
    FieldFx_SetSpriteCellSize(0x20, 0x20);
    FieldFx_SetSpriteColorMode(0x10);
    FieldFx_SetSpriteBlendMode(2);

    if (state->active_flag == 0) {
        return 0;
    }

    matrix.m[2][2] = 0x1000;
    matrix.m[1][1] = 0x1000;
    matrix.m[0][0] = 0x1000;
    matrix.t[2] = 0;
    matrix.t[1] = 0;
    matrix.t[0] = 0;
    matrix.m[2][1] = 0;
    matrix.m[2][0] = 0;
    matrix.m[1][2] = 0;
    matrix.m[1][0] = 0;
    matrix.m[0][2] = 0;
    matrix.m[0][1] = 0;

    memset(&draw_scale, 0, 0x10);
    draw_scale.x = state->drift_value;
    draw_scale.y = state->drift_value;
    draw_scale.z = state->drift_value;
    scratch_scale = draw_scale;
    ScaleMatrix(&matrix, &scratch_scale);

    g_EffectSequenceGlowSprite.depth = state->fade_alpha;

    matrix.t[0] = transform->x + (rand() % 20) - 10;
    matrix.t[1] = transform->y;
    matrix.t[2] = transform->z + (rand() % 20) - 10;

    FieldFx_InterpolateColorRamp(g_EffectSequenceAmberGlowRamp, state->field30, (u8 *)&g_EffectSequenceGlowSprite);
    g_EffectSequenceGlowSprite.cell = state->phase * 2;
    FieldFx_DrawGlowSprite((FieldGlowSprite *)((u8 *)&g_EffectSequenceGlowSprite), (GteMatrix *)&matrix, 1);
    RotMatrix(&base_seed.vector, &matrix);

    memset(&second_scale, 0, 0x10);
    second_scale.x = state->drift_value * 2;
    second_scale.y = state->drift_value;
    second_scale.z = state->drift_value * 4;
    draw_scale = second_scale;
    ScaleMatrix(&matrix, &draw_scale);

    g_EffectSequenceGlowSprite.depth = state->fade_alpha >> 2;
    matrix.t[0] = transform->x;
    matrix.t[1] = g_RoomFloorY->y;
    matrix.t[2] = transform->z;
    g_EffectSequenceGlowSprite.cell = 8;
    FieldFx_DrawGlowSprite((FieldGlowSprite *)((u8 *)&g_EffectSequenceGlowSprite), (GteMatrix *)&matrix, 0);

    if (control->frame < 0x5B) {
        return 0;
    }
    if (state->sparkle_timer <= 0) {
        return 0;
    }

    matrix.m[2][2] = 0x1000;
    matrix.m[1][1] = 0x1000;
    matrix.m[0][0] = 0x1000;
    matrix.t[2] = 0;
    matrix.t[1] = 0;
    matrix.t[0] = 0;
    matrix.m[2][1] = 0;
    matrix.m[2][0] = 0;
    matrix.m[1][2] = 0;
    matrix.m[1][0] = 0;
    matrix.m[0][2] = 0;
    matrix.m[0][1] = 0;

    second_scale = s_EffectNarrowScale;
    ScaleMatrix(&matrix, &second_scale);

    matrix.t[0] = state->sparkle_x;
    matrix.t[1] = state->sparkle_y;
    matrix.t[2] = state->sparkle_z;
    g_EffectSequenceSparkleSprite.depth = state->sparkle_alpha;
    FieldFx_DrawGlowSprite((FieldGlowSprite *)&g_EffectSequenceSparkleSprite, (GteMatrix *)&matrix, 0);

    return 0;
}

int RoomEffect_UpdateSparkleHeight(void *object_, void *slot_, void *data_) {
    void *unused = object_;
    RoomFxControl *control = slot_;
    RoomFxPairedSpriteState *state = data_;
    RoomFxTransformOwner **ownerPtr;
    RoomFxTransform *transform;

    ownerPtr = FieldFx_GetScriptContext();
    transform = &(*ownerPtr)->transforms[state->transformIndex];

    state->phase++;
    state->phase %= 4;

    if (state->counter < 200) {
        state->counter++;
    }

    if (state->resourceSelector > 0) {
        state->resourceSelector--;
        if (state->resourceSelector == 0) {
            state->active = 0;
        }
    }

    if ((rand() % 200) == 0) {
        state->sparkleX = transform->x;
        state->sparkleY = transform->y;
        state->sparkleZ = transform->z;
        state->sparkleAlpha = 0xFF;
        state->sparkleLife = 20;
    }

    if (state->sparkleLife > 0) {
        int random;
        int life;
        int alpha;
        int y;

        random = rand();
        life = (unsigned short)state->sparkleLife;
        alpha = state->sparkleAlpha;
        y = state->sparkleY;
        life--;
        alpha -= 8;
        y -= 10;
        state->sparkleAlpha = alpha;
        state->sparkleLife = life;
        y -= random % 20;
        state->sparkleY = y;
    }

    if (control->frame >= 31) {
        state->height -= rand() % 20;
        state->height += rand() % 2;
        if (state->height < 500) {
            state->height = 500;
        }
    } else {
        if (state->height < 1300) {
            state->height += rand() % 60;
        }
        if (state->height >= 1101) {
            state->height -= rand() % 40;
        }
    }

    if (state->resourceSelector == -1 && state->width != 0x80) {
        state->width += 8;
    }

    if ((unsigned short)(state->resourceSelector - 1) < 31) {
        state->width -= 4;
        if (state->width < 0) {
            state->width = 0;
        }
    }

    return 0;
}

s32 *FieldFx_RegPtr(s32 slot);

int RoomEffect_InitVariantD(void *object_, void *slot_, void *data_) {
    void *arg0 = object_;
    void *arg1 = slot_;
    RoomOverlay024VariantDSetupRecord *obj = data_;
    s32 random;

    obj->half2E = 0xD;
    obj->resource1 = *FieldFx_RegPtr(1);
    obj->resource2 = *FieldFx_RegPtr(2);
    obj->half26 = 1;
    obj->half2A = 0x80;
    obj->resource1_again = *FieldFx_RegPtr(1);
    obj->half2C = 0;
    obj->word30 = 0;
    obj->half22 = 0;
    obj->half14 = 0;
    random = rand();
    obj->random_mod = random % 40;

    return 0;
}

int RoomEffect_DrawVariantD(void *object_, void *slot_, void *data_) {
    void *arg0 = object_;
    RoomOverlay024VariantControl *control = slot_;
    RoomOverlay024VariantDState *state = data_;
    RoomOverlay024Root **owner_ptr;
    RoomOverlay024Root *owner;
    RoomOverlay024Transform *transform;
    RoomOverlay024Matrix matrix;
    RoomOverlay024MatrixSeed8 base_seed;
    RoomOverlay024MatrixSeed8 project_seed;
    s16 projected[4];
    RoomOverlay024Vec4 scratch_scale;
    RoomOverlay024Vec4 draw_scale;
    RoomOverlay024Vec4 second_scale;
    s32 scale;

    owner_ptr = FieldFx_GetScriptContext();
    owner = *owner_ptr;
    transform = &((RoomOverlay024Transform *)owner->view)[state->transform_index];
    base_seed = s_EffectBaseSeed;
    project_seed = s_EffectProjectSeed;

    FieldFx_SetSpriteTexturePreset(((RoomLibFxMatrixState *)owner_ptr)->asset.page);
    FieldFx_SetSpriteCellSize(0x40, 0x40);
    FieldFx_SetSpriteColorMode(0x10);
    FieldFx_SetSpriteBlendMode(2);
    memcpy(&matrix, transform, sizeof(matrix));
    ApplyMatrixSV(&matrix, &project_seed.vector, (GteShortVector *)projected);

    scale = (state->drift_value) >> 2;
    if (state->active_flag == 0) {
        return 0;
    }

    FieldFx_SetSpriteCellSize(0x20, 0x20);

    matrix.m[2][2] = 0x1000;
    matrix.m[1][1] = 0x1000;
    matrix.m[0][0] = 0x1000;
    matrix.t[2] = 0;
    matrix.t[1] = 0;
    matrix.t[0] = 0;
    matrix.m[2][1] = 0;
    matrix.m[2][0] = 0;
    matrix.m[1][2] = 0;
    matrix.m[1][0] = 0;
    matrix.m[0][2] = 0;
    matrix.m[0][1] = 0;

    memset(&draw_scale, 0, 0x10);
    draw_scale.x = scale * 3;
    draw_scale.y = state->drift_value;
    draw_scale.z = scale * 3;
    scratch_scale = draw_scale;
    ScaleMatrix(&matrix, &scratch_scale);

    g_EffectSequenceGlowSpriteSecondary.depth = state->fade_alpha;

    matrix.t[0] = transform->x + projected[0] + (rand() % 20) - 10;
    matrix.t[1] = transform->y + projected[1] - ((state->vertical_offset) >> 4);
    matrix.t[2] = transform->z + projected[2] + (rand() % 20) - 10;

    FieldFx_InterpolateColorRamp(g_EffectSequenceAmberGlowRamp, state->field30, (u8 *)&g_EffectSequenceGlowSpriteSecondary);
    g_EffectSequenceGlowSpriteSecondary.cell = state->phase * 2;
    FieldFx_DrawGlowSprite((FieldGlowSprite *)&g_EffectSequenceGlowSpriteSecondary, (GteMatrix *)&matrix, 1);

    FieldFx_SetSpriteCellSize(0x20, 0x20);
    RotMatrix(&base_seed.vector, &matrix);

    memset(&second_scale, 0, 0x10);
    second_scale.x = state->drift_value;
    second_scale.y = state->drift_value;
    second_scale.z = state->drift_value;
    draw_scale = second_scale;
    ScaleMatrix(&matrix, &draw_scale);

    g_EffectSequenceGlowSpriteSecondary.depth = state->fade_alpha >> 2;
    matrix.t[0] = transform->x + projected[0];
    matrix.t[1] = -0x457;
    matrix.t[2] = transform->z + projected[2];
    g_EffectSequenceGlowSpriteSecondary.cell = 8;
    FieldFx_DrawGlowSprite((FieldGlowSprite *)&g_EffectSequenceGlowSpriteSecondary, (GteMatrix *)&matrix, 0);

    if (control->frame < 0x5B) {
        return 0;
    }
    if (state->sparkle_timer <= 0) {
        return 0;
    }

    matrix.m[2][2] = 0x1000;
    matrix.m[1][1] = 0x1000;
    matrix.m[0][0] = 0x1000;
    matrix.t[2] = 0;
    matrix.t[1] = 0;
    matrix.t[0] = 0;
    matrix.m[2][1] = 0;
    matrix.m[2][0] = 0;
    matrix.m[1][2] = 0;
    matrix.m[1][0] = 0;
    matrix.m[0][2] = 0;
    matrix.m[0][1] = 0;

    second_scale = s_EffectNarrowScale;
    ScaleMatrix(&matrix, &second_scale);

    matrix.t[0] = state->sparkle_x;
    matrix.t[1] = state->sparkle_y;
    matrix.t[2] = state->sparkle_z;
    g_EffectSequenceSparkleSprite.depth = state->sparkle_alpha;
    FieldFx_DrawGlowSprite((FieldGlowSprite *)&g_EffectSequenceSparkleSprite, (GteMatrix *)&matrix, 0);

    return 0;
}

int RoomEffect_UpdateTransformedSparkle(void *object_, void *slot_, void *data_) {
    void *unused = object_;
    void *unusedControl = slot_;
    RoomFxPairedSpriteState *state = data_;
    RoomFxTransformOwner **ownerPtr;
    RoomFxTransform *transform;
    RoomFxSeed8 seed;
    RoomFxSeed8 transformed;

    ownerPtr = FieldFx_GetScriptContext();
    transform = &(*ownerPtr)->transforms[state->transformIndex];
    seed = s_EffectProjectSeed;
    ApplyMatrixSV((const GteMatrix *)transform, &seed.vector, &transformed.vector);

    state->phase++;
    state->phase %= 4;

    if (state->counter < 200) {
        state->counter++;
    }

    if (state->resourceSelector > 0) {
        state->resourceSelector--;
        if (state->resourceSelector == 0) {
            state->active = 0;
        }
    }

    if ((rand() % 400) == 0) {
        state->sparkleX = (u16)transformed.vector.x +
                          transform->x + (rand() % 60) - 30;
        state->sparkleY = (u16)transformed.vector.y +
                          transform->y;
        {
            int random;
            u16 z;
            int transformZ;

            random = rand();
            transformZ = transform->z;
            z = (u16)transformed.vector.z;
            state->sparkleAlpha = 0xFF;
            state->sparkleLife = 20;
            z += transformZ;
            random %= 60;
            state->sparkleZ = z + random - 30;
        }
    }

    if (state->sparkleLife > 0) {
        int random;
        int life;
        int alpha;
        int y;

        random = rand();
        life = (u16)state->sparkleLife;
        alpha = state->sparkleAlpha;
        y = state->sparkleY;
        life--;
        alpha -= 8;
        y -= 10;
        state->sparkleAlpha = alpha;
        state->sparkleLife = life;
        y -= random % 20;
        state->sparkleY = y;
    }

    state->height += (rand() % 30) - 15;
    if (state->height < state->minimumHeight) {
        state->height = state->minimumHeight;
    }

    if (state->resourceSelector == -1 && state->width != 0x80) {
        state->width += 8;
    }

    if ((unsigned short)(state->resourceSelector - 1) < 31) {
        state->width -= 4;
        if (state->width < 0) {
            state->width = 0;
        }
    }

    return 0;
}

int RoomEffect_EffectSequenceNopB(void *object_, void *slot_, void *data_) {


    return 0;
}


void FieldFx_ModelSetTexture(s32 arg0, s32 arg1);

int RoomEffect_DrawEffectGlow(void *object_, void *slot_, void *data_) {

    RoomOverlay024Root **root;
    RoomOverlay024View *view;
    RoomOverlay024ViewMatrix matrix;
    RoomOverlay024MatrixSeed8 seed;
    short out[4];
    RoomOverlay024VectorSeed vector_seed;
    s32 handle_a;
    s32 handle_b;

    root = FieldFx_GetScriptContext();
    view = (RoomOverlay024View *)(*root)->view;

    seed = s_EffectProjectSeed;
    matrix = view->tail.matrix;
    vector_seed = s_EffectVectorSeed;

    ScaleMatrix(&matrix, &vector_seed.vector);
    ApplyMatrixSV(&view->tail.matrix, &seed.vector, (GteShortVector *)out);

    matrix.t[0] += out[0];
    matrix.t[1] += out[1];
    matrix.t[2] += out[2];

    FieldFx_SetSpriteTexturePreset(((RoomLibFxMatrixState *)root)->asset.page);
    FieldFx_SetSpriteCellSize(0x20, 0x20);
    FieldFx_SetSpriteColorMode(0x10);
    FieldFx_SetSpriteBlendMode(2);
    FieldFx_ModelScrollUv(g_EffectSequenceModel, 0, 0);
    FieldFx_ModelSetDepthBias(-10);
    handle_a = GetTPage(0, 1, 0x340, 0x100);
    handle_a = (u16)handle_a;
    handle_b = GetClut(0x10, 0x1D7);
    FieldFx_ModelSetTexture(handle_a, (u16)handle_b);
    FieldFx_ModelSetSemiTrans(0);
    FieldFx_ModelSaveColors(g_EffectSequenceModel);
    FieldFx_ModelScaleColors(g_EffectSequenceModel, 0x80);
    FieldFx_ModelDraw(g_EffectSequenceModel, (GteMatrix *)&matrix);
    FieldFx_ModelRestoreColors(g_EffectSequenceModel);

    return 0;
}

int RoomEffect_EffectSequenceNopC(void *object_, void *slot_, void *data_) {


    return 0;
}

int RoomEffect_SpawnDriftingEffect(void *object_, void *slot_, void *data_) {
    void *arg0 = object_;
    void *arg1 = slot_;
    RoomOverlay024Effect *effect = data_;
    RoomOverlay024View *view;
    RoomOverlay024MatrixSeed8 seed;
    u16 out[4];
    s32 random;

    view = (RoomOverlay024View *)(*(RoomOverlay024Root **)FieldFx_GetScriptContext())->view;
    seed = s_EffectProjectSeed;
    ApplyMatrixSV(&view->tail.matrix, &seed.vector, (GteShortVector *)out);

    if (effect->timer == 0) {
        effect->half14 = 0;
        effect->x = out[0] + view->tail.position.base_x;
        effect->y = out[1] + view->tail.position.base_y;
        effect->z = out[2] + view->tail.position.base_z;
        effect->half8 = 0;
        effect->velocity_y = -8;
        effect->halfC = 0;
        random = rand();
        effect->duration = 0x96;
        effect->timer = random % 80 + 0x50;
    }

    return 0;
}

typedef GteMatrix RoomLibEffectMatrix;

typedef struct RoomLibEffectView {
    char pad00[0x1A0];
    RoomLibEffectMatrix matrix;
} RoomLibEffectView;

typedef struct RoomLibEffectRoot {
    char pad00[PE1_OFFSETOF(FieldActor, render_object) + PE1_OFFSETOF(RenderObjectEntity, matrices)];
    RoomLibEffectView *view;
} RoomLibEffectRoot;

typedef struct RoomLibEffectRootTable {
    RoomLibEffectRoot *root;
    char pad04[0x20];
    unsigned char mode;
} RoomLibEffectRootTable;

typedef struct RoomLibScaledEffect {
    short x;
    short y;
    short z;
    char pad06[0xA];
    unsigned short depth;
    unsigned short scaleTimer;
} RoomLibScaledEffect;

typedef RoomFxSeed8 RoomLibMatrixSeed;

typedef RoomOverlay024VectorSeed RoomLibVectorSeed;

int RoomEffect_DrawScaledEffect(void *object_, void *slot_, void *data_) {
    void *unused0 = object_;
    void *unused1 = slot_;
    RoomLibScaledEffect *effect = data_;
    RoomLibEffectRootTable *roots;
    RoomLibEffectView *view;
    RoomLibEffectMatrix matrix;
    RoomLibMatrixSeed seed;
    short offset[4];
    RoomLibVectorSeed vectorSeed;
    RoomLibVectorSeed scaleSeed;
    unsigned short *depth;
    int scale;
    unsigned short depthValue;

    roots = FieldFx_GetScriptContext();
    view = roots->root->view;
    seed = s_EffectProjectSeed;
    matrix = view->matrix;
    vectorSeed = s_EffectVectorSeed;
    ScaleMatrix((RoomSpriteMatrix *)&matrix, (RoomFxVec4 *)&vectorSeed);
    ApplyMatrixSV(&view->matrix, &seed.vector, (GteShortVector *)offset);
    matrix.t[0] += offset[0];
    matrix.t[1] += offset[1];
    matrix.t[2] += offset[2];

    FieldFx_SetSpriteTexturePreset(roots->mode);
    FieldFx_SetSpriteCellSize(0x20, 0x20);
    FieldFx_SetSpriteColorMode(0x10);
    FieldFx_SetSpriteBlendMode(2);
    FieldFx_SetSpriteBlendMode(3);

    matrix.m[2][2] = 0x1000;
    matrix.m[1][1] = 0x1000;
    matrix.m[0][0] = 0x1000;
    matrix.t[2] = 0;
    matrix.t[1] = 0;
    matrix.t[0] = 0;
    matrix.m[2][1] = 0;
    matrix.m[2][0] = 0;
    matrix.m[1][2] = 0;
    matrix.m[1][0] = 0;
    matrix.m[0][2] = 0;
    matrix.m[0][1] = 0;

    scale = (short)effect->scaleTimer >> 2;
    memset(&scaleSeed, 0, 0x10);
    scaleSeed.words[0] = scale * 3;
    scaleSeed.words[1] = scale * 3;
    scaleSeed.words[2] = scale * 3;
    ScaleMatrix((RoomSpriteMatrix *)&matrix, (RoomFxVec4 *)&scaleSeed);

    depth = &g_EffectSequenceFinalGlowSprite.depth;
    matrix.t[0] = effect->x;
    matrix.t[1] = effect->y;
    matrix.t[2] = effect->z;
    depthValue = effect->depth;
    g_EffectSequenceFinalGlowSprite.offset = -0x65;
    *depth = depthValue;
    FieldFx_DrawGlowSprite((FieldGlowSprite *)((char *)depth - 0xA), (RoomSpriteMatrix *)&matrix, 1);

    return 0;
}

typedef struct RoomDriftView {
    char pad0[0x1A0];
    char transform[0x14];
    s32 base_x;
    s32 base_y;
    s32 base_z;
} RoomDriftView;

typedef struct RoomDriftRoot {
    char pad0[PE1_OFFSETOF(FieldActor, render_object) + PE1_OFFSETOF(RenderObjectEntity, matrices)];
    RoomDriftView *view;
} RoomDriftRoot;

typedef struct RoomDriftEffect {
    s16 x;
    s16 y;
    s16 z;
    char pad6[0x2];
    s16 velocity_x;
    s16 velocity_y;
    s16 velocity_z;
    char padE[0x2];
    s16 timer;
    s16 duration;
    s16 half14;
} RoomDriftEffect;

typedef RoomFxSeed8 RoomDriftMatrixSeed;

int RoomEffect_UpdateDriftingEffect(void *object_, void *slot_, void *data_) {
    void *arg0 = object_;
    void *arg1 = slot_;
    RoomDriftEffect *effect = data_;
    RoomDriftView *view;
    RoomDriftEffect *current;
    RoomDriftMatrixSeed seed;
    u16 out[4];
    s32 initialRandom;
    s32 durationRandom;
    u16 timer;
    u16 x;
    u16 y;
    u16 z;
    u16 velocity_x;
    u16 velocity_y;
    u16 velocity_z;

    current = effect;
    view = (*(RoomDriftRoot **)FieldFx_GetScriptContext())->view;
    seed = s_EffectProjectSeed;
    ApplyMatrixSV((const GteMatrix *)&view->transform, &seed.vector, (GteShortVector *)out);

    if (current->timer == 0) {
        current->half14 = 0;
        current->x = out[0] + view->base_x;
        current->y = out[1] + view->base_y + 0x1E;
        current->z = out[2] + view->base_z;
        current->velocity_x = 0;
        current->velocity_y = -8;
        current->velocity_z = 0;
        initialRandom = rand();
        current->duration = 0x96;
        current->timer = initialRandom % 88 + 40;
    }

    x = current->x;
    velocity_x = current->velocity_x;
    velocity_y = current->velocity_y;
    velocity_z = current->velocity_z;
    current->x = x + velocity_x;
    y = current->y;
    z = current->z;
    current->y = y + velocity_y;
    current->z = z + velocity_z;
    durationRandom = rand();
    timer = current->timer - 4;
    current->timer = timer;
    current->duration += durationRandom % 30 + 50;
    if ((s16)timer < 0) {
        current->timer = 0;
    }

    return 0;
}

/* USA M049/M050/M090/M344/M374 share these original tables and data.
 * Only controller state grows by the native owner pointer's extra bytes. */
FieldFxHandler g_RoomEffectSequenceInitList[8] = {
    RoomEffect_InitEffectSequenceSprites, RoomEffect_InitVariant38,
    RoomEffect_InitVariant38Pulse, RoomEffect_InitVariant290, RoomEffect_InitVariantD,
    RoomEffect_EffectSequenceNopB, RoomEffect_SpawnDriftingEffect,
    (FieldFxHandler)(intptr_t)-1};
FieldFxHandler g_RoomEffectSequenceDrawList[8] = {
    RoomEffect_EffectSequenceNopA, RoomEffect_DrawVariant38,
    RoomEffect_DrawVariant38Pulse, RoomEffect_DrawVariant290, RoomEffect_DrawVariantD,
    RoomEffect_DrawEffectGlow, RoomEffect_DrawScaledEffect, (FieldFxHandler)(intptr_t)-1};
FieldFxHandler g_RoomEffectSequenceUpdateList[8] = {
    RoomEffect_ConfigureEffectSequence, RoomEffect_UpdatePairedSprite,
    RoomEffect_UpdatePairedSpriteBurst, RoomEffect_UpdateSparkleHeight,
    RoomEffect_UpdateTransformedSparkle, RoomEffect_EffectSequenceNopC,
    RoomEffect_UpdateDriftingEffect, (FieldFxHandler)(intptr_t)-1};
u16 g_RoomEffectSequenceSizes[8] = {
    (PE1_OFFSETOF(RoomLibFxMatrixState, asset)+4+sizeof(void *)-1) & ~(sizeof(void *)-1), 0x38,0x38,0x38,0x38,0x38,0x38,0xffff};
int g_RoomScript[] = {0x00010000,-1};
PmCommand g_RoomEffectSequenceHandlers = {RoomLib_ClassNop0,RoomLib_PlantScript,
    RoomLib_Spawn6,RoomEffect_EffectSequenceRegister,RoomEffect_EffectSequenceStart,
    RoomEffect_EffectSequenceNop5,RoomEffect_EffectSequenceNop6};
RoomEffectSequenceSprite g_EffectSequenceGlowSprite, g_EffectSequenceGlowSpriteSecondary,
    g_EffectSequenceFinalGlowSprite, g_EffectSequenceSparkleSprite;
void *g_EffectSequenceModel;
u8 g_EffectSequenceAmberGlowRamp[12] = {180,255,0,10,160,200,10,25,80,40,20,255};
u8 g_EffectSequenceLimeGlowRamp[12] = {190,180,0,5,200,100,10,25,80,40,20,255};
