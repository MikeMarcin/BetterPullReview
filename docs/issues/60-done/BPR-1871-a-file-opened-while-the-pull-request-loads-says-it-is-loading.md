---
schema: "implement-rfc.card.v1"
id: "BPR-1871"
title: "A file opened while the pull request loads says it is loading"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","found_by":"Owner, 2026-11-05: \"I don't like that if I try to open a file while it's still loading the pr the tab shows the \\\"no file opened\\\" screen instead it should indicate were loading that file. And if possible we should prioritize loading that file.\"","reservation":"docs/issues/_reservations/BPR-1871--BPR-1871--33618eb8205e.json","theme":"app responsiveness","related_cards":["BPR-1287","BPR-1327","BPR-1344","BPR-1576","BPR-1842","BPR-1857"],"frontend_design":"frontend-design:frontend-design loaded before the presentation change; no new visual language: the comparison's existing BPR-1576 veil (code-shaped placeholders, turning mark, words line) names the file, and a failure uses the veil's in-place failure sentence. Offscreen renders inspected: the slow line was shortened so a narrow pane no longer elides it, and the veil's in-place failure may now wrap to six lines so BPR-1857's reason is read whole.","measurement":"Release (qualification-release), macOS arm64, offscreen, public-path fixture with the Git fetch held 2,500 ms; before at 7da4da3e2 (plus the course and fixture hold only) and after at 82784ad9e."}
acceptance: [{"criterion":"AC1 A file opened before its comparison can be read shows a loading state naming it, never No file open, and opens by itself when ready; a second choice replaces the first","outcome":"pass","revision":"82784ad9e","evidence":{"kind":"command","command":"ctest --preset qualification-release -R '^application_file_open_while_loading_public_path$'","detail":"Composed application, clone held: the review is accepted and Files lists review.cpp and secondary.cpp while the clone is held. Choosing review.cpp shows 'Loading review.cpp…' (not idle); choosing secondary.cpp replaces it with 'Loading secondary.cpp…'; the surface is never idle until secondary.cpp's comparison shows with no further gesture, and the published file is the selected one. Before (same course, --measure-only): idle=1, empty loading words, i.e. No file open."}},{"criterion":"AC2 A file that cannot open says why in its place, with BPR-1857's reason","outcome":"pass","revision":"82784ad9e","evidence":{"kind":"command","command":"ctest --preset qualification-release -R '^application_file_open_while_loading_failure_public_path$'","detail":"Same course, the held clone then fails with a network error: the comparison area shows the veil's failure look in secondary.cpp's place: 'No exact comparison is ready for this file. Fetching the pull request's commits did not reach GitHub: a network, proxy or certificate problem, or a timeout. Check the connection, then refresh.' The refusal is kept off the status line while the load still runs and reaches it when the load stops."}},{"criterion":"AC3 The requested file's work goes first; what is serialized is named and measured","outcome":"pass","revision":"82784ad9e","evidence":{"kind":"command","command":"ctest --preset qualification-release -R '^(core_comparison_preparation|application_file_open_while_loading_public_path)$'","detail":"core::decide_opened_file_wait and core::opening_preempts_preparation are unit-tested. In the course no preparation round started while the chosen file waited (PreparationsStartedBeforeOpenedFile 0). Clone answered to file shown: before 998/739 ms, after 814/764/756/732 ms; of that, clone answered to review history ready is 663-741 ms (whole-PR pack import and catalogue commit) and ready to file shown is 70-76 ms (selected resolve 22-26 ms, diff 50 ms). No measurable change is claimed: the per-file stage was already first."}},{"criterion":"AC4 Owner: the loading state's look","outcome":"pending","revision":"82784ad9e","evidence":{"kind":"observation","detail":"Pending: attended session (see Attended review below), candidate BPR-1871-loading-look-v001."}},{"criterion":"AC5 Owner: the file appears quickly enough once the pull request has loaded","outcome":"pending","revision":"82784ad9e","evidence":{"kind":"observation","detail":"Pending: attended session (see Attended review below), candidate BPR-1871-file-appears-v001."}},{"criterion":"AC6 Neighbouring courses and gates","outcome":"pass","revision":"82784ad9e","evidence":{"kind":"command","command":"ctest --preset qualification-release -R '^(qt_diff_workbench_scale_100|qt_workbench_window_scale_100|qt_pending_veil|application_pending_states_public_path|application_progressive_open_public_path|application_refresh_current_file_public_path)$'; _coding-standard/scripts/format_check.py <changed files>; python3 scripts/verify_documentation.py --root .","detail":"All pass; clang-format 22 passes on the 14 changed C++ files. test_network_isolation_manifest stops on gh_cli_git_smart_transport (an executable this partial build tree did not build), before reaching this card's two new WINDOWS_UNCONTAINED rows, which are pinned 'unmeasured (registered on macOS)'."}}]
---

# BPR-1871: A file opened while the pull request loads says it is loading

## Report

The owner, 2026-11-05: "I don't like that if I try to open a file while it's
still loading the pr the tab shows the "no file opened" screen instead it
should indicate were loading that file. And if possible we should prioritize
loading that file."

## Root causes

- **The host's "not yet" read as nothing.** Opening a file before the review
  history had an accepted comparison made `submit_selected_diff` publish
  `selected_file_comparison_unavailable`. The window put that sentence on the
  status line and left the comparison area idle ("No file open"). Once the
  comparison arrived, the composition already asked for the file again
  (`refresh_selected_diff`), so it did open by itself; only the wait was
  invisible.
- **The selected diff's begin cleared to idle.** Between a selected diff's
  `begin` (which calls `clear_diff_selection`, then `surface->clear()`) and the
  diff host's `set_loading`, the comparison area showed "No file open" too,
  for the resolve of the file's blobs.

## Built

- **Core** (`comparison_preparation`): `decide_opened_file_wait(facts)`
  returns `none`, `loading` or `failed` from whether a file is open and
  shown, its own failure, a missing-comparison refusal, the accepted and
  pending comparison and the review history's status. A refusal while the
  history is still being read means "not yet"; once the history has stopped
  without a comparison (ready with nothing pending, unavailable, failed,
  cancelled) it fails the file. `opening_preempts_preparation(running,
  preparesOpenedFile)` decides that an opened file stops a running
  preparation round that is not preparing it.
- **Composition** (`github_application`): gathers the facts and presents the
  decision after the selection changes, a workbench failure, a selected diff's
  begin and failure, and every review-history update. The missing-comparison
  refusal is kept off the status line while the decision is `loading`, and
  reaches it when the decision becomes `failed`. Failures are bound to the
  workbench generation, so another pull request's never applies. On a selected
  diff's begin it stops a running preparation round of other files and defers
  preparation until the reviewer's work is done.
- **Qt** (`diff_workbench`, `workbench_window`): `set_waiting(path, history)`
  shows the panes (not the idle direction) under the BPR-1576 veil with
  "Loading <file>…" and, past the slow mark, "Still fetching the pull request's
  history." or "Still reading the file from the repository."; `set_loading`
  keeps the name through the diff host's own wait; `set_waiting_failed`
  shows the BPR-1857 sentence in the veil's in-place failure look. The status
  line above the panes says the whole path. `pending_veil` lets an in-place
  failure wrap to six lines instead of three.
- **Fixture**: `github_read_delay_fixture::GitFetchMilliseconds` holds each
  Git fetch (cancellable), `GitFetchFails` then fails it as a network error;
  the attended launch takes `--fixture-clone-delay-ms`.

## What is serialized, and what was reprioritized

1. **Review history acquisition (whole pull request, unavoidable before any
   file).** One smart fetch per revision (sparse commit trees when the host
   supports it, otherwise the complete closure), then pack inventory, spool,
   import and catalogue commit on the repository's actor. No file's blobs can
   be resolved before the revision is accepted, so nothing per file can go
   ahead of it. On the fixture this import and commit take 663-741 ms after the
   fetch answers; that is the largest share of the wait after the clone and a
   candidate for its own card.
2. **The repository actor's queue** (`profile_epoch_actor_registry`): reviewer
   jobs ahead of preparation jobs since [[BPR-1344]], but a job already running
   is not preempted. Reprioritized here: opening a file now stops a running
   preparation round that is not preparing that file
   (`opening_preempts_preparation`), so a remote blob batch for other files
   does not hold the opened file back. Preparation starts again after.
3. **Per-file resolve, diff and syntax.** Already ordered for the reviewer:
   when the comparison becomes available the composition asks for the
   selected file before scheduling preparation, and preparation is deferred
   while the reviewer's work is outstanding ([[BPR-1344]]). The course shows
   no preparation round started while the chosen file waited.

## Measured

`application_file_open_while_loading_public_path` (`--measure-only` for a
build without this card; `--clone-ms N`; `--render-directory DIR`). Release,
clone held 2,500 ms, milliseconds:

| | Before | After |
|---|---|---|
| Comparison area while the file waits | idle, No file open | "Loading secondary.cpp…" |
| Clone answered to file shown | 998, 739 | 814, 764, 756, 732 |
| Clone answered to history ready | 663 | 741, 688, 683, 662 |
| History ready to file shown | 76 | 73, 76, 73, 70 |

## Attended review

Pending on the owner. Frozen with `scripts/freeze_attended_build.py freeze
--name BPR-1871-v001 --fixture github-com --open-pull-request --commit
82784ad9e --argument=--fixture-clone-delay-ms --argument=12000` into
`/Users/jaeger/BPR-attended/BPR-1871-v001.d9g45768`; the launcher's own
check reached `ATTENDED_PULL_REQUEST_OPENED`. `better_pull_review` SHA-256
`046d422df4084742ce88fa68f46d035f70853674c34a12fb1a95611c63b7a267`. It opens
octo-org/review-repository #17 on its Overview with each Git fetch held
12 s. Launch: `python3
/Users/jaeger/BPR-attended/BPR-1871-v001.d9g45768/launch.py`.

Fresh short session `9661616c0a3205dd8d94027b12f65c82`, state
`/Users/jaeger/BPR-attended/review-state/BPR-1871-v001/attended-review`,
both candidates ready (loopback host started for the owner):
`BPR-1871-loading-look-v001` (AC4, Accept/Reject) and
`BPR-1871-file-appears-v001` (AC5, Accept/Reject).

## Owner acceptance (2026-11-06)

AC4 and AC5 passed: the owner accepted `BPR-1871-loading-look-v001` and `BPR-1871-file-appears-v001` (session `9661616c0a3205dd8d94027b12f65c82`) with no note.
