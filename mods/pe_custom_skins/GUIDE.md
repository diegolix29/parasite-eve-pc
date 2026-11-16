# Making an Aya skin

This guide turns your own 3D model (FBX, OBJ, Collada .dae, glTF or a .blend) into a costume Aya can wear in the
Parasite Eve PC port. Two pieces do the work:

- `pe_aya_skin.py`, a Blender add-on in this folder. It reads Aya's skeleton from **your own disc 1**,
  fits your model to it, assigns every vertex to one of the game's bones and writes a skin folder.
- The `pe_custom_skins` mod in the game. It lists every skin folder that has a `skin.ini` in Aya's
  Wardrobe, so you don't have to write any code.

Nothing from the game ships with the tool or with your skin's source files. The skin folder the
tool writes holds data copied from your disc, though: share your model (and this tool), not that
folder, and not a .blend that still contains `PE_Aya_Reference`.

## What you need

- Blender 4.2 or newer (tested on 5.3).
- `Parasite Eve (USA) (Disc 1).bin` (the raw .bin or an .iso; not the .cue).
- A game build with the mods on and `pe_custom_skins` in the mod list (it is in the default list).
  In the game: F1 > Manage mods > All mods on, then restart once.
- Your model, standing, with a UV map and one texture (or several materials, each with a picture).

## How the game moves a model (the limits to work with)

Aya is a PlayStation model, and the PC port draws her the same way:

| | |
|---|---|
| Skeleton | 29 parts. A skin moves with Aya's own animations; the bones can't change. |
| Skinning | **Rigid**: each vertex follows exactly one bone. No weights, no blending. A face whose corners sit on two bones stretches at the joint (that is how the original elbows and knees work). |
| Bind pose | The game's idle stance (arms hanging, slightly bent). Your model is stored as it stands in that stance. |
| Size | With **Full detail** (game builds where skins draw from host memory): up to **65,535 faces**, no reduction. On older builds, Safe: about **1,100 quads or 1,300 triangles** (the PlayStation-sized pool the game gives Aya); the tool reduces bigger models, but a model you lower yourself looks better. |
| Texture | One **256×256** page, 4-bit: up to 32 palettes of 16 colours, one palette per UV island group. Fully see-through texels (alpha below 50%) are not drawn. |

Bones the game draws (vertex group names):

| Bone | What it carries |
|---|---|
| `01_chest` | chest, upper back, shoulders' top |
| `03_L_clav`, `13_R_clav` | collarbones (tiny; the automatic pass leaves them out) |
| `04_L_upperarm`, `14_R_upperarm` | upper arms |
| `05_L_forearm`, `15_R_forearm` | forearms |
| `06_L_hand`, `16_R_hand` | hands and fingers (the right hand holds the gun) |
| `10_neck`, `11_head` | neck; head, face and hair |
| `20_pelvis` | hips, belt, skirt |
| `22_L_thigh`, `23_L_shin`, `24_L_foot` | left leg |
| `26_R_thigh`, `27_R_shin`, `28_R_foot` | right leg |

L and R are Aya's own left and right. The other bones (finger joints, arm and hip bases) are
never drawn; a vertex put on one of them is moved to its parent at export.

## Install the add-on

Blender: Edit > Preferences > Add-ons > (the arrow at the top right) > Install from Disk, pick
`pe_aya_skin.py`, tick it. Open a 3D view's sidebar (N): a **PE Aya Skin** tab appears.
(Or open the file in the Text Editor and press Run Script.)

Start from an empty scene (File > New > General, delete the cube).

## Step 1: Game reference

Set **Disc 1** to your disc image and **Skeleton** to the outfit to build on:

- `12: Jacket` (default): Aya's main gameplay skeleton.
- `13: T-shirt`, `10: Evening dress`: the other outfits' skeletons (the dress is only fully animated
  in the opening).

Press **Load reference**. You get two objects in the collection *PE Aya Reference*:

- `PE_Aya_Rig`: the game's skeleton in the idle stance.
- `PE_Aya_Reference`: Aya's own low-poly model, one colour per bone. It can't be selected; it shows
  where your model must be and how the game split her between bones.

## Step 2: Your model

Import your model (File > Import). For a Collada **.dae** file use **Import Collada (.dae)** in the
panel (or File > Import > Collada (.dae) - PE Aya Skin): Blender 5 has no Collada importer of its own.
The add-on reads the mesh in its bind pose (skinned meshes posed by their own skeleton, usually a
T-pose), its UVs and its textures; the file's skeleton itself is not needed. Pick it in **Model** (select all its pieces if it has several)
and press **Fit size to reference**. This:

- joins the selected meshes into one object,
- applies an imported skeleton's current pose and drops that skeleton,
- applies location, rotation and scale,
- scales the model to Aya's height (head top to sole) and stands it on the reference's floor.

Fit size centres the model on its lower legs, so a braid, ponytail or cape doesn't push the skeleton
behind the body. Then check the direction: your model must **face the same way as the reference**. If not, rotate
it (R Z 180), and apply the rotation (Ctrl+A > Rotation).

## Clean up hidden faces (optional, after step 2)

Ripped and game models often keep layers no one sees: the body under the clothes, the scalp under the
hair, the inside of the mouth or of the boots. The game has no depth buffer (faces are sorted in steps
of about 1.5 cm), so these faces draw through the layer over them and flicker as the camera moves.

1. **Select hidden**: the faces where every ray from the face hits the model again within
   **Search distance** (0.25 m) are selected in Edit mode. Look them over; deselect any you want
   to keep (Shift+click).
2. Delete them (X > Faces), or press **Remove hidden** to delete them all at once (Ctrl+Z undoes it).

Faces facing a gap (eye holes, sleeves, the neck of a top) let a ray out and are kept. Run it after
**Fit size** (the distance is in metres at Aya's height). The report also counts the faces lying
on another surface (straps, a belt, hair over the ears): those stay visible, and the export sorts
them a little nearer so they don't flicker under the surface below them (needs a game build with
face lifts, `RENDER_FACE_LIFT` in object.c).

## Step 3: Fit the joints (and pose T-pose / A-pose models)

Do this for every model whose body isn't built like Aya's, and always for T-pose and A-pose
models. Aya has short legs and high hips: a model with longer legs that skips this step bends
10-20 cm above its knees. The game takes a skin's own bone lengths (animations only carry
rotations), so the joints can go where your model's are.

1. **Make fitting rig**: a copy of the skeleton, `PE_Aya_FitRig`, opens in Pose mode.
   - If your model came with its own skeleton (a .dae, or an FBX / glTF with an armature) using
     common bone names (LeftArm, LeftForeArm, LeftHand, LeftUpLeg, LeftLeg, LeftFoot, Neck, Head;
     Mixamo, Unity, Unreal, VRM and Biped names work too), every joint is placed on it: shoulders,
     elbows, wrists, neck, head, hips, knees and ankles, each bone aimed at the next joint. The
     status line lists them. Check them in X-ray (Alt+Z) from the front and the side.
   - Otherwise only the arms are aimed at your model's arms when the tool can find them.
2. Put every joint where your model bends. In Pose mode, select a bone and press G to move its
   joint (the round end where it starts), R to turn it. Check from the front (numpad 1) and the side
   (numpad 3):
   - thigh bones (`22`, `26`) at the hip joints, shin bones (`23`, `27`) at the knees, foot bones
     (`24`, `28`) at the ankles;
   - upper arms (`04`, `14`) in the shoulders, forearms (`05`, `15`) at the elbows, hands (`06`, `16`)
     at the wrists;
   - for a T-pose or A-pose, turn the arm bones down into the arms (the auto aim usually did it).
   Moving a joint carries the bones below it: fix them top to bottom (hip, then knee, then ankle).
   Don't scale bones (S).
3. **Pose into game stance**: the model is bent from the fitting rig's pose into the game's stance,
   joints blended over a couple of centimetres, each vertex gets its bone, and the skeleton
   `PE_Aya_Rig` is rebuilt with your model's bone lengths (the status line lists them). The fitting
   rig is removed. The export writes those lengths into the skin.
   The chest and pelvis keep their shape: only the limbs bend. The arm's and thigh's own skin next
   to the torso blends over about 6 cm, so a shoulder stretches a little instead of the chest
   caving in.

## Step 4: Bones

**Assign bones** (not needed right after step 3, which already did it) gives every vertex the
nearest game bone, with these rules:

- an arm bone only takes vertices close to it (sleeves, cuffs and a closed hand fit; a hip touched
  by the hand stays hip),
- a surface facing its bone (the side of the body next to a hanging arm) is not that bone's,
- arms and legs never take vertices across the body's middle,
- a leg bone only takes vertices close to it: skirts, dresses and coat tails farther out go to
  `20_pelvis` (rigid: the legs pass through a long skirt when she runs),
- a model cut at every texture seam (common in rips) is judged as one surface: the copies of a
  vertex always get the same bone,
- loose pieces (buttons, earrings, eyes) move with one bone; a loose piece reaching above the head
  joint (hair, ribbons, earrings) goes to `11_head`; a small piece sitting on a bigger one (a hair
  tie on a braid, a buckle) follows the bone of what it sits on,
- each bone keeps one connected region; stray bits join what surrounds them,
- no edge may join two bones without a joint between them (left and right thigh across a skirt,
  neck to pelvis down a ponytail, head to arm through hair): those edges stretch across the screen
  when the bones move apart ("exploding" faces), so their vertices move to a bone that joins both
  (the pelvis between the thighs, the chest between neck and pelvis). The export runs this check
  again on your hand edits.

Check the result: select your model, Weight Paint mode, and click through the vertex groups in
Object Data > Vertex Groups. To fix a vertex: Edit mode, select it, pick the right group and press
**Assign**, then remove it from the wrong one (**Remove**). The strongest group wins at export, so
a vertex in several groups goes to the one with the highest weight.

Things worth checking by hand:

- **Shoulders**: where `01_chest` stops and the upper arm starts. Put the seam where your sleeve
  or the shoulder's crease is.
- **Hands touching the body**: a fist resting on the hip or a hand on the thigh.
- **Hair**: long hair falling on the shoulders or back should stay on `11_head`, or the head will
  turn without it.
- **Skirts and coats**: anything below the hips on `20_pelvis` stays rigid while the legs move, so
  a long skirt will be crossed by the legs when she runs. Short skirts and fitted clothes work best.

## Step 5: Preview

Your model carries an Armature modifier on `PE_Aya_Rig`. **Play row** loads one of the game's
animation rows onto the rig; press Space to play it, ◀ ▶ step through the row numbers. The report
lists every row id the bundle has. Look for faces that stretch badly or parts that move with the
wrong limb, fix their bones, and play again. **Stop preview** puts the rig back in the stance.

## Step 6: Export

- **Name**: shown in the Wardrobe (e.g. `Aya: Leather jacket`).
- **Folder**: `%APPDATA%\Parasite Eve\pe_skins\<one_word_name>\` on Windows (beside
  `parasite-eve.toml`; on other systems, the game's preferences folder). The folder name is also the
  skin's id: letters, digits, `_` and `-`, up to 40 characters, and **never rename it** once people
  use the skin (their Wardrobe remembers it by name).
- **Texture** (optional): one picture for the whole model, used when the materials don't link their
  picture (common with FBX exports from AI tools and some stores).
- **Size**: *Full detail* keeps your model as it is (up to 65,535 faces): the skin draws from PC
  memory, which game builds with `PE_HOOK_ACTOR_ALLOC` give a worn skin. On a build without it the
  skin falls back to the game's own pool and may fail to load: use *Safe* (about 1,100 quads, fits
  any build) or *Large* (twice that, may fail in rooms with many actors) for those.
  A bigger model is reduced automatically; small separate pieces (eyes, brows, buttons) are kept
  whole.

Small separate pieces lying on a surface (eyes, brows, mouth, decals) are drawn first and on top:
the game has no depth buffer, it sorts faces in steps of about 1.5 cm, and inside a step the first
face drawn wins. Keep such pieces as their own separate mesh pieces (not joined to the face's
vertices) so the tool finds them.
- **HD texture**: also writes `texture_hd.png`, only used by game builds with HD skin support.

Press **Export skin**. The report gives the size. The folder gets:

| File | |
|---|---|
| `model.bin` | Aya's player bundle from your disc with your model added as her model |
| `texture.bin` | Aya's texture file with your page and palettes |
| `day1.bin` … `day8.bin` | Aya's animation rows for each story day, from your disc |
| `meta.bin` | which part holds the gun (the right hand) |
| `skin.ini` | name, key and group for the Wardrobe |
| `page_preview.png` | your 256×256 page as the game will draw it (4-bit) |

Start the game (restart it if it was running): Aya's menu > Wardrobe, filter group *Custom skins*.
Custom skins are free and owned from the start.

## Without the window

The same steps run from a command line for a model that already stands in the game stance
(steps 1, 2, 4 and 6; bones the model already has in PE groups are kept unless `--reassign`):

```
blender -b -P pe_aya_skin.py -- --disc "Parasite Eve (USA) (Disc 1).bin" --model my_aya.fbx ^
        --out "%APPDATA%\Parasite Eve\pe_skins\my_aya" --name "Aya: My look" [--texture tex.png] ^
        [--bank 12] [--size 4|8|HOST] [--no-fit] [--reassign] [--save-blend work.blend]
```

`--save-blend` keeps the working file so you can fix bones in Blender and export again.

## skin.ini

```
name = Aya: My look
key = custom/my_aya
group = Custom skins
```

`key` is what players' profiles remember: keep it once the skin is shared. `group` is the Wardrobe
filter. Lines starting with `#` are comments. The `bank` line the tool writes is for your
information only.

## Tips for a good result

- **Make the low-poly yourself.** The automatic reduction keeps the silhouette but spends faces
  evenly; a hand-made 1,000-quad model with faces where they matter (face, hands) looks far
  better. Keep joints (elbows, knees, shoulders) with a ring of vertices close to the joint so they
  bend like Aya's.
- **Layout your UVs for 256×256**: give the face the biggest island, avoid tiny islands (each
  island's colours must share a 16-colour palette with similar islands). Islands of one material
  (skin, hair, cloth) packed together share palettes well.
- **One closed surface per piece.** Inner layers (a body under clothes) are wasted faces and draw
  through: the PlayStation has no depth buffer, faces are sorted in steps of about 1.5 cm.
- **Keep it close to Aya's shape.** The game's hit boxes, the gun's place and the camera follow
  Aya's skeleton, not your mesh.
- A model that stands slightly differently from the reference is fine; a different height is not
  (step 2 scales to her height).

## Troubleshooting

| Message | What to do |
|---|---|
| *Disc image not found* / *Not a CD image* | Point **Disc 1** at the .bin (or .iso) of disc 1, not the .cue. |
| *unexpected player model* | Use the USA release of disc 1. |
| *The model has no UV map* | Unwrap it (Edit mode > U) and give it a picture. |
| *material "X" has no picture (flat colour)* | Link the material's Base Color to an Image Texture, or set **Texture**. |
| *vertices on bones the game does not draw* | Move them to the bones in the table above. |
| *a vertex is too far from its bone* | The model is far off the reference: run **Fit size to reference**. |
| *could not reduce the model to the budget* | Lower it yourself (Decimate modifier, or a retopology), then export again. |
| The skin isn't in the Wardrobe | `skin.ini` and `model.bin` must be in `pe_skins/<folder>/`; mods must be on; restart the game. `pe_mods.log` beside the preferences lists the custom skins found. |
| Patches of wrong colour in game | Skins exported before this fix: export again. (Each texel keeps a 4-bit colour number that only means something in its own palette, so UV islands that share texels now always share a palette.) |
| Faces flicker or vanish in game | Run **Clean up > Remove hidden** and export again. Otherwise very thin or tiny faces: lower the model's detail. |
