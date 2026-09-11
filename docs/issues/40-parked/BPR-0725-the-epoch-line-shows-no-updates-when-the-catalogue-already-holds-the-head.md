---
schema: "implement-rfc.card.v1"
id: "BPR-0725"
title: "The epoch line shows no updates when the catalogue already holds the head"
status: "Parked"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"d06b4b43-0eb3-42fc-aa35-608f7da8935f"}
parked_on: "human-review"
acceptance: [{"criterion":"AC1 application-host test: a catalogue already holding the head publishes a loaded HistoryPage before or with ready; failed at HEAD","outcome":"pass","revision":"working tree over a8425b72","evidence":{"kind":"command","detail":"a_cached_head_publishes_the_newest_history_page_before_ready failed at 113e8dea (no reserve call, ready reached, no page); passes after the core requests the newest page; ctest application_epoch_host 6/6 batch"}},{"criterion":"AC2 Qt test: catalogue identity plus comparison only; line honest, page draws nodes","outcome":"pass","revision":"working tree over a8425b72","evidence":{"kind":"command","detail":"test_epoch_line_waits_for_the_newest_page_on_a_cached_head; qt_epoch_comparison_scale_100/125/200 pass"}},{"criterion":"AC3 composed course restoring or opening a cached review ends with the line carrying the catalogue's updates","outcome":"pass","revision":"working tree over a8425b72","evidence":{"kind":"command","detail":"application_profile_window_review_restoration_public_path, application_epoch_persistence_github-com, application_composition_contract, qt_composed_github_scale_100 and application_epoch_maintenance_public_path (after the probe repair) pass"}},{"criterion":"AC4 the owner sees nodes, span and handles on the signed bundle with microsoft/terminal#20607 restored; BPR-0646/0663/0676 re-asked","outcome":"pending","revision":"working tree over a8425b72","evidence":{"kind":"freeform","detail":"Waits for the v007 bundle and attended session"}}]
---

# The epoch line shows no updates when the catalogue already holds the head

The owner, in the v006 attended session on the bundle built at d1ba791f,
with microsoft/terminal#20607 restored at launch and read signed out,
rejected three candidates for one thing. On
[BPR-0646](BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md)
(decision `d06b4b43`): "There is no visible epoch line". On
[BPR-0663](BPR-0663-public-preview-refuses-the-epoch-smart-acquire.md)
(decision `da24251c`): "Shows No updates loaded". On
[BPR-0676](BPR-0676-the-epoch-line-does-not-show-whether-it-is-working.md)
(decision `180fd1d8`): "No visible epoch line in the test". The screenshot
([owner-read-chrome-2026-09-11.png](../../verification/reviews/rfc-bpr-0028-attended-v006/captures/owner-read-chrome-2026-09-11.png))
shows the Updates row reading "No updates loaded." beside "pair ready" and
"Comparing exact anchors", the diff rendered ("Diff ready — 2 semantic
changes") and the status line "Review history is ready." The same picture
was recorded without a verdict on the v003 run.

## Cause

The line has a ready pair and no nodes because nothing publishes the rows.
`epoch_line` prints "No updates loaded." when `Updates` is empty
(`src/adapters/qt/src/epoch_line.cpp:428-433`); `epoch_comparison_control::update_line`
fills it from `HistoryModel->rows()` (`epoch_comparison_control.cpp:640-649`),
and the model gains rows only from an `insert` in
`epoch_catalog_projection::Changes` or from a `HistoryPage` in the `loaded`
phase (`epoch_history_model.cpp:321-374`); the `Catalog` field carries the
identity only. The host produces an insert only from
`core::epoch_transition_output::AcceptedEpoch` and page rows only from
`History` (`src/application/src/epoch_application_host.cpp:2908-2952`).
In `src/core/src/epoch_transition.cpp`, `AcceptedEpoch` is set only when an
`append_epoch_mutation` commits (1368-1398); `epoch_catalog_loaded_event`
emits the identity only (1195-1198); and `epoch_revision_prepared_event`
with no mutation, which is the case when the cached catalogue already holds
the head epoch, discards the preparation and calls `start_comparison`
directly (1302-1309) without an `AcceptedEpoch`, a `History` or a page
request. No caller in `src/app` or `src/application` sends
`request_epoch_history_page_intent`; it is wired only to the Load history
and Return to newest menu actions (`epoch_comparison_control.cpp:522-527,
835-866`).

So the diagnostics log of the owner's launch (`catalog_open`,
`catalog_recovery`, `catalog_prepare`, no acquire) is the expected trace of
a cache hit, and the line is empty on every cache hit: a restored review,
and equally a fresh open through Home of a pull request already in the
cache. A first open inserts only the one new epoch, never the older
history. This is a defect in the direction RFC-BPR-0028 AC-02 fixes (one
node per upload, the compared pair as handles), not in the acquire
BPR-0663 corrected or the comparison BPR-0667 corrected.

## Scope

- Whenever a catalogue is loaded, or a revision is prepared with no
  mutation, the newest history page is published before or with the ready
  status, so the line draws every update the catalogue holds. Decide where
  that belongs (the core emitting the page after a no-mutation prepare or
  a catalogue load; the host or the control requesting the newest page
  once `Catalog` arrives with no rows) and say why in the code. The core
  decides, the host performs, Qt presents.
- Side finding to settle in the same slice or to card: a later append
  calls `invalidate_history_window` (1381) and the host then drops the
  insert when a window exists (2922-2925), so after a refresh the line can
  lose its rows until a page is reloaded.
- Owned paths: `src/core/src/epoch_transition.cpp`,
  `src/application/src/epoch_application_host.cpp`,
  `src/adapters/qt/src/epoch_comparison_control.cpp`, their tests.

## Acceptance

- An application-host test (`tests/application/epoch_application_host_tests.cpp`)
  opens a catalogue that already holds the head epoch, so prepare returns
  no mutation, and asserts a `loaded` `HistoryPage` with at least one row
  is published before or with the ready status; it fails at HEAD.
- A Qt test (`tests/adapters_qt/epoch_comparison_control_tests.cpp`) feeds
  only the catalogue identity and a comparison, as the product does on a
  cache hit, and asserts the line has nodes and does not read "No updates
  loaded."; today `test_epoch_line_selects_updates_by_click_drag_and_keys`
  supplies its own rows.
- The composed public-path course that restores a review, or opens a
  cached one, ends with the epoch line carrying the catalogue's updates.
- The owner sees the nodes, the span and the handles on the signed bundle
  with microsoft/terminal#20607 restored; the BPR-0646, BPR-0663 and
  BPR-0676 candidates are asked again on that bundle.

## Verification record, 2026-09-11 (d10)

Reproduced before fixing: the new application-host test
`a_cached_head_publishes_the_newest_history_page_before_ready`
(`tests/application/epoch_application_host_tests.cpp`) failed at
113e8dea with "a no-mutation prepare publishes the newest loaded history
page, carrying the held epoch, before or with the ready status", with no
`reserve` call and the comparison reaching ready, so the cause above is
the cause.

Fix, in the core: `request_newest_history_page`
(`src/core/src/epoch_transition.cpp:697-743`) records a newest
`PendingHistory`, emits the same `load_epoch_history_page_request` an
explicit Return to newest does and publishes `History` as `loading`; it is
called once the catalogue revision is settled, after a no-mutation prepare
(:1354) and after a committed mutation (:1453), each just before
`start_comparison`, so the actor's FIFO publishes the page before the
ready status. It is skipped when a page is in flight, when a window is
already bound to this revision (an older page the reviewer chose is
kept) or when the request counter is exhausted, and it is not issued at
catalogue load, where a commit about to advance the revision would retire
the read. The host is unchanged: an appended epoch still inserts its row
while the page loads (a first open whose page read fails keeps that row),
and the side finding, the window losing the new row after a refresh, is
closed by the commit-path reload. A host with a single job slot now
refuses the first comparison after ingestion (the shipped default is 64);
`establish_ready_comparison_in_one_slot` pins that in the three one-slot
host tests.

Tests: `test_settled_ingestion_requests_the_newest_history_page`
(`tests/core/epoch_policy_tests.cpp`; the no-mutation order, a bound
window left alone, an in-flight page not duplicated; `make_ready_fixture`
now asserts the page request precedes the resolve request and cancels
it); the host test above; `test_epoch_line_waits_for_the_newest_page_on_a_cached_head`
(`tests/adapters_qt/epoch_comparison_control_tests.cpp`: catalogue
identity plus a ready pair leaves the line at zero updates, the
"No updates loaded." condition, with no page intent from Qt; a loaded
page fed after draws every update with the pair's handles).

Integration found one regression: `application_epoch_maintenance_public_path`
green at 216f1f21 on a clean worktree (49 s), red with the change (budget
expired at `open`). Cause: in the pruned restart the line now already
holds both epochs at `open`, so the probe skipped its Return-to-newest
hop into the history stage, requested its diff from `open`, and `open`
then re-gated on the focused presentation every tick. The probe
(`src/app/main.cpp`, tests-enabled automated run) now advances to the
history stage when a probe step returns true without leaving `open`.
Observed, not chased: after that diff request the conversation panel
stays on the six unfocused roots (`file=0`); at HEAD the phase never
checked the focused presentation, so this is latent and not this card's.
Also not this card's: maintenance completion still marks the history
window stale without reloading it (`epoch_transition.cpp` ~:1940).

Commands, all at the working tree that became this commit: dev build
`-k 0` exit 0; `ctest --preset dev -R '^(application_epoch_host|application_epoch_contract|core_epoch_policy|qt_epoch_comparison_scale_(100|125|200))$'`
6/6; `qt_composed_github_scale_100` pass; `application_epoch_catalog_contract`,
`sqlite_epoch_catalog_behavior_contract`, `epoch_maintenance_integration`,
`application_composition_contract` (155 s),
`application_review_round_trip_composition_contract` (no digest change),
`application_epoch_persistence_github-com`,
`application_profile_window_review_restoration_public_path` (restores a
review through the same path; 930 s under a concurrent build) all pass;
`application_epoch_maintenance_public_path` pass (47 s) after the probe
change; `check_boundaries` exit 0; strict tidy on `epoch_transition.cpp`
and `main.cpp` clean; `format_check.py` on the five touched files exit 0;
`git diff --check` clean.

Acceptance 1-3 pass on these; 4 (the owner sees the nodes, span and
handles with microsoft/terminal#20607 restored) waits for the v007
bundle. Parked on human-review.
