#ifndef PE1_FIELD_RING_GEOMETRY_H
#define PE1_FIELD_RING_GEOMETRY_H

#include "common.h"
#include "pe1/gte_types.h"

/* Native runtime record used by room emitters and the field ring builder /
 * renderer. The room-side names describe the producer view; in the field
 * ring view, mode is the point count, extent0/1 are inner/outer radii, and
 * offset/intensity are depth/brightness. */
typedef struct FieldRingGeometry {
    /* 0x00 */ void *source;
    /* 0x04 */ u8 color0[3];
    /* 0x07 */ u8 pad07;
    /* 0x08 */ u8 color1[3];
    /* 0x0B */ u8 pad0B;
    /* 0x0C */ u16 mode;
    /* 0x0E */ u16 extent0;
    /* 0x10 */ u16 extent1;
    /* 0x12 */ s16 offset;
    /* 0x14 */ s16 intensity;
    /* 0x16 */ s16 pad16;
} FieldRingGeometry;

typedef FieldRingGeometry RoomFxEmitterParams;

PE1_STATIC_ASSERT(PE1_OFFSETOF(FieldRingGeometry, source) == 0,
                  field_ring_geometry_source_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(FieldRingGeometry, color0) == sizeof(void *) + 0,
                  field_ring_geometry_color0_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(FieldRingGeometry, color1) == sizeof(void *) + 4,
                  field_ring_geometry_color1_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(FieldRingGeometry, mode) == sizeof(void *) + 8,
                  field_ring_geometry_mode_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(FieldRingGeometry, extent0) == sizeof(void *) + 10,
                  field_ring_geometry_extent0_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(FieldRingGeometry, extent1) == sizeof(void *) + 12,
                  field_ring_geometry_extent1_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(FieldRingGeometry, offset) == sizeof(void *) + 14,
                  field_ring_geometry_offset_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(FieldRingGeometry, intensity) == sizeof(void *) + 16,
                  field_ring_geometry_intensity_offset);
PE1_STATIC_ASSERT(PE1_OFFSETOF(FieldRingGeometry, pad16) == sizeof(void *) + 18,
                  field_ring_geometry_pad16_offset);
PE1_STATIC_ASSERT(sizeof(FieldRingGeometry) == ((sizeof(void *) + 20 + _Alignof(void *) - 1) / _Alignof(void *)) * _Alignof(void *),
                  field_ring_geometry_size);

/* Field engine: builds and draws one ring through placement; mode 0
 * composes the camera rotation first. */
int Geo_ClipAlternatingArcPoint(int radius, int divisor, int segment);
void FieldFx_BuildRingGeometry(FieldRingGeometry *ring);
void FieldFx_DrawShadedRing(FieldRingGeometry *ring, GteMatrix *placement, u8 mode);

#endif /* PE1_FIELD_RING_GEOMETRY_H */
