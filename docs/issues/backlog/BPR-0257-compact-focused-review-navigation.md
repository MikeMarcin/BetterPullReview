---
id: BPR-0257
title: Compact focused-review navigation into one icon row
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0246]
---

# Compact focused-review navigation into one icon row

The BPR-0246 focused state makes the comparison dominant, but its retained
navigation still presents drawer and navigation actions as text-heavy controls
whose placement does not match the product owner's review workflow. During the
UIforETW v002 attended checkpoint, the product owner required the Files and
Conversations drawer controls at the left and right window edges, on the same
row as compact Previous/Next change navigation, with Previous/Next file added
to that row. Every control is an icon, following Collaborator's Diff Viewer
toolbar navigation pattern rather than consuming code-pane space with labels.

## Required final state

- One compact navigation row spans the focused review surface.
- The Files drawer toggle is the row's far-left control; the Conversations
  drawer toggle is its far-right control.
- Previous file, Next file, Previous change, and Next change are icon-only
  controls on that same row, in semantic order between the drawer controls.
- Each icon has a stable tooltip, accessible name/description, focus behavior,
  keyboard shortcut, enabled state, and semantic intent. Icons never become the
  only machine-readable label.
- Previous/Next file traverse the accepted Files model without bypassing the
  existing semantic selection/host path. Previous/Next change retain the exact
  diff-workbench navigation and never recompute mapping in Qt.
- The row remains usable with either drawer open or collapsed, at 100%, 125%,
  and 200% scale, and in every shipped appearance profile.
- The UIforETW public course selects the recorded file through the shipped Files
  drawer, exercises all four navigation intents, collapses both drawers, and
  captures a new immutable attended batch. Prior v001 and v002 evidence remain
  unchanged; no verdict carries forward.

## Non-goals

- Do not replace semantic actions with tests-only widget manipulation.
- Do not hide repository, epoch, status, or drawer information permanently;
  compact controls must reopen every retained surface.
- Do not infer an attended verdict from conversational feedback. The new batch
  requires its own exact exported verdict.
- Do not change diff mapping, connector geometry, intraline roles, or recorded
  fixture identity unless direct verification exposes a defect.

## Verification

```sh
ctest --preset dev -R '^(qt_workbench_window_scale_(100|125|200)|application_uiforetw_selected_diff_contract|application_workbench_composition_contract)$' --output-on-failure
python3 scripts/verify_documentation.py --root .
git diff --check
```

The attended package must bind the four full-window profiles and comparison
crops after the deterministic row-placement, action-routing, accessibility,
enabled-state, navigation, selected-diff, role-pixel, and connector assertions
have all passed.

## Implementation checkpoint

The product now presents one persistent icon-only row in the required order:
Files, Previous file, Next file, Previous change, Next change, bounded Review
details/private-progress controls, a flexible spacer, and Conversations. The
row uses live `QStyle` standard icons and refreshes them on style/theme change.
Every drawer/navigation action retains its tooltip, accessible name and
description, focus stop, shortcut, and core-projected enabled state.

The comparison surface no longer carries duplicate Previous/Next buttons. Its
narrow `navigate_change(direction, policy)` bridge uses the existing semantic
index and the exact projected stop-versus-wrap policy, while the application
host admits bounded change-cursor facts only from the exact accepted
selected-diff request. File actions still traverse the retained Files model
through the semantic workbench-host path. Review receipts remain the same
actions inside one keyboard-accessible private-progress popup.

The focused host, Qt, composed-application, and recorded UIforETW checks are
green. The real course exercises all four navigation actions and both drawer
toggles, captures all four appearance profiles, and preserves exact blobs,
intraline role pixels, grounded connectors, and the cut-selection-wire
negative. Immutable v003 binds implementation commit `2124cbf`, ordered-source
digest `sha256:45f5c6da4c6c6c23c675565df6b94113ec07a54c55e28eef4bdbc403127f6616`,
and batch `bpr:/attended-review/betterpullreview/99b3c1cc6701ce6ab64d7ecb`.

No v001 or v002 decision is carried forward. The exact v003 verdict was
validated and recorded at
`docs/verification/reviews/uiforetw-intraline-acceptance-v003/` as a rejection:
the dark-mode Previous/Next icons are unreadable. BPR-0258 owns that narrow
repair and the required new attended batch. This card remains Backlog; the
rejected v003 subject is evidence, not acceptance.
