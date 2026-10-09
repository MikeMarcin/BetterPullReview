---
schema: "implement-rfc.card.v1"
id: "BPR-0169"
title: "Confirm observer enrollment leaves GitHub's reviewer state untouched"
status: "Parked"
rfcs: []
depends_on: ["BPR-0142"]
parked_on: "awaiting-live-github"
metadata: {"origin_rfc":"RFC-BPR-0010","mandate_revision":"r4","deferral":"Standalone live server non-mutation observation; not an RFC-0010 terminal gate."}
---

# Confirm observer enrollment leaves GitHub's reviewer state untouched

That BetterPullReview's correlator *cannot* issue a request is structural and
provable here: it holds no provider, so there is no path. That GitHub's own
reviewer and subscription state is also unchanged is a different claim, and it
needs a server to answer.

## What is missing

A disposable pull request that MikeMarcin is neither author nor reviewer of,
plus read access to its reviewer, requested-reviewer, subscription and label
state through an independent client. BPR-0142 is Done and remembered sign-in
is retained. MikeMarcin authored BPR-Test#1, so that PR cannot prove this
criterion. Observer enrollment itself is local and requires no write token.

## Required direct evidence

- Reviewer list, requested reviewers, subscription state, and labels recorded
  before enrollment.
- The pull request enrolled locally by a repository, author, and path rule in
  turn.
- The same four read back from GitHub and compared, field by field, not merely
  eyeballed.
- The comparison run through a second client, so it is GitHub's answer rather
  than this tool's projection of it.

## What must not be done instead

Reading BetterPullReview's own projection back and calling it GitHub's state.
The projection is exactly what would be unchanged if the tool had made a
request it forgot to record.

## Parked checkpoint — 2026-09-05

The implementation dependency is Done. A suitable PR and independent read
surface remain missing; this card is parked awaiting live GitHub evidence.
A disposable PR requested for BPR-0144 may also be suitable, but only if the
actor's nonauthor/nonreviewer state is independently established first.

Read the four server fields before enrollment, apply and remove the local
repository/author/path rules one at a time, and read the same fields after each
course. A missing or unavailable subscription fact is a limitation, not proof
that it stayed unchanged. Preserve sign-in. No GitHub write is required or
authorized by this observer-only check; no Actions, push or paid CI may run.
BPR-0168 retains its accepted native notification evidence and still waits on
this card; no redundant notification approval is requested.

## Standalone follow-up under owner amendment r4 — 2026-09-21

The owner-directed RFC-BPR-0010 closeout transfers this unobserved external
criterion to standalone follow-up scope. It does not mark it passed. This card
stays Parked on `awaiting-live-github`, with its original ID, path, BPR-0142
dependency and required direct evidence intact. BPR-0168's terminal native
replay still depends on it; neither card gates the amended RFC closeout.

Next action: obtain a suitable disposable PR on which the actor is neither
author nor reviewer and an independent client that can read all four fields.
Preserve remembered sign-in; no new token or GitHub write is required by this
observation. A missing subscription fact remains a limitation. The
[r4 mandate](../../rfcs/plans/rfc-bpr-0010-owner-closeout/mandate-r4.md) grants no
live operation, publication, message, push or Actions authority.

## Owner designation of BPR-Test — 2026-09-30

On 2026-09-30 the owner wrote: "You can do anything you need to with the
MikeMarcin/BPR-Test github repo". That supplies a repository, and `gh api`
under the Mac's sign-in is an independent client that can read the reviewer,
requested-reviewer and label fields.

It does not supply the missing input. The card needs a pull request whose
author and reviewers do not include `MikeMarcin`, and `MikeMarcin` is the only
account available: any PR it opens on BPR-Test is its own, like BPR-Test#1.
A PR authored by a bot would need Dependabot or another automation, which
runs on GitHub Actions, and the r4 mandate grants no Actions. The
subscription fact is also not readable for another user through REST, which
stays a recorded limitation.

The card stays Parked on `awaiting-live-github`. Next: a PR on BPR-Test
authored by a second account with `MikeMarcin` neither author nor reviewer,
then the before-and-after reads the card lists. No live object was made for it.

## Reassessment after the BPR-Test designation — 2026-10-05

The owner's 2026-10-05 designation (root `AGENTS.md`) repeats the 2026-09-30
grant of BPR-Test and supplies no second identity. A read-only `gh api` check
as `MikeMarcin` found `MikeMarcin` the only collaborator and the author of
all 40 PRs on BPR-Test (#1–#40). No PR there has an author and reviewer set
that excludes `MikeMarcin`, and one cannot be made without another account. A bot-authored PR would need Actions, which no mandate
grants (BPR-Test has no workflows, `actions/workflows` total 0).

The card stays Parked on `awaiting-live-github`. No live object was made for
it, and no GitHub write is needed once the PR exists: the before and after
reads run through `gh api`, with the subscription field a recorded limitation.

**Owner question:** which second GitHub account will open a disposable PR on
BPR-Test that `MikeMarcin` neither authors nor reviews?

## Owner session prepared, 2026-10-09 (main, Mac)

The missing input (a pull request on BPR-Test that `MikeMarcin` neither authors nor reviews) is asked for in BPR-0144's wizard `bpr-second-account-20261009-v001` (session `c79054f3-14cf-44fe-9645-947ffd5f6d53`, state `/Users/jaeger/.mask/wizard/bpr-second-account-20261009-v001`). The second account opens pull request A, 'BPR-0169 observer check', with no reviewers or labels, and the owner is told not to open it in BetterPullReview. The wizard's last step is the agent's `gh api` read of A's reviewers, requested reviewers and labels: the record before enrollment. The local repository, author and path enrollment and the after-reads follow in their own session once that record exists.
