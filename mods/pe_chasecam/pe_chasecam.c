/* Chase camera (experimental): a camera that orbits Aya, turned with the right stick or by
 * dragging with the right mouse button, the wheel for distance. The room's pre-rendered
 * background can't turn with it, so while it is on the background sprites are skipped and the
 * room is rebuilt from its collision data: the walkable floor, and walls standing on the edges
 * where the floor ends. Both are textured by projecting them through the room's own camera onto
 * its background tiles (the picture the player normally sees), so each piece shows what was
 * painted there; what the room camera can't see stays flat-shaded. The stick moves Aya
 * relative to this camera, which stops short of walls.
 *
 * Hooks: FRAME (input, the camera base angle the movement code reads), FIELD_CAMERA (swap the
 * view matrix around the actor draw pass, draw the room), FIELD_BACKGROUND (skip the sprites,
 * read where they would have gone). Battles draw through their own pass and keep the room
 * camera. */
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
#include "pe1/render_prim.h"

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
static int s_textures = 1;
static int s_solid_walls = 1;
static int s_outlines;
static int s_wall_height = 1400;
static int s_collide = 1;

/* ---- camera state ---- */
static float s_yaw, s_pitch = 0.30f;
static int s_place_behind = 1;   /* next active frame: put the camera behind Aya */
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

/* ---- room surfaces: floor triangles and wall quads (as triangles), in world units ---- */
typedef struct { float x, y, z; } V3;
typedef struct { V3 v[3]; u8 shade, wall; } Surface;
typedef struct { V3 a, b; } WallEdge;

#define MAX_SURFACES 16000
#define MAX_WALLS 4096
#define SPLIT_EDGE 450.0f
#define WALL_STEP 400.0f
static Surface s_surf[MAX_SURFACES];
static int s_surf_count, s_floor_surf_count;
static WallEdge s_walls_list[MAX_WALLS];
static int s_wall_count;
static const void *s_mesh_key;
static unsigned s_mesh_tris;
static int s_mesh_wall_height;

/* ---- background tiles this frame, where Render_DrawSpriteEntry would have put them ---- */
typedef struct {
    s16 x, y;      /* screen position of the 16x16 tile */
    u8 u, v;
    u16 clut, tpage;
    u8 code, r, g, b;
    u16 ot;        /* the tile's ordering-table entry: lower is nearer */
} Tile;
#define MAX_TILES 6144
static Tile s_tiles[MAX_TILES];
static int s_tile_count;
static unsigned s_tile_hash, s_tile_frame;

/* Screen cells (16 px) covering a margin around the frame, each listing its tiles. */
#define CELL_X0 (-96)
#define CELL_Y0 (-96)
#define CELLS_W 32
#define CELLS_H 26
#define MAX_CELL_ITEMS (MAX_TILES * 4)
static int s_cell_start[CELLS_W * CELLS_H + 1];
static u16 s_cell_items[MAX_CELL_ITEMS];
static unsigned s_tile_stamp[MAX_TILES], s_stamp;

/* ---- textured pieces: a surface cut along one tile, with that tile's texture coordinates ---- */
typedef struct {
    V3 p[3];
    u8 uv[3][2];
    u16 tile;
    u8 wall;
} Piece;
#define MAX_PIECES 40000
static Piece s_pieces[MAX_PIECES];
static int s_piece_count;
static unsigned s_piece_key;

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
    u8 u0, v0;
    u16 clut;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad;
} PrimFT3;
typedef struct {
    RenderNativeTag tag;
    u8 r, g, b, code;
    s16 x0, y0;
    s16 x1, y1;
} PrimLine;

#define MAX_TRI_PRIMS 24000
#define MAX_TEX_PRIMS 60000
#define MAX_LINE_PRIMS 12000
static PrimG3 s_tri_prims[2][MAX_TRI_PRIMS];
static PrimFT3 s_tex_prims[2][MAX_TEX_PRIMS];
static PrimLine s_line_prims[2][MAX_LINE_PRIMS];
static int s_tri_used, s_tex_used, s_line_used;

/* ---- chase camera basis for this frame ---- */
static float s_rx[3], s_ry[3], s_rz[3], s_cam[3], s_h;
static float s_cur_distance = 1600.0f;

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ================= room mesh ================= */

static void push_surface(const V3 *a, const V3 *b, const V3 *c, u8 shade, u8 wall, int depth) {
    if (s_surf_count >= MAX_SURFACES) return;
    if (!wall) {
        float ab = (b->x - a->x) * (b->x - a->x) + (b->z - a->z) * (b->z - a->z);
        float bc = (c->x - b->x) * (c->x - b->x) + (c->z - b->z) * (c->z - b->z);
        float ca = (a->x - c->x) * (a->x - c->x) + (a->z - c->z) * (a->z - c->z);
        float longest = ab > bc ? (ab > ca ? ab : ca) : (bc > ca ? bc : ca);
        if (longest > SPLIT_EDGE * SPLIT_EDGE && depth < 5) {
            V3 m_ab = {(a->x + b->x) / 2, (a->y + b->y) / 2, (a->z + b->z) / 2};
            V3 m_bc = {(b->x + c->x) / 2, (b->y + c->y) / 2, (b->z + c->z) / 2};
            V3 m_ca = {(c->x + a->x) / 2, (c->y + a->y) / 2, (c->z + a->z) / 2};
            push_surface(a, &m_ab, &m_ca, shade, 0, depth + 1);
            push_surface(&m_ab, b, &m_bc, shade, 0, depth + 1);
            push_surface(&m_ca, &m_bc, c, shade, 0, depth + 1);
            push_surface(&m_ab, &m_bc, &m_ca, shade, 0, depth + 1);
            return;
        }
    }
    Surface *s = &s_surf[s_surf_count++];
    s->v[0] = *a;
    s->v[1] = *b;
    s->v[2] = *c;
    s->shade = shade;
    s->wall = wall;
}

/* A wall on edge a-b, split into a grid of quads small enough to texture and clip. */
static void push_wall(const WallEdge *w, int index) {
    float dx = w->b.x - w->a.x, dz = w->b.z - w->a.z;
    float len = sqrtf(dx * dx + dz * dz);
    int cols = (int)ceilf(len / WALL_STEP), rows = (int)ceilf((float)s_wall_height / WALL_STEP);
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    for (int c = 0; c < cols; ++c) {
        float f0 = (float)c / cols, f1 = (float)(c + 1) / cols;
        V3 b0 = {w->a.x + dx * f0, w->a.y + (w->b.y - w->a.y) * f0, w->a.z + dz * f0};
        V3 b1 = {w->a.x + dx * f1, w->a.y + (w->b.y - w->a.y) * f1, w->a.z + dz * f1};
        for (int r = 0; r < rows; ++r) {
            float h0 = (float)s_wall_height * r / rows, h1 = (float)s_wall_height * (r + 1) / rows;
            V3 p00 = {b0.x, b0.y - h0, b0.z}, p10 = {b1.x, b1.y - h0, b1.z};
            V3 p01 = {b0.x, b0.y - h1, b0.z}, p11 = {b1.x, b1.y - h1, b1.z};
            push_surface(&p00, &p10, &p11, (u8)((index + c) & 1), 1, 0);
            push_surface(&p00, &p11, &p01, (u8)((index + c) & 1), 1, 0);
        }
    }
}

/* Rebuilds the floor and walls when the room's collision data changes. */
static void build_mesh(void) {
    const CollisionDatabase *db = g_CollisionDb;
    if (!db || !db->triangles.pointer || !db->vertices.pointer) {
        s_surf_count = s_floor_surf_count = s_wall_count = 0;
        s_mesh_key = NULL;
        return;
    }
    if (s_mesh_key == db->triangles.pointer && s_mesh_tris == db->triangleCount &&
        s_mesh_wall_height == s_wall_height)
        return;
    s_mesh_key = db->triangles.pointer;
    s_mesh_tris = db->triangleCount;
    s_mesh_wall_height = s_wall_height;
    s_surf_count = s_wall_count = 0;
    s_piece_key = 0;

    const int sloped = g_CollisionPlaneTable != NULL;
    float minx = 1e9f, maxx = -1e9f, minz = 1e9f, maxz = -1e9f;
    for (unsigned i = 0; i < db->triangleCount; ++i) {
        const CollisionTriangleLinks *links;
        u8 region;
        if (sloped) {
            links = &db->triangles.xyz[i].layout.links;
            region = db->triangles.xyz[i].header.region;
        } else {
            links = &db->triangles.xz[i].layout.links;
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
        push_surface(&v[0], &v[1], &v[2], (u8)(i & 1), 0, 0);
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
    s_floor_surf_count = s_surf_count;
    for (int i = 0; i < s_wall_count; ++i) push_wall(&s_walls_list[i], i);
    pe_mod_log(MOD_ID, "room %d: %u triangles (%s), %d wall edges -> %d floor + %d wall surfaces, "
               "x %.0f..%.0f z %.0f..%.0f", pe_room_number(), db->triangleCount,
               sloped ? "sloped" : "flat", s_wall_count, s_floor_surf_count,
               s_surf_count - s_floor_surf_count, minx, maxx, minz, maxz);
}

/* ================= background tiles ================= */

static unsigned hash_add(unsigned h, unsigned v) { return (h ^ v) * 16777619u; }

/* The sprites Render_BuildMeshList would draw this frame, with Render_DrawSpriteEntry's
 * placement (static layers; scrolling, wrapped layers are left out). */
static void gather_tiles(void) {
    GeomState *state = g_GeomState;
    s_tile_count = 0;
    s_tile_frame = s_frame;
    if (!state) return;
    u16 out_x = (u16)(state->disp_src_x - ((u16)g_GeomScroll.state.position.x - 160));
    u16 out_y = (u16)(state->disp_src_y - ((u16)g_GeomScroll.state.position.y - 112));
    GeomEntry *entries = (GeomEntry *)((u8 *)state + state->entry_offset);
    unsigned h = 2166136261u;
    for (unsigned e = 0; e < state->entry_count06; ++e) {
        GeomEntry *entry = &entries[e];
        if (!(entry->head.b.flags & 2) || entry->group != g_GeomScroll.state.position.groupSelect)
            continue;
        if (entry->head.b.flags & 4) continue;
        const RenderTilePacket *sprites = (const RenderTilePacket *)entry->u30.prim;
        const RenderTexturePagePacket *pages = entry->u34.pagePackets;
        if (!sprites || !pages) continue;
        const u32 *pos = (const u32 *)((const u8 *)entry + entry->u28.pos_ptr);
        int raw_x = (s16)(u16)(entry->scr_x + out_x);
        int raw_y = (s16)(u16)(entry->scr_y + out_y);
        int base_ot = state->field26 + (int)((*(const u32 *)entry >> 8) & 0xFFF);
        for (unsigned i = 0; i < entry->prim_count && s_tile_count < MAX_TILES; ++i) {
            u32 word = pos[i];
            int x = (s16)(raw_x + (int)(word >> 22));
            int y = (s16)(raw_y + (int)((word >> 12) & 0x3FF));
            int ot = base_ot + (int)(word & 0xFFF);
            if (((ot - 8) & 0xFFFF) >= 0xFF1) continue;
            if (x < CELL_X0 || y < CELL_Y0 || x >= CELL_X0 + CELLS_W * 16 - 16 ||
                y >= CELL_Y0 + CELLS_H * 16 - 16)
                continue;
            Tile *t = &s_tiles[s_tile_count++];
            t->x = (s16)x;
            t->y = (s16)y;
            t->u = sprites[i].u;
            t->v = sprites[i].v;
            t->clut = sprites[i].clut;
            t->tpage = (u16)(pages[i].command & 0x1FF);
            t->code = sprites[i].code;
            t->r = sprites[i].r;
            t->g = sprites[i].g;
            t->b = sprites[i].b;
            t->ot = (u16)ot;
            h = hash_add(h, (unsigned)x | ((unsigned)y << 16));
            h = hash_add(h, t->u | (t->v << 8) | ((unsigned)t->clut << 16));
            h = hash_add(h, t->tpage | ((unsigned)t->ot << 16));
        }
    }
    s_tile_hash = h;
}

static void bin_tiles(void) {
    static int counts[CELLS_W * CELLS_H];
    memset(counts, 0, sizeof counts);
    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 1) {
            int sum = 0;
            for (int c = 0; c < CELLS_W * CELLS_H; ++c) {
                s_cell_start[c] = sum;
                sum += counts[c];
                counts[c] = 0;
            }
            s_cell_start[CELLS_W * CELLS_H] = sum;
        }
        for (int i = 0; i < s_tile_count; ++i) {
            int cx0 = (s_tiles[i].x - CELL_X0) >> 4, cy0 = (s_tiles[i].y - CELL_Y0) >> 4;
            int cx1 = (s_tiles[i].x + 15 - CELL_X0) >> 4, cy1 = (s_tiles[i].y + 15 - CELL_Y0) >> 4;
            for (int cy = cy0; cy <= cy1; ++cy)
                for (int cx = cx0; cx <= cx1; ++cx) {
                    if (cx < 0 || cy < 0 || cx >= CELLS_W || cy >= CELLS_H) continue;
                    int c = cy * CELLS_W + cx;
                    if (pass == 1 && s_cell_start[c] + counts[c] < MAX_CELL_ITEMS)
                        s_cell_items[s_cell_start[c] + counts[c]] = (u16)i;
                    counts[c]++;
                }
        }
    }
}

/* ================= projection through the room camera ================= */

/* A clip vertex: room screen position, 1/z, and world position / z (both linear on screen). */
typedef struct { float sx, sy, q, wx, wy, wz; } CV;

static void room_view(const V3 *p, float out[3]) {
    const GteMatrix *m = (const GteMatrix *)s_saved_scratch;
    for (int r = 0; r < 3; ++r)
        out[r] = (m->m[r][0] * p->x + m->m[r][1] * p->y + m->m[r][2] * p->z) / 4096.0f +
                 (float)m->t[r];
}

static CV lerp_cv(const CV *a, const CV *b, float f) {
    CV r = {a->sx + (b->sx - a->sx) * f, a->sy + (b->sy - a->sy) * f, a->q + (b->q - a->q) * f,
            a->wx + (b->wx - a->wx) * f, a->wy + (b->wy - a->wy) * f, a->wz + (b->wz - a->wz) * f};
    return r;
}

/* Clips poly against one screen edge: axis 0 = x, 1 = y; keep values >= lim (sign 1) or <= lim. */
static int clip_edge(CV *in, int n, CV *out, int axis, float lim, int keep_greater) {
    int m = 0;
    for (int i = 0; i < n; ++i) {
        CV *a = &in[i], *b = &in[(i + 1) % n];
        float va = axis ? a->sy : a->sx, vb = axis ? b->sy : b->sx;
        int ia = keep_greater ? va >= lim : va <= lim;
        int ib = keep_greater ? vb >= lim : vb <= lim;
        if (ia) out[m++] = *a;
        if (ia != ib) out[m++] = lerp_cv(a, b, (lim - va) / (vb - va));
    }
    return m;
}

static void add_piece(const CV *a, const CV *b, const CV *c, const Tile *t, int tile, u8 wall) {
    if (s_piece_count >= MAX_PIECES) return;
    Piece *p = &s_pieces[s_piece_count++];
    const CV *v[3] = {a, b, c};
    for (int k = 0; k < 3; ++k) {
        p->p[k].x = v[k]->wx / v[k]->q;
        p->p[k].y = v[k]->wy / v[k]->q;
        p->p[k].z = v[k]->wz / v[k]->q;
        int u = t->u + (int)lrintf(clampf(v[k]->sx - t->x, 0, 16));
        int w = t->v + (int)lrintf(clampf(v[k]->sy - t->y, 0, 16));
        p->uv[k][0] = (u8)(u > 255 ? 255 : u);
        p->uv[k][1] = (u8)(w > 255 ? 255 : w);
    }
    p->tile = (u16)tile;
    p->wall = wall;
}

/* Cuts one surface along every tile it covers in the room camera's picture. */
static void project_surface(const Surface *s) {
    const float H = (float)s_saved_scratch[8];
    const float ox = s_saved_offset_x, oy = s_saved_offset_y;
    float view[3][3];
    for (int k = 0; k < 3; ++k) room_view(&s->v[k], view[k]);
    /* Near clip in the room's view space, keeping world positions. */
    float poly[4][6];
    int n = 0;
    const float nearz = 16.0f;
    for (int k = 0; k < 3; ++k) {
        const float *a = view[k], *b = view[(k + 1) % 3];
        const V3 *wa = &s->v[k], *wb = &s->v[(k + 1) % 3];
        int a_in = a[2] >= nearz, b_in = b[2] >= nearz;
        if (a_in) {
            float r[6] = {a[0], a[1], a[2], wa->x, wa->y, wa->z};
            memcpy(poly[n++], r, sizeof r);
        }
        if (a_in != b_in) {
            float f = (nearz - a[2]) / (b[2] - a[2]);
            float r[6] = {a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f, nearz,
                          wa->x + (wb->x - wa->x) * f, wa->y + (wb->y - wa->y) * f,
                          wa->z + (wb->z - wa->z) * f};
            memcpy(poly[n++], r, sizeof r);
        }
    }
    if (n < 3) return;
    CV base[4];
    float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
    for (int k = 0; k < n; ++k) {
        float q = 1.0f / poly[k][2];
        base[k].sx = ox + H * poly[k][0] * q;
        base[k].sy = oy + H * poly[k][1] * q;
        base[k].q = q;
        base[k].wx = poly[k][3] * q;
        base[k].wy = poly[k][4] * q;
        base[k].wz = poly[k][5] * q;
        if (base[k].sx < minx) minx = base[k].sx;
        if (base[k].sx > maxx) maxx = base[k].sx;
        if (base[k].sy < miny) miny = base[k].sy;
        if (base[k].sy > maxy) maxy = base[k].sy;
    }
    int cx0 = (int)floorf((minx - CELL_X0) / 16), cx1 = (int)floorf((maxx - CELL_X0) / 16);
    int cy0 = (int)floorf((miny - CELL_Y0) / 16), cy1 = (int)floorf((maxy - CELL_Y0) / 16);
    if (cx1 < 0 || cy1 < 0 || cx0 >= CELLS_W || cy0 >= CELLS_H) return;
    if (cx0 < 0) cx0 = 0;
    if (cy0 < 0) cy0 = 0;
    if (cx1 >= CELLS_W) cx1 = CELLS_W - 1;
    if (cy1 >= CELLS_H) cy1 = CELLS_H - 1;
    ++s_stamp;
    for (int cy = cy0; cy <= cy1; ++cy)
        for (int cx = cx0; cx <= cx1; ++cx) {
            int c = cy * CELLS_W + cx;
            for (int it = s_cell_start[c]; it < s_cell_start[c + 1]; ++it) {
                int ti = s_cell_items[it];
                if (s_tile_stamp[ti] == s_stamp) continue;
                s_tile_stamp[ti] = s_stamp;
                const Tile *t = &s_tiles[ti];
                CV a[12], b[12];
                int m = n;
                memcpy(a, base, sizeof(CV) * (size_t)n);
                m = clip_edge(a, m, b, 0, (float)t->x, 1);
                if (m < 3) continue;
                m = clip_edge(b, m, a, 0, (float)t->x + 16, 0);
                if (m < 3) continue;
                m = clip_edge(a, m, b, 1, (float)t->y, 1);
                if (m < 3) continue;
                m = clip_edge(b, m, a, 1, (float)t->y + 16, 0);
                if (m < 3) continue;
                for (int k = 1; k + 1 < m; ++k) add_piece(&a[0], &a[k], &a[k + 1], t, ti, s->wall);
            }
        }
}

static void build_pieces(void) {
    unsigned key = hash_add(s_tile_hash, (unsigned)s_surf_count);
    for (int i = 0; i < 8; ++i) key = hash_add(key, s_saved_scratch[i]);
    key = hash_add(key, s_saved_scratch[8] | ((unsigned)s_saved_offset_x << 16));
    key = hash_add(key, s_saved_offset_y | ((unsigned)(uintptr_t)s_mesh_key << 16));
    if (key == s_piece_key) return;
    s_piece_key = key;
    s_piece_count = 0;
    bin_tiles();
    for (int i = 0; i < s_surf_count; ++i) project_surface(&s_surf[i]);
}

/* ================= drawing from the chase camera ================= */

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

static float fogk(float z) { return clampf(1.15f - z / 10000.0f, 0.2f, 1.0f); }
static u8 fog(float z, int base) { return (u8)(base * fogk(z)); }

static int clamp_depth(int d) { return d < 1 ? 1 : d > 0xFFE ? 0xFFE : d; }
/* The floor sorts past everything standing on it; walls by distance, a little back. */
static int floor_depth(float maxz) { return clamp_depth(0x800 + (int)(maxz / 8.0f)); }
static int wall_depth(float maxz) { return clamp_depth((int)(maxz / 4.0f) + 24); }

static void link_prim(RenderNativeTag *tag, int depth) {
    RenderNativeTag *entry = &D_800B0E38.ordering[g_ActiveDrawSlot][depth];
    tag->address = entry->address;
    entry->address = (uintptr_t)tag;
}

static int screen_ok(int minx, int maxx, int miny, int maxy) {
    if (maxx < 0 || minx > 320 || maxy < 0 || miny > 224) return 0;
    return maxx - minx <= 1023 && maxy - miny <= 511; /* the GPU drops bigger ones */
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
    if (!screen_ok(minx, maxx, miny, maxy)) return;
    s_tri_used++;
    p->tag.length = 6;
    p->code = 0x30;
    p->r0 = fog(a[2], base_r); p->g0 = fog(a[2], base_g); p->b0 = fog(a[2], base_b);
    p->r1 = fog(b[2], base_r); p->g1 = fog(b[2], base_g); p->b1 = fog(b[2], base_b);
    p->r2 = fog(c[2], base_r); p->g2 = fog(c[2], base_g); p->b2 = fog(c[2], base_b);
    p->p1 = p->p2 = 0;
    link_prim(&p->tag, depth);
}

/* View-space vertex with texture coordinates, for near clipping. */
typedef struct { float v[3], u, w; } TV;

static int near_clip(const TV *in, int n, TV *out) {
    int m = 0;
    for (int k = 0; k < n; ++k) {
        const TV *a = &in[k], *b = &in[(k + 1) % n];
        int a_in = a->v[2] >= NEAR_Z, b_in = b->v[2] >= NEAR_Z;
        if (a_in) out[m++] = *a;
        if (a_in != b_in) {
            float f = (NEAR_Z - a->v[2]) / (b->v[2] - a->v[2]);
            TV r;
            for (int i = 0; i < 3; ++i) r.v[i] = a->v[i] + (b->v[i] - a->v[i]) * f;
            r.v[2] = NEAR_Z;
            r.u = a->u + (b->u - a->u) * f;
            r.w = a->w + (b->w - a->w) * f;
            out[m++] = r;
        }
    }
    return m;
}

/* A surface in flat colour: the floor where the room camera shows nothing, and walls. */
static void draw_surface(const Surface *s) {
    TV in[3], poly[4];
    for (int k = 0; k < 3; ++k) {
        to_view(&s->v[k], in[k].v);
        in[k].u = in[k].w = 0;
    }
    if (in[0].v[2] < NEAR_Z && in[1].v[2] < NEAR_Z && in[2].v[2] < NEAR_Z) return;
    if (in[0].v[2] > FAR_Z && in[1].v[2] > FAR_Z && in[2].v[2] > FAR_Z) return;
    int n = near_clip(in, 3, poly);
    float maxz = 0;
    for (int k = 0; k < n; ++k) if (poly[k].v[2] > maxz) maxz = poly[k].v[2];
    int r, g, b, depth;
    if (s->wall) {
        r = s->shade ? 58 : 52; g = s->shade ? 64 : 58; b = s->shade ? 76 : 70;
        depth = wall_depth(maxz);
    } else {
        r = s->shade ? 92 : 78; g = s->shade ? 98 : 84; b = s->shade ? 112 : 98;
        depth = floor_depth(maxz);
    }
    if (n >= 3) emit_tri(poly[0].v, poly[1].v, poly[2].v, r, g, b, depth);
    if (n == 4) emit_tri(poly[0].v, poly[2].v, poly[3].v, r, g, b, depth);
}

static void emit_tex(const TV *a, const TV *b, const TV *c, const Tile *t, int depth) {
    if (s_tex_used >= MAX_TEX_PRIMS) return;
    PrimFT3 *p = &s_tex_prims[g_ActiveDrawSlot][s_tex_used];
    if (!project(a->v, &p->x0, &p->y0) || !project(b->v, &p->x1, &p->y1) ||
        !project(c->v, &p->x2, &p->y2))
        return;
    int minx = p->x0, maxx = p->x0, miny = p->y0, maxy = p->y0;
    if (p->x1 < minx) minx = p->x1; if (p->x1 > maxx) maxx = p->x1;
    if (p->x2 < minx) minx = p->x2; if (p->x2 > maxx) maxx = p->x2;
    if (p->y1 < miny) miny = p->y1; if (p->y1 > maxy) maxy = p->y1;
    if (p->y2 < miny) miny = p->y2; if (p->y2 > maxy) maxy = p->y2;
    if (!screen_ok(minx, maxx, miny, maxy)) return;
    s_tex_used++;
    float k = fogk((a->v[2] + b->v[2] + c->v[2]) / 3.0f);
    p->tag.length = 7;
    p->code = (u8)(0x24 | (t->code & 3));
    p->r = (u8)(t->r * k);
    p->g = (u8)(t->g * k);
    p->b = (u8)(t->b * k);
    p->u0 = (u8)lrintf(a->u); p->v0 = (u8)lrintf(a->w);
    p->u1 = (u8)lrintf(b->u); p->v1 = (u8)lrintf(b->w);
    p->u2 = (u8)lrintf(c->u); p->v2 = (u8)lrintf(c->w);
    p->clut = t->clut;
    p->tpage = t->tpage;
    p->pad = 0;
    link_prim(&p->tag, depth);
}

static void draw_piece(const Piece *pc) {
    TV in[3], poly[4];
    for (int k = 0; k < 3; ++k) {
        to_view(&pc->p[k], in[k].v);
        in[k].u = pc->uv[k][0];
        in[k].w = pc->uv[k][1];
    }
    if (in[0].v[2] < NEAR_Z && in[1].v[2] < NEAR_Z && in[2].v[2] < NEAR_Z) return;
    if (in[0].v[2] > FAR_Z && in[1].v[2] > FAR_Z && in[2].v[2] > FAR_Z) return;
    int n = near_clip(in, 3, poly);
    if (n < 3) return;
    float maxz = 0;
    for (int k = 0; k < n; ++k) if (poly[k].v[2] > maxz) maxz = poly[k].v[2];
    const Tile *t = &s_tiles[pc->tile];
    /* Nearer background layers (masks) draw over farther ones, all over the flat colour. */
    int layer = 1 + ((0xFFF - (t->ot & 0xFFF)) >> 8);
    int depth = clamp_depth((pc->wall ? wall_depth(maxz) : floor_depth(maxz)) - layer);
    emit_tex(&poly[0], &poly[1], &poly[2], t, depth);
    if (n == 4) emit_tex(&poly[0], &poly[2], &poly[3], t, depth);
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
    link_prim(&p->tag, clamp_depth((int)(z / 4.0f)));
}

static void draw_room(void) {
    s_tri_used = s_tex_used = s_line_used = 0;
    int textured = s_textures && s_tile_frame + 2 >= s_frame && s_tile_count > 0;
    if (textured) build_pieces();
    for (int i = 0; i < s_floor_surf_count; ++i) draw_surface(&s_surf[i]);
    if (s_solid_walls)
        for (int i = s_floor_surf_count; i < s_surf_count; ++i) draw_surface(&s_surf[i]);
    if (textured)
        for (int i = 0; i < s_piece_count; ++i)
            if (!s_pieces[i].wall || s_solid_walls) draw_piece(&s_pieces[i]);
    if (!s_outlines) return;
    for (int i = 0; i < s_wall_count; ++i) {
        const WallEdge *w = &s_walls_list[i];
        V3 ta = {w->a.x, w->a.y - s_wall_height, w->a.z};
        V3 tb = {w->b.x, w->b.y - s_wall_height, w->b.z};
        draw_line(&w->a, &w->b, 120, 210, 230);
        draw_line(&ta, &tb, 60, 110, 130);
        draw_line(&w->a, &ta, 60, 110, 130);
    }
}

/* ================= camera ================= */

static float aya_yaw_behind(void) {
    /* Facing a points along (sin a, cos a), as the camera looks along (sin yaw, cos yaw). */
    float a = (float)(g_PlayerEntity->rot_y & 0xFFF) * (2.0f * PI_F / 4096.0f);
    return a;
}

/* The camera stops short of the first wall between it and the focus point (seen from above). */
static float collide_distance(float tx, float tz, float fx, float fz, float want) {
    if (!s_collide) return want;
    float cx = tx - fx * want, cz = tz - fz * want;
    float best = 1.0f;
    for (int i = 0; i < s_wall_count; ++i) {
        const WallEdge *w = &s_walls_list[i];
        float ex = w->b.x - w->a.x, ez = w->b.z - w->a.z;
        float dx = cx - tx, dz = cz - tz;
        float den = dx * ez - dz * ex;
        if (fabsf(den) < 1e-3f) continue;
        float ax = w->a.x - tx, az = w->a.z - tz;
        float t = (ax * ez - az * ex) / den;   /* along the focus -> camera segment */
        float u = (ax * dz - az * dx) / den;   /* along the wall edge */
        if (t > 0.0f && t < best && u >= 0.0f && u <= 1.0f) best = t;
    }
    float d = want * best;
    if (best < 1.0f) d -= 120.0f;
    return d < 200.0f ? 200.0f : d;
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
    float hx = sinf(s_yaw), hz = cosf(s_yaw);
    float want = collide_distance(tx, tz, hx, hz, (float)s_distance * cp) / (cp > 0.05f ? cp : 0.05f);
    /* Ease back out after a wall let go; snap in when one gets closer. */
    if (want < s_cur_distance) s_cur_distance = want;
    else s_cur_distance += (want - s_cur_distance) * 0.15f;
    s_cam[0] = tx - f[0] * s_cur_distance;
    s_cam[1] = ty - f[1] * s_cur_distance;
    s_cam[2] = tz - f[2] * s_cur_distance;
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

/* ================= input and hooks ================= */

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
    /* Up walks along (sin B, cos B), seen in game: B = yaw. */
    int angle = (int)lrintf(s_yaw * 4096.0f / (2.0f * PI_F));
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
        s_cur_distance = (float)s_distance;
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

/* The background stays hidden only while the field pass draws the room in its place; its tiles
 * are read for the textures. */
static int on_background(void *ctx, void *user) {
    (void)ctx;
    (void)user;
    if (!can_run() || s_frame - s_drawn_frame > 2) return 0;
    if (s_textures) gather_tiles();
    return 1;
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
                            "Camera behind Aya; the room is rebuilt from its floor and walls.",
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
    pe_menu_register_toggle(0, "Room textures",
                            "Paint the floor and walls with the room's picture, seen from its camera.",
                            &s_textures, NULL);
    pe_menu_register_toggle(0, "Solid walls", "Walls on the edges where the floor ends.",
                            &s_solid_walls, NULL);
    pe_menu_register_int(0, "Wall height", "Height of the walls.", &s_wall_height, 200, 4000, 100);
    pe_menu_register_toggle(0, "Camera stops at walls", "Pull the camera in front of walls.",
                            &s_collide, NULL);
    pe_menu_register_toggle(0, "Wall outlines", "Lines along the floor's edges and wall tops.",
                            &s_outlines, NULL);
    pe_menu_begin(0);
    return 0;
}

static void chasecam_shutdown(void) {
    SDL_RemoveEventWatch(on_event, NULL);
    release_angles();
}

const PeMod pe_mod_pe_chasecam = {MOD_ID, "Chase camera", "0.2", NULL, chasecam_init,
                                  chasecam_shutdown};
