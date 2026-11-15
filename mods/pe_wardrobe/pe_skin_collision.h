#ifndef PE_SKIN_COLLISION_H
#define PE_SKIN_COLLISION_H

#include <stddef.h>
#include <stdint.h>

/* Native jacket Aya's movement cylinder in the supported US assets. NPC skin
 * cylinders range above and below this. This is NOT a per-part combat radius. */
#define PE_SKIN_PLAYER_MOVEMENT_RADIUS 63u

static uint32_t pe_skin_collision_u32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static unsigned pe_skin_collision_u16(const uint8_t *p) {
    return (unsigned)p[0] | (unsigned)p[1] << 8;
}

/* Skins made by pe_aya_skin.py before 2026-11-15 append their (bigger) model block at the end of
 * model.bin and point the root record at it, but keep Aya's own block length in the record. When
 * that length is too short for the block's own counts, set it to the rest of the file. Returns 1 if
 * the record was changed; malformed input is left alone (the check below rejects it). */
static inline int pe_skin_repair_appended_length(void *data, size_t size) {
    uint8_t *b = (uint8_t *)data;
    size_t dir, table, model, length, section;
    uint32_t roots, source, word;
    unsigned parts, verts, packets = 0, i;
    if (!b || size < 8) return 0;
    dir = pe_skin_collision_u32(b + 4);
    if (dir > size || size - dir < 0x34) return 0;
    roots = pe_skin_collision_u32(b + dir + 0x0c);
    table = roots & 0x3fffffu;
    if (!(roots >> 22) || table > size || size - table < 12) return 0;
    source = pe_skin_collision_u32(b + table + 4);
    if (source >> 24) return 0;
    model = source & 0xffffffu;
    if (model > size || size - model < 0x1c || size - model > 0xffffffu) return 0;
    word = pe_skin_collision_u32(b + table);
    length = word & 0xffffffu;
    parts = b[model + 2];
    verts = pe_skin_collision_u16(b + model + 6);
    for (i = 0; i < 4; ++i)
        packets += pe_skin_collision_u16(b + model + 8 + 2 * i);
    section = 0x1cu + 12u * parts + 12u * verts + 12u * packets;
    if (length >= section + 16 || size - model < section + 16) return 0;
    word = (word & 0xff000000u) | (uint32_t)(size - model);
    for (i = 0; i < 4; ++i) b[table + i] = (uint8_t)(word >> (8 * i));
    return 1;
}

/* Patch only the host-owned model copy, before Render_SetupEntityPrims reads
 * section14.pad into hit_cylinder.radius for field movement. On malformed
 * input fail without writes. Byte reads avoid alignment/aliasing assumptions. */
static int pe_skin_normalize_movement(void *data, size_t size) {
    uint8_t *b = (uint8_t *)data;
    size_t dir, table, model, length, section;
    uint32_t roots, source;
    unsigned parts, verts, packets = 0, i;
    if (!b || size < 8) return 0;
    dir = pe_skin_collision_u32(b + 4);
    if (dir > size || size - dir < 0x34) return 0;
    roots = pe_skin_collision_u32(b + dir + 0x0c);
    table = roots & 0x3fffffu;
    if (!(roots >> 22) || table > size || size - table < 12) return 0;
    source = pe_skin_collision_u32(b + table + 4);
    if (source >> 24) return 0; /* the loader expects player slot zero first */
    model = source & 0xffffffu;
    length = pe_skin_collision_u32(b + table) & 0xffffffu;
    if (model > size || length > size - model || length < 0x1c) return 0;
    parts = b[model + 2];
    verts = pe_skin_collision_u16(b + model + 6);
    if (!parts) return 0;
    for (i = 0; i < 4; ++i)
        packets += pe_skin_collision_u16(b + model + 8 + 2 * i);
    /* Header, part table, vertices, colours, primitive descriptors, section14. */
    section = 0x1cu + 12u * parts + 12u * verts + 12u * packets;
    if (section > length || length - section < 16) return 0;
    b[model + section + 6] = PE_SKIN_PLAYER_MOVEMENT_RADIUS;
    b[model + section + 7] = 0;
    return 1;
}

#endif
