/* Chase camera (experimental): a camera that orbits Aya, turned with the right stick or by
 * dragging with the right mouse button, the wheel for distance. The room's pre-rendered
 * background can't turn with it, so while it is on the background is hidden and the room's
 * walkable floor (its collision triangles) is drawn instead, with outlines where the walls are.
 * The stick then moves Aya relative to this camera.
 *
 * Hooks: FRAME (input, the camera base angle the movement code reads), FIELD_CAMERA (swap the
 * view matrix around the actor draw pass, draw the floor), FIELD_BACKGROUND (hide the sprites).
 * Battles draw through their own pass and keep the room camera. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

#include "pe_game.h"
#include "pe_mod_host.h"
#include "pe_mod_menu.h"
#include "pe_hooks.h"
#include "common.h"
#include "pe1/collision_database.h"
#include "pe1/field_actor.h"
#include "pe1/field_collision.h"
#include "pe1/game_state.h"
#include "pe1/geom_state.h"
#include "pe1/gte_types.h"
#include "pe1/player_entity.h"
#include "pe1/render_buffers.h"
#include "pe1/render_camera.h"
#include "pe1/render_packets.h"

#define MOD_ID "pe_chasecam"
#define PAGE "Chase camera"
#define PI_F 3.14159265f

/* ---- settings (F1 > Chase camera) ---- */
static int s_enabled;          /* F8 */
static int s_distance = 1600;  /* world units from the focus point */
static int s_focus = 600;      /* focus height above Aya's feet */
static int s_fov = 280;        /* projection distance: lower is wider */
static int s_sensitivity = 5;
static int s_invert_y;
static int s_walls = 1;
static int s_wall_height = 1400;

/* ---- camera state ---- */
static float s_yaw, s_pitch = 0.30f;
static int s_place_behind = 1;   /* next active frame: put the camera behind Aya */
static int s_active;             /* this frame swaps the camera */
static unsigned s_frame, s_drawn_frame;

/* Mouse deltas and wheel, gathered by an event watch between frames. */
static float s_mouse_dx, s_mouse_dy, s_wheel;
static int s_f8_down, s_home_down;

/* Room camera words saved around the actor pass. */
static u32 s_saved_scratch[9];
static u16 s_saved_offset_x, s_saved_offset_y;
static int s_swapped;
static float s_mirror = 1.0f;    /* -1 when the room's camera is a mirror (det < 0) */

/* The movement code's base angles while the chase camera owns them. */
static int s_angles_owned;
static u16 s_room_angle_digital, s_room_angle_analog, s_written_angle;

/* ---- floor mesh: the collision triangles, split until small enough to clip and draw ---- */
typedef struct { float x, y, z; } V3;
typedef struct { V3 v[3]; u8 shade; } FloorTri;
typedef struct { V3 a, b; } WallEdge;

#define MAX_FLOOR 12000
#define MAX_WALLS 4096
#define SPLIT_EDGE 450.0f
static FloorTri s_floor[MAX_FLOOR];
static int s_floor_count;
static WallEdge s_walls_list[MAX_WALLS];
static int s_wall_count;
static const void *s_mesh_key;
static unsigned s_mesh_tris;

/* ---- packets (two buffers, one per draw slot) ---- */
typedef struct {
    RenderNativeTag tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, p1;
    s16 x1, y1;
    u8 r2, g2, b2, p2;
    s16 x2, y2;
} PrimG3;
typedef struct {
    RenderNativeTag tag;
    u8 r, g, b, code;
    s16 x0, y0;
    s16 x1, y1;
} PrimLine;

#define MAX_TRI_PRIMS 16000
#define MAX_LINE_PRIMS 12000
static PrimG3 s_tri_prims[2][MAX_TRI_PRIMS];
static PrimLine s_line_prims[2][MAX_LINE_PRIMS];
static int s_tri_used, s_line_used;

/* ---- camera basis for this frame ---- */
static float s_rx[3], s_ry[3], s_rz[3], s_cam[3], s_h;

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static void push_floor(const V3 *a, const V3 *b, const V3 *c, u8 shade, int depth) {
    if (s_floor_count >= MAX_FLOOR) return;
    float ab = (b->x - a->x) * (b->x - a->x) + (b->z - a->z) * (b->z - a->z);
    float bc = (c->x - b->x) * (c->x - b->x) + (c->z - b->z) * (c->z - b->z);
    float ca = (a->x - c->x) * (a->x - c->x) + (a->z - c->z) * (a->z - c->z);
    float longest = ab > bc ? (ab > ca ? ab : ca) : (bc > ca ? bc : ca);
    if (longest > SPLIT_EDGE * SPLIT_EDGE && depth < 5) {
        V3 m_ab = {(a->x + b->x) / 2, (a->y + b->y) / 2, (a->z + b->z) / 2};
        V3 m_bc = {(b->x + c->x) / 2, (b->y + c->y) / 2, (b->z + c->z) / 2};
        V3 m_ca = {(c->x + a->x) / 2, (c->y + a->y) / 2, (c->z + a->z) / 2};
        push_floor(a, &m_ab, &m_ca, shade, depth + 1);
        push_floor(&m_ab, b, &m_bc, shade, depth + 1);
        push_floor(&m_ca, &m_bc, c, shade, depth + 1);
        push_floor(&m_ab, &m_bc, &m_ca, shade, depth + 1);
        return;
    }
    FloorTri *t = &s_floor[s_floor_count++];
    t->v[0] = *a;
    t->v[1] = *b;
    t->v[2] = *c;
    t->shade = shade;
}

/* Rebuilds the floor when the room's collision data changes. */
static void build_mesh(void) {
    const CollisionDatabase *db = g_CollisionDb;
    if (!db || !db->triangles.pointer || !db->vertices.pointer) {
        s_floor_count = s_wall_count = 0;
        s_mesh_key = NULL;
        return;
    }
    if (s_mesh_key == db->triangles.pointer && s_mesh_tris == db->triangleCount) return;
    s_mesh_key = db->triangles.pointer;
    s_mesh_tris = db->triangleCount;
    s_floor_count = s_wall_count = 0;

    const int sloped = g_CollisionPlaneTable != NULL;
    float minx = 1e9f, maxx = -1e9f, minz = 1e9f, maxz = -1e9f;
    for (unsigned i = 0; i < db->triangleCount; ++i) {
        const CollisionTriangleLinks *links;
        u8 kind, region;
        if (sloped) {
            links = &db->triangles.xyz[i].layout.links;
            kind = db->triangles.xyz[i].kind;
            region = db->triangles.xyz[i].header.region;
        } else {
            links = &db->triangles.xz[i].layout.links;
            kind = db->triangles.xz[i].kind;
            region = db->triangles.xz[i].header.region;
        }
        V3 v[3];
        for (int k = 0; k < 3; ++k) {
            u16 index = links->vertices[k];
            if (sloped) {
                const CollisionVertexXYZ *p = &db->vertices.xyz[index];
                v[k].x = (s16)p->x;
                v[k].y = (s16)p->y;
                v[k].z = (s16)p->z;
            } else {
                const CollisionVertexXZ *p = &db->vertices.xz[index];
                v[k].x = (s16)p->x;
                v[k].z = (s16)p->z;
                v[k].y = 0;
                if (region < db->regionCount && db->regions[region].flat)
                    v[k].y = db->regions[region].flat->height;
            }
            if (v[k].x < minx) minx = v[k].x;
            if (v[k].x > maxx) maxx = v[k].x;
            if (v[k].z < minz) minz = v[k].z;
            if (v[k].z > maxz) maxz = v[k].z;
        }
        (void)kind;
        push_floor(&v[0], &v[1], &v[2], (u8)(i & 1), 0);
        /* Edge `slot` runs from vertex slot-1 to vertex slot; no neighbour (or one that can't
         * be entered) is a wall. */
        for (int slot = 0; slot < 3 && s_wall_count < MAX_WALLS; ++slot) {
            u16 n = links->neighbours[slot];
            int wall = n == 0xFFFF;
            if (!wall && n < db->triangleCount)
                wall = sloped ? (db->triangles.xyz[n].kind & 0x80) : (db->triangles.xz[n].kind & 0x80);
            if (!wall) continue;
            s_walls_list[s_wall_count].a = v[(slot + 2) % 3];
            s_walls_list[s_wall_count].b = v[slot];
            s_wall_count++;
        }
    }
    pe_mod_log(MOD_ID, "floor: %u triangles (%s) -> %d drawn, %d wall edges, x %.0f..%.0f z %.0f..%.0f",
               db->triangleCount, sloped ? "sloped" : "flat", s_floor_count, s_wall_count,
               minx, maxx, minz, maxz);
}

/* ---- projection ---- */
static void to_view(const V3 *p, float out[3]) {
    float dx = p->x - s_cam[0], dy = p->y - s_cam[1], dz = p->z - s_cam[2];
    out[0] = s_rx[0] * dx + s_rx[1] * dy + s_rx[2] * dz;
    out[1] = s_ry[0] * dx + s_ry[1] * dy + s_ry[2] * dz;
    out[2] = s_rz[0] * dx + s_rz[1] * dy + s_rz[2] * dz;
}

#define NEAR_Z 40.0f
#define FAR_Z 16000.0f

static int project(const float v[3], s16 *x, s16 *y) {
    float sx = 160.0f + s_h * v[0] / v[2];
    float sy = 112.0f + s_h * v[1] / v[2];
    if (sx < -1000 || sx > 1320 || sy < -1000 || sy > 1224) return 0;
    *x = (s16)lrintf(sx);
    *y = (s16)lrintf(sy);
    return 1;
}

static u8 fog(float z, int base) {
    float k = clampf(1.15f - z / 10000.0f, 0.2f, 1.0f);
    return (u8)(base * k);
}

static int ot_depth(float z) {
    int d = (int)(z / 4.0f);
    return d < 1 ? 1 : d > 0xFFE ? 0xFFE : d;
}

static void link_prim(RenderNativeTag *tag, int depth) {
    RenderNativeTag *entry = &D_800B0E38.ordering[g_ActiveDrawSlot][depth];
    tag->address = entry->address;
    entry->address = (uintptr_t)tag;
}

static void emit_tri(const float a[3], const float b[3], const float c[3], int base_r, int base_g,
                     int base_b, int depth) {
    if (s_tri_used >= MAX_TRI_PRIMS) return;
    PrimG3 *p = &s_tri_prims[g_ActiveDrawSlot][s_tri_used];
    if (!project(a, &p->x0, &p->y0) || !project(b, &p->x1, &p->y1) || !project(c, &p->x2, &p->y2))
        return;
    int minx = p->x0, maxx = p->x0, miny = p->y0, maxy = p->y0;
    if (p->x1 < minx) minx = p->x1; if (p->x1 > maxx) maxx = p->x1;
    if (p->x2 < minx) minx = p->x2; if (p->x2 > maxx) maxx = p->x2;
    if (p->y1 < miny) miny = p->y1; if (p->y1 > maxy) maxy = p->y1;
    if (p->y2 < miny) miny = p->y2; if (p->y2 > maxy) maxy = p->y2;
    if (maxx < 0 || minx > 320 || maxy < 0 || miny > 224) return;
    if (maxx - minx > 1023 || maxy - miny > 511) return; /* the GPU drops these */
    s_tri_used++;
    p->tag.length = 6;
    p->code = 0x30;
    p->r0 = fog(a[2], base_r); p->g0 = fog(a[2], base_g); p->b0 = fog(a[2], base_b);
    p->r1 = fog(b[2], base_r); p->g1 = fog(b[2], base_g); p->b1 = fog(b[2], base_b);
    p->r2 = fog(c[2], base_r); p->g2 = fog(c[2], base_g); p->b2 = fog(c[2], base_b);
    p->p1 = p->p2 = 0;
    link_prim(&p->tag, depth);
}

/* Clips a view-space triangle to the near plane and draws what is left. */
static void draw_floor_tri(const FloorTri *t) {
    float in[3][3], poly[4][3];
    int n = 0;
    float maxz = 0;
    for (int k = 0; k < 3; ++k) to_view(&t->v[k], in[k]);
    if (in[0][2] < NEAR_Z && in[1][2] < NEAR_Z && in[2][2] < NEAR_Z) return;
    if (in[0][2] > FAR_Z && in[1][2] > FAR_Z && in[2][2] > FAR_Z) return;
    for (int k = 0; k < 3; ++k) {
        const float *a = in[k], *b = in[(k + 1) % 3];
        int a_in = a[2] >= NEAR_Z, b_in = b[2] >= NEAR_Z;
        if (a_in) memcpy(poly[n++], a, sizeof poly[0]);
        if (a_in != b_in) {
            float f = (NEAR_Z - a[2]) / (b[2] - a[2]);
            poly[n][0] = a[0] + (b[0] - a[0]) * f;
            poly[n][1] = a[1] + (b[1] - a[1]) * f;
            poly[n][2] = NEAR_Z;
            n++;
        }
    }
    for (int k = 0; k < n; ++k) if (poly[k][2] > maxz) maxz = poly[k][2];
    /* Behind everything standing on it: the floor sorts past the actors. */
    int depth = 0x800 + (int)(maxz / 8.0f);
    if (depth > 0xFFE) depth = 0xFFE;
    int r = t->shade ? 92 : 78, g = t->shade ? 98 : 84, b = t->shade ? 112 : 98;
    if (n >= 3) emit_tri(poly[0], poly[1], poly[2], r, g, b, depth);
    if (n == 4) emit_tri(poly[0], poly[2], poly[3], r, g, b, depth);
}

static void draw_line(const V3 *pa, const V3 *pb, int r, int g, int b) {
    if (s_line_used >= MAX_LINE_PRIMS) return;
    float a[3], c[3];
    to_view(pa, a);
    to_view(pb, c);
    if (a[2] < NEAR_Z && c[2] < NEAR_Z) return;
    if (a[2] > FAR_Z && c[2] > FAR_Z) return;
    if (a[2] < NEAR_Z || c[2] < NEAR_Z) {
        float *inside = a[2] < NEAR_Z ? c : a, *outside = a[2] < NEAR_Z ? a : c;
        float f = (NEAR_Z - inside[2]) / (outside[2] - inside[2]);
        outside[0] = inside[0] + (outside[0] - inside[0]) * f;
        outside[1] = inside[1] + (outside[1] - inside[1]) * f;
        outside[2] = NEAR_Z;
    }
    PrimLine *p = &s_line_prims[g_ActiveDrawSlot][s_line_used];
    if (!project(a, &p->x0, &p->y0) || !project(c, &p->x1, &p->y1)) return;
    if (abs(p->x1 - p->x0) > 1023 || abs(p->y1 - p->y0) > 511) return;
    s_line_used++;
    float z = (a[2] + c[2]) * 0.5f;
    p->tag.length = 3;
    p->code = 0x40;
    p->r = fog(z, r);
    p->g = fog(z, g);
    p->b = fog(z, b);
    link_prim(&p->tag, ot_depth(z));
}

static void draw_room(void) {
    s_tri_used = s_line_used = 0;
    for (int i = 0; i < s_floor_count; ++i) draw_floor_tri(&s_floor[i]);
    if (!s_walls) return;
    for (int i = 0; i < s_wall_count; ++i) {
        const WallEdge *w = &s_walls_list[i];
        V3 ta = {w->a.x, w->a.y - s_wall_height, w->a.z};
        V3 tb = {w->b.x, w->b.y - s_wall_height, w->b.z};
        draw_line(&w->a, &w->b, 120, 210, 230);
        draw_line(&ta, &tb, 60, 110, 130);
        draw_line(&w->a, &ta, 60, 110, 130);
    }
}

/* ---- camera ---- */
static float aya_yaw_behind(void) {
    /* Facing a points along (-sin a, -cos a); the camera looks along (sin yaw, cos yaw). */
    float a = (float)(g_PlayerEntity->rot_y & 0xFFF) * (2.0f * PI_F / 4096.0f);
    return a + PI_F;
}

static void build_camera(void) {
    const FieldActor *aya = g_PlayerEntity;
    float tx = (float)(aya->pos_x >> 16);
    float ty = (float)(aya->pos_y >> 16) - (float)s_focus;
    float tz = (float)(aya->pos_z >> 16);
    float cp = cosf(s_pitch), sp = sinf(s_pitch);
    float f[3] = {sinf(s_yaw) * cp, sp, cosf(s_yaw) * cp};
    /* Y points down in the world, as on screen: right = down x forward, down = forward x right. */
    float right[3] = {f[2], 0.0f, -f[0]};
    float len = sqrtf(right[0] * right[0] + right[2] * right[2]);
    right[0] /= len;
    right[2] /= len;
    float down[3] = {f[1] * right[2] - f[2] * right[1], f[2] * right[0] - f[0] * right[2],
                     f[0] * right[1] - f[1] * right[0]};
    for (int i = 0; i < 3; ++i) {
        s_rx[i] = right[i] * s_mirror;
        s_ry[i] = down[i];
        s_rz[i] = f[i];
    }
    s_cam[0] = tx - f[0] * (float)s_distance;
    s_cam[1] = ty - f[1] * (float)s_distance;
    s_cam[2] = tz - f[2] * (float)s_distance;
    s_h = (float)s_fov;

    GteMatrix m;
    memset(&m, 0, sizeof m);
    for (int c = 0; c < 3; ++c) {
        m.m[0][c] = (s16)lrintf(s_rx[c] * 4096.0f);
        m.m[1][c] = (s16)lrintf(s_ry[c] * 4096.0f);
        m.m[2][c] = (s16)lrintf(s_rz[c] * 4096.0f);
    }
    for (int r = 0; r < 3; ++r) {
        const float *row = r == 0 ? s_rx : r == 1 ? s_ry : s_rz;
        m.t[r] = (s32)lrintf(-(row[0] * s_cam[0] + row[1] * s_cam[1] + row[2] * s_cam[2]));
    }
    memcpy(g_EntityRenderScratch, &m, sizeof m);
    g_EntityRenderScratch[8] = (u32)s_fov;
}

static int room_camera_mirrored(void) {
    const GteMatrix *m = (const GteMatrix *)s_saved_scratch;
    double a = m->m[0][0], b = m->m[0][1], c = m->m[0][2];
    double d = m->m[1][0], e = m->m[1][1], f = m->m[1][2];
    double g = m->m[2][0], h = m->m[2][1], i = m->m[2][2];
    double det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    return det < 0;
}

static int can_run(void) {
    return s_enabled && pe_game_loaded() && g_PlayerEntity && g_CollisionDb;
}

/* ---- hooks ---- */
static bool SDLCALL on_event(void *user, SDL_Event *ev) {
    (void)user;
    if (ev->type == SDL_EVENT_MOUSE_MOTION && (ev->motion.state & SDL_BUTTON_RMASK)) {
        s_mouse_dx += ev->motion.xrel;
        s_mouse_dy += ev->motion.yrel;
    } else if (ev->type == SDL_EVENT_MOUSE_WHEEL) {
        s_wheel += ev->wheel.y;
    } else if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev->button.button == SDL_BUTTON_MIDDLE) {
        s_place_behind = 1;
    }
    return true;
}

static void read_stick(float *x, float *y) {
    int count = 0;
    *x = *y = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    if (!ids) return;
    for (int i = 0; i < count; ++i) {
        SDL_Gamepad *pad = SDL_GetGamepadFromID(ids[i]);
        if (!pad) continue;
        int ax = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX);
        int ay = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY);
        if (abs(ax) < 7000) ax = 0;
        if (abs(ay) < 7000) ay = 0;
        if (ax || ay) {
            *x = ax / 32767.0f;
            *y = ay / 32767.0f;
            break;
        }
    }
    SDL_free(ids);
}

static void read_keys(void) {
    const bool *keys = SDL_GetKeyboardState(NULL);
    if (!keys) return;
    int f8 = keys[SDL_SCANCODE_F8], home = keys[SDL_SCANCODE_HOME];
    if (f8 && !s_f8_down) {
        s_enabled = !s_enabled;
        s_place_behind = 1;
        pe_toast(s_enabled ? "Chase camera on" : "Chase camera off");
        pe_menu_save_settings();
    }
    if (home && !s_home_down) s_place_behind = 1;
    s_f8_down = f8;
    s_home_down = home;
}

static void release_angles(void) {
    if (!s_angles_owned) return;
    if (g_GeomScroll.state.position.baseAngleDigital == s_written_angle)
        g_GeomScroll.state.position.baseAngleDigital = s_room_angle_digital;
    if (g_GeomScroll.state.position.baseAngleAnalog == s_written_angle)
        g_GeomScroll.state.position.baseAngleAnalog = s_room_angle_analog;
    s_angles_owned = 0;
}

/* Up on the stick walks away from the camera. */
static void own_angles(void) {
    GeomScrollCoordinates *pos = &g_GeomScroll.state.position;
    if (!s_angles_owned || pos->baseAngleDigital != s_written_angle ||
        pos->baseAngleAnalog != s_written_angle) {
        /* First frame, or a script set the room's angle: keep it for when the camera goes off. */
        if (!s_angles_owned || pos->baseAngleDigital != s_written_angle)
            s_room_angle_digital = pos->baseAngleDigital;
        if (!s_angles_owned || pos->baseAngleAnalog != s_written_angle)
            s_room_angle_analog = pos->baseAngleAnalog;
        s_angles_owned = 1;
    }
    int angle = (int)lrintf((s_yaw - PI_F) * 4096.0f / (2.0f * PI_F)) * (s_mirror < 0 ? -1 : 1);
    s_written_angle = (u16)(angle & 0xFFF);
    pos->baseAngleDigital = s_written_angle;
    pos->baseAngleAnalog = s_written_angle;
}

static int on_frame(void *ctx, void *user) {
    (void)ctx;
    (void)user;
    s_frame++;
    read_keys();
    float mdx = s_mouse_dx, mdy = s_mouse_dy, wheel = s_wheel;
    s_mouse_dx = s_mouse_dy = s_wheel = 0;
    if (!can_run()) {
        release_angles();
        return 0;
    }
    if (s_place_behind) {
        s_yaw = aya_yaw_behind();
        s_pitch = 0.30f;
        s_place_behind = 0;
    }
    float sx, sy;
    read_stick(&sx, &sy);
    float speed = 0.012f * (float)s_sensitivity;
    float inv = s_invert_y ? -1.0f : 1.0f;
    s_yaw += sx * speed + mdx * 0.0012f * (float)s_sensitivity;
    s_pitch += (sy * speed * 0.7f + mdy * 0.0012f * (float)s_sensitivity) * inv;
    s_pitch = clampf(s_pitch, -0.35f, 1.35f);
    if (s_yaw > PI_F) s_yaw -= 2 * PI_F;
    if (s_yaw < -PI_F) s_yaw += 2 * PI_F;
    if (wheel != 0) {
        s_distance -= (int)(wheel * 150.0f);
        if (s_distance < 400) s_distance = 400;
        if (s_distance > 6000) s_distance = 6000;
    }
    own_angles();
    return 0;
}

static int on_camera(void *ctx, void *user) {
    (void)user;
    const PeHookFieldCamera *cam = ctx;
    if (cam->phase == 0) {
        s_swapped = 0;
        if (!can_run()) return 0;
        build_mesh();
        memcpy(s_saved_scratch, g_EntityRenderScratch, sizeof s_saved_scratch);
        s_saved_offset_x = g_GeomScroll.state.position.screenOffsetX;
        s_saved_offset_y = g_GeomScroll.state.position.screenOffsetY;
        float mirror = room_camera_mirrored() ? -1.0f : 1.0f;
        if (mirror != s_mirror) {
            s_mirror = mirror;
            pe_mod_log(MOD_ID, "room camera is %s", mirror < 0 ? "mirrored" : "a plain rotation");
        }
        build_camera();
        g_GeomScroll.state.position.screenOffsetX = 160;
        g_GeomScroll.state.position.screenOffsetY = 112;
        SetGeomScreen(s_fov);
        draw_room();
        s_swapped = 1;
        s_drawn_frame = s_frame;
    } else if (s_swapped) {
        memcpy(g_EntityRenderScratch, s_saved_scratch, sizeof s_saved_scratch);
        g_GeomScroll.state.position.screenOffsetX = s_saved_offset_x;
        g_GeomScroll.state.position.screenOffsetY = s_saved_offset_y;
        SetGeomScreen((long)s_saved_scratch[8]);
        s_swapped = 0;
    }
    return 0;
}

/* The background stays hidden only while the field pass draws the floor in its place. */
static int on_background(void *ctx, void *user) {
    (void)ctx;
    (void)user;
    return can_run() && s_frame - s_drawn_frame <= 2;
}

static void place_behind(void) { s_place_behind = 1; }

static int chasecam_init(void) {
    pe_hook_add(PE_HOOK_FRAME, on_frame, NULL, PE_PRIORITY_NORMAL, MOD_ID);
    pe_hook_add(PE_HOOK_FIELD_CAMERA, on_camera, NULL, PE_PRIORITY_NORMAL, MOD_ID);
    pe_hook_add(PE_HOOK_FIELD_BACKGROUND, on_background, NULL, PE_PRIORITY_NORMAL, MOD_ID);
    SDL_AddEventWatch(on_event, NULL);

    pe_menu_begin(PAGE);
    pe_menu_top_page(PAGE, "Experimental camera behind Aya over the room's walkable floor.", 45);
    pe_menu_register_toggle(0, "Chase camera (F8)",
                            "Camera behind Aya; the background is replaced by the walkable floor.",
                            &s_enabled, NULL);
    pe_menu_register_action(0, "Behind Aya (Home)", "Put the camera back behind Aya.", place_behind);
    pe_menu_register_int(0, "Distance", "How far the camera stays (mouse wheel).", &s_distance,
                         400, 6000, 100);
    pe_menu_register_int(0, "Focus height", "The point the camera looks at, above Aya's feet.",
                         &s_focus, 0, 2000, 50);
    pe_menu_register_int(0, "Field of view", "Projection distance: lower is wider.", &s_fov,
                         120, 600, 20);
    pe_menu_register_int(0, "Turn speed", "Right stick and right-mouse drag.", &s_sensitivity,
                         1, 15, 1);
    pe_menu_register_toggle(0, "Invert vertical", "Up on the stick or mouse looks down.",
                            &s_invert_y, NULL);
    pe_menu_register_toggle(0, "Wall outlines", "Lines where the walkable floor ends.",
                            &s_walls, NULL);
    pe_menu_register_int(0, "Wall height", "Height of the wall outlines.", &s_wall_height, 200,
                         4000, 100);
    pe_menu_begin(0);
    return 0;
}

static void chasecam_shutdown(void) {
    SDL_RemoveEventWatch(on_event, NULL);
    release_angles();
}

const PeMod pe_mod_pe_chasecam = {MOD_ID, "Chase camera", "0.1", NULL, chasecam_init,
                                  chasecam_shutdown};
