---
id: BPR-0090
title: Implement core workbench navigation and command projections
status: Done
rfc: RFC-BPR-0005
depends_on: [BPR-0089]
---

# Implement core workbench navigation and command projections

Add the deterministic core transitions and command projections for file, change, epoch, drawer-semantic, and review actions: previous/next file, previous/next change, selection identity, drawer semantic state, and their enable/disable reasons.

The core stays Qt-free, effect-free, and compact. Selection follows the filtered actionable order. Command enablement is projected by the core, never inferred by a widget.

## Implementation

`src/core/include/bpr/core/workbench_interaction.hpp` and its implementation
add the file-level and drawer-level core surface. `diff_interaction` keeps
owning line, group, and within-file change selection unchanged.

Two design decisions are recorded in the source rather than left implicit.
`workbench_file_id` is an alias of the diff pipeline's `file_identity` rather
than a new identity space, because a separate space would force the host to
retain a second mapping between two notions of the same file and would stop the
selected file being usable directly as a diff request key. A narrow
`workbench_reason_code` carries workbench navigation legality because no member
of `diff_reason_code` can express it, and widening the diff vocabulary to cover
workbench concerns would blur two unrelated domains.

The core stays compact as required. It retains the selected file identity, a
bounded navigation cursor holding the 1-based ordinal, the actionable count,
and the neighbour facts, and the two drawer states. It never retains the file
list, any path string, or any file content. A selection that has left the
filtered actionable order has no ordinal to step from, so the core reports that
only the host can name its neighbour rather than inventing one.

## Direct verification

- `core_workbench_interaction` passes with 59 assertions covering cursor
  well-formedness, rejection of an ordinal past the actionable count and of an
  ordinal without a selection, zero-identity rejection, neighbour-fact
  consistency, forward and backward stepping, stop versus wrap policy at both
  ends, single-entry and empty-order behaviour, and the nonresident-selection
  case.
- `architecture_boundaries` and `architecture_boundary_contract` pass, so the
  new module keeps the core free of Qt, provider, and effect dependencies. It
  builds and runs in the Qt-free `core-only` preset.
- Both required rules were proven non-vacuous by breaking them. Forcing the
  command projection to report `Enabled = true` unconditionally fails with
  "an enabled command is exactly an accepted gesture". Stepping the next
  ordinal by two instead of one fails with "next steps to the following ordinal
  of the actionable order". Both restore green.
- The 185-file format gate passes, and strict `clang-tidy` with
  `--warnings-as-errors` passes on both new translation units.

## Result

The workbench's file navigation, drawer semantics, and command enablement are
decided in a compact, deterministic, Qt-free core that holds no file list.
BPR-0090 is complete.
