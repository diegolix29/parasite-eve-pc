# Mod Catalog in Aya's menu

The Catalog uses the native menu widgets and equipment-screen arrangement. The equipped
weapon/armor and BASE/PLUS values stay on the left, with its effect slots below. Catalog effects,
aligned Junk prices and a details panel occupy the right. Stat labels and BASE/PLUS headers reuse
the retail Tune Up sprite artwork, with fitted-text fallback for oversized replacement assets. No overlay renderer or economy/save
format changes are involved.

## Controls

- L1/R1: switch equipped weapon/armor, retaining each side's list positions.
- Up/Down: move and scroll within the focused list, including stock end wrapping.
- Left/Right: focus installed slots / Catalog effects without moving the other list.
- Cross on a slot: focus the Catalog. Cross on an effect: review the change.
- Square: review removing the selected slot's effect.
- Cross in review: open the game's Yes/No dialog, initially on No.
- Circle: cancel the dialog/review, return from Catalog to slots, or close from slots.

The review lists the cost, Tool requirement, resulting Junk balance and every effect the existing
Catalog rules report as replaced. Long reviews scroll in the right pane. Selected review rows
also appear in the wider title/help bar. The normal Catalog viewport is restored afterwards.
Long labels are fitted using the native font's measured widths; short row labels do not overwrite
the price column. Full selected names appear in the help bar up to its own finite screen width.

## Rules and layout

`pe_catalog.c` continues to own eligibility, prices/discounts, Tool/Tool Kit behavior, exclusivity
groups, protected Bag effects, spending and mutations. Review uses dry runs and `pe_cat_replaces`.
Accepting a review rechecks the equipped record, balance and price before calling the existing
install/remove function. Cancelling never calls a mutation. Stale confirmations require a fresh
review, and duplicate callbacks cannot spend twice.

The item format and save validator currently support ten slots. The viewport model uses the
validated count supplied by the Catalog; it does not hard-code ten presentation rows. The
installed-slot viewport shows five rows and the Catalog/review viewport eight. Their cursors,
scroll offsets and animation corrections are independent. The Effects counter reports the
visible range. Native scroll arrows indicate additional entries. Every panel remains inside
the game's 320x224 menu area and retains its outer size for short/empty lists.

## Modules

- `mods/pe_economy/pe_catalog_menu.c`: screen lifecycle, focus, inputs, gear memory and transaction review.
- `mods/pe_economy/pe_catalog_menu_view.c/.h`: native panel geometry, width-aware text and row drawing.
- `mods/pe_economy/pe_catalog_menu_state.c/.h`: count-driven viewport clamping without rendering dependencies.
- `mods/pe_economy/pe_catalog.c`: existing rules, unchanged.

The shared menu host, Dismantle, Rebuild and F1 Catalog pages are unchanged.

## Verification

Run `tests/run_pe_catalog_menu_test.ps1` after configuring `build-mods`. It compiles the production
controller, renderer, viewport model and shared host helpers against simulated native widget and
economy boundaries. Coverage includes separate scrolling, wraps/rapid input, per-gear memory,
0..40 model rows, native panel bounds, long labels, ten-effect reviews, cancellation, blocked
operations, stale/duplicate confirmations, record-count changes and widget cleanup.

These tests do not replace an in-game visual/controller pass using real font assets and a save.
