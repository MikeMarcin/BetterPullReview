---
schema: "implement-rfc.card.v1"
id: "BPR-1202"
title: "The review bar puts file navigation before change navigation"
status: "Parked"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1129"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac","win","linux"]
metadata: {"origin":"owner revision on attended candidate BPR-1129-bar, 2026-09-25","session":"06ad5734ed9f7c2fd6d0a62c8328fa0a","candidate":"BPR-1129-bar","decision":"fd158272-768f-469d-838a-08ef08e3e47a","reviewed_revision":"ba9d272339ad235f1d99688c5c1af198b0c9c7ef","reviewed_executable_sha256":"a2e5128a8990ccf35cda27b5bebfbbc2c76c2096d97c0fd85e3e7857a0fd8a83","criteria":["AC-03"],"related_cards":["BPR-1129","BPR-1135"],"reservation":"docs/issues/_reservations/BPR-1199--BPR-1207--b05c1525088e.json","milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"},"owner_handback49":{"session":"d7cf21b16656c6ec19c8787255aa4006","cursor":49,"source_revision":"8b5bc31cea0da8ec6d1cb016613052134366cae6","binary_sha256":"96f2083ce961d08dbbc022fc00b3e4178304e8f6536c949dc7de0a1f0292125b","decisions":[{"candidate":"BPR-1202-bar","criterion":"AC3","fingerprint":"e6db6d6f9d7c8d90f18dc17bdfc4a087aa26c1c73e2fd51d64e7f1f2cee0476b","decision_id":"dc8624d3-1462-44a7-b653-0bbf7a00645c","outcome":"reject","note":"Your screenshot doesn't show changes label for some reason.\nI think the file and changes arrow buttons should come after the labels.\nI think there should be something like perhaps a horizontal colored rail or something to signal the relationship of label to controls more strongly.","saved_at":"2026-09-26T21:05:53.594560+00:00","actor":"human"}]}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"1971a9ab82b965d57925023469ec9b14513de894","evidence":{"kind":"command","detail":"9/9 focused checks at 100/125/200, four-appearance composed capture, pin contract and documentation; rail pixels/negative order control and inspected renders. Logs /Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-r2-ln5i2_nd/evidence/1202-focused-final.log Native follow-up found stale rails; LayoutRequest repaint corrected, affected scale courses3/3 and native loading/loaded rails verified."}},{"criterion":"AC2","outcome":"pass","revision":"1971a9ab82b965d57925023469ec9b14513de894","evidence":{"kind":"command","detail":"9/9 focused checks at 100/125/200, four-appearance composed capture, pin contract and documentation; rail pixels/negative order control and inspected renders. Logs /Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-r2-ln5i2_nd/evidence/1202-focused-final.log Native follow-up found stale rails; LayoutRequest repaint corrected, affected scale courses3/3 and native loading/loaded rails verified."}},{"criterion":"AC3","outcome":"pending","revision":"1971a9ab82b965d57925023469ec9b14513de894","evidence":{"kind":"observation","detail":"Original handback49 Reject retained; corrected grouping awaits narrow successor review."}}]
parked_on: "human-review"
---

# BPR-1202: The review bar puts file navigation before change navigation

## Owner decision

In attended session `06ad5734` (packet [owner-review-20260925-fresh-v001](../../verification/reviews/owner-review-20260925-fresh-v001/operator-packet.md)), candidate `BPR-1129-bar` on the
frozen m2 app (`ba9d2723`), the owner first chose "Keep this review bar"
(decision `8b8b310b`, sequence 38) and later replaced it (decision
`fd158272-768f-469d-838a-08ef08e3e47a`, sequence 48, `actor: human`; the
session keeps the later decision and lists the earlier one as superseded):
"Revise the review bar; describe the change", with the note, verbatim:

> I want to swap the order of file and change. File contains changes so file should be left of change.

RFC-BPR-0052 AC-03 is amended to that order.

## Outcome

From its left the review bar reads: the Updates disclosure with the comparison
and its dates; File and its position before the file arrows; Changes and its
position before the change arrows; private progress. Compose review and Comment stay at
its right. Keys and tooltips are unchanged.

## Acceptance

- AC1 The bar's actions are in the amended order at every scale and at the
  width floor, and the focus order follows the visual order.
- AC2 The exact 14-action order in the window rows and in
  `application_uiforetw_selected_diff_contract`'s capture (all four appearance
  profiles) asserts the new order; a control with the old order fails. Any
  composition pin the change touches is resealed with its recorded review.
- AC3 The owner confirms the built bar on macOS: one question for this
  criterion alone.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
as the owner amended or decided it on 2026-09-25. The core decides, the host
performs, Qt presents. No publication, authority, provider, Git or storage
capability changes. Windows and Linux station runs follow the owed commands on
the originating card and [[BPR-1135]].

## Engineering evidence — 2026-09-26

The visual and declared focus order now places the file group before the
change group. The 14-action predicates cover the window and composed fixture;
a test copy with the old group order is rejected. Full-window and focus
courses at 100/125/200 and the four-appearance UIforETW composed course pass
7/7 in 136.44 seconds. The first run exposed a second old focus expectation
and the old composed geometry comparison; both were corrected and the whole
focused selection rerun. BPR-1201's width-floor loading and synthetic obsolete
readiness-row negative controls also pass in these window courses.

Composition source review: `5c094f80..BPR-1202 candidate` changes only the
14-action test-build capture expectation in `github_application.cpp`; no
runtime authority, publication, persistence or mutation selection changed.
The full reviewed digest is
`34ffd0af35bd3f46ad0e9ba1839191d82059b1f8cf5aefedc20a6d1e44ec31a5`,
production manifest `8a32003c8eff9ca994a5f1245f483648044baa9fc06967ef2b3c7c6379770859`;
mutation digest stays `a7e2081e57d4344efdf0caf8d6ad63568ba5717a6a9b0a61d6e57ddf053d1e8a`.
This intentional reseal records the source review, not an acceptance waiver.

Owner look AC3 and all originating native/station obligations remain pending.

## Combined main integration review — 2026-09-26

The [combined-source review](../../verification/combined-main-integration-2026-09-26.md)
reseals actual main after all 33 m2b commits and the already-integrated BPR-0972
changes. It names every changed production-manifest commit, preserves the
mutation pin and records the integration mapping. This engineering self-review
does not replace AC3's frozen-candidate owner judgment or station evidence.

## Owner hand-back 49 — 2026-09-26

The owner reviewed frozen source `8b5bc31c`, executable SHA-256
`96f2083ce961d08dbbc022fc00b3e4178304e8f6536c949dc7de0a1f0292125b`,
in session `d7cf21b16656c6ec19c8787255aa4006`. Full decision identities and
fingerprints are retained in the card metadata.

- **AC3 — Reject**, candidate `BPR-1202-bar`, decision `dc8624d3-1462-44a7-b653-0bbf7a00645c`.

  > Your screenshot doesn't show changes label for some reason.
  > I think the file and changes arrow buttons should come after the labels.
  > I think there should be something like perhaps a horizontal colored rail or something to signal the relationship of label to controls more strongly.

## Correction decision — hand-back 49

Keep File and Changes visible before their arrows at every width and in an
unavailable comparison. A two-pixel palette-derived accent rail runs beneath
each named group, joining the label to both arrows. Rationale: the position
states which thing moves; the continuous rail makes the control association
visible without adding another row or taking room from the comparison. The
groups retain their existing command, shortcut and focus identities.

The source review from integrated baseline `eb14f40e` finds only the test-build
14-action capture assertion changed in `github_application.cpp`. The production
mutation/admission branch is unchanged. Qt owns label wording, order and paint;
no source, sprite asset, provider, storage or authority policy changes.

At the width floor with a doubled system font, retaining both names exposed
an overflow. The final compact form keeps File and Changes while placing
their ordinal numbers in the full existing tooltips. This is the last yield,
after dates and connective words, and keeps every action top level.

Correction verification: all 9 focused checks pass in 160.94 seconds: window
and focus at 100/125/200, the shortest synthetic public path, the four-appearance
UIforETW composed capture, and the review composition pin contract. The original
run's three doubled-font overflow failures led to the compact correction above.
The final run occurred after rebuilding that correction. Pixel assertions and
inspected renders show each rail under its named group; a labels-after-controls
negative control fails. Documentation passes after reconciling the parent's
canonical AC-03 wording. The first smoke selector matched no tests and was
replaced by the registered public-path name; it supplied no evidence.

Reviewed full application digest:
`e496dd1ca8e476d887b464642f7799d9bf13215fcffc3fbda5cc664909ce3856`;
production manifest:
`e1f84da2b4fd484736cf85524fabdc304f58e2ab0e438b336dbf217925dbce30`;
mutation digest remains
`a7e2081e57d4344efdf0caf8d6ad63568ba5717a6a9b0a61d6e57ddf053d1e8a`.
AC3 remains a new pending owner judgment; the original rejection is retained.

## Native repaint correction

During authenticated fixture preparation, the native backing store retained
old rail positions when asynchronous comparison text moved child controls
without resizing the toolbar. A forced offscreen grab had hidden this defect.
The toolbar now requests repaint after servicing LayoutRequest. Three affected
window scale courses pass (66.25 seconds); a native run of the rebuilt bundle
shows both rails spanning their labels and arrow pairs during loading and after
the comparison label contracts. Existing action ordering and mutation pins stay
unchanged. Human AC3 remains pending on the successor.
