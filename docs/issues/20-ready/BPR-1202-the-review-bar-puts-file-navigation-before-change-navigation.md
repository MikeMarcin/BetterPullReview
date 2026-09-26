---
schema: "implement-rfc.card.v1"
id: "BPR-1202"
title: "The review bar puts file navigation before change navigation"
status: "Ready"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1129"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac","win","linux"]
metadata: {"origin":"owner revision on attended candidate BPR-1129-bar, 2026-09-25","session":"06ad5734ed9f7c2fd6d0a62c8328fa0a","candidate":"BPR-1129-bar","decision":"fd158272-768f-469d-838a-08ef08e3e47a","reviewed_revision":"ba9d272339ad235f1d99688c5c1af198b0c9c7ef","reviewed_executable_sha256":"a2e5128a8990ccf35cda27b5bebfbbc2c76c2096d97c0fd85e3e7857a0fd8a83","criteria":["AC-03"],"related_cards":["BPR-1129","BPR-1135"],"reservation":"docs/issues/_reservations/BPR-1199--BPR-1207--b05c1525088e.json","milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"},"owner_handback49":{"session":"d7cf21b16656c6ec19c8787255aa4006","cursor":49,"source_revision":"8b5bc31cea0da8ec6d1cb016613052134366cae6","binary_sha256":"96f2083ce961d08dbbc022fc00b3e4178304e8f6536c949dc7de0a1f0292125b","decisions":[{"candidate":"BPR-1202-bar","criterion":"AC3","fingerprint":"e6db6d6f9d7c8d90f18dc17bdfc4a087aa26c1c73e2fd51d64e7f1f2cee0476b","decision_id":"dc8624d3-1462-44a7-b653-0bbf7a00645c","outcome":"reject","note":"Your screenshot doesn't show changes label for some reason.\nI think the file and changes arrow buttons should come after the labels.\nI think there should be something like perhaps a horizontal colored rail or something to signal the relationship of label to controls more strongly.","saved_at":"2026-09-26T21:05:53.594560+00:00","actor":"human"}]}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"3161f459eca6dbba8ddb0e9a49e37b15cb145757","evidence":{"kind":"command","detail":"7/7 focused scale/focus/width-floor and composed four-appearance courses; old-order negative control; recorded source review and pin reseal."}},{"criterion":"AC2","outcome":"pass","revision":"3161f459eca6dbba8ddb0e9a49e37b15cb145757","evidence":{"kind":"command","detail":"7/7 focused scale/focus/width-floor and composed four-appearance courses; old-order negative control; recorded source review and pin reseal."}},{"criterion":"AC3","outcome":"fail","revision":"8b5bc31cea0da8ec6d1cb016613052134366cae6","evidence":{"kind":"observation","detail":"Owner hand-back 49, session d7cf21b16656c6ec19c8787255aa4006; BPR-1202-bar reject dc8624d3-1462-44a7-b653-0bbf7a00645c. Exact fingerprints and notes retained in metadata. Verdict applies only to the frozen candidate."}}]
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
and its dates; previous and next file with the position; previous and next
change with the position; private progress. Compose review and Comment stay at
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
