STATIC BACKGROUND EXPANSION — PRIVATE ASSET STUDY

Scope: 186 M/E world maps, 234 views, 253 unique material contexts.
Both discs' complete source texture bytes and geometry were compared.
Candidates exclude serialized animation controllers, hidden/switchable entries,
initial scrolling/parallax layers, and title/tutorial/UI/Game Over cards.
Camera panning is allowed. This classification is not a complete runtime proof.

Generated with the built-in OpenAI imagegen tool; the exact model version is
not exposed. Each source has an individual prompt/provenance record in metadata/.
Original generated outputs are in generated/; OpenAI/ contains normalized 4x
full canvases. Original geometry, masking, tile depth, STP, native reference RGB,
palette fallback and game logic remain authoritative.

pack/ contains the static expansion. pack-with-day1/ combines it with the
reviewed Day1 pack, giving the Day1 files priority on all overlapping keys. Explicit user-requested
local corrections in corrections/regions.json are then applied by apply_corrections.py.
day1-preservation.json verifies unchanged Day1 atlases and lists intentional
user-requested M005 floor corrections separately.
No animated Day1 work, including the accepted M037 car interior, is regenerated.
Localized corrections preserve every native-reference and unselected atlas texel.

Use the HD WIP build with:
PE_HD_BACKGROUNDS="/absolute/path/to/pack-with-day1" ./build/parasite-eve-pc --disc "/path/to/disc.cue"
The path affects only that process. Do not change the user's normal install.

Review gallery: gallery/index.html. It reconstructs complete scenes from the
actual packed tiles using native coverage, depth and blending. These are offline
previews without live actors, mirrors, scripts or camera movement. Generative
art still requires visual review; registration checks are evidence, not proof
that every fine detail or glyph is faithful.

Rebuild with the existing hd-poc exporter and this directory's helpers:
  inventory.py; export.py with the selected room list; prepare.py
  next.py and generate_batch.js (built-in tool orchestration)
  normalize.py --require-complete
  ../hd-poc/pack.py OpenAI --root . --destination pack --no-family-lock --preserve-source-black
  combine.py
  audit_pack.py --pack pack-with-day1
  composite.py --pack pack-with-day1
  gallery.py
  audit_registration.py; audit_registration.py --normalized
Run these from the project with absolute --root/--destination paths as needed.

Do not upload these disc-derived images or packs to a public Git repository.
Backups: /Users/hasik/Backups/parasite-eve-pc/2026-10-27-hd-static/
and darwine:/mnt/storage/hasik/backups/parasite-eve-pc/2026-10-27-hd-static/.
