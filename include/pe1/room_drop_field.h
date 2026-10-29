#ifndef PE1_ROOM_DROP_FIELD_H
#define PE1_ROOM_DROP_FIELD_H

#include "common.h"
#include "pe1/field_collision.h"
#include "pe1/field_ring_geometry.h"
#include "pe1/field_script_context.h"
#include "pe1/field_sprite_state.h"
#include "pe1/fieldfx_calls.h"
#include "pe1/gte_types.h"
#include "pe1/pe_image.h"
#include "pe1/psyq_libc.h"
#include "pe1/room_drop_field_class.h"
#include "pe1/room_fx.h"
#include "pe1/room_interface.h"
#include "pe1/room_spark.h"
#include "pe1/field_actor.h"
#include "pe1/fieldfx_state.h"

/* The drop field effect set linked by room_m174, room_m348 and room_m383
 * (src/overlays/room_lib/drop_field.c): a controller and
 * twelve init/draw/update triples, among them the drop field, a beam pair,
 * a ring pair, staged motion and eight particles. */

typedef struct RoomDropFieldQuad {
    s32 x;
    s32 y;
    s32 z;
    s32 w;
} RoomDropFieldQuad;

/* A view matrix copied as words; the drop field takes its two halves as
 * its anchor and facing. */
typedef struct RoomDropFieldMatrixWords {
    RoomDropFieldQuad head;
    RoomDropFieldQuad tail;
} RoomDropFieldMatrixWords;

/* The room entity and its link as the controller init reads them. */
typedef FieldActor RoomDropFieldLink;
typedef FieldFxObject RoomDropFieldEntity;

typedef struct RoomDropFieldState {
    RoomDropFieldLink *link;
    RoomDropFieldMatrixWords matrix0;
    RoomDropFieldMatrixWords matrix1;
    union {
        int index;
        u8 page;                  /* texture page the draws select */
    } asset;                      /* 0x44 */
    u16 holdFrames;               /* 0x48 */
    s16 holdTimer;                /* 0x4A */
    s16 hit;                      /* 0x4C */
    s16 particlePattern;          /* 0x4E */
    s32 radiusRate;               /* 0x50 */
    s32 fallRate;                 /* 0x54 */
    s32 initialFallStep;          /* 0x58, converted to 8.8 at spawn */
} RoomDropFieldState;

PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomDropFieldState, holdFrames) == sizeof(void *) + 0x44,
                  room_drop_field_hold_offset);
PE1_STATIC_ASSERT(sizeof(RoomDropFieldState) == sizeof(void *) + 0x58,
                  room_drop_field_state_size);

typedef struct RoomDropFieldVec {
    s16 x, y, z, pad;
} RoomDropFieldVec;

/* State of the sixteen drops. */
typedef struct RoomDropFieldDrops {
    u8 flag[16];                  /* 0x00 */
    u8 pad10[0x10];
    s16 height[16];               /* 0x20 */
    s16 scale[16];                /* 0x40 */
    RoomDropFieldVec pos[16];     /* 0x60 */
    RoomDropFieldVec vel[16];     /* 0xE0 */
    RoomDropFieldQuad anchor;     /* 0x160 */
    RoomDropFieldQuad facing;     /* 0x170 */
    s16 h180, h182, h184;
} RoomDropFieldDrops;

PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomDropFieldDrops, h180) == 0x180,
                  room_drop_field_drops_h180);

/* Work block of the spin, double and model sprites: position, rotation,
 * uniform scale and depth. The spin sprite grows while phase is 1 and
 * shrinks while it is 0; the double sprite counts its frames there. */
typedef struct RoomDropFieldSprite {
    short x;                      /* 0x00 */
    short y;                      /* 0x02 */
    short z;                      /* 0x04 */
    u8 pad06[2];
    GteShortVector rotation;      /* 0x08 */
    u16 scale;                    /* 0x10 */
    u16 depth;                    /* 0x12 */
    u16 phase;                    /* 0x14 */
} RoomDropFieldSprite;

PE1_STATIC_ASSERT(PE1_OFFSETOF(RoomDropFieldSprite, phase) == 0x14,
                  room_drop_field_sprite_phase);

/* Stack frame of the two rotated sprite draws. */
typedef struct RoomDropFieldSpinFrame {
    union {
        GteMatrix matrix;
        struct {
            GteVector sp10;
            s32 pad20;
            s32 sp24[3];
        };
    };
    GteVector sp30;
    GteVector sp40;
} RoomDropFieldSpinFrame;

PE1_STATIC_ASSERT(sizeof(RoomDropFieldSpinFrame) == 0x40,
                  room_drop_field_spin_frame_size);

/* Per-room data: seven sprite records, four ring records (the beam pair
 * draws the last two), the ring and beam images and the particle animation
 * table. The spawn tables are declared with the module class
 * (pe1/room_drop_field_class.h). */
extern RoomFxSpritePacket g_RoomDropFieldSpinSprite;
extern RoomFxSpritePacket g_RoomDropFieldModelSprite;
/* The model the controller loads; it follows the model sprite record. */
extern void *g_RoomDropFieldModel;
extern RoomFxSpritePacket g_RoomDropFieldDoubleSprite;
extern RoomFxSpritePacket g_RoomDropFieldParticleSprite;
extern RoomFxSpritePacket g_RoomDropFieldQuadSprite;
extern RoomFxEmitterParams g_RoomDropFieldRings[4];
extern RoomFxSpritePacket g_RoomDropFieldDropSprite;
extern RoomFxSpritePacket g_RoomDropFieldMotionSprite;
extern char g_RoomDropFieldRingImages[];
extern unsigned char g_RoomDropFieldBeamImages[];
extern u8 g_RoomDropFieldParticleFrames[];

s32 *FieldFx_RegPtr(s32 index);
#include "pe1/field_model_draw.h"

#endif
