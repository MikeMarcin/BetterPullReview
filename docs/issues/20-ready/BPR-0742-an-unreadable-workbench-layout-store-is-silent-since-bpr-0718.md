---
schema: "implement-rfc.card.v1"
id: "BPR-0742"
title: "An unreadable workbench layout store is silent since BPR-0718"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: []
priority: "P3"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"found_at":"f35bbeb8","found_by":"attended-prep-2026-09-10 d18"}
---

# An unreadable workbench layout store is silent since BPR-0718

[BPR-0708](../60-done/BPR-0708-window-layout-restoration.md) accepted that a
layout store with a future schema (`PRAGMA user_version` above
`workbench_settings_schema_version`) is preserved and reported: the
window's status line carried "The saved workbench layout is unavailable."
from the workbench host's `settings_unavailable`.
[BPR-0718](../60-done/BPR-0718-the-workbench-composition-contract-is-red-at-the-pack-head.md)
then made composition decide availability by one open that is not kept
(`src/app/src/github_application.cpp`, the `aDesktop` branch that installs
`lazy_workbench_defaults_port` or `session_workbench_settings`) and
`configure_desktop_layouts` installs the per-window layout service only
when persistence is available. A store that cannot open now yields the
composed defaults for every window and the load succeeds, so nothing is
published and the user is not told that their drawer widths and states
will not be kept. Found under
[BPR-0740](../60-done/BPR-0740-the-layout-public-path-no-longer-opens-the-exact-public-review.md)
when the layout course's broken phase was traced (d18); that course now
pins the BPR-0718 fallback and the preserved store.

## Scope

- Decide where the loss of layout persistence is told once per process:
  a status-line sentence on each window, or a Home notice. The window's
  `Failure` is cleared by the next workbench projection, so a one-shot
  `settings_unavailable` at start is not visible; the report needs a
  retained state or a different surface.
- Keep BPR-0718's fallback (composed defaults, the store untouched) and
  the composition contract green.
- Owned paths: `src/app/src/github_application.cpp` (both digests
  repinned), `src/adapters/qt/src/workbench_window.cpp`,
  `tests/end_to_end/profile_window_layout_public_path_tests.cpp` (the
  broken phase regains a visibility assertion).

## Acceptance

- With `PRAGMA user_version=999` on `state/v1/workbench-settings.sqlite3`,
  a restarted window tells the user the saved layout is unavailable, and
  the sentence survives the first workbench projections; the store's
  version and the window allocator are unchanged.
- `application_profile_window_layout_public_path` and
  `application_workbench_composition_contract` pass.
