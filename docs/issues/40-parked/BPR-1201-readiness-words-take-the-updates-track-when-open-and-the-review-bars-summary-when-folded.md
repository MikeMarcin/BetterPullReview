---
schema: "implement-rfc.card.v1"
id: "BPR-1201"
title: "Readiness words take the Updates line's track when open and the review bar's summary when folded"
status: "Parked"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1130", "BPR-0795"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "owner decision on attended candidate BPR-1130-readiness, 2026-09-25", "session": "06ad5734ed9f7c2fd6d0a62c8328fa0a", "candidate": "BPR-1130-readiness", "decision": "5b349ec3-1536-4ed9-954b-572fda931f4f", "reviewed_revision": "ba9d272339ad235f1d99688c5c1af198b0c9c7ef", "reviewed_executable_sha256": "a2e5128a8990ccf35cda27b5bebfbbc2c76c2096d97c0fd85e3e7857a0fd8a83", "criteria": ["AC-01"], "related_cards": ["BPR-1130", "BPR-0795", "BPR-1197", "BPR-1135"], "reservation": "docs/issues/_reservations/BPR-1199--BPR-1207--b05c1525088e.json", "milestone_plan": {"id": "rfc-bpr-0052-review-workbench-scaffolding", "path": "docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"21f3c14781a743dbc9162545444c00c95c7e28bc","evidence":{"kind":"command","detail":"4/4 focused loading and full window scale courses; renders inspected; final integration controls still recorded separately."}},{"criterion":"AC2","outcome":"pending","revision":"21f3c14781a743dbc9162545444c00c95c7e28bc","evidence":{"kind":"command","detail":"Focused tests and scale renders pass; final integrated negative control and loading width-floor observations remain required."}},{"criterion":"AC3","outcome":"pending","revision":"21f3c14781a743dbc9162545444c00c95c7e28bc","evidence":{"kind":"command","detail":"Focused tests and scale renders pass; final integrated negative control and loading width-floor observations remain required."}},{"criterion":"AC4","outcome":"pending","revision":"21f3c14781a743dbc9162545444c00c95c7e28bc","evidence":{"kind":"observation","detail":"Owner loading-state look required on the final m2b candidate."}}]
---

# BPR-1201: Readiness words take the Updates line's track when open and the review bar's summary when folded

## Owner decision

[[BPR-1130]] asked first, before [[BPR-0795]]'s readiness row changes, where the
loading and readiness words should go. In attended session `06ad5734` (packet
[owner-review-20260925-fresh-v001](../../verification/reviews/owner-review-20260925-fresh-v001/operator-packet.md)), candidate `BPR-1130-readiness` on the frozen m2 app (`ba9d2723`,
executable `a2e5128a…8a83`), decision `5b349ec3-1536-4ed9-954b-572fda931f4f`,
sequence 44, `actor: human`, the owner selected, verbatim, "Move to the track
when open and summary when folded". No note.

Until this card lands, AC-01 is unsatisfied while a pull request loads: the
window course lets `reviewReadiness` sit between the review bar and the
Updates line.

## Outcome

While a pull request loads, or when its comparisons are unavailable, the
readiness words appear in the Updates line's track when the line is open, and
in the review bar's summary (beside the comparison and its dates) when it is
folded. [[BPR-0795]]'s row is gone, so no row comes or goes above the
comparison in any state. The words keep their accessible text, and a failure's
explanation stays complete and copyable as [[BPR-1197]] made it.

## Acceptance

- AC1 Loading, preparing and unavailable-comparison words appear in the track
  when open and in the bar's summary when folded, in every modality; the
  readiness row no longer exists.
- AC2 `test_the_epoch_row_spans_the_window_in_every_modality` stops allowing
  `reviewReadiness` and asserts nothing sits between the title row, the review
  bar and the open timeline while loading. A control that keeps the row fails.
  The loading courses that read the row's text follow it to its new place.
- AC3 The height floors hold open and folded while loading and loaded, and the
  renders at 100, 125 and 200 percent are inspected.
- AC4 The owner looks at the loading state on macOS: one question for this
  criterion alone.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
as the owner amended or decided it on 2026-09-25. The core decides, the host
performs, Qt presents. No publication, authority, provider, Git or storage
capability changes. Windows and Linux station runs follow the owed commands on
the originating card and [[BPR-1135]].

## Engineering checkpoint — 2026-09-26

Candidate `21f3c14781a743dbc9162545444c00c95c7e28bc` removes the readiness widget and places its words in the
existing 34-pixel Updates track when open or in the comparison summary when
folded. The complete words remain in the tooltip and accessible description;
the track text is selectable. No authority or loading policy changes.

`qt_composed_loading_scale_100` and `qt_workbench_window_scale_{100,125,200}`
pass, 4/4 in 64.89 seconds, with held-provider loading, every modality,
open/folded geometry and absence of the old row. The 100/125/200 loading
renders were inspected. Format and documentation gates pass. The isolated
frozen m2 public fixture still prints PUBLIC_PATH_OK, GITHUB_FIXTURE_OK and
RENDER_OK. Evidence is retained at
`/Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-it1v3wy6/evidence/`.

AC4's owner judgment is pending on the final integrated m2b candidate. The
combined-candidate epoch and full-window rows now bind the width-floor loading
and obsolete-row negative-control observations; none of this accepts
Windows/Linux station evidence.

### Combined-candidate correction

The later full-window render exposed Qt's centred clipping of a long folded
readiness message, hiding its beginning. The summary now elides its right edge
within the same 320-pixel bound; tooltip and accessible description retain the
complete words. The epoch-control course checks the beginning, ellipsis, complete
accessible text and restoration of the ordinary width after unfolding. This
changes no admission, loading or history policy.

The final epoch and full-window courses pass at 100/125/200 percent (six
rows in `final-qt-tests.log`), including loading/open/folded floors and the
obsolete-row negative control. The rendered loading text at each scale was
inspected. AC1–AC3 are verified; AC4 remains the separate owner question.
