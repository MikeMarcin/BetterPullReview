---
schema: "implement-rfc.card.v1"
id: "BPR-0169"
title: "Confirm observer enrollment leaves GitHub's reviewer state untouched"
status: "Done"
rfcs: []
depends_on: ["BPR-0142"]
metadata: {"origin_rfc":"RFC-BPR-0010","mandate_revision":"r4","deferral":"Standalone live server non-mutation observation; not an RFC-0010 terminal gate."}
acceptance: [{"criterion":"BPR-0169:before-state-recorded","outcome":"pass","revision":"e964b5a83","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-0169-rfc-0010-observer-rules-leave-github-untouched.md","detail":"MikeMarcin/BPR-Test#48 (author MikeMarcinTest, no reviewers, labels or assignees), read through gh api at 2026-10-10T00:16Z before BetterPullReview opened it or any rule existed: reviews [], requested reviewers none, labels [], GraphQL viewerSubscription UNSUBSCRIBED for MikeMarcin and SUBSCRIBED for MikeMarcinTest, assignees [], events [], notifications [], updated_at 00:16:25Z."}},{"criterion":"BPR-0169:enrolled-by-repository-author-path-in-turn","outcome":"pass","revision":"e964b5a83","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-0169-rfc-0010-observer-rules-leave-github-untouched.md","detail":"live_second_account_course --course observer (harness 400e6ddc2, shipped composition tests-on Release, offscreen, GitHub CLI sign-in as MikeMarcin) opened #48 and through the queue controls added then removed repository MikeMarcin/BPR-Test, author MikeMarcinTest and path bpr-2acct/observer/; each enrolled #48 with exactly that rule, role none, and its removal emptied the queue."}},{"criterion":"BPR-0169:fields-compared-through-a-second-client","outcome":"pass","revision":"e964b5a83","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-0169-rfc-0010-observer-rules-leave-github-untouched.md","detail":"The same gh api reads ran after each of the six adds and removes and a script compared every field with the before-state: all six identical, including both accounts' subscription state and the pull request's updated_at."}}]
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

## Owner wizard withdrawn, 2026-10-09

The owner added a second GitHub account, `MikeMarcinTest`, so the wizard
`bpr-second-account-20261009-v001` (session
`c79054f3-14cf-44fe-9645-947ffd5f6d53`) was stopped (`wizard.py stop`) and
paused with 0 of 7 steps answered. What the account can do on BPR-Test is
recorded on [BPR-0144](BPR-0144-acting-identity-on-a-real-account.md#what-mikemarcintest-can-do-on-bpr-test-2026-10-09):
read, fork and open a pull request from its fork, with `pull` access only.

## Live course, 2026-10-09 (PDT; GitHub times UTC 2026-10-10)

Pull request A, `MikeMarcin/BPR-Test#48`, "bpr-2acct: BPR-0169 observer
check (disposable)", id 4807247502, node `PR_kwDOTyvof88AAAABHojGjg`, opened
by `MikeMarcinTest` from its fork branch `bpr-2acct-0169-observer` (head
`b7f0047ec1fd78ea16a5d2c20dffd98e653f09eb`, one file
`bpr-2acct/observer/observed.txt`) with no reviewers, labels or assignees.
`MikeMarcin` is neither its author nor a reviewer.

**Before-state**, read right after A was opened and before BetterPullReview
opened it or any observer rule existed (00:16 UTC), through `gh api` as
`MikeMarcin`: reviews `[]`; requested reviewers `{users: [], teams: []}`;
labels `[]`; assignees `[]`; issue events `[]`; notifications for #48 `[]`;
issue `updated_at` 2026-10-10T00:16:25Z; GraphQL `viewerSubscription`
UNSUBSCRIBED (`viewerCanSubscribe` true) for `MikeMarcin`, and SUBSCRIBED for
`MikeMarcinTest` through its own token.

**Enrollment**: `live_second_account_course --course observer` (harness
`400e6ddc2`; the shipped composition built tests-on Release at `e964b5a83`,
offscreen) signed in through Use GitHub CLI as `MikeMarcin`, opened
`MikeMarcin/BPR-Test#48` from Home, and through the queue's own controls
added, then removed, in turn: repository `MikeMarcin/BPR-Test`, author
`MikeMarcinTest`, path `bpr-2acct/observer/`. After each add the queue held
one row, `github.com/MikeMarcin/BPR-Test#48` under account `MikeMarcin`,
matched by exactly that rule, role none, not authored, not requested. After
each remove the row was gone.

**After-reads**: the same `gh api` reads (the second client; BetterPullReview
took no part) ran at all six points, after each add and after each remove,
and were compared field by field with the before-state by script. All six
are identical in every field above, including both accounts' subscription
state and `updated_at`.

The subscription limitation recorded on 2026-09-30 no longer applies: REST
cannot read another user's subscription, but each account's own
`viewerSubscription` is readable through GraphQL with its own token.

Cleanup: #48 closed by `MikeMarcinTest` at 01:26:27 UTC and its fork branch
deleted. The private profile root the course used was in the session's
scratch directory and is removed with it.

Every required item passes; the card closes Done. BPR-0168 (Backlog) lists
this card among its dependencies; this one no longer holds it.
