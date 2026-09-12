---
schema: "implement-rfc.card.v1"
id: "BPR-0742"
title: "An unreadable workbench layout store is silent since BPR-0718"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: []
priority: "P3"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"found_at":"f35bbeb8","found_by":"attended-prep-2026-09-10 d18"}
acceptance: [{"criterion":"AC1 With PRAGMA user_version=999 on state/v1/workbench-settings.sqlite3, a restarted window tells the user the saved layout is unavailable, and the sentence survives the first workbench projections; the store's version and the window allocator are unchanged","outcome":"pass","revision":"7c6e2ca9","evidence":{"kind":"command","detail":"ctest --test-dir build/dev -j1 -R '^(application_profile_window_layout_public_path|application_profile_window_restart_public_path|application_profile_multi_window_public_path|qt_workbench_window_scale_(100|125|200)|qt_composed_github_scale_(100|125|200)|architecture_boundaries|application_workbench_composition_contract|application_composition_contract|application_review_round_trip_composition_contract)$' --output-on-failure, 13/13 passed (build/logs/bpr-0742-d24-integration.log), run on the working tree that was then committed unchanged as 7c6e2ca9. application_profile_window_layout_public_path passed in 22.99 s. Its driver, tests/end_to_end/profile_window_layout_contract.py, sets PRAGMA user_version=999 before the broken phase. Afterwards it refuses a changed profile_registry_meta.last_window_id or a user_version other than 999. The broken phase waits, for every restarted window, until the window has a workbench publication and its status line contains 'The saved workbench layout is unavailable.'. It then asserts the sentence again after open_review's projections. The restore phase asserts its absence over a readable store. The defect was reproduced red first, against the product code at 004ee4ec with only the new assertion added: 'window=2 publications=1 failures=0 status=No pull request is open in the workbench.' and 'FAILED: a restarted window tells the user the saved layout is unavailable' (build/logs/bpr-0742-d24-red.log). qt_workbench_window_scale_{100,125,200} pin the retained state at the widget (test_unavailable_saved_layout_survives_workbench_projections): the sentence survives two projections, appears once alongside a host settings_unavailable failure, and remains after that failure is cleared."}},{"criterion":"AC2 application_profile_window_layout_public_path and application_workbench_composition_contract pass","outcome":"pass","revision":"7c6e2ca9","evidence":{"kind":"command","detail":"ctest --test-dir build/dev -j1 -R '^(application_profile_window_layout_public_path|application_profile_window_restart_public_path|application_profile_multi_window_public_path|qt_workbench_window_scale_(100|125|200)|qt_composed_github_scale_(100|125|200)|architecture_boundaries|application_workbench_composition_contract|application_composition_contract|application_review_round_trip_composition_contract)$' --output-on-failure, 13/13 passed (build/logs/bpr-0742-d24-integration.log), run on the working tree that was then committed unchanged as 7c6e2ca9. Required: application_profile_window_layout_public_path passed in 22.99 s, and application_workbench_composition_contract in 22.06 s. Also kept green: application_composition_contract (143.89 s); application_review_round_trip_composition_contract (11.22 s), whose full-application source digest and production app manifest digest are repinned for the github_application.cpp change; application_profile_window_restart_public_path (6.61 s); application_profile_multi_window_public_path (77.35 s); qt_composed_github_scale_{100,125,200}; and architecture_boundaries."}},{"criterion":"Scope: decide where the loss of layout persistence is told, keeping BPR-0718's fallback and the composition contract green","outcome":"pass","revision":"7c6e2ca9","evidence":{"kind":"observation","detail":"Decided: the status line of each window, as retained window state. It is not a Home notice, and no chrome was added. Composition already decides availability once, from the one store open it does not keep. At window composition, github_application.cpp passes !Profile->WorkbenchPersistenceAvailable to the new workbench_window::set_saved_layout_unavailable. While the flag is set, update_status_text appends the existing settings_unavailable sentence, only once if a host failure says the same. A later projection that resets Failure does not clear it. BPR-0718's fallback is untouched: the aDesktop branch and configure_desktop_layouts are unchanged, and the broken phase still asserts the composed files width of 320 rather than the saved width."}},{"criterion":"Touched C++ is formatted and the boundaries hold","outcome":"pass","revision":"7c6e2ca9","evidence":{"kind":"command","detail":"clang-format 23.1.0 --dry-run --Werror exited 0 on workbench_window.hpp, workbench_window.cpp, github_application.cpp, workbench_window_tests.cpp and profile_window_layout_public_path_tests.cpp. architecture_boundaries passed in 8.52 s in the same batch. The slice owner (rfc-worker) reached its turn limit before returning. The integrator inspected its quiescent partial work (the diff, the logs, and no process left running), adopted it, and ran the verification above itself."}}]
---

# An unreadable workbench layout store is silent since BPR-0718

[BPR-0708](BPR-0708-window-layout-restoration.md) accepted that a
layout store with a future schema (`PRAGMA user_version` above
`workbench_settings_schema_version`) is preserved and reported: the
window's status line carried "The saved workbench layout is unavailable."
from the workbench host's `settings_unavailable`.
[BPR-0718](BPR-0718-the-workbench-composition-contract-is-red-at-the-pack-head.md)
then made composition decide availability by one open that is not kept
(`src/app/src/github_application.cpp`, the `aDesktop` branch that installs
`lazy_workbench_defaults_port` or `session_workbench_settings`) and
`configure_desktop_layouts` installs the per-window layout service only
when persistence is available. A store that cannot open now yields the
composed defaults for every window and the load succeeds, so nothing is
published and the user is not told that their drawer widths and states
will not be kept. Found under
[BPR-0740](BPR-0740-the-layout-public-path-no-longer-opens-the-exact-public-review.md)
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
