#include "common.h"
#include "pe1/gpu_image.h"
#include "pe1/gte.h"
#include "pe1/psyq_gpu.h"
#include "pe1/render_packets.h"

#include "fx_common.h"
#include "fx_common_model.h"
#include "fx_common_render.h"
#include "model_buffers_externs.h"

#define FX_COMMON_LINK_PACKET(packet) do { \
    FxCommonBuffer *buffer=g_FxCommonActiveBuffer; \
    if((unsigned)sp->orderingDepth>=4096){buffer->error=-2;buffer->data=(u8 *)(packet);return;} \
    RenderNativeTag *ordering=(RenderNativeTag *)buffer->allocation; \
    (packet)->tag.address=ordering[sp->orderingDepth].address; \
    ordering[sp->orderingDepth].address=(uintptr_t)(packet); \
} while(0)

/* Draw all eight polygon streams, subdividing nearby FT3/FT4 primitives. */
void FxCommon_DrawPolyResource(void *context, FxCommonPolyResource *resource) {
    (void)context;
    u32 *gteScreen1;
    u32 *gteScreen0;
    FxCommonRenderScratchpad *sp;
    FxCommonVector *quadMidpoint13;
    FxCommonBuffer *buffer;
    s32 resourceOffset;
    s32 resource_offset2;
    FxCommonColoredTexturedTriangle *gt3Cursor;
    FxCommonColoredTexturedQuad *gt4Cursor;
    FxCommonVector *quadMidpoint23;
    FxCommonTexturedQuad *var_fp;
    u8 *var_s2;
    u8 *var_s3;
    u8 *var_s5;
    FxCommonColoredQuad *var_s7;
    u8 *var_t0;
    FxCommonTexturedTriangle *var_t3;
    u8 *var_t5;
    u8 *var_t6;

    s32 temp_a0;

    s32 var_s4;
    u8 *var_s1;
    FxCommonFt4Packet *var_t1_2;
    FxCommonPolyResource *var_t8;
    s32 primitiveCount;
    s32 initialDepthBias;

    var_t8 = resource;
    var_s4 = 0;
    resourceOffset = var_t8->offsets[0];
    resource_offset2 = var_t8->offsets[2];
    buffer = g_FxCommonActiveBuffer;
    var_s3 = ((u8 *)var_t8) + resourceOffset;
    var_t0 = ((u8 *)var_t8) + resource_offset2;
    resourceOffset = var_t8->offsets[1];
    resource_offset2 = var_t8->offsets[4];
    var_s5 = ((u8 *)var_t8) + resourceOffset;
    var_t3 = (FxCommonTexturedTriangle *)(((u8 *)var_t8) + resource_offset2);
    resourceOffset = var_t8->offsets[3];
    resource_offset2 = var_t8->offsets[6];
    var_s7 = (FxCommonColoredQuad *)(((u8 *)var_t8) + resourceOffset);
    resourceOffset = var_t8->offsets[5];
    gt3Cursor = (FxCommonColoredTexturedTriangle *)(((u8 *)var_t8) + resource_offset2);
    initialDepthBias = g_FxCommonDepthBase;
    var_fp = (FxCommonTexturedQuad *)(((u8 *)var_t8) + resourceOffset);
    resourceOffset = var_t8->offsets[7];
    FX_COMMON_SCRATCHPAD->depthBias = initialDepthBias;
    gt4Cursor = (FxCommonColoredTexturedQuad *)(((u8 *)var_t8) + resourceOffset);
    {
        var_s2 = buffer->data;
        sp = FX_COMMON_SCRATCHPAD;
        primitiveCount = var_t8->counts[0];
        if (primitiveCount > 0) {
            FxCommonFlatTriangle *triangle=(FxCommonFlatTriangle *)var_s3;

            s32 temp_v1;

            var_s1 = var_s2 + sizeof(RenderNativeTag);
            do {
                for (;;) {
                    temp_v1 = RotAverageNclip3(
                        &triangle->vertices[0],
                        &triangle->vertices[1],
                        &triangle->vertices[2],
                        &sp->screenCoordinates[0], &sp->screenCoordinates[1],
                        &sp->screenCoordinates[2], &sp->perspective,
                        &sp->orderingDepth, &sp->transformFlags);
                    temp_a0 = sp->orderingDepth;
                    if (temp_a0 > 0) {
                        sp->orderingDepth = ((s32)((u32)temp_a0+(u32)g_FxCommonDepthBase)) >> 2;
                        if (temp_v1 > 0) {
                            if ((unsigned)sp->orderingDepth >= 4096) {
                                buffer->error = -2;
                                buffer->data = (u8 *)var_s2;
                                return;
                            }
                            if (!buffer->end || (uintptr_t)var_s2 > (uintptr_t)buffer->end ||
                                (1) * sizeof(FxCommonF3Packet) > (uintptr_t)buffer->end - (uintptr_t)var_s2) {
                                buffer->error = -1;
                                buffer->data = (u8 *)var_s2;
                                return;
                            }
                            ((FxCommonF3Packet *)(var_s1 - offsetof(FxCommonF3Packet, color)))->tag.length = 4;
                            ((FxCommonF3Packet *)(var_s1 - offsetof(FxCommonF3Packet, color)))->xy0 =
                                sp->screenCoordinates[0];
                            ((FxCommonF3Packet *)(var_s1 - offsetof(FxCommonF3Packet, color)))->xy1 =
                                sp->screenCoordinates[1];
                            ((FxCommonF3Packet *)(var_s1 - offsetof(FxCommonF3Packet, color)))->xy2 =
                                sp->screenCoordinates[2];
                            *((u32 *)&((FxCommonF3Packet *)(var_s1 - offsetof(FxCommonF3Packet, color)))->color) =
                                triangle->color;
                            var_s1 += sizeof(FxCommonF3Packet);
                            FX_COMMON_LINK_PACKET((FxCommonF3Packet *)var_s2);
                            var_s2 += sizeof(FxCommonF3Packet);
                        }
                    }
                    break;
                }
                var_s4 += 1;
                ++triangle;
                var_s3=(u8 *)triangle;
            } while (var_s4 < var_t8->counts[0]);
        }
        {
            s32 primitiveIndex1;
            primitiveIndex1 = 0;
            primitiveCount = var_t8->counts[1];
            if (primitiveCount > 0) {
                FxCommonFlatQuad *quad;

                s32 temp_v1_3;

                var_s1 = var_s2 + sizeof(RenderNativeTag);
                do {
                    for (;;) {
                        quad=(FxCommonFlatQuad *)var_s5;
                        temp_v1_3 = RotAverageNclip4(
                            &quad->vertices[0],
                            &quad->vertices[1],
                            &quad->vertices[2],
                            &quad->vertices[3],
                            &sp->screenCoordinates[0], &sp->screenCoordinates[1],
                            &sp->screenCoordinates[2], &sp->screenCoordinates[3],
                            &sp->perspective, &sp->orderingDepth,
                            &sp->transformFlags);
                        temp_a0 = sp->orderingDepth;
                        if (temp_a0 > 0) {
                            sp->orderingDepth = ((s32)((u32)temp_a0+(u32)g_FxCommonDepthBase)) >> 2;
                            if (temp_v1_3 > 0) {
                            if ((unsigned)sp->orderingDepth >= 4096) {
                                buffer->error = -2;
                                buffer->data = (u8 *)var_s2;
                                return;
                            }
                            if (!buffer->end || (uintptr_t)var_s2 > (uintptr_t)buffer->end ||
                                (1) * sizeof(FxCommonF4Packet) > (uintptr_t)buffer->end - (uintptr_t)var_s2) {
                                buffer->error = -1;
                                buffer->data = (u8 *)var_s2;
                                return;
                            }
                            ((FxCommonF4Packet *)(var_s1 - offsetof(FxCommonF4Packet, color)))->tag.length = 5;
                                ((FxCommonF4Packet *)(var_s1 - offsetof(FxCommonF4Packet, color)))->xy0 =
                                    sp->screenCoordinates[0];
                                ((FxCommonF4Packet *)(var_s1 - offsetof(FxCommonF4Packet, color)))->xy1 =
                                    sp->screenCoordinates[1];
                                ((FxCommonF4Packet *)(var_s1 - offsetof(FxCommonF4Packet, color)))->xy2 =
                                    sp->screenCoordinates[2];
                                ((FxCommonF4Packet *)(var_s1 - offsetof(FxCommonF4Packet, color)))->xy3 =
                                    sp->screenCoordinates[3];
                                *((u32 *)&((FxCommonF4Packet *)(var_s1 - offsetof(FxCommonF4Packet, color)))->color) =
                                    quad->color;
                                var_s1 += sizeof(FxCommonF4Packet);
                                FX_COMMON_LINK_PACKET((FxCommonF4Packet *)var_s2);
                            var_s2 += sizeof(FxCommonF4Packet);
                            }
                        }
                        break;
                    }
                    primitiveIndex1 += 1;
                    var_s5 += 0x24;
                } while (primitiveIndex1 < var_t8->counts[1]);
            }
        }
        {
            s32 primitiveIndex2;
            primitiveIndex2 = 0;
            primitiveCount = var_t8->counts[2];
            if (primitiveCount > 0) {

                s32 temp_v1_5;

                var_s3 = var_t0 + 8;
                var_s1 = var_s2 + offsetof(FxCommonG3Packet,color2);
                do {
                    temp_v1_5 = RotAverageNclip3(
                        &((FxCommonColoredTriangle *)var_t0)->vertices[0],
                        &((FxCommonColoredTriangle *)var_t0)->vertices[1],
                        &((FxCommonColoredTriangle *)var_t0)->vertices[2],
                        &sp->screenCoordinates[0], &sp->screenCoordinates[1],
                        &sp->screenCoordinates[2], &sp->perspective,
                        &sp->orderingDepth, &sp->transformFlags);
                    temp_a0 = sp->orderingDepth;
                    if (temp_a0 > 0) {
                        sp->orderingDepth = ((s32)((u32)temp_a0+(u32)g_FxCommonDepthBase)) >> 2;
                        if (temp_v1_5 > 0) {
                            if ((unsigned)sp->orderingDepth >= 4096) {
                                buffer->error = -2;
                                buffer->data = (u8 *)var_s2;
                                return;
                            }
                            if (!buffer->end || (uintptr_t)var_s2 > (uintptr_t)buffer->end ||
                                (1) * sizeof(FxCommonG3Packet) > (uintptr_t)buffer->end - (uintptr_t)var_s2) {
                                buffer->error = -1;
                                buffer->data = (u8 *)var_s2;
                                return;
                            }
                            ((FxCommonG3Packet *)(var_s1 - offsetof(FxCommonG3Packet, color2)))->tag.length = 6;
                            ((FxCommonG3Packet *)(var_s1 - offsetof(FxCommonG3Packet, color2)))->xy0 =
                                sp->screenCoordinates[0];
                            ((FxCommonG3Packet *)(var_s1 - offsetof(FxCommonG3Packet, color2)))->xy1 =
                                sp->screenCoordinates[1];
                            ((FxCommonG3Packet *)(var_s1 - offsetof(FxCommonG3Packet, color2)))->xy2 =
                                sp->screenCoordinates[2];
                            ((FxCommonG3Packet *)(var_s1 - offsetof(FxCommonG3Packet, color2)))->color0 =
                                ((FxCommonColoredTriangle *)var_t0)->colors[0];
                            ((FxCommonG3Packet *)(var_s1 - offsetof(FxCommonG3Packet, color2)))->color1 =
                                (*((u32 *)&((FxCommonColoredTriangle *)(var_s3 - 8))->colors[1]));
                            *((u32 *)&((FxCommonG3Packet *)(var_s1 - offsetof(FxCommonG3Packet, color2)))->color2) =
                                (*((u32 *)&((FxCommonColoredTriangle *)(var_s3 - 8))->colors[2]));
                            var_s1 += sizeof(FxCommonG3Packet);
                            FX_COMMON_LINK_PACKET((FxCommonG3Packet *)var_s2);
                            var_s2 += sizeof(FxCommonG3Packet);
                        }
                    }
                    primitiveIndex2 += 1;
                    var_s3 += 0x24;
                    var_t0 += 0x24;
                } while (primitiveIndex2 < var_t8->counts[2]);
            }
        }
        {
            s32 primitiveIndex3;
            primitiveIndex3 = 0;
            primitiveCount = var_t8->counts[3];
            if (primitiveCount > 0) {

                s32 temp_v1_7;

                var_s3 = (u8 *)&var_s7->colors[3];
                var_s1 = var_s2 + offsetof(FxCommonG4Packet,color3);
                do {
                    temp_v1_7 = RotAverageNclip4(
                        &var_s7->vertices[0], &var_s7->vertices[1], &var_s7->vertices[2],
                        &var_s7->vertices[3], &sp->screenCoordinates[0],
                        &sp->screenCoordinates[1], &sp->screenCoordinates[2],
                        &sp->screenCoordinates[3], &sp->perspective,
                        &sp->orderingDepth, &sp->transformFlags);
                    temp_a0 = sp->orderingDepth;
                    if (temp_a0 > 0) {
                        sp->orderingDepth = ((s32)((u32)temp_a0+(u32)g_FxCommonDepthBase)) >> 2;
                        if (temp_v1_7 > 0) {
                            if ((unsigned)sp->orderingDepth >= 4096) {
                                buffer->error = -2;
                                buffer->data = (u8 *)var_s2;
                                return;
                            }
                            if (!buffer->end || (uintptr_t)var_s2 > (uintptr_t)buffer->end ||
                                (1) * sizeof(FxCommonG4Packet) > (uintptr_t)buffer->end - (uintptr_t)var_s2) {
                                buffer->error = -1;
                                buffer->data = (u8 *)var_s2;
                                return;
                            }
                            ((FxCommonG4Packet *)(var_s1 - offsetof(FxCommonG4Packet, color3)))->tag.length = 8;
                            ((FxCommonG4Packet *)(var_s1 - offsetof(FxCommonG4Packet, color3)))->xy0 =
                                sp->screenCoordinates[0];
                            ((FxCommonG4Packet *)(var_s1 - offsetof(FxCommonG4Packet, color3)))->xy1 =
                                sp->screenCoordinates[1];
                            ((FxCommonG4Packet *)(var_s1 - offsetof(FxCommonG4Packet, color3)))->xy2 =
                                sp->screenCoordinates[2];
                            ((FxCommonG4Packet *)(var_s1 - offsetof(FxCommonG4Packet, color3)))->xy3 =
                                sp->screenCoordinates[3];
                            ((FxCommonG4Packet *)(var_s1 - offsetof(FxCommonG4Packet, color3)))->color0 = var_s7->colors[0];
                            ((FxCommonG4Packet *)(var_s1 - offsetof(FxCommonG4Packet, color3)))->color1 =
                                ((FxCommonColoredQuad *)(var_s3 - 12))->colors[1];
                            ((FxCommonG4Packet *)(var_s1 - offsetof(FxCommonG4Packet, color3)))->color2 =
                                (*((u32 *)&((FxCommonColoredQuad *)(var_s3 - 12))->colors[2]));
                            *((u32 *)&((FxCommonG4Packet *)(var_s1 - offsetof(FxCommonG4Packet, color3)))->color3) =
                                ((FxCommonColoredQuad *)(var_s3 - 12))->colors[3];
                            var_s1 += sizeof(FxCommonG4Packet);
                            FX_COMMON_LINK_PACKET((FxCommonG4Packet *)var_s2);
                            var_s2 += sizeof(FxCommonG4Packet);
                        }
                    }
                    primitiveIndex3 += 1;
                    var_s3 += 0x30;
                    ++var_s7;
                } while (primitiveIndex3 < var_t8->counts[3]);
            }
            var_t0 = var_s2;
        }
        {
            FxCommonFt3Packet *trianglePacketCursor;
            u32 *triangleScreen2Address;
            u32 *triangleScreen1Address;
            u32 *triangleMidpointScreen4Address;
            trianglePacketCursor = (FxCommonFt3Packet *)var_t0;
            var_s4 = 0;
            primitiveCount = var_t8->counts[4];
            if (primitiveCount > 0) {
                u32 *triangleGteScreen2;
                FxCommonVector *triangleMidpoint02;
                FxCommonVector *triangleMidpoint12;
                u8 *var_a3;
                FxCommonVector *var_t4;

                s32 temp_v1_9;
                u16 *triangleUv2Cursor;
                u16 var_v0;
                u32 triangleLength = 7;

                var_a3 = (u8 *)&var_t3->textures[1].uv2;
                var_t6 = (u8 *)&var_t3->vertices[2];
                var_t5 = (u8 *)&var_t3->vertices[1];
                var_t4 = &var_t3->vertices[0];
                triangleUv2Cursor = &trianglePacketCursor->uv2;
                do {
                    gte_ldvxy0_precise(var_t4);
                    gte_ldvz0_precise(var_t4);
                    gte_ldvxy1_precise(var_t5);
                    gte_lwc2_3_4(var_t5);
                    gte_ldvxy2_precise(var_t6);
                    gte_ldvz2_precise(var_t6);
                    gte_rtpt_padded();
                    gte_nclip();
                    gte_stmac0_precise(&sp->normalClip);
                    if (sp->normalClip > 0) {
                    gte_avsz3_padded();
                    gte_stotz_precise(&sp->orderingDepth);
                    temp_a0 = ((s32)((u32)sp->orderingDepth+(u32)sp->depthBias)) >> 2;
                    sp->orderingDepth = temp_a0;
                    if (((s32)((u32)temp_a0*4u-(u32)sp->depthBias)) <= 0) {
                    }
                    else
                    {
                    if ((unsigned)sp->orderingDepth >= 4096) {
                        buffer->error = -2;
                        buffer->data = (u8 *)trianglePacketCursor;
                        return;
                    }
                    if (!buffer->end || (uintptr_t)trianglePacketCursor > (uintptr_t)buffer->end ||
                        (temp_a0<0x1f4?4:1) * sizeof(FxCommonFt3Packet) > (uintptr_t)buffer->end - (uintptr_t)trianglePacketCursor) {
                        buffer->error = -1;
                        buffer->data = (u8 *)trianglePacketCursor;
                        return;
                    }
                    if (temp_a0 < 0x1F4) {
                        FxCommonVector *triangleMidpoint01;
                        FxCommonVector *triangleNamedInput2;
                        FxCommonTextureSet *triangleTexture;
                        u32 *triangleMidpointOutput0;
                        u32 *triangleMidpointOutput2;
                        sp->vertices[1] = (FxCommonVector *)var_t5;
                        sp->vertices[2] = var_t4;
                        sp->vertices[3] = (FxCommonVector *)var_t6;
                        triangleMidpoint12 = &sp->midpoints[3];
                        sp->midpoints[2].x =
                            (((sp->vertices[1]->x)) + (sp->vertices[2]->x)) >> 1;
                        sp->midpoints[2].y =
                            (((sp->vertices[1]->y)) + (sp->vertices[2]->y)) >> 1;
                        sp->midpoints[2].z =
                            (((sp->vertices[1]->z)) + (sp->vertices[2]->z)) >> 1;
                        triangleMidpoint02 = &sp->midpoints[4];
                        triangleMidpoint02->x =
                            ((sp->vertices[3]->x) + (sp->vertices[2]->x)) >> 1;
                        sp->midpoints[4].y =
                            ((sp->vertices[3]->y) + (sp->vertices[2]->y)) >> 1;
                        sp->midpoints[4].z =
                            ((sp->vertices[3]->z) + (sp->vertices[2]->z)) >> 1;
                        triangleMidpoint12->x =
                            (((sp->vertices[1]->x)) + (sp->vertices[3]->x)) >> 1;
                        sp->midpoints[3].y =
                            (((sp->vertices[1]->y)) + (sp->vertices[3]->y)) >> 1;
                        sp->midpoints[3].z =
                            (((sp->vertices[1]->z)) + (sp->vertices[3]->z)) >> 1;
                        triangleMidpoint01 = &sp->midpoints[2];
                        triangleNamedInput2 = &sp->midpoints[3];
                        gte_ldvxy0_precise(triangleMidpoint01);
                        gte_ldvz0_precise(triangleMidpoint01);
                        gte_ldvxy1_precise(triangleMidpoint02);
                        gte_ldvz1_precise(triangleMidpoint02);
                        gte_ldvxy2_precise(triangleNamedInput2);
                        gte_ldvz2_precise(triangleNamedInput2);
                        gte_rtpt_padded();
                        triangleTexture = &((FxCommonTexturedTriangle *)(var_a3 - 24))->textures[0];
                        sp->midpointUv[4].components.u =
                            ((triangleTexture->uv0.components.u) +
                             (triangleTexture->uv2.components.u)) >>
                            1;
                        sp->midpointUv[4].components.v =
                            ((triangleTexture->uv0.components.v) +
                             (triangleTexture->uv2.components.v)) >>
                            1;
                        sp->midpointUv[2].components.u =
                            ((triangleTexture->uv0.components.u) +
                             (triangleTexture->uv1.components.u)) >>
                            1;
                        triangleMidpointScreen4Address = &sp->midpointScreenCoordinates[4];
                        triangleMidpointOutput0 = &sp->midpointScreenCoordinates[2];
                        triangleMidpointOutput2 = &sp->midpointScreenCoordinates[3];
                        gte_stsxy0_precise(triangleMidpointOutput0);
                        gte_stsxy1_precise(&sp->midpointScreenCoordinates[4]);
                        gte_stsxy2_mem(triangleMidpointOutput2);
                        gte_ldvxy0_precise(var_t4);
                        gte_ldvz0_precise(var_t4);
                        gte_lwc2_2_0(var_t5);
                        gte_lwc2_3_4(var_t5);
                        gte_lwc2_4_0(var_t6);
                        gte_lwc2_5_4(var_t6);
                        gte_rtpt_padded();
                        sp->midpointUv[2].components.v =
                            ((triangleTexture->uv0.components.v) +
                             (triangleTexture->uv1.components.v)) >>
                            1;
                        sp->midpointUv[3].components.u =
                            (u8)((triangleTexture->uv1.components.u +
                                       (triangleTexture->uv2.components.u)) >>
                                 1);
                        sp->midpointUv[3].components.v =
                            (u8)(((triangleTexture->uv1.components.v) +
                                  (triangleTexture->uv2.components.v)) >>
                                 1);
                        triangleScreen1Address = &sp->screenCoordinates[1];
                        triangleScreen2Address = (u32 *)(&sp->screenCoordinates[2]);
                        gte_stsxy0_precise(&sp->screenCoordinates[0]);
                        gte_stsxy1_precise(&sp->screenCoordinates[1]);
                        gte_stsxy2_precise(&sp->screenCoordinates[2]);
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->tag.length =
                            triangleLength;
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->color = var_t3->color;
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->xy0 =
                            sp->screenCoordinates[0];
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->xy1 =
                            sp->midpointScreenCoordinates[2];
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->xy2 =
                            sp->midpointScreenCoordinates[4];
                        *((s32 *)&((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv0) =
                            *((s32 *)&((FxCommonTexturedTriangle *)(var_a3 - 24))->textures[0].uv0);
                        *((s32 *)&((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv1) =
                            *((s32 *)&((FxCommonTexturedTriangle *)(var_a3 - 24))->textures[0].uv1);
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv0 =
                            (((FxCommonTexturedTriangle *)(var_a3 - 24))
                                      ->textures[0]
                                      .uv0.packed);
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv1 =
                            sp->midpointUv[2].packed;
                        *triangleUv2Cursor =
                            sp->midpointUv[4].packed;
                        triangleUv2Cursor += sizeof(FxCommonFt3Packet)/sizeof(*triangleUv2Cursor);
                        FX_COMMON_LINK_PACKET(trianglePacketCursor);
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->tag.length =
                            triangleLength;
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->color = var_t3->color;
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->xy0 =
                            sp->midpointScreenCoordinates[2];
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->xy1 =
                            *triangleScreen1Address;
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->xy2 =
                            sp->midpointScreenCoordinates[3];
                        *((s32 *)&((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv0) =
                            *((s32 *)&((FxCommonTexturedTriangle *)(var_a3 - 24))->textures[0].uv0);
                        *((s32 *)&((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv1) =
                            *((s32 *)&((FxCommonTexturedTriangle *)(var_a3 - 24))->textures[0].uv1);
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv0 =
                            sp->midpointUv[2].packed;
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv1 =
                            (((FxCommonTexturedTriangle *)(var_a3 - 24))
                                      ->textures[0]
                                      .uv1.packed);
                        ++trianglePacketCursor;
                        *((u16 *)(triangleUv2Cursor)) = sp->midpointUv[3].packed;
                        triangleUv2Cursor += sizeof(FxCommonFt3Packet)/sizeof(*triangleUv2Cursor);
                        FX_COMMON_LINK_PACKET(trianglePacketCursor);
                        ++trianglePacketCursor;
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->tag.length =
                            triangleLength;
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->color = var_t3->color;
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->xy0 =
                            sp->midpointScreenCoordinates[2];
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->xy1 =
                            sp->midpointScreenCoordinates[3];
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->xy2 =
                            sp->midpointScreenCoordinates[4];
                        *((s32 *)&((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv0) =
                            *((s32 *)&((FxCommonTexturedTriangle *)(var_a3 - 24))->textures[0].uv0);
                        *((s32 *)&((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv1) =
                            *((s32 *)&((FxCommonTexturedTriangle *)(var_a3 - 24))->textures[0].uv1);
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv0 =
                            sp->midpointUv[2].packed;
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv1 =
                            (sp->midpointUv[3].packed);
                        *triangleUv2Cursor = sp->midpointUv[4].packed;
                        triangleUv2Cursor += sizeof(FxCommonFt3Packet)/sizeof(*triangleUv2Cursor);
                        FX_COMMON_LINK_PACKET(trianglePacketCursor);
                        ++trianglePacketCursor;
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->tag.length =
                            triangleLength;
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->color = var_t3->color;
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->xy0 =
                            sp->midpointScreenCoordinates[3];
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->xy1 =
                            *triangleScreen2Address;
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->xy2 =
                            *triangleMidpointScreen4Address;
                        *((s32 *)&((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv0) =
                            *((s32 *)&((FxCommonTexturedTriangle *)(var_a3 - 24))->textures[0].uv0);
                        *((s32 *)&((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv1) =
                            *((s32 *)&((FxCommonTexturedTriangle *)(var_a3 - 24))->textures[0].uv1);
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv0 =
                            (sp->midpointUv[3].packed);
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv1 =
                            (((FxCommonTexturedTriangle *)(var_a3 - 24))
                                      ->textures[0]
                                      .uv2.packed);
                        var_v0 = sp->midpointUv[4].packed;
                    } else {

                        gteScreen0 = &trianglePacketCursor->xy0;
                        triangleGteScreen2 = &trianglePacketCursor->xy2;
                        gteScreen1 = &trianglePacketCursor->xy1;
                        gte_stsxy0_precise(gteScreen0);
                        gte_stsxy1_precise(gteScreen1);
                        gte_stsxy2_precise(triangleGteScreen2);
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->tag.length =
                            triangleLength;
                        ((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->color = var_t3->color;
                        temp_v1_9 = sp->orderingDepth;
                        if (temp_v1_9 < 0x1FE) {
                            *((s32 *)&((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv0) =
                                *((s32 *)&((FxCommonTexturedTriangle *)(var_a3 - 24))
                                      ->textures[0]
                                      .uv0);
                            *((s32 *)&((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv1) =
                                *((s32 *)&((FxCommonTexturedTriangle *)(var_a3 - 24))
                                      ->textures[0]
                                      .uv1);
                            var_v0 =
                                ((FxCommonTexturedTriangle *)(var_a3 - 24))->textures[0].uv2.packed;
                        } else if (temp_v1_9 >= 0x2BD) {
                            *((s32 *)&((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv0) =
                                *((s32 *)&((FxCommonTexturedTriangle *)(var_a3 - 24))
                                      ->textures[2]
                                      .uv0);
                            *((s32 *)&((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv1) =
                                *((s32 *)&((FxCommonTexturedTriangle *)(var_a3 - 24))
                                      ->textures[2]
                                      .uv1);
                            var_v0 =
                                ((FxCommonTexturedTriangle *)(var_a3 - 24))->textures[2].uv2.packed;
                        } else {
                            *((s32 *)&((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv0) =
                                *((s32 *)&((FxCommonTexturedTriangle *)(var_a3 - 24))
                                      ->textures[1]
                                      .uv0);
                            *((s32 *)&((FxCommonFt3Packet *)((u8 *)triangleUv2Cursor - offsetof(FxCommonFt3Packet, uv2)))->uv1) =
                                *((s32 *)&((FxCommonTexturedTriangle *)(var_a3 - 24))
                                      ->textures[1]
                                      .uv1);
                            var_v0 =
                                ((FxCommonTexturedTriangle *)(var_a3 - 24))->textures[1].uv2.packed;
                        }
                    }
                    *triangleUv2Cursor = var_v0;
                    triangleUv2Cursor += sizeof(FxCommonFt3Packet)/sizeof(*triangleUv2Cursor);
                    FX_COMMON_LINK_PACKET(trianglePacketCursor);
                    ++trianglePacketCursor;
                    }
                    }
                    var_s4 += 1;

                    var_a3 += 0x40;
                    var_t6 += 0x40;
                    var_t5 += 0x40;
                    var_t4 += 8;
                    ++var_t3;
                } while (var_s4 < var_t8->counts[4]);
            }
            var_t1_2 = (FxCommonFt4Packet *)trianglePacketCursor;
        }
        var_s4 = 0;
        primitiveCount = var_t8->counts[5];
        if (primitiveCount > 0) {

            u8 *quadTextureCursor;
            u16 *quadUv3Cursor;
            FxCommonVector *quadVertex3;

            u32 quadLength;

            ;
            quadLength = 9;

            quadTextureCursor = (u8 *)&var_fp->textures[0].clut;
            quadVertex3 = &var_fp->vertices[3];
            var_s5 = (u8 *)&var_fp->vertices[2];
            var_s3 = (u8 *)&var_fp->vertices[1];
            var_s2 = (u8 *)&var_fp->vertices[0];
            quadUv3Cursor = &var_t1_2->uv3;
            do {
                gte_ldvxy0_precise_keep(var_s2, quadVertex3);
                gte_ldvz0_precise(var_s2);
                gte_ldvxy1_precise(var_s3);
                gte_ldvz1_precise(var_s3);
                gte_ldvxy2_precise(var_s5);
                gte_ldvz2_precise(var_s5);
                gte_rtpt_padded();
                gte_nclip();
                gte_stmac0_precise(&sp->normalClip);
                if (sp->normalClip > 0) {
                gte_avsz3_padded();
                gte_stotz_precise(&sp->orderingDepth);
                temp_a0 = ((s32)((u32)sp->orderingDepth+(u32)sp->depthBias)) >> 2;
                sp->orderingDepth = temp_a0;
                if (((s32)((u32)temp_a0*4u-(u32)sp->depthBias)) <= 0) {
                }
                else
                {
                if ((unsigned)sp->orderingDepth >= 4096) {
                    buffer->error = -2;
                    buffer->data = (u8 *)var_t1_2;
                    return;
                }
                if (!buffer->end || (uintptr_t)var_t1_2 > (uintptr_t)buffer->end ||
                    (temp_a0<0x1f5?4:1) * sizeof(FxCommonFt4Packet) > (uintptr_t)buffer->end - (uintptr_t)var_t1_2) {
                    buffer->error = -1;
                    buffer->data = (u8 *)var_t1_2;
                    return;
                }
                if (temp_a0 >= 0x1F5) {
                    u32 *quadGteScreen2;
                    if (temp_a0 < 0x1FE) {
                        *((s32 *)&((FxCommonFt4Packet *)((u8 *)quadUv3Cursor - offsetof(FxCommonFt4Packet, uv3)))->uv0) =
                            *((u32 *)&((FxCommonTexturedQuad *)(quadTextureCursor - 6))
                                  ->textures[0]
                                  .uv0);
                        *((s32 *)&((FxCommonFt4Packet *)((u8 *)quadUv3Cursor - offsetof(FxCommonFt4Packet, uv3)))->uv1) =
                            *((u32 *)&((FxCommonTexturedQuad *)(quadTextureCursor - 6))
                                  ->textures[0]
                                  .uv1);
                        ((FxCommonFt4Packet *)((u8 *)quadUv3Cursor - offsetof(FxCommonFt4Packet, uv3)))->uv2 =
                            ((FxCommonTexturedQuad *)(quadTextureCursor - 6))
                                ->textures[0]
                                .uv2.packed;
                        *quadUv3Cursor =
                            ((FxCommonTexturedQuad *)(quadTextureCursor - 6))
                                ->textures[0]
                                .uv3.packed;
                    } else if (temp_a0 >= 0x2BD) {
                        *((s32 *)&((FxCommonFt4Packet *)((u8 *)quadUv3Cursor - offsetof(FxCommonFt4Packet, uv3)))->uv0) =
                            *((u32 *)&((FxCommonTexturedQuad *)(quadTextureCursor - 6))
                                  ->textures[2]
                                  .uv0);
                        *((s32 *)&((FxCommonFt4Packet *)((u8 *)quadUv3Cursor - offsetof(FxCommonFt4Packet, uv3)))->uv1) =
                            *((u32 *)&((FxCommonTexturedQuad *)(quadTextureCursor - 6))
                                  ->textures[2]
                                  .uv1);
                        ((FxCommonFt4Packet *)((u8 *)quadUv3Cursor - offsetof(FxCommonFt4Packet, uv3)))->uv2 =
                            ((FxCommonTexturedQuad *)(quadTextureCursor - 6))
                                ->textures[2]
                                .uv2.packed;
                        *quadUv3Cursor =
                            ((FxCommonTexturedQuad *)(quadTextureCursor - 6))
                                ->textures[2]
                                .uv3.packed;
                    } else {
                        *((s32 *)&((FxCommonFt4Packet *)((u8 *)quadUv3Cursor - offsetof(FxCommonFt4Packet, uv3)))->uv0) =
                            *((u32 *)&((FxCommonTexturedQuad *)(quadTextureCursor - 6))
                                  ->textures[1]
                                  .uv0);
                        *((s32 *)&((FxCommonFt4Packet *)((u8 *)quadUv3Cursor - offsetof(FxCommonFt4Packet, uv3)))->uv1) =
                            *((u32 *)&((FxCommonTexturedQuad *)(quadTextureCursor - 6))
                                  ->textures[1]
                                  .uv1);
                        ((FxCommonFt4Packet *)((u8 *)quadUv3Cursor - offsetof(FxCommonFt4Packet, uv3)))->uv2 =
                            ((FxCommonTexturedQuad *)(quadTextureCursor - 6))
                                ->textures[1]
                                .uv2.packed;
                        *quadUv3Cursor =
                            ((FxCommonTexturedQuad *)(quadTextureCursor - 6))
                                ->textures[1]
                                .uv3.packed;
                    }
                    gteScreen0 = &var_t1_2->xy0;
                    gteScreen1 = &var_t1_2->xy1;
                    quadGteScreen2 = &var_t1_2->xy2;
                    gte_stsxy0_precise(gteScreen0);
                    gte_stsxy1_precise(gteScreen1);
                    gte_stsxy2_precise(quadGteScreen2);
                    gte_lwc2_0_0(quadVertex3);
                    gte_lwc2_1_4(quadVertex3);
                    gte_rtps();
                    gte_stsxy2(&var_t1_2->xy3);
                    ((FxCommonFt4Packet *)((u8 *)quadUv3Cursor - offsetof(FxCommonFt4Packet, uv3)))->tag.length = quadLength;
                    ((FxCommonFt4Packet *)((u8 *)quadUv3Cursor - offsetof(FxCommonFt4Packet, uv3)))->color = var_fp->color;
                } else {
                    u32 *subdividedQuadGteScreen1;
                    u32 *quadMiddleOutput0;
                    u32 *quadMiddleOutput1;
                    u32 *subdividedQuadGteScreen3;
                    FxCommonUv *quadUv0;
                    FxCommonUv *quadUv3;
                    s16 midpoint13Z;
                    FxCommonUv *quadUv1;
                    FxCommonUv *quadUv2;
                    FxCommonVector *quadInput0;
                    FxCommonVector *quadInput1;
                    FxCommonVector *quadInput2;
                    FxCommonVector *quadCenterGteAddress;
                    u32 *quadLifetimeOutput0;
                    u32 *quadLifetimeOutput1;
                    FxCommonVector *quadCombinedIn0;
                    u32 *quadMidpointGteScreen1;
                    u32 *quadScreenValueAddress;
                    s32 midpoint23Z;
                    u32 *new_var;
                    s32 midpoint02X;
                    s32 midpoint02Y;
                    s32 quadEdgeY2;
                    s32 quadEdgeY3;
                    FxCommonVector *edgeX2;
                    FxCommonVector *edgeX3;
                    FxCommonVector *edgeY3;
                    s32 midpoint02Z;
                    u32 *midpointScreen2Address;
                    if (temp_a0 < 0x1FE) {
                        quadUv0 = &var_fp->textures[0].uv0;
                        var_t5 = (u8 *)&var_fp->textures[0].uv1;
                        var_t6 = (u8 *)&var_fp->textures[0].uv2;
                        quadUv3 = &var_fp->textures[0].uv3;
                    } else if (temp_a0 >= 0x2BD) {
                        quadUv0 = &var_fp->textures[2].uv0;
                        var_t5 = (u8 *)&var_fp->textures[2].uv1;
                        var_t6 = (u8 *)&var_fp->textures[2].uv2;
                        quadUv3 = &var_fp->textures[2].uv3;
                    } else {
                        quadUv0 = &var_fp->textures[1].uv0;
                        var_t5 = (u8 *)&var_fp->textures[1].uv1;
                        var_t6 = (u8 *)&var_fp->textures[1].uv2;
                        quadUv3 = &var_fp->textures[1].uv3;
                    }
                    quadUv1 = (FxCommonUv *)var_t5;
                    quadUv2 = (FxCommonUv *)var_t6;
                    sp->vertices[1] = (FxCommonVector *)var_s3;
                    sp->vertices[0] = (FxCommonVector *)var_s2;
                    sp->vertices[2] = (FxCommonVector *)var_s5;
                    quadMidpoint13 = &sp->midpoints[3];
                    sp->vertices[3] = quadVertex3;
                    sp->midpoints[0].x =
                        (((sp->vertices[1]->x)) + (sp->vertices[0]->x)) >> 1;
                    sp->midpoints[0].y =
                        (((sp->vertices[1]->y)) + (sp->vertices[0]->y)) >> 1;
                    sp->midpoints[0].z =
                        (((sp->vertices[1]->z)) + (sp->vertices[0]->z)) >> 1;
                    quadMidpoint23 = &sp->midpoints[4];
                    sp->midpoints[1].x =
                        ((sp->vertices[0]->x) + (sp->vertices[2]->x)) >> 1;
                    sp->midpoints[1].y =
                        ((sp->vertices[0]->y) + (sp->vertices[2]->y)) >> 1;
                    sp->midpoints[1].z =
                        ((sp->vertices[0]->z) + (sp->vertices[2]->z)) >> 1;
                    quadMidpoint13->x =
                        (((sp->vertices[1]->x)) + (sp->vertices[3]->x)) >> 1;
                    sp->midpoints[3].y =
                        (((sp->vertices[1]->y)) + (sp->vertices[3]->y)) >> 1;
                    sp->midpoints[3].z =
                        (((sp->vertices[1]->z)) + (sp->vertices[3]->z)) >> 1;
                    quadInput1 = &sp->midpoints[1];
                    quadInput0 = &sp->midpoints[0];
                    quadInput2 = &sp->midpoints[3];
                    gte_ldvxy0_mem(quadInput0);
                    gte_ldvz0_mem(quadInput0);
                    gte_ldvxy1_mem(quadInput1);
                    gte_ldvz1_precise(quadInput1);
                    gte_ldvxy2_precise(quadInput2);
                    gte_ldvz2_precise(quadInput2);
                    gte_rtpt_padded();

                    edgeX2 = sp->vertices[2];
                    edgeX3 = sp->vertices[3];

                    edgeY3 = sp->vertices[3];
                    midpoint02X = quadInput1->x;
                    quadMidpoint23->x = (edgeX2->x + edgeX3->x) >> 1;

                    quadEdgeY2 = sp->vertices[2]->y;
                    quadEdgeY3 = edgeY3->y;

                    quadLifetimeOutput0 = &sp->midpointScreenCoordinates[0];
                    midpoint02Y = quadInput1->y;
                    quadMidpoint23->y = (quadEdgeY2 + quadEdgeY3) >> 1;
                    midpoint23Z =
                        ((sp->vertices[2]->z) + (sp->vertices[3]->z)) >> 1;
                    sp->midpoints[2].x =
                        (midpoint02X + (quadInput2->x)) >> 1;
                    midpoint02Z = quadInput1->z;
                    sp->midpoints[2].y =
                        (midpoint02Y + (quadInput2->y)) >> 1;
                    midpoint13Z = (quadInput2->z);
                    quadMidpoint23->z = midpoint23Z;
                    sp->midpoints[2].z = (midpoint02Z + midpoint13Z) >> 1;
                    quadMidpointGteScreen1 = &sp->midpointScreenCoordinates[1];
                    quadLifetimeOutput1 = &sp->midpointScreenCoordinates[3];

                    gte_stsxy0_precise(quadLifetimeOutput0);
                    gte_stsxy1_precise(quadMidpointGteScreen1);
                    new_var = &sp->midpointScreenCoordinates[3];
                    gte_stsxy2_mem(quadLifetimeOutput1);
                    quadCombinedIn0 = &sp->midpoints[4];
                    quadCenterGteAddress = &sp->midpoints[2];
                    gte_ldvxy0_precise(quadCombinedIn0);
                    gte_ldvz0_precise(quadCombinedIn0);
                    gte_ldvxy1_precise(quadCenterGteAddress);
                    gte_ldvz1_precise(quadCenterGteAddress);
                    gte_ldvxy2_precise(var_s2);
                    gte_ldvz2_precise(var_s2);
                    gte_rtpt_padded();
                    sp->midpointUv[0].components.u =
                        (quadUv0->components.u + quadUv1->components.u) >> 1;
                    sp->midpointUv[0].components.v =
                        (quadUv0->components.v + quadUv1->components.v) >> 1;
                    sp->midpointUv[1].components.u =
                        ((quadUv0->components.u + quadUv2->components.u) >> 1);
                    sp->midpointUv[1].components.v =
                        ((quadUv0->components.v + quadUv2->components.v) >> 1);
                    sp->midpointUv[3].components.u =
                        ((quadUv1->components.u + quadUv3->components.u) >> 1);
                    sp->midpointUv[3].components.v =
                        ((quadUv1->components.v + quadUv3->components.v) >> 1);
                    quadMiddleOutput0 = &sp->midpointScreenCoordinates[4];
                    quadMiddleOutput1 = &sp->midpointScreenCoordinates[2];
                    gte_stsxy0_precise(quadMiddleOutput0);
                    gte_stsxy1_precise(quadMiddleOutput1);
                    gte_stsxy2_precise(&sp->screenCoordinates[0]);
                    gte_ldvxy0_precise(var_s3);
                    gte_ldvz0_precise(var_s3);
                    gte_ldvxy1_precise(var_s5);
                    gte_ldvz1_precise(var_s5);
                    gte_lwc2_4_0(quadVertex3);
                    gte_lwc2_5_4(quadVertex3);
                    gte_rtpt_padded();
                    subdividedQuadGteScreen3 = &sp->screenCoordinates[3];
                    sp->midpointUv[4].components.u =
                        (quadUv2->components.u + quadUv3->components.u) >> 1;
                    sp->midpointUv[4].components.v =
                        (quadUv2->components.v + quadUv3->components.v) >> 1;
                    subdividedQuadGteScreen1 = &sp->screenCoordinates[1];
                    sp->midpointUv[2].components.u =
                        ((sp->midpointUv[3].components.u) +
                         sp->midpointUv[1].components.u) >>
                        1;
                    sp->midpointUv[2].components.v =
                        (sp->midpointUv[3].components.v +
                         sp->midpointUv[1].components.v) >>
                        1;
                    quadScreenValueAddress = &sp->screenCoordinates[2];
                    gte_stsxy0_precise(subdividedQuadGteScreen1);
                    gte_stsxy1_precise_keep_address(quadScreenValueAddress);
                    gte_stsxy2_precise_keep_address(subdividedQuadGteScreen3);
                    {
                        FxCommonFt4Packet *quadPacket0 =
                            ((FxCommonFt4Packet *)((u8 *)quadUv3Cursor - offsetof(FxCommonFt4Packet, uv3)));
                        quadPacket0->tag.length = quadLength;
                        quadPacket0->color = var_fp->color;
                        quadPacket0->xy0 = sp->screenCoordinates[0];
                        quadPacket0->xy1 = sp->midpointScreenCoordinates[0];
                        quadPacket0->xy2 = sp->midpointScreenCoordinates[1];
                        quadPacket0->xy3 = sp->midpointScreenCoordinates[2];
                        quadPacket0->tpage =
                            ((FxCommonTexturedQuad *)(quadTextureCursor - 6))->textures[0].tpage;
                        quadPacket0->clut =
                            ((FxCommonTexturedQuad *)(quadTextureCursor - 6))->textures[0].clut;
                        quadPacket0->uv0 = quadUv0->packed;
                        quadPacket0->uv1 = sp->midpointUv[0].packed;
                        quadPacket0->uv2 = sp->midpointUv[1].packed;
                        {
                            quadPacket0->uv3 = sp->midpointUv[2].packed;
                          }
                        quadUv3Cursor += sizeof(FxCommonFt4Packet)/sizeof(*quadUv3Cursor);
                        FX_COMMON_LINK_PACKET(var_t1_2);
                    }
                    midpointScreen2Address = &sp->midpointScreenCoordinates[2];
                    {
                        FxCommonFt4Packet *quadPacket1 =
                            ((FxCommonFt4Packet *)((u8 *)quadUv3Cursor - offsetof(FxCommonFt4Packet, uv3)));
                        quadPacket1->tag.length = quadLength;
                        quadPacket1->color = var_fp->color;
                        quadPacket1->xy0 = sp->midpointScreenCoordinates[0];
                        quadPacket1->xy1 = sp->screenCoordinates[1];
                        quadPacket1->xy2 = *midpointScreen2Address;
                        quadPacket1->xy3 = sp->midpointScreenCoordinates[3];
                        quadPacket1->tpage = ((FxCommonTexturedQuad *)(quadTextureCursor - 6))
                                                 ->textures[0]
                                                 .tpage;
                        quadPacket1->clut = ((FxCommonTexturedQuad *)(quadTextureCursor - 6))
                                                ->textures[0]
                                                .clut;
                        quadPacket1->uv0 = sp->midpointUv[0].packed;
                        quadPacket1->uv1 = quadUv1->packed;
                        quadPacket1->uv2 = sp->midpointUv[2].packed;
                        *quadUv3Cursor = sp->midpointUv[3].packed;
                        quadUv3Cursor += sizeof(FxCommonFt4Packet)/sizeof(*quadUv3Cursor);
                        ++var_t1_2;
                        FX_COMMON_LINK_PACKET(var_t1_2);
                    }
                    {

                        FxCommonFt4Packet *quadPacket2 =
                            ((FxCommonFt4Packet *)((u8 *)quadUv3Cursor - offsetof(FxCommonFt4Packet, uv3)));
                        quadPacket2->tag.length = quadLength;
                        quadPacket2->color = var_fp->color;
                        quadPacket2->xy0 = sp->midpointScreenCoordinates[1];
                        quadPacket2->xy1 = sp->midpointScreenCoordinates[2];
                        quadPacket2->xy2 = *quadScreenValueAddress;
                        quadPacket2->xy3 = sp->midpointScreenCoordinates[4];
                        quadPacket2->tpage = ((FxCommonTexturedQuad *)(quadTextureCursor - 6))
                                                 ->textures[0]
                                                 .tpage;
                        quadPacket2->clut =
                            ((FxCommonTexturedQuad *)(quadTextureCursor - 6))->textures[0].clut;
                        quadPacket2->uv0 = sp->midpointUv[1].packed;
                        quadPacket2->uv1 = sp->midpointUv[2].packed;
                        quadPacket2->uv2 = quadUv2->packed;

                        ++var_t1_2;
                        quadPacket2->uv3 = sp->midpointUv[4].packed;
                        quadUv3Cursor += sizeof(FxCommonFt4Packet)/sizeof(*quadUv3Cursor);
                        FX_COMMON_LINK_PACKET(var_t1_2);
                    }
                    {
                        FxCommonFt4Packet *quadPacket3 =
                            ((FxCommonFt4Packet *)((u8 *)quadUv3Cursor - offsetof(FxCommonFt4Packet, uv3)));
                        quadPacket3->tag.length = quadLength;
                        quadPacket3->color = var_fp->color;
                        quadPacket3->xy0 = sp->midpointScreenCoordinates[2];
                        quadPacket3->xy1 = *new_var;
                        quadPacket3->xy2 = sp->midpointScreenCoordinates[4];
                        quadPacket3->xy3 = sp->screenCoordinates[3];
                        quadPacket3->tpage =
                            ((FxCommonTexturedQuad *)(quadTextureCursor - 6))->textures[0].tpage;
                        quadPacket3->clut = ((FxCommonTexturedQuad *)(quadTextureCursor - 6))
                                                ->textures[0]
                                                .clut;
                        quadPacket3->uv0 = sp->midpointUv[2].packed;
                        quadPacket3->uv1 = sp->midpointUv[3].packed;
                        quadPacket3->uv2 = sp->midpointUv[4].packed;
                        ++var_t1_2;
                        quadPacket3->uv3 = quadUv3->packed;
                    }
                }
                quadUv3Cursor += sizeof(FxCommonFt4Packet)/sizeof(*quadUv3Cursor);
                FX_COMMON_LINK_PACKET(var_t1_2);
                ++var_t1_2;
                }
                }
                var_s4 += 1;
                quadTextureCursor += 72;
                quadVertex3 += 9;
                var_s5 += 0x48;
                var_s3 += 0x48;
                var_s2 += 0x48;
                ++var_fp;
            } while (var_s4 < var_t8->counts[5]);
        }
        var_s3 = (u8 *)var_t1_2;
        {
            s32 primitiveIndex6;
            primitiveIndex6 = 0;
            primitiveCount = var_t8->counts[6];
            if (primitiveCount > 0) {
                s32 orderingDepth6;
                FxCommonColoredTexturedTriangle *transientCursor;

                s32 temp_v0_2;

                {
                    var_s1 = var_s3 + offsetof(FxCommonGt3Packet,color2);
                    transientCursor = gt3Cursor;
                    var_s2 = (u8 *)&transientCursor->colors[2];
                }
                do {
                    transientCursor = gt3Cursor;
                    temp_v0_2 = RotAverageNclip3(
                        &transientCursor->vertices[0], &transientCursor->vertices[1],
                        &transientCursor->vertices[2], &sp->screenCoordinates[0],
                        &sp->screenCoordinates[1], &sp->screenCoordinates[2],
                        &sp->perspective, &sp->orderingDepth,
                        &sp->transformFlags);
                    orderingDepth6 = ((s32)((u32)sp->orderingDepth+(u32)g_FxCommonDepthBase)) >> 2;
                    sp->orderingDepth = orderingDepth6;
                    if ((temp_v0_2 > 0) && (((orderingDepth6 > 0) && (orderingDepth6 < 0x1000)))) {
                        if ((unsigned)sp->orderingDepth >= 4096) {
                            buffer->error = -2;
                            buffer->data = (u8 *)var_s3;
                            return;
                        }
                        if (!buffer->end || (uintptr_t)var_s3 > (uintptr_t)buffer->end ||
                            (1) * sizeof(FxCommonGt3Packet) > (uintptr_t)buffer->end - (uintptr_t)var_s3) {
                            buffer->error = -1;
                            buffer->data = (u8 *)var_s3;
                            return;
                        }
                        ((FxCommonGt3Packet *)(var_s1 - offsetof(FxCommonGt3Packet, color2)))->tag.length = 9;
                        ((FxCommonGt3Packet *)(var_s1 - offsetof(FxCommonGt3Packet, color2)))->xy0 =
                            sp->screenCoordinates[0];
                        ((FxCommonGt3Packet *)(var_s1 - offsetof(FxCommonGt3Packet, color2)))->xy1 =
                            sp->screenCoordinates[1];
                        ((FxCommonGt3Packet *)(var_s1 - offsetof(FxCommonGt3Packet, color2)))->xy2 =
                            sp->screenCoordinates[2];
                        ((FxCommonGt3Packet *)(var_s1 - offsetof(FxCommonGt3Packet, color2)))->texture0 = *(
                            (u32 *)&((FxCommonColoredTexturedTriangle *)(var_s2 - 8))->texture.uv0);
                        ((FxCommonGt3Packet *)(var_s1 - offsetof(FxCommonGt3Packet, color2)))->texture1 = *(
                            (u32 *)&((FxCommonColoredTexturedTriangle *)(var_s2 - 8))->texture.uv1);
                        ((FxCommonGt3Packet *)(var_s1 - offsetof(FxCommonGt3Packet, color2)))->texture2 = *(
                            (u32 *)&((FxCommonColoredTexturedTriangle *)(var_s2 - 8))->texture.uv2);
                        transientCursor = gt3Cursor;
                        ((FxCommonGt3Packet *)(var_s1 - offsetof(FxCommonGt3Packet, color2)))->color0 =
                            *((u32 *)&transientCursor->colors[0]);
                        ((FxCommonGt3Packet *)(var_s1 - offsetof(FxCommonGt3Packet, color2)))->color1 =
                            *((u32 *)&((FxCommonColoredTexturedTriangle *)(var_s2 - 8))->colors[1]);
                        *((u32 *)&((FxCommonGt3Packet *)(var_s1 - offsetof(FxCommonGt3Packet, color2)))->color2) =
                            *((u32 *)&((FxCommonColoredTexturedTriangle *)(var_s2 - 8))->colors[2]);
                        var_s1 += sizeof(FxCommonGt3Packet);
                        FX_COMMON_LINK_PACKET((FxCommonGt3Packet *)var_s3);
                        var_s3 += sizeof(FxCommonGt3Packet);
                    }
                    primitiveIndex6 += 1;
                    var_s2 += 0x3C;
                    primitiveCount = var_t8->counts[6];
                    transientCursor = gt3Cursor;
                    ++transientCursor;
                    gt3Cursor = transientCursor;
                } while (primitiveIndex6 < primitiveCount);
            }
        }
        {
            s32 primitiveIndex7;
            primitiveIndex7 = 0;
            primitiveCount = var_t8->counts[7];
            if (primitiveCount > 0) {

                FxCommonColoredTexturedQuad *transientCursor;

                s32 temp_v0_3;
                s32 temp_v1_14;

                if (var_s2) {
                    var_s1 = var_s3 + offsetof(FxCommonGt4Packet,uv3);
                    transientCursor = gt4Cursor;
                    var_s2 = (u8 *)&transientCursor->texture.uv3;
                } else {
                    var_s1 = var_s3 + offsetof(FxCommonGt4Packet,uv3);
                    transientCursor = gt4Cursor;
                    var_s2 = (u8 *)&transientCursor->texture.uv3;
                }
                do {
                    transientCursor = gt4Cursor;
                    temp_v0_3 = RotAverageNclip4(
                        &transientCursor->vertices[0], &transientCursor->vertices[1],
                        &transientCursor->vertices[2], &transientCursor->vertices[3],
                        &sp->screenCoordinates[0], &sp->screenCoordinates[1],
                        &sp->screenCoordinates[2], &sp->screenCoordinates[3],
                        &sp->perspective, &sp->orderingDepth,
                        &sp->transformFlags);
                    temp_v1_14 = ((s32)((u32)sp->orderingDepth+(u32)g_FxCommonDepthBase)) >> 2;
                    sp->orderingDepth = temp_v1_14;
                    if ((temp_v0_3 > 0) && (((temp_v1_14 > 0) && (temp_v1_14 < 0x1000)))) {
                        if ((unsigned)sp->orderingDepth >= 4096) {
                            buffer->error = -2;
                            buffer->data = (u8 *)var_s3;
                            return;
                        }
                        if (!buffer->end || (uintptr_t)var_s3 > (uintptr_t)buffer->end ||
                            (1) * sizeof(FxCommonGt4Packet) > (uintptr_t)buffer->end - (uintptr_t)var_s3) {
                            buffer->error = -1;
                            buffer->data = (u8 *)var_s3;
                            return;
                        }
                        ((FxCommonGt4Packet *)(var_s1 - offsetof(FxCommonGt4Packet, uv3)))->tag.length = 0xC;
                        ((FxCommonGt4Packet *)(var_s1 - offsetof(FxCommonGt4Packet, uv3)))->xy0 =
                            sp->screenCoordinates[0];
                        ((FxCommonGt4Packet *)(var_s1 - offsetof(FxCommonGt4Packet, uv3)))->xy1 =
                            sp->screenCoordinates[1];
                        ((FxCommonGt4Packet *)(var_s1 - offsetof(FxCommonGt4Packet, uv3)))->xy2 =
                            sp->screenCoordinates[2];
                        ((FxCommonGt4Packet *)(var_s1 - offsetof(FxCommonGt4Packet, uv3)))->xy3 =
                            sp->screenCoordinates[3];
                        transientCursor = gt4Cursor;
                        ((FxCommonGt4Packet *)(var_s1 - offsetof(FxCommonGt4Packet, uv3)))->color0 =
                            *((u32 *)&transientCursor->colors[0]);
                        ((FxCommonGt4Packet *)(var_s1 - offsetof(FxCommonGt4Packet, uv3)))->color1 =
                            *((u32 *)&((FxCommonColoredTexturedQuad *)(var_s2 - 26))->colors[1]);
                        ((FxCommonGt4Packet *)(var_s1 - offsetof(FxCommonGt4Packet, uv3)))->color2 =
                            *((u32 *)&((FxCommonColoredTexturedQuad *)(var_s2 - 26))->colors[2]);
                        ((FxCommonGt4Packet *)(var_s1 - offsetof(FxCommonGt4Packet, uv3)))->color3 =
                            *((u32 *)&((FxCommonColoredTexturedQuad *)(var_s2 - 26))->colors[3]);
                        ((FxCommonGt4Packet *)(var_s1 - offsetof(FxCommonGt4Packet, uv3)))->texture0 =
                            *((u32 *)&((FxCommonColoredTexturedQuad *)(var_s2 - 26))->texture.uv0);
                        ((FxCommonGt4Packet *)(var_s1 - offsetof(FxCommonGt4Packet, uv3)))->texture1 =
                            *((u32 *)&((FxCommonColoredTexturedQuad *)(var_s2 - 26))->texture.uv1);
                        ((FxCommonGt4Packet *)(var_s1 - offsetof(FxCommonGt4Packet, uv3)))->texture2 =
                            *((u32 *)&((FxCommonColoredTexturedQuad *)(var_s2 - 26))->texture.uv2);
                        *((u16 *)&((FxCommonGt4Packet *)(var_s1 - offsetof(FxCommonGt4Packet, uv3)))->uv3) =
                            *((u16 *)&((FxCommonColoredTexturedQuad *)(var_s2 - 26))->texture.uv3);
                        var_s1 += sizeof(FxCommonGt4Packet);
                        FX_COMMON_LINK_PACKET((FxCommonGt4Packet *)var_s3);
                        var_s3 += sizeof(FxCommonGt4Packet);
                    }
                    primitiveIndex7 += 1;
                    var_s2 += 0x4C;
                    primitiveCount = var_t8->counts[7];
                    transientCursor = gt4Cursor;
                    ++transientCursor;
                    gt4Cursor = transientCursor;
                } while (primitiveIndex7 < primitiveCount);
            }
        }
        g_FxCommonActiveBuffer->data = var_s3;
    }
}

int FxCommon_GetOffsetTableValue(FxCommonOffsetTable *table, int index)
{
    FxCommonOffsetByte *base;
    FxCommonOffsetByte *cursor;

    base = (FxCommonOffsetByte *)table;
    cursor = base + index * sizeof(s32);
    cursor = base + *(s32 *)cursor;
    return ((FxCommonOffsetEntry *)cursor)->value;
}

/* addPrim: chain the packet into the ordering table slot at the depth the
 * projection left in the scratchpad. */
/* Draws every primitive list of one model of a bank with the current GTE
 * matrices: flat and gouraud triangles through RotTransPers3/4, the quads and
 * textured primitives through inline RTPT with back-face culling. */
void FxCommon_DrawModel(void *context, FxCommonPolyModel *model, int index)
{
    (void)context;
    FxCommonRenderScratchpad *sp;
    FxCommonModelF3 *f3;
    FxCommonModelF4 *f4;
    FxCommonModelG3 *g3;
    FxCommonModelG4 *g4;
    FxCommonModelFt3 *ft3;
    FxCommonModelFt4 *ft4;
    FxCommonModelGt3 *gt3;
    FxCommonModelGt4 *gt4;
    FxCommonModelF3Packet *f3Packet;
    FxCommonModelF4Packet *f4Packet;
    FxCommonModelG3Packet *g3Packet;
    FxCommonModelG4Packet *g4Packet;
    FxCommonModelFt3Packet *ft3Packet;
    FxCommonModelFt4Packet *ft4Packet;
    FxCommonModelGt3Packet *gt3Packet;
    FxCommonModelGt4Packet *gt4Packet;
    int i;
    int result;
    s32 *entry;

    entry = model->bankOffsets + index;
    model = (FxCommonPolyModel *)&model->bytes[*entry];
    f3Packet = (FxCommonModelF3Packet *)g_FxCommonActiveBuffer->data;
    f3 = (FxCommonModelF3 *)&model->bytes[model->header.offsets[0]];
    f4 = (FxCommonModelF4 *)&model->bytes[model->header.offsets[1]];
    g3 = (FxCommonModelG3 *)&model->bytes[model->header.offsets[2]];
    g4 = (FxCommonModelG4 *)&model->bytes[model->header.offsets[3]];
    ft3 = (FxCommonModelFt3 *)&model->bytes[model->header.offsets[4]];
    ft4 = (FxCommonModelFt4 *)&model->bytes[model->header.offsets[5]];
    gt3 = (FxCommonModelGt3 *)&model->bytes[model->header.offsets[6]];
    gt4 = (FxCommonModelGt4 *)&model->bytes[model->header.offsets[7]];
    sp = FX_COMMON_SCRATCHPAD;

    for (i = 0; i < model->header.counts[0]; i++, f3++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)f3Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*f3Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)f3Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)f3Packet;return;
        }
        result = RotAverageNclip3(&f3->vertices[0], &f3->vertices[1], &f3->vertices[2],
                               &sp->screenCoordinates[0], &sp->screenCoordinates[1],
                               &sp->screenCoordinates[2], &sp->perspective, &sp->orderingDepth,
                               &sp->transformFlags);
        sp->orderingDepth = (s32)((u32)sp->orderingDepth + (u32)g_FxCommonDepthBias);
        if (result > 0 && sp->orderingDepth - 1U < 0xFFF) {
            sp->orderingDepth >>= 2;
            f3Packet->tag.length = 4;
            f3Packet->xy0 = sp->screenCoordinates[0];
            f3Packet->xy1 = sp->screenCoordinates[1];
            f3Packet->xy2 = sp->screenCoordinates[2];
            *(u32 *)&f3Packet->rgb = f3->color.word;
            FX_COMMON_LINK_PACKET(f3Packet);
            f3Packet++;
        }
    }

    f4Packet = (FxCommonModelF4Packet *)f3Packet;
    for (i = 0; i < model->header.counts[1]; i++, f4++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)f4Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*f4Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)f4Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)f4Packet;return;
        }
        result = RotAverageNclip4(&f4->vertices[0], &f4->vertices[1], &f4->vertices[2],
                               &f4->vertices[3], &sp->screenCoordinates[0],
                               &sp->screenCoordinates[1], &sp->screenCoordinates[2],
                               &sp->screenCoordinates[3], &sp->perspective, &sp->orderingDepth,
                               &sp->transformFlags);
        sp->orderingDepth = (s32)((u32)sp->orderingDepth + (u32)g_FxCommonDepthBias);
        if (result > 0 && sp->orderingDepth - 1U < 0xFFF) {
            sp->orderingDepth >>= 2;
            f4Packet->tag.length = 5;
            f4Packet->xy0 = sp->screenCoordinates[0];
            f4Packet->xy1 = sp->screenCoordinates[1];
            f4Packet->xy2 = sp->screenCoordinates[2];
            f4Packet->xy3 = sp->screenCoordinates[3];
            *(u32 *)&f4Packet->rgb = f4->color.word;
            FX_COMMON_LINK_PACKET(f4Packet);
            f4Packet++;
        }
    }

    g3Packet = (FxCommonModelG3Packet *)f4Packet;
    for (i = 0; i < model->header.counts[2]; i++, g3++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)g3Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*g3Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)g3Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)g3Packet;return;
        }
        result = RotAverageNclip3(&g3->vertices[0], &g3->vertices[1], &g3->vertices[2],
                               &sp->screenCoordinates[0], &sp->screenCoordinates[1],
                               &sp->screenCoordinates[2], &sp->perspective, &sp->orderingDepth,
                               &sp->transformFlags);
        sp->orderingDepth = ((s32)((u32)sp->orderingDepth + (u32)g_FxCommonDepthBias)) >> 2;
        if (result > 0 && sp->orderingDepth - 1U < 0xFFF) {
            g3Packet->tag.length = 6;
            g3Packet->xy0 = sp->screenCoordinates[0];
            g3Packet->xy1 = sp->screenCoordinates[1];
            g3Packet->xy2 = sp->screenCoordinates[2];
            *(u32 *)&g3Packet->rgb0 = g3->colors[0].word;
            *(u32 *)&g3Packet->rgb1 = g3->colors[1].word;
            *(u32 *)&g3Packet->rgb2 = g3->colors[2].word;
            FX_COMMON_LINK_PACKET(g3Packet);
            g3Packet++;
        }
    }

    g4Packet = (FxCommonModelG4Packet *)g3Packet;
    for (i = 0; i < model->header.counts[3]; i++, g4++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)g4Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*g4Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)g4Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)g4Packet;return;
        }
        gte_ldv3(&g4->vertices[0], &g4->vertices[1], &g4->vertices[2]);
        gte_rtpt_padded();
        gte_nclip();
        gte_stmac0(&sp->normalClip);
        if (sp->normalClip > 0) {
            gte_avsz3_padded();
            gte_stotz(&sp->orderingDepth);
            sp->orderingDepth = (s32)((u32)sp->orderingDepth + (u32)g_FxCommonDepthBias);
            if (sp->orderingDepth > 0) {
                sp->orderingDepth >>= 2;
                gte_stsxy3(&g4Packet->xy0, &g4Packet->xy1, &g4Packet->xy2);
                gte_ldv0(&g4->vertices[3]);
                gte_rtps();
                gte_stsxy2(&g4Packet->xy3);
                g4Packet->tag.length = 8;
                *(u32 *)&g4Packet->rgb0 = g4->colors[0].word;
                *(u32 *)&g4Packet->rgb1 = g4->colors[1].word;
                *(u32 *)&g4Packet->rgb2 = g4->colors[2].word;
                *(u32 *)&g4Packet->rgb3 = g4->colors[3].word;
                FX_COMMON_LINK_PACKET(g4Packet);
                g4Packet++;
            }
        }
    }
    g_FxCommonActiveBuffer->data = (u8 *)g4Packet;

    ft3Packet = (FxCommonModelFt3Packet *)g4Packet;
    for (i = 0; i < model->header.counts[4]; i++, ft3++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)ft3Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*ft3Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)ft3Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)ft3Packet;return;
        }
        gte_ldv3(&ft3->vertices[0], &ft3->vertices[1], &ft3->vertices[2]);
        gte_rtpt_padded();
        gte_nclip();
        gte_stmac0(&sp->normalClip);
        if (sp->normalClip > 0) {
            gte_avsz3_padded();
            gte_stotz(&sp->orderingDepth);
            sp->orderingDepth = (s32)((u32)sp->orderingDepth + (u32)g_FxCommonDepthBias);
            if (sp->orderingDepth > 0) {
                sp->orderingDepth >>= 2;
                gte_stsxy3(&ft3Packet->xy0, &ft3Packet->xy1, &ft3Packet->xy2);
                ft3Packet->tag.length = 7;
                *(u32 *)&ft3Packet->rgb = ft3->color.word;
                *(u32 *)&ft3Packet->uv0 = ft3->uv0;
                *(u32 *)&ft3Packet->uv1 = ft3->uv1;
                *(u32 *)&ft3Packet->uv2 = ft3->uv2;
                FX_COMMON_LINK_PACKET(ft3Packet);
                ft3Packet++;
            }
        }
    }

    ft4Packet = (FxCommonModelFt4Packet *)ft3Packet;
    for (i = 0; i < model->header.counts[5]; i++, ft4++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)ft4Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*ft4Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)ft4Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)ft4Packet;return;
        }
        gte_ldv3(&ft4->vertices[0], &ft4->vertices[1], &ft4->vertices[2]);
        gte_rtpt_padded();
        gte_nclip();
        gte_stmac0(&sp->normalClip);
        if (sp->normalClip > 0) {
            gte_avsz3_padded();
            gte_stotz(&sp->orderingDepth);
            sp->orderingDepth = (s32)((u32)sp->orderingDepth + (u32)g_FxCommonDepthBias);
            if (sp->orderingDepth > 0) {
                sp->orderingDepth >>= 2;
                gte_stsxy3(&ft4Packet->xy0, &ft4Packet->xy1, &ft4Packet->xy2);
                gte_ldv0(&ft4->vertices[3]);
                gte_rtps();
                gte_stsxy2(&ft4Packet->xy3);
                ft4Packet->tag.length = 9;
                *(u32 *)&ft4Packet->rgb = ft4->color.word;
                *(u32 *)&ft4Packet->uv0 = ft4->uv0;
                *(u32 *)&ft4Packet->uv1 = ft4->uv1;
                *(u32 *)&ft4Packet->uv2 = ft4->uv23.word;
                *(u16 *)&ft4Packet->u3 = ft4->uv23.halves.uv3;
                FX_COMMON_LINK_PACKET(ft4Packet);
                ft4Packet++;
            }
        }
    }

    gt3Packet = (FxCommonModelGt3Packet *)ft4Packet;
    for (i = 0; i < model->header.counts[6]; i++, gt3++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)gt3Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*gt3Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)gt3Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)gt3Packet;return;
        }
        gte_ldv3(&gt3->vertices[0], &gt3->vertices[1], &gt3->vertices[2]);
        gte_rtpt_padded();
        gte_nclip();
        gte_stmac0(&sp->normalClip);
        if (sp->normalClip > 0) {
            gte_avsz3_padded();
            gte_stotz(&sp->orderingDepth);
            sp->orderingDepth = (s32)((u32)sp->orderingDepth + (u32)g_FxCommonDepthBias);
            if (sp->orderingDepth > 0) {
                sp->orderingDepth >>= 2;
                gte_stsxy3(&gt3Packet->xy0, &gt3Packet->xy1, &gt3Packet->xy2);
                gt3Packet->tag.length = 9;
                *(u32 *)&gt3Packet->rgb0 = gt3->colors[0].word;
                *(u32 *)&gt3Packet->rgb1 = gt3->colors[1].word;
                *(u32 *)&gt3Packet->rgb2 = gt3->colors[2].word;
                *(u32 *)&gt3Packet->uv0 = gt3->uv0;
                *(u32 *)&gt3Packet->uv1 = gt3->uv1;
                *(u32 *)&gt3Packet->uv2 = gt3->uv2;
                FX_COMMON_LINK_PACKET(gt3Packet);
                gt3Packet++;
            }
        }
    }

    gt4Packet = (FxCommonModelGt4Packet *)gt3Packet;
    for (i = 0; i < model->header.counts[7]; i++, gt4++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)gt4Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*gt4Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)gt4Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)gt4Packet;return;
        }
        gte_ldv3(&gt4->vertices[0], &gt4->vertices[1], &gt4->vertices[2]);
        gte_rtpt_padded();
        gte_nclip();
        gte_stmac0(&sp->normalClip);
        if (sp->normalClip > 0) {
            gte_avsz3_padded();
            gte_stotz(&sp->orderingDepth);
            sp->orderingDepth = (s32)((u32)sp->orderingDepth + (u32)g_FxCommonDepthBias);
            if (sp->orderingDepth > 0) {
                sp->orderingDepth >>= 2;
                gte_stsxy3(&gt4Packet->xy0, &gt4Packet->xy1, &gt4Packet->xy2);
                gte_ldv0(&gt4->vertices[3]);
                gte_rtps();
                gte_stsxy2(&gt4Packet->xy3);
                gt4Packet->tag.length = 12;
                *(u32 *)&gt4Packet->rgb0 = gt4->colors[0].word;
                *(u32 *)&gt4Packet->rgb1 = gt4->colors[1].word;
                *(u32 *)&gt4Packet->rgb2 = gt4->colors[2].word;
                *(u32 *)&gt4Packet->rgb3 = gt4->colors[3].word;
                *(u32 *)&gt4Packet->uv0 = gt4->uv0;
                *(u32 *)&gt4Packet->uv1 = gt4->uv1;
                *(u32 *)&gt4Packet->uv2 = gt4->uv23.word;
                *(u16 *)&gt4Packet->u3 = gt4->uv23.halves.uv3;
                FX_COMMON_LINK_PACKET(gt4Packet);
                gt4Packet++;
            }
        }
    }
    g_FxCommonActiveBuffer->data = (u8 *)gt4Packet;
}


/* addPrim: chain the packet into the ordering table slot at the depth the
 * projection left in the scratchpad. */
/* Draws one model of a bank like FxCommon_DrawModel, but darkens the gouraud
 * colours to a quarter and tints every textured primitive with the scene
 * tint colour. */
void FxCommon_DrawModelTinted(void *context, FxCommonPolyModel *model, int index)
{
    (void)context;
    FxCommonRenderScratchpad *sp;
    FxCommonModelF3 *f3;
    FxCommonModelF4 *f4;
    FxCommonModelG3 *g3;
    FxCommonModelG4 *g4;
    FxCommonModelFt3 *ft3;
    FxCommonModelFt4 *ft4;
    FxCommonModelGt3 *gt3;
    FxCommonModelGt4 *gt4;
    FxCommonModelF3Packet *f3Packet;
    FxCommonModelF4Packet *f4Packet;
    FxCommonModelG3Packet *g3Packet;
    FxCommonModelG4Packet *g4Packet;
    FxCommonModelFt3Packet *ft3Packet;
    FxCommonModelFt4Packet *ft4Packet;
    FxCommonModelGt3Packet *gt3Packet;
    FxCommonModelGt4Packet *gt4Packet;
    int i;
    int result;
    s32 *entry;

    entry = model->bankOffsets + index;
    model = (FxCommonPolyModel *)&model->bytes[*entry];
    f3Packet = (FxCommonModelF3Packet *)g_FxCommonActiveBuffer->data;
    f3 = (FxCommonModelF3 *)&model->bytes[model->header.offsets[0]];
    f4 = (FxCommonModelF4 *)&model->bytes[model->header.offsets[1]];
    g3 = (FxCommonModelG3 *)&model->bytes[model->header.offsets[2]];
    g4 = (FxCommonModelG4 *)&model->bytes[model->header.offsets[3]];
    ft3 = (FxCommonModelFt3 *)&model->bytes[model->header.offsets[4]];
    ft4 = (FxCommonModelFt4 *)&model->bytes[model->header.offsets[5]];
    gt3 = (FxCommonModelGt3 *)&model->bytes[model->header.offsets[6]];
    gt4 = (FxCommonModelGt4 *)&model->bytes[model->header.offsets[7]];
    sp = FX_COMMON_SCRATCHPAD;

    for (i = 0; i < model->header.counts[0]; i++, f3++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)f3Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*f3Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)f3Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)f3Packet;return;
        }
        result = RotAverageNclip3(&f3->vertices[0], &f3->vertices[1], &f3->vertices[2],
                               &sp->screenCoordinates[0], &sp->screenCoordinates[1],
                               &sp->screenCoordinates[2], &sp->perspective, &sp->orderingDepth,
                               &sp->transformFlags);
        sp->orderingDepth = ((s32)((u32)sp->orderingDepth + (u32)g_FxCommonDepthBias)) >> 2;
        if (result > 0 && sp->orderingDepth - 1U < 0xFFF) {
            f3Packet->tag.length = 4;
            f3Packet->xy0 = sp->screenCoordinates[0];
            f3Packet->xy1 = sp->screenCoordinates[1];
            f3Packet->xy2 = sp->screenCoordinates[2];
            *(u32 *)&f3Packet->rgb = f3->color.word;
            FX_COMMON_LINK_PACKET(f3Packet);
            f3Packet++;
        }
    }

    f4Packet = (FxCommonModelF4Packet *)f3Packet;
    for (i = 0; i < model->header.counts[1]; i++, f4++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)f4Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*f4Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)f4Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)f4Packet;return;
        }
        result = RotAverageNclip4(&f4->vertices[0], &f4->vertices[1], &f4->vertices[2],
                               &f4->vertices[3], &sp->screenCoordinates[0],
                               &sp->screenCoordinates[1], &sp->screenCoordinates[2],
                               &sp->screenCoordinates[3], &sp->perspective, &sp->orderingDepth,
                               &sp->transformFlags);
        sp->orderingDepth = ((s32)((u32)sp->orderingDepth + (u32)g_FxCommonDepthBias)) >> 2;
        if (result > 0 && sp->orderingDepth - 1U < 0xFFF) {
            f4Packet->tag.length = 5;
            f4Packet->xy0 = sp->screenCoordinates[0];
            f4Packet->xy1 = sp->screenCoordinates[1];
            f4Packet->xy2 = sp->screenCoordinates[2];
            f4Packet->xy3 = sp->screenCoordinates[3];
            *(u32 *)&f4Packet->rgb = f4->color.word;
            FX_COMMON_LINK_PACKET(f4Packet);
            f4Packet++;
        }
    }

    g3Packet = (FxCommonModelG3Packet *)f4Packet;
    for (i = 0; i < model->header.counts[2]; i++, g3++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)g3Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*g3Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)g3Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)g3Packet;return;
        }
        result = RotAverageNclip3(&g3->vertices[0], &g3->vertices[1], &g3->vertices[2],
                               &sp->screenCoordinates[0], &sp->screenCoordinates[1],
                               &sp->screenCoordinates[2], &sp->perspective, &sp->orderingDepth,
                               &sp->transformFlags);
        sp->orderingDepth = ((s32)((u32)sp->orderingDepth + (u32)g_FxCommonDepthBias)) >> 2;
        if (result > 0 && sp->orderingDepth - 1U < 0xFFF) {
            g3Packet->tag.length = 6;
            g3Packet->xy0 = sp->screenCoordinates[0];
            g3Packet->xy1 = sp->screenCoordinates[1];
            g3Packet->xy2 = sp->screenCoordinates[2];
            *(u32 *)&g3Packet->rgb0 = g3->colors[0].word;
            g3Packet->rgb0.r = g3->colors[0].rgb.r >> 2;
            g3Packet->rgb0.g = g3->colors[0].rgb.g >> 2;
            g3Packet->rgb0.b = g3->colors[0].rgb.b >> 2;
            g3Packet->rgb1.r = g3->colors[1].rgb.r >> 2;
            g3Packet->rgb1.g = g3->colors[1].rgb.g >> 2;
            g3Packet->rgb1.b = g3->colors[1].rgb.b >> 2;
            g3Packet->rgb2.r = g3->colors[2].rgb.r >> 2;
            g3Packet->rgb2.g = g3->colors[2].rgb.g >> 2;
            g3Packet->rgb2.b = g3->colors[2].rgb.b >> 2;
            FX_COMMON_LINK_PACKET(g3Packet);
            g3Packet++;
        }
    }

    g4Packet = (FxCommonModelG4Packet *)g3Packet;
    for (i = 0; i < model->header.counts[3]; i++, g4++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)g4Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*g4Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)g4Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)g4Packet;return;
        }
        result = RotAverageNclip4(&g4->vertices[0], &g4->vertices[1], &g4->vertices[2],
                               &g4->vertices[3], &sp->screenCoordinates[0],
                               &sp->screenCoordinates[1], &sp->screenCoordinates[2],
                               &sp->screenCoordinates[3], &sp->perspective, &sp->orderingDepth,
                               &sp->transformFlags);
        sp->orderingDepth = (s32)((u32)sp->orderingDepth + (u32)g_FxCommonDepthBias);
        if (result > 0 && sp->orderingDepth - 1U < 0xFFF) {
            sp->orderingDepth >>= 2;
            g4Packet->tag.length = 8;
            g4Packet->xy0 = sp->screenCoordinates[0];
            g4Packet->xy1 = sp->screenCoordinates[1];
            g4Packet->xy2 = sp->screenCoordinates[2];
            g4Packet->xy3 = sp->screenCoordinates[3];
            *(u32 *)&g4Packet->rgb0 = g4->colors[0].word;
            g4Packet->rgb0.r = g4->colors[0].rgb.r >> 2;
            g4Packet->rgb0.g = g4->colors[0].rgb.g >> 2;
            g4Packet->rgb0.b = g4->colors[0].rgb.b >> 2;
            g4Packet->rgb1.r = g4->colors[1].rgb.r >> 2;
            g4Packet->rgb1.g = g4->colors[1].rgb.g >> 2;
            g4Packet->rgb1.b = g4->colors[1].rgb.b >> 2;
            g4Packet->rgb2.r = g4->colors[2].rgb.r >> 2;
            g4Packet->rgb2.g = g4->colors[2].rgb.g >> 2;
            g4Packet->rgb2.b = g4->colors[2].rgb.b >> 2;
            g4Packet->rgb3.r = g4->colors[3].rgb.r >> 2;
            g4Packet->rgb3.g = g4->colors[3].rgb.g >> 2;
            g4Packet->rgb3.b = g4->colors[3].rgb.b >> 2;
            FX_COMMON_LINK_PACKET(g4Packet);
            g4Packet++;
        }
    }

    ft3Packet = (FxCommonModelFt3Packet *)g4Packet;
    for (i = 0; i < model->header.counts[4]; i++, ft3++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)ft3Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*ft3Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)ft3Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)ft3Packet;return;
        }
        gte_ldv3(&ft3->vertices[0], &ft3->vertices[1], &ft3->vertices[2]);
        gte_rtpt_padded();
        gte_nclip();
        gte_stmac0(&sp->normalClip);
        if (sp->normalClip > 0) {
            gte_avsz3_padded();
            gte_stotz(&sp->orderingDepth);
            sp->orderingDepth = ((s32)((u32)sp->orderingDepth + (u32)g_FxCommonDepthBias)) >> 2;
            if (sp->orderingDepth > 0) {
                gte_stsxy3(&ft3Packet->xy0, &ft3Packet->xy1, &ft3Packet->xy2);
                ft3Packet->tag.length = 7;
                *(u32 *)&ft3Packet->rgb = ft3->color.word;
                ft3Packet->rgb.r = g_FxCommonFlatColor[0];
                ft3Packet->rgb.g = g_FxCommonFlatColor[1];
                ft3Packet->rgb.b = g_FxCommonFlatColor[2];
                *(u32 *)&ft3Packet->uv0 = ft3->uv0;
                *(u32 *)&ft3Packet->uv1 = ft3->uv1;
                *(u32 *)&ft3Packet->uv2 = ft3->uv2;
                FX_COMMON_LINK_PACKET(ft3Packet);
                ft3Packet++;
            }
        }
    }

    ft4Packet = (FxCommonModelFt4Packet *)ft3Packet;
    for (i = 0; i < model->header.counts[5]; i++, ft4++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)ft4Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*ft4Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)ft4Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)ft4Packet;return;
        }
        gte_ldv3(&ft4->vertices[0], &ft4->vertices[1], &ft4->vertices[2]);
        gte_rtpt_padded();
        gte_nclip();
        gte_stmac0(&sp->normalClip);
        if (sp->normalClip > 0) {
            gte_avsz3_padded();
            gte_stotz(&sp->orderingDepth);
            sp->orderingDepth = ((s32)((u32)sp->orderingDepth + (u32)g_FxCommonDepthBias)) >> 2;
            if (sp->orderingDepth > 0) {
                gte_stsxy3(&ft4Packet->xy0, &ft4Packet->xy1, &ft4Packet->xy2);
                gte_ldv0(&ft4->vertices[3]);
                gte_rtps();
                gte_stsxy2(&ft4Packet->xy3);
                ft4Packet->tag.length = 9;
                *(u32 *)&ft4Packet->rgb = ft4->color.word;
                ft4Packet->rgb.r = g_FxCommonFlatColor[0];
                ft4Packet->rgb.g = g_FxCommonFlatColor[1];
                ft4Packet->rgb.b = g_FxCommonFlatColor[2];
                *(u32 *)&ft4Packet->uv0 = ft4->uv0;
                *(u32 *)&ft4Packet->uv1 = ft4->uv1;
                *(u32 *)&ft4Packet->uv2 = ft4->uv23.word;
                *(u16 *)&ft4Packet->u3 = ft4->uv23.halves.uv3;
                FX_COMMON_LINK_PACKET(ft4Packet);
                ft4Packet++;
            }
        }
    }

    gt3Packet = (FxCommonModelGt3Packet *)ft4Packet;
    for (i = 0; i < model->header.counts[6]; i++, gt3++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)gt3Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*gt3Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)gt3Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)gt3Packet;return;
        }
        gte_ldv3(&gt3->vertices[0], &gt3->vertices[1], &gt3->vertices[2]);
        gte_rtpt_padded();
        gte_nclip();
        gte_stmac0(&sp->normalClip);
        if (sp->normalClip > 0) {
            gte_avsz3_padded();
            gte_stotz(&sp->orderingDepth);
            sp->orderingDepth = ((s32)((u32)sp->orderingDepth + (u32)g_FxCommonDepthBias)) >> 2;
            if (sp->orderingDepth > 0) {
                gte_stsxy3(&gt3Packet->xy0, &gt3Packet->xy1, &gt3Packet->xy2);
                gt3Packet->tag.length = 9;
                *(u32 *)&gt3Packet->rgb0 = gt3->colors[0].word;
                gt3Packet->rgb0.r = g_FxCommonFlatColor[0];
                gt3Packet->rgb0.g = g_FxCommonFlatColor[1];
                gt3Packet->rgb0.b = g_FxCommonFlatColor[2];
                *(u32 *)&gt3Packet->rgb2 = *(u32 *)&gt3Packet->rgb1 = *(u32 *)g_FxCommonFlatColor;
                *(u32 *)&gt3Packet->uv0 = gt3->uv0;
                *(u32 *)&gt3Packet->uv1 = gt3->uv1;
                *(u32 *)&gt3Packet->uv2 = gt3->uv2;
                FX_COMMON_LINK_PACKET(gt3Packet);
                gt3Packet++;
            }
        }
    }

    gt4Packet = (FxCommonModelGt4Packet *)gt3Packet;
    for (i = 0; i < model->header.counts[7]; i++, gt4++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)gt4Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*gt4Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)gt4Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)gt4Packet;return;
        }
        gte_ldv3(&gt4->vertices[0], &gt4->vertices[1], &gt4->vertices[2]);
        gte_rtpt_padded();
        gte_nclip();
        gte_stmac0(&sp->normalClip);
        if (sp->normalClip > 0) {
            gte_avsz3_padded();
            gte_stotz(&sp->orderingDepth);
            sp->orderingDepth = ((s32)((u32)sp->orderingDepth + (u32)g_FxCommonDepthBias)) >> 2;
            if (sp->orderingDepth > 0) {
                gte_stsxy3(&gt4Packet->xy0, &gt4Packet->xy1, &gt4Packet->xy2);
                gte_ldv0(&gt4->vertices[3]);
                gte_rtps();
                gte_stsxy2(&gt4Packet->xy3);
                gt4Packet->tag.length = 12;
                *(u32 *)&gt4Packet->rgb0 = gt4->colors[0].word;
                gt4Packet->rgb0.r = g_FxCommonFlatColor[0];
                gt4Packet->rgb0.g = g_FxCommonFlatColor[1];
                gt4Packet->rgb0.b = g_FxCommonFlatColor[2];
                *(u32 *)&gt4Packet->rgb3 = *(u32 *)&gt4Packet->rgb2 = *(u32 *)&gt4Packet->rgb1 =
                    *(u32 *)g_FxCommonFlatColor;
                *(u32 *)&gt4Packet->uv0 = gt4->uv0;
                *(u32 *)&gt4Packet->uv1 = gt4->uv1;
                *(u32 *)&gt4Packet->uv2 = gt4->uv23.word;
                *(u16 *)&gt4Packet->u3 = gt4->uv23.halves.uv3;
                FX_COMMON_LINK_PACKET(gt4Packet);
                gt4Packet++;
            }
        }
    }
    g_FxCommonActiveBuffer->data = (u8 *)gt4Packet;
}


/* addPrim: chain the packet into the ordering table slot at the depth the
 * projection left in the scratchpad. */
/* Draws one model of a bank at the fixed ordering depth g_FxCommonDepthBias: no
 * per-primitive depth sort, only the projection (or back-face) test decides
 * whether a primitive is drawn. */
void FxCommon_DrawModelFlat(void *context, FxCommonPolyModel *model, int index)
{
    (void)context;
    FxCommonRenderScratchpad *sp;
    FxCommonModelF3 *f3;
    FxCommonModelF4 *f4;
    FxCommonModelG3 *g3;
    FxCommonModelG4 *g4;
    FxCommonModelFt3 *ft3;
    FxCommonModelFt4 *ft4;
    FxCommonModelGt3 *gt3;
    FxCommonModelGt4 *gt4;
    FxCommonModelF3Packet *f3Packet;
    FxCommonModelF4Packet *f4Packet;
    FxCommonModelG3Packet *g3Packet;
    FxCommonModelG4Packet *g4Packet;
    FxCommonModelFt3Packet *ft3Packet;
    FxCommonModelFt4Packet *ft4Packet;
    FxCommonModelGt3Packet *gt3Packet;
    FxCommonModelGt4Packet *gt4Packet;
    int i;
    s32 *entry;
    s32 depth;

    entry = model->bankOffsets + index;
    model = (FxCommonPolyModel *)&model->bytes[*entry];
    f3 = (FxCommonModelF3 *)&model->bytes[model->header.offsets[0]];
    f4 = (FxCommonModelF4 *)&model->bytes[model->header.offsets[1]];
    g3 = (FxCommonModelG3 *)&model->bytes[model->header.offsets[2]];
    g4 = (FxCommonModelG4 *)&model->bytes[model->header.offsets[3]];
    ft3 = (FxCommonModelFt3 *)&model->bytes[model->header.offsets[4]];
    ft4 = (FxCommonModelFt4 *)&model->bytes[model->header.offsets[5]];
    gt3 = (FxCommonModelGt3 *)&model->bytes[model->header.offsets[6]];
    gt4 = (FxCommonModelGt4 *)&model->bytes[model->header.offsets[7]];
    FX_COMMON_SCRATCHPAD->orderingDepth = g_FxCommonDepthBias;
    f3Packet = (FxCommonModelF3Packet *)g_FxCommonActiveBuffer->data;
    sp = FX_COMMON_SCRATCHPAD;

    for (i = 0; i < model->header.counts[0]; i++, f3++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)f3Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*f3Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)f3Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)f3Packet;return;
        }
        if (RotAverageNclip3(&f3->vertices[0], &f3->vertices[1], &f3->vertices[2],
                          &sp->screenCoordinates[0], &sp->screenCoordinates[1],
                          &sp->screenCoordinates[2], &sp->perspective, &depth,
                          &sp->transformFlags) > 0) {
            f3Packet->tag.length = 4;
            f3Packet->xy0 = sp->screenCoordinates[0];
            f3Packet->xy1 = sp->screenCoordinates[1];
            f3Packet->xy2 = sp->screenCoordinates[2];
            *(u32 *)&f3Packet->rgb = f3->color.word;
            FX_COMMON_LINK_PACKET(f3Packet);
            f3Packet++;
        }
    }

    f4Packet = (FxCommonModelF4Packet *)f3Packet;
    for (i = 0; i < model->header.counts[1]; i++, f4++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)f4Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*f4Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)f4Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)f4Packet;return;
        }
        if (RotAverageNclip4(&f4->vertices[0], &f4->vertices[1], &f4->vertices[2],
                          &f4->vertices[3], &sp->screenCoordinates[0], &sp->screenCoordinates[1],
                          &sp->screenCoordinates[2], &sp->screenCoordinates[3], &sp->perspective,
                          &depth, &sp->transformFlags) > 0) {
            f4Packet->tag.length = 5;
            f4Packet->xy0 = sp->screenCoordinates[0];
            f4Packet->xy1 = sp->screenCoordinates[1];
            f4Packet->xy2 = sp->screenCoordinates[2];
            f4Packet->xy3 = sp->screenCoordinates[3];
            *(u32 *)&f4Packet->rgb = f4->color.word;
            FX_COMMON_LINK_PACKET(f4Packet);
            f4Packet++;
        }
    }

    g3Packet = (FxCommonModelG3Packet *)f4Packet;
    for (i = 0; i < model->header.counts[2]; i++, g3++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)g3Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*g3Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)g3Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)g3Packet;return;
        }
        if (RotAverageNclip3(&g3->vertices[0], &g3->vertices[1], &g3->vertices[2],
                          &sp->screenCoordinates[0], &sp->screenCoordinates[1],
                          &sp->screenCoordinates[2], &sp->perspective, &depth,
                          &sp->transformFlags) > 0) {
            g3Packet->tag.length = 6;
            g3Packet->xy0 = sp->screenCoordinates[0];
            g3Packet->xy1 = sp->screenCoordinates[1];
            g3Packet->xy2 = sp->screenCoordinates[2];
            *(u32 *)&g3Packet->rgb0 = g3->colors[0].word;
            *(u32 *)&g3Packet->rgb1 = g3->colors[1].word;
            *(u32 *)&g3Packet->rgb2 = g3->colors[2].word;
            FX_COMMON_LINK_PACKET(g3Packet);
            g3Packet++;
        }
    }

    g4Packet = (FxCommonModelG4Packet *)g3Packet;
    for (i = 0; i < model->header.counts[3]; i++, g4++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)g4Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*g4Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)g4Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)g4Packet;return;
        }
        if (RotAverageNclip4(&g4->vertices[0], &g4->vertices[1], &g4->vertices[2],
                          &g4->vertices[3], &sp->screenCoordinates[0], &sp->screenCoordinates[1],
                          &sp->screenCoordinates[2], &sp->screenCoordinates[3], &sp->perspective,
                          &depth, &sp->transformFlags) > 0) {
            g4Packet->tag.length = 8;
            g4Packet->xy0 = sp->screenCoordinates[0];
            g4Packet->xy1 = sp->screenCoordinates[1];
            g4Packet->xy2 = sp->screenCoordinates[2];
            g4Packet->xy3 = sp->screenCoordinates[3];
            *(u32 *)&g4Packet->rgb0 = g4->colors[0].word;
            *(u32 *)&g4Packet->rgb1 = g4->colors[1].word;
            *(u32 *)&g4Packet->rgb2 = g4->colors[2].word;
            *(u32 *)&g4Packet->rgb3 = g4->colors[3].word;
            FX_COMMON_LINK_PACKET(g4Packet);
            g4Packet++;
        }
    }

    ft3Packet = (FxCommonModelFt3Packet *)g4Packet;
    for (i = 0; i < model->header.counts[4]; i++, ft3++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)ft3Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*ft3Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)ft3Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)ft3Packet;return;
        }
        gte_ldv3(&ft3->vertices[0], &ft3->vertices[1], &ft3->vertices[2]);
        gte_rtpt_padded();
        gte_nclip();
        gte_stmac0(&sp->normalClip);
        if (sp->normalClip > 0) {
            ft3Packet->tag.length = 7;
            ft3Packet->xy0 = sp->screenCoordinates[0];
            ft3Packet->xy1 = sp->screenCoordinates[1];
            ft3Packet->xy2 = sp->screenCoordinates[2];
            *(u32 *)&ft3Packet->rgb = ft3->color.word;
            *(u32 *)&ft3Packet->uv0 = ft3->uv0;
            *(u32 *)&ft3Packet->uv1 = ft3->uv1;
            *(u32 *)&ft3Packet->uv2 = ft3->uv2;
            FX_COMMON_LINK_PACKET(ft3Packet);
            ft3Packet++;
        }
    }

    ft4Packet = (FxCommonModelFt4Packet *)ft3Packet;
    for (i = 0; i < model->header.counts[5]; i++, ft4++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)ft4Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*ft4Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)ft4Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)ft4Packet;return;
        }
        gte_ldv3(&ft4->vertices[0], &ft4->vertices[1], &ft4->vertices[2]);
        gte_rtpt_padded();
        gte_nclip();
        gte_stmac0(&sp->normalClip);
        if (sp->normalClip > 0) {
            gte_avsz3_padded();
            gte_stsxy3(&ft4Packet->xy0, &ft4Packet->xy1, &ft4Packet->xy2);
            gte_ldv0(&ft4->vertices[3]);
            gte_rtps();
            gte_stsxy2(&ft4Packet->xy3);
            ft4Packet->tag.length = 9;
            *(u32 *)&ft4Packet->rgb = ft4->color.word;
            *(u32 *)&ft4Packet->uv0 = ft4->uv0;
            *(u32 *)&ft4Packet->uv1 = ft4->uv1;
            *(u32 *)&ft4Packet->uv2 = ft4->uv23.word;
            *(u16 *)&ft4Packet->u3 = ft4->uv23.halves.uv3;
            FX_COMMON_LINK_PACKET(ft4Packet);
            ft4Packet++;
        }
    }

    gt3Packet = (FxCommonModelGt3Packet *)ft4Packet;
    for (i = 0; i < model->header.counts[6]; i++, gt3++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)gt3Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*gt3Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)gt3Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)gt3Packet;return;
        }
        if (RotAverageNclip3(&gt3->vertices[0], &gt3->vertices[1], &gt3->vertices[2],
                          &sp->screenCoordinates[0], &sp->screenCoordinates[1],
                          &sp->screenCoordinates[2], &sp->perspective, &depth,
                          &sp->transformFlags) > 0) {
            gt3Packet->tag.length = 9;
            gt3Packet->xy0 = sp->screenCoordinates[0];
            gt3Packet->xy1 = sp->screenCoordinates[1];
            gt3Packet->xy2 = sp->screenCoordinates[2];
            *(u32 *)&gt3Packet->uv0 = gt3->uv0;
            *(u32 *)&gt3Packet->uv1 = gt3->uv1;
            *(u32 *)&gt3Packet->uv2 = gt3->uv2;
            *(u32 *)&gt3Packet->rgb0 = gt3->colors[0].word;
            *(u32 *)&gt3Packet->rgb1 = gt3->colors[1].word;
            *(u32 *)&gt3Packet->rgb2 = gt3->colors[2].word;
            FX_COMMON_LINK_PACKET(gt3Packet);
            gt3Packet++;
        }
    }

    gt4Packet = (FxCommonModelGt4Packet *)gt3Packet;
    for (i = 0; i < model->header.counts[7]; i++, gt4++) {
        if(!g_FxCommonActiveBuffer->end || (uintptr_t)gt4Packet>(uintptr_t)g_FxCommonActiveBuffer->end ||
           sizeof(*gt4Packet)>(uintptr_t)g_FxCommonActiveBuffer->end-(uintptr_t)gt4Packet){
            g_FxCommonActiveBuffer->error=-1;g_FxCommonActiveBuffer->data=(u8 *)gt4Packet;return;
        }
        if (RotAverageNclip4(&gt4->vertices[0], &gt4->vertices[1], &gt4->vertices[2],
                          &gt4->vertices[3], &sp->screenCoordinates[0], &sp->screenCoordinates[1],
                          &sp->screenCoordinates[2], &sp->screenCoordinates[3], &sp->perspective,
                          &depth, &sp->transformFlags) > 0) {
            gt4Packet->tag.length = 12;
            gt4Packet->xy0 = sp->screenCoordinates[0];
            gt4Packet->xy1 = sp->screenCoordinates[1];
            gt4Packet->xy2 = sp->screenCoordinates[2];
            gt4Packet->xy3 = sp->screenCoordinates[3];
            *(u32 *)&gt4Packet->rgb0 = gt4->colors[0].word;
            *(u32 *)&gt4Packet->rgb1 = gt4->colors[1].word;
            *(u32 *)&gt4Packet->rgb2 = gt4->colors[2].word;
            *(u32 *)&gt4Packet->rgb3 = gt4->colors[3].word;
            *(u32 *)&gt4Packet->uv0 = gt4->uv0;
            *(u32 *)&gt4Packet->uv1 = gt4->uv1;
            *(u32 *)&gt4Packet->uv2 = gt4->uv23.word;
            *(u16 *)&gt4Packet->u3 = gt4->uv23.halves.uv3;
            FX_COMMON_LINK_PACKET(gt4Packet);
            gt4Packet++;
        }
    }
    g_FxCommonActiveBuffer->data = (u8 *)gt4Packet;
}


void SetGeomOffset(long,long);
void SetGeomScreen(long);

/* Native storage replaces the executable overlay's address-owned frame records.
 * Packet arenas remain in the original shared scene work area. */
FxCommonRenderScratchpad g_FxCommonRenderWorkspace;
s32 g_FxCommonDepthBias,g_FxCommonDepthBase;
_Alignas(4) u8 g_FxCommonFlatColor[4];
FxCommonFrame g_FxCommonFrames[2];
FxCommonBuffer *g_FxCommonActiveBuffer;

void FxCommon_InitDisplay(void)
{
    RECT rect;

    SetDispMask(0);
    SetBackColor(0x80, 0x80, 0x80);
    SetFarColor(0, 0, 0);
    SetFogNearFar(0x1964, 0x2CEC, 0x300);
    SetGeomOffset(0xA0, 0x78);
    SetGeomScreen(0x300);

    FxCommon_SelectBufferAllocation(&g_FxCommonFrames[0].buffer);
    FxCommon_SelectBufferAllocation(&g_FxCommonFrames[1].buffer);
    g_FxCommonFrames[0].buffer.allocation = (RenderNativeTag *)g_OtBufferTable[0];
    g_FxCommonFrames[1].buffer.allocation = (RenderNativeTag *)g_RenderOtBufferBaseAlt[0];
    ClearOTagR(g_FxCommonFrames[0].buffer.allocation, 0x1000);
    ClearOTagR(g_FxCommonFrames[1].buffer.allocation, 0x1000);

    SetDefDrawEnv(&g_FxCommonFrames[0].drawEnv, 0, 0, 0x140, 0xF0);
    SetDefDispEnv(&g_FxCommonFrames[0].dispEnv, 0, 0xF0, 0x140, 0xF0);
    SetDefDrawEnv(&g_FxCommonFrames[1].drawEnv, 0, 0xF0, 0x140, 0xF0);
    SetDefDispEnv(&g_FxCommonFrames[1].dispEnv, 0, 0, 0x140, 0xF0);
    for(unsigned i=0;i<2;++i){
        g_FxCommonFrames[i].drawEnv.isbg=0;
        g_FxCommonFrames[i].drawEnv.r0=0;
        g_FxCommonFrames[i].drawEnv.g0=0;
        g_FxCommonFrames[i].drawEnv.b0=0;
    }

    rect=(RECT){0,0,0x140,0xF0};
    ClearImage(&rect, 0, 0, 0);
    rect.y=0xF0;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
    SetDispMask(1);
    g_FxCommonActiveBuffer = &g_FxCommonFrames[0].buffer;
}

void FxCommon_ClearActiveOrdering(void)
{
    ClearOTagR(g_FxCommonActiveBuffer->allocation, 0x1000);
    FxCommon_SelectBufferAllocation(g_FxCommonActiveBuffer);
}

void FxCommon_SelectBufferAllocation(FxCommonBuffer *buffer)
{
    if (buffer == &g_FxCommonFrames[0].buffer)
        buffer->data = g_RenderScratchBufferBase[0];
    else
        buffer->data = g_RenderScratchBufferBase[0] + sizeof(FxCommonBufferRegion);
    buffer->end=buffer->data+sizeof(FxCommonBufferRegion);
    buffer->error=0;
}
