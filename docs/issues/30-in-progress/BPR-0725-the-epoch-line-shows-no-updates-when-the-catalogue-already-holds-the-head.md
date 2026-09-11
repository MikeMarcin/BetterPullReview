---
schema: "implement-rfc.card.v1"
id: "BPR-0725"
title: "The epoch line shows no updates when the catalogue already holds the head"
status: "In Progress"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"d06b4b43-0eb3-42fc-aa35-608f7da8935f"}
---

# The epoch line shows no updates when the catalogue already holds the head

The owner, in the v006 attended session on the bundle built at d1ba791f,
with microsoft/terminal#20607 restored at launch and read signed out,
rejected three candidates for one thing. On
[BPR-0646](../40-parked/BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md)
(decision `d06b4b43`): "There is no visible epoch line". On
[BPR-0663](../40-parked/BPR-0663-public-preview-refuses-the-epoch-smart-acquire.md)
(decision `da24251c`): "Shows No updates loaded". On
[BPR-0676](../40-parked/BPR-0676-the-epoch-line-does-not-show-whether-it-is-working.md)
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
