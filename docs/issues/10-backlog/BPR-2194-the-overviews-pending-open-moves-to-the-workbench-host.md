---
schema: "implement-rfc.card.v1"
id: "BPR-2194"
title: "The Overview's pending open moves to the workbench host"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"BPR-2191 follow-up, 2026-11-15","governing_rfc":"RFC-BPR-0028","related_cards":["BPR-2191","BPR-1842","BPR-2180"],"reservation":"docs/issues/_reservations/BPR-2194--BPR-2195--007cce25d132.json"}
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
