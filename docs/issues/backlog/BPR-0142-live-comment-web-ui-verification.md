---
id: BPR-0142
title: Verify a published comment in the GitHub web UI
status: Backlog
rfc: RFC-BPR-0008
depends_on: [BPR-0139, BPR-0221, BPR-0229, BPR-0235]
---

# Verify a published comment in the GitHub web UI

## Final-state safety correction

BPR-0221 repaired the false-green exit handling and added a useful structural
inventory, but it does not deny generic network calls, scripts, helper
subprocesses, or production-application launches. BPR-0235 must establish the
default-deny process boundary before this card may construct or use a live
provider. The designated repository decision below remains valid.

RFC-BPR-0008 requires that a BetterPullReview comment is **readable and
replyable in the GitHub web UI with no tool-specific marker**. A fixture cannot
establish this. The fixture proves what BetterPullReview sent; only GitHub can
show what GitHub made of it, and only a person looking at the page can confirm
it carries no marker and behaves like an ordinary comment.

## Exactly what is missing

1. **A designated safe repository.** Nothing in `docs/` designates one. It must
   be a repository whose pull requests may be mutated by automated runs without
   consequence, named in documentation so no other target is ever reachable.
2. **A write-scoped token for that repository, supplied by the product owner.**
   It is never committed, never logged, and never placed in a fixture.
3. **A human at the keyboard** to open the pull request, read the comment,
   reply to it, and confirm it is indistinguishable from one written in the web
   UI.

## What must not be done instead

Do not point this at an arbitrary repository. Do not create a repository and
assume it is designated. Do not weaken the coordinator's safe-target check to
let a test through — that check is the reason an accident is impossible, and a
test that disables it has removed the property it was meant to exercise.

## A guard that must be rebuilt when this card lands

Today "no automated test can mutate a non-designated live repository" holds for
a reason stronger than care: `review_mutation_port` has exactly one
implementation in the tree, the fake in the test, and no production code
constructs a coordinator. No code path can reach GitHub with a mutation.

This card introduces the first live implementation and **destroys that
argument**. A scan that keeps passing afterwards would be passing for a reason
that no longer exists, which is worse than no scan at all.

Before the live provider is used, replace the property with one that survives
it: the live implementation must be unreachable from any test target, and that
must be asserted rather than arranged.

## Required direct evidence

- The comment is read in the GitHub web UI and reproduced here in full,
  including that it carries no BetterPullReview marker, badge, or footer.
- A reply posted in the web UI is shown returning to the workbench.
- The account shown as author is the authenticated profile.
- The designated repository is named in documentation before the run, not after.

## Owner decision, 2026-08-08: the designated repository

`https://github.com/MikeMarcin/BPR-Test`. It is named in
`docs/architecture/review-round-trip.md` under "The live provider contract",
which discharges the first of the three missing things above **and** this
card's last completion criterion: the repository is documented before the run,
not after.

The token is minted at session time with `gh auth token` and pasted into the
application's Qt-owned credential input. It is never committed, logged, echoed,
or fixtured, and never typed into a shell that records history.

## The structural guard was rebuilt first; the executable boundary is separate

[[BPR-0221]], now in `done/`, rebuilt the provider inventory and binary-symbol
guard before a live implementation existed. It does not satisfy this card's
whole-process requirement by itself. [[BPR-0235]] separately owns the CTest
launcher boundary and remains in progress until Linux and Windows are proved
target-native and one immutable pull-request identity can authorize the only
live exception.

Building the structural half before the live provider existed was the point: it is the only moment
the guard could be shown to fail on a tree whose answer was already known, and
it was — a rogue provider compiled into `review_mutation_tests` was caught by
both the source and binary halves.

**Three things this card now inherits as obligations, not choices.**

1. The live implementation must use the contracted name, factory, namespace,
   and file paths recorded in `docs/architecture/review-round-trip.md`. Any
   other name fails clause (A) of the gate. The code moves to fit the contract;
   the contract does not widen to fit the code.
2. The safe-target check goes in the composition root, immediately before the
   coordinator is constructed, comparing the requested repository against the
   compiled-in designated constant. Not in the coordinator, which cannot know
   what is documented, and not in the provider, which must not police itself.
3. **The runtime complement is this card's work.** Clause (B) deliberately
   excludes `app_better_pull_review`, because end-to-end tests execute it and
   after this card it will contain live code while running in fixture mode. A
   fixture-driven run must therefore emit `REVIEW_MUTATION_PROVIDER:fixture`,
   asserted in the end-to-end `PASS_REGULAR_EXPRESSION` chains. Symbol absence
   and runtime selection are different claims; the gate proves the first and
   only this marker proves the second.

Landing the provider also requires moving `live_review_mutation_provider` from
`NOT_COMPOSED` to `COMPOSED` in `scripts/verify_composed_hosts.py` in the same
commit — that entry exists so this cannot be forgotten.
