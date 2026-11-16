---
schema: "implement-rfc.card.v1"
id: "BPR-2194"
title: "The Overview's pending open moves to the workbench host"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"BPR-2191 follow-up, 2026-11-15","governing_rfc":"RFC-BPR-0028","related_cards":["BPR-2191","BPR-1842","BPR-2180"],"reservation":"docs/issues/_reservations/BPR-2194--BPR-2195--007cce25d132.json","lane":"claude/v0-5-0-prerelease-targets-52265d","commit":"18c55f938"}
---

# BPR-2194: The Overview's pending open moves to the workbench host

[[BPR-2191]] (1eab5b868) moved one pending open into the workbench host: a
file asked for from a first read's reported files. The host now ties it to
the review that reported the file, opens it once that review's own rows are
accepted, and cancels it on a newer review, another selection or a read with
no review. Qt shows only the pending open the host publishes.

The Overview's own pending open, from [[BPR-1842]], is still held in Qt. It is
`workbench_window::PendingReviewPath`, taken from
`OverviewPage->pending_open()`.

- **Set:** in `workbench_window_github_presentation.cpp`, when the read ends
  with `pull_request_ready`.
- **Opened:** by `open_pending_review_path`
  (`workbench_window_drafts_and_behaviour.cpp`). It runs on a deferred event
  turn after the overview is published and after file rows are applied.
- **How it opens:** it looks the path up in whatever rows the drawer holds
  (`file_for_path`) and dispatches a selection itself.

So Qt decides when the review has loaded, and which rows count as that
review's. That is the decision the architecture gives to the core and host.
Nothing in this path is known to fail. The card is about where the decision
lives.

## Acceptance

- AC01: The Overview's pending open is a workbench-host pending open, through
  the same mechanism as BPR-2191 (`workbench_application_host::open_when_listed`
  and `workbench_projection::PendingOpen`), or an extension of it that keeps
  one owner. The host opens it only once the review it was asked under is
  accepted and that review's own rows are observed. A newer review, another
  selection or a read with no review cancels it. Qt keeps no path of its own
  and no rule for when to open it.
- AC02: The behaviour the owner accepted under BPR-1842 and BPR-2180 is
  unchanged:
  - a file activated on the Overview while the review loads still opens in
    Review once the review has loaded;
  - an Overview no longer shown for that review opens nothing;
  - a read that ends without a review opens nothing.
- AC03: A host-level course covers opening on accept, cancelling on a newer
  review and cancelling on a new selection.
  `application_progressive_open_public_path` and
  `application_pending_states_public_path` pass `ctest --repeat
  until-fail:10`.

## Built (`18c55f938`)

In the window the Overview's activation while its review loads already went
to `wait_for_review_at`, which hands the path to the workbench host's
`open_when_listed` (BPR-2191). The Qt-held open was therefore reachable only
through the page's fallback, used when no waiting dispatch is set. It is
removed: `workbench_window::PendingReviewPath`, `open_pending_review_path`,
its three deferred triggers, the page's `PendingOpen`, `pending_open()`, the
fallback branch and the "Opening … when the review has loaded" plate. Qt keeps
no path and no rule for when to open. The host is the only owner (AC01). The
behaviour the owner accepted is the host's, which AC02 covers.

## Evidence

- AC03 host course: `application_workbench_host` passes. It opens on accept
  and cancels on a newer review and on a new selection (BPR-2191's cases).
  `qt_pending_veil` passes.
- `application_progressive_open_public_path --repeat until-fail:10`: 10 of
  10. It now counts the host's open (`PendingOpensOpened`) in place of the
  removed `pending_open()`.
- `application_pending_states_public_path --repeat until-fail:10`: failed on
  run 3 of 10 under concurrent builds. The cause was BPR-2195's
  repair_required, not this change: the kept diagnostic log shows
  revision_prepare and then catalog_recovery ending in repair_required, and
  the clicked file never arrived. This criterion waits on BPR-2195, so the
  card depends on it. What remains is that run, repeated once BPR-2195 is
  fixed.

## Closed, 2026-11-16

`application_pending_states_public_path --repeat until-fail:10` passes 10 of
10 (131 to 143 s each), at HEAD `605556fc9`, which holds `18c55f938`. With
the earlier 10 of 10 for `application_progressive_open_public_path`, AC03 is
met. AC01 and AC02 are met as recorded above.

The one earlier failure was BPR-2195's defect: under heavy load a cancelled
`git index-pack` leaves the cache needing repair. That is not this change,
and it stays open on its own card. The card no longer depends on it, since
this criterion now passes.
