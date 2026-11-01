# Remaining animated backgrounds

Working expansion of the frozen HD background mod 0.1, on `poc/hd-backgrounds`. This is an art review build, not another released mod version.

The remaining catalog covers **192 maps, 228 views, 469 independent controllers and 2095 serialized phase slots**. Six slots intentionally have no drawable packet; they remain empty. The export contains 3215 entry records with 3049 unique geometry keys. Both-disc geometry and texture bytes agree. UI/title/tutorial screens are outside this background catalog.

## Material and motion

OpenAI built-in imagegen supplies fixed scene materials. Its exact model version is not exposed. There are 215 distinct view anchors: 197 newly generated, nine reused from the initial animation study or reviewed background pack, and nine restored directly from the source. The latter preserve five text-heavy boards/screens and four anchors retained after generation/reference failures; the original failure records remain in `metadata/`.

Independent generated phase candidates missed motion in the first study. The expansion therefore follows the accepted M037 approach: deterministic **RealESRNet_x4plus** restoration of original changing regions, combined with fixed OpenAI scenery. Native temporal changes define common motion support for every phase. Hidden states retain their own source material; sparse foreground fences and poles cannot inherit lights from the scene behind them. Native glyph shapes are restored using source text-region coordinates, without rendering OCR guesses.

`register_anchors.py` corrects global output framing/aspect differences with one recorded affine transform per anchor. Uncovered edges use source restoration. `assemble_sources.py` locks low-frequency lighting to the original once per view, retaining generated fine texture. This prevents a material reference from brightening a later night/damaged version of the same room. Moving regions retain the original phase lighting. No per-phase exposure adjustment or optical flow is used.

The first M038–M043 study remains byte-identical: seven views, thirteen controllers and 105 packets. Eight controllers use source-guided motion; five retain the previously checked sequential imagegen phases. Raw generated candidates and their prompts remain available for comparison.

## Original game behavior

Native coverage, STP blending, tile depth, controller independence, phase order and durations remain authoritative. Original scripts decide when motion starts, stops, reverses or holds. No interpolation, extra frames, replacement video or runtime animation layer is introduced. The exporter retains controller flags, position, step and elapsed values for inspection. Gameplay and runtime C modules are unchanged by this asset batch.

`pack-with-0.1/` combines 4069 unique packets. The frozen 0.1 snapshot is immutable. Later reviewed corrections are applied separately from `corrections/pack-overrides/`; the preservation audit lists any intentionally overridden baseline keys. Nine keys recur in later rooms; reviewed baseline material wins for those packets. The native reference half of every added atlas is checked against original RGB555 source data.

## Rebuild and review

All artwork, disc-derived metadata, packs, model weights and generated previews are private and ignored by Git. Only these offline helpers and this explanation are tracked. Prompts and provenance are in `metadata/`; source inference records are in `source-sr-full/`.

1. Export the remaining catalog with `../hd-poc/export.py`, then run `prepare.py`, `plan_anchors.py` and `anchor_materials.py`.
2. Produce missing material anchors with the built-in imagegen tool using `generate_anchors.js`. Restore all original active contexts with `../hd-poc/infer.py` and the recorded RealESRNet weights. Do not generate animation phases independently for the expansion.
3. Run the read-only native text-region extractor `text_regions.swift`, then `register_anchors.py` and `audit_registration.py --anchors --registered`. Low-confidence registration requires review.
4. Run `assemble_sources.py --require-complete`. Optional `--shards N --shard I` partitions views; merge the resulting `views` and `pending` arrays into `source-guided-assembly.json`.
5. Run `../hd-poc/pack.py OpenAI-source-guided --root . --destination pack --no-family-lock --preserve-source-black --entry-overrides corrections/entry-overrides` from this directory.
6. Run `retain_reviewed.py --baseline /absolute/frozen/0.1/pack`, then `combine.py` with the same baseline.
7. Run `audit_pack.py --pack pack-with-0.1`, `composite.py --pack pack-with-0.1`, `audit_temporal.py` and `review.py`. Add `composite.py --pack pack-with-0.1 --room M038 --synchronized`, and likewise M039, before the final review build for combined car-controller previews.

The gallery provides a map selector, paired full-scene views, phase selection, playback and diagnostic script-layer views. Preview speed/repeat are inspection controls; compositions exclude actors, camera movement and running scripts. A door is not required to loop or to have identical first/last poses.

Data/reference audits, packed-scene temporal checks and preservation checks are separate from runtime testing. `original-all-room-loads` checks native room loading from both discs. Headless GPU integration checks the assembled pack on its existing replay route, comparing original state, VRAM and PCM; it does **not** visit every new map. The expansion still needs visual review in those later locations.

Reviewed text corrections are retained as per-entry overrides. M195’s lower red exhibition-banner dates read **Nov 1st, Jan 15th** (two lines). The imagegen source, prompt, localized patch and verification are in private `corrections/M195/`; only the date rectangle changes, consistently across all original phases. Include `--entry-overrides corrections/entry-overrides` when repacking.

Chinatown M161–M163: the straight-row road reinterpretation is rejected. Restore the original irregular herringbone road from each native layer using `source-material-regions.json` and full-context `source-materials/` inputs. M164 is an interior; M165 already retains the original pattern. The reviewed overlay changes only road support and tile gutters; native reference halves and all other atlas pixels remain identical. Private provenance and before/after previews are in `corrections/chinatown-source-road/`. Do not restore `M162/road-v2.png` as a generation reference.

M192 group 1: eight door poses use one existing HD carved-wood material projected onto the native door plane. Imagegen supplies only a localized handle-free wood patch; the original protruding handles keep their own per-pose restoration. Original silhouettes, independent layer coverage and controller timings remain unchanged. Private projection matrices, provenance, overrides and phase audits are in `corrections/M192/`; preserve its per-entry overrides when repacking.

For distribution, run `../hd-poc/compress_pack.py pack-with-0.1 pack-0.1-png
--version 0.1` after applying all reviewed corrections. This creates a separate
lossless PNG pack, checks every decoded pixel and updates file hashes without
modifying the BMP authoring pack. `audit_pack.py` and `composite.py` accept either
format. Repacking and correction tools continue to use the BMP authoring sources.
