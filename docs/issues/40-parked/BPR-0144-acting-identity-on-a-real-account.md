---
schema: "implement-rfc.card.v1"
id: "BPR-0144"
title: "Confirm review identity on a real account"
status: "Parked"
rfcs: []
depends_on: ["BPR-0142","BPR-0610","BPR-0613","BPR-0614","BPR-0615"]
parked_on: "awaiting-live-github"
metadata: {"origin_rfc":"RFC-BPR-0008","mandate_revision":"r4","disposition":"owner-deferred standalone follow-up; not an RFC-0008 closeout dependency"}
---

# Confirm review identity on a real account

## Owner-deferred follow-up — 2026-09-21

Owner amendment r4 transfers the real-account observations out of RFC-BPR-0008's
terminal gate. This card remains Parked with all three review dispositions
unobserved on a live account. Its implemented wire/core/provider/recovery/public
workflow evidence is reused by the [amended RFC](../../rfcs/RFC-BPR-0008-github-review-round-trip-interactions.md#owner-amendment-r4--2026-09-21);
that is not acceptance of this card. Its unchanged next course still requires
the suitable disposable PR/account, exact candidate, bounded operation authority
and workflow-trigger check below. No remote operation ran in the r4 closeout.

RFC-BPR-0008 requires that reviews appear under the authenticated user's
identity with ordinary GitHub semantics. BPR-0137 proves the acting identity is
*carried* into the request. Whether GitHub attributes the review to that
account is GitHub's behaviour, not this project's, and only a real account can
show it.

## Exactly what is missing

BPR-0610 completed the adapter wire contract; BPR-0613 completed separate core
policy and application requests/receipts. BPR-0614 completed provider execution
with durable indeterminate recovery at `f2d23e0f`. BPR-0615 connects the async
host, normal PR-level controls, exact one-use user-submit authority, SQLite
pending restoration and production provider selection. Its local public fixture
covers Comment, empty Approve and Request Changes under the exact synthetic
account, including reconstruction at a newer head without another submission.
The remaining work here is GitHub's real attribution for all three dispositions.

GitHub does not allow authors to approve their own pull requests. The current
MikeMarcin account authored BPR-Test#1, so an authorized different reviewer
or a separately designated PR by another author will be required for the
Approve check. This does not block offline implementation.
[GitHub approval rules](https://docs.github.com/en/pull-requests/how-tos/review-pull-requests/approving-a-pull-request-with-required-reviews).

The designated repository and a token belonging to a **real account** whose
login is known in advance, so the observed author can be compared to it rather
than merely being non-empty.

## Required direct evidence

- A review submitted as Comment, Approve, and Request Changes each appears
  under the expected login.
- The observed author is compared to the active profile's account rather than
  checked for presence.
- No BetterPullReview-specific metadata accompanies the review.

## Next attended course

First identify a disposable PR authored by someone other than MikeMarcin while
preserving the chosen reviewer account. Then prepare the exact built application,
current head, account and the three visible review summaries for the existing
attended workflow. No live whole-review approval is inferred from the earlier
source-line comment or mixed-client edit approvals. No live mutation ran in
BPR-0615. The owner has been asked to identify a suitable disposable PR.

The owner requires local verification and no paid CI. Before proposing any live
review or comment operation, inspect the designated target's relevant workflow
triggers; do not execute an operation that would start GitHub Actions. Do not push
or dispatch a workflow. The local fixtures require neither credentials nor CI.

## Owner designation of BPR-Test — 2026-09-30

On 2026-09-30 the owner wrote: "You can do anything you need to with the
MikeMarcin/BPR-Test github repo". That supplies the repository and the
authority to open a disposable PR there. BPR-Test has no workflow files on any
branch (checked 2026-09-30), so a review there starts no Actions.

It does not supply the missing input. GitHub refuses Approve and Request
Changes from a pull request's author, and `MikeMarcin` is the only account
available, so every PR on BPR-Test that this account can open is one it
cannot approve or request changes on. Two of the three required dispositions
need either a second account as the reviewer, or a PR on BPR-Test authored by
another account with `MikeMarcin` reviewing it. The Comment disposition alone
would not satisfy the card, and the card's course is the attended one through
the built application.

The card stays Parked on `awaiting-live-github`. Next: the owner names a
second GitHub account (as author or reviewer) for BPR-Test. No live object
was made for it.

## Reassessment after the BPR-Test designation — 2026-10-05

The owner's 2026-10-05 designation (root `AGENTS.md`) repeats the 2026-09-30
grant of BPR-Test and supplies no second identity. A read-only `gh api` check
as `MikeMarcin` found `MikeMarcin` the only collaborator and the author of
every PR on BPR-Test. GitHub refuses Approve and Request Changes from a PR's
author, so two of the three required dispositions cannot be observed with
this account alone. BPR-Test still has no workflows (`actions/workflows`
total 0), so a review there starts no Actions.

The card stays Parked on `awaiting-live-github`. A Comment-only run was not
made: it would not satisfy the card and would spend a live review on a
partial course. No live object was made for it.

**Owner question:** which second GitHub account should take part on BPR-Test,
either opening a PR that `MikeMarcin` reviews in BetterPullReview or being
the account signed into BetterPullReview to review a `MikeMarcin` PR?

## Owner session prepared, 2026-10-09 (main, Mac)

The owner question above (which second account) is asked in a wizard, with the course that follows from it. The same wizard serves BPR-0169 and BPR-1222.

- Wizard `bpr-second-account-20261009-v001` (session `c79054f3-14cf-44fe-9645-947ffd5f6d53`, 7 steps), state `/Users/jaeger/.mask/wizard/bpr-second-account-20261009-v001`, source root this checkout; its loopback host runs detached. The page's URL carries a token and is handed to the owner directly, not committed. Steps: name the account (login only); from it, fork BPR-Test and open pull request A ('BPR-0169 observer check') and pull request B ('BPR-0144 review identity') with no reviewers or labels; give their numbers. Then, in BetterPullReview v0.1.5, the signed private prerelease (DMG sha256 `b7c9378cc93c93c6435b1970456596281d346f09d45202fb558a92ae32e2e727`, matching the release's `.sha256` asset; `spctl` accepted it as Notarized Developer ID and `stapler validate` passed), downloaded to `/Users/jaeger/.mask/wizard/bpr-live-github-20261009-files/`, signed in as `MikeMarcin`, submit Comment, Request changes and Approve on B. Then BPR-1222's reviewer question, notes, and the agent's `gh api` read: each review's author against `MikeMarcin`, and no BetterPullReview marker.
- BPR-Test is public with forking allowed; a fork pull request changes no repository setting or collaborator.
