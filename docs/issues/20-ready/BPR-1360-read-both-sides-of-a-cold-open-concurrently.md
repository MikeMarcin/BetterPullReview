---
schema: "implement-rfc.card.v1"
id: "BPR-1360"
title: "A cold open reads both sides of its comparison at once"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"BPR-1344's live probe on macOS (BPR-Test PR #43), 2026-10-09","reservation":"docs/issues/_reservations/BPR-1360--BPR-1360--54fd657dec33.json","related_cards":["BPR-1344","BPR-1327"],"filed_by":"bpr-main-win, at the coordinator's request"}
---

# BPR-1360: A cold open reads both sides of its comparison at once

## Observation

On live macOS, opening a modified file that is not prepared takes about 4.3 s
(BPR-Test PR #43, 60 modified files; shown, median; offscreen, real app). Most
of that is two exact blob reads, one per side, run one after the other. Each
costs about 2.1 s: the shipped single read is five `gh` processes.

The comparison's own work, `diff_application_host`'s `perform`, materializes
the left side, waits for it, and only then asks for the right
(`src/application/src/diff_application_host.cpp`, the two `materialize` calls).
Each read goes through the repository's epoch actor, so nothing overlaps.

Measured on Windows over the same `gh` calls (12 runs): a single exact read as
the transport makes it is 1044 ms median (p90 1163). A cold open's two reads
are 2097–2110 ms in `application_epoch_host`'s live-shaped test with those
figures.

## Outcome

A cold open asks for both sides' blobs together, so their network round trips
overlap: one batched read for both sides where the transport reads batches,
or two reads in flight at once. The reviewer then waits about one round trip,
not two.

Unchanged:
- every read stays authorized by the selected comparison's grant;
- a stopped request reads nothing more;
- a prepared comparison is answered without any read.

## Acceptance

- A host test: a cold open over a transport whose reads each take a fixed
  delay completes in about one delay, not two. A control with the current
  sequential reads takes two.
- The Mac live course of BPR-1344 (c), or its successor, shows the cold open
  on PR #43 near one round trip. Watch `X-RateLimit-Remaining` as AGENTS.md
  requires.
- `application_epoch_host`, `application_diff_host`, `gh_cli_git_smart_transport`
  and the BPR-1327 large-review selection pass.

## Notes

Filed separately from [[BPR-1344]], which keeps preparation out of the
reviewer's way. It depends on nothing there.
