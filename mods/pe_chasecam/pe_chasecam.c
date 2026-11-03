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
#include "pe1/psyq_gpu.h"
#include "pe1/draw_buffers.h"
#include <psyz/gpu.h>
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
static int s_obstacle_height = 500; /* walls around closed holes in the floor (seats, tables) */
static int s_collide = 1;

/* ---- camera state ---- */
static float s_yaw, s_pitch = 0.30f;
static int s_place_behind = 1;   /* next active frame: put the camera behind Aya */
static unsigned s_frame, s_drawn_frame;

/* Mouse deltas and wheel, gathered by an event watch between frames. */
static float s_mouse_dx, s_mouse_dy, s_wheel;
static int s_f8_down, s_home_down, s_f7_down, s_f9_down, s_dump_pending;

/* Per room and camera: how the room's picture sits on the rebuilt walls and floor. The picture
 * position of every surface point is moved by (ox, oy) and scaled by (sx, sy) about the screen
 * centre before it picks its tile; walls are wall / obst high. The mod's data/rooms.txt (see
 * data_dir) keeps them, one "room group ox oy sx sy wall obst shape" line each. */
/* shape 1: the room is a box built from the picture (see build_box); 0: walls on the collision
 * floor's edges. */
typedef struct { int room, group; float ox, oy, sx, sy; int wall, obst, shape; } RoomFit;
#define MAX_FITS 512
static RoomFit s_fits[MAX_FITS];
static int s_fit_count;
static RoomFit s_fit = {-1, -1, 0, 0, 1, 1, 1400, 500, -1};
static int s_default_shape = 0; /* F1 "Rooms' shape": 0 collision walls (default), 1 picture box */
/* A room's shape: its own choice (Backspace), or -1 = the F1 option. */
static int room_shape(void) { return s_fit.shape < 0 ? s_default_shape : s_fit.shape; }
static int s_fit_saved = 1;   /* the current fit matches the file */
static int s_edit;            /* F9: fit editor keys on the numpad */
static unsigned s_edit_toast;
static void dump_room(void);

/* Room camera words saved around the actor pass. */
static u32 s_saved_scratch[9];
/* Signed: rooms that scroll put the screen origin above or left of the picture. */
static s16 s_saved_offset_x, s_saved_offset_y;
/* The same, as they were when the tiles were read (the frame before the one being drawn):
 * the picture moves with the game's scroll, so it is projected with its own camera. */
static u32 s_pic_scratch[9];
static s16 s_pic_offset_x, s_pic_offset_y;
static int s_swapped;
static float s_mirror = 1.0f;    /* -1 when the room's camera is a mirror (det < 0) */

/* The movement code's base angles while the chase camera owns them. */
static int s_angles_owned;
static u16 s_room_angle_digital, s_room_angle_analog, s_written_angle;

/* ---- room surfaces: floor triangles and wall quads (as triangles), in world units ---- */
typedef struct { float x, y, z; } V3;
typedef struct { V3 v[3]; u8 shade, wall; u16 src; } Surface; /* src: floor triangle / wall edge */
typedef struct { V3 a, b; float height; int loop; } WallEdge;
/* A closed run of wall edges: the floor's outline (outer) or a hole in it (an obstacle). */
typedef struct { int first, count, outer; float area, minx, maxx, minz, maxz; } WallLoop;
#define MAX_LOOPS 512
static WallLoop s_loops[MAX_LOOPS];
static int s_loop_count;

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
static int s_mesh_wall_height, s_mesh_obstacle_height;
static u16 s_surf_pieces[MAX_SURFACES]; /* textured pieces per surface, for the dump */
static unsigned s_mesh_version;   /* bumped whenever build_mesh rebuilds s_surf */
static float s_floor_y;           /* the collision floor's usual height (median) */
static int s_mesh_shape = -1;
static float s_coll[4];           /* the collision floor's bounds: x0, x1, z0, z1 */
/* Picture box: floor at s_floor_y, side walls on these bounds, walls up to the room's height. */
static float s_box[4];            /* x0, x1, z0, z1 */
static int s_box_active;          /* s_surf holds the box this frame */
static unsigned s_box_key;
#define SRC_NO_TEXTURE 0xFFFD     /* a plain backing face of the box */
#define SRC_BOX 0xFFFE            /* a face of the box cut from the picture */
/* The obstacle loop Aya stands inside (a stage or platform: its floor isn't in the triangles),
 * -1 for none, and her height there; the mesh is built with it. */
static int s_inside_loop = -1, s_mesh_inside_loop = -2;
static float s_inside_y;

/* Texture layers: on top of the room's fit, every wall edge and the floor can move, size and turn
 * their own part of the picture (about the wall's centre / the screen centre, in picture pixels). */
/* h: the wall's own height (< 0: the room's). rect: the wall takes its texture from a plain
 * rectangle of the picture (its bounding box there), spread evenly over the wall, instead of
 * its perspective outline. */
typedef struct { float ox, oy, sx, sy, rot, h, rect; } Adjust;
static const Adjust k_identity = {0, 0, 1, 1, 0, -1, 0};
static Adjust s_wall_adj[MAX_WALLS], s_floor_adj = {0, 0, 1, 1, 0, -1, 0};
static float s_wall_rect[MAX_WALLS][4]; /* x, y, w, h on the picture; w <= 0 when not seen */
static unsigned s_height_version, s_mesh_height_version = ~0u;
static float s_wall_pivot[MAX_WALLS][2];
typedef struct { int room, group, layer; Adjust a; } LayerFit; /* layer -1 floor, >= 0 wall edge */
#define MAX_LAYER_FITS 8192
static LayerFit s_layer_fits[MAX_LAYER_FITS];
static int s_layer_fit_count;
static unsigned s_adj_version;
static int s_sel = -2; /* editor selection: -2 the whole room, -1 the floor, >= 0 a wall edge */
static float s_cy = 112.0f; /* chase view centre: lower while the editor splits the screen */

/* ---- custom wall images (data/textures/room_NNN and /common: 256-colour BMPs, entry 0
 * transparent; make them with tools/make_texture.py). An image put on wall edge i spreads over
 * walls i .. i+span-1 of its loop (by length) and floor to wall top. It is uploaded into video
 * memory the room leaves blank (all zero, never drawn into), in 128-texel-wide 8-bit columns,
 * halved until it fits, and checked every frame; if the game writes there it moves. ---- */
/* The room data built into the game (embed_data.cmake; see data_dir). */
typedef struct PeChasecamFile { const char *path; const unsigned char *data; unsigned size; } PeChasecamFile;
extern const PeChasecamFile pe_chasecam_files[];
extern const unsigned pe_chasecam_file_count;
#define MAX_IMAGES 48
#define IMG_CHUNK 128
typedef struct {
    char name[96], path[600];
    const PeChasecamFile *file; /* built in (path unused), or NULL: read path */
    int loaded, failed;       /* loaded: in video memory now */
    int w, h;                 /* size as uploaded */
    int chunks, vx[8], vy[8]; /* each 128-texel column's place in video memory */
    int cx, cy;               /* its palette's place */
    u8 *pix;                  /* full-size indices, w0 x h0 */
    int w0, h0;
    u16 clut[256];
    u16 *up;                  /* what was uploaded: chunk k's rows at k * 64 * h */
    unsigned checked;         /* the frame image_check last compared it */
    unsigned retry;           /* no room in video memory: the frame to look again, or 0 */
} CustomImage;
static CustomImage s_images[MAX_IMAGES];
static int s_image_count, s_images_room = -999, s_images_group = -1;
static int s_wall_img[MAX_WALLS], s_wall_span[MAX_WALLS]; /* image index (-1 none) on a first wall */
static int s_wall_cover[MAX_WALLS];      /* the first wall of the image covering this one, or -1 */
static float s_wall_cum[MAX_WALLS];      /* along the covering image: length before this wall */
static float s_cover_len[MAX_WALLS];     /* the covering image's total length (on its first wall) */
static unsigned s_img_version;
static unsigned s_cover_img = ~0u, s_cover_mesh = ~0u; /* versions update_cover last saw */
static int s_cover_walls = -1;
typedef struct { int room, group, wall, span; char name[96]; } WallImage;
#define MAX_WALL_IMAGES 1024
static WallImage s_wall_images[MAX_WALL_IMAGES];
static int s_wall_image_count;
static void images_enter_room(int room, int group);

static int adjust_is_identity(const Adjust *a) {
    return a->ox == 0 && a->oy == 0 && a->sx == 1 && a->sy == 1 && a->rot == 0 && a->h < 0 &&
           a->rect == 0;
}

static void apply_adjust(float *x, float *y, const Adjust *a, float px, float py) {
    float dx = (*x - px) * a->sx, dy = (*y - py) * a->sy;
    float c = cosf(a->rot), sn = sinf(a->rot);
    *x = px + c * dx - sn * dy + a->ox;
    *y = py + sn * dx + c * dy + a->oy;
}

/* ---- background tiles this frame, where Render_DrawSpriteEntry would have put them ---- */
typedef struct {
    s16 x, y;      /* screen position of the 16x16 tile */
    u8 u, v;
    u16 clut, tpage;
    u8 code, r, g, b;
    u16 ot;        /* the tile's ordering-table entry: lower is nearer */
    u16 cell;      /* the sprite's index in its mesh entry = its HD atlas cell */
    uint64_t hd;   /* the entry's HD atlas key, 0 when there is none */
} Tile;
#define MAX_TILES 16384 /* the frame's tiles, then the filled cells' (see build_fill_map) */
static Tile s_tiles[MAX_TILES];
static int s_tile_count;
static unsigned s_tile_hash, s_tile_frame;

/* Screen cells (16 px) covering a margin around the frame, each listing its tiles. */
/* The whole picture, not only the part on screen: scrolling rooms are taller or wider. */
#define CELL_X0 (-1024)
#define CELL_Y0 (-1024)
#define CELLS_W 128
#define CELLS_H 128
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
    /* HD background cell for the polygon (see hd_select): command, key low/high, the
     * sprite's texel origin, then PsyZ's packed-stream marker. All no-ops (0) without one. */
    u32 hd_command, hd_key_low, hd_key_high, hd_tile, packed_marker;
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

/* ---- HD backgrounds ----
 * The game's room tiles carry a command that makes PsyZ draw them from the HD background
 * pack (Render_LoadMeshEntry). Triangles cut from those tiles send the same cell with the
 * tile's texel origin (opcode 0xFA), so PsyZ samples the matching part of the HD cell. */
#define HD_TILE_OPCODE 0xFAu
#define HD_PACKED_MARKER 0x005A4D50u /* PsyZ PSYZ_MERGED_PRIM_NOP: the packet holds packed 32-bit words */
static int s_hd_registered;

static int hd_tile_command(const u_long *words, int available, void *user) {
    (void)user;
    if (available < 4) return -1;
    u32 tile = (u32)words[3];
    Psyz_GpuSelectSpriteAtlasTile((uint64_t)(u32)words[1] | ((uint64_t)(u32)words[2] << 32),
                                  (u32)words[0] & 0xFFFFFF, tile & 0xFF, (tile >> 8) & 0xFF);
    return 4;
}

/* Render_HdHash: the key of mesh entry `index` in the room's HD pack. */
static uint64_t hd_entry_key(uint64_t geometry_key, u32 index) {
    const u8 *bytes = (const u8 *)&index;
    for (int i = 0; i < 4; ++i) geometry_key = (geometry_key ^ bytes[i]) * UINT64_C(1099511628211);
    return geometry_key;
}

static void hd_select(PrimFT3 *p, const Tile *t) {
    p->packed_marker = HD_PACKED_MARKER;
    if (!t || !t->hd || !s_hd_registered) {
        p->hd_command = p->hd_key_low = p->hd_key_high = p->hd_tile = 0;
        return;
    }
    p->hd_command = (HD_TILE_OPCODE << 24) | t->cell;
    p->hd_key_low = (u32)t->hd;
    p->hd_key_high = (u32)(t->hd >> 32);
    p->hd_tile = t->u | ((u32)t->v << 8);
}

/* ---- chase camera basis for this frame ---- */
static float s_rx[3], s_ry[3], s_rz[3], s_cam[3], s_h;
static float s_cur_distance = 1600.0f;

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ================= room mesh ================= */

static void push_surface(const V3 *a, const V3 *b, const V3 *c, u8 shade, u8 wall, u16 src, int depth) {
    if (s_surf_count >= MAX_SURFACES) return;
    if (!wall && depth < 99) {
        float ab = (b->x - a->x) * (b->x - a->x) + (b->z - a->z) * (b->z - a->z);
        float bc = (c->x - b->x) * (c->x - b->x) + (c->z - b->z) * (c->z - b->z);
        float ca = (a->x - c->x) * (a->x - c->x) + (a->z - c->z) * (a->z - c->z);
        float longest = ab > bc ? (ab > ca ? ab : ca) : (bc > ca ? bc : ca);
        if (longest > SPLIT_EDGE * SPLIT_EDGE && depth < 5) {
            V3 m_ab = {(a->x + b->x) / 2, (a->y + b->y) / 2, (a->z + b->z) / 2};
            V3 m_bc = {(b->x + c->x) / 2, (b->y + c->y) / 2, (b->z + c->z) / 2};
            V3 m_ca = {(c->x + a->x) / 2, (c->y + a->y) / 2, (c->z + a->z) / 2};
            push_surface(a, &m_ab, &m_ca, shade, 0, src, depth + 1);
            push_surface(&m_ab, b, &m_bc, shade, 0, src, depth + 1);
            push_surface(&m_ca, &m_bc, c, shade, 0, src, depth + 1);
            push_surface(&m_ab, &m_bc, &m_ca, shade, 0, src, depth + 1);
            return;
        }
    }
    Surface *s = &s_surf[s_surf_count++];
    s->v[0] = *a;
    s->v[1] = *b;
    s->v[2] = *c;
    s->shade = shade;
    s->wall = wall;
    s->src = src;
}

/* A wall on edge a-b, split into a grid of quads small enough to texture and clip. */
static void push_wall(const WallEdge *w, int index) {
    float dx = w->b.x - w->a.x, dz = w->b.z - w->a.z;
    float len = sqrtf(dx * dx + dz * dz);
    int cols = (int)ceilf(len / WALL_STEP), rows = (int)ceilf(w->height / WALL_STEP);
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    for (int c = 0; c < cols; ++c) {
        float f0 = (float)c / cols, f1 = (float)(c + 1) / cols;
        V3 b0 = {w->a.x + dx * f0, w->a.y + (w->b.y - w->a.y) * f0, w->a.z + dz * f0};
        V3 b1 = {w->a.x + dx * f1, w->a.y + (w->b.y - w->a.y) * f1, w->a.z + dz * f1};
        for (int r = 0; r < rows; ++r) {
            float h0 = w->height * r / rows, h1 = w->height * (r + 1) / rows;
            V3 p00 = {b0.x, b0.y - h0, b0.z}, p10 = {b1.x, b1.y - h0, b1.z};
            V3 p01 = {b0.x, b0.y - h1, b0.z}, p11 = {b1.x, b1.y - h1, b1.z};
            push_surface(&p00, &p10, &p11, (u8)((index + c) & 1), 1, (u16)index, 0);
            push_surface(&p00, &p11, &p01, (u8)((index + c) & 1), 1, (u16)index, 0);
        }
    }
}

static int same_point(const V3 *a, const V3 *b) {
    return fabsf(a->x - b->x) < 0.5f && fabsf(a->z - b->z) < 0.5f;
}

/* Whether (x, z) is inside the closed loop (even-odd rule over its edges). */
static int inside_loop(const WallLoop *l, float x, float z) {
    int in = 0;
    for (int k = 0; k < l->count; ++k) {
        const WallEdge *e = &s_walls_list[l->first + k];
        if ((e->a.z > z) != (e->b.z > z) &&
            x < e->a.x + (z - e->a.z) * (e->b.x - e->a.x) / (e->b.z - e->a.z))
            in = !in;
    }
    return in;
}

/* Chains the wall edges into loops, end to start, turning an edge round when it was stored the
 * other way (the floor's triangles aren't all wound alike), and orders s_walls_list by loop.
 * A closed loop that lies inside an odd number of other closed loops is a hole in the floor:
 * an obstacle such as a seat row or a table, which gets the lower obstacle height. Loops that
 * don't close stay outer walls. */
static void build_loops(void) {
    static WallEdge sorted[MAX_WALLS];
    static u8 used[MAX_WALLS];
    static u8 closed[MAX_LOOPS];
    int n = 0;
    s_loop_count = 0;
    memset(used, 0, sizeof used);
    for (int start = 0; start < s_wall_count; ++start) {
        if (used[start]) continue;
        WallLoop loop = {n, 0, 0, 0, 1e9f, -1e9f, 1e9f, -1e9f};
        int cur = start, flip = 0;
        V3 first = s_walls_list[start].a;
        int loop_index = s_loop_count < MAX_LOOPS ? s_loop_count : MAX_LOOPS - 1;
        while (cur >= 0) {
            used[cur] = 1;
            WallEdge e = s_walls_list[cur];
            if (flip) {
                V3 t = e.a;
                e.a = e.b;
                e.b = t;
            }
            e.loop = loop_index;
            sorted[n++] = e;
            loop.count++;
            loop.area += e.a.x * e.b.z - e.b.x * e.a.z;
            if (e.a.x < loop.minx) loop.minx = e.a.x;
            if (e.a.x > loop.maxx) loop.maxx = e.a.x;
            if (e.a.z < loop.minz) loop.minz = e.a.z;
            if (e.a.z > loop.maxz) loop.maxz = e.a.z;
            int next = -1;
            for (int j = 0; j < s_wall_count && next < 0; ++j) {
                if (used[j]) continue;
                if (same_point(&s_walls_list[j].a, &e.b)) { next = j; flip = 0; }
                else if (same_point(&s_walls_list[j].b, &e.b)) { next = j; flip = 1; }
            }
            if (next < 0) {
                if (s_loop_count < MAX_LOOPS) closed[s_loop_count] = same_point(&e.b, &first);
                break;
            }
            cur = next;
        }
        loop.area *= 0.5f;
        if (s_loop_count < MAX_LOOPS) s_loops[s_loop_count++] = loop;
    }
    memcpy(s_walls_list, sorted, sizeof(WallEdge) * (size_t)n);
    for (int i = 0; i < s_loop_count; ++i) {
        WallLoop *l = &s_loops[i];
        int depth = 0;
        if (closed[i] && l->count >= 3) {
            const WallEdge *e = &s_walls_list[l->first];
            float x = (e->a.x + e->b.x) * 0.5f, z = (e->a.z + e->b.z) * 0.5f;
            for (int j = 0; j < s_loop_count; ++j)
                if (j != i && closed[j] && s_loops[j].count >= 3 && inside_loop(&s_loops[j], x, z))
                    depth++;
        }
        l->outer = (depth & 1) == 0;
        float h = (float)(l->outer ? s_fit.wall : s_fit.obst);
        if (i == s_inside_loop && !l->outer) h = 0.0f; /* Aya stands on it: a platform */
        for (int k = 0; k < l->count; ++k) s_walls_list[l->first + k].height = h;
    }
}

/* A floor over obstacle loop `index` at height y: its outline cut into triangles (ear clipping). */
static void push_platform(int index, float y) {
    const WallLoop *l = &s_loops[index];
    if (l->count < 3 || l->count > 256) return;
    float px[256], pz[256];
    int idx[256], n = l->count;
    for (int k = 0; k < n; ++k) {
        px[k] = s_walls_list[l->first + k].a.x;
        pz[k] = s_walls_list[l->first + k].a.z;
        idx[k] = k;
    }
    float area = 0;
    for (int k = 0; k < n; ++k) area += px[k] * pz[(k + 1) % n] - px[(k + 1) % n] * pz[k];
    float sign = area >= 0 ? 1.0f : -1.0f;
    int guard = 0;
    while (n > 3 && guard++ < 4096) {
        int cut = 0;
        for (int i = 0; i < n && !cut; ++i) {
            int a = idx[(i + n - 1) % n], b = idx[i], c = idx[(i + 1) % n];
            float cross = (px[b] - px[a]) * (pz[c] - pz[a]) - (pz[b] - pz[a]) * (px[c] - px[a]);
            if (cross * sign <= 0) continue;
            int inside = 0;
            for (int j = 0; j < n && !inside; ++j) {
                int q = idx[j];
                if (q == a || q == b || q == c) continue;
                float d1 = (px[b] - px[a]) * (pz[q] - pz[a]) - (pz[b] - pz[a]) * (px[q] - px[a]);
                float d2 = (px[c] - px[b]) * (pz[q] - pz[b]) - (pz[c] - pz[b]) * (px[q] - px[b]);
                float d3 = (px[a] - px[c]) * (pz[q] - pz[c]) - (pz[a] - pz[c]) * (px[q] - px[c]);
                inside = d1 * sign >= 0 && d2 * sign >= 0 && d3 * sign >= 0;
            }
            if (inside) continue;
            V3 va = {px[a], y, pz[a]}, vb = {px[b], y, pz[b]}, vc = {px[c], y, pz[c]};
            push_surface(&va, &vb, &vc, 1, 0, 0xFFFF, 0);
            for (int j = i; j < n - 1; ++j) idx[j] = idx[j + 1];
            n--;
            cut = 1;
        }
        if (!cut) break;
    }
    if (n == 3) {
        V3 va = {px[idx[0]], y, pz[idx[0]]}, vb = {px[idx[1]], y, pz[idx[1]]}, vc = {px[idx[2]], y, pz[idx[2]]};
        push_surface(&va, &vb, &vc, 1, 0, 0xFFFF, 0);
    }
}

/* Which obstacle loop Aya stands inside, if any (the innermost, smallest one). */
static void find_inside_loop(void) {
    const FieldActor *aya = g_PlayerEntity;
    float x = (float)(aya->pos_x >> 16), z = (float)(aya->pos_z >> 16);
    int best = -1;
    for (int i = 0; i < s_loop_count; ++i) {
        const WallLoop *l = &s_loops[i];
        if (l->outer || l->count < 3) continue;
        if (x < l->minx || x > l->maxx || z < l->minz || z > l->maxz) continue;
        if (!inside_loop(l, x, z)) continue;
        if (best < 0 || fabsf(l->area) < fabsf(s_loops[best].area)) best = i;
    }
    if (best != s_inside_loop) {
        s_inside_loop = best;
        s_inside_y = (float)(aya->pos_y >> 16);
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
        s_mesh_wall_height == s_fit.wall && s_mesh_obstacle_height == s_fit.obst &&
        s_mesh_inside_loop == s_inside_loop && s_mesh_height_version == s_height_version &&
        s_mesh_shape == room_shape())
        return;
    s_mesh_shape = room_shape();
    s_mesh_version++;
    s_box_active = 0;
    s_mesh_inside_loop = s_inside_loop;
    s_mesh_height_version = s_height_version;
    s_mesh_key = db->triangles.pointer;
    s_mesh_tris = db->triangleCount;
    s_mesh_wall_height = s_fit.wall;
    s_mesh_obstacle_height = s_fit.obst;
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
        push_surface(&v[0], &v[1], &v[2], (u8)(i & 1), 0, (u16)i, 0);
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
            s_walls_list[s_wall_count].height = (float)s_wall_height;
            s_walls_list[s_wall_count].loop = -1;
            s_wall_count++;
        }
    }
    build_loops();
    for (int i = 0; i < s_wall_count && i < MAX_WALLS; ++i)
        if (s_wall_adj[i].h >= 0) s_walls_list[i].height = s_wall_adj[i].h;
    if (s_inside_loop >= 0 && s_inside_loop < s_loop_count && !s_loops[s_inside_loop].outer)
        push_platform(s_inside_loop, s_inside_y);
    s_floor_surf_count = s_surf_count;
    {
        /* Median floor height, for the picture box. */
        static float ys[MAX_SURFACES];
        int n = 0;
        for (int i = 0; i < s_floor_surf_count; ++i) ys[n++] = s_surf[i].v[0].y;
        for (int i = 1; i < n; ++i) {
            float v = ys[i];
            int j = i - 1;
            while (j >= 0 && ys[j] > v) { ys[j + 1] = ys[j]; --j; }
            ys[j + 1] = v;
        }
        s_floor_y = n ? ys[n / 2] : 0.0f;
        s_coll[0] = minx; s_coll[1] = maxx; s_coll[2] = minz; s_coll[3] = maxz;
    }
    for (int i = 0; i < s_wall_count; ++i)
        if (s_walls_list[i].height > 0.0f) push_wall(&s_walls_list[i], i);
    int outer = 0;
    for (int i = 0; i < s_loop_count; ++i) outer += s_loops[i].outer;
    pe_mod_log(MOD_ID, "room %d: %u triangles (%s), %d wall edges in %d loops (%d outer, %d "
               "obstacles) -> %d floor + %d wall surfaces, x %.0f..%.0f z %.0f..%.0f",
               pe_room_number(), db->triangleCount, sloped ? "sloped" : "flat", s_wall_count,
               s_loop_count, outer, s_loop_count - outer, s_floor_surf_count,
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
    memcpy(s_pic_scratch, s_saved_scratch, sizeof s_pic_scratch);
    s_pic_offset_x = s_saved_offset_x;
    s_pic_offset_y = s_saved_offset_y;
    if (!state) return;
    u16 out_x = (u16)(state->disp_src_x - ((u16)g_GeomScroll.state.position.x - 160));
    u16 out_y = (u16)(state->disp_src_y - ((u16)g_GeomScroll.state.position.y - 112));
    GeomEntry *entries = (GeomEntry *)((u8 *)state + state->entry_offset);
    unsigned h = 2166136261u;
    PeBackgroundInfo bg;
    Render_GetBackgroundInfo(&bg);
    uint64_t geometry_key = bg.hd_enabled && bg.hd_pack_path && *bg.hd_pack_path ? bg.geometry_key : 0;
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
        uint64_t hd = geometry_key ? hd_entry_key(geometry_key, e) : 0;
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
            t->cell = (u16)i;
            t->hd = hd;
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
    const GteMatrix *m = (const GteMatrix *)s_pic_scratch;
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

static int s_projecting; /* the surface being cut, for the per-surface counts */

/* One texel of a tile (i, j inside it) from a 1024x512 copy of video memory; 0 = transparent. */
static u16 vram_tile_texel(const u16 *vram, const Tile *t, int i, int j) {
    int bx = (t->tpage & 0xF) * 64, by = ((t->tpage >> 4) & 1) * 256, depth = (t->tpage >> 7) & 3;
    int cx = (t->clut & 0x3F) * 16, cy = (t->clut >> 6) & 0x1FF;
    int tu = (t->u + i) & 0xFF, tv = (t->v + j) & 0xFF;
    if (depth == 0) {
        u16 w = vram[(by + tv) * 1024 + ((bx + tu / 4) & 1023)];
        return vram[cy * 1024 + ((cx + ((w >> ((tu & 3) * 4)) & 0xF)) & 1023)];
    }
    if (depth == 1) {
        u16 w = vram[(by + tv) * 1024 + ((bx + tu / 2) & 1023)];
        return vram[cy * 1024 + ((cx + ((w >> ((tu & 1) * 8)) & 0xFF)) & 1023)];
    }
    return vram[(by + tv) * 1024 + ((bx + tu) & 1023)];
}

/* Each surface's average texture colour (r, g, b, weight), for its flat backing: the thin gaps
 * along the textured pieces' edges then show the same colour instead of grey. */
static float s_surf_rgb[MAX_SURFACES][4];
static const u16 *s_piece_vram;

static void add_piece_colour(const CV *a, const CV *b, const CV *c, const Tile *t) {
    if (!s_piece_vram) return;
    float area = fabsf((b->sx - a->sx) * (c->sy - a->sy) - (c->sx - a->sx) * (b->sy - a->sy)) * 0.5f;
    if (area <= 0) return;
    float mx = (a->sx + b->sx + c->sx) / 3.0f, my = (a->sy + b->sy + c->sy) / 3.0f;
    const CV *v[3] = {a, b, c};
    float r = 0, g = 0, bl = 0;
    int n = 0;
    for (int k = 0; k < 4; ++k) {
        float x = k < 3 ? (mx + v[k]->sx) * 0.5f : mx, y = k < 3 ? (my + v[k]->sy) * 0.5f : my;
        int i = (int)clampf(x - t->x, 0, 15), j = (int)clampf(y - t->y, 0, 15);
        u16 texel = vram_tile_texel(s_piece_vram, t, i, j);
        if (!texel) continue;
        r += (float)((texel & 31) << 3);
        g += (float)(((texel >> 5) & 31) << 3);
        bl += (float)(((texel >> 10) & 31) << 3);
        n++;
    }
    if (!n) return;
    /* Tinted as the GPU does it: texel * colour / 128. */
    float w = area / (float)n, *acc = s_surf_rgb[s_projecting];
    acc[0] += r * t->r / 128.0f * w;
    acc[1] += g * t->g / 128.0f * w;
    acc[2] += bl * t->b / 128.0f * w;
    acc[3] += area;
}

static void add_piece(const CV *a, const CV *b, const CV *c, const Tile *t, int tile, u8 wall) {
    if (s_piece_count >= MAX_PIECES) return;
    if (s_surf_pieces[s_projecting] < 0xFFFF) s_surf_pieces[s_projecting]++;
    add_piece_colour(a, b, c, t);
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

static void cut_into_tiles(const CV *base, int n, u8 wall, float minx, float maxx, float miny,
                           float maxy);
static int wall_rect_xy(int i, const V3 *p, float *x, float *y);
static int picture_xy(const V3 *p, float *x, float *y);

/* Cuts one surface along every tile it covers in the room camera's picture. */
static void project_surface(const Surface *s) {
    if (s->src == SRC_NO_TEXTURE) return;
    if (s->wall && s->src < MAX_WALLS && s_wall_cover[s->src] >= 0) return;
    if (s->wall && s->src < MAX_WALLS && s_wall_adj[s->src].rect > 0.5f) {
        /* Rectangle mode: picture position is an even (affine) spread over the wall, so 1/z = 1. */
        CV base[3];
        float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
        const Adjust *a = &s_wall_adj[s->src];
        const float *r = s_wall_rect[s->src];
        for (int k = 0; k < 3; ++k) {
            if (!wall_rect_xy(s->src, &s->v[k], &base[k].sx, &base[k].sy)) return;
            apply_adjust(&base[k].sx, &base[k].sy, a, r[0] + r[2] * 0.5f, r[1] + r[3] * 0.5f);
            base[k].q = 1.0f;
            base[k].wx = s->v[k].x;
            base[k].wy = s->v[k].y;
            base[k].wz = s->v[k].z;
            minx = fminf(minx, base[k].sx); maxx = fmaxf(maxx, base[k].sx);
            miny = fminf(miny, base[k].sy); maxy = fmaxf(maxy, base[k].sy);
        }
        cut_into_tiles(base, 3, s->wall, minx, maxx, miny, maxy);
        return;
    }
    const float H = (float)s_pic_scratch[8];
    const float ox = s_pic_offset_x, oy = s_pic_offset_y;
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
        base[k].sx = 160.0f + (ox + H * poly[k][0] * q - 160.0f) * s_fit.sx + s_fit.ox;
        base[k].sy = 112.0f + (oy + H * poly[k][1] * q - 112.0f) * s_fit.sy + s_fit.oy;
        if (s->wall && s->src < MAX_WALLS)
            apply_adjust(&base[k].sx, &base[k].sy, &s_wall_adj[s->src], s_wall_pivot[s->src][0],
                         s_wall_pivot[s->src][1]);
        else if (!s->wall)
            apply_adjust(&base[k].sx, &base[k].sy, &s_floor_adj, 160.0f, 112.0f);
        base[k].q = q;
        base[k].wx = poly[k][3] * q;
        base[k].wy = poly[k][4] * q;
        base[k].wz = poly[k][5] * q;
        if (base[k].sx < minx) minx = base[k].sx;
        if (base[k].sx > maxx) maxx = base[k].sx;
        if (base[k].sy < miny) miny = base[k].sy;
        if (base[k].sy > maxy) maxy = base[k].sy;
    }
    cut_into_tiles(base, n, s->wall, minx, maxx, miny, maxy);
}

/* ---- filling what the picture leaves out ----
 * Where a surface runs past the picture's edge or over a hole in it, the 16x16 cells there get
 * the nearest tile's texture again (on the grid most tiles sit on), so the last pattern repeats
 * up to FILL_CELLS cells out instead of leaving the flat colour. Filled cells are extra tiles
 * after the frame's own in s_tiles, rebuilt with the pieces. */
#define FILL_CELLS 40
static int s_fill_ax, s_fill_ay;             /* the grid: x, y of tiles mod 16 */
static u8 s_fill_covered[CELLS_W * CELLS_H]; /* some tile covers (part of) the cell */
static u8 s_fill_dist[CELLS_W * CELLS_H];    /* cells to the nearest tile on the grid, 255 none */
static s16 s_fill_src[CELLS_W * CELLS_H];    /* that tile */
static s16 s_fill_tile[CELLS_W * CELLS_H];   /* the cell's filled tile once made, -1 not yet */
static int s_fill_count;

static void build_fill_map(void) {
    static int votes[16][16];
    static int queue[CELLS_W * CELLS_H];
    memset(votes, 0, sizeof votes);
    for (int i = 0; i < s_tile_count; ++i) votes[s_tiles[i].y & 15][s_tiles[i].x & 15]++;
    int best = -1;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            if (votes[y][x] > best) { best = votes[y][x]; s_fill_ax = x; s_fill_ay = y; }
    memset(s_fill_covered, 0, sizeof s_fill_covered);
    memset(s_fill_dist, 255, sizeof s_fill_dist);
    memset(s_fill_tile, 0xFF, sizeof s_fill_tile);
    s_fill_count = 0;
    int head = 0, tail = 0;
    for (int i = 0; i < s_tile_count; ++i) {
        const Tile *t = &s_tiles[i];
        int gx0 = (int)floorf((float)(t->x - CELL_X0 - s_fill_ax) / 16), gx1 = gx0 + ((t->x & 15) != s_fill_ax);
        int gy0 = (int)floorf((float)(t->y - CELL_Y0 - s_fill_ay) / 16), gy1 = gy0 + ((t->y & 15) != s_fill_ay);
        for (int gy = gy0; gy <= gy1; ++gy)
            for (int gx = gx0; gx <= gx1; ++gx)
                if (gx >= 0 && gy >= 0 && gx < CELLS_W && gy < CELLS_H) s_fill_covered[gy * CELLS_W + gx] = 1;
        if (gx1 != gx0 || gy1 != gy0 || gx0 < 0 || gy0 < 0 || gx0 >= CELLS_W || gy0 >= CELLS_H) continue;
        /* On the grid: a source. The farthest layer is the background, the nearer ones masks. */
        int c = gy0 * CELLS_W + gx0;
        if (s_fill_dist[c] == 0) {
            if (t->ot > s_tiles[s_fill_src[c]].ot) s_fill_src[c] = (s16)i;
            continue;
        }
        s_fill_dist[c] = 0;
        s_fill_src[c] = (s16)i;
        queue[tail++] = c;
    }
    while (head < tail) {
        int c = queue[head++], d = s_fill_dist[c] + 1;
        if (d > FILL_CELLS) continue;
        int x = c % CELLS_W, y = c / CELLS_W;
        const int nb[4][2] = {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}};
        for (int k = 0; k < 4; ++k) {
            if (nb[k][0] < 0 || nb[k][1] < 0 || nb[k][0] >= CELLS_W || nb[k][1] >= CELLS_H) continue;
            int o = nb[k][1] * CELLS_W + nb[k][0];
            if (s_fill_dist[o] <= d) continue;
            s_fill_dist[o] = (u8)d;
            s_fill_src[o] = s_fill_src[c];
            queue[tail++] = o;
        }
    }
}

/* The filled tile of cell c (made on first use), or -1. */
static int fill_tile(int c) {
    if (s_fill_tile[c] >= 0) return s_fill_tile[c];
    int at = s_tile_count + s_fill_count;
    if (at >= MAX_TILES) return -1;
    Tile *t = &s_tiles[at];
    *t = s_tiles[s_fill_src[c]];
    t->x = (s16)(CELL_X0 + s_fill_ax + (c % CELLS_W) * 16);
    t->y = (s16)(CELL_Y0 + s_fill_ay + (c / CELLS_W) * 16);
    s_fill_count++;
    s_fill_tile[c] = (s16)at;
    return at;
}

/* The part of a surface inside one tile, as pieces. */
static void cut_by_tile(const CV *base, int n, u8 wall, int ti) {
    const Tile *t = &s_tiles[ti];
    CV a[12], b[12];
    int m = n;
    memcpy(a, base, sizeof(CV) * (size_t)n);
    m = clip_edge(a, m, b, 0, (float)t->x, 1);
    if (m < 3) return;
    m = clip_edge(b, m, a, 0, (float)t->x + 16, 0);
    if (m < 3) return;
    m = clip_edge(a, m, b, 1, (float)t->y, 1);
    if (m < 3) return;
    m = clip_edge(b, m, a, 1, (float)t->y + 16, 0);
    if (m < 3) return;
    for (int k = 1; k + 1 < m; ++k) add_piece(&a[0], &a[k], &a[k + 1], t, ti, wall);
}

static void cut_into_tiles(const CV *base, int n, u8 wall, float minx, float maxx, float miny,
                           float maxy) {
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
                cut_by_tile(base, n, wall, ti);
            }
        }
    /* And the cells no tile covers, from the nearest one. */
    int gx0 = (int)floorf((minx - CELL_X0 - s_fill_ax) / 16), gx1 = (int)floorf((maxx - CELL_X0 - s_fill_ax) / 16);
    int gy0 = (int)floorf((miny - CELL_Y0 - s_fill_ay) / 16), gy1 = (int)floorf((maxy - CELL_Y0 - s_fill_ay) / 16);
    if (gx0 < 0) gx0 = 0;
    if (gy0 < 0) gy0 = 0;
    if (gx1 >= CELLS_W) gx1 = CELLS_W - 1;
    if (gy1 >= CELLS_H) gy1 = CELLS_H - 1;
    for (int gy = gy0; gy <= gy1; ++gy)
        for (int gx = gx0; gx <= gx1; ++gx) {
            int c = gy * CELLS_W + gx;
            if (s_fill_covered[c] || s_fill_dist[c] > FILL_CELLS) continue;
            int ti = fill_tile(c);
            if (ti >= 0) cut_by_tile(base, n, wall, ti);
        }
}

/* Where a world point sits on the room's picture (room camera and room fit), 0 behind it. */
static int picture_xy(const V3 *p, float *x, float *y) {
    float v[3];
    room_view(p, v);
    if (v[2] < 16.0f) return 0;
    float H = (float)s_pic_scratch[8];
    *x = 160.0f + (s_pic_offset_x + H * v[0] / v[2] - 160.0f) * s_fit.sx + s_fit.ox;
    *y = 112.0f + (s_pic_offset_y + H * v[1] / v[2] - 112.0f) * s_fit.sy + s_fit.oy;
    return 1;
}

/* The same with the point's layer adjustment (wall edge `wall`, or -1 for the floor). */
static int picture_xy_layer(const V3 *p, int wall, float *x, float *y) {
    if (wall >= 0 && wall < MAX_WALLS && s_wall_adj[wall].rect > 0.5f) {
        const float *r = s_wall_rect[wall];
        if (!wall_rect_xy(wall, p, x, y)) return 0;
        apply_adjust(x, y, &s_wall_adj[wall], r[0] + r[2] * 0.5f, r[1] + r[3] * 0.5f);
        return 1;
    }
    if (!picture_xy(p, x, y)) return 0;
    if (wall >= 0 && wall < MAX_WALLS)
        apply_adjust(x, y, &s_wall_adj[wall], s_wall_pivot[wall][0], s_wall_pivot[wall][1]);
    else
        apply_adjust(x, y, &s_floor_adj, 160.0f, 112.0f);
    return 1;
}

/* A point of wall i in its rectangle: along the edge left to right, floor to top. */
static int wall_rect_xy(int i, const V3 *p, float *x, float *y) {
    const float *r = s_wall_rect[i];
    const WallEdge *w = &s_walls_list[i];
    if (r[2] <= 0 || r[3] <= 0) return 0;
    float ex = w->b.x - w->a.x, ez = w->b.z - w->a.z, len2 = ex * ex + ez * ez;
    float u = len2 > 0 ? ((p->x - w->a.x) * ex + (p->z - w->a.z) * ez) / len2 : 0;
    float base_y = w->a.y + (w->b.y - w->a.y) * u;
    float v = w->height > 0 ? (base_y - p->y) / w->height : 0; /* 0 floor, 1 top (y is down) */
    *x = r[0] + u * r[2];
    *y = r[1] + (1.0f - v) * r[3];
    return 1;
}

/* Each wall turns and sizes about its centre as the room camera sees it; its rectangle is the
 * bounding box of its four corners on the picture. */
static void wall_pivots(void) {
    for (int i = 0; i < s_wall_count && i < MAX_WALLS; ++i) {
        const WallEdge *w = &s_walls_list[i];
        {
            V3 c[4] = {w->a, w->b, {w->b.x, w->b.y - w->height, w->b.z},
                       {w->a.x, w->a.y - w->height, w->a.z}};
            float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f, x, y;
            int ok = 1;
            for (int k = 0; k < 4 && ok; ++k) {
                ok = picture_xy(&c[k], &x, &y);
                x0 = fminf(x0, x); x1 = fmaxf(x1, x);
                y0 = fminf(y0, y); y1 = fmaxf(y1, y);
            }
            s_wall_rect[i][0] = x0;
            s_wall_rect[i][1] = y0;
            s_wall_rect[i][2] = ok ? x1 - x0 : 0;
            s_wall_rect[i][3] = ok ? y1 - y0 : 0;
        }
        V3 mid = {(w->a.x + w->b.x) * 0.5f, (w->a.y + w->b.y) * 0.5f - w->height * 0.5f,
                  (w->a.z + w->b.z) * 0.5f};
        if (!picture_xy(&mid, &s_wall_pivot[i][0], &s_wall_pivot[i][1])) {
            s_wall_pivot[i][0] = 160.0f;
            s_wall_pivot[i][1] = 112.0f;
        }
    }
}

static void build_pieces(void) {
    unsigned key = hash_add(s_tile_hash, (unsigned)s_surf_count);
    for (int i = 0; i < 8; ++i) key = hash_add(key, s_pic_scratch[i]);
    key = hash_add(key, s_pic_scratch[8] | ((unsigned)(u16)s_pic_offset_x << 16));
    key = hash_add(key, (u16)s_pic_offset_y | ((unsigned)(uintptr_t)s_mesh_key << 16));
    {
        const float f[4] = {s_fit.ox, s_fit.oy, s_fit.sx, s_fit.sy};
        unsigned bits[4];
        memcpy(bits, f, sizeof bits);
        for (int i = 0; i < 4; ++i) key = hash_add(key, bits[i]);
    }
    key = hash_add(key, s_adj_version);
    key = hash_add(key, s_img_version);
    if (key == s_piece_key) return;
    s_piece_key = key;
    s_piece_count = 0;
    bin_tiles();
    build_fill_map();
    wall_pivots();
    memset(s_surf_pieces, 0, sizeof s_surf_pieces);
    memset(s_surf_rgb, 0, sizeof s_surf_rgb);
    s_piece_vram = Psyz_GpuGetVramCopy(NULL);
    for (int i = 0; i < s_surf_count; ++i) {
        s_projecting = i;
        project_surface(&s_surf[i]);
    }
}

/* ================= picture box ================= */
/* The room rebuilt from its picture: every point of the background is the room camera's ray
 * through it, which lands on the floor (at the collision floor's usual height) or, past the
 * floor, on one of the four side walls of a box that holds everything the picture shows. A grid
 * over the picture, split where its corners land on different faces, gives the surfaces; seen
 * from the room camera they cover the picture exactly, so the textures line up by construction.
 * The collision floor still moves Aya; it only no longer shapes the walls. */

static float s_cam_pos[3], s_cam_axes[3][3], s_cam_h, s_cam_ox, s_cam_oy;

static void room_camera_ray(float sx, float sy, float d[3]) {
    float v[3] = {(sx - s_cam_ox) / s_cam_h, (sy - s_cam_oy) / s_cam_h, 1.0f};
    for (int i = 0; i < 3; ++i)
        d[i] = s_cam_axes[0][i] * v[0] + s_cam_axes[1][i] * v[1] + s_cam_axes[2][i] * v[2];
}

/* Face hit by the ray through picture point (sx, sy): 0 floor, 1..4 side walls (x0, x1, z0, z1),
 * -1 nothing (above the walls, or behind). */
static int box_hit(float sx, float sy, V3 *out) {
    float d[3], best = 1e30f;
    int face = -1;
    room_camera_ray(sx, sy, d);
    const float *c = s_cam_pos;
    if (d[1] > 1e-6f) { float t = (s_floor_y - c[1]) / d[1]; if (t > 0 && t < best) { best = t; face = 0; } }
    if (d[0] < -1e-6f) { float t = (s_box[0] - c[0]) / d[0]; if (t > 0 && t < best) { best = t; face = 1; } }
    if (d[0] > 1e-6f) { float t = (s_box[1] - c[0]) / d[0]; if (t > 0 && t < best) { best = t; face = 2; } }
    if (d[2] < -1e-6f) { float t = (s_box[2] - c[2]) / d[2]; if (t > 0 && t < best) { best = t; face = 3; } }
    if (d[2] > 1e-6f) { float t = (s_box[3] - c[2]) / d[2]; if (t > 0 && t < best) { best = t; face = 4; } }
    if (face < 0) return -1;
    out->x = c[0] + d[0] * best;
    out->y = c[1] + d[1] * best;
    out->z = c[2] + d[2] * best;
    if (face > 0 && (out->y < s_floor_y - (float)s_fit.wall || out->y > s_floor_y)) return -1;
    return face;
}

static void box_cell(float x, float y, float size, int depth) {
    V3 p[4];
    int f[4];
    const float cx[4] = {x, x + size, x + size, x}, cy[4] = {y, y, y + size, y + size};
    for (int k = 0; k < 4; ++k) f[k] = box_hit(cx[k], cy[k], &p[k]);
    if (f[0] >= 0 && f[0] == f[1] && f[0] == f[2] && f[0] == f[3]) {
        u8 wall = f[0] > 0;
        push_surface(&p[0], &p[1], &p[2], (u8)(f[0] & 1), wall, SRC_BOX, 99);
        push_surface(&p[0], &p[2], &p[3], (u8)(f[0] & 1), wall, SRC_BOX, 99);
        return;
    }
    if (depth >= 3) return;
    float h = size * 0.5f;
    box_cell(x, y, h, depth + 1);
    box_cell(x + h, y, h, depth + 1);
    box_cell(x, y + h, h, depth + 1);
    box_cell(x + h, y + h, h, depth + 1);
}

/* Plain backing faces: the whole box floor and walls, untextured, under the picture's faces. */
static void box_backing(void) {
    float y0 = s_floor_y + 2.0f, top = s_floor_y - (float)s_fit.wall;
    V3 a = {s_box[0], y0, s_box[2]}, b = {s_box[1], y0, s_box[2]}, c = {s_box[1], y0, s_box[3]},
       d = {s_box[0], y0, s_box[3]};
    push_surface(&a, &b, &c, 0, 0, SRC_NO_TEXTURE, 0);
    push_surface(&a, &c, &d, 0, 0, SRC_NO_TEXTURE, 0);
    const V3 *base[4] = {&a, &b, &c, &d};
    for (int i = 0; i < 4; ++i) {
        const V3 *p = base[i], *q = base[(i + 1) % 4];
        WallEdge w = {*p, *q, (float)s_fit.wall + 2.0f, -1};
        (void)top;
        float dx = q->x - p->x, dz = q->z - p->z, len = sqrtf(dx * dx + dz * dz);
        int cols = (int)ceilf(len / WALL_STEP), rows = (int)ceilf(w.height / WALL_STEP);
        if (cols < 1) cols = 1;
        if (rows < 1) rows = 1;
        for (int cc = 0; cc < cols; ++cc)
            for (int r = 0; r < rows; ++r) {
                float f0 = (float)cc / cols, f1 = (float)(cc + 1) / cols;
                float h0 = w.height * r / rows, h1 = w.height * (r + 1) / rows;
                V3 p00 = {p->x + dx * f0, y0 - 2.0f - h0, p->z + dz * f0}, p10 = {p->x + dx * f1, y0 - 2.0f - h0, p->z + dz * f1};
                V3 p01 = {p->x + dx * f0, y0 - 2.0f - h1, p->z + dz * f0}, p11 = {p->x + dx * f1, y0 - 2.0f - h1, p->z + dz * f1};
                push_surface(&p00, &p10, &p11, (u8)(cc & 1), 1, SRC_NO_TEXTURE, 0);
                push_surface(&p00, &p11, &p01, (u8)(cc & 1), 1, SRC_NO_TEXTURE, 0);
            }
    }
}

/* Replaces s_surf with the picture box for this room camera and picture (rebuilt when either,
 * the collision mesh or the wall height change). */
static void build_box(void) {
    if (s_tile_count <= 0 || s_surf_count <= 0) return;
    unsigned key = hash_add(s_tile_hash, s_mesh_version);
    for (int i = 0; i < 9; ++i) key = hash_add(key, s_pic_scratch[i]);
    key = hash_add(key, (u16)s_pic_offset_x | ((unsigned)(u16)s_pic_offset_y << 16));
    key = hash_add(key, (unsigned)s_fit.wall);
    if (s_box_active && key == s_box_key) return;

    /* Room camera: position and axes in the world. */
    const GteMatrix *m = (const GteMatrix *)s_pic_scratch;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) s_cam_axes[r][c] = m->m[r][c] / 4096.0f;
    for (int c = 0; c < 3; ++c)
        s_cam_pos[c] = -(s_cam_axes[0][c] * m->t[0] + s_cam_axes[1][c] * m->t[1] + s_cam_axes[2][c] * m->t[2]);
    s_cam_h = (float)s_pic_scratch[8];
    s_cam_ox = s_pic_offset_x;
    s_cam_oy = s_pic_offset_y;
    if (s_cam_h <= 0) return;

    /* The picture's extent on screen. */
    float px0 = 1e9f, px1 = -1e9f, py0 = 1e9f, py1 = -1e9f;
    for (int i = 0; i < s_tile_count; ++i) {
        px0 = fminf(px0, s_tiles[i].x); px1 = fmaxf(px1, s_tiles[i].x + 16.0f);
        py0 = fminf(py0, s_tiles[i].y); py1 = fmaxf(py1, s_tiles[i].y + 16.0f);
    }

    /* Box: the collision floor, the floor the picture shows (within reason) and the room camera. */
    float cx0 = s_coll[0], cx1 = s_coll[1], cz0 = s_coll[2], cz1 = s_coll[3];
    float reach = fmaxf(cx1 - cx0, cz1 - cz0) * 1.5f + 3000.0f;
    float mx = (cx0 + cx1) * 0.5f, mz = (cz0 + cz1) * 0.5f;
    float bx0 = cx0, bx1 = cx1, bz0 = cz0, bz1 = cz1;
    for (int i = 0; i <= 32; ++i)
        for (int e = 0; e < 4; ++e) {
            float f = i / 32.0f;
            float sx = e < 2 ? px0 + (px1 - px0) * f : (e == 2 ? px0 : px1);
            float sy = e < 2 ? (e == 0 ? py0 : py1) : py0 + (py1 - py0) * f;
            float d[3];
            room_camera_ray(sx, sy, d);
            if (d[1] <= 1e-6f) continue;
            float t = (s_floor_y - s_cam_pos[1]) / d[1];
            if (t <= 0) continue;
            float x = s_cam_pos[0] + d[0] * t, z = s_cam_pos[2] + d[2] * t;
            if (fabsf(x - mx) > reach || fabsf(z - mz) > reach) continue;
            bx0 = fminf(bx0, x); bx1 = fmaxf(bx1, x);
            bz0 = fminf(bz0, z); bz1 = fmaxf(bz1, z);
        }
    bx0 = fminf(bx0, s_cam_pos[0] - 200); bx1 = fmaxf(bx1, s_cam_pos[0] + 200);
    bz0 = fminf(bz0, s_cam_pos[2] - 200); bz1 = fmaxf(bz1, s_cam_pos[2] + 200);
    s_box[0] = bx0 - 50; s_box[1] = bx1 + 50; s_box[2] = bz0 - 50; s_box[3] = bz1 + 50;

    /* Rebuild s_surf: backing first, then the picture's faces (floor faces, then wall faces). */
    s_surf_count = 0;
    box_backing();
    int backing = s_surf_count;
    for (float y = floorf(py0 / 16) * 16; y < py1; y += 16)
        for (float x = floorf(px0 / 16) * 16; x < px1; x += 16) box_cell(x, y, 16, 0);
    /* Floors before walls, as draw_room expects. */
    {
        static Surface tmp[MAX_SURFACES];
        int n = 0;
        for (int pass = 0; pass < 2; ++pass)
            for (int i = 0; i < s_surf_count; ++i)
                if (s_surf[i].wall == pass) tmp[n++] = s_surf[i];
        memcpy(s_surf, tmp, sizeof(Surface) * (size_t)n);
        s_floor_surf_count = 0;
        while (s_floor_surf_count < n && !s_surf[s_floor_surf_count].wall) s_floor_surf_count++;
    }
    s_box_key = key;
    s_box_active = 1;
    s_piece_key = 0;
    pe_mod_log(MOD_ID, "room %d camera %d picture box x %.0f..%.0f z %.0f..%.0f floor %.0f: %d faces "
               "(%d backing)", pe_room_number(), s_fit.group, s_box[0], s_box[1], s_box[2], s_box[3],
               s_floor_y, s_surf_count - backing, backing);
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
    float sy = s_cy + s_h * v[1] / v[2];
    if (sx < -1000 || sx > 1320 || sy < -1000 || sy > 1224) return 0;
    *x = (s16)lrintf(sx);
    *y = (s16)lrintf(sy);
    return 1;
}

static float fogk(float z) { return clampf(1.15f - z / 10000.0f, 0.2f, 1.0f); }
static u8 fog(float z, int base) { return (u8)(base * fogk(z)); }

static int clamp_depth(int d) { return d < 1 ? 1 : d > 0xFFE ? 0xFFE : d; }
/* The floor sorts past everything standing on it; walls by distance, a little back. The floor's
 * flat backing sorts past all of the floor's textures: by its own distance, a nearer flat
 * triangle would cover the next surface's pieces where their edges round differently. */
static int floor_depth(float maxz) { return clamp_depth(0x800 + (int)(maxz / 16.0f)); }
static int floor_back_depth(float maxz) { return clamp_depth(0xC00 + (int)(maxz / 16.0f)); }
static int s_tint_backing; /* flat surfaces take their textures' average colour this frame */
static int wall_depth(float maxz) { return clamp_depth((int)(maxz / 4.0f) + 24); }

static void link_prim(RenderNativeTag *tag, int depth) {
    RenderNativeTag *entry = &D_800B0E38.ordering[g_ActiveDrawSlot][depth];
    tag->address = entry->address;
    entry->address = (uintptr_t)tag;
}

static int screen_ok(int minx, int maxx, int miny, int maxy) {
    if (maxx < 0 || minx > 320 || maxy < (s_edit ? 112 : 0) || miny > 224) return 0;
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

/* What a triangle needs: 0 draw it, 1 cut it (the GPU drops one over 1023 x 511 on screen, and
 * project() refuses points far off it), -1 nothing of it is on screen. */
static int tri_screen(const TV *a, const TV *b, const TV *c) {
    const TV *v[3] = {a, b, c};
    float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
    for (int k = 0; k < 3; ++k) {
        float sx = 160.0f + s_h * v[k]->v[0] / v[k]->v[2], sy = s_cy + s_h * v[k]->v[1] / v[k]->v[2];
        minx = fminf(minx, sx); maxx = fmaxf(maxx, sx);
        miny = fminf(miny, sy); maxy = fmaxf(maxy, sy);
    }
    if (maxx < 0 || minx > 320 || maxy < (s_edit ? 112 : 0) || miny > 224) return -1;
    if (minx < -990 || maxx > 1310 || miny < -990 || maxy > 1214) return 1;
    return maxx - minx > 1000 || maxy - miny > 500;
}

static TV tv_mid(const TV *a, const TV *b) {
    TV r = {{(a->v[0] + b->v[0]) * 0.5f, (a->v[1] + b->v[1]) * 0.5f, (a->v[2] + b->v[2]) * 0.5f},
            (a->u + b->u) * 0.5f, (a->w + b->w) * 0.5f};
    return r;
}

/* One triangle to draw: flat colour, a background tile, or a custom wall image. */
typedef struct { int kind; const Tile *t; u16 tpage, clut; int r, g, b, depth; } TriJob;
enum { TRI_FLAT, TRI_TILE, TRI_IMAGE };
static void emit_tex(const TV *a, const TV *b, const TV *c, const Tile *t, int depth);
static void emit_img(const TV *a, const TV *b, const TV *c, u16 tpage, u16 clut, int depth);

/* Draws a triangle, cut in four (a few times over) while it is too big to draw whole. */
static void emit_split(const TV *a, const TV *b, const TV *c, const TriJob *job, int level) {
    int need = tri_screen(a, b, c);
    if (need < 0) return;
    if (need && level < 6) {
        TV ab = tv_mid(a, b), bc = tv_mid(b, c), ca = tv_mid(c, a);
        emit_split(a, &ab, &ca, job, level + 1);
        emit_split(&ab, b, &bc, job, level + 1);
        emit_split(&ca, &bc, c, job, level + 1);
        emit_split(&ab, &bc, &ca, job, level + 1);
        return;
    }
    if (job->kind == TRI_FLAT) emit_tri(a->v, b->v, c->v, job->r, job->g, job->b, job->depth);
    else if (job->kind == TRI_TILE) emit_tex(a, b, c, job->t, job->depth);
    else emit_img(a, b, c, job->tpage, job->clut, job->depth);
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
        depth = floor_back_depth(maxz);
    }
    const float *avg = s_surf_rgb[s - s_surf];
    if (s_tint_backing && avg[3] > 0) {
        r = (int)clampf(avg[0] / avg[3], 0, 255);
        g = (int)clampf(avg[1] / avg[3], 0, 255);
        b = (int)clampf(avg[2] / avg[3], 0, 255);
    }
    TriJob job = {TRI_FLAT, NULL, 0, 0, r, g, b, depth};
    if (n >= 3) emit_split(&poly[0], &poly[1], &poly[2], &job, 0);
    if (n == 4) emit_split(&poly[0], &poly[2], &poly[3], &job, 0);
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
    p->tag.length = 12;
    hd_select(p, t);
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
    TriJob job = {TRI_TILE, t, 0, 0, 0, 0, 0, depth};
    emit_split(&poly[0], &poly[1], &poly[2], &job, 0);
    if (n == 4) emit_split(&poly[0], &poly[2], &poly[3], &job, 0);
}

static int s_line_depth; /* > 0: lines go to this ordering-table entry (drawn on top) */

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
    link_prim(&p->tag, s_line_depth > 0 ? s_line_depth : clamp_depth((int)(z / 4.0f)));
}

/* ================= custom wall images ================= */

int ClearImage(RECT *rect, u_char r, u_char g, u_char b);
static u8 s_vram_reserved[512][1024]; /* blocks this module took in the current search */

/* The room data (rooms.txt, textures/common, textures/room_NNN) is built into the game from
 * mods/pe_chasecam/data (embed_data.cmake): the player's machine needs no files. Only a build made
 * from the source tree, whose folder is still there, reads and saves that folder instead, so the
 * in-game editor's changes land in the repository; everywhere else the data is read-only. */

static int is_dir(const char *path) {
    SDL_PathInfo info;
    return path && *path && SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

/* The source tree's data folder when this build can edit it, else "". */
static const char *data_dir(void) {
    static char dir[1024];
    static int checked;
    if (checked) return dir;
    checked = 1;
#ifdef PE_CHASECAM_SOURCE_DATA
    if (is_dir(PE_CHASECAM_SOURCE_DATA)) snprintf(dir, sizeof dir, "%s", PE_CHASECAM_SOURCE_DATA);
#endif
    if (dir[0]) pe_mod_log(MOD_ID, "room data: editing %s", dir);
    else pe_mod_log(MOD_ID, "room data: built in (%u files), read-only", pe_chasecam_file_count);
    return dir;
}

static const PeChasecamFile *embedded_file(const char *rel) {
    for (unsigned i = 0; i < pe_chasecam_file_count; ++i)
        if (!strcmp(pe_chasecam_files[i].path, rel)) return &pe_chasecam_files[i];
    return NULL;
}

/* A data file as a NUL-terminated buffer (SDL_free it), from the source folder or built in. */
static char *data_read(const char *rel, size_t *size) {
    const char *dir = data_dir();
    if (dir[0]) {
        char path[1200];
        snprintf(path, sizeof path, "%s/%s", dir, rel);
        return SDL_LoadFile(path, size);
    }
    const PeChasecamFile *f = embedded_file(rel);
    if (!f) return NULL;
    char *buf = SDL_malloc((size_t)f->size + 1);
    if (!buf) return NULL;
    memcpy(buf, f->data, f->size);
    buf[f->size] = 0;
    if (size) *size = f->size;
    return buf;
}

static void textures_dir(char *out, size_t size, const char *sub) {
    const char *base = data_dir();
    if (!base[0]) { out[0] = 0; return; }
    snprintf(out, size, "%s/textures/%s", base, sub);
}

static int ends_with_bmp(const char *name) {
    size_t n = strlen(name);
    return n > 4 && !SDL_strcasecmp(name + n - 4, ".bmp");
}

static void image_forget_vram(CustomImage *im) {
    /* Blank our blocks again if they are still ours, so they stay free for later. */
    const unsigned char *known;
    const unsigned short *vram = Psyz_GpuGetVramCopy(&known);
    for (int k = 0; k < im->chunks && im->loaded; ++k) {
        int tw = im->w - k * IMG_CHUNK < IMG_CHUNK ? im->w - k * IMG_CHUNK : IMG_CHUNK;
        int cols = (tw + 1) / 2;
        if (cols < 1) cols = 1;
        RECT r = {(short)im->vx[k], (short)im->vy[k], (short)cols, (short)im->h};
        if (vram && known[im->vy[k] * 1024 + im->vx[k]]) ClearImage(&r, 0, 0, 0);
        for (int y = 0; y < im->h; ++y)
            for (int x = 0; x < 64; ++x) s_vram_reserved[im->vy[k] + y][im->vx[k] + x] = 0;
    }
    if (im->loaded) {
        RECT r = {(short)im->cx, (short)im->cy, 256, 1};
        ClearImage(&r, 0, 0, 0);
        for (int x = 0; x < 256; ++x) s_vram_reserved[im->cy][im->cx + x] = 0;
    }
    im->loaded = 0;
}

/* Lists the room's and the common images; forgets the last room's uploads. */
static void images_enter_room(int room, int group) {
    for (int i = 0; i < s_image_count; ++i) {
        image_forget_vram(&s_images[i]);
        free(s_images[i].pix);
        free(s_images[i].up);
    }
    memset(s_images, 0, sizeof s_images);
    s_image_count = 0;
    s_images_room = room;
    s_images_group = group;
    char sub[32], dir[640];
    snprintf(sub, sizeof sub, "room_%03d", room);
    const char *subs[2] = {sub, "common"};
    for (int d = 0; d < 2; ++d) {
        if (!data_dir()[0]) {
            /* Built in: textures/<sub>/<name>.bmp entries. */
            char prefix[64];
            snprintf(prefix, sizeof prefix, "textures/%s/", subs[d]);
            size_t pl = strlen(prefix);
            for (unsigned i = 0; i < pe_chasecam_file_count && s_image_count < MAX_IMAGES; ++i) {
                const PeChasecamFile *f = &pe_chasecam_files[i];
                const char *name = f->path + pl;
                if (strncmp(f->path, prefix, pl) || strchr(name, '/') || !ends_with_bmp(name)) continue;
                int dup = 0;
                for (int j = 0; j < s_image_count; ++j) dup |= !strcmp(s_images[j].name, name);
                if (dup) continue;
                CustomImage *im = &s_images[s_image_count++];
                snprintf(im->name, sizeof im->name, "%s", name);
                im->file = f;
            }
            continue;
        }
        textures_dir(dir, sizeof dir, subs[d]);
        if (!dir[0]) continue;
        int count = 0;
        char **files = SDL_GlobDirectory(dir, "*.bmp", SDL_GLOB_CASEINSENSITIVE, &count);
        for (int i = 0; files && i < count && s_image_count < MAX_IMAGES; ++i) {
            int dup = 0;
            for (int j = 0; j < s_image_count; ++j) dup |= !strcmp(s_images[j].name, files[i]);
            if (dup) continue;
            CustomImage *im = &s_images[s_image_count++];
            snprintf(im->name, sizeof im->name, "%s", files[i]);
            snprintf(im->path, sizeof im->path, "%s/%s", dir, files[i]);
        }
        SDL_free(files);
    }
    /* This room's assignments. */
    for (int i = 0; i < MAX_WALLS; ++i) { s_wall_img[i] = -1; s_wall_span[i] = 1; }
    for (int i = 0; i < s_wall_image_count; ++i) {
        const WallImage *w = &s_wall_images[i];
        if (w->room != room || w->group != group || w->wall < 0 || w->wall >= MAX_WALLS) continue;
        for (int j = 0; j < s_image_count; ++j)
            if (!strcmp(s_images[j].name, w->name)) {
                s_wall_img[w->wall] = j;
                s_wall_span[w->wall] = w->span < 1 ? 1 : w->span;
            }
    }
    s_img_version++;
}

static int vram_blank(const unsigned short *vram, const unsigned char *known, int x, int y) {
    int at = y * 1024 + x;
    return known[at] && vram[at] == 0 && !s_vram_reserved[y][x];
}

/* A 64-column block of h rows inside one 256-row texture page half, all blank. */
static int find_block(const unsigned short *vram, const unsigned char *known, int h, int *ox, int *oy) {
    for (int half = 1; half >= 0; --half)          /* the lower half first: usually emptier */
        for (int px = 960; px >= 0; px -= 64) {
            int run = 0;
            for (int y = half * 256; y < half * 256 + 256; ++y) {
                int ok = 1;
                for (int x = px; x < px + 64 && ok; ++x) ok = vram_blank(vram, known, x, y);
                run = ok ? run + 1 : 0;
                if (run >= h) { *ox = px; *oy = y - h + 1; return 1; }
            }
        }
    return 0;
}

static int find_clut(const unsigned short *vram, const unsigned char *known, int *ox, int *oy) {
    for (int y = 511; y >= 0; --y)
        for (int x = 0; x + 256 <= 1024; x += 16) {
            int ok = 1;
            for (int i = 0; i < 256 && ok; ++i) ok = vram_blank(vram, known, x + i, y);
            if (ok) { *ox = x; *oy = y; return 1; }
        }
    return 0;
}

static void reserve(int x, int y, int w, int h, u8 v) {
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i) s_vram_reserved[y + j][x + i] = v;
}

/* Reserves only what the other loaded images hold (their transparent texels read as blank). */
static void reserve_loaded(const CustomImage *except) {
    memset(s_vram_reserved, 0, sizeof s_vram_reserved);
    for (int i = 0; i < s_image_count; ++i) {
        const CustomImage *o = &s_images[i];
        if (o == except || !o->loaded) continue;
        for (int k = 0; k < o->chunks; ++k) reserve(o->vx[k], o->vy[k], 64, o->h, 1);
        reserve(o->cx, o->cy, 256, 1, 1);
    }
}

/* Reads the BMP (once) and uploads it at the largest size the blank video memory takes. */
static int image_load(CustomImage *im) {
    if (im->loaded) return 1;
    if (im->failed || (im->retry && (int)(s_frame - im->retry) < 0)) return 0;
    if (!im->pix) {
        SDL_Surface *surf = im->file ? SDL_LoadBMP_IO(SDL_IOFromConstMem(im->file->data, im->file->size), true)
                                     : SDL_LoadBMP(im->path);
        SDL_Palette *pal = surf ? SDL_GetSurfacePalette(surf) : NULL;
        if (!surf || surf->format != SDL_PIXELFORMAT_INDEX8 || !pal) {
            pe_mod_log(MOD_ID, "image %s: not a 256-colour BMP (%s)", im->name, surf ? "wrong format" : SDL_GetError());
            if (surf) SDL_DestroySurface(surf);
            im->failed = 1;
            return 0;
        }
        im->w0 = surf->w;
        im->h0 = surf->h;
        im->pix = malloc((size_t)im->w0 * im->h0);
        for (int y = 0; y < im->h0; ++y)
            memcpy(im->pix + (size_t)y * im->w0, (const u8 *)surf->pixels + (size_t)y * surf->pitch, (size_t)im->w0);
        for (int i = 0; i < 256; ++i) {
            u16 c = 0;
            if (i < pal->ncolors && i != 0) {
                const SDL_Color *k = &pal->colors[i];
                c = (u16)((k->r >> 3) | ((k->g >> 3) << 5) | ((k->b >> 3) << 10));
                if (!c) c = 0x8000; /* opaque black */
            }
            im->clut[i] = c;
        }
        SDL_DestroySurface(surf);
    }
    const unsigned char *known;
    const unsigned short *vram = Psyz_GpuGetVramCopy(&known);
    if (!vram) { im->failed = 1; return 0; }
    for (int shift = 0; shift < 4; ++shift) {
        int w = im->w0 >> shift, h = im->h0 >> shift;
        if (w > 1024) continue;
        if (h > 256 || w < 16 || h < 4) continue;
        int chunks = (w + IMG_CHUNK - 1) / IMG_CHUNK, ok = 1;
        for (int k = 0; k < chunks && ok; ++k) {
            ok = find_block(vram, known, h, &im->vx[k], &im->vy[k]);
            if (ok) reserve(im->vx[k], im->vy[k], 64, h, 1);
        }
        if (ok) ok = find_clut(vram, known, &im->cx, &im->cy);
        if (!ok) {
            reserve_loaded(im);
            continue;
        }
        reserve(im->cx, im->cy, 256, 1, 1);
        im->w = w;
        im->h = h;
        im->chunks = chunks;
        free(im->up);
        im->up = malloc((size_t)chunks * 64 * h * sizeof *im->up);
        /* Upload: two 8-bit texels per halfword, left one in the low byte. */
        static u16 buf[64 * 256];
        for (int k = 0; k < chunks; ++k) {
            int tw = w - k * IMG_CHUNK < IMG_CHUNK ? w - k * IMG_CHUNK : IMG_CHUNK;
            int cols = (tw + 1) / 2;
            for (int y = 0; y < h; ++y)
                for (int c = 0; c < cols; ++c) {
                    int sx0 = ((k * IMG_CHUNK + c * 2) << shift), sx1 = ((k * IMG_CHUNK + c * 2 + 1) << shift);
                    int sy = y << shift;
                    u8 a = sx0 < im->w0 ? im->pix[(size_t)sy * im->w0 + sx0] : 0;
                    u8 b = sx1 < im->w0 && c * 2 + 1 < tw ? im->pix[(size_t)sy * im->w0 + sx1] : 0;
                    buf[y * cols + c] = (u16)(a | (b << 8));
                }
            RECT r = {(short)im->vx[k], (short)im->vy[k], (short)cols, (short)h};
            LoadImage(&r, buf);
            DrawSync(0);
            if (im->up) memcpy(im->up + (size_t)k * 64 * h, buf, (size_t)cols * h * sizeof *buf);
        }
        RECT r = {(short)im->cx, (short)im->cy, 256, 1};
        LoadImage(&r, im->clut);
        DrawSync(0);
        im->loaded = 1;
        im->checked = s_frame;
        pe_mod_log(MOD_ID, "image %s: %dx%d (1/%d size) in video memory at x %d y %d, palette %d,%d",
                   im->name, w, h, 1 << shift, im->vx[0], im->vy[0], im->cx, im->cy);
        return 1;
    }
    /* Video memory changes while a room loads: try again in a second. */
    if (!im->retry) pe_mod_log(MOD_ID, "image %s: no blank video memory for it, even at 1/8 size", im->name);
    im->retry = s_frame + 60;
    return 0;
}

/* Still ours? The game may load textures into what was blank, or clear it while a room loads
 * (a cleared image would still pass a check of its transparent corners); then the image moves.
 * Every texel and the palette are compared, once a frame. */
static int image_check(CustomImage *im) {
    if (!im->loaded) return 0;
    if (im->checked == s_frame) return 1;
    const unsigned char *known;
    const unsigned short *vram = Psyz_GpuGetVramCopy(&known);
    if (!vram) return 1;
    int ok = 1;
    for (int k = 0; k < im->chunks && ok && im->up; ++k) {
        int tw = im->w - k * IMG_CHUNK < IMG_CHUNK ? im->w - k * IMG_CHUNK : IMG_CHUNK;
        int cols = (tw + 1) / 2;
        for (int y = 0; y < im->h && ok; ++y) {
            int at = (im->vy[k] + y) * 1024 + im->vx[k];
            ok = known[at] && known[at + cols - 1] &&
                 !memcmp(vram + at, im->up + (size_t)k * 64 * im->h + (size_t)y * cols, (size_t)cols * 2);
        }
    }
    if (ok) {
        int at = im->cy * 1024 + im->cx;
        ok = known[at] && known[at + 255] && !memcmp(vram + at, im->clut, sizeof im->clut);
    }
    if (!ok) {
        pe_mod_log(MOD_ID, "image %s: its video memory was taken, moving it", im->name);
        im->loaded = 0;
        reserve_loaded(im);
        return image_load(im);
    }
    im->checked = s_frame;
    return 1;
}

/* Which wall each image covers, and how far along it each wall starts. */
static void update_cover(void) {
    if (s_cover_img == s_img_version && s_cover_mesh == s_mesh_version && s_cover_walls == s_wall_count)
        return;
    s_cover_img = s_img_version;
    s_cover_mesh = s_mesh_version;
    s_cover_walls = s_wall_count;
    for (int i = 0; i < MAX_WALLS; ++i) s_wall_cover[i] = -1;
    for (int i = 0; i < s_wall_count && i < MAX_WALLS; ++i) {
        if (s_wall_img[i] < 0) continue;
        float total = 0;
        for (int k = 0; k < s_wall_span[i] && i + k < s_wall_count; ++k) {
            const WallEdge *w = &s_walls_list[i + k];
            if (w->loop != s_walls_list[i].loop) break;
            s_wall_cover[i + k] = i;
            s_wall_cum[i + k] = total;
            total += sqrtf((w->b.x - w->a.x) * (w->b.x - w->a.x) + (w->b.z - w->a.z) * (w->b.z - w->a.z));
        }
        s_cover_len[i] = total > 1 ? total : 1;
    }
    s_piece_key = 0;
}

/* Image texel (u, v) of a point on wall j: along the covered walls, floor (v = h) to top (0),
 * through the first wall's move / size adjustment (move in % of the image). */
static void wall_image_uv(int j, const V3 *p, const CustomImage *im, float *u, float *v) {
    int first = s_wall_cover[j];
    const WallEdge *w = &s_walls_list[j];
    const Adjust *a = &s_wall_adj[first];
    float ex = w->b.x - w->a.x, ez = w->b.z - w->a.z, len2 = ex * ex + ez * ez;
    float t = len2 > 0 ? ((p->x - w->a.x) * ex + (p->z - w->a.z) * ez) / len2 : 0;
    float sacross = (s_wall_cum[j] + t * sqrtf(len2)) / s_cover_len[first];
    float base_y = w->a.y + (w->b.y - w->a.y) * t;
    float up = w->height > 0 ? (base_y - p->y) / w->height : 0;
    float su = 0.5f + (sacross - 0.5f - a->ox / 100.0f) / a->sx;
    float sv = 0.5f + ((1.0f - up) - 0.5f - a->oy / 100.0f) / a->sy;
    *u = su * im->w;
    *v = sv * im->h;
}

typedef struct { float u, v; V3 p; } IV; /* image-space clip vertex */

static int clip_iv(const IV *in, int n, IV *out, int axis, float lim, int keep_greater) {
    int m = 0;
    for (int i = 0; i < n; ++i) {
        const IV *a = &in[i], *b = &in[(i + 1) % n];
        float va = axis ? a->v : a->u, vb = axis ? b->v : b->u;
        int ia = keep_greater ? va >= lim : va <= lim, ib = keep_greater ? vb >= lim : vb <= lim;
        if (ia) out[m++] = *a;
        if (ia != ib) {
            float f = (lim - va) / (vb - va);
            IV r = {a->u + (b->u - a->u) * f, a->v + (b->v - a->v) * f,
                    {a->p.x + (b->p.x - a->p.x) * f, a->p.y + (b->p.y - a->p.y) * f, a->p.z + (b->p.z - a->p.z) * f}};
            out[m++] = r;
        }
    }
    return m;
}

static void emit_img(const TV *a, const TV *b, const TV *c, u16 tpage, u16 clut, int depth) {
    if (s_tex_used >= MAX_TEX_PRIMS) return;
    PrimFT3 *p = &s_tex_prims[g_ActiveDrawSlot][s_tex_used];
    if (!project(a->v, &p->x0, &p->y0) || !project(b->v, &p->x1, &p->y1) || !project(c->v, &p->x2, &p->y2))
        return;
    int minx = p->x0, maxx = p->x0, miny = p->y0, maxy = p->y0;
    if (p->x1 < minx) minx = p->x1; if (p->x1 > maxx) maxx = p->x1;
    if (p->x2 < minx) minx = p->x2; if (p->x2 > maxx) maxx = p->x2;
    if (p->y1 < miny) miny = p->y1; if (p->y1 > maxy) maxy = p->y1;
    if (p->y2 < miny) miny = p->y2; if (p->y2 > maxy) maxy = p->y2;
    if (!screen_ok(minx, maxx, miny, maxy)) return;
    s_tex_used++;
    float k = fogk((a->v[2] + b->v[2] + c->v[2]) / 3.0f);
    p->tag.length = 12;
    hd_select(p, NULL);
    p->code = 0x24;
    p->r = p->g = p->b = (u8)(128 * k);
    p->u0 = (u8)lrintf(a->u); p->v0 = (u8)lrintf(a->w);
    p->u1 = (u8)lrintf(b->u); p->v1 = (u8)lrintf(b->w);
    p->u2 = (u8)lrintf(c->u); p->v2 = (u8)lrintf(c->w);
    p->clut = clut;
    p->tpage = tpage;
    p->pad = 0;
    link_prim(&p->tag, depth);
}

/* One wall surface with its image: cut to the image and to its 128-texel columns. */
static void draw_wall_image(const Surface *s) {
    int first = s_wall_cover[s->src];
    if (first < 0 || s_wall_img[first] < 0 || s_wall_img[first] >= s_image_count) return;
    CustomImage *im = &s_images[s_wall_img[first]];
    if (!image_load(im) || !image_check(im)) return;
    IV a[12], b[12];
    for (int k = 0; k < 3; ++k) {
        a[k].p = s->v[k];
        wall_image_uv(s->src, &s->v[k], im, &a[k].u, &a[k].v);
    }
    int n = 3;
    n = clip_iv(a, n, b, 1, 0, 1); if (n < 3) return;
    n = clip_iv(b, n, a, 1, (float)im->h, 0); if (n < 3) return;
    for (int c = 0; c < im->chunks; ++c) {
        float u0 = (float)(c * IMG_CHUNK), u1 = fminf((float)((c + 1) * IMG_CHUNK), (float)im->w);
        IV x[12], y[12];
        int m = clip_iv(a, n, x, 0, u0, 1); if (m < 3) continue;
        m = clip_iv(x, m, y, 0, u1, 0); if (m < 3) continue;
        u16 tpage = (u16)(((im->vx[c] >> 6) & 0xF) | (((im->vy[c] >> 8) & 1) << 4) | (1 << 7));
        u16 clut = (u16)((im->cy << 6) | ((im->cx >> 4) & 0x3F));
        float voff = (float)(im->vy[c] & 255);
        for (int t = 1; t + 1 < m; ++t) {
            const IV *tri[3] = {&y[0], &y[t], &y[t + 1]};
            TV in[3], poly[4];
            for (int k = 0; k < 3; ++k) {
                to_view(&tri[k]->p, in[k].v);
                in[k].u = tri[k]->u - u0;
                in[k].w = tri[k]->v + voff;
            }
            if (in[0].v[2] < NEAR_Z && in[1].v[2] < NEAR_Z && in[2].v[2] < NEAR_Z) continue;
            if (in[0].v[2] > FAR_Z && in[1].v[2] > FAR_Z && in[2].v[2] > FAR_Z) continue;
            int q = near_clip(in, 3, poly);
            if (q < 3) continue;
            float maxz = 0;
            for (int k = 0; k < q; ++k) if (poly[k].v[2] > maxz) maxz = poly[k].v[2];
            int depth = clamp_depth(wall_depth(maxz) - 20);
            TriJob job = {TRI_IMAGE, NULL, tpage, clut, 0, 0, 0, depth};
            emit_split(&poly[0], &poly[1], &poly[2], &job, 0);
            if (q == 4) emit_split(&poly[0], &poly[2], &poly[3], &job, 0);
        }
    }
}

/* ---- editor panel: the room's own picture, half size, over the top half of the screen ---- */
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
    u16 pad2;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad3;
} PrimFT4;
typedef struct {
    RenderNativeTag tag;
    u8 r, g, b, code;
    s16 x0, y0, x1, y1, x2, y2, x3, y3;
} PrimF4;
#define PANEL_X0 80.0f
#define PANEL_W 160.0f
#define PANEL_H 112.0f
#define PANEL_DEPTH_LINES 8
#define PANEL_DEPTH_TILES 10
#define PANEL_DEPTH_BACK 14
static PrimFT4 s_panel_tiles[2][MAX_TILES];
static PrimF4 s_panel_back[2];

static void panel_xy(float px, float py, float *x, float *y) {
    *x = PANEL_X0 + px * 0.5f;
    *y = py * 0.5f;
}

/* A line on the panel, clipped to it (Liang-Barsky). */
static void panel_line(float x0, float y0, float x1, float y1, int r, int g, int b) {
    float t0 = 0, t1 = 1, dx = x1 - x0, dy = y1 - y0;
    const float p[4] = {-dx, dx, -dy, dy};
    const float q[4] = {x0 - PANEL_X0, PANEL_X0 + PANEL_W - x0, y0, PANEL_H - y0};
    for (int i = 0; i < 4; ++i) {
        if (p[i] == 0) {
            if (q[i] < 0) return;
            continue;
        }
        float t = q[i] / p[i];
        if (p[i] < 0) { if (t > t1) return; if (t > t0) t0 = t; }
        else { if (t < t0) return; if (t < t1) t1 = t; }
    }
    if (s_line_used >= MAX_LINE_PRIMS) return;
    PrimLine *l = &s_line_prims[g_ActiveDrawSlot][s_line_used++];
    l->tag.length = 3;
    l->code = 0x40;
    l->r = (u8)r;
    l->g = (u8)g;
    l->b = (u8)b;
    l->x0 = (s16)lrintf(x0 + dx * t0);
    l->y0 = (s16)lrintf(y0 + dy * t0);
    l->x1 = (s16)lrintf(x0 + dx * t1);
    l->y1 = (s16)lrintf(y0 + dy * t1);
    link_prim(&l->tag, PANEL_DEPTH_LINES);
}

/* A world-space triangle or quad outline on the panel, through its layer. */
static void panel_outline(const V3 *pts, int n, int layer, int r, int g, int b) {
    float x[4], y[4];
    for (int k = 0; k < n; ++k) {
        if (!picture_xy_layer(&pts[k], layer, &x[k], &y[k])) return;
        panel_xy(x[k], y[k], &x[k], &y[k]);
    }
    for (int k = 0; k < n; ++k) panel_line(x[k], y[k], x[(k + 1) % n], y[(k + 1) % n], r, g, b);
}

static void wall_corners(int i, V3 out[4]) {
    const WallEdge *w = &s_walls_list[i];
    out[0] = w->a;
    out[1] = w->b;
    out[2] = (V3){w->b.x, w->b.y - w->height, w->b.z};
    out[3] = (V3){w->a.x, w->a.y - w->height, w->a.z};
}

static void draw_panel(void) {
    int slot = g_ActiveDrawSlot;
    wall_pivots();
    PrimF4 *bg = &s_panel_back[slot];
    bg->tag.length = 5;
    bg->code = 0x28;
    bg->r = bg->g = bg->b = 8;
    bg->x0 = 0; bg->y0 = 0; bg->x1 = 320; bg->y1 = 0;
    bg->x2 = 0; bg->y2 = (s16)PANEL_H; bg->x3 = 320; bg->y3 = (s16)PANEL_H;
    link_prim(&bg->tag, PANEL_DEPTH_BACK);
    /* The picture as the player normally sees it: on-screen tiles, nearer layers on top. */
    for (int i = 0; i < s_tile_count; ++i) {
        const Tile *t = &s_tiles[i];
        if (t->x <= -16 || t->x >= 320 || t->y <= -16 || t->y >= 224) continue;
        PrimFT4 *q = &s_panel_tiles[slot][i];
        float x, y;
        panel_xy((float)t->x, (float)t->y, &x, &y);
        int x0 = (int)lrintf(x), y0 = (int)lrintf(y), x1 = x0 + 8, y1 = y0 + 8;
        int u1 = t->u + 16 > 255 ? 255 : t->u + 16, v1 = t->v + 16 > 255 ? 255 : t->v + 16;
        q->tag.length = 9;
        q->code = (u8)(0x2C | (t->code & 3));
        q->r = t->r;
        q->g = t->g;
        q->b = t->b;
        q->x0 = (s16)x0; q->y0 = (s16)y0; q->u0 = t->u; q->v0 = t->v;
        q->x1 = (s16)x1; q->y1 = (s16)y0; q->u1 = (u8)u1; q->v1 = t->v;
        q->x2 = (s16)x0; q->y2 = (s16)y1; q->u2 = t->u; q->v2 = (u8)v1;
        q->x3 = (s16)x1; q->y3 = (s16)y1; q->u3 = (u8)u1; q->v3 = (u8)v1;
        q->clut = t->clut;
        q->tpage = t->tpage;
        q->pad2 = q->pad3 = 0;
        int layer = 3 - ((t->ot >> 10) & 3); /* nearer tiles (lower ot) draw later */
        link_prim(&q->tag, PANEL_DEPTH_TILES + 3 - layer);
    }
    /* Frame of the picture, Aya, and what the selection takes its texture from. */
    panel_line(PANEL_X0, 0, PANEL_X0 + PANEL_W, 0, 90, 90, 90);
    panel_line(PANEL_X0, PANEL_H - 1, PANEL_X0 + PANEL_W, PANEL_H - 1, 90, 90, 90);
    {
        const FieldActor *aya = g_PlayerEntity;
        V3 feet = {(float)(aya->pos_x >> 16), (float)(aya->pos_y >> 16), (float)(aya->pos_z >> 16)};
        float x, y;
        if (picture_xy(&feet, &x, &y)) {
            panel_xy(x, y, &x, &y);
            panel_line(x - 3, y, x + 3, y, 255, 255, 255);
            panel_line(x, y - 3, x, y + 3, 255, 255, 255);
        }
    }
    if (s_sel >= 0 && s_sel < s_wall_count) {
        V3 c[4];
        wall_corners(s_sel, c);
        panel_outline(c, 4, s_sel, 255, 230, 0);
    } else if (s_sel == -1) {
        for (int i = 0; i < s_floor_surf_count; ++i)
            if (s_surf[i].src != 0xFFFF) panel_outline(s_surf[i].v, 3, -1, 0, 255, 90);
    } else {
        for (int i = 0; i < s_wall_count; ++i) {
            if (s_walls_list[i].height <= 0) continue;
            V3 c[4];
            wall_corners(i, c);
            panel_outline(c, 4, i, 0, 200, 255);
        }
    }
}

static void draw_room(void) {
    s_tri_used = s_tex_used = s_line_used = 0;
    if (pe_room_number() != s_images_room || s_fit.group != s_images_group)
        images_enter_room(pe_room_number(), s_fit.group);
    update_cover();
    int textured = s_textures && s_tile_frame + 2 >= s_frame && s_tile_count > 0;
    if (textured) build_pieces();
    s_tint_backing = textured;
    for (int i = 0; i < s_floor_surf_count; ++i) draw_surface(&s_surf[i]);
    if (s_solid_walls)
        for (int i = s_floor_surf_count; i < s_surf_count; ++i) draw_surface(&s_surf[i]);
    if (textured)
        for (int i = 0; i < s_piece_count; ++i)
            if (!s_pieces[i].wall || s_solid_walls) draw_piece(&s_pieces[i]);
    if (s_solid_walls && room_shape() == 0)
        for (int i = s_floor_surf_count; i < s_surf_count; ++i)
            if (s_surf[i].wall && s_surf[i].src < MAX_WALLS && s_wall_cover[s_surf[i].src] >= 0)
                draw_wall_image(&s_surf[i]);
    if (s_edit) {
        draw_panel();
        if (s_sel >= 0 && s_sel < s_wall_count) {
            V3 c[4];
            wall_corners(s_sel, c);
            s_line_depth = 16; /* the selected wall shows through everything below the panel */
            for (int k = 0; k < 4; ++k) draw_line(&c[k], &c[(k + 1) % 4], 255, 230, 0);
            draw_line(&c[0], &c[2], 255, 230, 0);
            s_line_depth = 0;
        }
    }
    if (!s_outlines && !s_edit) return;
    for (int i = 0; i < s_wall_count; ++i) {
        const WallEdge *w = &s_walls_list[i];
        V3 ta = {w->a.x, w->a.y - w->height, w->a.z};
        V3 tb = {w->b.x, w->b.y - w->height, w->b.z};
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
    if (room_shape() == 1 && s_box_active) {
        /* Stay inside the box: where the line back from the focus leaves it. */
        float t = want, m = 100.0f;
        if (-fx < 0) t = fminf(t, (tx - (s_box[0] + m)) / fx);
        if (-fx > 0) t = fminf(t, ((s_box[1] - m) - tx) / -fx);
        if (-fz < 0) t = fminf(t, (tz - (s_box[2] + m)) / fz);
        if (-fz > 0) t = fminf(t, ((s_box[3] - m) - tz) / -fz);
        return t < 200.0f ? 200.0f : t;
    }
    float cx = tx - fx * want, cz = tz - fz * want;
    float best = 1.0f;
    for (int i = 0; i < s_wall_count; ++i) {
        const WallEdge *w = &s_walls_list[i];
        if (w->height <= 0.0f) continue; /* the platform Aya stands on */
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

/* ================= per-room fit ================= */

static const char *data_dir(void);

static void fit_path(char *out, size_t size) {
    const char *base = data_dir();
    if (base[0]) snprintf(out, size, "%s/rooms.txt", base);
    else out[0] = 0;
}

static void fits_load(void) {
    int v2 = 0; /* older files saved shape 1 for every room: the old default, not a choice */
    s_fit_count = 0;
    s_layer_fit_count = 0;
    s_wall_image_count = 0;
    char *buf = data_read("rooms.txt", NULL);
    if (!buf) return;
    for (char *line = buf, *next; line; line = next) {
        next = strchr(line, '\n');
        if (next) *next++ = 0;
        if (s_fit_count >= MAX_FITS) break;
        RoomFit r;
        if (!strncmp(line, "# v2", 4)) v2 = 1;
        if (line[0] == '#') continue;
        if (line[0] == 'W') {
            WallImage wi;
            if (s_wall_image_count < MAX_WALL_IMAGES &&
                sscanf(line + 1, "%d %d %d %d %95[^\r\n]", &wi.room, &wi.group, &wi.wall, &wi.span,
                       wi.name) == 5)
                s_wall_images[s_wall_image_count++] = wi;
            continue;
        }
        if (line[0] == 'L') {
            LayerFit lf;
            if (s_layer_fit_count < MAX_LAYER_FITS &&
                (lf.a.h = -1, lf.a.rect = 0,
                 sscanf(line + 1, "%d %d %d %f %f %f %f %f %f %f", &lf.room, &lf.group, &lf.layer,
                        &lf.a.ox, &lf.a.oy, &lf.a.sx, &lf.a.sy, &lf.a.rot, &lf.a.h,
                        &lf.a.rect) >= 8) &&
                lf.a.sx > 0.01f && lf.a.sy > 0.01f)
                s_layer_fits[s_layer_fit_count++] = lf;
            continue;
        }
        r.shape = -1;
        if (sscanf(line, "%d %d %f %f %f %f %d %d %d", &r.room, &r.group, &r.ox, &r.oy, &r.sx,
                   &r.sy, &r.wall, &r.obst, &r.shape) >= 8 && r.sx > 0.01f && r.sy > 0.01f) {
            if (!v2 && r.shape == 1) r.shape = -1;
            if (r.shape < -1 || r.shape > 1) r.shape = -1;
            s_fits[s_fit_count++] = r;
        }
    }
    SDL_free(buf);
    pe_mod_log(MOD_ID, "%d room fits and %d wall/floor fits loaded", s_fit_count, s_layer_fit_count);
}

static void fits_save(void) {
    char path[512];
    int found = 0;
    if (!data_dir()[0]) {
        /* Built-in data is read-only: edits last until the game closes. */
        if (s_edit) pe_toast("Room data is built into this game: save from a build of the source tree");
        s_fit_saved = 1;
        return;
    }
    for (int i = 0; i < s_fit_count; ++i)
        if (s_fits[i].room == s_fit.room && s_fits[i].group == s_fit.group) {
            s_fits[i] = s_fit;
            found = 1;
        }
    if (!found && s_fit_count < MAX_FITS) s_fits[s_fit_count++] = s_fit;
    /* This room and camera's layers replace the stored ones. */
    int kept = 0;
    for (int i = 0; i < s_layer_fit_count; ++i)
        if (s_layer_fits[i].room != s_fit.room || s_layer_fits[i].group != s_fit.group)
            s_layer_fits[kept++] = s_layer_fits[i];
    s_layer_fit_count = kept;
    if (!adjust_is_identity(&s_floor_adj) && s_layer_fit_count < MAX_LAYER_FITS)
        s_layer_fits[s_layer_fit_count++] = (LayerFit){s_fit.room, s_fit.group, -1, s_floor_adj};
    for (int i = 0; i < s_wall_count && i < MAX_WALLS; ++i)
        if (!adjust_is_identity(&s_wall_adj[i]) && s_layer_fit_count < MAX_LAYER_FITS)
            s_layer_fits[s_layer_fit_count++] = (LayerFit){s_fit.room, s_fit.group, i, s_wall_adj[i]};
    /* And its wall images. */
    if (s_images_room == s_fit.room && s_images_group == s_fit.group) {
        kept = 0;
        for (int i = 0; i < s_wall_image_count; ++i)
            if (s_wall_images[i].room != s_fit.room || s_wall_images[i].group != s_fit.group)
                s_wall_images[kept++] = s_wall_images[i];
        s_wall_image_count = kept;
        for (int i = 0; i < s_wall_count && i < MAX_WALLS; ++i)
            if (s_wall_img[i] >= 0 && s_wall_image_count < MAX_WALL_IMAGES) {
                WallImage *w = &s_wall_images[s_wall_image_count++];
                w->room = s_fit.room;
                w->group = s_fit.group;
                w->wall = i;
                w->span = s_wall_span[i];
                snprintf(w->name, sizeof w->name, "%s", s_images[s_wall_img[i]].name);
            }
    }
    fit_path(path, sizeof path);
    FILE *f = path[0] ? fopen(path, "w") : NULL;
    if (!f) {
        pe_toast("Chase camera: can't write rooms.txt");
        return;
    }
    fprintf(f, "# v2\n# room group offset_x offset_y scale_x scale_y wall_height obstacle_height "
               "shape(-1 the F1 option, 1 picture box, 0 collision walls)\n");
    for (int i = 0; i < s_fit_count; ++i)
        fprintf(f, "%d %d %.2f %.2f %.4f %.4f %d %d %d\n", s_fits[i].room, s_fits[i].group,
                s_fits[i].ox, s_fits[i].oy, s_fits[i].sx, s_fits[i].sy, s_fits[i].wall,
                s_fits[i].obst, s_fits[i].shape);
    fprintf(f, "# L room group layer(-1 floor, else wall edge) offset_x offset_y scale_x scale_y "
               "rotation height(-1 = the room's) rectangle(0/1)\n");
    for (int i = 0; i < s_layer_fit_count; ++i) {
        const LayerFit *l = &s_layer_fits[i];
        fprintf(f, "L %d %d %d %.2f %.2f %.4f %.4f %.4f %.0f %d\n", l->room, l->group, l->layer,
                l->a.ox, l->a.oy, l->a.sx, l->a.sy, l->a.rot, l->a.h, l->a.rect > 0.5f);
    }
    fprintf(f, "# W room group wall span image (a file in textures/room_NNN or textures/common)\n");
    for (int i = 0; i < s_wall_image_count; ++i) {
        const WallImage *w = &s_wall_images[i];
        fprintf(f, "W %d %d %d %d %s\n", w->room, w->group, w->wall, w->span, w->name);
    }
    fclose(f);
    s_fit_saved = 1;
    char msg[96];
    snprintf(msg, sizeof msg, "Room %d camera %d fit saved", s_fit.room, s_fit.group);
    pe_toast(msg);
}

/* Picks the fit for the room and camera now showing (defaults when there is none). */
static void fit_select(void) {
    int room = pe_room_number(), group = g_GeomScroll.state.position.groupSelect;
    if (room == s_fit.room && group == s_fit.group) return;
    if (!s_fit_saved && s_fit.room >= 0) fits_save(); /* leaving an edited room keeps its edits */
    RoomFit r = {room, group, 0, 0, 1, 1, s_wall_height, s_obstacle_height, -1};
    for (int i = 0; i < s_fit_count; ++i)
        if (s_fits[i].room == room && s_fits[i].group == group) r = s_fits[i];
    s_fit = r;
    s_fit_saved = 1;
    s_floor_adj = k_identity;
    for (int i = 0; i < MAX_WALLS; ++i) s_wall_adj[i] = k_identity;
    for (int i = 0; i < s_layer_fit_count; ++i) {
        const LayerFit *l = &s_layer_fits[i];
        if (l->room != room || l->group != group) continue;
        if (l->layer == -1) s_floor_adj = l->a;
        else if (l->layer >= 0 && l->layer < MAX_WALLS) s_wall_adj[l->layer] = l->a;
    }
    s_sel = -2;
    s_adj_version++;
    s_height_version++;
}

static void fit_reset(void) {
    s_fit.ox = s_fit.oy = 0;
    s_fit.sx = s_fit.sy = 1;
    s_fit.wall = s_wall_height;
    s_fit.obst = s_obstacle_height;
    s_fit_saved = 0;
}

static void fit_toast(void) {
    char msg[200];
    snprintf(msg, sizeof msg, "Room %d cam %d  move %.1f, %.1f  size %.3f x %.3f  wall %d  obstacle %d%s",
             s_fit.room, s_fit.group, s_fit.ox, s_fit.oy, s_fit.sx, s_fit.sy, s_fit.wall, s_fit.obst,
             s_fit_saved ? "" : "  (not saved)");
    pe_toast(msg);
}

static const char *sel_name(char *buf, size_t size) {
    if (s_sel == -2) snprintf(buf, size, "Whole room");
    else if (s_sel == -1) snprintf(buf, size, "Floor");
    else {
        const WallEdge *w = &s_walls_list[s_sel];
        int outer = w->loop >= 0 && w->loop < s_loop_count && s_loops[w->loop].outer;
        snprintf(buf, size, "Wall %d/%d (%s)", s_sel, s_wall_count, outer ? "outer" : "obstacle");
    }
    return buf;
}

static void sel_toast(void) {
    char msg[220], name[64];
    sel_name(name, sizeof name);
    if (s_sel == -2) {
        fit_toast();
        return;
    }
    const Adjust *a = s_sel == -1 ? &s_floor_adj : &s_wall_adj[s_sel];
    char height[40] = "";
    if (s_sel >= 0)
        snprintf(height, sizeof height, "  height %.0f%s%s", s_walls_list[s_sel].height,
                 a->h >= 0 ? "" : " (room's)", a->rect > 0.5f ? "  rectangle" : "");
    snprintf(msg, sizeof msg, "%s  move %.1f, %.1f  size %.3f x %.3f  turn %.1f deg%s%s", name,
             a->ox, a->oy, a->sx, a->sy, a->rot * 180.0f / PI_F, height,
             s_fit_saved ? "" : "  (not saved)");
    pe_toast(msg);
}

/* Walls by distance from Aya: numpad . steps to the next one out (Shift: back in). */
static float s_wall_dist[MAX_WALLS];
static int cmp_wall_dist(const void *a, const void *b) {
    float da = s_wall_dist[*(const int *)a], db = s_wall_dist[*(const int *)b];
    return da < db ? -1 : da > db;
}

static void select_next_wall(int back) {
    static int order[MAX_WALLS];
    int n = 0;
    const FieldActor *aya = g_PlayerEntity;
    float x = (float)(aya->pos_x >> 16), z = (float)(aya->pos_z >> 16);
    for (int i = 0; i < s_wall_count && i < MAX_WALLS; ++i) {
        const WallEdge *w = &s_walls_list[i];
        if (w->height <= 0) continue;
        float ex = w->b.x - w->a.x, ez = w->b.z - w->a.z, len2 = ex * ex + ez * ez;
        float t = len2 > 0 ? clampf(((x - w->a.x) * ex + (z - w->a.z) * ez) / len2, 0, 1) : 0;
        float dx = w->a.x + ex * t - x, dz = w->a.z + ez * t - z;
        s_wall_dist[i] = dx * dx + dz * dz;
        order[n++] = i;
    }
    if (!n) return;
    qsort(order, (size_t)n, sizeof order[0], cmp_wall_dist);
    int at = -1;
    for (int i = 0; i < n; ++i)
        if (order[i] == s_sel) at = i;
    if (at < 0) at = back ? 0 : n - 1;
    s_sel = order[(at + (back ? n - 1 : 1)) % n];
}

/* F9 editor, numpad. Selection: . next wall out from Aya (Shift: back), / the floor, * the whole
 * room. The selection: 4/6/8/2 move, +/- size. Wall or floor: 7/9 turn, 1/3 width, Ctrl+1/3
 * height. Whole room: 7/9 width, 1/3 height. Always: PageUp/PageDown wall height,
 * Insert/Delete obstacle height, 5 reset the selection, 0 save; Shift is faster. */
static void edit_keys(const bool *keys) {
    static int dot_down, div_down, mul_down, end_down;
    if (!s_edit) return;
    int shift = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
    int ctrl = keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL];
    float fast = shift ? 5.0f : 1.0f;
    float step = 0.25f * fast, grow = 1.0f + 0.002f * fast, turn = 0.002f * fast;
    int hstep = (int)(10 * fast);

    int moved_sel = 0;
    if (keys[SDL_SCANCODE_KP_PERIOD] && !dot_down) { select_next_wall(shift); moved_sel = 1; }
    if (keys[SDL_SCANCODE_KP_DIVIDE] && !div_down) { s_sel = -1; moved_sel = 1; }
    if (keys[SDL_SCANCODE_KP_MULTIPLY] && !mul_down) { s_sel = -2; moved_sel = 1; }
    if (keys[SDL_SCANCODE_END] && !end_down) {
        /* Rectangle mode: the selected wall, or with the whole room selected every wall
         * (all of them off again when all are on). */
        if (s_sel >= 0) {
            s_wall_adj[s_sel].rect = s_wall_adj[s_sel].rect > 0.5f ? 0.0f : 1.0f;
        } else if (s_sel == -2) {
            int all = 1;
            for (int i = 0; i < s_wall_count && i < MAX_WALLS; ++i)
                if (s_walls_list[i].height > 0 && s_wall_adj[i].rect < 0.5f) all = 0;
            for (int i = 0; i < s_wall_count && i < MAX_WALLS; ++i) s_wall_adj[i].rect = all ? 0.0f : 1.0f;
            pe_toast(all ? "All walls: perspective" : "All walls: rectangles");
        }
        if (s_sel != -1) {
            s_adj_version++;
            s_fit_saved = 0;
            if (s_sel >= 0) sel_toast();
        }
    }
    end_down = keys[SDL_SCANCODE_END];
    {
        static int back_down;
        if (keys[SDL_SCANCODE_BACKSPACE] && !back_down) {
            s_fit.shape = !room_shape(); /* this room's own choice from now on */
            s_fit_saved = 0;
            pe_toast(s_fit.shape ? "This room: picture box" : "This room: collision walls");
        }
        back_down = keys[SDL_SCANCODE_BACKSPACE];
    }
    {
        /* F5: next image on the selected wall (none, then each file); Shift+F5 / Ctrl+F5 spread
         * it over one more / one fewer wall along its loop. */
        static int f5_down;
        if (keys[SDL_SCANCODE_F5] && !f5_down) {
            if (room_shape() != 0) {
                pe_toast("Images go on collision walls: Backspace switches the room shape");
            } else if (s_sel < 0) {
                pe_toast("Select a wall first (numpad .)");
            } else if (!s_image_count) {
                pe_toast("No images: put 256-colour BMPs in the mod's data/textures/common");
            } else {
                if (shift) s_wall_span[s_sel]++;
                else if (ctrl) { if (s_wall_span[s_sel] > 1) s_wall_span[s_sel]--; }
                else s_wall_img[s_sel] = s_wall_img[s_sel] + 1 >= s_image_count ? -1 : s_wall_img[s_sel] + 1;
                s_img_version++;
                s_fit_saved = 0;
                char msg[200];
                if (s_wall_img[s_sel] < 0) snprintf(msg, sizeof msg, "Wall %d: no image", s_sel);
                else snprintf(msg, sizeof msg, "Wall %d: %s over %d wall(s)", s_sel,
                              s_images[s_wall_img[s_sel]].name, s_wall_span[s_sel]);
                pe_toast(msg);
            }
        }
        f5_down = keys[SDL_SCANCODE_F5];
    }
    dot_down = keys[SDL_SCANCODE_KP_PERIOD];
    div_down = keys[SDL_SCANCODE_KP_DIVIDE];
    mul_down = keys[SDL_SCANCODE_KP_MULTIPLY];
    if (s_sel >= s_wall_count) s_sel = -2;
    if (moved_sel) sel_toast();

    RoomFit before = s_fit;
    int changed = 0;
    if (s_sel == -2) {
        if (keys[SDL_SCANCODE_KP_4]) s_fit.ox -= step;
        if (keys[SDL_SCANCODE_KP_6]) s_fit.ox += step;
        if (keys[SDL_SCANCODE_KP_8]) s_fit.oy -= step;
        if (keys[SDL_SCANCODE_KP_2]) s_fit.oy += step;
        if (keys[SDL_SCANCODE_KP_PLUS]) { s_fit.sx *= grow; s_fit.sy *= grow; }
        if (keys[SDL_SCANCODE_KP_MINUS]) { s_fit.sx /= grow; s_fit.sy /= grow; }
        if (keys[SDL_SCANCODE_KP_9]) s_fit.sx *= grow;
        if (keys[SDL_SCANCODE_KP_7]) s_fit.sx /= grow;
        if (keys[SDL_SCANCODE_KP_3]) s_fit.sy *= grow;
        if (keys[SDL_SCANCODE_KP_1]) s_fit.sy /= grow;
        if (keys[SDL_SCANCODE_KP_5]) {
            s_fit.ox = s_fit.oy = 0;
            s_fit.sx = s_fit.sy = 1;
        }
    } else {
        Adjust *a = s_sel == -1 ? &s_floor_adj : &s_wall_adj[s_sel];
        Adjust old = *a;
        if (keys[SDL_SCANCODE_KP_4]) a->ox -= step;
        if (keys[SDL_SCANCODE_KP_6]) a->ox += step;
        if (keys[SDL_SCANCODE_KP_8]) a->oy -= step;
        if (keys[SDL_SCANCODE_KP_2]) a->oy += step;
        if (keys[SDL_SCANCODE_KP_PLUS]) { a->sx *= grow; a->sy *= grow; }
        if (keys[SDL_SCANCODE_KP_MINUS]) { a->sx /= grow; a->sy /= grow; }
        if (keys[SDL_SCANCODE_KP_9]) a->rot += turn;
        if (keys[SDL_SCANCODE_KP_7]) a->rot -= turn;
        if (keys[SDL_SCANCODE_KP_3]) { if (ctrl) a->sy *= grow; else a->sx *= grow; }
        if (keys[SDL_SCANCODE_KP_1]) { if (ctrl) a->sy /= grow; else a->sx /= grow; }
        if (keys[SDL_SCANCODE_KP_5]) *a = k_identity;
        a->sx = clampf(a->sx, 0.05f, 20.0f);
        a->sy = clampf(a->sy, 0.05f, 20.0f);
        if (memcmp(&old, a, sizeof old) != 0) {
            changed = 1;
            s_adj_version++;
            if (old.h != a->h) s_height_version++; /* 5 reset it */
        }
    }
    if (s_sel >= 0 && (keys[SDL_SCANCODE_PAGEUP] || keys[SDL_SCANCODE_PAGEDOWN])) {
        /* This wall's own height, starting from what it has now. */
        Adjust *a = &s_wall_adj[s_sel];
        float h = a->h >= 0 ? a->h : s_walls_list[s_sel].height;
        h += keys[SDL_SCANCODE_PAGEUP] ? hstep : -hstep;
        a->h = clampf(h, 0, 8000);
        s_height_version++;
        s_adj_version++;
        changed = 1;
    } else {
        if (keys[SDL_SCANCODE_PAGEUP]) s_fit.wall += hstep;
        if (keys[SDL_SCANCODE_PAGEDOWN]) s_fit.wall -= hstep;
    }
    if (keys[SDL_SCANCODE_INSERT]) s_fit.obst += hstep;
    if (keys[SDL_SCANCODE_DELETE]) s_fit.obst -= hstep;
    s_fit.sx = clampf(s_fit.sx, 0.1f, 10.0f);
    s_fit.sy = clampf(s_fit.sy, 0.1f, 10.0f);
    if (s_fit.wall < 0) s_fit.wall = 0;
    if (s_fit.wall > 8000) s_fit.wall = 8000;
    if (s_fit.obst < 0) s_fit.obst = 0;
    if (s_fit.obst > 8000) s_fit.obst = 8000;
    if (memcmp(&before, &s_fit, sizeof before) != 0) changed = 1;
    if (changed) {
        s_fit_saved = 0;
        if (s_frame - s_edit_toast > 12) {
            s_edit_toast = s_frame;
            if (s_sel < 0 && (s_fit.wall != before.wall || s_fit.obst != before.obst)) fit_toast();
            else sel_toast();
        }
    }
    if (keys[SDL_SCANCODE_KP_0] && !s_fit_saved) fits_save();
}

/* F1 > Chase camera > Room fit: the same values as live rows. */
static int get_ox(void) { return (int)lrintf(s_fit.ox * 4); }
static int set_ox(int v) { s_fit.ox = v / 4.0f; s_fit_saved = 0; return 1; }
static int get_oy(void) { return (int)lrintf(s_fit.oy * 4); }
static int set_oy(int v) { s_fit.oy = v / 4.0f; s_fit_saved = 0; return 1; }
static int get_sx(void) { return (int)lrintf(s_fit.sx * 1000); }
static int set_sx(int v) { s_fit.sx = v / 1000.0f; s_fit_saved = 0; return 1; }
static int get_sy(void) { return (int)lrintf(s_fit.sy * 1000); }
static int set_sy(int v) { s_fit.sy = v / 1000.0f; s_fit_saved = 0; return 1; }
static int get_wall(void) { return s_fit.wall; }
static int set_wall(int v) { s_fit.wall = v; s_fit_saved = 0; return 1; }
static int get_obst(void) { return s_fit.obst; }
static int set_obst(int v) { s_fit.obst = v; s_fit_saved = 0; return 1; }
static void fit_save_action(void) { fits_save(); }
static const char *const kShapes[] = {"Collision walls", "Picture box"};
static const char *const kRoomShapes[] = {"As the F1 option", "Collision walls", "Picture box"};
static int get_shape(void) { return s_fit.shape + 1; }
static int set_shape(int v) { s_fit.shape = v < 1 ? -1 : v - 1; s_fit_saved = 0; return 1; }
static void fit_reset_action(void) { fit_reset(); fit_toast(); }

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
    if (keys[SDL_SCANCODE_F7] && !s_f7_down) s_dump_pending = 1;
    s_f7_down = keys[SDL_SCANCODE_F7];
    if (keys[SDL_SCANCODE_F9] && !s_f9_down && s_enabled) {
        s_edit = !s_edit;
        if (s_edit)
            pe_toast("Fit editor: numpad . next wall, / floor, * whole room; 4/6/8/2 move, "
                     "+/- size, 7/9 turn, 1/3 width (Ctrl: height), End rectangle, Backspace "
                     "room shape, F5 image, 5 reset, 0 save");
        else {
            if (!s_fit_saved) fits_save();
            pe_toast("Fit editor off");
        }
    }
    s_f9_down = keys[SDL_SCANCODE_F9];
    edit_keys(keys);
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
        fit_select();
        if (s_mesh_key && g_CollisionDb && s_mesh_key == g_CollisionDb->triangles.pointer)
            find_inside_loop();
        else
            s_inside_loop = -1;
        build_mesh();
        memcpy(s_saved_scratch, g_EntityRenderScratch, sizeof s_saved_scratch);
        s_saved_offset_x = (s16)g_GeomScroll.state.position.screenOffsetX;
        s_saved_offset_y = (s16)g_GeomScroll.state.position.screenOffsetY;
        float mirror = room_camera_mirrored() ? -1.0f : 1.0f;
        if (mirror != s_mirror) {
            s_mirror = mirror;
            pe_mod_log(MOD_ID, "room camera is %s", mirror < 0 ? "mirrored" : "a plain rotation");
        }
        if (room_shape() == 1 && s_tile_frame + 2 >= s_frame) build_box();
        build_camera();
        s_cy = s_edit ? 168.0f : 112.0f;
        g_GeomScroll.state.position.screenOffsetX = 160;
        g_GeomScroll.state.position.screenOffsetY = (u16)lrintf(s_cy);
        SetGeomScreen(s_fov);
        draw_room();
        /* Wait (a few frames at most) for tiles read this frame. */
        if (s_dump_pending && (s_tile_frame + 1 >= s_frame || ++s_dump_pending > 6)) {
            s_dump_pending = 0;
            dump_room();
        }
        s_swapped = 1;
        s_drawn_frame = s_frame;
    } else if (s_swapped) {
        memcpy(g_EntityRenderScratch, s_saved_scratch, sizeof s_saved_scratch);
        g_GeomScroll.state.position.screenOffsetX = (u16)s_saved_offset_x;
        g_GeomScroll.state.position.screenOffsetY = (u16)s_saved_offset_y;
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
    if (s_textures || s_dump_pending || s_edit || room_shape() == 1) gather_tiles();
    return 1;
}

static void place_behind(void) { s_place_behind = 1; }
static void request_dump(void) { s_dump_pending = 1; }

static int chasecam_init(void) {
    pe_hook_add(PE_HOOK_FRAME, on_frame, NULL, PE_PRIORITY_NORMAL, MOD_ID);
    pe_hook_add(PE_HOOK_FIELD_CAMERA, on_camera, NULL, PE_PRIORITY_NORMAL, MOD_ID);
    pe_hook_add(PE_HOOK_FIELD_BACKGROUND, on_background, NULL, PE_PRIORITY_NORMAL, MOD_ID);
    SDL_AddEventWatch(on_event, NULL);
    s_hd_registered = Psyz_GpuRegisterCommandHandlerIfVacant(HD_TILE_OPCODE, hd_tile_command, NULL) == 0;
    if (!s_hd_registered) pe_mod_log(MOD_ID, "GPU command %02X is taken: no HD backgrounds", HD_TILE_OPCODE);
    fits_load();

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
    pe_menu_register_choice(0, "Rooms' shape",
                            "Every room without a choice of its own (Backspace): built from the "
                            "picture, or walls on the walkable floor's edges.", &s_default_shape,
                            kShapes, 2);
    pe_menu_register_int(0, "Wall height", "Height of the walls.", &s_wall_height, 200, 4000, 100);
    pe_menu_register_int(0, "Obstacle height",
                         "Height of walls around holes in the floor (seat rows, tables).",
                         &s_obstacle_height, 0, 4000, 100);
    pe_menu_register_action(0, "Debug dump (F7)",
                            "Write the room's picture, where each floor and wall piece takes its "
                            "texture from, and a report, to chasecam_dump beside the settings.",
                            request_dump);
    pe_menu_register_toggle(0, "Camera stops at walls", "Pull the camera in front of walls.",
                            &s_collide, NULL);
    pe_menu_register_toggle(0, "Wall outlines", "Lines along the floor's edges and wall tops.",
                            &s_outlines, NULL);
    pe_menu_page("Room fit", "Line the room's picture up with its walls, per room and camera (F9).", 0);
    pe_menu_register_live_int("Room fit", "Move right", "Picture offset, quarter pixels (numpad 4/6).",
                              -4000, 4000, 1, get_ox, set_ox);
    pe_menu_register_live_int("Room fit", "Move down", "Picture offset, quarter pixels (numpad 8/2).",
                              -4000, 4000, 1, get_oy, set_oy);
    pe_menu_register_live_int("Room fit", "Width x1000", "Picture width (numpad 7/9).", 100, 10000,
                              5, get_sx, set_sx);
    pe_menu_register_live_int("Room fit", "Height x1000", "Picture height (numpad 1/3).", 100,
                              10000, 5, get_sy, set_sy);
    pe_menu_register_live_int("Room fit", "Wall height", "This room's walls (PageUp/PageDown).", 0,
                              8000, 50, get_wall, set_wall);
    pe_menu_register_live_int("Room fit", "Obstacle height",
                              "This room's seat rows and tables (Insert/Delete).", 0, 8000, 50,
                              get_obst, set_obst);
    pe_menu_register_live_choice("Room fit", "Room shape",
                                 "Picture box: built from the room's picture. Collision walls: on "
                                 "the walkable floor's edges (Backspace).",
                                 kRoomShapes, 3, get_shape, set_shape);
    pe_menu_register_action("Room fit", "Save", "Keep this room's fit (numpad 0).", fit_save_action);
    pe_menu_register_action("Room fit", "Reset", "Back to no offset, size 1 (numpad 5).",
                            fit_reset_action);
    pe_menu_begin(0);
    return 0;
}

static void chasecam_shutdown(void) {
    if (!s_fit_saved && s_fit.room >= 0) fits_save();
    SDL_RemoveEventWatch(on_event, NULL);
    if (s_hd_registered) Psyz_GpuRegisterCommandHandler(HD_TILE_OPCODE, NULL, NULL);
    s_hd_registered = 0;
    release_angles();
}

/* ================= debug dump (F7) =================
 * Writes chasecam_dump/room_NNN_FFFFFF/ beside the settings:
 *   picture.bmp  the room's background as the tiles build it, in screen space (the yellow box
 *                is the 320x224 screen), magenta where no tile covers
 *   overlay.bmp  the same picture with every floor triangle and wall quad drawn where the room
 *                camera sees it: green floor / cyan wall = textured, red floor / orange wall =
 *                no texture (outside the picture or behind the room camera); white cross = Aya
 *   map.bmp      the room from above: floor triangles (green textured, dark red not), outer walls
 *                blue, obstacle loops orange, Aya white, the chase camera yellow
 *   report.txt   cameras, background layers (which ones were used and why not), loops with
 *                their heights, per-triangle and per-wall texture coverage
 *   tiles.csv    every background tile: screen position, texture page, CLUT, uv, layer */

typedef struct { int w, h; u8 *rgb; } Canvas;

static int canvas_new(Canvas *c, int w, int h, u8 r, u8 g, u8 b) {
    c->w = w;
    c->h = h;
    c->rgb = malloc((size_t)w * h * 3);
    if (!c->rgb) return 0;
    for (int i = 0; i < w * h; ++i) {
        c->rgb[i * 3] = r;
        c->rgb[i * 3 + 1] = g;
        c->rgb[i * 3 + 2] = b;
    }
    return 1;
}

static void canvas_put(Canvas *c, int x, int y, u8 r, u8 g, u8 b) {
    if (x < 0 || y < 0 || x >= c->w || y >= c->h) return;
    u8 *p = &c->rgb[((size_t)y * c->w + x) * 3];
    p[0] = r;
    p[1] = g;
    p[2] = b;
}

static void canvas_line(Canvas *c, float x0, float y0, float x1, float y1, u8 r, u8 g, u8 b) {
    float dx = x1 - x0, dy = y1 - y0;
    int steps = (int)fmaxf(fabsf(dx), fabsf(dy));
    if (steps > 8000) return;
    if (steps < 1) steps = 1;
    for (int i = 0; i <= steps; ++i)
        canvas_put(c, (int)lrintf(x0 + dx * i / steps), (int)lrintf(y0 + dy * i / steps), r, g, b);
}

static void canvas_fill_tri(Canvas *c, const float *x, const float *y, u8 r, u8 g, u8 b) {
    int minx = (int)floorf(fminf(x[0], fminf(x[1], x[2]))), maxx = (int)ceilf(fmaxf(x[0], fmaxf(x[1], x[2])));
    int miny = (int)floorf(fminf(y[0], fminf(y[1], y[2]))), maxy = (int)ceilf(fmaxf(y[0], fmaxf(y[1], y[2])));
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx >= c->w) maxx = c->w - 1;
    if (maxy >= c->h) maxy = c->h - 1;
    float area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
    if (fabsf(area) < 1e-6f) return;
    for (int py = miny; py <= maxy; ++py)
        for (int px = minx; px <= maxx; ++px) {
            float fx = px + 0.5f, fy = py + 0.5f;
            float w0 = (x[1] - fx) * (y[2] - fy) - (x[2] - fx) * (y[1] - fy);
            float w1 = (x[2] - fx) * (y[0] - fy) - (x[0] - fx) * (y[2] - fy);
            float w2 = (x[0] - fx) * (y[1] - fy) - (x[1] - fx) * (y[0] - fy);
            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0))
                canvas_put(c, px, py, r, g, b);
        }
}

static int canvas_save(const Canvas *c, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    int row = (c->w * 3 + 3) & ~3;
    unsigned size = 54u + (unsigned)row * c->h;
    u8 hdr[54] = {'B', 'M'};
    hdr[2] = size; hdr[3] = size >> 8; hdr[4] = size >> 16; hdr[5] = size >> 24;
    hdr[10] = 54; hdr[14] = 40;
    hdr[18] = c->w; hdr[19] = c->w >> 8; hdr[22] = c->h; hdr[23] = c->h >> 8;
    hdr[26] = 1; hdr[28] = 24;
    fwrite(hdr, 1, 54, f);
    u8 *line = calloc((size_t)row, 1);
    for (int y = c->h - 1; y >= 0; --y) {
        for (int x = 0; x < c->w; ++x) {
            const u8 *p = &c->rgb[((size_t)y * c->w + x) * 3];
            line[x * 3] = p[2];
            line[x * 3 + 1] = p[1];
            line[x * 3 + 2] = p[0];
        }
        fwrite(line, 1, (size_t)row, f);
    }
    free(line);
    fclose(f);
    return 1;
}

static u16 s_vram[1024 * 512];

/* One texel of a tile from the VRAM copy; 0 = transparent. */
static u16 tile_texel(const Tile *t, int i, int j) { return vram_tile_texel(s_vram, t, i, j); }

static int tile_cmp_far_first(const void *a, const void *b) {
    const Tile *ta = &s_tiles[*(const u16 *)a], *tb = &s_tiles[*(const u16 *)b];
    return (int)tb->ot - (int)ta->ot;
}

static int s_dox, s_doy; /* dump canvas origin in screen space */

/* Room camera: world point -> picture canvas. 0 when behind the camera. */
static int room_to_canvas(const V3 *p, float *x, float *y) {
    float v[3];
    room_view(p, v);
    if (v[2] < 16.0f) return 0;
    float H = (float)s_pic_scratch[8];
    *x = s_pic_offset_x + H * v[0] / v[2] - s_dox;
    *y = s_pic_offset_y + H * v[1] / v[2] - s_doy;
    return 1;
}

static void dump_room(void) {
    char base[512], dir[600], path[700];
    if (pe_mod_path(base, sizeof base, "chasecam_dump") != 0) {
        pe_toast("Chase camera: no dump folder");
        return;
    }
    SDL_CreateDirectory(base);
    snprintf(dir, sizeof dir, "%s/room_%03d_%06u", base, pe_room_number(), s_frame);
    SDL_CreateDirectory(dir);

    int stale = s_tile_frame + 2 < s_frame;
    if (s_tile_count > 0) build_pieces();

    RECT all = {0, 0, 1024, 512};
    DrawSync(0);
    StoreImage(&all, s_vram);
    DrawSync(0);

    /* ---- picture ---- */
    int x0 = 0, y0 = 0, x1 = 320, y1 = 224;
    for (int i = 0; i < s_tile_count; ++i) {
        if (s_tiles[i].x < x0) x0 = s_tiles[i].x;
        if (s_tiles[i].y < y0) y0 = s_tiles[i].y;
        if (s_tiles[i].x + 16 > x1) x1 = s_tiles[i].x + 16;
        if (s_tiles[i].y + 16 > y1) y1 = s_tiles[i].y + 16;
    }
    s_dox = x0 - 16;
    s_doy = y0 - 16;
    const int W = x1 - x0 + 32, Hh = y1 - y0 + 32;
    Canvas pic;
    if (!canvas_new(&pic, W, Hh, 80, 0, 80)) return;
    static u16 order[MAX_TILES];
    for (int i = 0; i < s_tile_count; ++i) order[i] = (u16)i;
    qsort(order, (size_t)s_tile_count, sizeof order[0], tile_cmp_far_first);
    for (int k = 0; k < s_tile_count; ++k) {
        const Tile *t = &s_tiles[order[k]];
        for (int j = 0; j < 16; ++j)
            for (int i = 0; i < 16; ++i) {
                u16 c = tile_texel(t, i, j);
                if (!c) continue;
                canvas_put(&pic, t->x - s_dox + i, t->y - s_doy + j, (u8)((c & 31) << 3),
                           (u8)(((c >> 5) & 31) << 3), (u8)(((c >> 10) & 31) << 3));
            }
    }
    canvas_line(&pic, -s_dox, -s_doy, -s_dox + 320, -s_doy, 255, 255, 0);
    canvas_line(&pic, -s_dox, -s_doy + 224, -s_dox + 320, -s_doy + 224, 255, 255, 0);
    canvas_line(&pic, -s_dox, -s_doy, -s_dox, -s_doy + 224, 255, 255, 0);
    canvas_line(&pic, -s_dox + 320, -s_doy, -s_dox + 320, -s_doy + 224, 255, 255, 0);
    snprintf(path, sizeof path, "%s/picture.bmp", dir);
    canvas_save(&pic, path);
    {
        /* All of video memory as 15-bit colour (textures, palettes, screen buffers). */
        Canvas vr;
        if (canvas_new(&vr, 1024, 512, 0, 0, 0)) {
            for (int y = 0; y < 512; ++y)
                for (int x = 0; x < 1024; ++x) {
                    u16 c = s_vram[y * 1024 + x];
                    canvas_put(&vr, x, y, (u8)((c & 31) << 3), (u8)(((c >> 5) & 31) << 3), (u8)(((c >> 10) & 31) << 3));
                }
            snprintf(path, sizeof path, "%s/vram.bmp", dir);
            canvas_save(&vr, path);
            free(vr.rgb);
        }
    }

    /* ---- overlay: each surface outlined where the room camera sees it ---- */
    for (int i = 0; i < s_surf_count; ++i) {
        const Surface *s = &s_surf[i];
        float x[3], y[3];
        int ok = 1;
        for (int k = 0; k < 3; ++k) ok &= room_to_canvas(&s->v[k], &x[k], &y[k]);
        if (!ok) continue;
        int tex = s_surf_pieces[i] > 0;
        u8 r = s->wall ? (tex ? 0 : 255) : (tex ? 0 : 255);
        u8 g = s->wall ? (tex ? 220 : 140) : (tex ? 255 : 40);
        u8 b = s->wall ? (tex ? 255 : 0) : (tex ? 0 : 40);
        for (int k = 0; k < 3; ++k) canvas_line(&pic, x[k], y[k], x[(k + 1) % 3], y[(k + 1) % 3], r, g, b);
    }
    {
        const FieldActor *aya = g_PlayerEntity;
        V3 feet = {(float)(aya->pos_x >> 16), (float)(aya->pos_y >> 16), (float)(aya->pos_z >> 16)};
        float x, y;
        if (room_to_canvas(&feet, &x, &y)) {
            canvas_line(&pic, x - 6, y, x + 6, y, 255, 255, 255);
            canvas_line(&pic, x, y - 6, x, y + 6, 255, 255, 255);
        }
    }
    snprintf(path, sizeof path, "%s/overlay.bmp", dir);
    canvas_save(&pic, path);
    free(pic.rgb);

    /* ---- map from above ---- */
    float minx = 1e9f, maxx = -1e9f, minz = 1e9f, maxz = -1e9f;
    for (int i = 0; i < s_floor_surf_count; ++i)
        for (int k = 0; k < 3; ++k) {
            const V3 *v = &s_surf[i].v[k];
            minx = fminf(minx, v->x); maxx = fmaxf(maxx, v->x);
            minz = fminf(minz, v->z); maxz = fmaxf(maxz, v->z);
        }
    if (minx < maxx && minz < maxz) {
        float span = fmaxf(maxx - minx, maxz - minz);
        float scale = 1000.0f / span;
        int mw = (int)((maxx - minx) * scale) + 40, mh = (int)((maxz - minz) * scale) + 40;
        Canvas map;
        if (canvas_new(&map, mw, mh, 20, 20, 24)) {
#define MX(px) (20.0f + ((px) - minx) * scale)
#define MZ(pz) (20.0f + ((pz) - minz) * scale)
            for (int i = 0; i < s_floor_surf_count; ++i) {
                const Surface *s = &s_surf[i];
                float x[3], y[3];
                for (int k = 0; k < 3; ++k) { x[k] = MX(s->v[k].x); y[k] = MZ(s->v[k].z); }
                int tex = s_surf_pieces[i] > 0;
                canvas_fill_tri(&map, x, y, tex ? 40 : 110, tex ? 120 : 30, tex ? 50 : 30);
            }
            for (int i = 0; i < s_wall_count; ++i) {
                const WallEdge *w = &s_walls_list[i];
                int outer = w->loop >= 0 && w->loop < s_loop_count && s_loops[w->loop].outer;
                canvas_line(&map, MX(w->a.x), MZ(w->a.z), MX(w->b.x), MZ(w->b.z),
                            outer ? 80 : 255, outer ? 140 : 150, outer ? 255 : 0);
            }
            /* The scene's floor polygon, magenta. */
            for (unsigned i = 0; g_FloorPolygonVertices && i < g_FloorPolygonVertexCount; ++i) {
                const PolygonVertex *a = &g_FloorPolygonVertices[i];
                const PolygonVertex *b = &g_FloorPolygonVertices[(i + 1) % g_FloorPolygonVertexCount];
                canvas_line(&map, MX(a->x), MZ(a->z), MX(b->x), MZ(b->z), 255, 0, 255);
            }
            const FieldActor *aya = g_PlayerEntity;
            float ax = MX((float)(aya->pos_x >> 16)), az = MZ((float)(aya->pos_z >> 16));
            canvas_line(&map, ax - 5, az, ax + 5, az, 255, 255, 255);
            canvas_line(&map, ax, az - 5, ax, az + 5, 255, 255, 255);
            float cx = MX(s_cam[0]), cz = MZ(s_cam[2]);
            canvas_line(&map, cx, cz, ax, az, 255, 230, 0);
#undef MX
#undef MZ
            snprintf(path, sizeof path, "%s/map.bmp", dir);
            canvas_save(&map, path);
            free(map.rgb);
        }
    }

    /* ---- tiles.csv ---- */
    snprintf(path, sizeof path, "%s/tiles.csv", dir);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "index,x,y,u,v,tpage,clut,depth_bits,ot,code,r,g,b\n");
        for (int i = 0; i < s_tile_count; ++i) {
            const Tile *t = &s_tiles[i];
            fprintf(f, "%d,%d,%d,%u,%u,0x%03X,0x%04X,%d,%u,0x%02X,%u,%u,%u\n", i, t->x, t->y, t->u,
                    t->v, t->tpage, t->clut, (t->tpage >> 7) & 3, t->ot, t->code, t->r, t->g, t->b);
        }
        fclose(f);
    }

    /* ---- report.txt ---- */
    snprintf(path, sizeof path, "%s/report.txt", dir);
    f = fopen(path, "w");
    if (!f) {
        pe_toast("Chase camera: dump failed");
        return;
    }
    const GteMatrix *m = (const GteMatrix *)s_saved_scratch;
    const FieldActor *aya = g_PlayerEntity;
    fprintf(f, "room %d  frame %u\n", pe_room_number(), s_frame);
    fprintf(f, "aya pos %d %d %d  rot_y %d\n", aya->pos_x >> 16, aya->pos_y >> 16, aya->pos_z >> 16,
            aya->rot_y);
    fprintf(f, "room camera: rows (%d %d %d) (%d %d %d) (%d %d %d)  t %d %d %d  H %u  %s\n",
            m->m[0][0], m->m[0][1], m->m[0][2], m->m[1][0], m->m[1][1], m->m[1][2], m->m[2][0],
            m->m[2][1], m->m[2][2], m->t[0], m->t[1], m->t[2], s_saved_scratch[8],
            s_mirror < 0 ? "mirrored" : "rotation");
    fprintf(f, "screen offset %d %d  scroll x %d y %d  group %u\n", s_saved_offset_x,
            s_saved_offset_y, g_GeomScroll.state.position.x, g_GeomScroll.state.position.y,
            g_GeomScroll.state.position.groupSelect);
    fprintf(f, "chase camera: yaw %.3f pitch %.3f distance %.0f (want %d) pos %.0f %.0f %.0f  fov %d\n",
            s_yaw, s_pitch, s_cur_distance, s_distance, s_cam[0], s_cam[1], s_cam[2], s_fov);
    fprintf(f, "settings: textures %d solid_walls %d wall_height %d obstacle_height %d\n",
            s_textures, s_solid_walls, s_wall_height, s_obstacle_height);
    fprintf(f, "room fit (camera %d): move %.2f %.2f size %.4f x %.4f wall %d obstacle %d%s\n",
            s_fit.group, s_fit.ox, s_fit.oy, s_fit.sx, s_fit.sy, s_fit.wall, s_fit.obst,
            s_fit_saved ? "" : " (not saved)");
    for (int i = 0; i < s_image_count; ++i) {
        const CustomImage *im = &s_images[i];
        fprintf(f, "image %s: %s %dx%d chunks %d at x %d y %d palette %d,%d\n", im->name,
                im->loaded ? "loaded" : im->failed ? "FAILED" : "not used", im->w, im->h, im->chunks,
                im->vx[0], im->vy[0], im->cx, im->cy);
    }
    for (int i = 0; i < s_wall_count && i < MAX_WALLS; ++i)
        if (s_wall_img[i] >= 0)
            fprintf(f, "wall %d image %s span %d\n", i, s_images[s_wall_img[i]].name, s_wall_span[i]);
    fprintf(f, "room shape: %s  box x %.0f..%.0f z %.0f..%.0f floor %.0f\n",
            room_shape() ? (s_fit.shape < 0 ? "picture box (F1 option)" : "picture box") :
            (s_fit.shape < 0 ? "collision walls (F1 option)" : "collision walls"), s_box[0], s_box[1], s_box[2], s_box[3],
            s_floor_y);
    fprintf(f, "floor layer: move %.2f %.2f size %.4f x %.4f turn %.4f\n", s_floor_adj.ox,
            s_floor_adj.oy, s_floor_adj.sx, s_floor_adj.sy, s_floor_adj.rot);
    for (int i = 0; i < s_wall_count && i < MAX_WALLS; ++i)
        if (!adjust_is_identity(&s_wall_adj[i]))
            fprintf(f, "wall %d layer: move %.2f %.2f size %.4f x %.4f turn %.4f height %.0f\n", i,
                    s_wall_adj[i].ox, s_wall_adj[i].oy, s_wall_adj[i].sx, s_wall_adj[i].sy,
                    s_wall_adj[i].rot, s_wall_adj[i].h);
    fprintf(f, "floor polygon (scene command 0x2E): %u vertices", g_FloorPolygonVertexCount);
    for (unsigned i = 0; g_FloorPolygonVertices && i < g_FloorPolygonVertexCount && i < 64; ++i)
        fprintf(f, " (%d,%d)", g_FloorPolygonVertices[i].x, g_FloorPolygonVertices[i].z);
    fprintf(f, "\n");
    fprintf(f, "tiles %d%s, pieces %d, surfaces %d floor + %d wall\n\n", s_tile_count,
            stale ? " (STALE: not gathered this frame)" : "", s_piece_count, s_floor_surf_count,
            s_surf_count - s_floor_surf_count);

    GeomState *state = g_GeomState;
    if (state) {
        fprintf(f, "background layers (entries): %u, disp_src %d %d, field26 %d\n",
                state->entry_count06, state->disp_src_x, state->disp_src_y, state->field26);
        GeomEntry *entries = (GeomEntry *)((u8 *)state + state->entry_offset);
        for (unsigned e = 0; e < state->entry_count06; ++e) {
            GeomEntry *en = &entries[e];
            const char *why = "used";
            if (!(en->head.b.flags & 2)) why = "SKIPPED: hidden";
            else if (en->group != g_GeomScroll.state.position.groupSelect) why = "skipped: other camera group";
            else if (en->head.b.flags & 4) why = "SKIPPED: scrolling/wrapped layer (not supported yet)";
            else if (!en->u30.prim || !en->u34.pagePackets) why = "SKIPPED: no packets";
            fprintf(f, "  entry %2u flags %02X group %u tiles %3u scr %d,%d mod %u,%u ot %u  %s\n",
                    e, en->head.b.flags, en->group, en->prim_count, (s16)en->scr_x, (s16)en->scr_y,
                    en->anim_mod_x, en->anim_mod_y,
                    (unsigned)(state->field26 + ((*(const u32 *)en >> 8) & 0xFFF)), why);
        }
    }

    fprintf(f, "\nwall loops: %d (outer = the floor's outline, obstacle = a hole in the floor); "
            "Aya stands in loop %d (a platform, no walls)\n", s_loop_count, s_inside_loop);
    for (int i = 0; i < s_loop_count; ++i) {
        const WallLoop *l = &s_loops[i];
        fprintf(f, "  loop %3d %-8s edges %3d area %10.0f  x %.0f..%.0f z %.0f..%.0f  height %d\n",
                i, l->outer ? "outer" : "obstacle", l->count, l->area, l->minx, l->maxx, l->minz,
                l->maxz, l->outer ? s_fit.wall : s_fit.obst);
    }

    /* Per source triangle / wall edge: how many of its parts got a texture. */
    fprintf(f, "\nfloor triangles (index: textured parts / parts, vertices x,y,z)\n");
    {
        int last = -1, parts = 0, tex = 0, total_tex = 0, total = 0;
        for (int i = 0; i <= s_floor_surf_count; ++i) {
            int src = i < s_floor_surf_count ? (s_surf[i].src == 0xFFFF ? -3 : s_surf[i].src) : -2;
            if (src != last && last >= 0) {
                fprintf(f, "  tri %4d: %d/%d\n", last, tex, parts);
                total_tex += tex > 0;
                total++;
                parts = tex = 0;
            }
            if (i == s_floor_surf_count) break;
            if (src == -3) continue; /* platform floor under Aya, not a collision triangle */
            last = src;
            parts++;
            tex += s_surf_pieces[i] > 0;
        }
        fprintf(f, "  %d of %d triangles have some texture\n", total_tex, total);
    }
    fprintf(f, "\nwall edges (index loop: textured parts / parts, from -> to)\n");
    {
        static u16 parts[MAX_WALLS], tex[MAX_WALLS];
        memset(parts, 0, sizeof parts);
        memset(tex, 0, sizeof tex);
        for (int i = s_floor_surf_count; i < s_surf_count; ++i) {
            int e = s_surf[i].src;
            if (e >= MAX_WALLS) continue;
            parts[e]++;
            tex[e] += s_surf_pieces[i] > 0;
        }
        for (int i = 0; i < s_wall_count; ++i) {
            const WallEdge *w = &s_walls_list[i];
            fprintf(f, "  edge %4d loop %3d: %u/%u  (%.0f,%.0f,%.0f) -> (%.0f,%.0f,%.0f)\n", i,
                    w->loop, tex[i], parts[i], w->a.x, w->a.y, w->a.z, w->b.x, w->b.y, w->b.z);
        }
    }
    fclose(f);
    char msg[160];
    snprintf(msg, sizeof msg, "Chase camera dump: room_%03d_%06u", pe_room_number(), s_frame);
    pe_toast(msg);
    pe_mod_log(MOD_ID, "dump written to %s", dir);
}

const PeMod pe_mod_pe_chasecam = {MOD_ID, "Chase camera", "0.6", NULL, chasecam_init,
                                  chasecam_shutdown};
