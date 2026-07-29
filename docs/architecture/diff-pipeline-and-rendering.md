# Diff Pipeline and Rendering Contract

## Pipeline

```text
retained Git objects
  -> tree/file change discovery
  -> old/new blob loading
  -> normalized line sequences
  -> hunk and line mapping
  -> intraline spans
  -> change-group construction
  -> syntax spans per complete blob
  -> immutable review presentation model
  -> virtualized Qt rendering
```

## Architectural allocation

- The deterministic, Qt-free diff planner owns line identity, hunk construction, intraline spans, change grouping, selected-group mapping, and navigation semantics. It may run on a host worker with explicit inputs and retained scratch storage, but it performs no I/O and touches no Qt state.
- The application host owns Git blob residency, Git-process execution, job scheduling, cancellation, generation identity, and diff/syntax/layout caches.
- The functional core owns the currently selected file/change and accepts or rejects computed results by request/revision/generation.
- The Qt presentation adapter paints the immutable presentation model, translates selection/navigation events into intents, and applies semantic deltas without inferring new correspondence.

## File cases

The diff model explicitly represents modified, added, deleted, renamed, copied, type-changed, binary, submodule, generated, too-large, and unavailable files. Unsupported content remains visible with an explanatory state rather than disappearing.

## Line identity

Every displayed line carries origin side, blob OID, source line number, display row, hunk identity, change-group identity, and anchor eligibility. Navigation and comments use these identities rather than raw visual row numbers.

## Change groups and center connector bar

Adjacent or semantically contiguous changed rows form a `ChangeGroup`. The top side-by-side panes reserve a center band that connects corresponding group extents with clean tapered shapes. The layout must minimize crossings by preserving source order and must never imply correspondence absent from the diff model.

The first implementation does not require semantic moved-code detection. It must accurately group ordinary insert/delete/replace regions and may later add moved-code classification behind a separate RFC.

## Lower detail pane

Selecting a line or change group opens a full-width lower pane spanning the central workbench. It shows the selected old/new lines in line-over-line form with synchronized intraline emphasis, context controls, and previous/next selected-group navigation. It is not merely another copy of the upper split view.

## Rendering

- Custom `QAbstractScrollArea`-based views paint only visible rows.
- `QTextLayout` performs Unicode-aware text layout.
- Code comments never expand inline between source rows; they remain in the synchronized conversation drawer.
- The renderer preserves source position across font/theme changes where possible.
- Caches separate diff structure, syntax classification, text layout, and painted theme state.
