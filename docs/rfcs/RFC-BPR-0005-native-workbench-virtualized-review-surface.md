---
id: RFC-BPR-0005
title: Native Workbench Shell and Virtualized Review Surface
status: Proposed
area: BPR
milestone: M1 Native Read-Only Workbench
depends_on: [RFC-BPR-0004]
pack_version: 2
---

# RFC-BPR-0005: Native Workbench Shell and Virtualized Review Surface

## Summary

Implement the native Qt workbench with top-level review navigation, collapsible file and conversation drawers, independently selectable epochs, virtualized top diff panes, a clean grouping connector bar, and a full-width lower detail pane.


## Motivation

The native workbench must directly address the presentation failures that motivated the product: primary navigation buried in GitHub, comments consuming code width, weak patchset comparison, poor change grouping, and no focused lower line-over-line inspection pane.

## Goals

- Implement the complete central review layout in Qt Widgets.
- Put previous/next file and previous/next change in the top primary toolbar.
- Provide independently selectable left/right review epochs above the panes.
- Render side-by-side top panes with a clean P4Merge-inspired center grouping band.
- Render a full-width Beyond Compare-inspired line-over-line lower detail pane.
- Implement collapsible/resizable file and conversation drawers.
- Virtualize code rows for large files and high-DPI operation.
- Establish keyboard, focus, accessibility, loading, and error behavior.

## Non-goals

- Full GitHub mutations; conversation content may be read-only in this RFC.
- Final theming and syntax customization; those arrive in RFC-0006.
- Dashboard and observer queue.

## Architectural allocation

- **Core:** owns semantic navigation, current file/change selection, command legality, comparison mode, selection synchronization policy, and compact command/status projections.
- **Host:** assembles bounded file/conversation observations, materializes changed projections, schedules preparation work, persists settings through explicit requests, and publishes only affected deltas.
- **Qt adapter:** owns the main window, top toolbar, collapsible drawers, splitters, epoch controls, `QAction` instances, `QAbstractItemModel` adapters, custom diff widgets, focus, keyboard, accessibility, high-DPI layout, and visual-only scroll/hover state.
- A `QAction` trigger becomes a semantic intent. `QAction` enabled/checked state comes from one command projection. Qt models use stable identities and incremental insert/remove/move/data-change notifications; routine selection/comment/receipt changes must not reset the model.
- No workbench widget may call GitHub, Git, SQLite, or AI services directly or decide review policy.

## UI architecture

Custom diff views derive from `QAbstractScrollArea` and paint only visible rows using `QTextLayout`. The file and conversation regions use Qt model/view controls and custom delegates. Comments remain in the right drawer and synchronize with line anchors; they never become inline full-width rows in the code view.

## Required interactions

- Top toolbar buttons and shortcuts for file/change navigation.
- Drawer collapse, resize, restore, and keyboard access.
- Epoch selector changes that preserve file/selection when possible.
- Selecting a connector, hunk, line, or conversation synchronizes the top panes and lower detail pane.
- Lower pane resize/collapse and context adjustment.
- Split/unified top-view options may exist, but the default is side-by-side plus lower line-over-line detail.

## Acceptance criteria

- The rendered workbench matches the normative layout rather than the non-normative mockup pixel-for-pixel.
- File and conversation drawers are independently collapsible and restore persisted widths.
- Primary navigation is always visible at supported window sizes or moves into an explicit compact overflow without losing keyboard access.
- Left/right epoch selectors show labels and exact OIDs and can select arbitrary retained epochs.
- Connector shapes correspond to the diff group model and avoid confusing crossings.
- The lower pane spans the central width and shows the selected group line over line.
- UI automation proves navigation, selection synchronization, focus order, loading, failure, cancellation, and restart persistence.
- A large-file harness proves widget count remains bounded by visible UI structure rather than source-line count.
- Qt adapter tests prove all primary actions translate to semantic intents, `QAction` state follows command projections, and bounded row/state changes use incremental model signals rather than full resets.

## Verification matrix

| Claim | Direct surface | Proof |
|---|---|---|
| Primary actions are top-level | Real application | Inspect and invoke buttons/shortcuts at supported widths |
| Drawers return space to center | Real application | Collapse/reopen each drawer and measure central viewport |
| Connector band maps groups | Rendered fixture | Compare connector bounds to model group extents |
| Lower pane is full-width detail | Real application | Select group and verify paired rows/central width |
| Renderer is virtualized | Instrumented app | Load huge file and inspect widget count, frame time, and memory |
| Qt is a presentation adapter | Qt adapter harness | QAction/model/widget events produce intents and consume incremental projections only |

## Documentation impact

Workbench layout, command map, focus model, drawers, epoch controls, selection synchronization, custom rendering, accessibility, supported sizes, and error states are mandatory.

The documentation ledger must identify functional-core, application-host, and Qt-presentation-adapter ownership; typed intents/events/observations/requests/deltas; authority; concurrency/generation; and any evidence-backed no-impact region.
