---
schema: "implement-rfc.card.v1"
id: "BPR-0677"
title: "The comparison stays unavailable after the history is ready"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
acceptance: [{"criterion":"The status line never advises a refresh while the comparison is being prepared","outcome":"pass","revision":"bb99c12e","evidence":{"kind":"observation","detail":"The epoch control exposes the state behind its wording and the status line reads it: loading, preparing, committing, resolving and maintaining all produce 'Its comparison is being prepared.' with no advice; only a settled history with no comparison advises a refresh. qt_composed_github_scale_{100,125,200} pin both sentences, and making the preparing branch unreachable fails the first. Fifty-six Qt, core and composition selectors pass with boundaries, strict tidy, format and docs."}},{"criterion":"A public pull request opened signed out reaches an exact comparison with files that can be selected and read","outcome":"pass","revision":"d1ba791f","evidence":{"kind":"observation","detail":"Attended session cbd48b91000b00f1e72e1ad62c7c384c, candidate BPR-0677-comparison-readable, revision app-release-cc2afa7f-2026-09-11.2 (bundle built at d1ba791f, executable sha256 cc2afa7fb84d443a1bba93373b990be83748be20451fa0b6adde45cc6fae4c1c), fingerprint 5e2a51662f363c2800d2b490ce1367b727af838c814cd88c3f377a01c7309eaf, decision 541f2d1f-4318-429f-8ba5-b4d2dcf37eae, actor human, 2026-09-11T18:36:41Z: accept. The owner's screenshot on the BPR-0646 decision (captures/owner-read-chrome-2026-09-11.png) shows file 2 of 4 selected with 'Diff ready — 2 semantic changes' and the status line 'Actionable file 2 of 4. Private progress: unread, not accepted. Review history is ready.'"}}]
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"}}
---

# The comparison stays unavailable after the history is ready

The owner's 2026-09-07 capture, on the build carrying
[BPR-0667](BPR-0667-the-epoch-refuses-any-pull-request-whose-base-branch-moved.md),
shows the pull request loaded, four files listed, participants and
since-my-last-review populated, and this status line:

> This pull request is open. Its comparison is not ready; Refresh to try
> again. The workbench could not be updated. Review history is ready.

The diagnostics log then said something the screen did not. In that run
every stage succeeded: `protocol_acquire` at 20:56:50 after seven seconds,
`cache_inventory`, `catalog_prepare`, and `revision_prepare` at 20:58:25.
The screenshot is timestamped 20:58:28, three seconds after the comparison
was prepared, and it still said the comparison was not ready and offered a
refresh.

So there are two things here and neither is the acquire. The line treated a
comparison that was still being prepared, for ninety-five seconds, as one
that had failed, and told the reviewer to refresh, which would have thrown
that work away and started it again. And the workbench failure it also
quoted, "The workbench could not be updated", is cleared only by the next
workbench projection, so it can outlive the preparation that succeeded
after it.

## Scope

- The line tells work in progress from work that failed, and asks for
  nothing while the epoch is loading, preparing, committing, resolving or
  maintaining. Done.
- Ninety-five seconds of silent preparation is the rest of it: the reviewer
  is owed some sign that something is happening, and the wait itself is
  worth measuring against
  [BPR-0673](../30-in-progress/BPR-0673-the-large-review-benchmark-exceeds-two-mac-budgets.md).
- Whether a comparison actually arrives after a successful
  `revision_prepare` on the public path is still unobserved: the owner's
  screenshot was taken three seconds after it and they stopped there. A
  composed check that a public pull request reaches a comparison, not only
  a review history, would settle it.
- Owned paths: `src/adapters/qt` status line and epoch control, their
  tests.

## Acceptance

- The status line never advises a refresh while the comparison is being
  prepared. Done.
- A public pull request opened signed out reaches an exact comparison with
  files that can be selected and read, observed in the signed bundle.

## Notes (2026-09-07)

- **What the log settled.** In the owner's 20:56 run every epoch stage
  succeeded, `revision_prepare` included, at 20:58:25. Their screenshot is
  stamped 20:58:28. So the comparison was not refusing; it was being
  prepared, for ninety-five seconds, while the line called it not ready and
  told them to refresh.
- **What changed.** `epoch_comparison_control` exposes the state behind its
  wording, and the status line uses it: while the epoch is loading its
  catalogue, preparing, committing, resolving or maintaining, the line says
  "This pull request is open. Its comparison is being prepared." and asks
  for nothing. Only a settled history with no comparison still advises a
  refresh, because only then can a refresh help.
- **Evidence.** `qt_composed_github_scale_{100,125,200}` pin both
  sentences against a published epoch status; making the preparing branch
  unreachable fails "while the comparison is being prepared the line says
  so and asks for nothing". Fifty-six Qt, core and composition selectors
  pass, with `check_boundaries`, strict tidy, `clang-format --Werror` and
  `verify_documentation.py`.
- **Not closed.** Whether a comparison actually arrives on the public path
  after a successful `revision_prepare` is still unobserved: the owner
  stopped three seconds after theirs. That, and whether ninety-five silent
  seconds is acceptable, belong to the next attended session and to
  [[BPR-0673]].

## Attended verdict, 2026-09-11 (v006): accepted

Session `cbd48b91…`, revision `app-release-cc2afa7f-2026-09-11.2` (bundle built at d1ba791f, executable sha256 `cc2afa7f…`), actor human; the session was handed back at sequence 278 with all fifteen candidates decided. Candidate `BPR-0677-comparison-readable`, decision `541f2d1f`, 18:36:41Z: **accept**, no note.
The screenshot on the BPR-0646 decision
([owner-read-chrome-2026-09-11.png](../../verification/reviews/rfc-bpr-0028-attended-v006/captures/owner-read-chrome-2026-09-11.png))
shows file 2 of 4 selected, "Diff ready — 2 semantic changes" and "Review history is ready."
Done. The ninety-five-second silent preparation stays with
[BPR-0673](../30-in-progress/BPR-0673-the-large-review-benchmark-exceeds-two-mac-budgets.md).
