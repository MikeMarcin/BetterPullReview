---
schema: "implement-rfc.card.v1"
id: "BPR-2048"
title: "One oversized pull request fails no Home refresh, and opening it offers GitHub"
status: "Done"
rfcs: ["RFC-BPR-0098"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2048--BPR-2049--1f6f243721c8.json","origin":"owner report, 2026-11-10"}
depends_on: []
acceptance: [{"criterion":"BPR-2048 scope 2","outcome":"pass","revision":"146bc2b35ef9344ef382b83c0d319eb400efc3c7","evidence":{"kind":"command","detail":"One oversized pull request fails no Home refresh; root cause fixed","command":"ctest --preset dev -R 'application_review_queue_enrollment|github_paged_read|application_home_oversized_public_path'"}},{"criterion":"BPR-2048 scope 3","outcome":"pass","revision":"146bc2b35ef9344ef382b83c0d319eb400efc3c7","evidence":{"kind":"command","detail":"Too-large card with Open on GitHub and Retry","command":"ctest --preset dev -R 'core_pull_request_refusal|application_home_oversized_public_path'"}},{"criterion":"BPR-2048 owner look","outcome":"pass","revision":"6f12ae3f91ae20e99c2fd06b598ece3a08074c7b","evidence":{"kind":"observation","detail":"Owner accepted BPR-2048-oversized-row (decision b441c298) and BPR-2048-too-large-card (decision 4fd937ca), attended session 9ed1f1a047a56e4d35f43e92d4f9d70b, frozen course of 6f12ae3f9, no note"}}]
---

# One oversized pull request fails no Home refresh, and opening it offers GitHub

## Origin

The owner, 2026-11-10: "When there is a large PR that requested me as a
reviewer, even a closed one, I get it in needs me and all home refreshes get
an error. Opening the PR says \"The GitHub response exceeded
BetterPullReview's safe limits\" . We probably need something better for
recovery. Possibly suggesting opening it on GitHub."

## Scope and acceptance

1. Find the offending pull request among the owner's review requests,
   closed ones included (read only), and measure which limit trips.
2. One pull request too large to read never fails the whole Home refresh:
   its row says it is too large to load and the rest loads. Why a closed
   pull request sits in Needs me is explained and fixed if wrong.
3. Opening it shows a card naming what was too large, with Open on GitHub
   and Retry, instead of the bare error.
4. Whether `home-next-move`'s review-thread read in Home's batched query can
   itself cross the limit is checked.

Evidence: a fixture reproducing the oversized responses; a live check on
BPR-Test only if safe. Load frontend-design before the Qt work.

## Evidence

Record: [RFC-BPR-0098 verification, owner fix round](../../verification/RFC-BPR-0098.md#owner-fix-round-bpr-2048-and-bpr-2049-2026-11-10).
Frontend-design was loaded before the Qt work.

1. Not found from this machine: the owner's review requests in every state
   and every pull request involving the owner that the owner did not author
   (read only, `gh`) hold nothing large; only BPR-Test #151 is requested.
   Measured instead: Home's batched query stays far under its page limit
   (170 KB for ten of nodejs/node's busiest pull requests); opening trips at
   `changed_files` over 3,000; Home's own retained read tripped at more
   requested reviewers than one page, before reading the state.
2. Root cause and fix in the record. `application_review_queue_enrollment`,
   `github_paged_read`, `application_home_oversized_public_path` — pass.
3. The card: `core_pull_request_refusal`, `application_home_oversized_public_path` — pass.
4. The review-thread read cannot cross the limit (measured, bounded by
   `reviewThreads(last:50)` and `comments(last:1)`).

No live mutation was needed: the failure is reproduced on the fixture, and
BPR-Test cannot hold a pull request over 3,000 files or 100 review requests
without a disproportionate live setup. The owner's look is filed (see the
handoff), not waited on.

## Owner look (filed, not waited on)

Attended session `9ed1f1a047a56e4d35f43e92d4f9d70b`, state
`/Users/jaeger/.attended-review-state/home-oversized-seen-v001-2026-11-10/state`,
cursor 12, four candidates ready and protected: `BPR-2048-oversized-row`,
`BPR-2048-too-large-card`, `BPR-2049-seen`, `BPR-2049-approve-again`, on the
frozen composed course of `6f12ae3f9` in
`/Users/jaeger/home-oversized-seen-v001.9rw9vr9h` (course binary SHA-256
`2766916cba0e…`, app `f4309598a892…`; launcher `--check` passed on
README.md of nested-folders-v1). The URL stays in the session state.
