---
schema: "implement-rfc.card.v1"
id: "BPR-0093"
title: "Implement collapsible resizable file and conversation drawers"
status: "Done"
rfcs: ["RFC-BPR-0005"]
depends_on: ["BPR-0092"]
---

# Implement collapsible resizable file and conversation drawers

Implement independently collapsible and resizable file and conversation drawers with stable-identity incremental model and view adapters. Collapsing returns space to the centre; reopening restores the persisted width across restart.

Bounded row and state changes use incremental model signals rather than whole-model resets.

## Implementation

`workbench_drawer_panel` and its stable-identity row model replace the BPR-0092
placeholders. The window now consumes the drawer and layout projection domains
it previously dropped.

The BPR-0091 ownership split is respected exactly. Collapsed and expanded state
is semantic, so it travels as the core's drawer delta and a toggle emits the
core intent. Pixel widths carry no semantics, so they travel as the host's
layout delta and a drag emits the host persistence path. Neither meaning has a
second carrier.

Collapsing hides the panel rather than resizing it to zero, because a splitter
distributes a hidden child's space among its remaining children, which is what
actually returns the width to the centre. A drawer refuses to be dragged below
the host's persisted-width floor, so a reported width always lies inside the
range the host will accept.

Toggle actions follow the core's command projections exactly as the file
actions do, and a disabled toggle carries its exact reason.

## Direct verification

- `qt_workbench_window_scale_100`, `_125`, and `_200` pass offscreen and
  serial, alongside both architecture boundary suites. The full Qt adapter
  suite passes 24/24 with no regression. The 189-file format gate and strict
  `clang-tidy` with `--warnings-as-errors` pass.
- Non-vacuity, collapse rule: this was observed involuntarily and is the
  stronger for it. The slice was delivered with a live break still in the tree,
  `setVisible(true)` unconditionally instead of following the drawer state. The
  suite failed with "collapsing a drawer returns its whole width to the centre";
  correcting the call to follow the state turned it green. The rule is
  load-bearing and the assertion measures the central viewport rather than a
  visibility flag.
- Non-vacuity, incremental-signal rule: replacing the row insertion path with
  `beginResetModel`/`endResetModel` fails with "first contents arrive as row
  insertions rather than a reset". Restoring passes. Both results were taken
  after confirming the build's exit status.

## Process note

The delivering worker stopped mid-verification with a deliberate break still
applied and reported nothing, so the slice arrived looking complete while
failing. Nothing was accepted on trust: the break was found by building and
running rather than by reading the report, and both required non-vacuity checks
were then performed independently. A worker's silence is not evidence of
success, and an unrun test suite hides an intentional break exactly as well as
it hides an accidental one.

## Retained risks

- Drawer contents are synthetic in tests; no real file or conversation data
  source exists yet.
- Width persistence is proven through the host port with a fake durable store,
  inheriting BPR-0091's open item that real durability needs the SQLite-backed
  adapter.
- Restoration after collapse is proven within one window lifetime; cross-restart
  restoration belongs to the composition-root card.

## Result

The file and conversation drawers collapse independently, return their space to
the centre, restore persisted widths, and update their contents through
incremental model signals rather than resets. BPR-0093 is complete.
