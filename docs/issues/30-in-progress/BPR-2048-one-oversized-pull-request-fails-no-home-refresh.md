---
schema: "implement-rfc.card.v1"
id: "BPR-2048"
title: "One oversized pull request fails no Home refresh, and opening it offers GitHub"
status: "In Progress"
rfcs: ["RFC-BPR-0098"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2048--BPR-2049--1f6f243721c8.json","origin":"owner report, 2026-11-10"}
depends_on: []
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
