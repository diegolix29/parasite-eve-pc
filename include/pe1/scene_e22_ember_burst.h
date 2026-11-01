#ifndef PE1_SCENE_E22_EMBER_BURST_H
#define PE1_SCENE_E22_EMBER_BURST_H

#include "common.h"
#include "pe1/akao_send.h"
#include "pe1/asset_find.h"
#include "pe1/field_actor.h"
#include "pe1/field_model_draw.h"
#include "pe1/field_owner_actor.h"
#include "pe1/field_sprite_draw.h"
#include "pe1/field_texture_tables.h"
#include "pe1/fieldfx_callback_list.h"
#include "pe1/fieldfx_calls.h"
#include "pe1/fieldfx_task_pool.h"
#include "pe1/gte_types.h"
#include "pe1/psyq_gpu.h"
#include "pe1/render_object.h"
#include "pe1/room_floor.h"
#include "pe1/room_orbit_trail.h"
#include "pe1/scene_archive.h"

/* Ember controllers (scene_e22): ride the room actor, wake the scene
 * object's status byte, spray ember drift particles and draw a halo and a
 * ring while they glow. */

typedef struct SceneE22EmberBurst {
    s16 x, y, z;                  /* 0x00 */
    s16 reserved06;
    s16 state;                    /* 0x08 */
    s16 timer;                    /* 0x0A */
    s16 glow;                     /* 0x0C */
} SceneE22EmberBurst;

typedef struct SceneE22EmberActor {
    u8 reserved[0x4C];
    u32 flags;                    /* 0x4C */
} SceneE22EmberActor;

typedef struct SceneE22EmberObject {
    u32 flags;                    /* 0x00 */
    u8 reserved04[0x14];
    u8 *status;                   /* 0x18 */
} SceneE22EmberObject;

typedef struct SceneE22EmberSlot {
    SceneE22EmberObject *object;
} SceneE22EmberSlot;

typedef struct SceneE22EmberObjectChannel {
    s32 reserved[2];
    SceneE22EmberSlot *pool;      /* 0x08 */
} SceneE22EmberObjectChannel;

typedef struct SceneE22EmberChannel {
    s32 reserved[2];
    void *pool;                   /* 0x08 */
} SceneE22EmberChannel;

typedef struct SceneE22EmberEvent {
    u8 reserved[0xD];
    u8 active;                    /* 0x0D */
    u8 reserved0E[4];
    s16 jointSet;                 /* 0x12 */
} SceneE22EmberEvent;

extern struct FieldActor *g_PlayerEntity; /* the player actor (0x8009D254) */

extern int rand(void);
extern void Battle_HaltOnPositiveX(void);
extern void Battle_InitEntityColors(void);

/* Link beam controller (SceneE22_LinkBeam): two light ribbons strung across
 * three actor joints picked by the scene event's joint set; the last joint
 * sheds glow sparks along its heading, then the ribbons fade. */
typedef struct SceneE22LinkBeam {
    s16 reserved00;               /* 0x00 */
    s16 timer;                    /* 0x02 */
    s16 alpha;                    /* 0x04 */
    s16 jointSet;                 /* 0x06 */
    u8 trailA[0xB0];              /* 0x08 */
    u8 trailB[0xB0];              /* 0xB8 */
} SceneE22LinkBeam;

typedef struct SceneE22LinkJoints {
    s16 first, middle, last;
} SceneE22LinkJoints;

/* Swirl ring controller (SceneE22_SwirlRing): rides an actor joint, swells a
 * glow while it sprays swirl ring particles, then fades a halo and ring. */
typedef struct SceneE22SwirlRing {
    s16 x, y, z;                  /* 0x00 */
    s16 reserved06;
    s16 state;                    /* 0x08 */
    s16 timer;                    /* 0x0A */
    s16 glow;                     /* 0x0C */
    s16 ring;                     /* 0x0E */
} SceneE22SwirlRing;

/* Ember column controller (SceneE22_EmberColumn): its spin template and halo
 * colour, and the rising ember particle it sprays. */

/* Twist model controller (SceneE22_TwistModel): a model bolt that rides an
 * actor joint along its heading, sheds twisting trail particles, then
 * draws as a spinning model with glows and flares. */
typedef struct SceneE22TwistModel {
    GteShortVector heading;       /* 0x00 */
    GteShortVector position;      /* 0x08 */
    GteShortVector origin;        /* 0x10 */
    s16 state;                    /* 0x18 */
    s16 timer;                    /* 0x1A */
    s16 reserved1C;               /* 0x1C */
} SceneE22TwistModel;

typedef struct SceneE22ModelMatrix {
    s16 m[3][3];
    s16 reserved12;
    s32 t[3];                     /* 0x14 */
} SceneE22ModelMatrix;

typedef struct SceneE22ModelScale {
    s32 x, y, z, reserved0C;
} SceneE22ModelScale;

#include "pe1/field_pulled_sprite.h"

/* Sweep bolt controller (SceneE22_SweepBolt): the twist model's sibling that
 * sweeps the bolt out from its origin, sheds twist beam particles, then
 * hovers with a ring and halo. */
typedef struct SceneE22SweepBolt {
    GteShortVector heading;       /* 0x00 */
    GteShortVector origin;        /* 0x08 */
    GteShortVector position;      /* 0x10 */
    s16 state;                    /* 0x18 */
    s16 timer;                    /* 0x1A */
    s16 reserved1C;               /* 0x1C */
    s16 sweep;                    /* 0x1E */
} SceneE22SweepBolt;

#include "pe1/field_ring_geometry.h"

#endif
