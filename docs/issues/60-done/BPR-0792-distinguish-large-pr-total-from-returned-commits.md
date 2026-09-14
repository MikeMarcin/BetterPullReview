---
schema: "implement-rfc.card.v1"
id: "BPR-0792"
title: "Distinguish large pull request total from returned commits"
status: "Done"
rfcs: ["RFC-BPR-0013"]
depends_on: ["BPR-0643"]
priority: P1
metadata: {"milestone_plan":{"id":"rfc-bpr-0013-hardening","path":"docs/rfcs/plans/rfc-bpr-0013-hardening/milestone-plan.json"}}
acceptance: [{"criterion":"Bounded total and returned-commit parser","outcome":"pass","revision":"3cefa687a85d1bf42de33963feedb1ffbcffb6cf","evidence":{"kind":"observation","detail":"The production pager accepts reported 9,311 with exactly 250 returned records ending at the accepted head; short, overlong, wrong-head and tighter-local-budget cases refuse atomically. Focused github_paged_read passes in build/bpr0792-focused-final.log; source 3cefa687."}},{"criterion":"Snapshot and visible count distinguish total from records","outcome":"pass","revision":"3cefa687a85d1bf42de33963feedb1ffbcffb6cf","evidence":{"kind":"observation","detail":"Read model, projection and rendered Qt overview separately show reported total and returned records without fabricating history. Application and rendered Qt overview tests pass in build/bpr0792-focused-final.log; composed GitHub.com path also passes."}},{"criterion":"Focused checks and one exact read-only product attempt","outcome":"pass","revision":"3cefa687a85d1bf42de33963feedb1ffbcffb6cf","evidence":{"kind":"observation","detail":"Focused/composed tests pass 4/4, documentation and board gates pass. One tests-disabled Mac GitHub CLI profile attempt as MikeMarcin at EpicGames/UnrealEngine#1773 reached GetRepository, GetPullRequest, three commit pages, sixteen file pages, ListReviews and sixteen review-thread pages with HTTP 200, then failed malformed_response at ListReviewThreads. UI refused snapshot; no file list or selected diff. Fixed metadata-only record: /Users/jaeger/Library/Application Support/BetterPullReview/BPR-0792-evidence-2026-09-13/attempt-1-d9-cli-product-metadata.json (SHA-256 9e85ed8e4c8d6be93f1ba7f8482eeb90dc6a49ef8b6c581a716016d7014b4328)."}},{"criterion":"Read-only and privacy boundary","outcome":"pass","revision":"3cefa687a85d1bf42de33963feedb1ffbcffb6cf","evidence":{"kind":"observation","detail":"Exactly one Open action after explicit CLI read consent; no Publish or other live PR write, token extraction/transfer, global gh account switch or device-session replacement. Isolated task app and gh child stopped; unrelated app-release-d28 process untouched. Private response content was not retained in the checkout or evidence record."}}]
---

# Distinguish large pull request total from returned commits

The BPR-0643 read-only diagnostic reached `GetPullRequest` for the exact
owner-retained stress target with HTTP 200, then refused its reported 9,311
total commits against the 250-record returned-commit budget. BPR-0643 is Done
for its then-current known-limit outcome and remains terminal. This is a new
RFC-BPR-0013 limit repair under the owner's renewed read-only mandate. The
product must not invent unavailable history or raise its 250-record budget.

## Acceptance

- The provider accepts a metadata total above 250 only when the bounded pull
  request commit endpoint returns exactly its documented 250-record ceiling,
  ends at the accepted head, and all existing page, identity, text, and total
  snapshot checks pass. A smaller or larger returned collection refuses
  atomically; totals at or below 250 remain exact.
- The snapshot and visible count distinguish the reported total from the
  returned records; no missing commit is fabricated or presented as retained.
- A focused synthetic positive and negative course passes. Attempt the exact
  target read-only through the selected `gh` account without exposing its
  credential, and exercise a tests-disabled public product path where feasible.
  Record the first remaining refusal with fixed, metadata-only evidence if
  the target still cannot render its file list and selected diff.
- No live pull request write, credential transfer, private content in the
  checkout or logs, global `gh` account change, or device-session change occurs.

## Result and evidence

Source `3cefa687a85d1bf42de33963feedb1ffbcffb6cf` keeps the reported
commit total separate from the returned records in the provider snapshot and
overview. The bounded 250-record positive requires the accepted head; short,
overlong, wrong-head and tighter-local-budget cases refuse atomically. The
focused pager, application overview, rendered Qt overview and composed
GitHub.com public-path tests pass 4/4 in `build/bpr0792-focused-final.log`.
The documentation gate, scoped board check, format/diff checks and a critical
review with its narrowed correction recheck passed before the product attempt;
their pointers and limits remain on BPR-0617.

One tests-disabled Mac product attempt used an isolated ad-hoc-signed copy of
the candidate, with a task-only bundle identifier to reach its UI while an
unrelated instance remained active. Executable bytes before the unchanged
`LC_CODE_SIGNATURE` offset 14,643,728 matched the original candidate. In the
selected GitHub.com CLI profile, the UI observed connected `MikeMarcin` and
opened `EpicGames/UnrealEngine#1773` once after read-only consent. Request 1,
window session 17 received HTTP 200 for `GetRepository`, `GetPullRequest`,
three commit pages, sixteen file pages, `ListReviews` and sixteen review-thread
pages. The next parser terminal was `malformed_response` at
`ListReviewThreads`; the UI refused the pull request as data it could not
safely use. No file list or selected diff was published. Reaching the thread
stage shows the old total-versus-returned-commit refusal did not recur in
this attempt; it does not prove the private target fully renders.

The fixed metadata-only record is
`~/Library/Application Support/BetterPullReview/BPR-0792-evidence-2026-09-13/attempt-1-d9-cli-product-metadata.json`
(SHA-256 `9e85ed8e4c8d6be93f1ba7f8482eeb90dc6a49ef8b6c581a716016d7014b4328`).
No live write, token extraction or transfer, global `gh` account switch,
device-session change, or private response content in the checkout or
evidence record occurred. The task app and CLI child are quiescent; the
unrelated process was not touched. The review-thread parser refusal remains
for a separately authorized follow-up.
