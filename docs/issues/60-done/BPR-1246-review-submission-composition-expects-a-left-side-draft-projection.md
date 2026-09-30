---
schema: "implement-rfc.card.v1"
id: "BPR-1246"
title: "The review submission composition course expects a left-side draft projection BPR-0587 no longer offers"
status: "Done"
rfcs: []
depends_on: []
metadata: {"origin":"triaged by the main stabilization milestone, macOS arm64, 2026-09-30","reservation":"docs/issues/_reservations/BPR-1245--BPR-1249--6920ac689bb3.json","related_cards":["BPR-0587","BPR-1207","BPR-1238"]}
acceptance: [{"criterion":"The three rows pass on macOS","outcome":"pass","revision":"b2fd518bd","evidence":{"kind":"command","detail":"ctest -R 'application_review_submission_composition' on macOS arm64 dev: public_path, scale_125 and scale_200 pass; application_review_round_trip_composition_contract passes with the reviewed reseal (mutation pin unchanged)."}},{"criterion":"Course follows the accepted rule: no new left-side draft, left negative control kept, local comment on a right-side line","outcome":"pass","revision":"1dbf9d5ca","evidence":{"kind":"observation","detail":"Course asserts no left draft on left:45, Comment/Publish refused there and cannot be forced open, local comment on right:29."}},{"criterion":"A product defect found on the way is fixed where it lives","outcome":"pass","revision":"1dbf9d5ca","evidence":{"kind":"observation","detail":"promote_private_item read the draft before the private review accepted it; dispatch_review_draft now settles once on acceptance and promotion completes there. The course's unchanged post-promotion expectation was the failing control."}}]
---

# BPR-1246: The review submission composition course expects a left-side draft projection BPR-0587 no longer offers

## Observation

At `main` 83edd361b on macOS arm64, `application_review_submission_composition_public_path`
and its `_scale_125` and `_scale_200` rows fail with
`review-submission-composition:fail current line has a local inline draft projection`
(`tests/end_to_end/review_submission_composition_public_path.cpp:140`). After
closing the composition page the course waits for an inline draft on the
current line, then selects `left:45` and waits for a "left-side draft
projection".

Since f55e39e72 (BPR-0587) `present_review_draft` offers a new draft only on a
right-side line, and [[BPR-1207]] keeps that: left replies, no left roots. The
course's left-side expectations predate the rule; whether the current line it
lands on is left-side is not yet confirmed.

## Outcome

The course follows the accepted rule: it expects no new left-side draft, keeps
its negative control that Comment and Publish are refused on the left, and
reaches its local comment on a right-side line. A product defect found on the
way is fixed where it lives.

## Direct acceptance

- The three rows pass on macOS.

## Resolution (red-test backlog milestone, macOS arm64, 2026-09-30)

Each course step stops at its first failed expectation, so four causes came
out in turn. One was a product defect.

| Step | Cause | Now |
|---|---|---|
| `current line has a local inline draft projection`, `left-side draft projection` | f55e39e72 (BPR-0587): `present_review_draft` offers a new draft only on a right-side line | The course selects left:45 once the comparison admits it and expects no left-side draft. Its negative control is kept: Comment and Publish are refused on the left, and forcing Comment open does nothing. The local comment is then written on right:29 as before |
| `refused write retains the full local candidate and says why` | 9ad03a828 (BPR-1211): ledger answers arrive on their own lane | The course waits for the refusal instead of reading it at once |
| `the Notes privacy explanation wraps within the composed tray` | 17e6f85d1 put the purpose label above the Notes pages' tab widget; [[BPR-1242]] moved the Qt rows | The course measures it against `privateConversationTabs`, as the Qt row does |
| `one item is copied to Comments while its private source stays` | **Product defect.** With private review, `dispatch_review_draft` writes the draft through `private_review_workflow::change_draft` and accepts it on that answer, after the call returns. `promote_private_item` read the draft on return, so every note or chat-reply promotion said "Select Comment's current-head target…" and stayed on Notes, while the draft did receive the text | `dispatch_review_draft` takes an optional settlement callback, called once with whether the command was accepted. Promotion shows Comments on that answer and refuses only when the draft was not accepted |

The failing control for the product defect is this course's own unchanged
expectation (`tray_tabs()->currentIndex() == 0` after promotion), which failed
with the refusal text shown and passes with the fix. The chat course's reply
promotion ([[BPR-1245]]) met the same defect once it reached a right-side line.

`application_review_round_trip_composition_contract` pins
`github_application.cpp`; the reseal comment records the change (no grant,
provider or publication path; the mutation pin is unchanged).
