---
schema: "implement-rfc.card.v1"
id: "BPR-1886"
title: "Validate conversation-model ancestry without repeated walks"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
metadata: {"kind":"performance","reservation":"docs/issues/_reservations/BPR-1886--BPR-1886--6d72c0e26776.json","related_cards":["BPR-1882"],"milestone_plan":{"id":"responsiveness-latency-v3","path":"docs/rfcs/plans/responsiveness-latency-v3/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"1722ae93d8c0d198677d6e72dd1b141d7494a124","evidence":{"kind":"observation","detail":"Ancestry classification visits each node once before changing parent edges; the independent generated oracle and new deep/cyclic tests preserve duplicate, missing, self-parent, cyclic and descendant behavior."}},{"criterion":"AC2","outcome":"pass","revision":"1722ae93d8c0d198677d6e72dd1b141d7494a124","evidence":{"kind":"observation","detail":"qt_github_conversation_model_property, qt_composed_github_scale_100, qt_github_conversation_panel and application_refresh_current_file_public_path pass. Exact trees, row ordering, incremental signals and persistent indexes are retained. The separately identified navigation-link overlap is also present in the pre-change render and is not accepted as correct layout."}},{"criterion":"AC3","outcome":"pass","revision":"1722ae93d8c0d198677d6e72dd1b141d7494a124","evidence":{"kind":"observation","detail":"Optimized serial five-sample model fixture improves the 10,000-comment depth-250 unchanged update median from 39.580 to 9.014 ms; flat controls and their 0.786 ms largest-sample increase are recorded without a universal speedup claim. No Debug timings."}},{"criterion":"AC4","outcome":"pass","revision":"1722ae93d8c0d198677d6e72dd1b141d7494a124","evidence":{"kind":"observation","detail":"Both changed translation units pass clang-format/tidy 22; documentation and whitespace checks pass. Card and one-line journal are complete, source is landed on main, and inherited architecture/manifest/unrelated lint failures are explicitly retained."}}]
---

# BPR-1886: Validate conversation-model ancestry without repeated walks

The owner requested continued direct responsiveness work, preserving other
behavior and landing each verified round on main with a card and journal line.
BPR-1882 made host projection linear; the Qt model separately walks every
parent chain on every apply, including unchanged refreshes on the UI thread.

## Acceptance

- AC1: Resolve the model's ancestry once per update; preserve duplicate, missing,
  self-parent and cyclic-parent handling, including every descendant of a cycle.
- AC2: Preserve exact row order, incremental signals and persistent indexes;
  run the existing generated-model oracle and focused composed/model checks.
- AC3: Record optimized before/after timings with no Debug timing gate.
- AC4: Pass focused source/documentation checks, record inherited failures,
  complete the journal/card and land the round on main.

## Evidence

The temporary parent map now holds each node's ancestry classification. One
iterative walk classifies each previously unknown node, reuses completed
ancestors and avoids copying parent strings at every hop. A separate pass
promotes cycles and every descendant to roots after classification, preserving
the previous result independently of unordered-map iteration. Existing
reconciliation, row ordering, move minimization and signals remain unchanged.

Release, Apple M4 Pro arm64, macOS 26.6.2, AppleClang 21.0.0, Qt 6.12.0,
default test allocator. Baseline product source is landed `8e9f8e8b0`.
The same `better_pull_review_qt_github_conversation_model_property_tests
--measure-ancestry` harness measures five unchanged updates per shape in a
`QCoreApplication`, with construction and initial application outside the
clock and no concurrent task-owned builds or tests. Long IDs approximate
projected GitHub identities. Medians are ms:

| Comments | Shape | Before | After |
|---:|---|---:|---:|
| 1,000 | Direct children of a thread | 0.453 | 0.416 |
| 1,000 | Reply chains, depth 250 | 2.900 | 0.666 |
| 1,000 | 250-node cycle plus descendants | 18.402 | 0.439 |
| 5,000 | Direct children of a thread | 2.227 | 2.266 |
| 5,000 | Reply chains, depth 250 | 18.299 | 3.993 |
| 10,000 | Direct children of a thread | 4.829 | 5.615 |
| 10,000 | Reply chains, depth 250 | 39.580 | 9.014 |

The targeted 10,000-comment deep update improves about 4.4 times. Flat controls
show no consistent improvement: the largest sample increases by 0.786 ms
(before range 4.503–5.406, after 5.134–6.387). This is a synthetic model-update
measurement, not a whole-window latency or universal speedup claim. Every
measured unchanged update emits zero insert/remove/move/change/reset signals.
No Debug timings are run or claimed.

- `qt_github_conversation_model_property` passes its existing independent
  oracle over 3,000 mixed snapshots, 5,000 arbitrary permutations and
  50,000-row rotations, plus new 1,000-comment deep/cyclic cases. The new cases
  verify exact trees, unchanged updates without signals, reversed ordering and
  retained persistent indexes. Duplicate, empty, missing, self-parent and
  cyclic identities remain covered by the generated cases.
- `qt_composed_github_scale_100`, `qt_github_conversation_panel` and
  `application_refresh_current_file_public_path` pass against the final model.
  The composed GitHub offscreen full-window output was inspected. The owner
  identified an overlapping navigation link in that render; the saved BPR-1885
  render, before this model change, has the same defect. It is being fixed as a
  separate issue, not accepted as correct layout. This changes model bookkeeping
  only; existing widget, layout and wording source is intact.
  The frontend-design and code-simplifier skills were applied to retain the
  established presentation and keep the algorithm local.
- Both changed translation units pass clang-format 22.1.8 and clang-tidy
  22.1.8 with warnings as errors; the final algorithm was checked again after
  refining the direct-parent path. No new dependency or cross-layer edge is
  introduced.

The inherited `architecture_boundaries` refusal in the BPR-1884 Git smart
transport is reproduced on pre-round main and documented in [[BPR-1885]].
The earlier architecture-fixture, network-manifest and unrelated whole-window
lint failures remain recorded in [[BPR-1883]]. This round changes none of those
surfaces; those broader failing courses were not repeated or claimed as passing.

The final documentation gate and `git diff --check` pass. The unchanged documentation regression suite and
composition inventory checks reuse the passing round-one evidence. The
[journal](../../development/responsiveness-journal.md) has one entry for this
improvement; the closeout records the landed source revision.

Source, tests and journal landed on main at `1722ae93d8c0d198677d6e72dd1b141d7494a124`.
