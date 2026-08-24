---
id: BPR-0142
title: Verify a published comment in the GitHub web UI
status: Backlog
rfc: RFC-BPR-0008
depends_on: [BPR-0139, BPR-0221, BPR-0229, BPR-0235, BPR-0256, BPR-0318]
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

1. **Attended execution of the prepared one-shot shipped-app lane.** BPR-0256
   prepares the digest-bound signed-capsule supervisor, private in-application
   bootstrap, and exact tests-disabled application registration for designated
   `MikeMarcin/BPR-Test#1`. It does not execute the lane or provide
   Linux/Windows evidence.
2. **The required device-login path and distributable identity.** BPR-0318
   transitively requires RFC-BPR-0016's capture, implementation, production
   composition, and public-behavior evidence. The owner rejected manual token
   minting and pasting during BPR-0256 v006.
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
with GitHub database ID `1328277631` and node ID `R_kgDOTyvofw`. This discharges
repository designation and this card's last completion criterion: the
repository is documented before the run, not after.

On 2026-08-10 the product owner then authorized creation of the disposable
pull request and designated the result before any BetterPullReview live child
or review mutation ran. The only designated PR is `MikeMarcin/BPR-Test#1`,
database ID `4246446933`, node ID `PR_kwDOTyvof879G6NV`, head OID
`17990ba0c2fb50338141f35c0717f0f99639c7ba`, base-repository ID `1328277631`,
and open state. Creating the fixture PR is not the RFC's comment mutation and
does not itself satisfy any live-provider, web-UI, identity, or mixed-client
criterion.

BPR-0256 v006 was approved and started once. Its approval was consumed and
durably spent, but the owner cancelled at the current generic manual-token
dialog without entering a credential, selecting a line, or publishing. The
owner reaffirmed that a login/device-authorization flow was supposed to be
built. v006 is non-retryable and produces no evidence for this card.

A future BPR-0256 packet must bind the RFC-BPR-0016 production device-login
path after BPR-0318 establishes its distributable identity. The operator then
approves and materializes a new one-use artifact, authenticates through that
flow, runs the single live test, and inspects the resulting first comment in
GitHub's web UI. Its one-comment authority still does not include posting a
reply; reply-return evidence requires a separately authorized attended
checkpoint. Tokens, refresh material, device codes, and credentials remain
outside repository artifacts, logs, argv, environment, and fixtures.

## The structural guard was rebuilt first; the executable boundary is separate

[[BPR-0221]], now in `60-done/`, rebuilt the provider inventory and
binary-symbol guard before a live implementation existed. It does not satisfy
this card's whole-process requirement by itself. [[BPR-0235]] separately owns
the CTest launcher boundary and sits in `10-backlog/` until Windows is proved
target-native and the only live exception enforces the now-designated
pull-request identity before child creation. Its Linux criterion is no longer
outstanding: [[BPR-0156]] re-established the Ubuntu 26.04 x86_64 matrix and
[[BPR-0358]] records that half as discharged on 2026-08-17.

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
