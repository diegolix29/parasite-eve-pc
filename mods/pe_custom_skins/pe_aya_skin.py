"""PE Aya Skin: turn your own 3D model into a Parasite Eve PC skin for Aya (a Wardrobe costume).

Runs inside Blender (4.2 or newer; tested on 5.3). Install it as an add-on (Edit > Preferences >
Add-ons > Install from Disk, pick this file), then open the 3D view's sidebar (N) > "PE Aya Skin".
It can also run without the window:

    blender -b -P pe_aya_skin.py -- --disc "Parasite Eve (USA) (Disc 1).bin" --model my_aya.fbx \
            --out "%APPDATA%/Parasite Eve/pe_skins/my_aya" --name "Aya: My look"

Read GUIDE.md beside this file first. In short:
  1. Load reference   the game's skeleton (PE_Aya_Rig) and Aya's own mesh (PE_Aya_Reference), read
                      from YOUR disc 1 image. Nothing from the game ships with this tool.
  2. Fit size         your model scaled and moved onto the reference (same height, feet on the floor).
     Clean up         optional: find / remove faces hidden inside the model (a body under clothes,
                      the scalp under hair, the inside of the mouth); they flicker through in game.
  3. Pose             a model already standing like the reference: skip. A T-pose / A-pose model:
                      "Make fitting rig", turn its bones into your model's limbs (Pose mode), then
                      "Pose into game stance": the model is bent into the game's stance.
  4. Assign bones     every vertex gets one of the game's bones (vertex groups named like the bones).
                      The game moves each vertex with exactly one bone: fix mistakes in Edit mode
                      (select vertices, Object Data > Vertex Groups > Assign).
  5. Preview          play the game's own animations on your model to find bad assignments.
  6. Export           writes the skin folder (model.bin, texture.bin, day1-8.bin, meta.bin, skin.ini).

The skin is built in the game's own player formats on top of the game's bundle from your disc, so it
uses all of Aya's animations. Limits of the stock game (see GUIDE.md): about 1,100 quads / 1,300
triangles, one 256x256 texture page with 16 colours per palette (32 palettes).
"""
bl_info = {
    "name": "PE Aya Skin",
    "author": "Parasite Eve PC modders",
    "version": (1, 0, 0),
    "blender": (4, 2, 0),
    "location": "View3D > Sidebar > PE Aya Skin",
    "description": "Make Aya skins (Wardrobe costumes) for the Parasite Eve PC port from your own model",
    "category": "Import-Export",
}

import math
import os
import struct
import sys

import numpy as np

try:
    import bpy
    import bmesh
    from mathutils import Matrix, Vector
    HAVE_BPY = True
except ImportError:                     # the disc / format half also works in plain Python
    HAVE_BPY = False

# =============================================================================================
# Disc and game formats (no Blender needed)
# =============================================================================================
# PE.IMG file table (sector runs): Scene asset index i covers sectors T[i] .. T[i+1].
T = [0, 71, 72, 87, 121, 126, 131, 153, 156, 157, 180, 197, 200, 203, 220, 237, 254, 271, 288, 316, 340, 368,
     396, 428, 434, 442, 448, 456, 464, 472, 478, 483, 489, 497, 503, 510, 519, 526, 532, 537, 539]
BANKS = {12: "Jacket (main gameplay outfit)", 13: "T-shirt", 10: "Evening dress"}
SC = 272.0                      # game units per metre
QUAD_KIND, TRI_KIND = 0x0A, 0x0F
BLOCK = 0x47E0 * 2              # one entity pool block (36,800 bytes)
GUN_PART = 0x12                 # the part the weapon follows (Aya's right hand)
IDLE_ROW = 0x15

NAMES = ['root', 'chest', 'Larm_base', 'L_clav', 'L_upperarm', 'L_forearm', 'L_hand', 'L_f1', 'L_f2', 'L_f3',
         'neck', 'head', 'Rarm_base', 'R_clav', 'R_upperarm', 'R_forearm', 'R_hand', 'R_f1', 'R_f2_gun', 'R_f3',
         'pelvis', 'L_hipbase', 'L_thigh', 'L_shin', 'L_foot', 'R_hipbase', 'R_thigh', 'R_shin', 'R_foot',
         'R_extra_base', 'R_extra']
# bones the automatic assignment uses (the clavicles exist but are tiny: assign them by hand if wanted)
AUTO = (1, 4, 5, 6, 10, 11, 14, 15, 16, 20, 22, 23, 24, 26, 27, 28)
LEFT, RIGHT = (4, 5, 6, 22, 23, 24), (14, 15, 16, 26, 27, 28)
ARMS = (4, 5, 6, 14, 15, 16)
# bone lengths (offset from the parent joint) the fitting rig may change: shoulders, elbows, wrists,
# neck, head, hips, knees, ankles (not the fingers). Not the hidden arm bases, clavicles and hip bases:
# a joint only slides along its parent's axis, so a shoulder the skeleton can't reach (a model's
# shoulders 3.5 cm more forward than Aya's) made the fit stretch them (arm base -79 -> -97) and lifted
# both shoulders 4 cm, unevenly. What the skeleton can't reach is kept by LIMB_ROOTS instead.
LENGTHS = (4, 5, 6, 10, 11, 14, 15, 16, 22, 23, 24, 26, 27, 28)
# where a limb meets the torso: its skin stays where the model has it relative to the torso, rather
# than being carried onto the game's joint (shoulders, neck, hips)
LIMB_ROOTS = (4, 14, 10, 22, 26)
# farthest a vertex may be from an arm bone (metres, at the game's size): sleeves, cuffs, a closed hand
ARM_REACH = {4: 0.13, 5: 0.10, 6: 0.08, 14: 0.13, 15: 0.10, 16: 0.08}
LEG_REACH = {22: 0.11, 23: 0.08, 24: 0.085, 26: 0.11, 27: 0.08, 28: 0.085}   # feet: a shoe, not a dress hem

# The game's 240 vertex normals (g_RenderNormalTable, src/main/render/object.c); a vertex stores an index.
NORMAL_TABLE = [
    4013, -70, 818, 4013, -701, 429, 4013, 701, -429, 4013, 70, -818, 4011, 705, 436, 4011, -705, -436, 3853, -1182, -730, 3853, 1182, 730,
    3727, 70, -1697, 3727, -1486, 821, 3727, -70, 1697, 3727, 1486, -821, 3547, 1182, 1674, 3547, -1182, -1674, 3547, -2026, -308, 3547, 2026, 308,
    3501, 705, 2005, 3501, -705, -2005, 3501, -2109, 266, 3501, 2109, -266, 3498, 1486, -1526, 3498, 701, -2012, 3498, -701, 2012, 3498, -1486, 1526,
    3051, 2547, 991, 3051, 2026, 1835, 3051, -2547, -991, 3051, -2026, -1835, 2995, -2733, 583, 2995, 701, 2705, 2995, 2733, -583, 2995, -701, -2705,
    2989, -705, 2710, 2989, 705, -2710, 2989, -2109, 1843, 2989, 2109, -1843, 2765, 70, 3021, 2765, -70, -3021, 2765, -2733, 1288, 2765, 2733, -1288,
    2688, -2026, 2334, 2688, -1182, 2856, 2688, 1182, -2856, 2688, 2026, -2334, 2676, 2109, 2273, 2676, 2976, 870, 2676, -2976, -870, 2676, -2109, -2273,
    2533, -3219, -52, 2533, 3219, 52, 2533, -1486, -2855, 2533, 1486, 2855, 2080, 2733, 2232, 2080, 3219, 1446, 2080, -3219, -1446, 2080, -2733, -2232,
    2018, -3219, 1531, 2018, 70, 3564, 2018, -70, -3564, 2018, 3219, -1531, 1933, 1486, 3291, 1933, -1486, -3291, 1933, 3608, -143, 1933, -3608, 143,
    1885, -1182, 3439, 1885, 1182, -3439, 1885, -2547, 2595, 1885, 2547, -2595, 1654, 705, -3680, 1654, -2976, 2277, 1654, 2976, -2277, 1654, -705, 3680,
    1648, -701, -3684, 1648, -3608, 1021, 1648, 701, 3684, 1648, 3608, -1021, 1480, -2733, -2668, 1480, 2733, 2668, 1480, 3608, 1251, 1480, -3608, -1251,
    1389, 2026, -3278, 1389, -2026, 3278, 1335, 2109, 3248, 1335, -2109, -3248, 1335, -3848, -434, 1335, 3848, 434, 832, 3219, -2393, 832, -3219, 2393,
    832, 701, -3949, 832, -701, 3949, 829, 2109, -3412, 829, -2109, 3412, 825, 3848, -1135, 825, -3848, 1135, 825, 705, 3950, 825, -705, -3950,
    803, -4008, -261, 803, 4008, 261, 803, 2026, 3468, 803, -2026, -3468, 733, -3608, -1794, 733, -3219, -2425, 733, 3608, 1794, 733, 3219, 2425,
    496, -4008, 683, 496, 4008, -683, 496, 1182, 3890, 496, -1182, -3890, 462, 70, -4069, 462, -70, 4069, 462, 3608, -1882, 462, -3608, 1882,
    371, 2733, -3028, 371, 1486, -3799, 371, -2733, 3028, 371, -1486, 3799, 0, -2976, -2814, 0, -2547, -3208, 0, 2547, 3208, 0, 2976, 2814,
    0, 3848, 1403, 0, 4008, 844, 0, -4008, -844, 0, -3848, -1403, -371, -1486, 3799, -371, 1486, -3799, -371, -2733, 3028, -371, 2733, -3028,
    -462, 3608, -1882, -462, -3608, 1882, -462, -70, 4069, -462, 70, -4069, -496, 1182, 3890, -496, 4008, -683, -496, -4008, 683, -496, -1182, -3890,
    -733, 3219, 2425, -733, 3608, 1794, -733, -3608, -1794, -733, -3219, -2425, -803, -2026, -3468, -803, 2026, 3468, -803, 4008, 261, -803, -4008, -261,
    -825, 705, 3950, -825, -705, -3950, -825, 3848, -1135, -825, -3848, 1135, -829, -2109, 3412, -829, 2109, -3412, -832, -701, 3949, -832, 701, -3949,
    -832, -3219, 2393, -832, 3219, -2393, -1335, 3848, 434, -1335, -3848, -434, -1335, 2109, 3248, -1335, -2109, -3248, -1389, -2026, 3278, -1389, 2026, -3278,
    -1480, 3608, 1251, -1480, -3608, -1251, -1480, 2733, 2668, -1480, -2733, -2668, -1648, -3608, 1021, -1648, 3608, -1021, -1648, 701, 3684, -1648, -701, -3684,
    -1654, -2976, 2277, -1654, 2976, -2277, -1654, -705, 3680, -1654, 705, -3680, -1885, -2547, 2595, -1885, 2547, -2595, -1885, -1182, 3439, -1885, 1182, -3439,
    -1933, 3608, -143, -1933, -3608, 143, -1933, 1486, 3291, -1933, -1486, -3291, -2018, -3219, 1531, -2018, 3219, -1531, -2018, 70, 3564, -2018, -70, -3564,
    -2080, 2733, 2232, -2080, -2733, -2232, -2080, -3219, -1446, -2080, 3219, 1446, -2533, 1486, 2855, -2533, -3219, -52, -2533, -1486, -2855, -2533, 3219, 52,
    -2676, 2109, 2273, -2676, -2109, -2273, -2676, -2976, -870, -2676, 2976, 870, -2688, -1182, 2856, -2688, 2026, -2334, -2688, -2026, 2334, -2688, 1182, -2856,
    -2765, 70, 3021, -2765, -2733, 1288, -2765, 2733, -1288, -2765, -70, -3021, -2989, -705, 2710, -2989, -2109, 1843, -2989, 2109, -1843, -2989, 705, -2710,
    -2995, -2733, 583, -2995, 2733, -583, -2995, -701, -2705, -2995, 701, 2705, -3051, -2026, -1835, -3051, 2026, 1835, -3051, 2547, 991, -3051, -2547, -991,
    -3498, 701, -2012, -3498, 1486, -1526, -3498, -701, 2012, -3498, -1486, 1526, -3501, 705, 2005, -3501, -705, -2005, -3501, -2109, 266, -3501, 2109, -266,
    -3547, 2026, 308, -3547, -2026, -308, -3547, 1182, 1674, -3547, -1182, -1674, -3727, -1486, 821, -3727, 1486, -821, -3727, 70, -1697, -3727, -70, 1697,
    -3853, 1182, 730, -3853, -1182, -730, -4011, 705, 436, -4011, -705, -436, -4013, -70, 818, -4013, -701, 429, -4013, 701, -429, -4013, 70, -818,
]

u16 = lambda b, a: struct.unpack_from('<H', b, a)[0]
s16 = lambda b, a: struct.unpack_from('<h', b, a)[0]
u32 = lambda b, a: struct.unpack_from('<I', b, a)[0]


def bone_name(p):
    return '%02d_%s' % (p, NAMES[p] if p < len(NAMES) else 'part')


def part_of_name(name):
    """'06_L_hand' -> 6, anything else -> None."""
    if len(name) > 3 and name[:2].isdigit() and name[2] == '_':
        return int(name[:2])
    return None


class SkinError(Exception):
    pass


class Disc:
    """Parasite Eve disc 1 image: raw .bin (2352-byte sectors) or .iso (2048). Reads PE.IMG."""

    def __init__(self, path):
        if not path or not os.path.isfile(path):
            raise SkinError('Disc image not found: %r (use the .bin of disc 1, not the .cue)' % path)
        self.f = open(path, 'rb')
        self.size, self.skip = None, None
        for size, skip in ((2352, 24), (2048, 0), (2352, 16)):
            self.f.seek(16 * size + skip)
            if self.f.read(6)[1:6] == b'CD001':
                self.size, self.skip = size, skip
                break
        if self.size is None:
            raise SkinError('Not a CD image (no ISO 9660 volume): %s' % path)
        pvd = self.sector(16)
        root_lba, root_size = u32(pvd, 158), u32(pvd, 166)
        data = b''.join(self.sector(root_lba + i) for i in range((root_size + 2047) // 2048))
        p, self.img = 0, None
        while p < len(data):
            n = data[p]
            if n == 0:
                p = (p // 2048 + 1) * 2048
                continue
            if data[p + 33:p + 33 + data[p + 32]].decode('ascii', 'replace').upper() == 'PE.IMG;1':
                self.img = u32(data, p + 2)
                break
            p += n
        if self.img is None:
            raise SkinError('PE.IMG not found: is this Parasite Eve disc 1?')

    def sector(self, lba):
        self.f.seek(lba * self.size + self.skip)
        return self.f.read(2048)

    def asset(self, i):
        return b''.join(self.sector(self.img + s) for s in range(T[i], T[i + 1]))

    def bank(self, bank):
        """(texture blob, model bundle) of a player bank."""
        return self.asset(bank + 3), self.asset(bank + 8)

    def day(self, bank, d):
        return self.asset((bank - 10) // 2 * 8 + d + 0x16)


def records(b, field):
    d = u32(b, 4)
    desc = u32(b, d + field)
    return desc & 0x3FFFFF, desc >> 22


def root_record(b):
    """(where the root record's word is, model block offset)."""
    at, n = records(b, 0x0C)
    for i in range(n):
        w = u32(b, at + 12 * i + 4)
        if w >> 24 == 0:
            return at + 12 * i + 4, w & 0xFFFFFF
    raise SkinError('model bundle has no root record')


def game_rows(b):
    """{row id: offset} of a model bundle or day blob."""
    at, n = records(b, 0x10)
    out = {}
    for i in range(n):
        w = u32(b, at + 12 * i + 4)
        out.setdefault(w >> 24, w & 0xFFFFFF)
    return out


def parse_block(b, off):
    h = bytes(b[off:off + 0x1C])
    parts, verts = h[2], u16(h, 6)
    pk = [u16(h, 8 + 2 * k) for k in range(4)]
    cmds = u16(h, 0x18)
    nf = sum(pk)
    a = off + 0x1C
    B = dict(header=bytearray(h), pk=pk, ncmds=cmds)
    B['parts'] = [bytearray(b[a + 12 * i:a + 12 * i + 12]) for i in range(parts)]; a += 12 * parts
    B['verts'] = [list(struct.unpack_from('<4h', b, a + 8 * i)) for i in range(verts)]; a += 8 * verts
    B['colours'] = [u32(b, a + 4 * i) for i in range(verts)]; a += 4 * verts
    B['desc'] = [bytearray(b[a + 12 * i:a + 12 * i + 12]) for i in range(nf)]; a += 12 * nf
    nb = 2 + 2 * parts + 1
    B['bounds'] = [list(struct.unpack_from('<4h', b, a + 8 * i)) for i in range(nb)]; a += 8 * nb
    B['cmds'] = bytes(b[a:a + ((cmds + 3) & ~3)]); a += (cmds + 3) & ~3
    B['packets'] = []
    for i in range(pk[0] + pk[1]):
        ln = 16 if i < pk[0] else 12
        B['packets'].append(bytearray(b[a:a + ln])); a += ln
    B['end'] = a
    return B


def write_block(B):
    out = bytearray(B['header'])
    for p in B['parts']: out += p
    for v in B['verts']: out += struct.pack('<4h', *v)
    for c in B['colours']: out += struct.pack('<I', c)
    for d in B['desc']: out += d
    for bd in B['bounds']: out += struct.pack('<4h', *bd)
    out += B['cmds']
    for p in B['packets']: out += p
    return out


def rot_game(a):
    x, y, z = (t * 2 * math.pi / 4096 for t in a)
    cx, sx, cy, sy, cz, sz = math.cos(x), math.sin(x), math.cos(y), math.sin(y), math.cos(z), math.sin(z)
    ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
    rx = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]])
    rz = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])
    return ry @ rx @ rz


def row_frame(b, off, nparts, fr):
    """One frame of an animation row: (root translation, [part angles])."""
    flags, last, frames = b[off], b[off + 1], b[off + 2]
    fr = min(fr, max(frames - 1, 0))
    stride = ((frames >> 1) + 1) * 4
    bstride = stride if (flags & 3) == 2 else ((frames >> 2) + 1) * 4
    cur = off + 12
    tr = []
    for k in range(3):
        const = s16(b, cur) != 0
        tr.append(s16(b, cur + 2 if const else cur + 2 + 2 * fr)); cur += 4 if const else stride
    ang = [[0, 0, 0] for _ in range(nparts)]
    shorts = (flags & 3) == 2
    for p in range(last + 1):
        for k in range(3):
            const = (s16(b, cur) != 0) if shorts else (b[cur] != 0)
            a = u16(b, cur + 2 if const else cur + 2 + 2 * fr) if shorts else b[cur + 1 if const else cur + 1 + fr] * 16
            cur += 4 if const else bstride
            if p < nparts: ang[p][k] = a
    return tr, ang


def row_frames(b, off):
    return max(1, b[off + 2])


class GameAya:
    """The game's Aya for one bank, decoded from the disc: skeleton, idle pose, mesh."""

    def __init__(self, disc_path, bank):
        if bank not in BANKS:
            raise SkinError('bank must be one of %s' % sorted(BANKS))
        self.disc_path, self.bank = disc_path, bank
        self.disc = Disc(disc_path)
        self.tex, self.model = self.disc.bank(bank)
        self.rec_at, self.off = root_record(self.model)
        G = self.G = parse_block(self.model, self.off)
        if write_block(G) != self.model[self.off:G['end']]:
            raise SkinError('the game model did not decode cleanly: unsupported disc version?')
        self.NP = NP = len(G['parts'])
        if NP < 29:
            raise SkinError('unexpected player model (%d parts): use the USA disc 1' % NP)
        self.visible = [G['parts'][p][4] == 1 for p in range(NP)]
        self.oz = [struct.unpack_from('<i', G['parts'][p], 8)[0] for p in range(NP)]
        self.pstart = [u16(G['parts'][p], 0) for p in range(NP)]
        self.pcount = [u16(G['parts'][p], 2) for p in range(NP)]
        self.vpart = np.zeros(len(G['verts']), int)
        for p in range(NP):
            self.vpart[self.pstart[p]:self.pstart[p] + self.pcount[p]] = p
        self.commands = [c - 256 if c > 127 else c for c in G['cmds'][:G['ncmds']]]
        self.parent = [-1] * NP
        stack, cur = [], -1
        for c in self.commands:
            if c == -1: stack.append(cur); continue
            if c == -2: cur = stack.pop(); continue
            self.parent[c] = cur; cur = c
        rows = game_rows(self.model)
        if IDLE_ROW not in rows:
            raise SkinError('no idle animation in the bundle')
        self.idle_frame = row_frame(self.model, rows[IDLE_ROW], NP, 0)
        self.oz_game = list(self.oz)
        self.idle = self.pose(*self.idle_frame)
        nt = np.array(NORMAL_TABLE, float).reshape(-1, 3)
        self.NT = nt / np.linalg.norm(nt, axis=1, keepdims=True)
        self._conventions()

    def pose(self, tr, ang):
        """[(R, t)] per part in game space for one frame (the game's own hierarchy walk)."""
        cur = (np.eye(3), np.zeros(3)); stack = []; mats = [None] * self.NP
        for c in self.commands:
            if c == -1: stack.append(cur); continue
            if c == -2: cur = stack.pop(); continue
            r = cur[0] @ rot_game(ang[c])
            v = np.array(tr, float) if c == 0 else np.array([0, 0, self.oz[c]], float)
            mats[c] = (r, cur[0] @ v + cur[1]); cur = mats[c]
        return mats

    def set_oz(self, oz=None):
        """Bone lengths (each part's offset along its parent's z): the game's own (None) or a skin's.
        Animations carry only rotations, so a skin may stretch or shorten limbs."""
        oz = list(self.oz_game if oz is None else oz)
        if oz != self.oz:
            self.oz = oz
            self.idle = self.pose(*self.idle_frame)

    def idle_vertices(self):
        """The game mesh's vertices in the idle pose, game units."""
        out = np.zeros((len(self.G['verts']), 3))
        for i, v in enumerate(self.G['verts']):
            R, t = self.idle[self.vpart[i]]
            out[i] = R @ np.array(v[:3], float) + t
        return out

    def faces(self):
        """[(vertex indices in drawing order, is quad)] of the game mesh."""
        pk = self.G['pk']
        out = []
        for i, d in enumerate(self.G['desc']):
            quad = i < pk[0] or (pk[0] + pk[1] <= i < pk[0] + pk[1] + pk[2])
            out.append(([u16(d, 4 + 2 * k) for k in range(4 if quad else 3)], quad))
        return out

    def _conventions(self):
        """Normal sign and front-face winding, measured on the game's own model."""
        G = self.G
        s_rad = 0.0
        for v in G['verts']:
            n = self.NT[v[3]]
            s_rad += np.sign(n[0] * v[0] + n[1] * v[1])
        self.normal_sign = 1.0 if s_rad > 0 else -1.0
        W = self.idle_vertices()
        s_wind = 0.0
        for d in G['desc']:
            idx = [u16(d, 4 + 2 * k) for k in range(3)]
            fn = np.cross(W[idx[1]] - W[idx[0]], W[idx[2]] - W[idx[0]])
            nv = sum(self.idle[self.vpart[x]][0] @ self.NT[G['verts'][x][3]] for x in idx) * self.normal_sign
            s_wind += np.sign(fn @ nv)
        self.order_outward = s_wind > 0

    def tim_records(self):
        b = self.tex
        at, n = records(b, 0x28)
        out = []
        for i in range(n):
            r = at + 20 * i
            size, src, rect = u32(b, r), u32(b, r + 4), u32(b, r + 8)
            out.append(dict(size=size & 0xFFFFFF, off=src & 0xFFFFFF, x=(rect >> 10) & 0x7FF, y=rect >> 21, w=rect & 0x3FF))
        return out


# --- coordinate change: game (y down, z forward) <-> Blender (z up), metres
G2B = np.array([[1, 0, 0], [0, 0, 1], [0, -1, 0]], float)


def to_blender(p):
    return (G2B @ np.asarray(p, float).T).T / SC


def to_game(p):
    return (G2B.T @ np.asarray(p, float).T).T * SC


# =============================================================================================
# Texture: up to 32 palettes x 16 colours on Aya's 256x256 4-bit page (numpy only)
# =============================================================================================
def kmeans_colours(px, k, iters=12):
    px = np.asarray(px, float)
    if len(px) > 30000:
        px = px[np.random.default_rng(3).choice(len(px), 30000, replace=False)]
    uniq = np.unique(np.round(px), axis=0)
    if len(uniq) <= k:
        return np.concatenate([uniq, np.repeat(uniq[-1:], k - len(uniq), 0)]) if len(uniq) else np.zeros((k, 3))
    luma = px @ np.array([0.299, 0.587, 0.114])
    order = np.argsort(luma)
    cent = px[order[np.linspace(0, len(px) - 1, k).astype(int)]].copy()
    for _ in range(iters):
        grp = ((px[:, None, :] - cent[None]) ** 2).sum(2).argmin(1)
        for j in range(k):
            m = grp == j
            if m.any(): cent[j] = px[m].mean(0)
    return cent


def ps1_colour(c):
    r, g, b = (int(min(31, max(0, round(x / 8.0)))) for x in c)
    v = r | g << 5 | b << 10
    return v if v else 0x8000                  # 0 is transparent on the PlayStation: opaque black instead


def from_ps1(v):
    return np.array([(v & 31) * 8, ((v >> 5) & 31) * 8, ((v >> 10) & 31) * 8], float)


def build_page(atlas, faces_uv, log):
    """atlas: (H, W, 4) uint8, top row first. faces_uv: per face [(u, v) Blender UVs].
    Returns (page indices 256x256, [16 words] per palette, palette per face, label, preview)."""
    H = atlas.shape[0]
    f = H // 256
    small = atlas.reshape(256, f, 256, f, 4).mean((1, 3)) if f > 1 else atlas.astype(float)
    rgb, alpha = small[..., :3], small[..., 3]

    # islands: faces joined by edges whose UVs agree
    nf = len(faces_uv)
    parent = list(range(nf))

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]; x = parent[x]
        return x
    owner = {}
    for fi, f_ in enumerate(faces_uv):
        n = len(f_)
        for k in range(n):
            (a, ua), (b, ub) = f_[k], f_[(k + 1) % n]
            key = (min(a, b), max(a, b))
            uv = (tuple(np.round(ua, 5)), tuple(np.round(ub, 5))) if a < b else (tuple(np.round(ub, 5)), tuple(np.round(ua, 5)))
            if key in owner and owner[key][1] == uv:
                x, y = find(fi), find(owner[key][0])
                if x != y: parent[x] = y
            else:
                owner.setdefault(key, (fi, uv))
    island = [find(i) for i in range(nf)]
    ids = {k: i for i, k in enumerate(sorted(set(island)))}
    island = np.array([ids[k] for k in island])
    NI = len(ids)

    label = -np.ones((256, 256), int)
    yy, xx = np.mgrid[0:256, 0:256]
    cx, cy = xx + 0.5, yy + 0.5

    def raster(tri, isl):
        (x0, y0), (x1, y1), (x2, y2) = tri
        den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
        if abs(den) < 1e-12: return
        lo_x, hi_x = int(max(0, min(x0, x1, x2) - 1)), int(min(256, max(x0, x1, x2) + 2))
        lo_y, hi_y = int(max(0, min(y0, y1, y2) - 1)), int(min(256, max(y0, y1, y2) + 2))
        if lo_x >= hi_x or lo_y >= hi_y: return
        X, Y = cx[lo_y:hi_y, lo_x:hi_x], cy[lo_y:hi_y, lo_x:hi_x]
        w0 = ((y1 - y2) * (X - x2) + (x2 - x1) * (Y - y2)) / den
        w1 = ((y2 - y0) * (X - x2) + (x0 - x2) * (Y - y2)) / den
        w2 = 1 - w0 - w1
        # every texel the triangle can read: its square touches the triangle (centre within half a
        # texel of each edge, measured in texels)
        l0 = math.hypot(x2 - x1, y2 - y1); l1 = math.hypot(x0 - x2, y0 - y2); l2 = math.hypot(x1 - x0, y1 - y0)
        a2 = abs(den)
        m = (w0 * a2 >= -0.5 * l0) & (w1 * a2 >= -0.5 * l1) & (w2 * a2 >= -0.5 * l2)
        sub = label[lo_y:hi_y, lo_x:hi_x]
        other = np.unique(sub[m & (sub >= 0) & (sub != isl)])
        for o_ in other: shared.add((int(o_), int(isl)))
        sub[m & (sub < 0)] = isl
    shared = set()
    for fi, f_ in enumerate(faces_uv):
        pts = [(uv[0] * 256.0, (1.0 - uv[1]) * 256.0) for _, uv in f_]
        for k in range(1, len(pts) - 1):
            raster((pts[0], pts[k], pts[k + 1]), island[fi])
    # A texel keeps one 4-bit colour number, which means a colour only in its own palette: islands that
    # read the same texels (overlapping or mirrored UVs, islands touching on the 256 page) must share
    # a palette, or a face shows its texels through another palette's colours.
    up = list(range(NI))

    def root(x):
        while up[x] != x:
            up[x] = up[up[x]]; x = up[x]
        return x
    for a_, b_ in shared:
        ra, rb = root(a_), root(b_)
        if ra != rb: up[ra] = rb
    merged = sorted({root(i) for i in range(NI)})
    remap = {r: j for j, r in enumerate(merged)}
    comp = np.array([remap[root(i)] for i in range(NI)])
    if len(merged) < NI:
        log('texture: %d UV islands share texels: %d palette groups' % (NI, len(merged)))
    island = comp[island]
    label = np.where(label >= 0, comp[np.maximum(label, 0)], -1)
    NI = len(merged)
    covered = label >= 0
    for _ in range(512):                       # the rest of the page: nearest island
        if (label >= 0).all(): break
        grown = label.copy()
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            sh = np.roll(label, (dy, dx), (0, 1))
            m = (grown < 0) & (sh >= 0)
            grown[m] = sh[m]
        if (grown == label).all(): break
        label = grown
    label[label < 0] = 0

    # islands -> palettes: k-means on colour mean and spread
    NPAL = min(32, NI)
    feat = np.zeros((NI, 6)); wgt = np.ones(NI)
    for i in range(NI):
        m = covered & (label == i)
        if not m.any(): m = label == i
        if m.any():
            feat[i, :3] = rgb[m].mean(0); feat[i, 3:] = rgb[m].std(0); wgt[i] = max(1, m.sum())
        else:
            feat[i, :3] = 128
    cent = feat[np.argsort(-wgt)[:NPAL]].copy()
    rng = np.random.default_rng(1)
    for _ in range(60):
        grp = ((feat[:, None, :] - cent[None]) ** 2).sum(2).argmin(1)
        for k in range(NPAL):
            if (grp == k).any(): cent[k] = np.average(feat[grp == k], axis=0, weights=wgt[grp == k])
            else: cent[k] = feat[rng.integers(NI)]
    grp = ((feat[:, None, :] - cent[None]) ** 2).sum(2).argmin(1)

    index = np.zeros((256, 256), int)
    pal_words, err, clear = [], 0.0, 0
    tex_pal = grp[label]
    for k in range(NPAL):
        m = tex_pal == k
        see = m & covered & (alpha < 128)      # see-through texels inside the UVs: colour 0 (never drawn)
        solid = m & ~see
        px = rgb[solid & covered] if (solid & covered).any() else rgb[solid]
        n_col = 15 if see.any() else 16
        pal = kmeans_colours(px, n_col) if len(px) else np.zeros((n_col, 3))
        words = [ps1_colour(c) for c in pal]
        if see.any():
            words = [0x0000] + words
            clear += int(see.sum())
        pal_words.append(words)
        palr = np.array([from_ps1(w) for w in words])
        if solid.any():
            first = 1 if see.any() else 0
            d = ((rgb[solid][:, None, :] - palr[None, first:]) ** 2).sum(2)
            index[solid] = d.argmin(1) + first
            err += np.sqrt(d.min(1)).sum()
        index[see] = 0
    preview = np.zeros((256, 256, 4), np.uint8)
    for k in range(NPAL):
        m = tex_pal == k
        palr = np.array([from_ps1(w) for w in pal_words[k]]).astype(np.uint8)
        preview[m, :3] = palr[index[m]]
        preview[m, 3] = np.where(np.array(pal_words[k])[index[m]] == 0, 0, 255)
    log('texture: %d UV islands in %d palettes, mean colour error %.1f%s' % (
        NI, NPAL, err / 65536, ', %d see-through texels' % clear if clear else ''))
    return index, pal_words, grp[island], preview


# =============================================================================================
# Skin writer (no Blender needed): mesh arrays in, skin folder out
# =============================================================================================
def write_skin(game, mesh, atlas, out_dir, name, key, log, hd_size=0):
    """mesh: dict(verts (n,3) Blender metres in the game's idle stance, normals (n,3), part (n,),
    faces [[(vertex, (u, v)), ...] 3 or 4 corners]); atlas (H, W, 4) uint8, top row first, H = W >= 256."""
    G, NP = game.G, game.NP
    V, N, VP = np.asarray(mesh['verts'], float), np.asarray(mesh['normals'], float), np.asarray(mesh['part'], int)
    faces = mesh['faces']
    os.makedirs(out_dir, exist_ok=True)

    # vertices, part by part, in each part's own frame (idle pose = bind pose)
    used = np.zeros(len(V), bool)
    for f in faces:
        for vi, _ in f: used[vi] = True
    bad = sorted(set(int(p) for p in VP[used] if not (0 <= p < NP and game.visible[p])))
    if bad:
        raise SkinError('vertices on bones the game does not draw: %s' % [bone_name(p) for p in bad])
    Vg = to_game(V)
    Ng = (G2B.T @ N.T).T
    new_index, verts_out, ranges = {}, [], []
    for p in range(NP):
        start = len(verts_out)
        if not game.visible[p]:
            for vi in range(game.pstart[p], game.pstart[p] + game.pcount[p]):
                verts_out.append(list(G['verts'][vi]))
        else:
            R, t = game.idle[p]
            for vi in np.where((VP == p) & used)[0]:
                loc = R.T @ (Vg[vi] - t)
                nl = R.T @ Ng[vi]
                nl = nl / max(np.linalg.norm(nl), 1e-9) * game.normal_sign
                q = [int(round(x)) for x in loc]
                if not all(-32768 <= x < 32768 for x in q):
                    raise SkinError('a vertex is too far from its bone (%s): check the model scale' % bone_name(p))
                new_index[int(vi)] = len(verts_out)
                verts_out.append(q + [int(np.argmax(game.NT @ nl))])
        ranges.append((start, len(verts_out) - start))
    if len(verts_out) > 65535:
        raise SkinError('%d vertices: the game format allows 65,535' % len(verts_out))

    # texture page and palettes
    tims = game.tim_records()
    page_runs = sorted([t for t in tims if t['x'] == 960 and 256 <= t['y'] < 512 and t['w'] == 64], key=lambda t: t['y'])
    clut = next((t for t in tims if t['y'] == 448 and t['w'] == 256), None)
    if sum(t['size'] for t in page_runs) != 256 * 128 or clut is None:
        raise SkinError('texture layout of bank %d not understood' % game.bank)
    max_pal = 32 if clut['size'] >= 2 * 512 else 16
    faces_uv = [[(vi, uv) for vi, uv in f] for f in faces]
    index, pal_words, face_pal, preview = build_page(atlas, faces_uv, log)
    if len(pal_words) > max_pal:
        raise SkinError('internal: %d palettes' % len(pal_words))
    tex = bytearray(game.tex)
    page = bytearray(256 * 128)
    for v in range(256):
        row = index[v]
        page[v * 128:(v + 1) * 128] = bytes(((row[0::2] & 15) | (row[1::2] & 15) << 4).astype(np.uint8))
    for t in page_runs:
        row0 = t['y'] - 256
        tex[t['off']:t['off'] + t['size']] = page[row0 * 128:row0 * 128 + t['size']]
    for k, words in enumerate(pal_words):
        for i, w in enumerate(words):
            struct.pack_into('<H', tex, clut['off'] + ((k // 16) * 256 + (k % 16) * 16 + i) * 2, w)
    open(os.path.join(out_dir, 'texture.bin'), 'wb').write(tex)

    # faces -> the game's packets
    tpage = u16(G['packets'][0], 6)
    # Drawing order: the game links quads first, then triangles, into depth slots of about 1.5 cm,
    # and within a slot the face linked FIRST is drawn on top (Render_DrawTexturedQuads). Eyes,
    # brows and other small pieces lying on a surface go first, as quads (a triangle becomes a quad
    # with its last corner twice), so they stay over the face instead of flickering under it.
    overlay = mesh.get('overlay') or [False] * len(faces)
    outer = mesh.get('outer') or [0.0] * len(faces)
    lift = mesh.get('lift') or [0] * len(faces)
    front, quads, tris = [], [], []
    for fi, f in enumerate(faces):
        vi = [new_index[a] for a, _ in f]
        uv = [(int(min(255, max(0, round(u * 256.0 - 0.5)))), int(min(255, max(0, round((1.0 - v) * 256.0 - 0.5)))))
              for _, (u, v) in f]
        k = int(face_pal[fi])
        cl = 0x7000 | (k // 16) << 6 | (k % 16)
        if overlay[fi]:
            if len(f) == 3: vi, uv = vi + vi[2:], uv + uv[2:]
            o = [0, 1, 3, 2] if game.order_outward else [1, 0, 2, 3]
            front.append((-outer[fi], ([vi[j] for j in o], [uv[j] for j in o], cl, lift[fi])))
        elif len(f) == 4:
            o = [0, 1, 3, 2] if game.order_outward else [1, 0, 2, 3]
            quads.append((-outer[fi], ([vi[j] for j in o], [uv[j] for j in o], cl, lift[fi])))
        else:
            o = [0, 1, 2] if game.order_outward else [0, 2, 1]
            tris.append((-outer[fi], ([vi[j] for j in o], [uv[j] for j in o], cl, lift[fi])))
    # outer layers first (farther from their bone: hair over the face, a jacket over the dress), so in a
    # tie they are drawn on top of what lies under them
    srt = lambda lst: [x for _, x in sorted(lst, key=lambda t: t[0])]
    front, quads, tris = srt(front), srt(quads), srt(tris)
    quads = front + quads
    if front:
        log('%d faces of small pieces (eyes, decals) drawn first, on top' % len(front))
    if any(lift):
        log('%d faces lying on another surface (straps, hair over the face) sort %d slots nearer' %
            (sum(1 for x in lift if x), max(lift)))
    if len(quads) + len(tris) > 65535:
        raise SkinError('%d faces: the game format allows 65,535' % (len(quads) + len(tris)))
    NB = dict(header=bytearray(G['header']))
    NB['parts'] = []
    for p in range(NP):
        r = bytearray(G['parts'][p]); struct.pack_into('<HH', r, 0, *ranges[p])
        struct.pack_into('<i', r, 8, int(game.oz[p]))             # the skin's own bone lengths
        NB['parts'].append(r)
    NB['verts'] = verts_out
    NB['colours'] = [0x808080] * len(verts_out)
    NB['desc'], NB['packets'] = [], []
    for (vi, uv, cl, lf), kind in [(q, QUAD_KIND) for q in quads] + [(t, TRI_KIND) for t in tris]:
        # unused bytes: 'P' 'B' n = sort n slots nearer (game builds with RENDER_FACE_LIFT, object.c)
        d = bytearray(12); d[0:4] = bytes([0x50, 0x42, lf, kind] if lf else [0x80, 0x80, 0x80, kind])
        for k, x in enumerate(vi): struct.pack_into('<H', d, 4 + 2 * k, x)
        NB['desc'].append(d)
        pkt = bytearray(16 if len(vi) == 4 else 12)
        for k, (u, v) in enumerate(uv): pkt[4 * k] = u; pkt[4 * k + 1] = v
        struct.pack_into('<H', pkt, 2, cl); struct.pack_into('<H', pkt, 6, tpage)
        NB['packets'].append(pkt)
    h = NB['header']
    struct.pack_into('<H', h, 4, len(quads) + len(tris))
    struct.pack_into('<H', h, 6, len(verts_out))
    struct.pack_into('<4H', h, 8, len(quads), len(tris), 0, 0)
    NB['bounds'] = [list(x) for x in G['bounds']]       # the game's own (they may feed hit checks)
    NB['cmds'] = G['cmds']
    block = write_block(NB)
    model = bytearray(game.model)
    while len(model) % 4: model.append(0)
    new_off = len(model)
    model += block
    w = u32(model, game.rec_at)
    struct.pack_into('<I', model, game.rec_at, (w & 0xFF000000) | new_off)
    w = u32(model, game.rec_at - 4)                    # the record's length: the new block's
    struct.pack_into('<I', model, game.rec_at - 4, (w & 0xFF000000) | len(block))
    while len(model) % 2048: model.append(0)
    open(os.path.join(out_dir, 'model.bin'), 'wb').write(model)

    for d in range(1, 9):
        open(os.path.join(out_dir, 'day%d.bin' % d), 'wb').write(game.disc.day(game.bank, d))
    open(os.path.join(out_dir, 'meta.bin'), 'wb').write(b'PEMT' + bytes([GUN_PART, 0, 0, 0]))
    with open(os.path.join(out_dir, 'skin.ini'), 'w', encoding='utf-8', newline='\n') as fh:
        fh.write('# Read by the pe_custom_skins mod: this folder shows in Aya\'s Wardrobe.\n')
        fh.write('name = %s\nkey = %s\ngroup = Custom skins\nbank = %d\n' % (name, key, game.bank))
    save_png(os.path.join(out_dir, 'page_preview.png'), np.repeat(np.repeat(preview, 2, 0), 2, 1))
    if hd_size:
        hd = resize_rgba(atlas, hd_size)
        save_png(os.path.join(out_dir, 'texture_hd.png'), hd)
    need = memory_need(len(quads), len(tris), NP)
    log('skin: %d quads + %d triangles, %d vertices; %d KB of drawing memory (%s)' % (
        len(quads), len(tris), len(verts_out), need // 1024, budget_text(need)))
    return dict(quads=len(quads), tris=len(tris), verts=len(verts_out), need=need)


def memory_need(q, t, nparts=29):
    """Packet memory the game allocates for the model (64-bit build)."""
    return 128 * q + 112 * t + 32 * nparts + 16 * 64


def budget_text(need):
    for n in (1, 2, 4, 8):
        if need <= n * BLOCK:
            return '%d pool block%s%s' % (n, 's' if n > 1 else '', '' if n <= 4 else ': over the safe size, may fail in busy rooms')
    return 'host memory: needs a game build where skins draw from host memory (PE_HOOK_ACTOR_ALLOC)'


def resize_rgba(img, size):
    """Area/nearest resize of an (H, W, 4) uint8 picture to size x size, numpy only."""
    H, W = img.shape[:2]
    if (H, W) == (size, size):
        return img
    ys = (np.arange(size) + 0.5) * H / size
    xs = (np.arange(size) + 0.5) * W / size
    if H >= size and W >= size and H % size == 0 and W % size == 0:
        return img.reshape(size, H // size, size, W // size, 4).mean((1, 3)).astype(np.uint8)
    y0 = np.clip(np.floor(ys - 0.5).astype(int), 0, H - 1); y1 = np.clip(y0 + 1, 0, H - 1)
    x0 = np.clip(np.floor(xs - 0.5).astype(int), 0, W - 1); x1 = np.clip(x0 + 1, 0, W - 1)
    fy = np.clip(ys - 0.5 - y0, 0, 1)[:, None, None]; fx = np.clip(xs - 0.5 - x0, 0, 1)[None, :, None]
    a = img.astype(float)
    top = a[y0][:, x0] * (1 - fx) + a[y0][:, x1] * fx
    bot = a[y1][:, x0] * (1 - fx) + a[y1][:, x1] * fx
    return (top * (1 - fy) + bot * fy).round().astype(np.uint8)


def save_png(path, rgba):
    """PNG writer without PIL (zlib only)."""
    import zlib
    h, w = rgba.shape[:2]
    raw = b''.join(b'\0' + rgba[y].astype(np.uint8).tobytes() for y in range(h))

    def chunk(t, d):
        c = struct.pack('>I', len(d)) + t + d
        return c + struct.pack('>I', zlib.crc32(t + d) & 0xFFFFFFFF)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
    open(path, 'wb').write(png)


def check_skin(out_dir, nparts):
    """The checks the game's loader makes, run on the written files."""
    m = open(os.path.join(out_dir, 'model.bin'), 'rb').read()
    _, off = root_record(m)
    C = parse_block(m, off)
    nxt = 0
    for c in C['cmds'][:C['ncmds']]:
        c = c - 256 if c > 127 else c
        if c < 0: continue
        if c != nxt: raise SkinError('check: commands out of order')
        nxt += 1
    if nxt != len(C['parts']) or nxt < 29: raise SkinError('check: part count')
    nv = u16(C['header'], 6)
    for d in C['desc']:
        if not all(u16(d, 4 + 2 * k) < nv for k in range(4 if d[3] == QUAD_KIND else 3)):
            raise SkinError('check: face uses a missing vertex')
    for d in range(1, 9):
        if os.path.getsize(os.path.join(out_dir, 'day%d.bin' % d)) <= 8: raise SkinError('check: day file')


# =============================================================================================
# Bone assignment (numpy): one game bone per vertex
# =============================================================================================
def rest_segments(game):
    """Per part: (head, tail) in Blender space, idle pose. Head = joint; tail = the visible children's
    joints, or for hands / feet / head the far end of the game's own mesh for that part."""
    J = {p: to_blender(game.idle[p][1]) for p in range(game.NP)}
    W = to_blender(game.idle_vertices())
    kids = {p: [] for p in range(game.NP)}
    for c in range(game.NP):
        q = game.parent[c]
        while q >= 0 and not game.visible[q] and q != 0:
            q = game.parent[q]
        if q >= 0: kids[q].append(c)
    seg = {}
    for p in range(game.NP):
        if not game.visible[p]: continue
        ch = [c for c in kids[p] if game.visible[c]]
        if ch:
            tail = np.mean([J[c] for c in ch], axis=0)
        else:
            pts = W[game.vpart == p]
            tail = pts[np.argmax(np.linalg.norm(pts - J[p], axis=1))] if len(pts) else J[p] + np.array([0, 0, -0.05])
        seg[p] = (J[p].copy(), np.asarray(tail, float))
    seg['_kids'] = kids
    return seg


def seg_dist(P, h, t):
    ab = t - h
    k = np.clip(((P - h) @ ab) / max(ab @ ab, 1e-12), 0, 1)
    c = h + k[:, None] * ab
    return np.linalg.norm(P - c, axis=1), c


def components(n, edges, mask):
    """Connected component id per vertex, over edges whose both ends are in mask (others: -1)."""
    comp = np.where(mask, np.arange(n), -1)
    if len(edges) == 0: return comp
    e = edges[mask[edges[:, 0]] & mask[edges[:, 1]]]
    if len(e) == 0: return comp
    for _ in range(10000):
        a, b = comp[e[:, 0]], comp[e[:, 1]]
        lo = np.minimum(a, b)
        new = comp.copy()
        np.minimum.at(new, e[:, 0], lo); np.minimum.at(new, e[:, 1], lo)
        m = new >= 0
        new[m] = new[new[m]]                  # pointer jumping
        if (new == comp).all(): break
        comp = new
    return comp


def weld_map(V, tol=1e-4):
    """Index of each vertex among the distinct positions (vertices split at texture seams share one)."""
    _, w = np.unique(np.round(np.asarray(V, float) / tol).astype(np.int64), axis=0, return_inverse=True)
    return w.ravel()


def assign_bones(V, N, edges, seg, side_l, log=print, deform=None, tree=None):
    """V, N world positions / normals (n,3); seg = rest_segments; deform(p) -> 4x4 moving the rest
    skeleton to where the model stands (None = the model stands like the game). Returns part per vertex.
    Works on welded positions: a model cut at every texture seam (common in rips) is judged as the
    surface it is, and the copies of a vertex always get the same bone (no cracks)."""
    V = np.asarray(V, float); N = np.asarray(N, float)
    w = weld_map(V)
    nw = int(w.max()) + 1 if len(w) else 0
    Vw = np.zeros((nw, 3)); Nw = np.zeros((nw, 3))
    np.add.at(Vw, w, V); np.add.at(Nw, w, N)
    Vw /= np.maximum(np.bincount(w, minlength=nw), 1)[:, None]
    Nw /= np.maximum(np.linalg.norm(Nw, axis=1, keepdims=True), 1e-9)
    E = w[np.asarray(edges, int).reshape(-1, 2)]
    E = E[E[:, 0] != E[:, 1]]
    E = np.unique(np.sort(E, axis=1), axis=0) if len(E) else E
    part = _assign_welded(Vw, Nw, E, seg, side_l, log, deform, tree)
    assign_bones.segments = _assign_welded.segments
    return part[w]


def _assign_welded(V, N, edges, seg, side_l, log=print, deform=None, tree=None):
    n = len(V)
    cand = [p for p in AUTO if p in seg]

    def moved(p, x):
        if deform is None: return np.asarray(x, float)
        M = deform(p)
        return (M[:3, :3] @ np.asarray(x, float)) + M[:3, 3]
    kids = seg['_kids']
    S = {}
    for p in cand:
        h, t = seg[p]
        ch = [c for c in kids[p] if c in seg]
        tail = np.mean([moved(c, seg[c][0]) for c in ch], axis=0) if ch else moved(p, t)
        S[p] = (moved(p, h), tail)
    root = moved(0, seg[1][0]) if 1 in seg else np.zeros(3)
    D = np.zeros((n, len(cand)))
    for j, p in enumerate(cand):
        d, c = seg_dist(V, *S[p])
        D[:, j] = d
        # normal agreement: a surface facing its bone (torso skin next to a hanging arm) is not that bone's
        away = V - c
        ln = np.linalg.norm(away, axis=1)
        dot = (N * away).sum(1) / np.maximum(ln, 1e-9)
        D[:, j] += np.where((dot < -0.3) & (ln > 0.02), 0.08 + d, 0)
        # an arm is thin: a hip or thigh touched by a hanging hand stays body
        if p in ARM_REACH: D[:, j] += np.where(d > ARM_REACH[p], 0.5 + d, 0)
        # legs too: cloth farther out (skirts, dresses, coat tails) hangs from the pelvis
        if p in LEG_REACH: D[:, j] += np.where(d > LEG_REACH[p], 0.5 + d, 0)
        # limbs never cross the body's middle
        x = (V[:, 0] - root[0]) * side_l
        if p in LEFT: D[x < -0.01, j] += 1.0
        if p in RIGHT: D[x > 0.01, j] += 1.0
    cand = np.array(cand)
    part = cand[D.argmin(1)]
    edges = np.asarray(edges, int).reshape(-1, 2)
    nb_count = np.bincount(edges.ravel(), minlength=n)

    def neighbour_vote(sel):
        """majority label among the neighbours of the vertices in sel (outside sel)."""
        m = sel[edges[:, 0]] != sel[edges[:, 1]]
        e = edges[m]
        outside = np.where(sel[e[:, 0]], part[e[:, 1]], part[e[:, 0]])
        if len(outside) == 0: return None
        return int(np.bincount(outside).argmax())

    # small separate pieces (buttons, earrings, eyes) move with one bone
    isl = components(n, edges, np.ones(n, bool))
    sizes = np.bincount(isl, minlength=n)
    head_z = S[11][0][2] if 11 in S else np.inf
    for i in np.where((sizes > 0) & (sizes < 0.25 * n))[0]:
        idx = isl == i
        if V[idx, 2].max() > head_z and 11 in cand:
            part[idx] = 11                           # hair, earrings, ribbons: hang from the head
        elif sizes[i] < max(30, n // 100):
            part[idx] = cand[D[idx].sum(0).argmin()]     # the bone closest to the whole piece
    # every bone keeps its largest connected region; smaller detached bits join their surroundings
    for _ in range(2):
        for p in cand:
            m = part == p
            if not m.any(): continue
            comp = components(n, edges, m)
            ids, cnt = np.unique(comp[m], return_counts=True)
            if len(ids) < 2: continue
            big = cnt.max()
            for cid, c in zip(ids, cnt):
                if cid == ids[cnt.argmax()] or (c > big * 0.5 and p not in ARMS): continue
                sel = comp == cid
                v = neighbour_vote(sel)
                if v is not None: part[sel] = v
    # a small loose piece sitting on a bigger one (a hair tie on a braid, a button, a buckle) moves with
    # the bone of what it sits on, not with whatever bone happens to be nearest
    big = sizes[isl] >= max(30, n // 100)
    if big.any():
        Vb, Pb = V[big], part[big]
        for i in np.where((sizes > 0) & (sizes < max(30, n // 100)))[0]:
            idx = np.where(isl == i)[0]
            if part[idx[0]] == 11 and V[idx, 2].max() > head_z: continue
            best = (np.inf, None)
            for k in idx[::max(1, len(idx) // 8)]:
                d = np.linalg.norm(Vb - V[k], axis=1)
                j = int(d.argmin())
                if d[j] < best[0]: best = (d[j], int(Pb[j]))
            if best[0] < 0.03: part[idx] = best[1]
    # a vertex none of whose neighbours shares its bone follows them
    for _ in range(3):
        same = part[edges[:, 0]] == part[edges[:, 1]]
        cnt = np.bincount(edges[same].ravel(), minlength=n)
        lone = np.where((cnt == 0) & (nb_count > 0))[0]
        if len(lone) == 0: break
        for i in lone:
            nb = np.concatenate([edges[edges[:, 0] == i, 1], edges[edges[:, 1] == i, 0]])
            part[i] = np.bincount(part[nb]).argmax()
    counts = {bone_name(p): int((part == p).sum()) for p in cand if (part == p).any()}
    log('bones assigned: ' + ', '.join('%s %d' % kv for kv in counts.items()))
    _assign_welded.segments = S               # the bone segments where the model stands
    if tree is not None:
        col = {int(c): j for j, c in enumerate(cand)}
        part = fix_spans(part, edges, tree, log,
                         cost=lambda v, q: D[v, col[q]] if q in col else np.inf)
    return part


def fit_lengths(game, heads, caxis, reg=0.01):
    """Bone lengths that put the drawn joints where the fitting rig has them. A joint sits at its
    parent's joint plus the parent's z axis times its length, so every joint is linear in the lengths:
    least squares over all of them at once (a hip moved down is carried by the hidden hip base above
    it), pulled lightly towards the game's own. heads[p]: joint position, caxis[p]: part p's z axis
    (Blender space, the fitting rig's pose). Returns the new lengths, each kept within 0.5x..2x."""
    NP = game.NP
    var = [p for p in LENGTHS if p < NP and game.oz_game[p]]
    col = {p: j for j, p in enumerate(var)}

    def chain(p):
        out = []
        while p > 0: out.append(p); p = game.parent[p]
        return out
    rows, rhs = [], []
    for p in AUTO:
        if p not in heads: continue
        r = np.zeros((3, len(var)))
        b = np.asarray(heads[p], float) - np.asarray(heads[0], float)
        for a in chain(p):
            q = game.parent[a]
            if q < 0 or q not in caxis: continue
            if a in col: r[:, col[a]] += caxis[q] / SC
            else: b = b - caxis[q] * game.oz_game[a] / SC
        rows.append(r); rhs.append(b)
    A = np.vstack(rows + [np.eye(len(var)) * math.sqrt(reg) / SC])
    y = np.concatenate(rhs + [np.array([game.oz_game[p] for p in var], float) * math.sqrt(reg) / SC])
    sol = np.linalg.lstsq(A, y, rcond=None)[0]
    oz = list(game.oz_game)
    for p, v in zip(var, sol):
        g0 = game.oz_game[p]
        oz[p] = int(round(np.clip(v, min(g0 * 0.5, g0 * 2.0), max(g0 * 0.5, g0 * 2.0))))
    return oz


def major_tree(game):
    """Parent of each drawn bone among the bones the automatic pass uses, skipping the tiny and
    hidden ones between them (clavicles, arm and hip bases). Chest and pelvis hang off the root (0)."""
    tree = {}
    for p in AUTO:
        q = game.parent[p]
        while q > 0 and q not in AUTO: q = game.parent[q]
        tree[p] = max(q, 0)
    return tree


def fix_spans(part, edges, tree, log=print, cost=None):
    """The game moves each vertex with one bone, so an edge between two bones that don't share a joint
    (left and right thigh across a skirt, neck to pelvis down a ponytail, head to arm through hair)
    stretches across the screen when they move apart. Each vertex on such an edge may take a bone of
    its neighbours or a bone joining them (the pelvis between the thighs, the chest between neck and
    pelvis, the neck between head and chest) when that leaves fewer such edges around it; repeated
    until nothing improves. Local only: it never spreads over the model."""
    edges = np.asarray(edges, int).reshape(-1, 2)
    part = part.copy()
    n = len(part)
    nbrs = [[] for _ in range(n)]
    for x, y in edges:
        nbrs[x].append(y); nbrs[y].append(x)

    def joined(a, b):
        return a == b or tree.get(a) == b or tree.get(b) == a or (tree.get(a) == 0 and tree.get(b) == 0)

    def bad_at(v, label):
        return sum(1 for w in nbrs[v] if not joined(label, int(part[w])))
    moved = 0
    for _ in range(50):
        a, b = part[edges[:, 0]], part[edges[:, 1]]
        idx = np.where(a != b)[0]
        idx = [e for e in idx if not joined(int(a[e]), int(b[e]))]
        if not idx: break
        verts = sorted(set(edges[idx].ravel().tolist()))
        changed = 0
        for v in verts:
            cur = bad_at(v, int(part[v]))
            if cur == 0: continue
            cands = set()
            for w in nbrs[v]:
                q = int(part[w]); cands.add(q)
                if tree.get(q, 0) > 0: cands.add(tree[q])
            cands |= {c for c, par in tree.items() if par in cands}
            best, bc = int(part[v]), cur
            here = cost(v, int(part[v])) if cost else 0.0
            for c in sorted(cands):
                # never a bone much farther from the vertex than its own (a dress panel near a shin must
                # not go to the thigh 35 cm away: it would fly off when she runs)
                if cost is not None and cost(v, c) > here + 0.04: continue
                k = bad_at(v, c)
                if k < bc: best, bc = c, k
            if best != part[v]:
                part[v] = best; changed += 1
        moved += changed
        if not changed: break
    a, b = part[edges[:, 0]], part[edges[:, 1]]
    left = sum(1 for x, y in zip(a, b) if not joined(int(x), int(y)))
    if moved or left:
        log('%d vertices moved where an edge joined bones with no joint between them%s' % (
            moved, '; %d such edges left: assign them by hand' % left if left else ''))
    return part


def joint_sides(V, part, segments, log=print, shoulders=True):
    """Skin on the wrong side of a joint goes to the bone on that side (segments: where the model
    stands, assign_bones.segments).
    - Shoulders: upper-arm skin on the body's side of the shoulder joint goes to the chest. The chest
      is one line down the middle, so a wide torso's shoulders lie nearer the upper arm, and bending
      a T-pose arm ~65 degrees down swung them into the torso. Only when bending the model into the
      stance (shoulders=True): with the arms already down, this side of the joint is the shoulder's
      top, which must follow the arm when Aya aims.
    - Ankles: shin skin past the ankle joint (a boot's heel and sole) goes to the foot. The foot's
      reach is short (a shoe, not a dress hem), so a heel behind the ankle went to the shin and tore
      off the boot when the walk bent the ankle.
    - Wrists: hand skin on the forearm's side of the wrist joint (a gauntlet's or glove's cuff) goes to
      the forearm; on the hand, the cuff swung out of the arm whenever the wrist bent."""
    part = np.array(part)
    V = np.asarray(V, float)

    def along(p):
        h, t = segments[p]
        ab = t - h
        L = max(float(np.linalg.norm(ab)), 1e-9)
        return ((V - h) @ ab) / L, L
    do_shoulders, shoulders, ankles = shoulders, 0, 0
    for arm in (4, 14):
        if not do_shoulders or arm not in segments: continue
        s, _ = along(arm)
        sel = (part == arm) & (s < 0)
        part[sel] = 1
        shoulders += int(sel.sum())
    for shin, foot in ((23, 24), (27, 28)):
        if shin not in segments or foot not in segments: continue
        s, L = along(shin)
        sel = (part == shin) & (s > L)
        part[sel] = foot
        ankles += int(sel.sum())
    wrists = 0
    for fore, hand in ((5, 6), (15, 16)):
        if fore not in segments or hand not in segments: continue
        s, _ = along(hand)
        sel = (part == hand) & (s < 0)
        part[sel] = fore
        wrists += int(sel.sum())
    if shoulders: log('%d vertices on the body side of the shoulder joints moved to the chest' % shoulders)
    if ankles: log('%d vertices past the ankle joints (heels, soles) moved to the feet' % ankles)
    if wrists: log('%d vertices above the wrist joints (gauntlet and glove cuffs) moved to the forearms' % wrists)
    return part


def _nearest_dist(P, Q):
    """Distance from each point of P to the nearest point of Q (None when Q is empty)."""
    if len(Q) == 0 or len(P) == 0: return None
    try:
        from mathutils.kdtree import KDTree
    except ImportError:
        return np.array([np.min(np.linalg.norm(Q - x, axis=1)) for x in P])
    kd = KDTree(len(Q))
    for i, x in enumerate(Q): kd.insert(x.tolist(), i)
    kd.balance()
    return np.array([kd.find(x.tolist())[2] for x in P])


# the body's core: when bending a T-pose / A-pose model into the stance it keeps its shape, only the
# limbs bend at their joints
TORSO = (1, 20)


def smooth_weights(V, part, segments, neighbours, width=0.02, rigid=TORSO, seam_width=0.06):
    """Weights for bending the model into the game stance: each vertex follows its own bone, blended
    with the bones it joins (parent / children) by how close it is to them, over `width` metres.
    Distances, not mesh edges: the same on a 500 and a 500,000 vertex model.
    Torso vertices (rigid) never take a limb's move: the chest is one line down the middle, so the side
    of a wide chest lies nearer the upper arm than the chest line and followed the arm halfway when a
    T-pose arm came down 65 degrees (the chest and the top caved in; up to 3.6x stretched edges).
    The limb's own skin near the joint still blends toward the torso."""
    labels = sorted(set(part.tolist()))
    col = {p: j for j, p in enumerate(labels)}
    n = len(V)
    D = {p: seg_dist(V, *segments[p])[0] for p in labels if p in segments}
    W = np.zeros((n, len(labels)))
    for p in labels:
        sel = part == p
        if not sel.any(): continue
        W[sel, col[p]] = 1.0
        if p not in D: continue
        for q in neighbours.get(p, ()):
            if q not in D or q not in col: continue
            if p in rigid and q not in rigid: continue
            W[sel, col[q]] = np.exp(-np.maximum(D[q][sel] - D[p][sel], 0) / width)
            if q in rigid and p not in rigid:
                # a limb's skin next to the torso's (a shoulder by the chest, a thigh by the pelvis) blends
                # by its distance to the torso's own vertices, over seam_width: the torso doesn't move,
                # so without this the seam edges were stretched up to 6x
                near = _nearest_dist(V[sel], V[part == q])
                if near is not None:
                    W[sel, col[q]] = np.maximum(W[sel, col[q]], np.exp(-near / seam_width))
    W[W < 0.02] = 0
    W /= np.maximum(W.sum(1, keepdims=True), 1e-9)
    return labels, W


# =============================================================================================
# Collada (.dae) reader: Blender 5 dropped its own importer (numpy + xml only)
# =============================================================================================
def joint_kind(name):
    """What a model's own skeleton bone is, from its name (Mixamo, Unity, Unreal, Blender rigify-like,
    VRM, Biped): 'upperarm', 'forearm', 'hand', 'thigh', 'shin', 'foot', 'neck', 'head' or None."""
    n = name.lower()
    for junk in ('mixamorig:', 'mixamorig_', 'bip01', 'bip001', 'j_bip_', 'def-', 'org-', 'mch-'):
        n = n.replace(junk, '')
    if any(w in n for w in ('twist', 'end', 'nub', 'finger', 'thumb', 'index', 'middle', 'ring', 'pinky',
                            'toe', 'ball', 'roll', 'pole', 'ik', 'helper', 'hair', 'eye', 'jaw', 'tongue',
                            'breast', 'skirt', 'cloth', 'spine', 'chest')):
        return None
    if 'forearm' in n or 'lowerarm' in n or 'elbow' in n: return 'forearm'
    if 'upperarm' in n or n.rstrip('_.lr ').endswith('arm') or 'leftarm' in n or 'rightarm' in n: return 'upperarm'
    if 'hand' in n or 'wrist' in n: return 'hand'
    if 'upleg' in n or 'thigh' in n or 'upperleg' in n: return 'thigh'
    if 'calf' in n or 'shin' in n or 'lowerleg' in n or 'knee' in n or 'leg' in n: return 'shin'
    if 'foot' in n or 'ankle' in n: return 'foot'
    if 'neck' in n: return 'neck'
    if n.strip('_. ').endswith('head') or n == 'head': return 'head'
    return None


import xml.etree.ElementTree as _ET
ET_ERRORS = (_ET.ParseError, ValueError, KeyError, IndexError, AttributeError, OSError)


def read_dae(path):
    """Meshes of a Collada 1.4 / 1.5 file, in their bind pose (skinned meshes: the skin's bind shape),
    Blender axes (Z up), metres. Returns [dict(name, verts (n,3), faces [[v..]], uvs [[(u,v)..]] or None,
    mats [material index per face])] and the materials [dict(name, image path or None, colour)]."""
    import xml.etree.ElementTree as ET
    import urllib.parse
    root = ET.parse(path).getroot()
    ns = root.tag[:root.tag.index('}') + 1] if root.tag.startswith('{') else ''
    T_ = lambda *tags: '/'.join(ns + t for t in tags)
    ids = {el.get('id'): el for el in root.iter() if el.get('id')}
    base = os.path.dirname(os.path.abspath(path))

    def ref(url):
        return ids.get((url or '').lstrip('#'))

    def floats(el):
        return np.array((el.text or '').split(), float) if el is not None else np.zeros(0)

    def ints(el):
        return np.array((el.text or '').split(), int) if el is not None else np.zeros(0, int)
    asset = root.find(T_('asset'))
    up = (asset.findtext(T_('up_axis')) or 'Y_UP').strip() if asset is not None else 'Y_UP'
    unit = asset.find(T_('unit')) if asset is not None else None
    metre = float(unit.get('meter', '1')) if unit is not None else 1.0
    AX = {'Y_UP': np.array([[1, 0, 0], [0, 0, -1], [0, 1, 0]], float),
          'X_UP': np.array([[0, -1, 0], [1, 0, 0], [0, 0, 1]], float)}.get(up, np.eye(3))

    def image_file(img):
        txt = img.findtext(T_('init_from')) or img.findtext(T_('init_from', 'ref')) or ''
        txt = urllib.parse.unquote(txt.strip())
        for pre in ('file:///', 'file://'):
            if txt.startswith(pre): txt = txt[len(pre):]
        cands = [txt if os.path.isabs(txt) else os.path.join(base, txt), os.path.join(base, os.path.basename(txt.replace('\\', '/')))]
        return next((c for c in cands if txt and os.path.isfile(c)), None)

    def effect_look(eff):
        """(image path, colour) of an effect's diffuse."""
        if eff is None: return None, (0.8, 0.8, 0.8)
        tex = eff.find('.//' + ns + 'diffuse/' + ns + 'texture')
        if tex is None: tex = eff.find('.//' + ns + 'texture')
        if tex is not None:
            name = tex.get('texture', '')
            params = {p.get('sid'): p for p in eff.iter(ns + 'newparam')}
            img = None
            if name in params:                     # sampler -> surface -> image (1.4) or sampler -> image (1.5)
                smp = params[name]
                src = smp.findtext('.//' + ns + 'source')
                inst = smp.find('.//' + ns + 'instance_image')
                if inst is not None: img = ref(inst.get('url'))
                elif src and src.strip() in params:
                    img = ids.get((params[src.strip()].findtext('.//' + ns + 'init_from') or '').strip())
            if img is None: img = ids.get(name)
            if img is not None and img.tag == ns + 'image':
                return image_file(img), (1, 1, 1)
        col = eff.find('.//' + ns + 'diffuse/' + ns + 'color')
        c = floats(col) if col is not None else np.array([0.8, 0.8, 0.8])
        return None, tuple(c[:3]) if len(c) >= 3 else (0.8, 0.8, 0.8)

    materials, mat_index = [], {}

    def material(mid):
        if mid not in mat_index:
            el = ids.get(mid)
            eff = ref(el.find(T_('instance_effect')).get('url')) if el is not None and el.find(T_('instance_effect')) is not None else None
            image, colour = effect_look(eff)
            mat_index[mid] = len(materials)
            materials.append(dict(name=(el.get('name') if el is not None else None) or mid, image=image, colour=colour))
        return mat_index[mid]

    def source(src_id):
        el = ids.get(src_id.lstrip('#'))
        if el is None: return np.zeros((0, 3))
        if el.tag == ns + 'vertices':
            pos = next(i for i in el.findall(T_('input')) if i.get('semantic') == 'POSITION')
            return source(pos.get('source'))
        acc = el.find('.//' + ns + 'accessor')
        stride = int(acc.get('stride', '1')) if acc is not None else 3
        arr = floats(el.find(T_('float_array')))
        return arr[:len(arr) // stride * stride].reshape(-1, stride)

    def node_matrix(node):
        M = np.eye(4)
        for ch in node:
            tag = ch.tag[len(ns):]
            v = floats(ch)
            if tag == 'matrix' and len(v) == 16: M = M @ v.reshape(4, 4)
            elif tag == 'translate' and len(v) == 3:
                t = np.eye(4); t[:3, 3] = v; M = M @ t
            elif tag == 'scale' and len(v) == 3: M = M @ np.diag([*v, 1.0])
            elif tag == 'rotate' and len(v) == 4:
                ax = v[:3] / max(np.linalg.norm(v[:3]), 1e-12); a = math.radians(v[3])
                K = np.array([[0, -ax[2], ax[1]], [ax[2], 0, -ax[0]], [-ax[1], ax[0], 0]])
                r = np.eye(4); r[:3, :3] = np.eye(3) + math.sin(a) * K + (1 - math.cos(a)) * K @ K; M = M @ r
        return M

    def mesh_of(geom, M, symbols, name, skin=None):
        mesh = geom.find(T_('mesh'))
        if mesh is None: return None
        P = None
        faces, uvs, mats = [], [], []
        any_uv = False
        for prim in mesh:
            kind = prim.tag[len(ns):]
            if kind not in ('triangles', 'polylist', 'polygons'): continue
            inputs = prim.findall(T_('input'))
            stride = max(int(i.get('offset', '0')) for i in inputs) + 1
            vin = next((i for i in inputs if i.get('semantic') == 'VERTEX'), None)
            if vin is None: continue
            pos = source(vin.get('source'))
            if P is None: P = pos
            tin = sorted([i for i in inputs if i.get('semantic') == 'TEXCOORD'], key=lambda i: int(i.get('set', '0')))
            tin = tin[0] if tin else None
            tc = source(tin.get('source')) if tin is not None else None
            if tin is None:                        # UVs kept on the shared vertices (<vertices> inputs)
                vel = ids.get(vin.get('source', '').lstrip('#'))
                vt = [i for i in vel.findall(T_('input')) if i.get('semantic') == 'TEXCOORD'] if vel is not None else []
                if vt:
                    tc = source(vt[0].get('source')); tin = vin
            mi = material(symbols.get(prim.get('material'), prim.get('material') or 'default'))
            if kind == 'triangles':
                p = ints(prim.find(T_('p'))); counts = [3] * (len(p) // (3 * stride)); plist = [p]
            elif kind == 'polylist':
                p = ints(prim.find(T_('p'))); counts = ints(prim.find(T_('vcount'))).tolist(); plist = [p]
            else:
                plist = [ints(e) for e in prim.findall(T_('p'))]; counts = None
            for p in plist:
                cs = counts if counts is not None else [len(p) // stride]
                at = 0
                for c in cs:
                    idx = p[at * stride:(at + c) * stride].reshape(c, stride); at += c
                    faces.append(idx[:, int(vin.get('offset'))].tolist())
                    if tc is not None:
                        uv = tc[idx[:, int(tin.get('offset'))], :2]; any_uv = True
                        uvs.append([tuple(x) for x in uv])
                    else:
                        uvs.append([(0.0, 0.0)] * c)
                    mats.append(mi)
        if P is None or not faces: return None
        V = skin(P[:, :3]) if skin is not None else np.c_[P[:, :3], np.ones(len(P))] @ M.T
        V = (V[:, :3] @ AX.T) * metre
        return dict(name=name, verts=V, faces=faces, uvs=uvs if any_uv else None, mats=mats)

    out = []
    world = {}                                      # every node's world matrix, by id, sid and name

    joints = {}                                     # JOINT nodes: name -> position (scene pose)

    def collect(node, M):
        M = M @ node_matrix(node)
        if node.get('type') == 'JOINT' and (node.get('name') or node.get('sid')):
            joints.setdefault(node.get('name') or node.get('sid'), (AX @ M[:3, 3]) * metre)
        for k in (node.get('id'), node.get('sid'), node.get('name')):
            if k and k not in world: world[k] = M
        for ch in node.findall(T_('node')): collect(ch, M)

    def skinner(skin):
        """Posed positions of a skin's vertices: sum of weight x joint world x inverse bind x bind shape,
        with the joints where the scene has them (for a rip: its bind / T-pose)."""
        bsm = floats(skin.find(T_('bind_shape_matrix')))
        B = bsm.reshape(4, 4) if len(bsm) == 16 else np.eye(4)
        jel = skin.find(T_('joints'))
        jin = {i.get('semantic'): i.get('source') for i in jel.findall(T_('input'))} if jel is not None else {}
        names, ibm = [], None
        if 'JOINT' in jin:
            src = ids.get(jin['JOINT'].lstrip('#'))
            arr = src.find(T_('Name_array')) if src is not None else None
            if arr is None and src is not None: arr = src.find(T_('IDREF_array'))
            names = (arr.text or '').split() if arr is not None else []
        if 'INV_BIND_MATRIX' in jin:
            ibm = floats(ids.get(jin['INV_BIND_MATRIX'].lstrip('#')).find(T_('float_array'))).reshape(-1, 4, 4)
        vw = skin.find(T_('vertex_weights'))
        if not names or ibm is None or vw is None or any(n not in world for n in names):
            return lambda P: np.c_[P, np.ones(len(P))] @ B.T
        vin = vw.findall(T_('input'))
        jo = int(next(i for i in vin if i.get('semantic') == 'JOINT').get('offset'))
        wi = next(i for i in vin if i.get('semantic') == 'WEIGHT')
        wo = int(wi.get('offset')); wsrc = floats(ids.get(wi.get('source').lstrip('#')).find(T_('float_array')))
        st = max(int(i.get('offset')) for i in vin) + 1
        vc = ints(vw.find(T_('vcount'))); v = ints(vw.find(T_('v')))
        J = np.array([world[n] @ ibm[k] @ B for k, n in enumerate(names)])

        def apply(P):
            Ph = np.c_[P, np.ones(len(P))]
            out_ = np.zeros((len(P), 4)); at = 0
            for i, c in enumerate(vc[:len(P)]):
                pairs = v[at * st:(at + c) * st].reshape(c, st); at += c
                tot = 0.0
                for row in pairs:
                    j, w = int(row[jo]), float(wsrc[row[wo]])
                    if j < 0: out_[i] += w * (B @ Ph[i]); tot += w; continue
                    out_[i] += w * (J[j] @ Ph[i]); tot += w
                out_[i] = out_[i] / tot if tot > 1e-9 else B @ Ph[i]
            return out_
        return apply

    def symbols_of(inst):
        return {im.get('symbol'): (im.get('target') or '').lstrip('#') for im in inst.iter(ns + 'instance_material')}

    def walk(node, M):
        M = M @ node_matrix(node)
        for inst in node.findall(T_('instance_geometry')):
            g = ref(inst.get('url'))
            if g is not None:
                m = mesh_of(g, M, symbols_of(inst), node.get('name') or g.get('id'))
                if m: out.append(m)
        for inst in node.findall(T_('instance_controller')):
            c = ref(inst.get('url'))
            skin = c.find(T_('skin')) if c is not None else None
            if skin is None: continue
            g = ref(skin.get('source'))
            if g is not None:                      # skinned: posed by its joints, not by this node
                sk = skinner(skin)                 # then the node's own place (Blender's exporters put the
                m = mesh_of(g, None, symbols_of(inst), node.get('name') or g.get('id'),   # stand-up turn there)
                            skin=lambda P, sk=sk, M=M: sk(P) @ M.T)
                if m: out.append(m)
        for ch in node.findall(T_('node')):
            walk(ch, M)
    scene = root.find(T_('scene'))
    vs = ref(scene.find(T_('instance_visual_scene')).get('url')) if scene is not None and scene.find(T_('instance_visual_scene')) is not None else None
    if vs is not None:
        for n in vs.findall(T_('node')): collect(n, np.eye(4))
        for n in vs.findall(T_('node')): walk(n, np.eye(4))
    if not out:                                     # no scene: every geometry as it is
        for g in root.iter(ns + 'geometry'):
            m = mesh_of(g, np.eye(4), {}, g.get('name') or g.get('id'))
            if m: out.append(m)
    if not out:
        raise SkinError('no mesh found in %s' % os.path.basename(path))
    return out, materials, {k: [float(x) for x in v] for k, v in joints.items()}


# =============================================================================================
# Blender side
# =============================================================================================
if HAVE_BPY:
    RIG, REFMESH, FITRIG, COLL = 'PE_Aya_Rig', 'PE_Aya_Reference', 'PE_Aya_FitRig', 'PE Aya Reference'
    _GAME = {}

    def game_for(context):
        s = context.scene.pe_aya
        key = (bpy.path.abspath(s.disc), int(s.bank))
        if key not in _GAME:
            _GAME.clear()
            _GAME[key] = GameAya(*key)
        g = _GAME[key]
        rig = bpy.data.objects.get(RIG)
        oz = list(rig['pe_oz']) if rig and 'pe_oz' in rig and rig.get('pe_bank') == g.bank else None
        g.set_oz(oz if oz and len(oz) == g.NP else None)
        return g

    def mat4(R, t):
        """Bone matrix (Blender, armature space) for a game part frame (R, t): the bone's x = part a,
        y = -part c (towards the children), z = part b."""
        Rb = G2B @ np.asarray(R, float)
        tb = G2B @ np.asarray(t, float) / SC
        m = Matrix((Rb[:, 0].tolist(), (-Rb[:, 2]).tolist(), Rb[:, 1].tolist())).transposed().to_4x4()
        m.translation = Vector(tb.tolist())
        return m

    def collection():
        c = bpy.data.collections.get(COLL)
        if not c:
            c = bpy.data.collections.new(COLL)
            bpy.context.scene.collection.children.link(c)
        return c

    def remove_object(name):
        o = bpy.data.objects.get(name)
        if o:
            data = o.data
            bpy.data.objects.remove(o)
            if data is not None and data.users == 0:
                (bpy.data.armatures if isinstance(data, bpy.types.Armature) else bpy.data.meshes).remove(data)

    def set_active(obj, mode='OBJECT'):
        if bpy.context.object and bpy.context.object.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')
        for o in bpy.context.selected_objects: o.select_set(False)
        obj.hide_set(False); obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        if mode != 'OBJECT': bpy.ops.object.mode_set(mode=mode)

    def make_rig(game, name=RIG):
        remove_object(name)
        arm = bpy.data.armatures.new(name)
        ob = bpy.data.objects.new(name, arm)
        collection().objects.link(ob)
        ob.show_in_front = True
        arm.display_type = 'STICK'
        ob['pe_bank'] = game.bank
        set_active(ob, 'EDIT')
        seg = rest_segments(game)
        eb = {}
        for p in range(game.NP):
            R, t = game.idle[p]
            b = arm.edit_bones.new(bone_name(p))
            b.tail = (0.0, 0.1, 0.0)             # a new bone has no length: Blender ignores a matrix set on it
            b.matrix = mat4(R, t)
            L = 0.04
            if p in seg:
                h, tl = seg[p]
                L = max(0.03, float(np.dot(tl - h, np.array(b.matrix.col[1][:3]))))
            b.length = L
            b.use_deform = bool(game.visible[p])
            eb[p] = b
        for p in range(game.NP):
            if game.parent[p] >= 0:
                eb[p].parent = eb[game.parent[p]]; eb[p].use_connect = False
        bpy.ops.object.mode_set(mode='OBJECT')
        return ob

    PART_COLOURS = [(0.85, 0.3, 0.3), (0.3, 0.75, 0.35), (0.3, 0.45, 0.9), (0.9, 0.8, 0.25), (0.75, 0.35, 0.85),
                    (0.3, 0.8, 0.85), (0.95, 0.55, 0.2), (0.55, 0.55, 0.55)]

    def make_reference(game):
        remove_object(REFMESH)
        W = to_blender(game.idle_vertices())
        faces = []
        for idx, quad in game.faces():
            f = [idx[0], idx[1], idx[3], idx[2]] if quad else list(idx)
            faces.append(f if game.order_outward else f[::-1])      # Blender: counter-clockwise outside
        me = bpy.data.meshes.new(REFMESH)
        me.from_pydata(W.tolist(), [], faces)
        me.validate(); me.update()
        ob = bpy.data.objects.new(REFMESH, me)
        collection().objects.link(ob)
        mats = {}
        for p in range(game.NP):
            if not game.visible[p]: continue
            m = bpy.data.materials.get('PE part ' + bone_name(p)) or bpy.data.materials.new('PE part ' + bone_name(p))
            c = PART_COLOURS[p % len(PART_COLOURS)]
            m.diffuse_color = (*c, 1.0)
            me.materials.append(m); mats[p] = len(me.materials) - 1
        for poly in me.polygons:
            ps = [int(game.vpart[v]) for v in poly.vertices]
            poly.material_index = mats.get(max(set(ps), key=ps.count), 0)
        for p in range(game.NP):
            idx = np.where(game.vpart == p)[0].tolist()
            if idx and game.visible[p]:
                ob.vertex_groups.new(name=bone_name(p)).add(idx, 1.0, 'REPLACE')
        ob.display_type = 'SOLID'
        ob.show_wire = True
        ob.hide_select = True
        ob.hide_render = True
        return ob

    def mesh_arrays(obj):
        """World vertices, normals, edges of a mesh object (modifiers not applied)."""
        me = obj.data
        n = len(me.vertices)
        co = np.zeros(n * 3); me.vertices.foreach_get('co', co)
        co = co.reshape(-1, 3)
        M = np.array(obj.matrix_world)
        V = co @ M[:3, :3].T + M[:3, 3]
        nr = np.zeros(n * 3); me.vertices.foreach_get('normal', nr)
        nr = nr.reshape(-1, 3) @ np.linalg.inv(M[:3, :3])
        nr /= np.maximum(np.linalg.norm(nr, axis=1, keepdims=True), 1e-9)
        ed = np.zeros(len(me.edges) * 2, int); me.edges.foreach_get('vertices', ed)
        return V, nr, ed.reshape(-1, 2)

    def side_left(game):
        W = to_blender(game.idle_vertices())
        return 1.0 if W[game.vpart == 4, 0].mean() > to_blender(game.idle[1][1])[0] else -1.0

    def write_groups(obj, part):
        for g in list(obj.vertex_groups):
            if part_of_name(g.name) is not None: obj.vertex_groups.remove(g)
        for p in sorted(set(part.tolist())):
            obj.vertex_groups.new(name=bone_name(p)).add(np.where(part == p)[0].tolist(), 1.0, 'REPLACE')

    def attach_to_rig(obj):
        rig = bpy.data.objects.get(RIG)
        md = next((m for m in obj.modifiers if m.type == 'ARMATURE'), None) or obj.modifiers.new('PE Aya', 'ARMATURE')
        md.object = rig
        md.use_vertex_groups = True

    def part_per_vertex(obj, names=None):
        """Part per vertex from the bone vertex groups (strongest group wins); -1 = none.
        Groups on bones the game doesn't draw (fingers, hip bases) go to the nearest drawn parent."""
        game = game_for(bpy.context)
        gpart = {}
        for g in obj.vertex_groups:
            p = part_of_name(g.name)
            if p is None or p >= game.NP: continue
            while p > 0 and not game.visible[p]: p = game.parent[p]
            gpart[g.index] = p
        me = obj.data
        out = -np.ones(len(me.vertices), int)
        best = np.zeros(len(me.vertices))
        for v in me.vertices:
            for ge in v.groups:
                p = gpart.get(ge.group)
                if p is not None and ge.weight > best[v.index]:
                    best[v.index] = ge.weight; out[v.index] = p
        return out

    def user_mesh(context):
        s = context.scene.pe_aya
        obj = s.model or (context.object if context.object and context.object.type == 'MESH' else None)
        if obj is None or obj.type != 'MESH' or obj.name == REFMESH:
            raise SkinError('Pick your model in "Model" (one mesh object; join the pieces with Ctrl+J)')
        return obj

    # ---------------------------------------------------------------- steps
    def step_reference(context):
        remove_object(RIG)                       # a new reference starts from Aya's own bone lengths
        game = game_for(context)
        make_rig(game)
        make_reference(game)
        return 'Reference loaded: bank %d (%s), %d parts' % (game.bank, BANKS[game.bank], game.NP)

    def step_prepare(context):
        """Make the selected meshes one object with no own armature: pose applied, transforms applied."""
        obj = user_mesh(context)
        sel = [o for o in context.selected_objects if o.type == 'MESH' and o.name != REFMESH] or [obj]
        if obj not in sel: sel.append(obj)
        joints = {}
        for o in sel:
            if 'pe_joints' in o:
                joints.update({k: list(v) for k, v in o['pe_joints'].items()})
            # the model's own skeleton (FBX, glTF): where its joints are, for the fitting rig to snap to
            for m_ in o.modifiers:
                if m_.type == 'ARMATURE' and m_.object and m_.object.name not in (RIG, FITRIG) and m_.object.type == 'ARMATURE':
                    a = m_.object
                    for pb in a.pose.bones:
                        joints.setdefault(pb.name, list(a.matrix_world @ pb.head))
            # bake the model as it looks now (its own skeleton's pose, the shape keys' current mix, its
            # modifiers) into a plain mesh: Blender can't apply a modifier to a mesh with shape keys
            own = [m for m in o.modifiers if not (m.type == 'ARMATURE' and m.object and m.object.name == RIG)]
            if own or o.data.shape_keys:
                ours = [(m, m.show_viewport) for m in o.modifiers if m not in own]
                for m, _ in ours: m.show_viewport = False
                dg = context.evaluated_depsgraph_get()
                me = bpy.data.meshes.new_from_object(o.evaluated_get(dg), preserve_all_data_layers=True, depsgraph=dg)
                for m, v in ours: m.show_viewport = v
                old = o.data
                o.data = me
                for m in own: o.modifiers.remove(m)
                if old.users == 0: bpy.data.meshes.remove(old)
            if o.parent and o.parent.type == 'ARMATURE':
                mw = o.matrix_world.copy(); o.parent = None; o.matrix_world = mw
        set_active(obj)
        for o in sel: o.select_set(True)
        if len(sel) > 1:
            bpy.ops.object.join()
        obj = context.view_layer.objects.active
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        if joints: obj['pe_joints'] = joints             # world positions (the object is at the origin now)
        context.scene.pe_aya.model = obj
        return obj

    def step_fit(context):
        obj = step_prepare(context)
        game = game_for(context)
        ref = to_blender(game.idle_vertices())[np.isin(game.vpart, [p for p in range(game.NP) if game.visible[p]])]
        V, _, _ = mesh_arrays(obj)
        h_ref = ref[:, 2].max() - ref[:, 2].min()
        h_me = V[:, 2].max() - V[:, 2].min()
        if h_me <= 1e-6: raise SkinError('the model is flat')
        k = h_ref / h_me
        Vs = V * k

        def legs_centre(P):
            """Middle of the lower legs (15-40 % of the height): where the body stands. The whole
            model's box would be pulled back by a braid, ponytail or cape, putting the skeleton
            behind the body (head joint at the back of the skull, spine along the back)."""
            z0 = P[:, 2].min(); h = P[:, 2].max() - z0
            band = P[(P[:, 2] > z0 + 0.15 * h) & (P[:, 2] < z0 + 0.40 * h)]
            if len(band) < 8: band = P
            return np.array([(band[:, 0].min() + band[:, 0].max()) / 2, (band[:, 1].min() + band[:, 1].max()) / 2, z0])
        c_ref, c_me = legs_centre(ref), legs_centre(Vs)
        M = Matrix.Translation(Vector((c_ref - c_me).tolist())) @ Matrix.Scale(k, 4)
        obj.data.transform(M)
        if 'pe_joints' in obj:
            obj['pe_joints'] = {k: list(M @ Vector(v)) for k, v in obj['pe_joints'].items()}
        obj.data.update()
        return 'Fitted: scaled x%.3f, height %.2f m, feet on the reference floor' % (k, h_ref)

    def aim(rig, p, target):
        """Turn bone p (pose) about its head so its children's joint goes towards target (world)."""
        pb = rig.pose.bones[bone_name(p)]
        head = Vector(pb.head)
        kids = [c for c in pb.children if part_of_name(c.name) is not None]
        tip = Vector(np.mean([np.array(c.head) for c in kids], axis=0).tolist()) if kids else Vector(pb.tail)
        a, b = (tip - head), (Vector(target) - head)
        if a.length < 1e-6 or b.length < 1e-6: return
        q = a.normalized().rotation_difference(b.normalized())
        pb.matrix = Matrix.Translation(head) @ q.to_matrix().to_4x4() @ Matrix.Translation(-head) @ pb.matrix
        bpy.context.view_layer.update()

    def outer_cluster(pts, side, cell=0.012):
        """Points of a horizontal slice grouped by touching 1.2 cm cells (x, y); the group farthest out
        on this side, when the slice has more than one (else None: the arm touches the body here)."""
        key = np.floor(pts[:, :2] / cell).astype(int)
        cells = {}
        for i, k in enumerate(map(tuple, key)): cells.setdefault(k, []).append(i)
        comp, groups = {}, []
        for c in cells:
            if c in comp: continue
            comp[c] = len(groups); stack = [c]; members = []
            while stack:
                x = stack.pop(); members += cells[x]
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        y = (x[0] + dx, x[1] + dy)
                        if y in cells and y not in comp: comp[y] = comp[c]; stack.append(y)
            groups.append(members)
        if len(groups) < 2: return None
        out = max(groups, key=lambda g: pts[g, 0].mean() * side)
        if len(out) < 4 or len(out) > 0.7 * len(pts): return None
        return pts[out]

    def auto_fit_arms(rig, obj, game):
        """Aim the fitting rig's arms at the model's arms. Hanging / A-pose arms: horizontal slices,
        the arm = the slice's group of points farthest out (apart from the body), its centres make a
        line; T-pose arms: the points out from the shoulder. Elbow and wrist sit at the game's bone
        lengths along the arm. Returns the sides fitted."""
        V, _, _ = mesh_arrays(obj)
        done = []
        seg = rest_segments(game)
        sl = side_left(game)
        hip_z = to_blender(game.idle[20][1])[2]
        root_x = to_blender(game.idle[1][1])[0]
        cx = V[:, 0] - root_x
        for side, (u, f, h) in ((sl, (4, 5, 6)), (-sl, (14, 15, 16))):
            sh = np.array(rig.pose.bones[bone_name(u)].head)
            L1 = np.linalg.norm(to_blender(game.idle[f][1]) - to_blender(game.idle[u][1]))
            L2 = np.linalg.norm(to_blender(game.idle[h][1]) - to_blender(game.idle[f][1]))
            L3 = np.linalg.norm(seg[h][1] - seg[h][0]) if h in seg else 0.6 * L2   # wrist to fingertip
            line, gap = [], 0
            for zs in np.arange(sh[2] - 0.05, sh[2] - 1.4 * (L1 + L2 + L3), -0.01):
                pts = V[(np.abs(V[:, 2] - zs) < 0.006) & (cx * side > 0.0)]
                if len(pts) < 6: continue
                a_pts = outer_cluster(pts, side)
                if a_pts is not None and (a_pts[:, 0].mean() - root_x) * side > 0.06:
                    c = a_pts.mean(0)
                    if line and np.linalg.norm((c - line[-1])[:2]) > 0.03 + 0.01 * gap:
                        break                           # jumped to another limb (the leg below the hand)
                    line.append(c); gap = 0
                elif line:
                    gap += 1
            if len(line) >= 6:
                line = np.array([sh] + line)
                seg_len = np.linalg.norm(np.diff(line, axis=0), axis=1)
                arc = np.r_[0, np.cumsum(seg_len)]
            if len(line) >= 6 and arc[-1] > L1 + 0.5 * L2:
                def at(s):
                    i = int(np.clip(np.searchsorted(arc, s) - 1, 0, len(line) - 2))
                    t = (s - arc[i]) / max(seg_len[i], 1e-9)
                    return line[i] + (line[i + 1] - line[i]) * min(max(t, 0), 1.5)
                # the line runs to the fingertips: split the model's own arm as the game's is split, so
                # longer or shorter forearms bend at their own elbow
                k = arc[-1] / (L1 + L2 + L3)
                elbow, wrist = at(L1 * k), at((L1 + L2) * k)
                tip = line[-1]
            else:
                # T-pose: the points out past the shoulder are the arm
                x = (V[:, 0] - sh[0]) * side
                arm = V[(x > 0.06) & (V[:, 2] > hip_z - 0.25) & (V[:, 2] < sh[2] + 0.25)]
                if len(arm) < 50: continue
                d = arm - sh
                dist = np.linalg.norm(d, axis=1)
                dirn = (d / np.maximum(dist[:, None], 1e-9))[dist > np.percentile(dist, 30)].mean(0)
                if np.linalg.norm(dirn) < 0.5: continue
                dirn /= np.linalg.norm(dirn)
                # the model's own arm length (shoulder to fingertip), split as the game's is
                k = float(np.clip(np.percentile(dist, 99.5) / (L1 + L2 + L3), 0.6, 1.6))
                ring = arm[np.abs(dist - L1 * k) < 0.025]
                elbow = ring.mean(0) if len(ring) >= 8 else sh + dirn * L1 * k
                de = np.linalg.norm(arm - elbow, axis=1)
                ring = arm[(np.abs(de - L2 * k) < 0.025) & (dist > L1 * k)]
                wrist = ring.mean(0) if len(ring) >= 8 else elbow + (elbow - sh) / max(np.linalg.norm(elbow - sh), 1e-6) * L2 * k
                beyond = arm[(de > L2 * k + 0.03) & (dist > L1 * k)]
                tip = beyond.mean(0) if len(beyond) >= 8 else wrist + (wrist - elbow)
            aim(rig, u, elbow)
            aim(rig, f, wrist)
            aim(rig, h, tip)
            done.append('left' if side == sl else 'right')
        return done

    def joints_by_position(rig, J, CH, mid, sl, reach=0.08):
        """The model's skeleton names its bones in no way joint_kind knows (bone000, Bone.012, ...):
        each joint of the fitting rig, already aimed at the model's limbs, takes the model's joint
        nearest to it on the same side, within `reach` metres. One model joint per rig joint."""
        found, used = {}, set()
        names = list(J)
        P = np.array([J[n] for n in names]) if names else np.zeros((0, 3))
        for key, part, _ in CH:
            if not len(P): break
            pos = np.array(rig.pose.bones[bone_name(part)].head)
            d = np.linalg.norm(P - pos, axis=1)
            if key[0] in 'LR':
                side = 1 if key[0] == 'L' else -1
                d[(P[:, 0] - mid) * sl * side < 0.01] = np.inf
            for i in np.argsort(d):
                if not np.isfinite(d[i]) or d[i] > reach: break
                k = tuple(np.round(P[i], 4))
                if k in used: continue
                used.add(k)
                found[key] = (names[i], P[i])
                break
        # Elbows and knees: once both ends of the limb are matched (shoulder and wrist, hip and ankle),
        # the middle joint is the model's joint lying between them along the limb, not the one nearest
        # a guess made from Aya's proportions (a model with longer thighs has its knee 8 cm lower, past
        # `reach`, and the shin was left pointing nowhere).
        for S in 'LR':
            for a_, m_, b_ in (('upperarm', 'forearm', 'hand'), ('thigh', 'shin', 'foot')):
                ka, km, kb = S + a_, S + m_, S + b_
                if ka not in found or kb not in found: continue
                A, B = found[ka][1], found[kb][1]
                ab = B - A
                L = float(np.linalg.norm(ab))
                if L < 1e-4: continue
                t = ((P - A) @ ab) / (L * L)
                off = np.linalg.norm(P - (A + t[:, None] * ab), axis=1)
                ok = (t > 0.2) & (t < 0.8) & (off < 0.25 * L)
                if not ok.any(): continue
                i = int(np.argmin(np.where(ok, off + 0.05 * np.abs(t - 0.5) * L, np.inf)))
                found[km] = (names[i], P[i])
        return found

    def auto_fit_legs(rig, obj, game):
        """Fit the fitting rig's legs to the model's legs: horizontal cross-sections from below the
        crotch to the floor, each side's centres make a line. The hip joint moves onto the top of that
        line (a model's legs can be set wider or narrower than Aya's); the ankle is where the line is
        as high above the floor as the game's ankle; the knee splits hip..ankle as the game's thigh and
        shin do, so a model with longer or shorter thighs bends at its own knee and the bone-length
        fit (Pose into game stance) follows it. The foot points straight down to the sole, as the
        game's does. Returns the sides fitted."""
        V, _, E = mesh_arrays(obj)
        done = []
        sl = side_left(game)
        root_x = to_blender(game.idle[1][1])[0]
        z0 = V[:, 2].min()
        Ea, Eb = V[E[:, 0]], V[E[:, 1]]

        def cut(zs):
            """Where the mesh's edges cross the height zs: a whole cross-section even on a low-poly
            model, whose long leg faces leave a slice of vertices nearly empty."""
            da, db = Ea[:, 2] - zs, Eb[:, 2] - zs
            m = (da * db < 0) | ((da == 0) & (db != 0))
            k = (da[m] / (da[m] - db[m]))[:, None]
            return Ea[m] + (Eb[m] - Ea[m]) * k
        for side, (t, s_, ft) in ((sl, (22, 23, 24)), (-sl, (26, 27, 28))):
            hip = np.array(rig.pose.bones[bone_name(t)].head)
            L1 = np.linalg.norm(to_blender(game.idle[s_][1]) - to_blender(game.idle[t][1]))
            L2 = np.linalg.norm(to_blender(game.idle[ft][1]) - to_blender(game.idle[s_][1]))
            x = (V[:, 0] - root_x) * side
            line = []
            for zs in np.arange(hip[2] - 0.10, z0 + 0.06, -0.01):
                pts = cut(zs)
                pts = pts[(pts[:, 0] - root_x) * side > 0.005]
                if len(pts) < 4: continue
                # a hand hanging beside the thigh, a skirt or a cape is not the leg: the leg is near the
                # hip joint, then near the last slice's centre
                ref = line[-1] if line else hip
                pts = pts[np.linalg.norm((pts - ref)[:, :2], axis=1) < (0.10 if line else 0.12)]
                if len(pts) < 4: continue
                c = pts.mean(0)
                if line and np.linalg.norm((c - line[-1])[:2]) > 0.05:
                    continue
                line.append(c)
            if len(line) < 10: continue
            # the hip joint over the leg: the top slices' centre, at the joint's own height
            top = np.mean(line[:4], axis=0)
            pb = rig.pose.bones[bone_name(t)]
            pb.matrix = Matrix.Translation(Vector((top[0] - hip[0], top[1] - hip[1], 0.0))) @ pb.matrix
            bpy.context.view_layer.update()
            hip = np.array(pb.head)
            # the ankle: where the leg's line comes down to the game's ankle height above the floor
            ankle_h = to_blender(game.idle[ft][1])[2] - to_blender(game.idle_vertices())[:, 2].min()
            line = np.array(line)
            low = np.where(line[:, 2] <= z0 + ankle_h)[0]
            end = int(low[0]) if len(low) else len(line) - 1
            line = np.vstack([hip, line[:end + 1]])
            seg_len = np.linalg.norm(np.diff(line, axis=0), axis=1)
            arc = np.r_[0, np.cumsum(seg_len)]
            if arc[-1] < 0.5 * (L1 + L2): continue

            def at(s):
                i = int(np.clip(np.searchsorted(arc, s) - 1, 0, len(line) - 2))
                tt = (s - arc[i]) / max(seg_len[i], 1e-9)
                return line[i] + (line[i + 1] - line[i]) * min(max(tt, 0), 1.0)
            knee, ankle = at(arc[-1] * L1 / (L1 + L2)), line[-1]
            aim(rig, t, knee)
            aim(rig, s_, ankle)
            # the game's foot bone runs from the ankle straight down to the sole (not to the toes):
            # aimed at the toe tip, posing turned the feet ~45 degrees, toes down
            aim(rig, ft, np.array([ankle[0], ankle[1], z0]))
            done.append('left' if side == sl else 'right')
        return done

    def snap_to_joints(rig, obj, game, by_position=False):
        """The model brought its own skeleton (a .dae, an FBX or glTF with an armature): put the fitting
        rig's shoulders, elbows, wrists, neck, head, hips, knees and ankles on its joints, each bone aimed
        at the next joint. Which side is which comes from where the joint is, not its name."""
        J = {k: np.array(v, float) for k, v in obj['pe_joints'].items()}
        sl = side_left(game)
        mid = np.mean([v[0] for v in J.values()]) if J else 0.0
        found = {}
        for name, pos in J.items():
            kind = joint_kind(name)
            if not kind: continue
            if kind in ('neck', 'head'):
                key = kind
            else:
                key = ('L' if (pos[0] - mid) * sl > 0 else 'R') + kind
            if key not in found or len(name) < len(found[key][0]): found[key] = (name, pos)
        CH = [('neck', 10, 'head'), ('head', 11, None)]
        for S, (u, f, h, t, s_, ft) in (('L', (4, 5, 6, 22, 23, 24)), ('R', (14, 15, 16, 26, 27, 28))):
            CH += [(S + 'upperarm', u, S + 'forearm'), (S + 'forearm', f, S + 'hand'), (S + 'hand', h, None),
                   (S + 'thigh', t, S + 'shin'), (S + 'shin', s_, S + 'foot'), (S + 'foot', ft, None)]
        if not found and by_position:
            found = joints_by_position(rig, J, CH, mid, sl)
        done = []
        for key, part, nxt in CH:
            if key not in found: continue
            pb = rig.pose.bones[bone_name(part)]
            target = Vector(found[key][1].tolist())
            pb.matrix = Matrix.Translation(target - pb.head) @ pb.matrix       # joint onto the model's
            bpy.context.view_layer.update()
            if nxt and nxt in found:
                aim(rig, part, found[nxt][1])                                 # bone along the model's
            done.append(key)
        return done

    def step_fitrig(context):
        game = game_for(context)
        rig = make_rig(game, FITRIG)
        rig.show_in_front = True
        rig.data.display_type = 'OCTAHEDRAL'
        set_active(rig, 'POSE')
        obj = context.scene.pe_aya.model
        has_joints = obj is not None and 'pe_joints' in obj
        snapped = snap_to_joints(rig, obj, game) if has_joints else []
        auto = []
        if not snapped and obj:
            auto = ['%s arm' % s for s in auto_fit_arms(rig, obj, game)] + \
                   ['%s leg' % s for s in auto_fit_legs(rig, obj, game)]
            # a skeleton whose bone names say nothing (bone000...): its joints by where they are
            if has_joints: snapped = snap_to_joints(rig, obj, game, by_position=True)
        what = ('limbs aimed automatically (%s); ' % ', '.join(auto)) if auto else ''
        if snapped:
            what += 'joints placed on your model\'s own skeleton (%s); check them, ' % ', '.join(snapped)
        return ('Fitting rig made: %sin Pose mode turn (R) bones into your model\'s limbs and move (G) joints to its '
                'shoulders, elbows, wrists, hips, knees and ankles, then "Pose into game stance"' % what)

    def twist_leg_hinges(fit, obj, max_turn=90.0):
        """Turn the fitting rig's thighs, shins and feet about their own length so their hinge (the
        bone's x axis: the axis the game bends knees and ankles around) lies across the MODEL's knee
        and toes. The game's Aya stands with her feet turned out (right knee hinge ~19 deg, right
        ankle ~30-36 deg), and the fitting rig keeps that twist; a model with its knees and toes
        straight ahead was then bent sideways by every walk and run. Knees: across the model's
        front (-Y). Feet: across the model's own toe direction, from the shoe below the ankle.
        Returns what was turned."""
        Vw, _, _ = mesh_arrays(obj)
        inv = np.array(fit.matrix_world.inverted())
        V = Vw @ inv[:3, :3].T + inv[:3, 3]                 # model in the fitting rig's space
        done = []

        def turn(pb, target):
            M = np.array(pb.matrix)
            a = M[:3, 1] / max(np.linalg.norm(M[:3, 1]), 1e-9)      # along the bone
            x = M[:3, 0] - (M[:3, 0] @ a) * a
            t = np.asarray(target, float) - (np.asarray(target, float) @ a) * a
            if np.linalg.norm(t) < 1e-6 or np.linalg.norm(x) < 1e-6: return 0.0
            ang = math.atan2(np.cross(x, t) @ a, x @ t)
            ang = max(-math.radians(max_turn), min(math.radians(max_turn), ang))
            if abs(ang) < math.radians(1): return 0.0
            head = Vector(pb.head)
            pb.matrix = (Matrix.Translation(head) @ Matrix.Rotation(ang, 4, Vector(a.tolist()))
                         @ Matrix.Translation(-head) @ pb.matrix)
            bpy.context.view_layer.update()
            return math.degrees(ang)
        for name, (t, s_, ft) in (('left', (22, 23, 24)), ('right', (26, 27, 28))):
            names = [bone_name(p) for p in (t, s_, ft)]
            if not all(n in fit.pose.bones for n in names): continue
            th, sh, fo = (fit.pose.bones[n] for n in names)
            a1 = turn(th, (1.0, 0.0, 0.0))
            a2 = turn(sh, (1.0, 0.0, 0.0))
            # the shoe: the model below the ankle, near it, on this leg's side
            ank = np.array(fo.head)
            side = np.sign(ank[0]) or 1.0
            dz = ank[2] - V[:, 2]
            hd = np.linalg.norm((V - ank)[:, :2], axis=1)
            shoe = V[(dz > 0) & (hd < 0.20) & ((V[:, 0] * side) > 0)]
            toe = np.array([0.0, -1.0, 0.0])
            if len(shoe) >= 6:
                d = shoe.mean(0) - ank
                d[2] = 0
                if np.linalg.norm(d) > 0.01:
                    d /= np.linalg.norm(d)
                    if d @ toe > 0.5: toe = d                     # within 60 deg of straight ahead
            a3 = turn(fo, (-toe[1], toe[0], 0.0))                 # across the toes
            if any(abs(v) >= 1 for v in (a1, a2, a3)):
                done.append('%s knee %+.0f / ankle %+.0f deg' % (name, a2 + a1, a3))
        return done

    def step_pose(context):
        game = game_for(context)
        obj = user_mesh(context)
        fit = bpy.data.objects.get(FITRIG)
        rig = bpy.data.objects.get(RIG)
        if not fit or not rig: raise SkinError('Make the fitting rig first')
        if context.object and context.object.mode != 'OBJECT': bpy.ops.object.mode_set(mode='OBJECT')
        hinges = twist_leg_hinges(fit, obj)
        if hinges: print('leg hinges turned to the model\'s knees and toes: ' + ', '.join(hinges))

        def fit_matrix(pb):
            """Where the fitting rig has a bone, without scale: a bone scaled (S) instead of its joint
            moved (G) would scale its whole part of the model (arms 1.3x thicker); lengths come from
            where the joints are, not from scale."""
            loc, rot, _ = (fit.matrix_world @ pb.matrix).decompose()
            return Matrix.LocRotScale(loc, rot, None)
        Dm = {}
        for pb in fit.pose.bones:
            p = part_of_name(pb.name)
            if p is None: continue
            D = fit_matrix(pb) @ rig.data.bones[pb.name].matrix_local.inverted() @ rig.matrix_world.inverted()
            Dm[p] = np.array(D)
        V, N, E = mesh_arrays(obj)
        part = assign_bones(V, N, E, rest_segments(game), side_left(game), log=print, deform=lambda p: Dm[p], tree=major_tree(game))
        part = joint_sides(V, part, assign_bones.segments, log=print)
        nbr = {}
        for p in AUTO:
            q = game.parent[p]
            while q > 0 and q not in AUTO: q = game.parent[q]
            if q in AUTO:
                nbr.setdefault(p, set()).add(q); nbr.setdefault(q, set()).add(p)
        labels, W = smooth_weights(V, part, assign_bones.segments, nbr)
        # bone lengths where the fitting rig's joints are: each joint measured along its parent's axis
        F = {}
        for pb in fit.pose.bones:
            p = part_of_name(pb.name)
            if p is not None: F[p] = np.array(fit_matrix(pb))
        heads = {p: F[p][:3, 3] for p in F}
        caxis = {p: -F[p][:3, 1] / max(np.linalg.norm(F[p][:3, 1]), 1e-9) for p in F}   # bone y = -game z
        before = list(game.oz)
        oz = fit_lengths(game, heads, caxis)
        oz = [o if abs(o - b) >= 2 else b for o, b in zip(oz, before)]
        changed = ['%s %d->%d' % (bone_name(p)[3:], before[p], oz[p]) for p in range(game.NP) if oz[p] != before[p]]
        game.set_oz(oz)
        T = {p: np.array(mat4(*game.idle[p])) for p in range(game.NP)}
        M = {p: T[p] @ np.linalg.inv(F[p]) for p in F}   # from where the fitting rig has it to the game stance
        # A limb's joint the game's skeleton can't put where the model has it (the residual after the
        # length fit): the limb is moved by that much, so its skin meets the torso as on the model
        # instead of being stretched onto the game's joint. Its children move with it.
        tree = major_tree(game)
        shift = {}
        def depth(p):
            d = 0
            while p > 0: p = game.parent[p]; d += 1
            return d
        for p in sorted(F, key=depth):
            q = tree.get(p, 0)
            if p in LIMB_ROOTS and q in M:
                want = (M[q] @ np.r_[heads[p], 1.0])[:3]      # the model's joint, carried by the torso
                shift[p] = want - T[p][:3, 3]
            else:
                shift[p] = shift.get(q, np.zeros(3))
        Vh = np.c_[V, np.ones(len(V))]
        out = np.zeros_like(V)
        for j, p in enumerate(labels):
            out += W[:, j:j + 1] * ((Vh @ M[p].T)[:, :3] + shift.get(p, np.zeros(3)))
        Mi = np.array(obj.matrix_world.inverted())
        local = out @ Mi[:3, :3].T + Mi[:3, 3]
        obj.data.vertices.foreach_set('co', local.ravel())
        obj.data.update()
        rig = make_rig(game)                     # the skeleton with the skin's bone lengths
        rig['pe_oz'] = [int(x) for x in game.oz]
        write_groups(obj, part)
        attach_to_rig(obj)
        remove_object(FITRIG)
        set_active(obj)
        return 'Model bent into the game stance and its bones assigned%s: check it with Preview' % (
            ' (bone lengths: %s)' % ', '.join(changed) if changed else '')

    def hidden_faces(obj, reach, rays=32):
        """Faces no one can see: every ray from the face's centre into its outside half-space hits the
        model again within `reach` metres (a body under clothes, the scalp under hair, the inside of
        the mouth or of a boot). The game has no depth buffer and sorts faces in steps of 1.5 cm, so
        such faces draw through the layer over them. Faces facing a gap (eye holes, sleeves, the
        neck of a top) let some ray out and stay. Returns (bmesh in world space, hidden face indices,
        how many faces lie on another surface)."""
        from mathutils.bvhtree import BVHTree
        bm = bmesh.new(); bm.from_mesh(obj.data); bm.transform(obj.matrix_world)
        bm.faces.ensure_lookup_table(); bm.normal_update()
        tree = BVHTree.FromBMesh(bm)
        # directions spread evenly over a sphere (Fibonacci), the same every run
        golden = math.pi * (3.0 - math.sqrt(5.0))
        sphere = []
        for i in range(rays * 2):
            y = 1.0 - (i + 0.5) / rays
            r = math.sqrt(max(0.0, 1.0 - y * y))
            sphere.append(Vector((math.cos(golden * i) * r, y, math.sin(golden * i) * r)))
        hidden, lying = [], 0
        for f in bm.faces:
            n = f.normal
            if n.length < 0.5: continue
            c = f.calc_center_median() + n * 0.0008
            seen = False
            for d in sphere:
                if d.dot(n) < 0.1: continue
                hit = tree.ray_cast(c, d, reach)
                if hit[0] is None or hit[2] == f.index:
                    seen = True
                    break
            if not seen:
                hidden.append(f.index)
                continue
            hit = tree.ray_cast(f.calc_center_median() - n * 0.0005, -n, 0.04)
            if hit[0] is not None and hit[2] != f.index and bm.faces[hit[2]].normal.dot(n) > 0.5:
                lying += 1
        return bm, hidden, lying

    def step_clean(context, remove):
        obj = user_mesh(context)
        s = context.scene.pe_aya
        if context.object and context.object.mode != 'OBJECT': bpy.ops.object.mode_set(mode='OBJECT')
        size = max(obj.dimensions)
        if size > 5.0 or size < 0.5:
            raise SkinError('The model is %.2f m tall: run "Fit size to reference" first (the search '
                            'distance is in metres)' % size)
        bm, hidden, lying = hidden_faces(obj, s.clean_reach)
        bm.free()
        total = len(obj.data.polygons)
        note = ('; %d faces lie on another surface (straps, hair over the face): the export sorts them '
                'nearer' % lying) if lying else ''
        if not hidden:
            return 'No hidden faces in %d%s' % (total, note)
        keep = set(hidden)
        if not remove:
            for p in obj.data.polygons: p.select = p.index in keep
            for e in obj.data.edges: e.select = False
            for v in obj.data.vertices: v.select = False
            for p in obj.data.polygons:
                if p.select:
                    for vi in p.vertices: obj.data.vertices[vi].select = True
            for o in context.selected_objects: o.select_set(False)
            obj.select_set(True); context.view_layer.objects.active = obj
            bpy.ops.object.mode_set(mode='EDIT')
            bpy.ops.mesh.select_mode(type='FACE')
            return ('%d of %d faces are hidden (selected in Edit mode: deselect any to keep, X > Faces to '
                    'delete, or "Remove hidden faces")%s' % (len(hidden), total, note))
        bm = bmesh.new(); bm.from_mesh(obj.data); bm.faces.ensure_lookup_table()
        bmesh.ops.delete(bm, geom=[bm.faces[i] for i in hidden], context='FACES_ONLY')
        loose = [v for v in bm.verts if not v.link_faces]
        if loose: bmesh.ops.delete(bm, geom=loose, context='VERTS')
        bm.to_mesh(obj.data); bm.free(); obj.data.update()
        return 'Removed %d hidden faces: %d -> %d (Ctrl+Z to undo)%s' % (len(hidden), total, total - len(hidden), note)

    def step_assign(context):
        game = game_for(context)
        obj = user_mesh(context)
        if context.object and context.object.mode != 'OBJECT': bpy.ops.object.mode_set(mode='OBJECT')
        V, N, E = mesh_arrays(obj)
        part = assign_bones(V, N, E, rest_segments(game), side_left(game), log=print, tree=major_tree(game))
        part = joint_sides(V, part, assign_bones.segments, log=print, shoulders=False)
        write_groups(obj, part)
        attach_to_rig(obj)
        return 'Bones assigned (%d vertices). Fix any in Edit mode: select, Vertex Groups > Assign' % len(V)

    def step_play(context, row):
        game = game_for(context)
        rig = bpy.data.objects.get(RIG)
        if not rig: raise SkinError('Load the reference first')
        rows = dict(game_rows(game.model))
        for d in range(1, 9):
            for k, v in game_rows(game.disc.day(game.bank, d)).items():
                if k not in rows: rows[k] = (d, v)
        if row not in rows:
            raise SkinError('No row 0x%02X. Rows: %s' % (row, ' '.join('%02X' % k for k in sorted(rows))))
        src = rows[row]
        blob, off = (game.model, src) if not isinstance(src, tuple) else (game.disc.day(game.bank, src[0]), src[1])
        name = 'PE row %02X' % row
        act = bpy.data.actions.get(name)
        if act: bpy.data.actions.remove(act)
        rig.animation_data_create()
        act = bpy.data.actions.new(name)
        rig.animation_data.action = act
        n = row_frames(blob, off)
        bones = {part_of_name(pb.name): pb for pb in rig.pose.bones}
        order = sorted(bones, key=lambda q: len(bones[q].parent_recursive))
        for pb in rig.pose.bones: pb.rotation_mode = 'QUATERNION'
        for fr in range(n):
            mats = game.pose(*row_frame(blob, off, game.NP, fr))
            for p in order:
                bones[p].matrix = mat4(*mats[p])
                context.view_layer.update()
            for pb in rig.pose.bones:
                pb.keyframe_insert('rotation_quaternion', frame=fr + 1)
                pb.keyframe_insert('location', frame=fr + 1)
        context.scene.frame_start, context.scene.frame_end = 1, n
        context.scene.frame_set(1)
        return 'Row 0x%02X: %d frames (press Space to play). Rows: %s' % (row, n, ' '.join('%02X' % k for k in sorted(rows)))

    def step_rest(context):
        rig = bpy.data.objects.get(RIG)
        if rig:
            if rig.animation_data: rig.animation_data.action = None
            for pb in rig.pose.bones:
                pb.matrix_basis = Matrix()
        return 'Back to the game stance'

    # ---------------------------------------------------------------- export
    def image_pixels(img):
        """(H, W, 4) uint8, top row first, of a Blender image."""
        w, h = img.size
        px = np.zeros(w * h * img.channels, np.float32)
        img.pixels.foreach_get(px)
        px = px.reshape(h, w, img.channels)[::-1]
        if img.channels == 3: px = np.concatenate([px, np.ones((h, w, 1), np.float32)], 2)
        elif img.channels < 3: px = np.concatenate([np.repeat(px[..., :1], 3, 2), np.ones((h, w, 1), np.float32)], 2)
        return (np.clip(px, 0, 1) * 255).round().astype(np.uint8)

    def image_of(mat):
        """(H, W, 4) uint8 top-first picture of a material's base colour, or a flat colour."""
        if mat and mat.use_nodes and mat.node_tree:
            nodes = mat.node_tree.nodes
            bsdf = next((n for n in nodes if n.type == 'BSDF_PRINCIPLED'), None)
            img = None
            if bsdf:
                sock = bsdf.inputs[0]
                stack = [l.from_node for l in sock.links]
                seen = set()
                while stack and img is None:
                    nd = stack.pop()
                    if nd in seen: continue
                    seen.add(nd)
                    if nd.type == 'TEX_IMAGE' and nd.image: img = nd.image; break
                    for i in nd.inputs:
                        stack += [l.from_node for l in i.links]
            if img is None:
                img = next((n.image for n in nodes if n.type == 'TEX_IMAGE' and n.image), None)
            if img is not None and img.size[0] > 0:
                return image_pixels(img)
            image_of.missing.append(mat.name)
            if bsdf:
                c = bsdf.inputs[0].default_value
                lin = np.array(c[:3])
                srgb = np.where(lin <= 0.0031308, lin * 12.92, 1.055 * np.power(np.maximum(lin, 0), 1 / 2.4) - 0.055)
                return np.tile(np.r_[np.clip(srgb, 0, 1) * 255, 255].astype(np.uint8), (8, 8, 1))
        c = mat.diffuse_color if mat else (0.8, 0.8, 0.8, 1)
        return np.tile(np.array([c[0] * 255, c[1] * 255, c[2] * 255, 255], np.uint8), (8, 8, 1))

    def lying_faces(me):
        """Sort lift per face (0 = none): faces lying on another surface of the model facing the same
        way, closer than LIFT_GAP (a strap on the belly, hair over the ear, a belt on the skirt). The
        game sorts each face by its own average depth in steps of 1.5 cm with no depth buffer, so such
        a face and the bigger face under it traded places as the camera turned (flicker). The upper
        face sorts LIFT_SLOTS nearer; whatever is farther than LIFT_GAP keeps the plain sort."""
        from mathutils.bvhtree import BVHTree
        LIFT_GAP = 0.04     # metres in the model (the skin is scaled to Aya's 1.9 m)
        LIFT_SLOTS = 4      # about 6 cm
        bm = bmesh.new(); bm.from_mesh(me); bm.faces.ensure_lookup_table()
        tree = BVHTree.FromBMesh(bm)
        lift = [0] * len(bm.faces)
        for f in bm.faces:
            n = f.normal
            if n.length < 0.5: continue
            for c in [f.calc_center_median()] + [v.co * 0.5 + f.calc_center_median() * 0.5 for v in f.verts]:
                hit = tree.ray_cast(c - n * 0.0005, -n, LIFT_GAP)
                if hit[0] is not None and hit[2] != f.index and bm.faces[hit[2]].normal.dot(n) > 0.5:
                    lift[f.index] = LIFT_SLOTS
                    break
        bm.free()
        return lift

    def small_pieces(me):
        """Vertex mask of the mesh's small separate pieces (eyes, brows, buttons, earrings): pieces with
        under 5% of the vertices. They lie on a surface: decimation must keep them and the game must
        draw them on top."""
        n = len(me.vertices)
        if n == 0: return np.zeros(0, bool)
        co = np.zeros(n * 3); me.vertices.foreach_get('co', co)
        # pieces split only at texture seams (common in ripped models) touch at the same points: weld
        # them for this test, so only truly separate pieces count
        _, weld = np.unique(np.round(co.reshape(-1, 3) / 1e-4).astype(np.int64), axis=0, return_inverse=True)
        weld = weld.ravel(); nw = weld.max() + 1
        ed = np.zeros(len(me.edges) * 2, int); me.edges.foreach_get('vertices', ed)
        comp = components(nw, weld[ed.reshape(-1, 2)], np.ones(nw, bool))
        size = np.bincount(comp, minlength=nw)
        small_w = (size < max(4, 0.05 * nw)) & (size < size.max())
        # never more than a fifth of the model: the smallest pieces (eyes, brows) first
        order = np.argsort(size[comp], kind='stable')
        keep = np.zeros(nw, bool); budget = int(0.2 * nw)
        seen = set()
        for w in order:
            if not small_w[w] or comp[w] in seen: continue
            if keep.sum() + size[comp[w]] > budget: break
            keep[comp == comp[w]] = True; seen.add(comp[w])
        return keep[weld]

    def orient_faces(bm, game, log):
        """The game draws only the faces turned towards the camera (PS1 back-face culling), so a face
        wound the wrong way is a hole and its texture shows from inside. Each separate piece of the
        mesh gets one winding (Blender's Recalculate Outside), then is turned to face away from the
        game's skeleton if most of its area faces it. Double-sided pieces (each face has a reversed
        copy at the same place: hair cards, capes) are left as they are."""
        def key(f):
            return frozenset(tuple(round(c, 4) for c in v.co) for v in f.verts)
        count = {}
        for f in bm.faces:
            k = key(f); count[k] = count.get(k, 0) + 1
        segs = [(np.asarray(h, float), np.asarray(t, float)) for p, (h, t) in
                ((p, v) for p, v in rest_segments(game).items() if p != '_kids')]
        bm.faces.index_update()
        bm.faces.ensure_lookup_table()
        seen, recalc, flipped, kept = set(), 0, 0, 0
        for f0 in bm.faces:
            if f0.index in seen: continue
            island, stack = [], [f0]
            seen.add(f0.index)
            while stack:
                f = stack.pop(); island.append(f)
                for e in f.edges:
                    for g in e.link_faces:
                        if g.index not in seen: seen.add(g.index); stack.append(g)
            if sum(1 for f in island if count[key(f)] > 1) > len(island) // 2:
                kept += 1
                continue
            before = [f.normal.copy() for f in island]
            bmesh.ops.recalc_face_normals(bm, faces=island)
            if any(f.normal.dot(n) < 0 for f, n in zip(island, before)): recalc += 1
            C = np.array([f.calc_center_median() for f in island])
            Nf = np.array([f.normal for f in island])
            A = np.array([f.calc_area() for f in island])
            best_d = np.full(len(island), np.inf); best_p = np.zeros_like(C)
            for h, t in segs:
                d, near = seg_dist(C, h, t)
                m = d < best_d
                best_d[m] = d[m]; best_p[m] = near[m]
            out = float((A * ((C - best_p) * Nf).sum(1)).sum())
            if out < 0:
                bmesh.ops.reverse_faces(bm, faces=island); flipped += 1
        if recalc or flipped or kept:
            log('face orientation: %d pieces made consistent, %d turned to face out, %d double-sided pieces kept'
                % (recalc, flipped, kept))

    def export_mesh(obj, game, budget, log):
        """Triangulate n-gons, join triangles into quads, decimate to the budget (small pieces kept)."""
        dg = bpy.context.evaluated_depsgraph_get()
        mods = [(m, m.show_viewport) for m in obj.modifiers if m.type == 'ARMATURE']
        for m, _ in mods: m.show_viewport = False
        tmp = None
        try:
            ratio = 1.0
            for attempt in range(6):
                if tmp:
                    data = tmp.data; bpy.data.objects.remove(tmp); bpy.data.meshes.remove(data)
                tmp = obj.copy(); tmp.data = obj.data.copy()
                bpy.context.scene.collection.objects.link(tmp)
                for m in list(tmp.modifiers):
                    if m.type == 'ARMATURE': tmp.modifiers.remove(m)
                if ratio < 1.0:
                    keep = np.where(small_pieces(tmp.data))[0].tolist()
                    d = tmp.modifiers.new('budget', 'DECIMATE'); d.ratio = ratio
                    if keep:                       # Decimate only collapses vertices outside the group
                        tmp.vertex_groups.new(name='pe_keep').add(keep, 1.0, 'REPLACE')
                        d.vertex_group = 'pe_keep'; d.invert_vertex_group = True
                dg = bpy.context.evaluated_depsgraph_get()
                me = bpy.data.meshes.new_from_object(tmp.evaluated_get(dg), preserve_all_data_layers=True, depsgraph=dg)
                me.transform(obj.matrix_world)
                bm = bmesh.new(); bm.from_mesh(me)
                big = [f for f in bm.faces if len(f.verts) > 4]
                if big: bmesh.ops.triangulate(bm, faces=big)
                orient_faces(bm, game, log if attempt == 0 else (lambda *a: None))
                tri = [f for f in bm.faces if len(f.verts) == 3]
                if tri:
                    bmesh.ops.join_triangles(bm, faces=tri, angle_face_threshold=math.radians(40),
                                             angle_shape_threshold=math.radians(40), cmp_uvs=True, cmp_materials=True)
                q = sum(1 for f in bm.faces if len(f.verts) == 4); t = len(bm.faces) - q
                bm.to_mesh(me)
                small = small_pieces(me)
                lifted = sum(1 for f in me.polygons if len(f.vertices) == 3 and small[list(f.vertices)].all())
                need = memory_need(q, t, game.NP) + 16 * lifted     # small pieces' triangles go as quads
                if budget <= 0 or need <= budget or attempt == 5:
                    break
                log('  %d quads + %d triangles = %d KB, over %d KB: decimating' % (q, t, need // 1024, budget // 1024))
                ratio *= (budget / need) * 0.96
                bm.free(); bpy.data.meshes.remove(me)
            if budget > 0 and need > budget:
                raise SkinError('could not reduce the model to the budget: decimate it yourself first')
            bm.to_mesh(me); bm.free()
        finally:
            if tmp:
                data = tmp.data; bpy.data.objects.remove(tmp); bpy.data.meshes.remove(data)
            for m, v in mods: m.show_viewport = v
        if ratio < 1.0:
            log('decimated to %d quads + %d triangles (ratio %.2f): a model you lower yourself looks better' % (q, t, ratio))
        return me

    def step_export(context, log=print):
        s = context.scene.pe_aya
        game = game_for(context)
        obj = user_mesh(context)
        if context.object and context.object.mode != 'OBJECT': bpy.ops.object.mode_set(mode='OBJECT')
        name = s.skin_name.strip() or 'Aya: Custom'
        out = bpy.path.abspath(s.out_dir)
        if not out: raise SkinError('Choose the output folder (…/Parasite Eve/pe_skins/<your skin>)')
        out = out.rstrip('/\\')
        folder = os.path.basename(out)
        if not folder or any(c in folder for c in ' \t') or len(folder) > 40:
            raise SkinError('The skin folder name must be one word up to 40 letters (e.g. my_aya): %r' % folder)
        budget = {'4': 4 * BLOCK, '8': 8 * BLOCK, 'HOST': 0}[s.budget]
        if s.budget == 'HOST':                 # host memory: only the format's own 65,535 faces / vertices
            budget = memory_need(60000, 0, game.NP)
        part = part_per_vertex(obj)
        if (part < 0).any():
            V, N, E = mesh_arrays(obj)
            miss = part < 0
            auto = assign_bones(V, N, E, rest_segments(game), side_left(game), log=lambda *_: None, tree=major_tree(game))
            part[miss] = auto[miss]
            log('%d vertices had no bone: assigned automatically' % int(miss.sum()))
        V_src = mesh_arrays(obj)[0]
        me = export_mesh(obj, game, budget, log)
        try:
            n = len(me.vertices)
            co = np.zeros(n * 3); me.vertices.foreach_get('co', co); co = co.reshape(-1, 3)
            nr = np.zeros(n * 3); me.vertices.foreach_get('normal', nr); nr = nr.reshape(-1, 3)
            # bone of each exported vertex = the bone of the nearest vertex of your model (decimation
            # moves vertices and would blend bone numbers)
            from mathutils.kdtree import KDTree
            kd = KDTree(len(V_src))
            for i, v in enumerate(V_src): kd.insert(v.tolist(), i)
            kd.balance()
            vp = np.array([part[kd.find(v.tolist())[1]] for v in co], int)
            ed = np.zeros(len(me.edges) * 2, int); me.edges.foreach_get('vertices', ed)
            vp = fix_spans(vp, ed.reshape(-1, 2), major_tree(game), log)
            small = small_pieces(me)
            if not me.uv_layers:
                raise SkinError('The model has no UV map: unwrap it (Edit mode > U) and give it a texture')
            uvl = me.uv_layers.active.data
            uvs = np.zeros(len(uvl) * 2); uvl.foreach_get('uv', uvs); uvs = uvs.reshape(-1, 2)
            image_of.missing = []
            if s.texture.strip():
                path = bpy.path.abspath(s.texture.strip())
                try:
                    img = bpy.data.images.load(path, check_existing=True)
                except RuntimeError:
                    raise SkinError('Texture not found: %s' % path)
                if img.size[0] == 0: raise SkinError('Texture could not be read: %s' % path)
                pics = [image_pixels(img)]
                log('texture: %s for every face' % os.path.basename(path))
            else:
                pics = [image_of(ms.material) for ms in obj.material_slots] or [image_of(None)]
                for mname in image_of.missing:
                    log('warning: material "%s" has no picture (flat colour). Link its Base Color to an '
                        'Image Texture, or set "Texture"' % mname)
            g = int(math.ceil(math.sqrt(len(pics))))
            A = 2048 if s.hd and len(pics) == 1 and max(pics[0].shape[:2]) >= 2048 else 1024
            cell = A // g
            atlas = np.zeros((A, A, 4), np.uint8)
            for i, pic in enumerate(pics):
                r, c = divmod(i, g)
                atlas[r * cell:(r + 1) * cell, c * cell:(c + 1) * cell] = resize_rgba(pic, cell) if pic.shape[:2] != (cell, cell) else pic
            faces, wrapped = [], 0
            for poly in me.polygons:
                li = list(poly.loop_indices)
                fu = uvs[li].copy()
                lo = np.floor(fu.min(0) + 1e-6)
                if (fu.max(0) - lo > 1.0 + 1e-4).any(): wrapped += 1
                fu = np.clip(fu - lo, 0, 1)
                r, c = divmod(min(poly.material_index, len(pics) - 1) if len(pics) > 1 else 0, g)
                fu = np.c_[(c + fu[:, 0]) / g, 1.0 - (r + 1.0 - fu[:, 1]) / g]
                faces.append([(int(me.loops[l].vertex_index), (float(fu[k, 0]), float(fu[k, 1]))) for k, l in enumerate(li)])
            if wrapped:
                log('warning: %d faces use a repeating (tiled) texture: they are clamped' % wrapped)
            if len(pics) > 1:
                log('%d materials packed into one texture (%dx%d grid)' % (len(pics), g, g))
            key = 'custom/' + folder
            overlay = [bool(small[list(p.vertices)].all()) for p in me.polygons]
            segs = rest_segments(game)
            outer = []
            for poly in me.polygons:
                vs = list(poly.vertices); ps = vp[vs]
                pb = int(np.bincount(ps).argmax())
                c = co[vs].mean(0)[None]
                outer.append(float(seg_dist(c, *segs[pb])[0][0]) if pb in segs else 0.0)
            lift = lying_faces(me)
            info = write_skin(game, dict(verts=co, normals=nr, part=vp, faces=faces, overlay=overlay, outer=outer,
                                         lift=lift), atlas, out, name, key, log,
                              hd_size=A if s.hd else 0)
            check_skin(out, game.NP)
        finally:
            bpy.data.meshes.remove(me)
        verdict = budget_text(info['need'])
        return 'Skin written to %s (%d quads, %d triangles; %s)' % (out, info['quads'], info['tris'], verdict)

    # ---------------------------------------------------------------- UI
    class PeAyaSettings(bpy.types.PropertyGroup):
        disc: bpy.props.StringProperty(name='Disc 1', subtype='FILE_PATH',
                                       description='Your Parasite Eve (USA) disc 1 image (.bin or .iso, not the .cue)')
        bank: bpy.props.EnumProperty(name='Skeleton', items=[(str(k), '%d: %s' % (k, v), '') for k, v in BANKS.items()],
                                     default='12', description='Which of Aya\'s outfits the skin is built on (its skeleton and animations)')
        model: bpy.props.PointerProperty(name='Model', type=bpy.types.Object, poll=lambda self, o: o.type == 'MESH')
        row: bpy.props.IntProperty(name='Animation row', default=IDLE_ROW, min=0, max=255,
                                   description='Game animation row to preview (hex in the reports)')
        skin_name: bpy.props.StringProperty(name='Name', default='Aya: My look', description='Name shown in the Wardrobe')
        out_dir: bpy.props.StringProperty(name='Folder', subtype='DIR_PATH',
                                          description='%APPDATA%/Parasite Eve/pe_skins/<one_word_name>/')
        budget: bpy.props.EnumProperty(name='Size', default='4', items=[
            ('4', 'Safe (stock game)', 'Fits the memory the game gives Aya: about 1,100 quads'),
            ('8', 'Large (risky)', 'Twice that: may not load in rooms with many actors'),
            ('HOST', 'Full detail (host memory)', 'No reduction (up to 65,535 faces): the skin draws from PC memory. Needs a game build with PE_HOOK_ACTOR_ALLOC (skins in host memory)')])
        texture: bpy.props.StringProperty(name='Texture', subtype='FILE_PATH',
                                          description='Optional: one picture for the whole model (its UV map), instead of the materials\' pictures')
        hd: bpy.props.BoolProperty(name='HD texture', default=False,
                                   description='Also write texture_hd.png (used only by game builds with HD skin support)')
        clean_reach: bpy.props.FloatProperty(name='Search distance', default=0.25, min=0.02, max=2.0, unit='LENGTH',
                                             description='A face is hidden when every ray from it hits the model again '
                                                         'within this distance. Larger finds more (deep inside), '
                                                         'smaller only faces right under another layer')

    def run(op, fn, *a):
        try:
            msg = fn(bpy.context, *a)
            op.report({'INFO'}, msg); print('[PE Aya Skin]', msg)
            return {'FINISHED'}
        except SkinError as e:
            op.report({'ERROR'}, str(e)); print('[PE Aya Skin] error:', e)
            return {'CANCELLED'}

    def _op(idname, label, fn, desc):
        def execute(self, context): return run(self, fn)
        return type('PEAYA_OT_' + idname, (bpy.types.Operator,), dict(
            bl_idname='pe_aya.' + idname, bl_label=label, bl_description=desc, bl_options={'REGISTER', 'UNDO'},
            execute=execute))

    OPS = [
        _op('reference', 'Load reference', step_reference, 'Read Aya\'s skeleton and mesh from your disc'),
        _op('fit', 'Fit size to reference', step_fit, 'Join your model, apply its pose and transforms, scale and move it onto the reference'),
        _op('clean_select', 'Select hidden faces', lambda c: step_clean(c, False),
            'Select the faces hidden inside the model (under clothes, hair, inside the mouth) in Edit mode, to check them'),
        _op('clean_remove', 'Remove hidden faces', lambda c: step_clean(c, True),
            'Delete the faces hidden inside the model: they flicker through the layer over them in game'),
        _op('fitrig', 'Make fitting rig', step_fitrig, 'A copy of the skeleton to pose into a T-pose / A-pose model'),
        _op('pose', 'Pose into game stance', step_pose, 'Bend the model from the fitting rig\'s pose into the game\'s stance'),
        _op('assign', 'Assign bones', step_assign, 'Give every vertex one game bone (vertex groups)'),
        _op('rest', 'Stop preview', step_rest, 'Back to the game stance'),
        _op('export', 'Export skin', step_export, 'Write the skin folder for the Wardrobe'),
    ]

    class PEAYA_OT_play(bpy.types.Operator):
        bl_idname = 'pe_aya.play'
        bl_label = 'Play game animation'
        bl_description = 'Load a game animation row onto the skeleton'
        step: bpy.props.IntProperty(default=0)

        def execute(self, context):
            s = context.scene.pe_aya
            s.row = max(0, min(255, s.row + self.step))
            return run(self, step_play, s.row)

    class PEAYA_PT_panel(bpy.types.Panel):
        bl_label = 'PE Aya Skin'
        bl_space_type = 'VIEW_3D'
        bl_region_type = 'UI'
        bl_category = 'PE Aya Skin'

        def draw(self, context):
            s = context.scene.pe_aya
            L = self.layout
            b = L.box(); b.label(text='1. Game reference')
            b.prop(s, 'disc'); b.prop(s, 'bank'); b.operator('pe_aya.reference')
            b = L.box(); b.label(text='2. Your model')
            b.operator('pe_aya.import_dae', icon='IMPORT')
            b.prop(s, 'model'); b.operator('pe_aya.fit')
            b = L.box(); b.label(text='Clean up (optional, after 2)')
            b.prop(s, 'clean_reach')
            r = b.row(align=True)
            r.operator('pe_aya.clean_select', text='Select hidden', icon='RESTRICT_SELECT_OFF')
            r.operator('pe_aya.clean_remove', text='Remove hidden', icon='TRASH')
            b = L.box(); b.label(text='3. Fit joints / pose a T-pose')
            b.operator('pe_aya.fitrig'); b.operator('pe_aya.pose')
            b = L.box(); b.label(text='4. Bones')
            b.operator('pe_aya.assign')
            b = L.box(); b.label(text='5. Preview')
            r = b.row(align=True)
            r.operator('pe_aya.play', text='', icon='TRIA_LEFT').step = -1
            r.prop(s, 'row', text='Row')
            r.operator('pe_aya.play', text='', icon='TRIA_RIGHT').step = 1
            b.operator('pe_aya.play', text='Play row').step = 0
            b.operator('pe_aya.rest')
            b = L.box(); b.label(text='6. Export')
            b.prop(s, 'skin_name'); b.prop(s, 'out_dir'); b.prop(s, 'texture'); b.prop(s, 'budget'); b.prop(s, 'hd')
            b.operator('pe_aya.export')

    def register():
        for c in CLASSES: bpy.utils.register_class(c)
        bpy.types.Scene.pe_aya = bpy.props.PointerProperty(type=PeAyaSettings)
        bpy.types.TOPBAR_MT_file_import.append(menu_import)

    def unregister():
        bpy.types.TOPBAR_MT_file_import.remove(menu_import)
        del bpy.types.Scene.pe_aya
        for c in reversed(CLASSES): bpy.utils.unregister_class(c)

    # ---------------------------------------------------------------- Collada import
    def build_dae(path):
        """Collada file -> mesh objects with their UVs and materials (an Image Texture on Base Color)."""
        meshes, mats, joints = read_dae(path)
        bmats = []
        for md in mats:
            mat = bpy.data.materials.new(md['name'])
            if not mat.node_tree: mat.use_nodes = True
            nt = mat.node_tree
            bsdf = next((n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED'), None)
            if md['image'] and bsdf:
                tn = nt.nodes.new('ShaderNodeTexImage')
                tn.image = bpy.data.images.load(md['image'], check_existing=True)
                nt.links.new(tn.outputs[0], bsdf.inputs[0])
                nt.links.new(tn.outputs[1], bsdf.inputs['Alpha'])
            elif bsdf:
                bsdf.inputs[0].default_value = (*md['colour'], 1.0)
            mat.diffuse_color = (*md['colour'], 1.0)
            bmats.append(mat)
        objs = []
        for md in meshes:
            me = bpy.data.meshes.new(md['name'])
            me.from_pydata(md['verts'].tolist(), [], md['faces'])
            if md['uvs'] is not None:
                uvl = me.uv_layers.new(name='UVMap')
                flat = [uv for f in md['uvs'] for uv in f]
                uvl.data.foreach_set('uv', np.array(flat, float).ravel())
            used = sorted(set(md['mats']))
            slot = {mi: k for k, mi in enumerate(used)}
            for mi in used: me.materials.append(bmats[mi])
            me.polygons.foreach_set('material_index', [slot[mi] for mi in md['mats']])
            me.validate(); me.update()
            ob = bpy.data.objects.new(md['name'], me)
            bpy.context.scene.collection.objects.link(ob)
            if joints: ob['pe_joints'] = joints           # the model's own skeleton: the fitting rig snaps to it
            objs.append(ob)
        if bpy.context.object and bpy.context.object.mode != 'OBJECT': bpy.ops.object.mode_set(mode='OBJECT')
        for o in bpy.context.selected_objects: o.select_set(False)
        for o in objs: o.select_set(True)
        bpy.context.view_layer.objects.active = objs[0]
        missing = [m['name'] for m in mats if not m['image']]
        return objs, missing

    from bpy_extras.io_utils import ImportHelper

    class PEAYA_OT_import_dae(bpy.types.Operator, ImportHelper):
        bl_idname = 'pe_aya.import_dae'
        bl_label = 'Import Collada (.dae)'
        bl_description = 'Read a Collada model (Blender 5 has no importer of its own): mesh in its bind pose, UVs, textures'
        bl_options = {'REGISTER', 'UNDO'}
        filename_ext = '.dae'
        filter_glob: bpy.props.StringProperty(default='*.dae', options={'HIDDEN'})

        def execute(self, context):
            try:
                objs, missing = build_dae(self.filepath)
            except (SkinError, ET_ERRORS) as e:
                self.report({'ERROR'}, 'Could not read %s: %s' % (os.path.basename(self.filepath), e))
                return {'CANCELLED'}
            context.scene.pe_aya.model = objs[0]
            msg = 'Imported %d mesh%s from %s' % (len(objs), 'es' if len(objs) > 1 else '', os.path.basename(self.filepath))
            if missing: msg += '; no picture found for material(s) %s: set "Texture" at export' % ', '.join(missing)
            self.report({'INFO'}, msg); print('[PE Aya Skin]', msg)
            return {'FINISHED'}

    def menu_import(self, context):
        self.layout.operator(PEAYA_OT_import_dae.bl_idname, text='Collada (.dae) - PE Aya Skin')

    # ---------------------------------------------------------------- command line
    def import_model(path):
        before = set(bpy.data.objects)
        ext = os.path.splitext(path)[1].lower()
        if ext == '.fbx': bpy.ops.import_scene.fbx(filepath=path)
        elif ext == '.obj': bpy.ops.wm.obj_import(filepath=path)
        elif ext in ('.glb', '.gltf'): bpy.ops.import_scene.gltf(filepath=path)
        elif ext == '.dae': build_dae(path)
        elif ext == '.blend':
            with bpy.data.libraries.load(path) as (src, dst): dst.objects = src.objects
            for o in dst.objects:
                if o is not None and o.type in ('MESH', 'ARMATURE'): bpy.context.scene.collection.objects.link(o)
        else: raise SkinError('unknown model type %s (fbx, obj, dae, glb, gltf, blend)' % ext)
        new = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
        if not new: raise SkinError('no mesh in %s' % path)
        for o in bpy.context.selected_objects: o.select_set(False)
        for o in new: o.select_set(True)
        bpy.context.view_layer.objects.active = new[0]
        return new[0]

    CLASSES = [PeAyaSettings, *OPS, PEAYA_OT_play, PEAYA_PT_panel, PEAYA_OT_import_dae]

    def main(argv):
        import argparse
        ap = argparse.ArgumentParser(prog='pe_aya_skin', description='Make an Aya skin from a model already in the game stance')
        ap.add_argument('--disc', required=True, help='Parasite Eve (USA) disc 1 .bin or .iso')
        ap.add_argument('--model', required=True, help='.fbx .obj .dae .glb .gltf or .blend (the object named by --object)')
        ap.add_argument('--object', help='mesh to use from a .blend')
        ap.add_argument('--out', required=True, help='skin folder: .../Parasite Eve/pe_skins/<name>')
        ap.add_argument('--name', default='Aya: Custom', help='name in the Wardrobe')
        ap.add_argument('--bank', type=int, default=12, choices=sorted(BANKS))
        ap.add_argument('--size', default='4', choices=['4', '8', 'HOST'])
        ap.add_argument('--texture', default='', help='one picture for the whole model (else the materials\' pictures)')
        ap.add_argument('--hd', action='store_true')
        ap.add_argument('--no-fit', action='store_true', help='keep the model\'s own size and place')
        ap.add_argument('--reassign', action='store_true', help='assign bones again even if the model has PE bone groups')
        ap.add_argument('--save-blend', help='also save the working .blend here (to fix bones by hand later)')
        a = ap.parse_args(argv)
        try:
            register()
        except ValueError:
            pass
        sc = bpy.context.scene.pe_aya
        sc.disc, sc.bank, sc.out_dir, sc.skin_name, sc.budget, sc.hd = a.disc, str(a.bank), a.out, a.name, a.size, a.hd
        sc.texture = os.path.abspath(a.texture) if a.texture else ''
        print(step_reference(bpy.context))
        obj = import_model(a.model)
        if a.object:
            obj = bpy.data.objects.get(a.object) or obj
            for o in bpy.context.selected_objects: o.select_set(False)
            obj.select_set(True); bpy.context.view_layer.objects.active = obj
        sc.model = obj
        if a.no_fit:
            print('prepared:', step_prepare(bpy.context).name)
        else:
            print(step_fit(bpy.context))
        obj = sc.model
        has = any(part_of_name(g.name) is not None for g in obj.vertex_groups)
        if a.reassign or not has:
            print(step_assign(bpy.context))
        print(step_export(bpy.context))
        if a.save_blend:
            bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(a.save_blend))
            print('saved', a.save_blend, '(it holds the game\'s reference mesh: do not share it)')

    if __name__ == '__main__':
        argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
        if argv:
            try:
                main(argv)
            except SkinError as e:
                print('[PE Aya Skin] error:', e)
                sys.exit(1)
        else:
            register()                 # run from Blender's Text Editor: the panel appears
