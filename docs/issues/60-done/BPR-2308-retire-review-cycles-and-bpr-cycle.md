---
schema: "implement-rfc.card.v1"
id: "BPR-2308"
title: "Retire Review Cycles and bpr cycle: remove the feature, keep what local drafts share"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"retirement","target":"0.5.0","reservation":"docs/issues/_reservations/BPR-2308--BPR-2309--30601b62012d.json","origin":"owner, 2026-11-19/20: \"Are review_cycles even valuable anymore? I feel like they've been completely superseded by our local draft pr workflow\", then \"Yeah don't think we need it anymore\"","related_rfcs":["RFC-BPR-0053","RFC-BPR-0054","RFC-BPR-0055","RFC-BPR-0056","RFC-BPR-0057","RFC-BPR-0058","RFC-BPR-0059","RFC-BPR-0060","RFC-BPR-0061","RFC-BPR-0108"],"related_cards":["BPR-2309","BPR-1194"]}
---

# BPR-2308: Retire Review Cycles and bpr cycle

## Why

The owner, 2026-11-19: "Are review_cycles even valuable anymore? I feel like
they've been completely superseded by our local draft pr workflow", then on
2026-11-20: "Yeah don't think we need it anymore". Local PR drafts
(RFC-BPR-0108, 0110, 0111) cover reviewing one's own work before it is a pull
request. This card removes the feature; [[BPR-2309]] retires its RFCs, cards
and documentation and carries the release-notes line.

## Acceptance

- AC1: No Review Cycles surface remains: no Home section, no File › Open
  Project…, no local cycle page or sheets, no cycle keys in the shortcut
  catalogue, no Send to Agent, no `bpr cycle` commands, no `bpr-cycle` skill
  package or MASK selection.
- AC2: Code that local drafts, Suggest change, tours or diagnostic export use
  stays, with its tests (inventory below).
- AC3: Stored data is left alone. `review-cycles.sqlite3` and
  `publication-git-v1/` in an existing profile are neither migrated, read nor
  deleted by startup; a profile or settings file that mentions cycles still
  loads. Profile purge and the review-data reset still remove them, as
  before, because the person asked for that.
- AC4: `bpr cycle` says it was removed and points to `bpr draft`, exit 2.
- AC5: Build, `architecture_boundaries`, the focused tests below and the
  documentation gate pass; `application_composition_source_contract` passes
  after the sanctioned reseal.

## Inventory

### Removed

| Layer | Removed |
|---|---|
| core | `review_cycle_{association,bundle,command,disclosure,import,promotion,response,retrieval,round,round_markdown,send,share,tabs,verification}` (`.hpp`/`.cpp`); `agent_run_path::review_cycle_round` |
| application | every `review_cycle_*` header and source except `review_cycle_git_metadata` (store, runtime, capture, workbench, words, export, share, send, agent sender, round, response, verification, import, promotion, publication, publisher, publishing, projection, outgoing, comparison, feedback, retrieval, views); `build_round_launch`; the cycle overloads of `operation_failure_of`; the cycle half of `review_material_bridge`; the `cycle` owner operation and its request/response in `tour_owner_channel` |
| adapters | `sqlite/review_cycle_store`; the owner codec's cycle request and answer (owner protocol 9 → 10); `workspace/local_export_destination` and the `AdapterExport` library; Qt `review_cycle_view`, share, publish and promote sheets and share marks; the window's Open Project action and dispatch, Home's Review Cycles section and list, the local cycle page slot, `home_selection_list::cycles`; the cycle shortcut group and its seven commands; the resident menu bar's Open Project |
| app | `cycle_cli`, `cycle_commands`, `cycle_session`, `cycle_owner_service`, `review_cycle_desktop`; the windows' `CycleDesktop` and its wiring (Send to Agent offers, appearance, tour route, publication account, shutdown); the profile's cycle owner service and stop source; `desktop_cycle_owner_route`, `cycle_draft_editors`, `cycle_written`; the fixture hook `set_cycle_publication_account` |
| tests | the cycle core, application, sqlite, workspace-contract and end-to-end courses, the cycle CLI, skill and publication-matrix scripts, `cycle_send_agent_main`, the round-launch test, `review_cycle_view_tests`, the capture-based `review_cycle_git_reader_tests`, `stored_tour_port.hpp`; the cycle parts of the Home lists, title, page and catalogue tests; the fuzz target's round, response, manual-reply, bundle and claim families and their corpus seeds |
| packaging and tools | `packaging/agent-skills/bpr-cycle`, `scripts/run_bpr1170_harness_course.py`, `tests/scripts/run_live_publication_course.py`, the `.gitignore` lines for the MASK `bpr-cycle` links; the `bpr-cycle` MASK selection is uninstalled on main |
| registries | removed rows from `tests/release_test_profiles.json` and `scripts/verify_test_network_isolation.py`; the composed-host verifier's Send to Agent and cycle owner service anchors |

### Kept because something else uses it

| Kept | Used by |
|---|---|
| core `review_cycle.hpp`, `_workspace`, `_path_folding`, `_material`, `_manifest`, `_ignore`, `_git` | local drafts' worktree model and Git readers (RFC-BPR-0054, RFC-BPR-0083), `git_objects`, `generated_files`, prompt macros |
| core `_comparison`, `_views` | `local_draft_workbench`'s diff rows |
| core `_outgoing`, `_candidate`, `_policy`, `_publication` | local draft publication (`local_draft`, `local_draft_publication`, `publication_candidate_port`, `github_publication_contract`) |
| core `_feedback`, `_suggestion` | pull-request Suggest change (`suggestion_composer`, RFC-BPR-0090) |
| core `_json`, `_zip` (+ codec) | tours and agent context (`tour_agent_*`, `suggest_change`); diagnostic export (`release_maintenance`) |
| application `review_cycle_git_metadata` | `local_draft_discovery` |
| application `review_material_bridge.hpp` | only `review_material_status`, which a local draft's file view reports |
| adapter `workspace` (read-only source) | local drafts, clone-folder scan, app bundle read |
| core `operation_diagnosis_family::cycle_round`, its actions, `operation_store::review_cycles`, `diagnostic_operation::cycle_round` | reading diagnostic logs written before this card; nothing records them now |
| storage names `review-cycles.sqlite3`, `publication-git-v1` in storage diagnostics, Insights storage, release maintenance and profile data reset | the leftover files are measured, carried by maintenance and removed by purge or reset; nothing opens them |
| tests `core_review_cycle{,_policy,_workspace,_material,_manifest,_comparison,_feedback,_suggestion,_views,_outgoing,_candidate,_publication}`, `git_metadata_harness`, the `review_cycle` fuzz target's shared families | the kept code above |

The kept code keeps its `review_cycle_*` names; renaming it is a separate,
optional cleanup.

Also moved, not removed: the native-session name check `send_session_name`
became `native_session_name` in `agent_launch.cpp`; the profile's owner command
lane `CycleOwnerEffects` became `OwnerCommandEffects` and serves `bpr draft
open`; `LocalCycleTitle` became `LocalPageTitle` (a local draft's title); the
Git half of the old scale bench became `bench_git_metadata_scale` (row
`review_cycle_git_reader_scale` unchanged); the round launch the launcher tests
borrowed became `tests/support/plain_agent_launch.hpp` over the shipped
`build_agent_launch`; the composed-host verifier's local-automation anchor is
now `bpr::app::open_local_draft_here`.

## Evidence

macOS, dev preset (Debug, `-g0` to fit the disk), on the landed candidate's
source:

- Built: `better_pull_review_core`, `_application`, `_adapter_sqlite`,
  `_adapter_workspace`, `_adapter_qt`, `_composition`, `app_better_pull_review`
  and every test target below.
- Passed: `core_review_cycle`, `_policy`, `_workspace`, `_material`,
  `_manifest`, `_comparison`, `_feedback`, `_suggestion`, `_views`,
  `_outgoing`, `_candidate`, `_publication`, `core_operation_diagnosis`,
  `core_git_objects`, `application_agent_launch_policy`,
  `sqlite_tour_owner_channel`, `review_cycle_git_reader_scale`,
  `git_metadata_harness`, `qt_agent_steering_fixture`,
  `qt_claude_process_wrapper`, `qt_composed_github_scale_100`,
  `qt_keyboard_shortcuts_scale_100`, `qt_workbench_window_scale_100`,
  `qt_button_faces`, the six `application_local_draft_*_desktop_public_path`
  rows, `application_composition_source_contract`, `architecture_boundaries`,
  `verify_composed_hosts_tests.py`.
- Not built here: `bpr` (`BPR_BUILD_TOUR_CLI` is off in `dev`), so
  `tour_main.cpp`'s retired-command message and the tour CLI rows were not
  compiled on this lane. The fuzz target is built only in a fuzz graph.

`application_composition_source_contract` already failed on main before this
card (unsealed app changes). It is resealed under the contract's documented
procedure: the deleted sources leave the inventory, `window_follow_up.cpp`
joins it, and the full `github_application.cpp` digest and the production app
manifest digest are recorded with reseal notes naming every unsealed card.
