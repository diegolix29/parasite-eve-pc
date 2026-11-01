#ifndef PE1_RENDER_PRIM_H
#define PE1_RENDER_PRIM_H

#include "common.h"
#include "pe1/gte_types.h"
#include "pe1/render_packets.h"

/* A render primitive list and its 0x10-byte color records (Render_SetPrimColour).
 * The stride 0x10 is load-bearing (code indexes entries with i<<4). */
typedef struct PrimEntry {            /* 0x10 */
    unsigned char pad_00[0x04];
    unsigned char r;                  /* 0x04 */
    unsigned char g;                  /* 0x05 */
    unsigned char b;                  /* 0x06 */
    unsigned char pad_07[0x09];
} PrimEntry;

typedef struct PrimObj {
    unsigned char pad_00[0x26];
    unsigned short count;             /* 0x26 */
    unsigned char pad_28[0x08];
    PrimEntry *entries;               /* 0x30 */
} PrimObj;

/* Grid cursor state of Render_MoveGridCursor: the selected entry, the one
 * selected before it, and the selected entry's unpacked position. */
extern int g_GridCursorPrevious, g_GridCursorIndex, g_GridCursorX, g_GridCursorY;
int Render_MoveGridCursor(PrimObj *obj, u32 *entries, u8 ascending, u8 restart);

/* Flat-color GPU line packet: tag, RGB/code, and two screen points. */
typedef struct RenderLinePacket {
    RenderNativeTag tag;
    u8 r, g, b, code;
    s16 x0, y0, x1, y1;
} RenderLinePacket;

/* Fixed-size flat-color tile and draw-mode packets emitted by the main
 * renderer. */
typedef struct RenderColorTilePacket {
    RenderNativeTag tag;
    union {
        RenderPacketColor color;
        struct { u8 r0, g0, b0, code; };
    };
    u16 x0, y0;
    union { u16 w; u16 x1; };
    union { u16 h; u16 y1; };
} RenderColorTilePacket;

/* The wipe renderer uses the same native TILE with a packed colour view. */
typedef RenderColorTilePacket RenderFlatTilePacket;

/* Flat-colour triangle packet (POLY_F3). Coordinates retain the unsigned
 * halfword view used by the wipe renderer. */
typedef struct RenderColorTrianglePacket {
    RenderNativeTag tag;
    RenderPacketColor color;
    u16 x0, y0, x1, y1, x2, y2;
} RenderColorTrianglePacket;

PE1_STATIC_ASSERT(sizeof(RenderColorTrianglePacket) == sizeof(RenderNativeTag) + 16,
                  render_color_triangle_size);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RenderColorTrianglePacket, x0) == sizeof(RenderNativeTag) + 4,
                  render_color_triangle_first_vertex);

/* Four-vertex Gouraud quad (POLY_G4), used for HUD gauge gradients. */
typedef struct RenderGouraudQuad {
    RenderNativeTag tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
    u8 r3, g3, b3, pad3;
    s16 x3, y3;
} RenderGouraudQuad;

PE1_STATIC_ASSERT(sizeof(RenderGouraudQuad) == sizeof(RenderNativeTag) + 32, render_gouraud_quad_size);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RenderGouraudQuad, x0) == sizeof(RenderNativeTag) + 4,
                  render_gouraud_quad_first_vertex);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RenderGouraudQuad, x1) == sizeof(RenderNativeTag) + 12,
                  render_gouraud_quad_second_vertex);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RenderGouraudQuad, x2) == sizeof(RenderNativeTag) + 20,
                  render_gouraud_quad_third_vertex);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RenderGouraudQuad, x3) == sizeof(RenderNativeTag) + 28,
                  render_gouraud_quad_fourth_vertex);

typedef struct RenderDrawModePacket {
    RenderNativeTag tag;
    uintptr_t command;
} RenderDrawModePacket;

typedef struct RenderModeTilePacket {
    RenderDrawModePacket mode;
    RenderColorTilePacket tile;
} RenderModeTilePacket;

/* Four-vertex textured GPU packet. The final halfwords are packet padding. */
typedef struct RenderTexturedQuad {
    RenderNativeTag tag;
    union { u32 word; struct { u8 r, g, b, code; } bytes; } color;
    u16 x0, y0; u8 u0, v0; u16 clut;
    u16 x1, y1; u8 u1, v1; u16 tpage;
    u16 x2, y2; u8 u2, v2; u16 pad2;
    u16 x3, y3; u8 u3, v3; u16 pad3;
} RenderTexturedQuad;

PE1_STATIC_ASSERT(sizeof(RenderTexturedQuad) == ((sizeof(RenderNativeTag) + 36 + _Alignof(RenderTexturedQuad) - 1) / _Alignof(RenderTexturedQuad)) * _Alignof(RenderTexturedQuad), render_textured_quad_size);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RenderTexturedQuad, clut) == sizeof(RenderNativeTag) + 10, render_quad_clut_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RenderTexturedQuad, tpage) == sizeof(RenderNativeTag) + 18, render_quad_tpage_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RenderTexturedQuad, x3) == sizeof(RenderNativeTag) + 28, render_quad_x3_offset);

/* Native PsyZ sprite and texture-page packets built for room tiles.
 * Address and command length occupy full native tag words. */
typedef struct RenderTilePacket {
    uintptr_t address, length;
    /* Queued presentation metadata; the original SPRT_16 follows unchanged.
     * The NOP marker declares a packed command stream on LP64. */
    u32 hd_command, hd_key_low, hd_key_high, packed_marker;
    u8 r, g, b, code;
    u16 x, y;
    u8 u, v;
    u16 clut;
} RenderTilePacket;

typedef struct RenderTexturePagePacket {
    uintptr_t address, length;
    u32 command;
} RenderTexturePagePacket;

/* Texture-page command followed by the variable-size sprite it configures. */
typedef struct RenderPagedSpritePacket {
    RenderTexturePagePacket page;
    RenderSpritePacket sprite;
} RenderPagedSpritePacket;

PE1_STATIC_ASSERT(sizeof(RenderPagedSpritePacket) == sizeof(RenderTexturePagePacket) + sizeof(RenderSpritePacket),
                  render_paged_sprite_packet_size);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RenderPagedSpritePacket, sprite) == sizeof(RenderTexturePagePacket),
                  render_paged_sprite_packet_sprite_offset);

typedef union RenderTileTexture {
    u32 words[2];
    u8 bytes[8];
} RenderTileTexture;

PE1_STATIC_ASSERT(sizeof(RenderTilePacket) == ((sizeof(RenderNativeTag) + 28 + sizeof(void *) - 1) / sizeof(void *)) * sizeof(void *), render_tile_packet_size);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RenderTilePacket, clut) == sizeof(RenderNativeTag) + 26, render_tile_clut_offset);
PE1_STATIC_ASSERT(sizeof(RenderTexturePagePacket) == sizeof(RenderNativeTag) + sizeof(uintptr_t), render_texture_page_packet_size);
PE1_STATIC_ASSERT(sizeof(RenderTileTexture) == 8, render_tile_texture_size);

PE1_STATIC_ASSERT(sizeof(RenderLinePacket) == sizeof(RenderColorTilePacket), render_line_packet_size);
PE1_STATIC_ASSERT(sizeof(RenderColorTilePacket) == ((sizeof(RenderNativeTag) + 12 + _Alignof(RenderColorTilePacket) - 1) / _Alignof(RenderColorTilePacket)) * _Alignof(RenderColorTilePacket),
                  render_color_tile_packet_size);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RenderColorTilePacket, x0) == sizeof(RenderNativeTag) + 4,
                  render_color_tile_coordinates);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RenderColorTilePacket, w) == sizeof(RenderNativeTag) + 8,
                  render_color_tile_dimensions);
PE1_STATIC_ASSERT(sizeof(RenderDrawModePacket) == 3 * sizeof(uintptr_t),
                  render_draw_mode_packet_size);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RenderLinePacket, x0) == sizeof(RenderNativeTag) + 4,
                  render_line_packet_points);
PE1_STATIC_ASSERT(PE1_OFFSETOF(RenderBufferPrefix, packets) == 8 * sizeof(void *),
                  render_buffer_prefix_packets);
PE1_STATIC_ASSERT(sizeof(RenderBufferPrefix) == 10 * sizeof(void *),
                  render_buffer_prefix_size);
extern int g_DrawPacketOffset, g_ActiveDrawSlot;
void SetLineF2(RenderLinePacket *packet);
void FieldFx_LinkBlendedPacket(void *ordering, void *packet, int mode);
struct RenderColor;
void FieldFx_DrawDiamondOutline(GteShortVector *position, int x, int y, int angle,
                   int radius, int depth, int intensity, struct RenderColor *color);

#endif /* PE1_RENDER_PRIM_H */
