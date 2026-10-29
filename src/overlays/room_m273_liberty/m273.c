#include "common.h"
#include <stddef.h>
#include <string.h>
#include "pe1/fieldfx_task.h"
#include "m273_class_externs.h"
#include "pe1/asset_find.h"
#include "pe1/field_actor.h"
#include "pe1/field_collision.h"
#include "pe1/field_model_draw.h"
#include "pe1/field_sprite_draw.h"
#include "pe1/field_sprite_params.h"
#include "pe1/field_texture_tables.h"
#include "pe1/field_vector_math.h"
#include "pe1/fieldfx_callback_list.h"
#include "pe1/fieldfx_calls.h"
#include "pe1/fieldfx_task_pool.h"
#include "pe1/floor_polygon.h"
#include "pe1/geom_state.h"
#include "pe1/gte.h"
#include "pe1/gte_sine_table.h"
#include "pe1/gte_types.h"
#include "pe1/psyq_gpu.h"
#include "pe1/psyq_libc.h"
#include "pe1/room_floor.h"
#include "pe1/room_liberty_choice.h"
#include "pe1/scene_archive.h"
#include "pe1/scene_assets.h"
#include "pe1/scene_grab_head.h"

#include "m273_externs.h"
#include "m273_externs_1.h"
#include "m273_externs_2.h"
#include "room_m273.h"
#include "room_m273_boss.h"
#include "room_m273_boss_state.h"
#include "room_m273_effects.h"
#include "room_m273_particles.h"
#include "room_m273_sway.h"
#include "room_m273_sweep.h"

typedef struct { Vector position; unsigned char parameter[4]; } RoomM273ScaledLayerEffect;

typedef RoomM273ThresholdTransform Transform;

typedef RoomM273ScaledLayerOwner Owner;

typedef struct { short x, y, z, pad; } Sprite;

typedef GteMatrix ThresholdTripleMatrix;

typedef GteVector ThresholdTripleVector;

/* Ring band record the pair callback draws: anchor, scale and intensity. */
typedef struct { GteShortVector position; short scale; short pad0A; short intensity; } ThresholdPairBand;

typedef struct RoomM273RingController {
    u8 reserved_00[8];
    FieldActor *actor;
} RoomM273RingController;

typedef RoomM273RisingParticle Particle;

typedef GteMatrix Matrix;

typedef FieldActor Player;

typedef RoomM273PairPoolRecord Record;

typedef GteShortVector Position;

void RoomLib_GrabOnApproach(SceneGrabHead *head) {
    RoomSelectionState *typedState = (RoomSelectionState *)head;
    char *stateBytes = (char *)typedState;
    FieldActor *actor = typedState->actor;
    FieldActor *player = g_PlayerEntity;

    if (player->mode < 18) {
        int frame = actor->anim.parts.integer;

        if (frame < 14) {
            return;
        }
        if (frame < 30) {
            void *origin = actor->render_object.model_matrix.translation;
            int mapAngle;

            Vec3Fixed grabPosition = {
                actor->render_object.matrices[44].translation[0] * 65536,
                0,
                actor->render_object.matrices[44].translation[2] * 65536
            };
            mapAngle = FieldFx_VecToAngle(
                origin, (Vec3Fixed *)actor->render_object.matrices[44].translation);

            if (((FieldFx_VecToAngle(
                      origin, (Vec3Fixed *)g_PlayerEntity->render_object.model_matrix.translation) -
                  mapAngle + 0x40) &
                 0xFFF) < 0x100) {
                if (FieldFx_GetDistanceXZ((Vec3Fixed *)&g_PlayerEntity->pos_x, &grabPosition) <
                    0x180) {
                    typedState->callback = RoomM273_GrabHoldStep;
                    Battle_BeginPlayerAction();
                    {
                        FieldActor *activePlayer = g_PlayerEntity;

                        activePlayer->render_object.animation_source = &actor->render_object;
                        activePlayer->render_object.animation_state = 4;
                        activePlayer->render_object.animation_id = 44;
                        activePlayer->flags |= 0x10000;
                        activePlayer->render_object.flags_9C |= 0x400;
                        activePlayer->render_object.model_matrix = typedState->matrix;
                    }
                    typedState->activated = 1;
                    {
                        FieldActor *positionPlayer = g_PlayerEntity;

                        typedState->savedX = positionPlayer->pos_x;
                        typedState->savedZ = positionPlayer->pos_z;
                    }
                }
            }
            return;
        }
    }

    *((RoomSelectionState *)stateBytes)->signal = 3;
    ((RoomSelectionState *)stateBytes)->callback =
        RoomM273_GrabWaitRelease;
}

void RoomM273_GrabHoldStep(SceneGrabHead *head) {
    RoomSelectionState *state = (RoomSelectionState *)head;
    FieldActor *actor = state->actor;
    RoomPlacementState *stateTail = &state->placement;
    u16 currentFrame;
    u16 previousFrame;

    g_PlayerEntity->render_object.model_matrix = state->matrix;
    currentFrame = (unsigned int)actor->anim.fixed >> 16;
    previousFrame = (unsigned int)actor->anim_prev >> 16;
    g_PlayerEntity->pos_x = state->savedX;
    g_PlayerEntity->pos_z = state->savedZ;

    if (!stateTail->placement_checked) {
        FieldActorState *playerState = g_PlayerEntity->state;
        if (playerState == 0 || playerState->amount <= 0) {
            RoomM273_ReturnPlayerToPlacement(actor, stateTail);
            stateTail->phase_countdown = 0;
            stateTail->placement_checked = 1;
        }
    }

    switch (actor->mode) {
    case 16:
        if ((s16)currentFrame >= 33 && (s16)previousFrame < 33) {
            actor->anim.fixed = 0x3D0000;
        } else if ((s16)currentFrame >= actor->action - 1) {
            *stateTail->signal = 2;
        }
        return;

    case 9:
        if ((s16)previousFrame < 24 && (s16)currentFrame >= 24) {
            if (stateTail->phase_countdown > 0) {
                actor->anim.fixed = 0;
                --stateTail->phase_countdown;
            }
        } else if ((s16)previousFrame <= 0 && (s16)currentFrame > 0) {
            u8 *substate = actor->state->action_state;
            substate[1] = stateTail->pending_action;
        } else if ((s16)previousFrame < 34 && (s16)currentFrame >= 34) {
            s32 delta = ((s32)(s16)g_RoomFloorY->raw * 65536) - actor->pos_y;
            if (delta < 0) {
                delta += 15;
            }
            stateTail->vertical_step = 4;
            stateTail->vertical_step = delta >> stateTail->vertical_step;
            stateTail->vertical_ticks = 16;
        } else if ((s16)previousFrame < 50 && (s16)currentFrame >= 50) {
            if (stateTail->placement_checked == 0) {
                Battle_ReturnToIdle();
                RoomM273_ReturnPlayerToPlacement(actor, stateTail);
            }
        } else if ((s16)previousFrame < 3 && (s16)currentFrame >= 3) {
            FieldActorState *playerState = g_PlayerEntity->state;
            if (playerState != 0 && playerState->amount > 0) {
                playerState->flags |= 0x4000;
                if (actor->state != 0) {
                    actor->state->core_flags |= 0x80000000;
                }
            }
        } else if ((s16)currentFrame >= actor->action - 1) {
            if (actor->state != 0) {
                u8 *substate = actor->state->action_state;
                substate[0] = 4;
            }
            RoomM273_GrabRelease((PmSlotHeader *)state);
        }
        if (stateTail->vertical_ticks != 0) {
            actor->pos_y += stateTail->vertical_step;
            --stateTail->vertical_ticks;
        }
        return;

    case 8:
        if ((s16)currentFrame <= 0) {
            FieldActorState *actorState = actor->state;
            if (actorState != 0) {
                u8 *substate = actorState->action_state;
                substate[0] = 2;
            }
        }
        return;

    default:
        RoomM273_GrabRelease((PmSlotHeader *)state);
        return;
    }
}

void RoomM273_ReturnPlayerToPlacement(RoomPlacementOwner *owner, RoomPlacementState *state) {
    FieldActor *player = g_PlayerEntity;
    RoomPlacementMap *placement;
    PolygonVertex *script = g_FloorPolygonVertices;

    player->render_object.animation_source = 0;
    player->render_object.animation_state = 0;
    player->flags &= ~0x10000;
    player->render_object.flags_9C &= ~0x400;

    placement = (RoomPlacementMap *)owner->render_object.matrices;
    if (Geo_PointInPoly((s32)((u32)placement->x * 65536u), (s32)((u32)placement->z * 65536u),
                      script, g_FloorPolygonVertexCount)) {
        g_PlayerEntity->pos_x = (s32)((u32)placement->x * 65536u);
        g_PlayerEntity->pos_z = (s32)((u32)placement->z * 65536u);
    } else {
        g_PlayerEntity->pos_x = state->fallback_x;
        g_PlayerEntity->pos_z = state->fallback_z;
    }

    Entity_FindFloor((struct BattleEntity *)g_PlayerEntity);

    {
        FieldActor *updatedPlayer = g_PlayerEntity;
        GeomScrollState *scrollState;

        updatedPlayer->base_x = updatedPlayer->pos_x;
        scrollState = &g_GeomScroll.state;
        updatedPlayer->base_y = updatedPlayer->pos_y;
        updatedPlayer->base_z = updatedPlayer->pos_z;
        updatedPlayer->motion_x = 0;
        updatedPlayer->motion_y = 0;
        updatedPlayer->motion_z = 0;
        updatedPlayer->accel_x = 0;
        updatedPlayer->accel_y = 0;
        updatedPlayer->accel_z = 0;
        scrollState->flags |= 0x80;
        state->active = 0;
    }
}

void RoomM273_GrabWaitRelease(SceneGrabHead *head) {
    RoomSelectionState *state = (RoomSelectionState *)head;
    FieldActor *slot = state->actor;

    if (slot->anim.parts.integer >= 0x3B) {
        RoomM273_GrabRelease((PmSlotHeader *)state);
    }
}

int RoomM273_GrabRelease(PmSlotHeader *raw) {
    RoomSelectionState *selection = (void *)raw;
    RoomPlacementState *placementState;
    char *target;
    FieldActorState *actorState;

    actorState = selection->actor->state;
    placementState = &selection->placement;
    /* The animation event can finish before the asynchronous grab slot.
     * Release the slot and player even when its acknowledgement expired. */
    if (actorState != 0 && actorState->action_state != 0) {
        actorState->action_state[0] = 4;
    }

    *selection->signal = 0;
    selection->status = 4;
    if (selection->activated != 0) {
        target = *((void **)g_PlayerEntity);
        if (target != 0) {
            if (*(s16 *)(target + 0xC) > 0) {
                Battle_ReturnToIdle();
            }
        }
        RoomM273_ReturnPlayerToPlacement((RoomPlacementOwner *)selection->actor, placementState);
    }

    return 0;
}

void RoomM273_GrabNop6(void) {
}

void RoomM273_OscillationNop0(void) {
}

void RoomM273_OscillationInit(PmSlotHeader *raw) {
    RoomOscillationEffect *obj = (void *)raw;
    obj->params.angleOut = 0;
    obj->params.distanceOut = 0;
    obj->params.signal = 0;
    obj->params.active = 0;
    return;
}

int RoomM273_OscillationConfigure(PmSlotHeader *raw, int unused, int operation, intptr_t arg3,
                  intptr_t arg4, intptr_t arg5) {
    RoomOscillationEffect *base = (void *)raw;
    RoomOscillationParams *fields = &base->params;

    switch (operation) {
    case 0:
        fields->active = arg3;
        if (arg4 != 0) {
            fields->frame = 0;
        }
        break;
    case 1:
        fields->amplitude = arg3;
        fields->period = arg4;
        fields->baseHeight = arg5;
        break;
    case 2: {
        FieldActor *owner = base->actor;
        int dx = (*(s32 *)arg3 - owner->pos_x) >> 16;
        int dy = (*(s32 *)arg4 - owner->pos_z) >> 16;

        *(s32 *)arg5 = Math_IntSqrt(dx * dx + dy * dy);
        break;
    }
    case 3:
        *(s32 *)arg4 = RoomM273_ChooseWeightedAction(*(s32 *)arg3, (RoomLibertyChoiceInputs *)fields);
        break;
    case 4:
        fields->angleOut = (int *)arg3;
        fields->distanceOut = (int *)arg4;
        break;
    case 5:
        fields->signal = (int *)arg3;
        *(s32 *)arg3 = 1;
        break;
    }
    return 0;
}

int Math_IntSqrt(int squaredDistance);

int RoomM273_OscillationNop3(PmSlotHeader *raw) {
    return 0;
}

int RoomM273_OscillationUpdate(PmSlotHeader *raw) {
    RoomOscillationEffect *effect = (void *)raw;
    int **outs = &effect->params.angleOut;
    FieldActor *actor = effect->actor;

    if (effect->params.active) {
        int frame = effect->params.frame;
        int period = effect->params.period;
        int phase = ((s32)((u32)frame << 12) / period - 2048) & 4095;
        int sine;
        int half;
        FieldActor *target;

        effect->params.frame = frame + 1;
        sine = g_RoomM273Trig[phase].part.cos;
        half = (sine + 4096) / 2;
        sine = (half * effect->params.amplitude) / 4096 + 8;
        target = (effect)->actor;
        {
            int height;

            half = sine * 4096;
            height = effect->params.baseHeight - half;
            target->pos_y = height;
        }
    }

    if (outs[0]) {
        FieldActor *player = g_PlayerEntity;
        int angle = ratan2(actor->pos_x - player->pos_x,
                                  actor->pos_z - player->pos_z);

        *outs[0] = (angle - actor->rot_y) & 4095;
    }

    if (outs[1]) {
        FieldActor *player = g_PlayerEntity;
        int dx = (player->pos_x - actor->pos_x) >> 16;
        int dz = (player->pos_z - actor->pos_z) >> 16;

        *outs[1] = Math_IntSqrt(dx * dx + dz * dz);
    }

    return 0;
}

int RoomM273_OscillationRelease(PmSlotHeader *raw) {
    RoomOscillationEffect *obj = (void *)raw;
    if (obj->params.signal != 0) {
        obj->params.signal = 0;
    }

    obj->status = 4;
    return 0;
}

void RoomM273_OscillationNop6(void) {
}

s32 RoomM273_ChooseWeightedAction(s32 arg0, RoomLibertyChoiceInputs *inputs) {
    s32 variant;
    s32 distanceBand;
    s32 threshold;
    s32 i;
    s32 sum;
    u8 *weights;

    variant = (*inputs->variantBits >> 10) & 3;
    distanceBand = *inputs->distance;

    if (distanceBand < 0x280) {
        distanceBand = 0;
    } else if (distanceBand < 0x600) {
        distanceBand = 1;
    } else {
        distanceBand = 2;
    }

    weights = g_RoomM273ActionWeights + (arg0 * 84) + (distanceBand * 28) + (variant * 7);
    threshold = ((Inv_ScrambleGrid() & 0xFF) * 100) / 0x100;

    i = 0;
    sum = 0;
    while (i < 6) {
        sum += weights[i];
        if (threshold < sum) {
            return i;
        }
        i++;
    }

    return i;
}

int RoomM273_GrabPlacementFanCallback(int mode, void *data, intptr_t argument) {
    RoomM273SampledLayerEffect * effect = data;
    RoomM273Vector position, rotation;
    if (mode == 1) {
        if (g_FieldFxCurrentAge >= 8) return 1;
    } else if (mode == 2) {
        short frame = g_FieldFxCurrentAge - 1;
        int offset = ((unsigned int)(u32)frame << 9) & 0x3e00;
        int product = ((short *)g_RoomM273Trig)[offset / 2] * 2 + 4096;
        int sample = ((short *)g_RoomM273Trig)[offset / 2 + 1];
        short size = product;
        short shade = (unsigned int)sample >> 5;
        short i;
        rotation.x = 1024;
        rotation.y = 0;
        rotation.z = frame * 170;
        rotation.pad = -1;
        position = effect->position;
        i = 0;
        do {
            FieldFx_DrawStarFan(&position, 128, 128, 12, (GteRotation *)&rotation,
                          size, size, (RenderColor *)g_RoomM273BlackColor,
                          (RenderColor *)effect->parameter, shade, 1);
            shade = shade >> 1;
            size = size * 3 / 2;
            i++;
            position.y = g_RoomFloorY->raw;
        } while (i < 2);
    }
    return 0;
}

int RoomM273_GrabPlacementFanEmitter(int mode, void *task_data, intptr_t argument) {
    switch(mode) {
    case 0:
        return FieldFx_InitCallbackList(ANIM_POOL_AS(RoomM273PoolContext)->pool,12,9,
                             RoomM273_GrabPlacementFanCallback);
    case 1: {
        RoomM273SampledLayerEffect *effect;
        RoomPlacementMap *transform;
        if(g_FieldFxCurrentAge>=12) return 2;
        effect=FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
        if(!effect) break;
        transform=(RoomPlacementMap *)g_FieldFxOwner->actor->render_object.matrices;
        effect->position.x=transform->x;
        effect->position.y=transform->y;
        effect->position.z=transform->z;
        effect->parameter[0]=((short *)g_RoomM273Trig)[((g_FieldFxCurrentAge*1024/12)&0xFFF)*2]*40/4096;
        effect->parameter[1]=8;
        effect->parameter[2]=g_RoomM273Trig[(g_FieldFxCurrentAge*1024/12)&0xFFF].part.cos*40/4096;
        break;
    }
    case 2:
        break;
    }
    return 0;
}

/* The emitter owns the twelve-byte records rendered by this callback. */

int RoomM273_GrabPlacementScaledFanCallback(int mode, void *data, intptr_t argument) {
    RoomM273ScaledLayerEffect * effect = data;
    Vector position,rotation;
    /* Layout only: purpose of these eight original frame bytes is unknown. */
    int unknownFrame[2];
    if(mode==1) {
        if(g_FieldFxCurrentAge>=8) return 1;
    } else if(mode==2) {
        short frame=g_FieldFxCurrentAge-1;
        /* The input vector's fourth halfword supplies the initial scale. */
        int product=(g_RoomM273Trig[((unsigned int)frame<<7)&0xF80].part.sin*2+4096)*effect->position.pad;
        int sample=g_RoomM273Trig[((unsigned int)frame<<7)&0xF80].word;
        short size=product/4096;
        short shade=sample>>21;
        short i;
        rotation.x=1024; rotation.y=0; rotation.z=frame*170; rotation.pad=-1;
        position=effect->position;
        i=0;
        do {
            FieldFx_DrawStarFan(&position,128,128,12,(GteRotation *)&rotation,size,size,(struct RenderColor *)g_RoomM273BlackColor,(struct RenderColor *)effect->parameter,shade,1);
            shade>>=1;
            size=size*3/2;
            i++;
            position.y=g_RoomFloorY->raw;
        } while(i<2);
    }
    return 0;
}

int RoomM273_GrabPlacementScaledFanEmitter(int mode, void *task_data, intptr_t argument) {
    /* Layout only: purpose of these eight original frame bytes is unknown. */
    int unknownFrame[2];
    switch(mode) {
    case 0:
        return FieldFx_InitCallbackList(ANIM_POOL_AS(RoomM273PoolContext)->pool,12,9,
                             RoomM273_GrabPlacementScaledFanCallback);
    case 1: {
        RoomM273ScaledLayerEffect *effect;
        Transform *transform;
        int scale;
        if(g_FieldFxCurrentAge>=12) return 2;
        effect=FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
        if(!effect) break;
        transform=(Transform *)g_FieldFxOwner->actor->render_object.matrices;
        effect->position.x=transform->x;
        effect->position.y=transform->y;
        effect->position.z=transform->z;
        scale=g_RoomM273Trig[(g_FieldFxCurrentAge*1024/12)&0xFFF].part.cos;
        effect->parameter[0]=40;
        effect->position.pad=scale;
        effect->parameter[1]=((short *)g_RoomM273Trig)[((g_FieldFxCurrentAge*1024/12)&0xFFF)*2]*40/4096;
        effect->parameter[2]=0;
        break;
    }
    case 2:
        break;
    }
    return 0;
}

int RoomM273_GrabPlayerPulseCallback(int mode, void *data, intptr_t argument) {
    RoomM273SpritePoolEffect * state = data;
    if (mode == 1) {
        if (g_FieldFxCurrentAge >= 16) return 1;
    } else if (mode == 2) {
        int index = ((u32)(g_FieldFxCurrentAge - 1) << 8) & 0x3f00;
        Sprite sprite;
        int product;
        int sample;
        short size;
        short shade;
        RoomM273WorldPosition *p = state->position;
        int kind;
        int palette;
        int x;

        product = ((short *)g_RoomM273Trig)[index / 2] * 2 + 4096;
        size = product;
        sample = ((short *)g_RoomM273Trig)[index / 2 + 1];
        shade = (unsigned int)sample >> 5;
        x = p->x;
        sprite.y = p->y;
        sprite.z = p->z;
        sprite.x = x;
        kind = g_FieldFxSpriteParams.palette;
        palette = g_FieldFxPaletteTable[kind];
        FieldFx_DrawShapeQuads((GteShortVector *)&sprite, &g_RoomM273FloorRotation, size, size, 102,
                        GetClut(0, (kind == 4 && g_FieldFxAltPaletteFlag) ? palette + 9 : palette + 5), 1, shade, 0);
    }
    return 0;
}

int RoomM273_GrabPlayerPulseEmitter(int mode, void *task_data, intptr_t argument) {
    switch (mode) {
    case 0:
        g_RoomM273PlayerPulseSpawned = 0;
        return FieldFx_InitCallbackList(ANIM_POOL_AS(RoomM273PoolContext)->pool, sizeof(void *), 1,
                             RoomM273_GrabPlayerPulseCallback);
    case 1:
        if (g_FieldFxCurrentAge >= 20) return 2;
        if (g_RoomM273PlayerPulseSpawned) return 2;
        if (g_PlayerEntity->mode < 4) {
            RoomM273SpritePoolEffect *effect =
                FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
            if (effect) {
                RoomM273PlayerTransform *transform = (RoomM273PlayerTransform *)g_PlayerEntity->render_object.matrices;
                g_RoomM273PlayerPulseSpawned = 1;
                effect->position = &transform->position;
            }
        }
        break;
    case 2:
        if (g_RoomM273PlayerPulseSpawned) {
            int index = g_FieldFxTpageSlot11;
            int palette;
            g_FieldFxSpriteParams.parameter00 = 32;
            g_FieldFxSpriteParams.parameter02 = 2;
            g_FieldFxSpriteParams.extent_x = 32;
            g_FieldFxSpriteParams.extent_y = 32;
            g_FieldFxSpriteParams.extent_x = 32;
            g_FieldFxSpriteParams.extent_y = 32;
            palette = g_FieldFxTpageTable[index];
            g_FieldFxSpriteParams.palette = 3;
            g_FieldFxSpriteParams.parameter06 = 1;
            g_FieldFxSpriteParams.parameter0A = 0;
            g_FieldFxSpriteParams.depth = 0;
            g_FieldFxSpriteParams.tpage = palette;
        }
        break;
    }
    return 0;
}

void *Asset_FindTable08ByU32Key(void *base, s32 key);

int RoomM273_GrabModelRingCallback(int mode, void *data, intptr_t argument) {
    ThresholdPairBand ** arg1 = data;
    short params[4];
    short i;
    ThresholdPairBand **objp = arg1;
    short *x;
    short *x_next;
    int *z;
    int *z_next;

    if (mode == 1) {
        if (g_FieldFxCurrentAge < 0x20) {
            goto ret0;
        }
        return 1;
    }
    if (mode != 2) {
        return 0;
    }

    i = 0;
    x = g_RoomM273GrabModelRingX;
    x_next = x + 1;
    z = g_RoomM273GrabModelRingZ;
    z_next = z + 1;

    params[0] = 0x400;
    params[1] = g_FieldFxCurrentAge << 7;
    params[2] = 0;
    params[3] = 1;

    do {
        FieldFx_DrawRingBand(&(*objp)->position, x[i], x_next[i], 0x10, (GteRotation *)params,
                      (*objp)->scale, (*objp)->scale,
                      (RenderColor *)&z[i], (RenderColor *)&z_next[i],
                      (*objp)->intensity, 1);
        i++;
    } while (i < 2);

ret0:
    return 0;
ret1:
    return 1;
}

int RoomM273_GrabModelCallback(int mode, void *data, intptr_t argument) {
    ThresholdTripleEffect * effect = data;
    ThresholdTripleMatrix matrix;
    ThresholdTripleVector copy,scale;
    if(mode==1) {
        int frame=g_FieldFxCurrentAge;
        int *wave;
        int *shifted;
        int *table;
        unsigned int index;
        if(frame>=32) return 1;
        frame*=32;
        index=frame&0xFFF;
        table=(int *)g_RoomM273Trig;
        wave=&table[index];
        shifted=&table[(frame+1024)&0xFFF];
        {
            int sample=*wave;
            effect->scaleXZ=(short)sample/2;
        }
        effect->scaleY=(((short *)shifted)[1]+4096)*2;
        {
            unsigned int phase=(unsigned short)effect->phase;
            int parameter=((short *)wave)[1];
            effect->phase=phase+256;
            effect->parameter12=parameter>>6;
        }
    } else if(mode==2) {
        u16 flags=GetTPage(0,1,0,0);
        int page=(unsigned short)(g_FieldFxTpageTable[g_FieldFxTpageSlot11]|flags);
        int kind=g_FieldFxSpriteParams.palette,palette;
        int cosine,sine;
        void **asset;
        palette=g_FieldFxPaletteTable[kind];
        FieldFx_ModelSetTexture(page,GetClut(0, (kind == 4 && g_FieldFxAltPaletteFlag) ? palette + 11 : palette + 7));
        FieldFx_ModelSetSemiTrans(1);
        asset=&g_RoomM273GrabModel.asset;
        FieldFx_ModelSaveColors(*asset);
        FieldFx_ModelScaleColors(*asset,effect->parameter12);
        cosine=rcos(effect->phase);
        sine=rsin(effect->phase);
        matrix.m[0][2]=sine; matrix.m[2][0]=-sine;
        matrix.m[0][0]=cosine; matrix.m[2][2]=cosine;
        matrix.t[0]=matrix.t[1]=matrix.t[2]=0;
        matrix.m[0][1]=matrix.m[1][0]=matrix.m[1][2]=matrix.m[2][1]=0;
        matrix.m[1][1]=4096;
        memset(&scale,0,16);
        scale.x=effect->scaleXZ; scale.y=effect->scaleY; scale.z=effect->scaleXZ;
        copy=scale;
        ScaleMatrix(&matrix,&copy);
        {
            int x=effect->x;
            void *model=*asset;
            matrix.t[0]=x; matrix.t[1]=effect->y; matrix.t[2]=effect->z;
            FieldFx_ModelDraw(model,&matrix);
        }
        FieldFx_ModelRestoreColors(*asset);
    }
    return 0;
}

int RoomM273_GrabModelEmitter(int mode, void *task_data, intptr_t argument) {
    switch(mode) {
    case 0: {
        int size;
        g_RoomM273GrabModel.asset=Asset_FindTable08ByU32Key(g_LoadedSceneAssetBlock[0],0xC5941704);
        FieldFx_ModelScrollUv(g_RoomM273GrabModel.asset,0,0);
        size=FieldFx_InitCallbackList(ANIM_POOL_AS(RoomM273PoolContext)->pool,16,6,
                           RoomM273_GrabModelCallback);
        if (size < 0) return size;
        int extra = FieldFx_AllocCallbackList(&g_RoomM273GrabModel.pool,size,sizeof(void *),2,RoomM273_GrabModelRingCallback);
        if (extra < 0) return extra;
        return size + extra;
    }
    case 1: {
        RoomM273ThresholdEntityState *state;
        if(g_RoomM273GrabEffectStopped) return 2;
        state=ANIM_OWNER_AS(RoomM273EffectStateContext)->state.threshold;
        if(state->mode==9) {
            unsigned short value=((u32)state->anim_prev >> 16);
            if(state->anim.parts.integer>0 && (short)value<=0) {
                short i=0;
                do {
                    ThresholdTripleEffect *effect=FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
                    if(!effect) break;
                    effect->x=state->render_object.matrices[44].translation[0];
                    effect->y=g_RoomFloorY->raw;
                    effect->z=state->render_object.matrices[44].translation[2];
                    effect->phase=i*384;
                    if(i==0) *(ThresholdTripleEffect **)FieldFx_AcquireCallbackSlot(g_RoomM273GrabModel.pool)=effect;
                    i++;
                } while(i<3);
            }
        }
        FieldFx_UpdateCallbackList(g_RoomM273GrabModel.pool);
        break;
    }
    case 2:
        FieldFx_DrawCallbackList(g_RoomM273GrabModel.pool);
        break;
    }
    return 0;
}

int RoomM273_GrabPlayerStreakCallback(int mode, void *data, intptr_t argument) {
    GteRotation * rotation = data;
    GteShortVector position;
    int frame, size, kind, palette;
    u16 clut;

    if (mode == 1) {
        if (g_FieldFxCurrentAge >= 16) return 1;
    } else if (mode == 2) {
        RoomM273PlayerActorView *player;
        /* The loop index is reused for the sampled scale below. */
        size = 0;
        player = (RoomM273PlayerActorView *)g_PlayerEntity;
        for (; size < 3; size++) {
            RoomM273PlayerTransform *transform = (RoomM273PlayerTransform *)player->render_object.matrices;
            ((s16 *)&position)[size] = ((s32 *)&transform->position.x)[size];
        }

        frame = g_FieldFxCurrentAge - 1;
        kind = g_FieldFxSpriteParams.palette;
        size = g_RoomM273Trig[((u32)frame << 6) & 0xFC0].part.sin;
        palette = g_FieldFxPaletteTable[kind];

        clut = GetClut(0, (kind == 4 && g_FieldFxAltPaletteFlag) ? palette + 11 : palette + 7);
        FieldFx_DrawShapeQuads(&position, rotation, 8192, size * 3, 5, clut, 1,
                       /* The upper half of the packed trig entry is signed. */
                       (s16)(g_RoomM273Trig[((u32)frame << 6) & 0xFC0].word >> 16) >> 5, 0);
    }
    return 0;
}

int RoomM273_GrabPlayerStreakEmitter(int mode, void *task_data, intptr_t argument) {
    switch (mode) {
    case 0:
        return FieldFx_InitCallbackList(ANIM_POOL_AS(RoomM273PoolContext)->pool, 8, 4,
                             RoomM273_GrabPlayerStreakCallback);
    case 1: {
        RoomM273EffectModeState *state;
        if (g_RoomM273GrabEffectStopped) return 2;
        state = ANIM_OWNER_AS(RoomM273EffectStateContext)->state.mode;
        if (state->mode == 9) {
            unsigned short value = ((u32)state->anim_prev >> 16);
            if (state->anim.parts.integer > 0 && (short)value <= 0) {
                short i = 0;
                int angle = (s32)((u32)Inv_ScrambleGrid() << 4);
                do {
                    RoomM273PulseSeed *seed = FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
                    if (!seed) break;
                    *seed = g_RoomM273PlayerStreakSeeds[i];
                    i++;
                    seed->angle += angle;
                } while (i < 2);
            }
        }
        break;
    }
    case 2: {
        int index;
        int palette;
        g_FieldFxSpriteParams.parameter00 = 16;
        g_FieldFxSpriteParams.extent_x = 16; g_FieldFxSpriteParams.extent_y = 16; g_FieldFxSpriteParams.extent_x = 16;
        index = g_FieldFxTpageSlot11;
        g_FieldFxSpriteParams.parameter02 = 1; g_FieldFxSpriteParams.extent_y = 64;
        palette = g_FieldFxTpageTable[index];
        g_FieldFxSpriteParams.palette = 3; g_FieldFxSpriteParams.parameter06 = 1; g_FieldFxSpriteParams.parameter0A = 3; g_FieldFxSpriteParams.depth = 0;
        g_FieldFxSpriteParams.tpage = palette;
        break;
    }
    }
    return 0;
}

int RoomM273_GrabPlayerTrailCallback(int mode, void *data, intptr_t argument) {
    Vector * input = data;
    Vector *position = input;
    if (mode == 1) {
        if (g_FieldFxCurrentAge >= 9) return 1;
    } else if (mode == 2) {
        int firstFrame = g_FieldFxCurrentAge - 1;
        int frame;
        unsigned int offset;
        int kind = g_FieldFxSpriteParams.palette;
        s16 size;
        int palette;
        unsigned short clut;

        offset = ((unsigned int)firstFrame << 9) & 0x3E00;
        frame = firstFrame;
        size = g_RoomM273Trig[offset / 4].part.sin * 2 + 4096;

        palette = g_FieldFxPaletteTable[kind];
        clut = GetClut(0, (kind == 4 && g_FieldFxAltPaletteFlag) ? palette + 9 : palette + 5);
        FieldFx_DrawShapeQuads(position, 0, size, size, 102, clut, 1,
            (short)g_RoomM273Trig[((unsigned int)(u32)frame << 8) & 0xF00].word >> 6, 0);
    }
    return 0;
}

int RoomM273_GrabPlayerTrailEmitter(int mode, void *task_data, intptr_t argument) {
    switch(mode) {
    case 0:
        g_RoomM273PlayerTrailBurstsLeft=0; g_RoomM273PlayerTrailDelay=0;
        return FieldFx_InitCallbackList(ANIM_POOL_AS(RoomM273PoolContext)->pool,8,4,
                             RoomM273_GrabPlayerTrailCallback);
    case 1: {
        RoomM273EffectModeState *state;
        if(g_RoomM273GrabEffectStopped) return 2;
        state=ANIM_OWNER_AS(RoomM273EffectStateContext)->state.mode;
        if(state->mode==9) {
            unsigned short value=((u32)state->anim_prev >> 16);
            if(state->anim.parts.integer>=2 && (short)value<2) {
                g_RoomM273PlayerTrailBurstsLeft=4; g_RoomM273PlayerTrailDelay=0;
            }
        }
        if(g_RoomM273PlayerTrailBurstsLeft) {
            short old=g_RoomM273PlayerTrailDelay--;
            if(old<=0) {
                Vector *position=FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
                if(position) {
                    RoomM273PlayerTransform *transform=(RoomM273PlayerTransform *)g_PlayerEntity->render_object.matrices;
                    position->x=transform->position.x;
                    position->y=transform->position.y;
                    position->z=transform->position.z;
                    g_RoomM273PlayerTrailDelay=2;
                    g_RoomM273PlayerTrailBurstsLeft--;
                }
            }
        }
        break;
    }
    case 2: {
        int index=g_FieldFxTpageSlot11;
        int palette;
        g_FieldFxSpriteParams.parameter00=32; g_FieldFxSpriteParams.parameter02=2;
        g_FieldFxSpriteParams.extent_x=32; g_FieldFxSpriteParams.extent_y=32; g_FieldFxSpriteParams.extent_x=32; g_FieldFxSpriteParams.extent_y=32;
        palette=g_FieldFxTpageTable[index];
        g_FieldFxSpriteParams.palette=3; g_FieldFxSpriteParams.parameter06=1; g_FieldFxSpriteParams.parameter0A=0; g_FieldFxSpriteParams.depth=32;
        g_FieldFxSpriteParams.tpage=palette;
        break;
    }
    }
    return 0;
}

int RoomM273_GrabStarBurstCallback(int mode, void *data, intptr_t argument) {
    RoomM273PaletteInput * input = data;
    const GteSineWord *entry;
    RoomM273PaletteEffect *effect;
    GteShortVector position;
    GteVector *source;
    int x;
    int y;
    int z;
    GteRotation rotation;
    int random;

    if (mode == 1) {
        int frame;
        frame = g_FieldFxCurrentAge;
        if (frame >= 33) return 1;
        entry = &g_RoomM273Trig[(((unsigned int)(u32)frame << 7) & 0x3F80) / sizeof(RoomM273TrigEntry)];
        input->size = (entry->part.sin * 3 * 2048) / 4096 + 4096;
        if (frame >= 24) return 0;
        effect = FieldFx_AcquireCallbackSlot(g_RoomM273GrabSparkPool);
        if (!effect) return 0;
        effect->source = input->source;
        effect->size = (input->size * 3 * 256) / 4096;
        effect->depth = Inv_ScrambleGrid() + 256;
        effect->color = g_RoomM273StarBurstColors[g_FieldFxCurrentAge & 3];
        effect->x = (s32)((u32)Inv_ScrambleGrid() << 4);
        random = Inv_ScrambleGrid();
        effect->y = (128 - random) >> 2;
    } else if (mode == 2) {
        int frame;
        source = input->source;
        frame = g_FieldFxCurrentAge;
        y = source->y;
        x = source->x;
        frame = frame - 1;
        position.y = y;
        z = source->z;
        entry = &g_RoomM273Trig[(((unsigned int)(u32)frame << 7) & 0x3F80) / sizeof(RoomM273TrigEntry)];
        position.z = z;
        position.x = x;
        rotation.x = 0;
        rotation.y = 0;
        rotation.z = 170 * frame;
        rotation.flags = 0;
        FieldFx_DrawStarFan(&position, 256, 256, 12, &rotation,
                       input->size, input->size,
                       (RenderColor *)g_RoomM273BlackColor,
                       (RenderColor *)&g_RoomM273GrabColors[g_RoomM273GrabColorIndex],
                       entry->part.cos >> 5, 1);
    }
    return 0;
}

int RoomM273_GrabStarBurstEmitter(int mode, void *task_data, intptr_t argument) {
    switch (mode) {
    case 0: {
        int size = FieldFx_InitCallbackList(ANIM_POOL_AS(RoomM273PoolContext)->pool, sizeof(RoomM273PaletteInput), 2,
                                 RoomM273_GrabStarBurstCallback);
        if (size < 0) return size;
        int extra = FieldFx_AllocCallbackList(&g_RoomM273GrabSparkPool, size, sizeof(RoomM273PulseState), 9, RoomM273_GrabSparkTriangleCallback);
        if (extra < 0) return extra;
        return size + extra;
    }
    case 1: {
        RoomM273EffectModeState *state;
        if (g_RoomM273GrabEffectStopped) return 2;
        state = ANIM_OWNER_AS(RoomM273EffectStateContext)->state.mode;
        if (state->mode == 9) {
            unsigned short value = ((u32)state->anim_prev >> 16);
            if (state->anim.parts.integer >= 4 && (short)value < 4) {
                RoomM273PointerPoolEffect *effect = (RoomM273PointerPoolEffect *)FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
                if (effect) effect->position = g_PlayerEntity->render_object.matrices->translation;
            }
        }
        FieldFx_UpdateCallbackList(g_RoomM273GrabSparkPool);
        break;
    }
    case 2:
        FieldFx_DrawCallbackList(g_RoomM273GrabSparkPool);
        break;
    }
    return 0;
}

int RoomM273_GrabHoldFlareCallback(int mode, void *data, intptr_t argument) {
    GteShortVector * input = data;
    GteRotation rotation;
    GteShortVector position;
    int frame;
    const GteSineWord *entry;
    int size;
    s16 *indexPtr;
    RenderColor *sprite;

    if (mode == 1) {
        if (g_FieldFxCurrentAge >= 16) return 1;
    } else if (mode == 2) {
        frame = (s16)(g_FieldFxCurrentAge - 1);
        entry = &g_RoomM273Trig[(((unsigned int)(u32)frame << 8) & 0x3F00) / sizeof(RoomM273TrigEntry)];
        size = (entry->part.sin * 5 * 2048) / 4096 + 2048;
        position = *input;
        indexPtr = &g_RoomM273GrabColorIndex;
        sprite = (RenderColor *)g_RoomM273BlackColor;
        rotation.x = 0;
        rotation.y = 0;
        rotation.z = frame * 204;
        rotation.flags = 0;
        FieldFx_DrawStarFan(&position, 160, 160, 10, &rotation, (s16)size, (s16)size,
                       sprite, (RenderColor *)&g_RoomM273GrabColors[*indexPtr], entry->part.cos >> 5, 1);

        position.y = g_RoomFloorY->raw;
        rotation.x = 1024;
        rotation.flags = 1;
        FieldFx_DrawStarFan(&position, 256, 256, 10, &rotation,
                       (s16)size >> 1, (s16)size >> 1,
                       (RenderColor *)&g_RoomM273GrabColors[*indexPtr], sprite, entry->part.cos >> 5, 1);
    }
    return 0;
}

int RoomM273_GrabEffectController(int mode, void *task_data, intptr_t argument) {
    FieldActor *obj;
    unsigned char *state;
    unsigned char obj_mode;
    unsigned short frame;
    unsigned short timer;
    RenderMatrix *data;

    if (mode != 1) {
        if (mode >= 2) {
            return 0;
        }
        if (mode != 0) {
            return 0;
        }
        g_RoomM273GrabEffectStopped = 0;

    }
    else
    {
mode_one:
    obj = g_FieldFxOwner->actor;
    state = obj->state->action_state;
    frame = obj->anim.parts.integer;
    /* High half of anim_prev, the previous whole frame. */
    timer = ((u32)obj->anim_prev >> 16);

    /* The native animation engine releases its event record before an
     * asynchronous visual controller necessarily finishes. Only the event
     * acknowledgement depends on that record; its visual timing does not. */
    if (state && *state == 1) {
        *state = 2;
    }

    obj_mode = obj->mode;
    if (obj_mode != 9) {
        if (obj_mode == 0x10) {
            if ((unsigned int)(frame - 0x23) < 0x19) {
                g_RoomM273GrabEffectStopped = 1;
            }
        }
    } else if ((short)frame >= 0x21) {
        g_RoomM273GrabEffectStopped = 1;
    } else if ((short)frame > 0 && ((s32)((u32)timer << 16) <= 0)) {
        data = obj->render_object.matrices;
        Asset_Find08w(0x5CF, obj->state->progress,
                      (short)data[44].translation[0], (short)data[44].translation[1],
                      (short)data[44].translation[2]);
    }

    if (g_RoomM273GrabEffectStopped != 0) {
        return 1;
    }

    }
ret_zero:
    return 0;

ret_one:
    return 1;
}

/* Falling trail record: spins down from the boss, records where it lands or
 * touches the player, and draws a fading two-point trail ring behind it. */
int RoomM273_HandTrailCallback(int mode, void *data, intptr_t argument) {
    RoomM273FallingTrail * trail = data;
    GteMatrix matrix;
    GteShortVector vector;
    GteShortVector point;
    GteRotation rotation;
    s16 *source;
    int i;
    s16 floor;
    s16 intensity;
    s16 scale;

    if (mode == 1) {
        if (trail->landed) {
            return --trail->count <= 0;
        }
        if (trail->spin) {
            trail->yaw += trail->spin;
            trail->spin += trail->spin < 0 ? -2 : 2;
        }
        vector.x = trail->pitch;
        vector.y = trail->yaw;
        vector.z = 0;
        RotMatrixYXZ(&vector, &matrix);
        for (i = 0, source = &trail->position.x; i < 3; i++) {
            matrix.t[i] = *source++;
        }
        gte_ldrotmatrix(&matrix);
        gte_ldtransmatrix(&matrix);
        gte_ldv0(&g_RoomM273HandTrailDropOffset);
        gte_rtv0tr_mac();
        gte_stsv(&trail->position);
        floor = g_RoomFloorY->raw;
        if (trail->position.y >= floor - 0x80) {
            trail->position.y = floor;
            trail->landed = 1;
        }
        if (trail->position.y >= g_RoomM273HandTrailState.floor && g_PlayerEntity->mode >= 4) {
            int dx = g_PlayerEntity->render_object.model_matrix.translation[0] - trail->position.x;
            int dz = g_PlayerEntity->render_object.model_matrix.translation[2] - trail->position.z;
            if (Math_IntSqrt(dx * dx + dz * dz) < 0x140) {
                trail->landed = 1;
                source = &trail->position.x;
                for (i = 0; i < 3; i++) {
                    g_RoomM273HandTrailState.hit[i] = (g_PlayerEntity->render_object.matrices->translation[i] + source[i]) / 2;
                }
                g_RoomM273HandTrailState.hit_flag = 1;
                g_PlayerEntity->state->flags |= 0x4000;
                if (ANIM_OWNER_AS(RoomM273BossActor)->instance->state) {
                    ANIM_OWNER_AS(RoomM273BossActor)->instance->state->core_flags |= 0x80000000;
                }
            }
        }
        if (trail->landed) {
            i = g_RoomM273HandTrailState.landing_count++;
            g_RoomM273HandTrailState.landing_x[i] = trail->position.x;
            g_RoomM273HandTrailState.landing_y[i] = trail->position.y;
            g_RoomM273HandTrailState.landing_z[i] = trail->position.z;
        }
        trail->head = (trail->head - 1) & 7; i = trail->head;
        gte_ldv0(&g_RoomM273HandTrailSampleOffsetA);
        gte_rtv0tr_mac();
        gte_stsv(&vector);
        trail->trail_y[i] = vector.y;
        i *= 2;
        trail->trail_x[i] = vector.x;
        trail->trail_z[i] = vector.z;
        gte_ldv0(&g_RoomM273HandTrailSampleOffsetB);
        gte_rtv0tr_mac();
        gte_stsv(&vector);
        i++;
        trail->trail_x[i] = vector.x;
        trail->trail_z[i] = vector.z;
        if (trail->count < 8) {
            trail->count++;
        }
    } else if (mode == 2) {
        if (!trail->landed) {
            int kind;
            int palette;
            int index = TEXTURE_SLOT_RECORD(g_FieldFxTpageSlot3);

            g_FieldFxSpriteParams.parameter00 = 0x20;
            g_FieldFxSpriteParams.parameter02 = 2;
            g_FieldFxSpriteParams.extent_x = 0x20;
            g_FieldFxSpriteParams.extent_y = 0x20;
            g_FieldFxSpriteParams.tpage = g_FieldFxTpageTable[index];
            g_FieldFxSpriteParams.palette = 3;
            g_FieldFxSpriteParams.parameter06 = 0;
            rotation.x = trail->pitch + 0x400;
            rotation.y = trail->yaw;
            rotation.z = -0x400;
            rotation.flags = 1;
            if (g_FieldFxCurrentAge < 17) {
                scale = g_RoomM273Trig[(g_FieldFxCurrentAge << 8 & 0x3F00) >> 2].bits.sin * 3 * 2048 / 4096 + 0x800;
            } else {
                scale = 0x2000;
            }
            kind = g_FieldFxSpriteParams.palette;
            palette = g_FieldFxPaletteTable[kind];
            if (kind == 4 && g_FieldFxAltPaletteFlag) {
                palette += 4;
            }
            FieldFx_DrawShapeQuads(&trail->position, &rotation, scale, 0x1000, 0xC,
                          GetClut(0x30, palette), 1, 0x80, 0);
            intensity = 0x80;
        } else {
            intensity = trail->count * 16;
        }
        scale = (0x80 - intensity) * 64 + 0x1000;
        g_FieldFxSpriteParams.parameter00 = 0x10;
        g_FieldFxSpriteParams.parameter02 = 1;
        g_FieldFxSpriteParams.extent_x = 0x10;
        g_FieldFxSpriteParams.extent_y = 0x10;
        g_FieldFxSpriteParams.tpage = g_FieldFxTpageTable[g_FieldFxTpageSlot2];
        g_FieldFxSpriteParams.palette = 2;
        g_FieldFxSpriteParams.parameter06 = 0;
        rotation.x = 0;
        rotation.y = 0;
        rotation.z = g_FieldFxCurrentAge << 8;
        rotation.flags = 0;
        {
            s16 step;
            s16 index = trail->head;

            for (step = 0; step < trail->count; step++, index = (index + 1) & 7) {
                s16 side;

                point.y = trail->trail_y[index];
                for (side = 0; side < 2; side++) {
                    int kind;
                    int palette;

                    point.x = trail->trail_x[index * 2 + side];
                    point.z = trail->trail_z[index * 2 + side];
                    kind = g_FieldFxSpriteParams.palette;
                    palette = g_FieldFxPaletteTable[kind];
                    if (kind == 4 && g_FieldFxAltPaletteFlag) {
                        palette += 4;
                    }
                    FieldFx_DrawShapeQuads(&point, &rotation, scale, scale, 0xDC,
                                  GetClut(0x30, palette), 1, intensity, &g_RoomM273HandTrailColor);
                    rotation.y += 0x800;
                }
                intensity -= 0x10;
                scale += 0x400;
                rotation.z += 0x100;
            }
        }
    }
    return 0;
}

int RoomM273_HandTrailEmitter(int mode, void *task_data, intptr_t argument) {
    s16 nextIndex;
    FieldActor *actor;
    FieldActor *candidate;
    s16 size;
    s16 i;
    s32 pulse;
    s16 *pathAddress;
    s32 pathOffset;

    switch (mode) {
    case 0: {
        g_RoomM273HandTrailPhase = 0;
        g_RoomM273HandTrailCooldown = 0;
        g_RoomM273HandTrailPeriodA = (Inv_ScrambleGrid() & 7) + 4;
        g_RoomM273HandTrailPeriodB = (Inv_ScrambleGrid() & 7) + 4;
        return FieldFx_InitCallbackList(ANIM_POOL_AS(RoomM273PoolContext)->pool, 100, 10,
                             RoomM273_HandTrailCallback);
    }
    case 1: {
        s16 pathY[2];
        RenderMatrix *matrices;

        if (g_RoomM273HandTrailState.done)
            return 2;
        candidate = ANIM_OWNER_AS(RoomM273RingController)->actor;
        if (candidate->mode != 11)
            return 0;
        actor = candidate;
        {
            u16 previousFrame =
                ((u32)actor->anim_prev >> 16);
        if ((s16)actor->anim.parts.integer < 6 ||
            (s16)previousFrame >= 35)
            return 0;
        }
        {
            s16 countdown = g_RoomM273HandTrailCooldown;
            g_RoomM273HandTrailCooldown = countdown - 1;
            if (countdown > 0)
                return 0;
        }

        if (g_RoomM273HandTrailState.spinning == 0)
            goto deterministicPattern;
        if (g_RoomM273HandTrailState.spinning == mode)
            goto randomPattern;
        goto phasePattern;

deterministicPattern: {
            s16 phase = g_RoomM273HandTrailPhase;
            s16 offset;
            pulse = (s32)((u32)(u16)phase << 16);
            offset = (pulse >> 18) * 32 - 0x300;
            phase &= 3;
            pathY[1] = offset;
            pathY[0] = offset;
            pulse = (pulse >> 15) + 0x80;
            g_RoomM273HandTrailDropOffset.z = pulse;
            size = phase << 6;
            goto particleSetup;
        }
randomPattern: {
            int random = Inv_ScrambleGrid() & 0x3F;
            int center = 0x20 - random;
                if (g_RoomM273HandTrailPhase & 1) {
                    pathY[0] = center - 0x340;
                    pathY[1] = center - 0x2A0;
                } else {
                    pathY[1] = center - 0x2A0;
                    pathY[0] = center - 0x340;
            }
            size = 0x10;
            g_RoomM273HandTrailDropOffset.z = 0x80;
            goto particleSetup;
        }
phasePattern: {
            u16 phase = g_RoomM273HandTrailPhase;
            s16 offset;
            offset = (phase & 3) * 24;
                if (phase & 1) {
                    s16 pathValue;
                    pathValue = offset - 0x3C0;
                    pathY[0] = pathValue;
                    pathValue = offset - 0x2A0;
                    pathY[1] = pathValue;
                } else {
                    s16 pathValue;
                    pathValue = offset - 0x2A0;
                    pathY[1] = pathValue;
                    pathValue = offset - 0x3C0;
                    pathY[0] = pathValue;
            }
            size = 0x20;
            g_RoomM273HandTrailDropOffset.z = 0x80;
        }

particleSetup:
        i = 0;
        do {
            RoomM273FallingTrail *particle = FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
            if (particle == 0)
                break;
            particle->position.x = g_RoomM273HandTrailState.hands[i].x;
            particle->position.y = g_RoomM273HandTrailState.hands[i].y;
            pathOffset = i * 2;
            particle->position.z = g_RoomM273HandTrailState.hands[i].z;
            particle->yaw = actor->rot_y + size * 4;
            pathAddress = (s16 *)((u8 *)pathY + pathOffset);
            nextIndex = i + 1;
            particle->pitch = *pathAddress;
            i = nextIndex;
            particle->spin = size;
            particle->landed = 0;
            particle->head = 0;
            particle->count = 0;
            size = -size;
        } while (nextIndex < 2);

        g_RoomM273HandTrailCooldown = 6;
        g_RoomM273HandTrailPhase++;
        matrices = actor->render_object.matrices;
        Asset_Find08w(0x5D0, ((int *)actor->state)[2],
                      ((s16 *)matrices[2].translation)[0],
                      ((s16 *)matrices[2].translation)[2],
                      ((s16 *)matrices[2].translation)[4]);
        return 0;
    }
    case 2: {
        u16 drawSize = g_FieldFxSpriteParams.parameter00;
        g_FieldFxSpriteParams.parameter0A = 0;
        g_FieldFxSpriteParams.depth = 0;
        g_FieldFxSpriteParams.extent_x = drawSize;
        g_FieldFxSpriteParams.extent_y = drawSize;
        break;
    }
    }
    return 0;
}

int RoomM273_HandTrailLandingPulseCallback(int mode, void *data, intptr_t argument) {
    RoomM273PulseInput * input = data;
    GteShortVector position;
    int frame;
    int palette;
    int size;
    int sample;
    u16 clut;

    if (mode == 1) {
        if (g_FieldFxCurrentAge >= 16) return 1;
        input->position.pad += input->velocity;
        input->velocity += 2;
    } else if (mode == 2) {
        frame = g_FieldFxCurrentAge - 1;
        sample = g_RoomM273Trig[(((unsigned int)(u32)frame << 8) & 0x3F00) / sizeof(RoomM273TrigEntry)].bits.sin;
        position = input->position;
        palette = g_FieldFxPaletteTable[g_FieldFxSpriteParams.palette];
        size = sample * 2 + 2048;
        if (g_FieldFxSpriteParams.palette == 4 && g_FieldFxAltPaletteFlag) palette += 4;
        clut = GetClut(16, palette);
        FieldFx_DrawShapeQuads(&position, 0, (s16)size, (s16)size,
                       (s16)g_FieldFxSpriteParams.parameter02 * (frame / 2) + 128, clut, 1,
                       g_RoomM273Trig[(((unsigned int)(u32)frame << 8) & 0x3F00) / sizeof(RoomM273TrigEntry)].bits.cos >> 5, 0);
        position.y -= input->position.pad;
        {
            int clut2;
            palette = g_FieldFxPaletteTable[g_FieldFxSpriteParams.palette];
            if (g_FieldFxSpriteParams.palette == 4 && g_FieldFxAltPaletteFlag) palette += 4;
            clut2 = GetClut(64, palette);
            FieldFx_DrawShapeQuads(&position, 0, 8192, 8192,
                           (s16)g_FieldFxSpriteParams.parameter02 * g_RoomM273HandTrailLandingPulseScale[frame / 4], clut2, 1,
                           (s16)((s32 *)g_RoomM273Trig)[(((unsigned int)(u32)frame << 9) & 0x3E00) / 4] >> 5, 0);
        }
    }
    return 0;
}

int RoomM273_HandTrailLandingEmitter(int mode, void *task_data, intptr_t argument) {
    /* Layout only: purpose of these eight original frame bytes is unknown. */
    int unknownFrame[2];
    switch(mode) {
    case 0: {
        int size=FieldFx_InitCallbackList(ANIM_POOL_AS(RoomM273PoolContext)->pool,12,10,
                               RoomM273_HandTrailLandingPulseCallback);
        if (size < 0) return size;
        int extra = FieldFx_AllocCallbackList(&g_RoomM273HandTrailLandingFanPool,size,sizeof(RoomM273PhaseState),10,RoomM273_HandTrailLandingFanCallback);
        if (extra < 0) return extra;
        return size + extra;
    }
    case 1: {
        int i;
        unsigned char *stopped=&g_RoomM273HandTrailState.done;
        if(*stopped) {
            FieldFx_UpdateCallbackList(g_RoomM273HandTrailLandingFanPool);
            return 2;
        }
        i=0;
        if(g_RoomM273HandTrailState.landing_count>0) {
            /* The stop symbol is the byte at offset 0x40 of this buffer. */
            RoomM273PairedPulseBatch *batch=
                (RoomM273PairedPulseBatch *)(stopped-PE1_OFFSETOF(RoomM273PairedPulseBatch, stopped));
            unsigned short *x=batch->x;
            int count;
            do {
                RoomM273PulseEmitterView *effect=FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
                RoomM273PulseEmitterView **pointer;
                int offset;
                unsigned short y;
                if(!effect) break;
                pointer=FieldFx_AcquireCallbackSlot(g_RoomM273HandTrailLandingFanPool);
                offset=i*2;
                effect->x=*x;
                y=*(unsigned short *)((char *)batch+offset+16);
                effect->y=y-128;
                effect->z=*(unsigned short *)((char *)batch+offset+32);
                effect->phase=0;
                effect->size=128;
                *pointer=effect;
                count=batch->count;
                ++i;
                ++x;
            } while(i<count);
        }
        g_RoomM273HandTrailState.landing_count=0;
        FieldFx_UpdateCallbackList(g_RoomM273HandTrailLandingFanPool);
        break;
    }
    case 2: {
        unsigned int index;
        unsigned short palette;
        FieldFx_DrawCallbackList(g_RoomM273HandTrailLandingFanPool);
        index=g_FieldFxTpageSlot3;
        g_FieldFxSpriteParams.parameter00=32; g_FieldFxSpriteParams.parameter02=2;
        g_FieldFxSpriteParams.extent_x=32; g_FieldFxSpriteParams.extent_y=32;
        g_FieldFxSpriteParams.extent_x=32; g_FieldFxSpriteParams.extent_y=32;
        palette=g_FieldFxTpageTable[index];
        g_FieldFxSpriteParams.palette=3; g_FieldFxSpriteParams.parameter06=0; g_FieldFxSpriteParams.parameter0A=0; g_FieldFxSpriteParams.depth=0;
        g_FieldFxSpriteParams.tpage=palette;
        break;
    }
    }
    return 0;
}

/* The rising-sprite callback and its poll/reset controller share one TU. */

int RoomM273_HandTrailHitCallback(int mode, void *data, intptr_t argument) {
    Particle * particle = data;
    Color color = g_RoomM273HandTrailHitColor;
    if (mode == 1) {
        if (g_FieldFxCurrentAge >= 16) return 1;
        else {
            unsigned short y = particle->y;
            int speed = ((volatile Particle *)particle)->speed;
            int delta = ((Particle *)particle)->speed;
            speed += 1;
            particle->y = y - delta;
            particle->speed = speed;
        }
    } else if (mode == 2) {
        int frame = g_FieldFxCurrentAge - 1;
        int sample = g_RoomM273Trig[((unsigned int)(u32)frame << 6) & 0xFC0].part.sin;
        int size, kind, palette;
        int specialKind = 4;
        unsigned short clut;
        kind = g_FieldFxSpriteParams.palette;
        size = sample * 2;
        palette = g_FieldFxPaletteTable[kind];
        if (kind == specialKind && g_FieldFxAltPaletteFlag) palette += 9;
        else palette += 5;
        clut = GetClut(0, palette);
        FieldFx_DrawShapeQuads((struct GteShortVector *)particle, 0, size, size, 102, clut, 1,
            (short)g_RoomM273Trig[(((unsigned int)(u32)frame << 9) & 0x3E00) / 4].word >> 6, (struct RenderColor *)&color);
    }
    return 0;
}

#define ROOMLIB_POLL_RESET_POOL_CONTEXT_DECL
#define ROOMLIB_POLL_RESET_POOL_EXPR ANIM_POOL_AS(RoomM273PoolContext)->pool
#define ROOMLIB_POLL_RESET_FUNC RoomM273_HandTrailHitEmitter
#define ROOMLIB_POLL_RESET_CALLBACK RoomM273_HandTrailHitCallback
#define ROOMLIB_POLL_RESET_FLAG g_RoomM273HandTrailState.done
#define ROOMLIB_POLL_RESET_COUNTER g_RoomM273HandTrailState.hit_flag
#define ROOMLIB_POLL_RESET_SEED g_RoomM273HandTrailState.hit_seed
#include "../room_lib/poll_reset_actor.inc"

typedef RoomM273EffectModeState State;

int RoomM273_HandGlowEmitter(int mode, void *task_data, intptr_t argument) {
    /* Layout only: purpose of these eight original frame bytes is unknown. */
    int unknownFrame[2];
    switch(mode) {
    case 0: {
        int size=FieldFx_InitCallbackList(ANIM_POOL_AS(RoomM273PoolContext)->pool,sizeof(void *),6,RoomM273_PulseSpriteCallback);
        if (size < 0) return size;
        int extra = FieldFx_AllocCallbackList(&g_RoomM273HandRingBandPool,size,sizeof(RoomM273PointerPoolEffect),6,RoomM273_HandRingBandCallback);
        if (extra < 0) return extra;
        return size + extra;
    }
    case 1: {
        unsigned char *stopped=&g_RoomM273HandTrailState.done;
        State *state;
        if(*stopped) return 2;
        state=ANIM_OWNER_AS(RoomM273EffectStateContext)->state.mode;
        if(state->mode==11) {
            int count=state->anim.parts.integer;
            unsigned short value=*(volatile unsigned short *)&state->anim.parts.integer;
            if(count<35 && (value&7)==0) {
                short i=0;
                /* The shared positions start 0x5C bytes before the stop symbol. */
                Vector *positions=g_RoomM273HandTrailState.hands;
                do {
                    Vector **out=FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
                    if(!out) break;
                    *out=&positions[i];
                    i++;
                } while(i<2);
                i=0;
                positions=g_RoomM273HandTrailState.hands;
                do {
                    Vector **out=FieldFx_AcquireCallbackSlot(g_RoomM273HandRingBandPool);
                    if(!out) break;
                    *out=&positions[i];
                    i++;
                } while(i<2);
            }
            FieldFx_UpdateCallbackList(g_RoomM273HandRingBandPool);
        }
        break;
    }
    case 2: {
        unsigned int index;
        unsigned short palette;
        FieldFx_DrawCallbackList(g_RoomM273HandRingBandPool);
        index=g_FieldFxTpageSlot11;
        g_FieldFxSpriteParams.parameter00=32; g_FieldFxSpriteParams.parameter02=2;
        g_FieldFxSpriteParams.extent_x=32; g_FieldFxSpriteParams.extent_y=32;
        g_FieldFxSpriteParams.extent_x=32; g_FieldFxSpriteParams.extent_y=32;
        palette=g_FieldFxTpageTable[index];
        g_FieldFxSpriteParams.palette=3; g_FieldFxSpriteParams.parameter06=1; g_FieldFxSpriteParams.parameter0A=0; g_FieldFxSpriteParams.depth=0;
        g_FieldFxSpriteParams.tpage=palette;
        break;
    }
    }
    return 0;
}

#undef ROOMLIB_POLL_RESET_POOL_CONTEXT_DECL
#undef ROOMLIB_POLL_RESET_POOL_EXPR
#undef ROOMLIB_POLL_RESET_FUNC
#undef ROOMLIB_POLL_RESET_CALLBACK
#undef ROOMLIB_POLL_RESET_FLAG
#undef ROOMLIB_POLL_RESET_COUNTER
#undef ROOMLIB_POLL_RESET_SEED
#undef PE1_EXT_B7E658A5B5B4

/* Tracks the boss's two hand positions, turns it toward the player (or by
 * a scripted spin) during animation 11 and bobs its height. */
int RoomM273_HandTrailController(int mode, void *task_data, intptr_t argument) {
    RoomM273BossInstance *instance = ANIM_OWNER_AS(RoomM273BossActor)->instance;
    int frame;
    int value;
    GteShortVector *hand;

    if (mode == 0) {
        g_RoomM273HandTrailState.done = 0;
        g_RoomM273HandTrailState.landing_count = 0;
        g_RoomM273HandTrailState.hit_flag = 0;
        g_RoomM273HandTrailState.spin = 0;
        g_RoomM273HandTrailState.tick = 0;
        g_RoomM273HandTrailState.floor = g_RoomFloorY->raw - 0x200;
        g_RoomM273HandTrailState.base_height = instance->pos_y;
    } else if (mode == 1) {
        hand = g_RoomM273HandTrailState.hands;
        for (value = 0; value < 2; value++) {
            GteMatrix *m = (GteMatrix *)instance->render_object.matrices;
            s32 *t = value ? m[18].t : m[13].t;
            hand[value].x = t[0];
            hand[value].y = t[1];
            hand[value].z = t[2];
        }
        if (instance->state->action_state && *instance->state->action_state == 1) *instance->state->action_state = 2;
        frame = instance->anim.parts.integer;
        if (instance->mode != 11) return 0;
        if ((s16)frame >= instance->action - 1) {
            g_RoomM273HandTrailState.done = 1;
            if (instance->state && instance->state->action_state) *instance->state->action_state = 4;
            return 1;
        }
        if ((s16)frame >= 0x22) {
            if (g_RoomM273HandTrailState.repeat-- > 0) instance->anim.fixed = 0x40000;
        }
        if (g_RoomM273HandTrailState.spinning) {
            if (g_RoomM273HandTrailState.spin > g_RoomM273HandTrailState.spin_target) g_RoomM273HandTrailState.spin -= g_RoomM273HandTrailState.spin_step;
            else if (g_RoomM273HandTrailState.spin < g_RoomM273HandTrailState.spin_target) g_RoomM273HandTrailState.spin += g_RoomM273HandTrailState.spin_step;
            instance->rot_y = (instance->rot_y + g_RoomM273HandTrailState.spin) & 0xFFF;
            if (g_RoomM273HandTrailState.tick >= g_RoomM273HandTrailState.tick_limit) g_RoomM273HandTrailState.spin_target = 0;
        } else {
            /* The heading is turned half way round in 16.16 form. */
            value = FieldFx_VecToAngle((Vec3Fixed *)g_PlayerEntity->render_object.model_matrix.translation, (Vec3Fixed *)instance->render_object.model_matrix.translation);
            instance->rot_y = FieldFx_TurnToward(instance->rot_y, (short)(value + 2048), 0x20);
        }
        if (g_RoomM273HandTrailState.tick < 0x77) {
            value = (g_RoomM273HandTrailState.tick << 13) / 118;
            value = g_RoomM273Trig[(value - 0x800) & 0xFFF].bits.cos + 0x1000;
            instance->pos_y = ((-(value * 384) / 8192) * 65536) + g_RoomM273HandTrailState.base_height;
        }
        g_RoomM273HandTrailState.tick++;
    }
    return 0;
}

/* Falling drop: queues its landing position, then knocks the player back on
 * contact; draws two rings before landing and a splash flash after. */
int RoomM273_SweepDropCallback(int mode, void *data, intptr_t argument) {
    RoomM273Drop * drop = data;
    GteShortVector ring;
    GteShortVector unused;
    int i;

    if (mode == 1) {
        if (drop->position.pad == 0) {
            RoomFloorY *floor = g_RoomFloorY;
            drop->position.x += drop->vx;
            drop->position.y += drop->vy;
            drop->position.z += drop->vz;
            if (drop->position.y >= (s16)floor->raw) {
                drop->position.y = floor->raw;
                drop->position.pad = 1;
                i = g_RoomM273SweepState.count++;
                g_RoomM273SweepState.x[i] = drop->position.x;
                g_RoomM273SweepState.y[i] = floor->raw;
                g_RoomM273SweepState.z[i] = drop->position.z;
            }
            if (drop->touched) return 0;
            if (drop->position.y < g_RoomM273SweepState.floor) return 0;
            if (Math_IntSqrt((g_PlayerEntity->render_object.model_matrix.translation[0] - drop->position.x) *
                                 (g_PlayerEntity->render_object.model_matrix.translation[0] - drop->position.x) +
                             (g_PlayerEntity->render_object.model_matrix.translation[2] - drop->position.z) *
                                 (g_PlayerEntity->render_object.model_matrix.translation[2] - drop->position.z)) < 0x80) {
                drop->touched = 1;
                g_RoomM273SweepState.hit.vector = drop->position;
                g_RoomM273SweepState.hit.vector.pad = 1;
                g_PlayerEntity->state->flags |= 0x4000;
                if (ANIM_OWNER_AS(RoomM273BossActor)->instance->state)
                    ANIM_OWNER_AS(RoomM273BossActor)->instance->state->core_flags |= 0x80000000;
            }
        } else {
            if (drop->position.pad++ >= 8) return 1;
        }
    } else if (mode == 2) {
        int shade = g_RoomM273Trig[(drop->position.pad << 7) & 0xF80].bits.cos >> 5;
        RoomM273BossTrig *trig = ((RoomM273BossTrig *)g_RoomM273Trig);
        ring.x = drop->ring.x;
        ring.y = drop->ring.y;
        ring.z = drop->ring.z;
        if (drop->position.pad == 0) {
            for (i = 0; i < 2; i++) {
                FieldFx_DrawRotatedTriangle(&drop->position, (GteRotation *)&ring, 0x100, 0x10, &g_RoomM273SweepDropColorB,
                              &g_RoomM273SweepDropColorA, &g_RoomM273SweepDropColorA, (s16)shade, 1);
                ring.z += 0x400;
            }
        } else {
            int size = trig[(drop->position.pad << 7) & 0xF80].sine * 2 + 0x1000;
            int kind = g_FieldFxSpriteParams.palette;
            int palette = g_FieldFxPaletteTable[kind];
            if (kind == 4 && g_FieldFxAltPaletteFlag) palette += 4;
            FieldFx_DrawShapeQuads(&drop->position, &g_RoomM273FloorRotation, size, size, 0xDC,
                          GetClut(0x30, palette), 1, shade, &g_RoomM273SweepDropColorC);
        }
    }
    return 0;
}

/* room_m273, file offset 0x79F0, 1364 retail-matching bytes. */

int RoomM273_SweepDropEmitter(int mode, void *task_data, intptr_t argument) {
    Vector playerPosition;
    Matrix matrix;
    switch(mode) {
    case 0:
        return FieldFx_InitCallbackList(ANIM_POOL_AS(RoomM273PoolContext)->pool,24,40,
                             RoomM273_SweepDropCallback);
    case 1: {
        unsigned char *stopped=&g_RoomM273SweepState.done;
        Player *player;
        int dx,dy,dz,x,y,z; short spread; short angle;
        if(*stopped) return 2;
        player=g_PlayerEntity;
        playerPosition.x=x=((Pe1Fixed16_16 *)&player->pos_x)->parts.integer;
        dx=x-g_RoomM273SweepPoint.x;
        playerPosition.y=y=((Pe1Fixed16_16 *)&player->pos_y)->parts.integer;
        playerPosition.z=z=((Pe1Fixed16_16 *)&player->pos_z)->parts.integer;
        dz=z-g_RoomM273SweepPoint.z;
        dy=y-g_RoomM273SweepPoint.y;
        angle=ratan2(dy,Math_IntSqrt((unsigned int)dx*dx+(unsigned int)dz*dz))&4095;
        if(angle>768) angle=768;
        if(angle<384) angle=384;
        /* Pitch is clamped to [384, 768]; the second ring halves its spread. */
        spread=angle-256;
        if(g_RoomM273SweepState.in_contact) {
            short i=0;
            int width=spread;
            /* Retail address window: heading 0x8019AF60, origin 0x8019AEFC. */
            unsigned short *heading=(unsigned short *)&g_RoomM273SweepState.target;
            Matrix *transform=&matrix;
            matrix.t[0]=0; matrix.t[1]=0; matrix.t[2]=0;
            do {
                RoomM273Drop *particle=FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
                short *wave;
                if(!particle) break;
                wave=(short *)&((int *)g_RoomM273Trig)[(i*4096/6)&4095];
                particle->ring.x=angle+wave[1]*160/4096;
                particle->ring.y=wave[0]*width/4096+(*heading+2048);
                particle->ring.z=0; particle->ring.pad=1;
                RotMatrixYXZ(&particle->ring,transform);
                gte_ldrotmatrix(transform);
                gte_ldtransmatrix(transform);
                { Vector *input=&g_RoomM273SweepDropVector; gte_ldv0(input); }
                gte_cop2_hazard_slot();
                gte_cop2_hazard_slot();
                gte_mvmva_rotation_v0_translation_sf12();
                gte_stsv((GteShortVector *)&particle->vx);
                particle->position.x=g_RoomM273SweepPoint.x;
                particle->position.y=g_RoomM273SweepPoint.y;
                particle->position.z=g_RoomM273SweepPoint.z;
                particle->position.pad=0; particle->touched=0;
                i++;
            } while(i<6);
            /* Failure in the six-particle ring does not skip this ring. */
            i=0; width=spread/2; heading=&g_RoomM273SweepState.target;
            do {
                RoomM273Drop *particle=FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
                short *wave;
                if(!particle) break;
                {
                    int *table=(int *)g_RoomM273Trig;
                    wave=(short *)&table[((unsigned int)i<<10)&3072];
                }
                particle->ring.x=angle+wave[1]*96/4096;
                particle->ring.y=wave[0]*width/4096+(*heading+2048);
                particle->ring.z=0; particle->ring.pad=1;
                transform=&matrix;
                RotMatrixYXZ(&particle->ring,transform);
                gte_ldrotmatrix(transform);
                gte_ldtransmatrix(transform);
                { Vector *input=&g_RoomM273SweepDropVector; gte_ldv0(input); }
                gte_cop2_hazard_slot();
                gte_cop2_hazard_slot();
                gte_mvmva_rotation_v0_translation_sf12();
                gte_stsv((GteShortVector *)&particle->vx);
                particle->position.x=g_RoomM273SweepPoint.x;
                particle->position.y=g_RoomM273SweepPoint.y;
                particle->position.z=g_RoomM273SweepPoint.z;
                particle->position.pad=0; particle->touched=0;
                i++;
            } while(i<4);
            Asset_Find08w(0x5D1,g_FieldFxOwner->actor->state->progress,g_RoomM273SweepPoint.x,g_RoomM273SweepPoint.y,g_RoomM273SweepPoint.z);
        }
        break;
    }
    case 2: {
        unsigned int index=g_FieldFxTpageSlot2;
        unsigned short palette;
        g_FieldFxSpriteParams.parameter00=16; g_FieldFxSpriteParams.parameter02=1;
        g_FieldFxSpriteParams.extent_x=16; g_FieldFxSpriteParams.extent_y=16;
        g_FieldFxSpriteParams.extent_x=16; g_FieldFxSpriteParams.extent_y=16;
        palette=g_FieldFxTpageTable[index];
        g_FieldFxSpriteParams.palette=2; g_FieldFxSpriteParams.parameter06=0; g_FieldFxSpriteParams.parameter0A=0; g_FieldFxSpriteParams.depth=0;
        g_FieldFxSpriteParams.tpage=palette;
        break;
    }
    }
    return 0;
}

/* The batch emitter fills the eight-byte records rendered by this callback. */

int RoomM273_SweepSplashCallback(int mode, void *data, intptr_t argument) {
    GteShortVector * position = data;
    if (mode == 1) {
        if (g_FieldFxCurrentAge >= 16) return 1;
        position->y -= 6;
    } else if (mode == 2) {
        int frame = g_FieldFxCurrentAge - 1;
        int kind = g_FieldFxSpriteParams.palette;
        int palette = g_FieldFxPaletteTable[kind];
        int clut;
        if (kind == 4 && g_FieldFxAltPaletteFlag) palette += 4;
        clut = GetClut(64, palette);
        FieldFx_DrawShapeQuads(position, 0, 4096, 4096,
            44 + (s16)g_FieldFxSpriteParams.parameter02 * g_RoomM273SweepSplashScale[frame / 4], clut, 1,
            (short)g_RoomM273Trig[(((unsigned int)(u32)frame << 9) & 0x3E00) / 4].word >> 5, 0);
    }
    return 0;
}

int RoomM273_SweepSplashEmitter(int mode, void *task_data, intptr_t argument) {
    /* Layout only: purpose of these eight original frame bytes is unknown. */
    int stack_pad[2];
    switch(mode) {
    case 0:
        return FieldFx_InitCallbackList(ANIM_POOL_AS(RoomM273PoolContext)->pool,8,40,
                             RoomM273_SweepSplashCallback);
    case 1: {
        int i;
        s16 *x,*y;
        if(g_RoomM273SweepState.done) return 2;
        i=0;
        if(g_RoomM273SweepState.count>0) {
            RoomM273DropQueue *batch=&g_RoomM273SweepState;
            y=batch->y;
            x=batch->x;
            do {
                int offset;
                GteShortVector *out=FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
                if(!out) break;
                out->x=*x;
                out->y=*y++-128;
                ++x;
                offset=i*2;
                out->z=batch->z[i];
                out->pad=0;
                ++i;
            } while(i<batch->count);
        }
        g_RoomM273SweepState.count=0;
        break;
    }
    case 2: {
        unsigned int index=g_FieldFxTpageSlot3;
        unsigned short palette;
        g_FieldFxSpriteParams.parameter00=32; g_FieldFxSpriteParams.parameter02=2;
        g_FieldFxSpriteParams.extent_x=32; g_FieldFxSpriteParams.extent_y=32;
        g_FieldFxSpriteParams.extent_x=32; g_FieldFxSpriteParams.extent_y=32;
        palette=g_FieldFxTpageTable[index];
        g_FieldFxSpriteParams.palette=3; g_FieldFxSpriteParams.parameter06=0; g_FieldFxSpriteParams.parameter0A=0; g_FieldFxSpriteParams.depth=0;
        g_FieldFxSpriteParams.tpage=palette;
        break;
    }
    }
    return 0;
}

#undef ROOMLIB_POLL_RESET_POOL_CONTEXT_DECL
#undef ROOMLIB_POLL_RESET_POOL_EXPR
#undef ROOMLIB_POLL_RESET_FUNC
#undef ROOMLIB_POLL_RESET_CALLBACK
#undef ROOMLIB_POLL_RESET_FLAG
#undef ROOMLIB_POLL_RESET_COUNTER
#undef ROOMLIB_POLL_RESET_SEED

/* Sprite callback and poll/reset controller share one pool and layout. */

int RoomM273_SweepDropHitCallback(int mode, void *data, intptr_t argument) {
    Vector * position = data;
    if (mode == 1) {
        if (g_FieldFxCurrentAge >= 8) return 1;
    } else if (mode == 2) {
        int frame = g_FieldFxCurrentAge - 1;
        int kind, palette;
        int size;
        int sample;
        unsigned short clut;
        sample = g_RoomM273Trig[(((unsigned int)(u32)frame << 9) & 0x3E00) / 4].word;
        kind = g_FieldFxSpriteParams.palette;
        size = sample + 2048;
        palette = g_FieldFxPaletteTable[kind];
        clut = GetClut(0, (kind == 4 && g_FieldFxAltPaletteFlag) ? palette + 9 : palette + 5);
        FieldFx_DrawShapeQuads(position, 0, (short)size, (short)size, 102, clut, 1,
            (short)g_RoomM273Trig[(((unsigned int)frame << 10) & 0x3C00) / 4].word >> 5, 0);
    }
    return 0;
}

#define ROOMLIB_POLL_RESET_POOL_CONTEXT_DECL
#define ROOMLIB_POLL_RESET_POOL_EXPR ANIM_POOL_AS(RoomM273PoolContext)->pool
#define ROOMLIB_POLL_RESET_FUNC RoomM273_SweepDropHitEmitter
#define ROOMLIB_POLL_RESET_CALLBACK RoomM273_SweepDropHitCallback
#define ROOMLIB_POLL_RESET_FLAG g_RoomM273SweepState.done
#define ROOMLIB_POLL_RESET_COUNTER g_RoomM273SweepState.hit.vector.pad
#define ROOMLIB_POLL_RESET_SEED g_RoomM273SweepState.hit
#include "../room_lib/poll_reset_actor.inc"

/* The timed emitter fills the eight-byte records rendered by this callback. */
int RoomM273_SweepRisingCallback(int mode, void *data, intptr_t argument) {
    Particle * input = data;
    Particle *particle = input;
    /* Layout only: original purpose of these eight frame bytes is unknown. */
    int stack_pad[2];
    if (mode == 1) {
        if (g_FieldFxCurrentAge >= 32) return 1;
        else {
            unsigned short y = ((volatile Particle *)particle)->y;
            s16 speed = ((volatile Particle *)particle)->speed;
            int delta = ((Particle *)particle)->speed;
            speed += 1;
            particle->y = y - delta;
            particle->speed = speed;
        }
    } else if (mode == 2) {
        int frame = g_FieldFxCurrentAge - 1;
        int kind = g_FieldFxSpriteParams.palette;
        int sample, palette, size, shade;
        int paletteOffset, sizeOffset;
        unsigned int sampleOffset;
        int clut;
        sampleOffset = ((unsigned int)(u32)frame << 8) & 0x3F00;
        paletteOffset = kind * 2;
        sample = g_RoomM273Trig[sampleOffset / 4].word;
        sizeOffset = ((unsigned int)(u32)frame << 7) & 0x3F80;
        palette = *(unsigned short *)((char *)g_FieldFxPaletteTable + paletteOffset);
        shade = (short)sample >> 5;
        size = *(short *)((char *)g_RoomM273Trig + sizeOffset) + 8192;
        if (kind == 4 && g_FieldFxAltPaletteFlag) palette += 4;
        clut = GetClut(16, palette);
        FieldFx_DrawShapeQuads((GteShortVector *)particle, 0, size, size, (s16)g_FieldFxSpriteParams.parameter02 * (frame / 4) + 200,
            clut, 1, shade, (struct RenderColor *)g_RoomM273SweepRisingColor);
    }
    return 0;
}

int RoomM273_SweepRisingEmitter(int mode, void *task_data, intptr_t argument) {
    switch(mode) {
    case 0:
        g_RoomM273SweepRisingDelay=0;
        return FieldFx_InitCallbackList(ANIM_POOL_AS(RoomM273PoolContext)->pool,8,12,
                             RoomM273_SweepRisingCallback);
    case 1: {
        Particle *output;
        if(g_RoomM273SweepState.done) return 2;
        if(!g_RoomM273SweepState.in_window) return 0;
        if(g_RoomM273SweepRisingDelay-- > 0) return 0;
        output=FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
        if(!output) return 0;
        ((volatile Particle *)output)->x=g_RoomM273SweepPoint.x;
        ((volatile Particle *)output)->y=g_RoomM273SweepPoint.y-256;
        output->z=g_RoomM273SweepPoint.z;
        g_RoomM273SweepRisingDelay=2;
        ((volatile Particle *)output)->speed=0;
        break;
    }
    case 2: {
        unsigned int index=g_FieldFxTpageSlot2;
        unsigned short palette;
        g_FieldFxSpriteParams.parameter00=16;
        g_FieldFxSpriteParams.parameter02=1;
        g_FieldFxSpriteParams.extent_x=16;
        g_FieldFxSpriteParams.extent_y=16;
        g_FieldFxSpriteParams.extent_x=16;
        g_FieldFxSpriteParams.extent_y=16;
        palette=g_FieldFxTpageTable[index];
        g_FieldFxSpriteParams.palette=2;
        g_FieldFxSpriteParams.parameter06=0;
        g_FieldFxSpriteParams.parameter0A=0;
        g_FieldFxSpriteParams.depth=0;
        g_FieldFxSpriteParams.tpage=palette;
        break;
    }
    }
    return 0;
}

/* The position emitter fills the eight-byte records rendered by this callback. */
int RoomM273_SweepFlashCallback(int mode, void *data, intptr_t argument) {
    Vector * input = data;
    Vector rotation,position;
    if(mode==1) {
        if(g_FieldFxCurrentAge>=16) return 1;
    } else if(mode==2) {
        int counter=g_FieldFxCurrentAge;
        int frame=counter-1;
        int kind=g_FieldFxSpriteParams.palette;
        int firstSample=g_RoomM273Trig[((unsigned int)(u8)frame<<6)&0xFC0].word;
        int shade,size,palette;
        int secondSample;
        unsigned short clut;
        rotation.x=0; rotation.y=0; rotation.z=(unsigned int)frame<<8; rotation.pad=0;
        position=*input;
        palette=g_FieldFxPaletteTable[kind];
        shade=firstSample>>21;
        size=(short)firstSample*3+2048;
        if(kind==4 && g_FieldFxAltPaletteFlag) palette+=4;
        clut=GetClut(80,palette);
        FieldFx_DrawShapeQuads(&position,(GteRotation *)&rotation,size,size,74,clut,1,shade,0);
        rotation.z>>=1;
        secondSample=g_RoomM273Trig[((unsigned int)(u8)frame<<6)&0xFC0].word;
        shade=secondSample>>21;
        size=(short)secondSample+1024;
        FieldFx_DrawStarFan(&position,512,512,16,(GteRotation *)&rotation,size,size,(struct RenderColor *)g_RoomM273BlackColor,(struct RenderColor *)g_RoomM273SweepFlashColorA,shade,1);
        rotation.x=1024; rotation.y=0; rotation.z=(unsigned int)frame<<8; rotation.pad=1;
        position.y=g_RoomFloorY->raw;
        FieldFx_DrawStarFan(&position,768,768,8,(GteRotation *)&rotation,4096,4096,(struct RenderColor *)g_RoomM273SweepFlashColorB,(struct RenderColor *)g_RoomM273BlackColor,shade,1);
    }
    return 0;
}

int RoomM273_SweepFlashEmitter(int mode, void *task_data, intptr_t argument) {
    switch(mode) {
    case 0:
        return FieldFx_InitCallbackList(ANIM_POOL_AS(RoomM273PoolContext)->pool,8,4,
                             RoomM273_SweepFlashCallback);
    case 1: {
        Vector *output;
        if(g_RoomM273SweepState.done) return 2;
        if(!g_RoomM273SweepState.in_contact) return 0;
        output=FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
        if(!output) return 0;
        ((volatile Vector *)output)->x=g_RoomM273SweepPoint.x;
        ((volatile Vector *)output)->y=g_RoomM273SweepPoint.y;
        output->z=g_RoomM273SweepPoint.z;
        break;
    }
    case 2: {
        unsigned int index=g_FieldFxTpageSlot3;
        unsigned short palette;
        g_FieldFxSpriteParams.parameter00=32;
        g_FieldFxSpriteParams.parameter02=2;
        g_FieldFxSpriteParams.extent_x=32;
        g_FieldFxSpriteParams.extent_y=32;
        g_FieldFxSpriteParams.extent_x=32;
        g_FieldFxSpriteParams.extent_y=32;
        palette=g_FieldFxTpageTable[index];

        g_FieldFxSpriteParams.palette=3;
        g_FieldFxSpriteParams.parameter06=0;
        g_FieldFxSpriteParams.parameter0A=0;
        g_FieldFxSpriteParams.depth=64;
        g_FieldFxSpriteParams.tpage=palette;
        break;
    }
    }
    return 0;
}

#undef ROOMLIB_POLL_RESET_POOL_CONTEXT_DECL
#undef ROOMLIB_POLL_RESET_POOL_EXPR
#undef ROOMLIB_POLL_RESET_FUNC
#undef ROOMLIB_POLL_RESET_CALLBACK
#undef ROOMLIB_POLL_RESET_FLAG
#undef ROOMLIB_POLL_RESET_COUNTER
#undef ROOMLIB_POLL_RESET_SEED

/* Animation 13 controller: turns the boss toward the player on entry, loops
 * the sweep window while loops remain, publishes the sweep attack angle and
 * transforms the sweep point. */
int RoomM273_SweepController(int mode, void *task_data, intptr_t argument) {
    RoomM273BossInstance *instance = ANIM_OWNER_AS(RoomM273BossActor)->instance;
    RoomM273SweepStep *step;
    GteShortVector offset;
    s16 frame;
    s16 angle;
    s16 value;

    if (mode == 0) {
        g_RoomM273SweepState.started = 0;
        g_RoomM273SweepState.done = 0;
        g_RoomM273SweepState.in_window = 0;
        g_RoomM273SweepState.in_contact = 0;
        g_RoomM273SweepState.step = 0;
        g_RoomM273SweepState.count = 0;
        g_RoomM273SweepState.hit.vector.pad = 0;
        g_RoomM273SweepState.floor = g_RoomFloorY->raw - 0x180;
        instance->state->core_flags |= 0x40000000;
    } else if (mode == 1) {
        if (instance->state->action_state && *instance->state->action_state == 1) *instance->state->action_state = 2;
        if (instance->mode == 13) {
            frame = instance->anim.parts.integer;
            step = &g_RoomM273SweepSteps[g_RoomM273SweepState.side];
            if (frame == 0) {
                instance->anim.fixed = step->start << 16;
                angle = (ratan2(instance->pos_x - g_PlayerEntity->pos_x,
                                       instance->pos_z - g_PlayerEntity->pos_z)
                         - instance->rot_y) & 0xFFF;
                if (g_RoomM273SweepState.side != 0) {
                    if (angle <= 0x800) g_RoomM273SweepState.turn = angle >> 6;
                    else g_RoomM273SweepState.turn = 0;
                } else {
                    angle = 0x1000 - angle;
                    if (angle <= 0x800) g_RoomM273SweepState.turn = -angle >> 6;
                    else g_RoomM273SweepState.turn = 0;
                }
            } else {
                if (frame >= step->end) {
                    g_RoomM273SweepState.done = 1;
                    if (instance->state && instance->state->action_state) *instance->state->action_state = 4;
                    return 1;
                }
                if (frame >= step->loop_end && g_RoomM273SweepState.loops > 0) {
                    instance->anim.fixed = step->loop_start << 16;
                    g_RoomM273SweepState.loops--;
                }
            }
            if (frame >= step->loop_start) g_RoomM273SweepState.started = 1;
            g_RoomM273SweepState.in_window = frame >= step->loop_start && frame <= step->loop_end;
            {
                RoomM273BossInstance *current = ANIM_OWNER_AS(RoomM273BossActor)->instance;
                s16 previous;
                frame = current->anim.parts.integer;
                previous = (u32)current->anim_prev >> 16;
                g_RoomM273SweepState.in_contact = frame >= g_RoomM273SweepState.contact && previous < g_RoomM273SweepState.contact;
            }
            if (frame >= step->loop_start && frame <= step->loop_end) {
                value = step->scale * g_RoomM273SweepState.step / g_RoomM273SweepState.divisor;
                instance->render_object.rotation_overrides[0].matrix_index = 0x15;
                instance->render_object.rotation_overrides[0].flags = 2;
                instance->render_object.rotation_overrides[0].y = value;
                g_RoomM273SweepState.target = step->offset + (value + instance->rot_y);
                g_RoomM273SweepState.step++;
            } else {
                instance->render_object.rotation_overrides[0].matrix_index = 0;
                instance->render_object.rotation_overrides[0].flags = 0;
            }
            if (frame > step->loop_end) g_RoomM273SweepState.turn >>= 1;
            instance->rot_y += g_RoomM273SweepState.turn;
        }
    }
    offset.x = 0;
    offset.y = 0;
    offset.z = -0x180;
    {
        GteMatrix *matrix = (GteMatrix *)(instance->render_object.matrices + 22);
        GteShortVector *out;
        gte_ldrotmatrix(matrix);
        gte_ldtransmatrix(matrix);
        gte_ldv0(&offset);
        gte_rtv0tr_mac();
        out = &g_RoomM273SweepPoint;
        gte_stsv(out);
    }
    return 0;
}

int RoomM273_SwayShardTrailCallback(int mode, void *data, intptr_t argument) {
    GteShortVector * position = data;
    int frame, sample, size, kind, palette;
    u16 clut;

    if (mode == 1) {
        if (g_FieldFxCurrentAge >= 9) return 1;
    } else if (mode == 2) {
        frame = g_FieldFxCurrentAge;
        if (frame - 2 < 0) return 0;

        sample = g_RoomM273Trig[((u32)(frame - 1) << 7) & 0xF80].part.cos;
        size = sample * 2;
        kind = g_FieldFxSpriteParams.palette;
        /* Keep the table read before the palette lookup. */
        palette = g_FieldFxPaletteTable[kind];
        if (kind == 4 && g_FieldFxAltPaletteFlag) palette += 4;

        clut = GetClut(32, palette);
        FieldFx_DrawShapeQuads(position, 0, (s16)size, (s16)size,
                       (s16)g_FieldFxSpriteParams.parameter02 + 216, clut, 1,
                       /* The upper half of each packed trig entry is signed. */
                       (s16)(g_RoomM273Trig[((u32)(g_FieldFxCurrentAge - 1) << 7) & 0xF80].word >> 16) >> 5,
                       (RenderColor *)g_RoomM273SwayShardTrailColor);
    }
    return 0;
}

/* Sway shard: flies until it hits the player or the floor (recording the
 * impact in the sway point table), leaving a trail; draws a glow, two
 * crossed flares and two floor rings. */
int RoomM273_SwayShardCallback(int mode, void *data, intptr_t argument) {
    RoomM273SwayShard * shard = data;
    GteShortVector position;
    GteRotation spin;

    if (mode == 1) {
        if (shard->state == 0 && g_RoomM273SwayState.cooldown == 0 &&
            shard->position.y > (s16)g_RoomFloorY->raw - 0x240) {
            s16 dx = g_PlayerEntity->render_object.model_matrix.translation[0] - shard->position.x;
            s16 dz = g_PlayerEntity->render_object.model_matrix.translation[2] - shard->position.z;
            if ((s16)Math_IntSqrt(dx * dx + dz * dz) < 0x80) {
                shard->state = 1;
                g_PlayerEntity->state->flags |= 0x4000;
                if (ANIM_OWNER_AS(RoomM273BossActor)->instance->state)
                    ANIM_OWNER_AS(RoomM273BossActor)->instance->state->core_flags |= 0x80000000;
                g_RoomM273SwayState.hits[shard->side].x = shard->position.x;
                g_RoomM273SwayState.hits[shard->side].y = shard->position.y;
                g_RoomM273SwayState.hits[shard->side].z = shard->position.z;
                g_RoomM273SwayState.hits[shard->side].pad = 2;
                Asset_Find08w(0x5FC, ANIM_OWNER_AS(RoomM273BossActor)->instance->state->progress, shard->position.x,
                              shard->position.y, shard->position.z);
                g_RoomM273SwayState.cooldown = 0x28;
                return 1;
            }
        }
        shard->position.x += shard->velocity.x;
        shard->position.y += shard->velocity.y;
        shard->position.z += shard->velocity.z;
        if (shard->position.y >= (s16)g_RoomFloorY->raw) {
            g_RoomM273SwayState.hits[shard->side].x = shard->position.x;
            g_RoomM273SwayState.hits[shard->side].y = shard->position.y;
            g_RoomM273SwayState.hits[shard->side].z = shard->position.z;
            g_RoomM273SwayState.hits[shard->side].pad = 3;
            Asset_Find08w(0x5FC, ANIM_OWNER_AS(RoomM273BossActor)->instance->state->progress, shard->position.x,
                          shard->position.y, shard->position.z);
            return 1;
        } else {
            s16 frame = g_FieldFxCurrentAge & 7;
            GteShortVector *trail;
            if (frame < 4) shard->frame = frame;
            else shard->frame = frame + 12;
            trail = FieldFx_AcquireCallbackSlot(g_RoomM273SwayShardTrailPool);
            if (trail == 0) return 0;
            trail->x = shard->position.x;
            trail->y = shard->position.y;
            trail->z = shard->position.z;
        }
    } else if (mode == 2) {
        int i;
        int kind;
        int palette;
        position.x = shard->position.x;
        position.y = shard->position.y;
        position.z = shard->position.z;
        kind = g_FieldFxSpriteParams.palette;
        palette = g_FieldFxPaletteTable[kind];
        if (kind == 4 && g_FieldFxAltPaletteFlag) palette += 4;
        FieldFx_DrawShapeQuads(&position, 0, 0xA00, 0xA00,
                      (s16)g_FieldFxSpriteParams.parameter02 * shard->frame,
                      GetClut(0x10, palette), 1, 0x80, 0);
        spin.x = g_FieldFxCurrentAge * 0x300;
        spin.y = ANIM_OWNER_AS(RoomM273BossActor)->instance->rot_y;
        spin.z = 0;
        spin.flags = 1;
        for (i = 0; i < 2; i++) {
            FieldFx_DrawRingBand(&position, 0x60, 0x88, 8, &spin, 0x1000, 0x1000, &g_RoomM273SwayShardRingColor,
                          (RenderColor *)g_RoomM273BlackColor, 0x80, 1);
            spin.y = spin.x;
            spin.x = 0;
        }
        spin.x = 0;
        spin.y = 0;
        spin.flags = 0;
        spin.z = g_FieldFxCurrentAge << 8;
        FieldFx_DrawStarFan(&position, 0x100, 0x100, 8, &spin, 0x1000, 0x1000, &g_RoomM273SwayShardStarColor,
                      (RenderColor *)g_RoomM273BlackColor, 0x80, 1);
        position.y = g_RoomFloorY->raw;
        FieldFx_DrawStarFan(&position, 0xC0, 0xC0, 8, 0, 0x1000, 0x1000, &g_RoomM273SwayShardInnerStarColor,
                      (RenderColor *)g_RoomM273BlackColor, 0x80, 1);
    }
    return 0;
}

/* Spawns a shard from each sway point aimed at the player while animation
 * 15 swings through, and draws the shard and glow pools. */
int RoomM273_SwayShardEmitter(int mode, void *task_data, intptr_t argument) {
    GteShortVector target;
    GteShortVector angles;
    GteMatrix matrix;
    int count;

    switch (mode) {
    case 0:
        count = FieldFx_InitCallbackList(ANIM_POOL_AS(RoomM273PoolContext)->pool, 0x14, 4,
                              RoomM273_SwayShardCallback);
        if (count < 0) return count;
        {
            int extra = FieldFx_AllocCallbackList(&g_RoomM273SwayShardTrailPool, count, 8, 0x14, RoomM273_SwayShardTrailCallback);
            if (extra < 0) return extra;
            count += extra;
        }
        return count;
    case 1:
        if (g_RoomM273SwayState.done) return 2;
        if (ANIM_OWNER_AS(RoomM273BossActor)->instance->mode == 15 && g_RoomM273SwayState.frame >= 4 &&
            g_RoomM273SwayState.frame_1A < 4) {
            int i;
            for (i = 0; i < 2; i++) {
                RoomM273SwayShard *shard = FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
                RoomM273BossPlayer *player;
                int dx;
                int dy;
                int dz;
                if (shard == 0) break;
                shard->side = i;
                shard->state = 0;
                player = (RoomM273BossPlayer *)g_PlayerEntity;
                shard->position.x = g_RoomM273SwayState.points[i].x;
                shard->position.y = g_RoomM273SwayState.points[i].y;
                shard->position.z = g_RoomM273SwayState.points[i].z;
                target.x = Pe1Fixed_Integer(&player->pos_x);
                target.y = Pe1Fixed_Integer(&player->pos_y);
                target.z = Pe1Fixed_Integer(&player->pos_z);
                dx = target.x - shard->position.x;
                dz = target.z - shard->position.z;
                dy = target.y - shard->position.y;
                angles.x = (ratan2(dy, Math_IntSqrt(dx * dx + dz * dz)) & 0xFFF) - 0x30;
                angles.y = ANIM_OWNER_AS(RoomM273BossActor)->instance->rot_y;
                angles.z = 0;
                if (angles.x < 0x100) angles.x = 0x100;
                if (angles.x > 0x280) angles.x = 0x280;
                RotMatrixYXZ(&angles, &matrix);
                ApplyMatrixSV(&matrix, &g_RoomM273SwayShardVelocity, &shard->velocity);
            }
            target.x = (g_RoomM273SwayState.points[0].x + g_RoomM273SwayState.points[1].x) / 2;
            target.y = (g_RoomM273SwayState.points[0].y + g_RoomM273SwayState.points[1].y) / 2;
            target.z = (g_RoomM273SwayState.points[0].z + g_RoomM273SwayState.points[1].z) / 2;
            Asset_Find08w(0x5D2, ANIM_OWNER_AS(RoomM273BossActor)->instance->state->progress, target.x, target.y,
                          target.z);
        }
        FieldFx_UpdateCallbackList(g_RoomM273SwayShardTrailPool);
        break;
    case 2:
        g_FieldFxSpriteParams.parameter00 = 0x10;
        g_FieldFxSpriteParams.parameter02 = 1;
        g_FieldFxSpriteParams.extent_x = 0x10;
        g_FieldFxSpriteParams.extent_y = 0x10;
        g_FieldFxSpriteParams.extent_x = 0x10;
        g_FieldFxSpriteParams.extent_y = 0x10;
        {
            int tpage = g_FieldFxTpageTable[g_FieldFxTpageSlots[2]];
            g_FieldFxSpriteParams.palette = 2;
            g_FieldFxSpriteParams.parameter06 = 0;
            g_FieldFxSpriteParams.parameter0A = 0;
            g_FieldFxSpriteParams.depth = 0;
            g_FieldFxSpriteParams.tpage = tpage;
        }
        FieldFx_DrawCallbackList(g_RoomM273SwayShardTrailPool);
        g_FieldFxSpriteParams.parameter00 = 0x20;
        g_FieldFxSpriteParams.parameter02 = 2;
        g_FieldFxSpriteParams.extent_x = 0x20;
        g_FieldFxSpriteParams.extent_y = 0x20;
        {
            int tpage = g_FieldFxTpageTable[g_FieldFxTpageSlots[3]];
            g_FieldFxSpriteParams.palette = 3;
            g_FieldFxSpriteParams.parameter06 = 0;
            g_FieldFxSpriteParams.tpage = tpage;
        }
        break;
    }
    return 0;
}

/* Paired ground sprites and the boss ground ring share sway points and pool records. */

/* The emitter owns the eight-byte pool whose entries this callback renders. */

int RoomM273_SwayGroundCallback(int mode, void *data, intptr_t argument) {
    Vector * input = data;
    Vector position;
    if(mode==1) {
        if(g_FieldFxCurrentAge>=32) return 1;
    } else if(mode==2) {
        unsigned int phase=(unsigned int)(g_FieldFxCurrentAge-1)<<5;
        int shadeSample=g_RoomM273Trig[(phase+1024)&4095].part.cos;
        int sizeSample=g_RoomM273Trig[phase&4095].part.cos;
        unsigned int shade;
        int size,kind,palette;
        unsigned short clut;
        position=*input;
        kind=g_FieldFxSpriteParams.palette;
        shade=(unsigned int)(shadeSample+4096)>>5;
        size=sizeSample*2+4096;
        palette=g_FieldFxPaletteTable[kind];
        clut=GetClut(0,(kind==4 && g_FieldFxAltPaletteFlag) ? palette+8 : palette+4);
        FieldFx_DrawShapeQuads(&position,0,(short)size,(short)size,100,clut,1,(short)shade,0);
        position.y=g_RoomFloorY->raw;
        FieldFx_DrawStarFan(&position,384,384,8,&g_RoomM273FloorRotation,4096,4096,
            (RenderColor *)g_RoomM273SwayGroundColor,(RenderColor *)g_RoomM273BlackColor,(short)shade,1);
    }
    return 0;
}

int RoomM273_SwayGroundEmitter(int mode, void *task_data, intptr_t argument) {
    switch(mode) {
    case 0:
        return FieldFx_InitCallbackList(ANIM_POOL_AS(RoomM273PoolContext)->pool,8,4,
                             RoomM273_SwayGroundCallback);
    case 1: {
        Record *source;
        int i;
        if(g_RoomM273SwayState.done) return 2;
        if(g_RoomM273SwayState.animation!=15) return 0;
        if(g_RoomM273SwayState.frame<4) return 0;
        if(g_RoomM273SwayState.frame_1A>=4) break;
        i=0;
        source=(Record *)g_RoomM273SwayState.points;
        for(;i<2;++i) {
            Record *output=FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
            if(!output) break;
            *output=*source;
            ++source;
        }
        break;
    }
    case 2: {
        unsigned int index=g_FieldFxTpageSlot11;
        unsigned short palette;
        g_FieldFxSpriteParams.parameter00=32;
        g_FieldFxSpriteParams.parameter02=2;
        g_FieldFxSpriteParams.extent_x=32;
        g_FieldFxSpriteParams.extent_y=32;
        g_FieldFxSpriteParams.extent_x=32;
        g_FieldFxSpriteParams.extent_y=32;
        palette=g_FieldFxTpageTable[index];

        g_FieldFxSpriteParams.palette=3;
        g_FieldFxSpriteParams.parameter06=1;
        g_FieldFxSpriteParams.parameter0A=0;
        g_FieldFxSpriteParams.depth=32;
        g_FieldFxSpriteParams.tpage=palette;
        break;
    }
    }
    return 0;
}

/* Ground ring: while it opens (frames up to 24) it knocks the player back
 * once when the player stands inside the pulsing radius; while drawing it
 * shows the swelling ring model, a short flash and two rotating rings. */
int RoomM273_SwayGroundRingCallback(int mode, void *data, intptr_t argument) {
    GteShortVector * position = data;
    GteRotation rotation;
    GteMatrix matrix;
    GteVector scale;
    s16 unused[4];
    int frame;
    int size;
    int fade;
    int value;
    int i;

    if (mode == 1) {
        int now = g_FieldFxCurrentAge;
        if (now >= 0x28) return 1;
        if (now >= 0x19) return 0;
        if (position->pad != 0) return 0;
        if (g_RoomM273SwayState.cooldown != 0) return 0;
        {
            int radius = g_RoomM273Trig[((now << 11) / 40) & 0xFFF].bits.sin / 16 + 0x80;
            int dx = g_PlayerEntity->render_object.model_matrix.translation[0] - position->x;
            int dz = g_PlayerEntity->render_object.model_matrix.translation[2] - position->z;
            if (radius < Math_IntSqrt(dx * dx + dz * dz)) return 0;
        }
        position->pad = 1;
        g_PlayerEntity->state->flags |= 0x4000;
        if (ANIM_OWNER_AS(RoomM273BossActor)->instance->state) {
            ANIM_OWNER_AS(RoomM273BossActor)->instance->state->core_flags |= 0x80000000;
            g_RoomM273SwayState.cooldown = 0x28;
        }
        return 0;
    }
    if (mode != 2) return 0;
    frame = g_FieldFxCurrentAge - 1;
    if (g_FieldFxCurrentAge >= 5 && g_FieldFxCurrentAge < 0x25) {
        int page;
        int kind;
        int palette;
        int cosine;
        int sine;
        i = g_FieldFxCurrentAge - 5;
        if (i < 8) value = g_RoomM273Trig[(i << 7) & 0xF80].bits.sin;
        else value = g_RoomM273Trig[(((g_FieldFxCurrentAge - 13) << 10) / 24) & 0xFFF].bits.cos;
        page = (g_FieldFxTpageTable[g_FieldFxTpageSlot11] | GetTPage(0, 1, 0, 0)) & 0xFFFF;
        fade = value >> 6;
        kind = g_FieldFxSpriteParams.palette;
        palette = g_FieldFxPaletteTable[kind];
        size = value >> 2;
        FieldFx_ModelSetTexture(page, GetClut(0, (kind == 4 && g_FieldFxAltPaletteFlag != 0) ? palette + 6 : palette + 2));
        FieldFx_ModelSetSemiTrans(1);
        FieldFx_ModelSaveColors(g_RoomM273SweepState.model);
        FieldFx_ModelScaleColors(g_RoomM273SweepState.model, (u16)fade);
        cosine = rcos((u32)frame << 8);
        sine = rsin((u32)frame << 8);
        matrix.m[0][2] = sine;
        matrix.m[2][0] = -sine;
        matrix.m[0][0] = cosine;
        matrix.m[2][2] = cosine;
        matrix.t[2] = 0;
        matrix.t[1] = 0;
        matrix.t[0] = 0;
        matrix.m[2][1] = 0;
        matrix.m[1][2] = 0;
        matrix.m[1][0] = 0;
        matrix.m[0][1] = 0;
        matrix.m[1][1] = 0x1000;
        memset(&scale, 0, sizeof(scale));
        scale.x = size;
        scale.y = size;
        scale.z = size;
        ScaleMatrix(&matrix, &scale);
        matrix.t[0] = position->x;
        matrix.t[1] = position->y;
        matrix.t[2] = position->z;
        FieldFx_ModelDraw((union FieldTexturedModel *)g_RoomM273SweepState.model, &matrix);
        FieldFx_ModelRestoreColors(g_RoomM273SweepState.model);
    }
    g_FieldFxSpriteParams.parameter00 = 0x20;
    g_FieldFxSpriteParams.parameter02 = 2;
    g_FieldFxSpriteParams.extent_x = 0x20;
    g_FieldFxSpriteParams.extent_y = 0x20;
    g_FieldFxSpriteParams.extent_x = 0x20;
    g_FieldFxSpriteParams.extent_y = 0x20;
    g_FieldFxSpriteParams.tpage = g_FieldFxTpageTable[TEXTURE_SLOT_RECORD(g_FieldFxTpageSlot3)];
    g_FieldFxSpriteParams.palette = 3;
    g_FieldFxSpriteParams.parameter06 = 0;
    if (frame < 8) {
        int kind = g_FieldFxSpriteParams.palette;
        int palette = g_FieldFxPaletteTable[kind];
        if (kind == 4 && g_FieldFxAltPaletteFlag != 0) palette += 4;
        size = g_RoomM273Trig[((u32)frame << 7) & 0xF80].bits.sin + 0x1800;
        FieldFx_DrawShapeQuads(position, 0, size, size, (s16)g_FieldFxSpriteParams.parameter02 * frame + 0x80,
                      GetClut(0x10, palette), 1, 0x80, 0);
    }
    rotation.x = 0x400;
    rotation.y = -frame * 512;
    rotation.flags = 1;
    rotation.z = 0;
    fade = g_RoomM273Trig[(frame * 25) & 0xFFF].bits.cos >> 5;
    value = g_RoomM273Trig[(frame * 51) & 0xFFF].bits.sin / 16;
    for (i = 0; i < 2; i++) {
        FieldFx_DrawStarFan(position, value, value, 8, &rotation, 0x1000, 0x1000,
                      &g_RoomM273SwayGroundRingColors[0], &g_RoomM273SwayGroundRingColors[1], fade, 1);
        value += 0x100;
    }
    return 0;
}

int RoomM273_SwayGroundRingEmitter(int mode, void *task_data, intptr_t argument) {
    switch (mode) {
    case 0:
        g_RoomM273SweepState.model = Asset_FindTable08ByU32Key(g_LoadedSceneAssetBlock[0], 0xC5541704);
        FieldFx_ModelScrollUv((union FieldTexturedModel *)g_RoomM273SweepState.model, 0, 0);
        return FieldFx_InitCallbackList(ANIM_POOL_AS(RoomM273PoolContext)->pool, 8, 4,
                             RoomM273_SwayGroundRingCallback);
    case 1: {
        int i;
        unsigned char *flags;
        Entry *source;
        unsigned char *anchor = &g_RoomM273SwayState.done;
        if (*anchor) return 2;
        i = 0;
        flags = (unsigned char *)&g_RoomM273SwayState.hits[0].pad;
        source = (Entry *)g_RoomM273SwayState.hits;
        for (; i < 2; ++i) {
            if (*(unsigned short *)flags & 1) {
                Entry *entry = FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
                if (!entry) return 0;
                entry->emitter.x = source->emitter.x;
                entry->emitter.y = source->emitter.y;
                entry->emitter.z = source->emitter.z;
                entry->emitter.flags = 0;
                *(unsigned short *)flags &= 0xFFFE;
            }
            flags += 8;
            ++source;
        }
        break;
    }
    case 2:
        g_FieldFxSpriteParams.parameter0A = 0;
        g_FieldFxSpriteParams.depth = 0;
        break;
    }
    return 0;
}

int RoomM273_SwayBurstCallback(int mode, void *data, intptr_t argument) {
    RoomM273RisingParticle * particle = data;
    if (mode == 1) {
        int frame = g_FieldFxCurrentAge;
        if (frame >= 36) return 1;
        if (frame >= 4) {
            unsigned short y = ((volatile RoomM273RisingParticle *)particle)->y;
            s16 speed = ((volatile RoomM273RisingParticle *)particle)->speed;
            int delta = (particle)->speed;
            speed += 1;
            particle->y = y - delta;
            particle->speed = speed;
        }
    } else if (mode == 2) {
        int frame = g_FieldFxCurrentAge - 5;
        int kind, palette, clut;
        if (frame < 0) return 0;
        kind = g_FieldFxSpriteParams.palette;
        palette = g_FieldFxPaletteTable[kind];
        if (kind == 4 && g_FieldFxAltPaletteFlag) palette += 4;
        clut = GetClut(16, palette);
        FieldFx_DrawShapeQuads((GteShortVector *)particle, 0, 12288, 12288,
            (s16)g_FieldFxSpriteParams.parameter02 * (frame / 4) + 200, clut, 1,
            (short)((int *)g_RoomM273Trig)[(((unsigned int)(u32)frame << 8) & 0x3F00) / 4] >> 5, 0);
    }
    return 0;
}

int RoomM273_SwayBurstEmitter(int mode, void *task_data, intptr_t argument)
{
    RoomM273TemplatePoint *template;
    RoomM273DelayedBurstRecord *particle;
    const GteSineWord *entry;
    int i;
    int j;
    int angle;
    int offset;
    int flagOffset;
    int sixteen;
    u16 paletteIndex;
    u8 *stop;
    u16 *flag;
    RoomM273TemplatePoint *source;
    u16 palette;

    switch (mode) {
    case 0:
        return FieldFx_InitCallbackList(ANIM_POOL_AS(RoomM273PoolContext)->pool, 8, 24, RoomM273_SwayBurstCallback);
    case 1:
        stop = &g_RoomM273SwayState.done;
        if (*stop) return 2;
        i = 0;
        template = (RoomM273TemplatePoint *)g_RoomM273SwayState.hits;
        offset = 0;
        do {
            flag = (u16 *)((u8 *)&g_RoomM273SwayState.hits[0].pad + offset);
            if (*flag & 2) {
                j = 0;
                flagOffset = offset;
                source = template;
                do {
                    particle = (RoomM273DelayedBurstRecord *)FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
                    if (!particle) break;
                    particle->emitter = *source;
                    angle = ((j << 12) / 6) & 0xFFF;
                    entry = &g_RoomM273Trig[angle];
                    particle->emitter.x += (entry->part.sin * 3 * 64) / 4096;
                    particle->emitter.z += (entry->part.cos * 3 * 64) / 4096;
                    particle->emitter.flags = 0;
                    *(u16 *)((u8 *)&g_RoomM273SwayState.hits[0].pad + flagOffset) &= ~2;
                    j++;
                } while (j < 6);
            }
            template++;
            i++;
            offset += 8;
        } while (i < 2);
        break;
    case 2:
        paletteIndex = g_FieldFxTpageSlot2;
        sixteen = 16;
        g_FieldFxSpriteParams.parameter00 = sixteen;
        g_FieldFxSpriteParams.parameter02 = 1;
        g_FieldFxSpriteParams.extent_x = sixteen;
        g_FieldFxSpriteParams.extent_y = sixteen;
        g_FieldFxSpriteParams.extent_x = sixteen;
        g_FieldFxSpriteParams.extent_y = sixteen;
        palette = g_FieldFxTpageTable[paletteIndex];
        g_FieldFxSpriteParams.palette = 2;
        g_FieldFxSpriteParams.parameter06 = 0;
        g_FieldFxSpriteParams.parameter0A = 0;
        g_FieldFxSpriteParams.depth = 0;
        g_FieldFxSpriteParams.tpage = palette;
        break;
    }
    return 0;
}

/* Pointer sprite pool and boss sway state share g_RoomM273SwayState points. */

/* The emitter owns the pointer entries rendered by this callback. */

int RoomM273_SwayPointerCallback(int mode, void *data, intptr_t argument) {
    RoomM273PointerPoolEffect * effect = data;
    /* Layout only: the original purpose of these eight frame bytes is unknown. */
    int stack_pad[2];
    if (mode == 1) {
        if (g_FieldFxCurrentAge >= 16) return 1;
    } else if (mode == 2) {
        int frame = (short)(g_FieldFxCurrentAge - 1);
        int kind = g_FieldFxSpriteParams.palette;
        unsigned int sizeOffset, shadeOffset;
        s16 size;
        s16 shade;
        int palette;
        unsigned short clut;
        sizeOffset = ((unsigned int)(u32)frame << 8) & 0x3F00;
        shadeOffset = ((unsigned int)(u32)frame << 9) & 0x3E00;
        size = g_RoomM273Trig[sizeOffset / 4].word + 2048;
        shade = (short)g_RoomM273Trig[shadeOffset / 4].word >> 6;

        palette = g_FieldFxPaletteTable[kind];
        clut = GetClut(0, (kind == 4 && g_FieldFxAltPaletteFlag) ? palette + 7 : palette + 3);
        FieldFx_DrawShapeQuads(effect->position, 0, size, size,
            64, clut, 1, shade, 0);
    }
    return 0;
}

int RoomM273_SwayPointerEmitter(int mode, void *task_data, intptr_t argument) {
    switch(mode) {
    case 0:
        g_RoomM273SwayPointerSpawned=0;
        return FieldFx_InitCallbackList(ANIM_POOL_AS(RoomM273PoolContext)->pool,sizeof(void *),6,
                             RoomM273_SwayPointerCallback);
    case 1: {
        int i;
        Position *position;
        if(g_RoomM273SwayPointerSpawned) return 2;
        if(g_RoomM273SwayState.animation==15 && g_RoomM273SwayState.frame>=25) {
            g_RoomM273SwayPointerSpawned=1;
            return 2;
        }
        if(g_FieldFxCurrentAge&7) return 0;
        i=0;
        position=g_RoomM273SwayState.points;
        for(;i<2;++i) {
            RoomM273PointerPoolEffect *output=FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
            if(!output) break;
            output->position=position;
            ++position;
        }
        break;
    }
    case 2: {
        unsigned int index=g_FieldFxTpageSlot11;
        unsigned short palette;
        g_FieldFxSpriteParams.parameter00=64;
        g_FieldFxSpriteParams.parameter02=4;
        g_FieldFxSpriteParams.extent_x=64;
        g_FieldFxSpriteParams.extent_y=64;
        g_FieldFxSpriteParams.extent_x=64;
        g_FieldFxSpriteParams.extent_y=64;
        palette=g_FieldFxTpageTable[index];

        g_FieldFxSpriteParams.palette=3;
        g_FieldFxSpriteParams.parameter06=1;
        g_FieldFxSpriteParams.parameter0A=0;
        g_FieldFxSpriteParams.depth=32;
        g_FieldFxSpriteParams.tpage=palette;
        break;
    }
    }
    return 0;
}

/* Animation 15 controller: transforms two model points, then sways the
 * boss toward or away from the player for eight frames. */
int RoomM273_SwayController(int mode, void *task_data, intptr_t argument) {
    RoomM273BossInstance *instance = ANIM_OWNER_AS(RoomM273BossActor)->instance;
    GteShortVector offset;
    int value; /* loop index, then the player heading */
    int frame;
    int frame_1A;
    GteShortVector *point;

    if (mode == 0) {
        g_RoomM273SwayState.done = 0;
        instance->state->core_flags |= 0x40000000;
        g_RoomM273SwayState.points[0].pad = 0;
        g_RoomM273SwayState.points[1].pad = 1;
        g_RoomM273SwayState.hits[0].pad = 0;
        g_RoomM273SwayState.hits[1].pad = 0;
        g_RoomM273SwayState.sway_timer = 0;
        g_RoomM273SwayState.cooldown = 0;
    } else if (mode == 1) {
        if (instance->state->action_state && *instance->state->action_state == 1) *instance->state->action_state = 2;
        frame = instance->anim.parts.integer;
        frame_1A = (u32)instance->anim_prev >> 16;
        g_RoomM273SwayState.animation = instance->mode;
        g_RoomM273SwayState.frame = frame;
        g_RoomM273SwayState.frame_1A = frame_1A;
        offset.x = 0;
        offset.y = 0;
        offset.z = -0x130;
        point = g_RoomM273SwayState.points;
        for (value = 0; value < 2; value++) {
            GteMatrix *m = (GteMatrix *)instance->render_object.matrices;
            if (value) m += 36;
            else m += 25;
            gte_ldrotmatrix(m);
            gte_ldtransmatrix(m);
            gte_ldv0(&offset);
            gte_rtv0tr_mac();
            gte_stsv(&point[value]);
        }
        if (instance->mode == 15) {
            if ((s16)frame >= instance->action - 1) {
                g_RoomM273SwayState.done = 1;
                if (instance->state && instance->state->action_state) *instance->state->action_state = 4;
                return 1;
            }
            if ((s16)frame >= 0x16) {
                if (g_RoomM273SwayState.repeat-- > 0) instance->anim.fixed = 0;
            }
        }
        if (g_RoomM273SwayState.repeat && (s16)frame >= 0xF && (s16)frame_1A < 0xF) {
            int step;
            g_RoomM273SwayState.sway_timer = 8;
            value = (s16)FieldFx_VecToAngle((Vec3Fixed *)g_PlayerEntity->render_object.model_matrix.translation, (Vec3Fixed *)instance->render_object.model_matrix.translation);
            step = 0x20;
            if ((value - instance->rot_y) & 0x800) step = -0x20;
            g_RoomM273SwayState.sway_step = step;
        }
        if (g_RoomM273SwayState.sway_timer) {
            instance->rot_y += g_RoomM273SwayState.sway_step *
                             g_RoomM273Trig[(g_RoomM273SwayState.sway_timer << 7) & 0xF80].bits.sin / 4096;
            g_RoomM273SwayState.sway_timer--;
        }
        if (g_RoomM273SwayState.cooldown) g_RoomM273SwayState.cooldown--;
    }
    return 0;
}

intptr_t RoomM273_GrabEffectConfigure(int arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3) {
    short *ptr = &g_RoomM273GrabColorIndex;

    *ptr = arg2;
    return (intptr_t)&g_RoomM273GrabModel;
}

int RoomM273_GrabSparkTriangleCallback(int mode, void *data, intptr_t argument) {
    RoomM273PulseState * state = data;
    RoomM273Vector position, rotation;
    s32 frame;
    s32 *source;
    s32 positionX;
    int scaleX, scaleY;
    char *texture;

    if (mode == 1) {
        if (g_FieldFxCurrentAge >= 9) return 1;
        state->value += state->step;
    } else if (mode == 2) {
        source = state->source;
        positionX = source[0];
        position.y = source[1];
        position.z = source[2];
        rotation.x = 0;
        rotation.y = state->value;
        rotation.z = 0;
        frame = g_FieldFxCurrentAge - 1;
        scaleX = state->scaleX;
        scaleY = state->scaleY;
        texture = g_RoomM273BlackColor;
        position.x = positionX;
        FieldFx_DrawRotatedTriangle(&position, (GteRotation *)&rotation, scaleX, scaleY, (RenderColor *)state,
                       (RenderColor *)texture, (RenderColor *)texture,
                       g_RoomM273Trig[((u32)frame << 7) & 0xF80].bits.cos >> 5, 1);
    }
    return 0;
}


s32 RoomM273_GrabHoldFlareEmitter(int state, void *task_data, intptr_t argument) {
    GteShortVector *obj;
    RoomM273PlayerActorView *root;
    RoomM273ThresholdTransform *source;
    u8 status;

    if (state != 1) {
        if (state >= 2) {
            return 0;
        }
        if (state != 0) {
            return 0;
        }
        g_RoomM273HoldFlareSpawned = 0;
        return FieldFx_InitCallbackList(ANIM_POOL_AS(RoomM273PoolContext)->pool, 8, 5,
                             RoomM273_GrabHoldFlareCallback);
    }
    if (g_RoomM273HoldFlareSpawned != 0) {
        return 2;
    }

    root = ANIM_OWNER_AS(RoomM273EffectStateContext)->state.player;
    status = root->mode;
    if (status != 0x10 && status != 8) {
        g_RoomM273HoldFlareSpawned = 1;
        return 2;
    }
    if ((g_FieldFxCurrentAge & 3) != 0) {
        return 0;
    }

    obj = FieldFx_AcquireCallbackSlot(ANIM_POOL_AS(RoomM273PoolContext)->pool);
    if (obj != 0) {
        root = ANIM_OWNER_AS(RoomM273EffectStateContext)->state.player;
        source = (RoomM273ThresholdTransform *)root->render_object.matrices;
        obj->x = source->x;
        obj->y = source->y;
        obj->z = source->z;
    }
    return 0;
}

intptr_t RoomM273_HandTrailConfigure(int unused, intptr_t mode, intptr_t arg2, intptr_t arg3) {
    int direction;
    int value;

    g_RoomM273HandTrailState.spinning = mode;
    switch (mode) {
    case 0:
        g_RoomM273HandTrailState.spin_target = 0;
        g_RoomM273HandTrailState.repeat = 2;
        break;
    case 1:
        direction = (Inv_ScrambleGrid() & 1) ? 42 : -42;
        g_RoomM273HandTrailState.spin_step = 2;
        g_RoomM273HandTrailState.repeat = 2;
        value = 0x61;
        g_RoomM273HandTrailState.spin_target = direction;
        g_RoomM273HandTrailState.tick_limit = value;
        break;
    case 2:
        direction = (Inv_ScrambleGrid() & 1) ? 0x80 : -0x80;
        g_RoomM273HandTrailState.spin_step = 4;
        g_RoomM273HandTrailState.repeat = 5;
        value = 0xB0;
        g_RoomM273HandTrailState.spin_target = direction;
        g_RoomM273HandTrailState.tick_limit = value;
        break;
    }
    return (intptr_t)&g_RoomM273HandTrailState;
}

int RoomM273_HandTrailLandingFanCallback(int mode, void *data, intptr_t argument) {
    RoomM273PhaseState * state = data;
    RoomM273Vector position;
    int frame, size;
    const GteSineWord *entry;

    if (mode == 1) {
        if (g_FieldFxCurrentAge >= 16) return 1;
    } else if (mode == 2) {
        frame = g_FieldFxCurrentAge - 1;
        entry = &g_RoomM273Trig[(((u32)frame << 8) & 0x3F00) >> 2];
        size = (s16)(entry->part.sin * 2 + 2048);
        position.x = state->source[0];
        position.y = g_RoomFloorY->raw;
        position.z = state->source[2];
        FieldFx_DrawStarFan(&position, 192, 192, 10,
                       (GteRotation *)(g_RoomM273LandingFanRotations + ((frame & 1) << 3)), size, size,
                       (RenderColor *)g_RoomM273BlackColor, (RenderColor *)g_RoomM273LandingFanColor, entry->part.cos >> 5, 1);
    }
    return 0;
}

void RoomM273_GrabNop0(void);
void RoomM273_GrabInit(PmSlotHeader *);
int RoomM273_GrabConfigure(PmSlotHeader *, int, int, intptr_t, intptr_t, intptr_t);
int RoomM273_GrabNop3(PmSlotHeader *);
void RoomM273_GrabNop6(void);
int RoomLib_UpdateCallback(PmSlotHeader *);


/* Original USA M273 constants and native tables; addresses include the archive prefix. */
u8 g_RoomM273ActionWeights[672] = {78, 12, 0, 0, 0, 255, 255, 255, 32, 0, 0, 0, 192, 255, 255, 255, 0, 12, 0, 2, 64, 2, 0, 0, 9, 7, 3, 0, 4, 37, 25, 128, 12, 37, 25, 128, 176, 37, 25, 128, 8, 38, 25, 128, 16, 38, 25, 128, 140, 45, 25, 128, 60, 46, 25, 128, 68, 46, 25, 128, 76, 46, 25, 128, 100, 46, 25, 128, 84, 47, 25, 128, 92, 47, 25, 128, 232, 48, 25, 128, 12, 49, 25, 128, 0, 0, 0, 0, 0, 10, 90, 0, 0, 0, 0, 0, 10, 90, 0, 0, 0, 0, 0, 10, 90, 0, 0, 0, 0, 0, 10, 90, 0, 0, 0, 0, 0, 50, 50, 0, 0, 0, 0, 0, 30, 70, 0, 0, 0, 0, 0, 30, 70, 0, 0, 0, 0, 0, 50, 50, 0, 0, 0, 0, 0, 80, 20, 0, 0, 0, 0, 0, 80, 20, 0, 0, 0, 0, 0, 80, 20, 0, 0, 0, 0, 0, 80, 20, 0, 0, 0, 0, 50, 0, 50, 0, 0, 0, 0, 50, 0, 50, 0, 0, 0, 0, 50, 0, 50, 0, 0, 0, 0, 50, 0, 50, 0, 0, 50, 0, 30, 0, 20, 0, 0, 30, 0, 50, 0, 20, 0, 0, 0, 30, 50, 0, 20, 0, 0, 0, 50, 30, 0, 20, 0, 0, 70, 0, 20, 0, 10, 0, 0, 60, 0, 20, 0, 20, 0, 0, 0, 60, 20, 0, 20, 0, 0, 0, 70, 20, 0, 10, 0, 0, 0, 0, 50, 0, 50, 0, 0, 0, 0, 40, 0, 60, 0, 0, 0, 0, 40, 0, 60, 0, 0, 0, 0, 50, 0, 50, 0, 0, 30, 0, 20, 20, 30, 0, 0, 30, 0, 30, 0, 40, 0, 0, 0, 30, 30, 0, 40, 0, 0, 0, 30, 20, 20, 30, 0, 0, 30, 0, 20, 40, 10, 0, 0, 20, 0, 30, 0, 50, 0, 0, 0, 20, 30, 0, 50, 0, 0, 0, 30, 20, 40, 10, 0, 50, 0, 0, 0, 0, 50, 50, 20, 0, 0, 0, 0, 30, 50, 20, 0, 0, 0, 0, 30, 0, 50, 0, 0, 0, 0, 50, 0, 50, 0, 0, 0, 0, 50, 30, 40, 0, 0, 0, 0, 30, 30, 40, 0, 0, 0, 0, 30, 0, 50, 0, 0, 0, 0, 50, 0, 70, 0, 0, 0, 0, 30, 30, 50, 0, 0, 0, 0, 20, 30, 50, 0, 0, 0, 0, 20, 0, 70, 0, 0, 0, 0, 30, 0, 40, 0, 0, 0, 0, 60, 30, 20, 0, 0, 0, 0, 50, 30, 20, 0, 0, 0, 0, 50, 0, 40, 0, 0, 0, 0, 60, 0, 30, 0, 0, 0, 40, 30, 50, 20, 0, 0, 0, 0, 30, 50, 20, 0, 0, 0, 0, 30, 0, 30, 0, 0, 0, 40, 30, 0, 30, 0, 0, 0, 50, 20, 60, 30, 0, 0, 0, 0, 10, 60, 30, 0, 0, 0, 0, 10, 0, 30, 0, 0, 0, 50, 20, 0, 30, 0, 0, 40, 0, 30, 50, 20, 0, 0, 20, 0, 10, 50, 20, 0, 0, 20, 0, 10, 0, 30, 0, 0, 40, 0, 30, 0, 20, 30, 0, 30, 0, 20, 50, 20, 0, 0, 20, 0, 10, 50, 20, 0, 0, 20, 0, 10, 0, 20, 0, 30, 30, 0, 20, 0, 20, 60, 0, 10, 0, 10, 40, 40, 0, 0, 10, 0, 10, 40, 40, 0, 0, 10, 0, 10, 0, 20, 0, 60, 10, 0, 10, 0, 0, 0, 0, 50, 0, 50, 50, 0, 0, 0, 20, 0, 30, 50, 0, 0, 0, 20, 0, 30, 0, 0, 0, 0, 50, 0, 50, 0, 0, 25, 0, 25, 25, 25, 70, 0, 0, 0, 15, 0, 15, 70, 0, 0, 0, 15, 0, 15, 0, 0, 0, 25, 25, 25, 25, 0, 0, 50, 0, 0, 50, 0, 70, 0, 0, 0, 15, 0, 15, 70, 0, 0, 0, 15, 0, 15, 0, 0, 0, 50, 0, 50, 0};
u8 g_RoomM273GrabSelectionByIndex[4] = {9, 7, 3, 0};
int g_RoomM273GrabColors[4] = {90, 3287040, 10290, 2631720};
s16 g_RoomM273GrabModelRingX[4] = {1280, 1792, 2304, 0};
int g_RoomM273GrabModelRingZ[3] = {0, 14688416, 0};
s16 g_RoomM273HandTrailLandingPulseScale[4] = {22, 23, 38, 39};
s16 g_RoomM273SweepSplashScale[4] = {0, 1, 16, 17};
char g_RoomM273LandingFanColor[4] = {96, 32, 16, 0};
char g_RoomM273LandingFanRotations[16] = {0, 4, 0, 0, 0, 0, 1, 0, 0, 4, 0, 0, -52, 0, 1, 0};
char g_RoomM273SwayShardTrailColor[4] = {-128, 80, 48, 0};
u8 g_RoomM273BlackColor[4] = {0, 0, 0, 0};
u8 g_RoomM273SweepRisingColor[4] = {128, 128, 128, 0};
u8 g_RoomM273SweepFlashColorA[4] = {48, 32, 0, 0};
u8 g_RoomM273SweepFlashColorB[4] = {40, 24, 24, 0};
u8 g_RoomM273SwayGroundColor[4] = {128, 64, 32, 0};
u8 g_RoomM273HandRingBandColorA[4] = {0, 0, 0, 0};
u8 g_RoomM273HandRingBandColorB[4] = {64, 16, 0, 0};
char g_RoomM273SweepArcColor[4] = {48, 32, 0, 0};
GteRotation g_RoomM273FloorRotation = {1024, 0, 0, 1};
GteRotation g_RoomM273GrabInitAngles = {3072, 512, 576, 0};
GteShortVector g_RoomM273HandTrailDropOffset = {0, 0, 0, 0};
GteShortVector g_RoomM273HandTrailSampleOffsetA = {-96, 0, -128, -256};
GteShortVector g_RoomM273HandTrailSampleOffsetB = {96, 0, -128, 256};
GteShortVector g_RoomM273SweepDropVector = {0, 0, -192, 0};
GteShortVector g_RoomM273SwayShardVelocity = {0, 0, -96, 0};
Color g_RoomM273HandTrailHitColor = {192, 192, 192, 0};
RenderColor g_RoomM273HandTrailColor = {128, 96, 96, 0};
RenderColor g_RoomM273SweepDropColorA = {192, 96, 48, 0};
RenderColor g_RoomM273SweepDropColorB = {32, 16, 8, 0};
RenderColor g_RoomM273SweepDropColorC = {128, 80, 32, 0};
RenderColor g_RoomM273SwayShardRingColor = {192, 192, 128, 0};
RenderColor g_RoomM273SwayShardStarColor = {192, 128, 64, 0};
RenderColor g_RoomM273SwayShardInnerStarColor = {128, 64, 32, 0};
RenderColor g_RoomM273SwayGroundRingColors[2] = {{240,128,64,0},{0,0,0,0}};
RoomM273PaletteWord g_RoomM273StarBurstColors[4] = {{{48,24,24,0}},{{24,48,24,0}},{{24,24,48,0}},{{40,40,24,0}}};
RoomM273PulseSeed g_RoomM273PlayerStreakSeeds[2] = {{{0,254},256,{0,0,1,0}},{{0,254},65280,{0,0,1,0}}};
GteMatrix g_RoomM273GrabInitMatrix = {{{3150,0,0},{0,3150,0},{0,0,3150}},{-256,32,-64}};
int g_RoomM273GrabInitTranslationX = -256;
int g_RoomM273GrabInitTranslationY = 32;
int g_RoomM273GrabInitTranslationZ = -64;
RoomM273SweepStep g_RoomM273SweepSteps[2] = {{0,13,19,56,-1390,-1280},{63,80,86,119,1309,1280}};
RoomM273BossState g_RoomM273HandTrailState;
RoomM273DropQueue g_RoomM273SweepState;
RoomM273SwayStorage g_RoomM273SwayStorage;
RoomM273SweepPoint g_RoomM273SweepPoint;
AssetState g_RoomM273GrabModel;
void *g_RoomM273GrabSparkPool, *g_RoomM273HandTrailLandingFanPool, *g_RoomM273HandRingBandPool;
u8 g_RoomM273PlayerPulseSpawned, g_RoomM273GrabEffectStopped, g_RoomM273HoldFlareSpawned;
s16 g_RoomM273GrabColorIndex, g_RoomM273PlayerTrailBurstsLeft, g_RoomM273PlayerTrailDelay;
s16 g_RoomM273HandTrailPhase, g_RoomM273HandTrailCooldown, g_RoomM273HandTrailPeriodA, g_RoomM273HandTrailPeriodB;
s16 g_RoomM273SweepRisingDelay, g_RoomM273SwayPointerSpawned;
int RoomM273_SweepArcEmitter(int, void *, intptr_t);
int RoomM273_SweepArcCallback(int, void *, intptr_t);
intptr_t RoomM273_SweepConfigure(int, intptr_t, intptr_t, intptr_t);
intptr_t RoomM273_SwayConfigure(int, intptr_t, intptr_t, intptr_t);
static u16 g_RoomM273GrabPlacementEffectsScript[16] = {2,0,0,4,12,0,2,1,0,2,2,0,0,0,0,0};
FieldFxTaskProgram g_RoomM273GrabPlacementEffects = {{{RoomM273_GrabPlacementFanEmitter,RoomM273_GrabPlacementScaledFanEmitter,RoomM273_GrabPlayerPulseEmitter,NULL,NULL,NULL,NULL,NULL},{0,0,0,0,0,0,0,0}},NULL,g_RoomM273GrabPlacementEffectsScript};
static u16 g_RoomM273GrabEffectsScript[24] = {2,7,0,2,4,0,4,60,0,2,0,0,2,1,0,2,2,0,2,3,0,0,0,0};
FieldFxTaskProgram g_RoomM273GrabEffects = {{{RoomM273_GrabModelEmitter,RoomM273_GrabPlayerStreakEmitter,RoomM273_GrabPlayerTrailEmitter,RoomM273_GrabStarBurstEmitter,RoomM273_GrabHoldFlareEmitter,NULL,NULL,RoomM273_GrabEffectController},{0,0,0,0,0,0,0,0}},RoomM273_GrabEffectConfigure,g_RoomM273GrabEffectsScript};
static u16 g_RoomM273HandTrailEffectsScript[22] = {2,7,0,4,1,0,2,0,0,2,1,0,2,2,0,2,3,0,0,0,0,0};
FieldFxTaskProgram g_RoomM273HandTrailEffects = {{{RoomM273_HandTrailEmitter,RoomM273_HandTrailLandingEmitter,RoomM273_HandTrailHitEmitter,RoomM273_HandGlowEmitter,NULL,NULL,NULL,RoomM273_HandTrailController},{0,0,0,0,0,0,0,0}},RoomM273_HandTrailConfigure,g_RoomM273HandTrailEffectsScript};
static u16 g_RoomM273SweepEffectsScript[28] = {2,7,0,2,5,0,4,1,0,2,0,0,2,1,0,2,2,0,2,3,0,2,4,0,0,0,0,0};
FieldFxTaskProgram g_RoomM273SweepEffects = {{{RoomM273_SweepDropEmitter,RoomM273_SweepSplashEmitter,RoomM273_SweepDropHitEmitter,RoomM273_SweepRisingEmitter,RoomM273_SweepFlashEmitter,RoomM273_SweepArcEmitter,NULL,RoomM273_SweepController},{0,0,0,0,0,0,0,0}},RoomM273_SweepConfigure,g_RoomM273SweepEffectsScript};
static u16 g_RoomM273SwayEffectsScript[24] = {2,7,0,2,4,0,4,1,0,2,0,0,2,1,0,2,2,0,2,3,0,0,0,0};
FieldFxTaskProgram g_RoomM273SwayEffects = {{{RoomM273_SwayShardEmitter,RoomM273_SwayGroundEmitter,RoomM273_SwayGroundRingEmitter,RoomM273_SwayBurstEmitter,RoomM273_SwayPointerEmitter,NULL,NULL,RoomM273_SwayController},{0,0,0,0,0,0,0,0}},RoomM273_SwayConfigure,g_RoomM273SwayEffectsScript};

PmCommand g_RoomM273GrabClass = {RoomM273_GrabNop0,RoomM273_GrabInit,RoomM273_GrabConfigure,RoomM273_GrabNop3,RoomLib_UpdateCallback,RoomM273_GrabRelease,RoomM273_GrabNop6};
PmCommand g_RoomM273OscillationClass = {RoomM273_OscillationNop0,RoomM273_OscillationInit,RoomM273_OscillationConfigure,RoomM273_OscillationNop3,RoomM273_OscillationUpdate,RoomM273_OscillationRelease,RoomM273_OscillationNop6};

/* Loading an overlay restored its complete work area. Static linking must do
 * the same on every room load, including a later Continue or scene reload. */
void RoomM273_ResetModule(void) {
    memset(&g_RoomM273HandTrailState, 0, sizeof(g_RoomM273HandTrailState));
    memset(&g_RoomM273SweepState, 0, sizeof(g_RoomM273SweepState));
    memset(&g_RoomM273SwayStorage, 0, sizeof(g_RoomM273SwayStorage));
    memset(&g_RoomM273SweepPoint, 0, sizeof(g_RoomM273SweepPoint));
    memset(&g_RoomM273GrabModel, 0, sizeof(g_RoomM273GrabModel));
    g_RoomM273GrabSparkPool = g_RoomM273HandTrailLandingFanPool = g_RoomM273HandRingBandPool = NULL;
    g_RoomM273PlayerPulseSpawned = g_RoomM273GrabEffectStopped = g_RoomM273HoldFlareSpawned = 0;
    g_RoomM273GrabColorIndex = g_RoomM273PlayerTrailBurstsLeft = g_RoomM273PlayerTrailDelay = 0;
    g_RoomM273HandTrailPhase = g_RoomM273HandTrailCooldown = 0;
    g_RoomM273HandTrailPeriodA = g_RoomM273HandTrailPeriodB = 0;
    g_RoomM273SweepRisingDelay = g_RoomM273SwayPointerSpawned = 0;
    g_RoomM273HandTrailDropOffset.z = 0;
    for (int angle = 0; angle < 4096; ++angle) {
        g_RoomM273Trig[angle].part.sin = rsin(angle);
        g_RoomM273Trig[angle].part.cos = rcos(angle);
    }
}

GteSineWord g_RoomM273Trig[4096];
